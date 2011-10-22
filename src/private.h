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

#ifndef PRIVATE_H
#define PRIVATE_H

#include <io/defs.h>
#include <io/loop.h>
#include <io/event.h>
#include "list.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

#define nitems(arr)	(sizeof(arr) / sizeof((arr)[0]))

/*
 * Internal-use event options
 */
enum {
	IOEVENT_QUEUED		= 0x80		/* event is queued for dispatch */
};

/*
 * I/O loop structure
 */
struct ioloop {
	const struct iobackend	*backend;	/* backend to use */
	enum ioevent_kind	 kinds;		/* supported events kinds */
	unsigned int		 num;		/* number of events registered */
	struct ioevent_timer	**timers;	/* sorted array of timers */
	unsigned int		 numtimers,	/* number of timer events */
				 maxtimers;	/* max. number of timer events */
	struct timeval		 timerdebt;	/* time to be sub'd from timers */
	LIST_HEAD(, ioevent)	 dispatchq;	/* dispatch queue */
	bool			 broken;	/* ioloop_break() called */
};

/*
 * Event structures
 */
struct ioevent {
	enum ioevent_kind	 kind;		/* kind of event */
	enum ioevent_opt	 opt;		/* event options */
	ioevent_cb_t		*cb;		/* callback function */
	void			*arg;		/* callback argument */
	struct ioloop		*loop;		/* loop we're attached to */
	LIST_ENTRY(, ioevent)	 dispatchq;	/* dispatch queue entry */
};

struct ioevent_fd {
	struct ioevent		 event;
	int			 fd;
};

struct ioevent_timer {
	struct ioevent		 event;
	struct timeval		 tv,
				 remain;
};

struct ioevent_signal {
	struct ioevent		 event;
	int			 signal;
};

struct ioevent_child {
	struct ioevent		 event;
	pid_t			 child;
};

static inline bool
ioevent_attached(struct ioevent *event)
{
	return event->loop != NULL;
}

void	 ioevent_init(struct ioevent *event, enum ioevent_kind kind,
	              ioevent_cb_t *cb, void *arg, enum ioevent_opt opt);
void	 ioevent_queue(struct ioevent *event);

/*
 * I/O loop backends
 */
struct iobackend {
	const char		*name;	/* backend name */
	enum ioevent_kind	 kinds;	/* supported kinds of events */
	size_t			 loopsz;/* size of ioloop structure */

	int			(*init)(struct ioloop *);
	void			(*done)(struct ioloop *);
	int			(*attach)(struct ioloop *, struct ioevent *);
	int			(*detach)(struct ioloop *, struct ioevent *);
	int			(*prep)(struct ioloop *);
	int			(*go)(struct ioloop *, const struct timeval *);
	int			(*clean)(struct ioloop *);
};

const struct iobackend
iobackend_select;

#endif /* PRIVATE_H */
