/*
 * Copyright (c) 2011, Wouter Coene <wouter@irdc.nl>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <io/queue.h>
#include "private.h"

#include <stdlib.h>

const struct ioparam
ioqueue_limit_send = {
	.name	= "ioqueue_limit_send"
};

const struct ioparam
ioqueue_limit_recv = {
	.name	= "ioqueue_limit_recv"
};

struct ioqueue_limit {
	struct ioqueue		 queue;		/* header structure */
	struct ioqueue		*base;		/* underlying queue */
	struct ioloop		*loop;		/* I/O loop we're attached to */
	struct ioevent_timer	 timer;		/* rate-limiting timer */
	struct ioevent		*recv_event,	/* receive event */
				*send_event;	/* send event */
	size_t			 send_sec,	/* receive quota for this second */
				 recv_sec,	/* send quota for this second */
				 send_rate,	/* receive rate */
				 recv_rate,	/* send rate */
				 send_mark,	/* watermark for send */
				 recv_mark;	/* watermark for receive */
	bool			 send_ready,	/* data can be sent */
				 recv_ready;	/* data can be received */
};

static int		 limit_done(struct ioqueue *);
static int		 limit_attach(struct ioqueue *, struct ioloop *);
static int		 limit_detach(struct ioqueue *);
static ssize_t		 limit_maxsize(struct ioqueue *);
static ssize_t		 limit_nextsize(struct ioqueue *);
static ssize_t		 limit_send(struct ioqueue *, size_t,
			     const struct iobuf *, struct ioendpoint *);
static ssize_t		 limit_recv(struct ioqueue *, size_t,
			     const struct iobuf *, struct ioendpoint **);
static struct ioevent	*limit_send_event(struct ioqueue *, ioevent_cb_t *,
			     void *, enum ioevent_opt);
static struct ioevent	*limit_recv_event(struct ioqueue *, ioevent_cb_t *,
			     void *, enum ioevent_opt);
static int		 limit_get(struct ioqueue *, const struct ioparam *,
			     uintptr_t *);
static int		 limit_set(struct ioqueue *, const struct ioparam *,
			     uintptr_t);

static int		 limit_start(struct ioqueue_limit *);
static int		 limit_trigger(struct ioqueue_limit *);
static void		 limit_stop(struct ioqueue_limit *);

static void		 limit_timer(int, void *);
static void		 limit_writable(int, void *);
static void		 limit_readable(int, void *);

static const struct ioqueue_ops
limit_ops = {
	.done		= limit_done,
	.attach		= limit_attach,
	.detach		= limit_detach,
	.maxsize	= limit_maxsize,
	.nextsize	= limit_nextsize,
	.send		= limit_send,
	.recv		= limit_recv,
	.send_event	= limit_send_event,
	.recv_event	= limit_recv_event,
	.get		= limit_get,
	.set		= limit_set
};

static int
limit_done(struct ioqueue *q)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	limit_stop(queue);
	ioevent_free((struct ioevent *) queue->send_event);
	ioevent_free((struct ioevent *) queue->recv_event);

	return ioqueue_free(queue->base);
}

static int
limit_attach(struct ioqueue *q, struct ioloop *loop)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	/* prevent duplicate attachment */
	if (queue->loop != NULL) {
		errno = EBUSY;
		return -1;
	}

	/* record I/O loop and start */
	queue->loop = loop;
	if (limit_start(queue) >= 0) {
		if (ioqueue_attach(queue->base, loop) >= 0)
			return 0;

		limit_stop(queue);
	}

	queue->loop = NULL;

	return -1;
}

static int
limit_detach(struct ioqueue *q)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	return ioqueue_detach(queue->base);
}

static ssize_t
limit_maxsize(struct ioqueue *q)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	return ioqueue_maxsize(queue->base);
}

static ssize_t
limit_nextsize(struct ioqueue *q)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	return ioqueue_nextsize(queue->base);
}

static ssize_t
limit_send(struct ioqueue *q, size_t nbufs, const struct iobuf *bufs,
           struct ioendpoint *to)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;
	ssize_t size;

	size = ioqueue_sendv(queue->base, nbufs, bufs, to);
	if (size > 0) {
		queue->send_sec -= min((size_t) size, queue->send_sec);
		queue->send_ready = false;
	}

	return size;
}

static ssize_t
limit_recv(struct ioqueue *q, size_t nbufs, const struct iobuf *bufs,
           struct ioendpoint **from)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;
	ssize_t size;

	size = ioqueue_recvv(queue->base, nbufs, bufs, from);
	if (size > 0) {
		queue->recv_sec -= min((size_t) size, queue->recv_sec);
		queue->recv_ready = false;
	}

	return size;
}

static struct ioevent *
limit_send_event(struct ioqueue *q, ioevent_cb_t *cb, void *arg,
                 enum ioevent_opt opt)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	return ioevent_flag(&queue->send_ready, cb, arg, opt);
}

static struct ioevent *
limit_recv_event(struct ioqueue *q, ioevent_cb_t *cb, void *arg,
                 enum ioevent_opt opt)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	return ioevent_flag(&queue->recv_ready, cb, arg, opt);
}

static int
limit_get(struct ioqueue *q, const struct ioparam *param, uintptr_t *value)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	if (param == &ioqueue_limit_send)
		*value = queue->send_rate;
	else if (param == &ioqueue_limit_recv)
		*value = queue->recv_rate;
	else
		return ioqueue_get(queue->base, param, value);

	return 0;
}

static int
limit_set(struct ioqueue *q, const struct ioparam *param, uintptr_t value)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) q;

	if (param == &ioqueue_limit_send) {
		limit_stop(queue);
		queue->send_rate = value;
		limit_start(queue);
	} else if (param == &ioqueue_limit_recv) {
		limit_stop(queue);
		queue->recv_rate = value;
		limit_start(queue);
	} else {
		return ioqueue_set(queue->base, param, value);
	}

	return 0;
}

static int
limit_start(struct ioqueue_limit *queue)
{
	/* only if we're attached to an event loop */
	if (queue->loop == NULL)
		return 0;

	/* attach the timer */
	if ((queue->send_rate != 0 || queue->recv_rate != 0) &&
	    !ioevent_attached((struct ioevent *) &queue->timer) &&
	    ioevent_attach((struct ioevent *) &queue->timer, queue->loop) < 0)
		return -1;

	/* limit quota */
	if (queue->send_sec > queue->send_rate)
		queue->send_sec = queue->send_rate;
	if (queue->recv_sec > queue->recv_rate)
		queue->recv_sec = queue->recv_rate;

	/* may we read/write right now? */
	if (limit_trigger(queue) < 0)
		return -1;

	return 0;
}

static int
limit_trigger(struct ioqueue_limit *queue)
{
	if ((queue->send_rate == 0 || queue->send_sec >= queue->send_mark) &&
	    !ioevent_attached(queue->send_event) &&
	    ioevent_attach(queue->send_event, queue->loop) < 0)
		return -1;

	if ((queue->recv_rate == 0 || queue->recv_sec >= queue->recv_mark) &&
	    !ioevent_attached(queue->recv_event) &&
	    ioevent_attach(queue->recv_event, queue->loop) < 0)
		return -1;

	return 0;
}

static void
limit_stop(struct ioqueue_limit *queue)
{
	/* only if we're attached to an event loop */
	if (queue->loop == NULL)
		return;

	/* detach all events */
	ioevent_detach((struct ioevent *) &queue->timer);
	ioevent_detach((struct ioevent *) queue->send_event);
	ioevent_detach((struct ioevent *) queue->recv_event);
}

static void
limit_timer(int UNUSED(num), void *arg)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) arg;

	queue->send_sec += queue->send_rate;
	queue->recv_sec += queue->recv_rate;

	limit_trigger(queue);
}

static void
limit_writable(int UNUSED(num), void *arg)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) arg;

	queue->send_ready = true;
}

static void
limit_readable(int UNUSED(num), void *arg)
{
	struct ioqueue_limit *queue = (struct ioqueue_limit *) arg;

	queue->recv_ready = true;
}

struct ioqueue *
ioqueue_alloc_limit(struct ioqueue *base)
{
	struct ioqueue_limit *queue;

	/* allocate and initialise a new queue */
	queue = calloc(1, sizeof(*queue));
	if (queue == NULL)
		return NULL;

	queue->queue.ops = &limit_ops;
	queue->base = base;

	queue->send_mark = 1;
	queue->recv_mark = 1;

	/* create events */
	queue->send_event = ioqueue_send_event(queue->base, limit_writable, queue, 0);
	if (queue->send_event == NULL)
		goto error1;

	queue->recv_event = ioqueue_recv_event(queue->base, limit_readable, queue, 0);
	if (queue->recv_event == NULL)
		goto error2;

	/* set the timer */
	ioevent_init((struct ioevent *) &queue->timer, IOEVENT_TIMER,
	    limit_timer, queue, 0);
	queue->timer.tv = (struct timeval) { .tv_sec = 1, .tv_usec = 0 };

	return (struct ioqueue *) queue;

error2:
	ioevent_free(queue->send_event);
error1:
	free(queue);

	return NULL;
}
