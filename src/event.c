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

#include <io/event.h>
#include "private.h"

#include <stdlib.h>

void
ioevent_init(struct ioevent *event, enum ioevent_kind kind,
             ioevent_cb_t *cb, void *arg, enum ioevent_opt opt)
{
	event->kind = kind;
	event->opt = opt;
	event->cb = cb;
	event->arg = arg;
}

struct ioevent *
ioevent_read(int fd, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt)
{
	struct ioevent_fd *event;

	event = calloc(1, sizeof(*event));
	if (event != NULL) {
		ioevent_init((struct ioevent *) event, IOEVENT_READ, cb, arg, opt);
		event->fd = fd;
	}

	return (struct ioevent *) event;
}

struct ioevent *
ioevent_write(int fd, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt)
{
	struct ioevent_fd *event;

	event = calloc(1, sizeof(*event));
	if (event != NULL) {
		ioevent_init((struct ioevent *) event, IOEVENT_WRITE, cb, arg, opt);
		event->fd = fd;
	}

	return (struct ioevent *) event;
}

struct ioevent *
ioevent_timer(const struct timeval *tv, ioevent_cb_t *cb, void *arg,
              enum ioevent_opt opt)
{
	struct ioevent_timer *event;

	event = calloc(1, sizeof(*event));
	if (event != NULL) {
		ioevent_init((struct ioevent *) event, IOEVENT_TIMER, cb, arg, opt);
		event->tv = *tv;
	}

	return (struct ioevent *) event;
}

struct ioevent *
ioevent_signal(int signal, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt)
{
	struct ioevent_signal *event;

	event = calloc(1, sizeof(*event));
	if (event != NULL) {
		ioevent_init((struct ioevent *) event, IOEVENT_SIGNAL, cb, arg, opt);
		event->signal = signal;
	}

	return (struct ioevent *) event;
}

struct ioevent *
ioevent_child(pid_t pid, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt)
{
	struct ioevent_child *event;

	event = calloc(1, sizeof(*event));
	if (event != NULL) {
		ioevent_init((struct ioevent *) event, IOEVENT_CHILD, cb, arg, opt);
		event->child = pid;
	}

	return (struct ioevent *) event;
}

struct ioevent *
ioevent_flag(bool *flag, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt)
{
	struct ioevent_flag *event;

	event = calloc(1, sizeof(*event));
	if (event != NULL) {
		ioevent_init((struct ioevent *) event, IOEVENT_FLAG, cb, arg, opt);
		event->flag = flag;
	}

	return (struct ioevent *) event;
}

void
ioevent_free(struct ioevent *event)
{
	/* we're already freeing the event */
	event->opt &= ~IOEVENT_FREE;

	/* detach if attached */
	if (ioevent_attached(event))
		ioevent_detach(event);

	free(event);
}
