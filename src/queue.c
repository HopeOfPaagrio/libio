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
#include <limits.h>

const struct ioparam
ioqueue_mcast_join = {
	.name	= "ioqueue_mcast_join"
};

const struct ioparam
ioqueue_mcast_leave = {
	.name	= "ioqueue_mcast_leave"
};

const struct ioparam
ioqueue_mcast_loop = {
	.name	= "ioqueue_mcast_loop"
};

int
ioqueue_free(struct ioqueue *queue)
{
	int r;

	r = queue->ops->done(queue);
	free(queue);

	return r;
}

ssize_t
ioqueue_maxsize(struct ioqueue *queue)
{
	if (queue->ops->maxsize == NULL)
		return SSIZE_MAX;

	return queue->ops->maxsize(queue);
}

ssize_t
ioqueue_nextsize(struct ioqueue *queue)
{
	return queue->ops->nextsize(queue);
}

ssize_t
ioqueue_send(struct ioqueue *queue, const void *buf, size_t len,
            struct ioendpoint *to)
{
	struct iobuf iobuf;

	iobuf.base = (void *) buf;
	iobuf.len = len;

	return ioqueue_sendv(queue, 1, &iobuf, to);
}

ssize_t
ioqueue_sendv(struct ioqueue *queue, size_t nbufs, const struct iobuf *bufs,
              struct ioendpoint *to)
{
	if (queue->ops->send == NULL) {
		errno = EBADF;
		return -1;
	}

	return queue->ops->send(queue, nbufs, bufs, to);
}

ssize_t
ioqueue_recv(struct ioqueue *queue, void *buf, size_t len,
             struct ioendpoint **from)
{
	struct iobuf iobuf;

	iobuf.base = buf;
	iobuf.len = len;

	return ioqueue_recvv(queue, 1, &iobuf, from);
}

ssize_t
ioqueue_recva(struct ioqueue *queue, struct iobuf *buf,
              struct ioendpoint **from)
{
	ssize_t size;

	size = ioqueue_nextsize(queue);
	if (size < 0)
		return -1;

	buf->len = (size_t) size;
	buf->base = malloc(buf->len);
	if (buf->base == NULL)
		return -1;

	size = ioqueue_recvv(queue, 1, buf, from);
	if (size < 0)
		free(buf->base);
	buf->len = (size_t) size;

	return size;
}

ssize_t
ioqueue_recvv(struct ioqueue *queue, size_t nbufs, const struct iobuf *bufs,
              struct ioendpoint **from)
{
	if (queue->ops->recv == NULL) {
		errno = EBADF;
		return -1;
	}

	return queue->ops->recv(queue, nbufs, bufs, from);
}

struct ioevent *
ioqueue_send_event(struct ioqueue *queue, ioevent_cb_t *cb, void *arg,
                   enum ioevent_opt opt)
{
	if (queue->ops->send_event == NULL) {
		errno = EBADF;
		return NULL;
	}

	return queue->ops->send_event(queue, cb, arg, opt);
}

struct ioevent *
ioqueue_recv_event(struct ioqueue *queue, ioevent_cb_t *cb, void *arg,
                   enum ioevent_opt opt)
{
	if (queue->ops->recv_event == NULL) {
		errno = EBADF;
		return NULL;
	}

	return queue->ops->recv_event(queue, cb, arg, opt);
}

int
ioqueue_get(struct ioqueue *queue, const struct ioparam *param,
            uintptr_t *value)
{
	if (queue->ops->get == NULL) {
		errno = ENOTSUP;
		return -1;
	}

	return queue->ops->get(queue, param, value);
}

int
ioqueue_set(struct ioqueue *queue, const struct ioparam *param,
            uintptr_t value)
{
	if (queue->ops->set == NULL) {
		errno = ENOTSUP;
		return -1;
	}

	return queue->ops->set(queue, param, value);
}
