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
ioqueue_rate_send = {
	.name	= "ioqueue_rate_send"
};

const struct ioparam
ioqueue_rate_recv = {
	.name	= "ioqueue_rate_recv"
};

struct ioqueue_rate {
	struct ioqueue		 queue;		/* header structure */
	struct ioqueue		*base;		/* underlying queue */
	struct ioevent_timer	 timer;		/* rate-monitoring timer */
	size_t			 send_sec,	/* bytes sent this second */
				 recv_sec,	/* bytes received this second */
				 send_rate,	/* bytes sent last second */
				 recv_rate;	/* bytes received last second */
};

static int		 rate_done(struct ioqueue *);
static int		 rate_attach(struct ioqueue *, struct ioloop *);
static int		 rate_detach(struct ioqueue *);
static ssize_t		 rate_maxsize(struct ioqueue *);
static ssize_t		 rate_nextsize(struct ioqueue *);
static ssize_t		 rate_send(struct ioqueue *, size_t,
			     const struct iobuf *, struct ioendpoint *);
static ssize_t		 rate_recv(struct ioqueue *, size_t,
			     const struct iobuf *, struct ioendpoint **);
static struct ioevent	*rate_send_event(struct ioqueue *, ioevent_cb_t *,
			     void *, enum ioevent_opt);
static struct ioevent	*rate_recv_event(struct ioqueue *, ioevent_cb_t *,
			     void *, enum ioevent_opt);
static int		 rate_get(struct ioqueue *, const struct ioparam *,
			     uintptr_t *);
static int		 rate_set(struct ioqueue *, const struct ioparam *,
			     uintptr_t);

static void		 rate_timer(int, void *);

static const struct ioqueue_ops
rate_ops = {
	.done		= rate_done,
	.attach		= rate_attach,
	.detach		= rate_detach,
	.maxsize	= rate_maxsize,
	.nextsize	= rate_nextsize,
	.send		= rate_send,
	.recv		= rate_recv,
	.send_event	= rate_send_event,
	.recv_event	= rate_recv_event,
	.get		= rate_get,
	.set		= rate_set
};

static int
rate_done(struct ioqueue *q)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;

	ioevent_detach((struct ioevent *) &queue->timer);

	return ioqueue_free(queue->base);
}

static int
rate_attach(struct ioqueue *q, struct ioloop *loop)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;
	int r;

	if (ioevent_attach((struct ioevent *) &queue->timer, loop) < 0)
		return -1;

	r = ioqueue_attach(queue->base, loop);
	if (r < 0)
		ioevent_detach((struct ioevent *) &queue->timer);

	return r;
}

static int
rate_detach(struct ioqueue *q)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;

	if (ioevent_detach((struct ioevent *) &queue->timer) < 0)
		return -1;

	return ioqueue_detach(queue->base);
}

static ssize_t
rate_maxsize(struct ioqueue *q)
{
	struct ioqueue_rate	*queue = (struct ioqueue_rate *) q;

	return ioqueue_maxsize(queue->base);
}

static ssize_t
rate_nextsize(struct ioqueue *q)
{
	struct ioqueue_rate	*queue = (struct ioqueue_rate *) q;

	return ioqueue_nextsize(queue->base);
}

static ssize_t
rate_send(struct ioqueue *q, size_t nbufs, const struct iobuf *bufs,
          struct ioendpoint *endp)
{
	struct ioqueue_rate	*queue = (struct ioqueue_rate *) q;
	ssize_t			 size;

	size = ioqueue_sendv(queue->base, nbufs, bufs, endp);
	if (size > 0)
		queue->send_sec += size;

	return size;
}

static ssize_t
rate_recv(struct ioqueue *q, size_t nbufs, const struct iobuf *bufs,
          struct ioendpoint **endp)
{
	struct ioqueue_rate	*queue = (struct ioqueue_rate *) q;
	ssize_t			 size;

	size = ioqueue_recvv(queue->base, nbufs, bufs, endp);
	if (size > 0)
		queue->recv_sec += size;

	return size;
}

static struct ioevent *
rate_send_event(struct ioqueue *q, ioevent_cb_t *cb, void *arg,
                enum ioevent_opt opt)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;

	return ioqueue_send_event(queue->base, cb, arg, opt);
}

static struct ioevent *
rate_recv_event(struct ioqueue *q, ioevent_cb_t *cb, void *arg,
                enum ioevent_opt opt)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;

	return ioqueue_recv_event(queue->base, cb, arg, opt);
}

static int
rate_get(struct ioqueue *q, const struct ioparam *param, uintptr_t *value)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;

	if (param == &ioqueue_rate_send) {
		*value = queue->send_rate;
		return 0;
	}

	if (param == &ioqueue_rate_recv) {
		*value = queue->recv_rate;
		return 0;
	}

	return ioqueue_get(queue->base, param, value);
}

static int
rate_set(struct ioqueue *q, const struct ioparam *param, uintptr_t value)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) q;

	if (param == &ioqueue_rate_send || param == &ioqueue_rate_recv) {
		errno = EPERM;
		return -1;
	}

	return ioqueue_set(queue->base, param, value);
}

static void
rate_timer(int UNUSED(num), void *arg)
{
	struct ioqueue_rate *queue = (struct ioqueue_rate *) arg;

	queue->send_rate = queue->send_sec;
	queue->recv_rate = queue->recv_sec;

	queue->send_sec = 0;
	queue->recv_sec = 0;
}

struct ioqueue *
ioqueue_alloc_rate(struct ioqueue *base)
{
	struct ioqueue_rate *queue;

	/* allocate and initialise a new queue */
	queue = calloc(1, sizeof(*queue));
	if (queue == NULL)
		return NULL;

	queue->queue.ops = &rate_ops;
	queue->base = base;

	/* set the timer */
	ioevent_init((struct ioevent *) &queue->timer, IOEVENT_TIMER,
	    rate_timer, queue, 0);
	queue->timer.tv = (struct timeval) { .tv_sec = 1, .tv_usec = 0 };

	return (struct ioqueue *) queue;
}
