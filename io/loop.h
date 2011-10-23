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

#ifndef IO_LOOP_H
#define IO_LOOP_H

#include <io/defs.h>

IO_BEGIN_DECLS

/**
 * Kinds of events.
 */
enum ioevent_kind {
	IOEVENT_READ	= 0x01,	/**< File descriptor is ready for reading;
				 *   allocate using ioevent_read(). */
	IOEVENT_WRITE	= 0x02,	/**< File descriptor is ready for writing;
				 *   allocate using ioevent_read(). */
	IOEVENT_TIMER	= 0x04,	/**< Timer timed out;
				 *   allocate using ioevent_timer(). */
	IOEVENT_SIGNAL	= 0x08,	/**< Signal was delivered to the process;
				 *   allocate using ioevent_signal(). */
	IOEVENT_CHILD	= 0x10	/**< Child process terminated;
				 *   allocate using ioevent_child(). */
};

/**
 * Allocate a new I/O loop.
 *
 * \param kinds	The binary OR of the kinds of events the event loop must
 *		support. Any attempt to attach an event to the event loop
 *		whose kind is not among the requested kinds of events will
 *		fail.
 * \returns	On success, a pointer to a newly allocated I/O loop is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioloop *
ioloop_alloc(enum ioevent_kind kinds);

/**
 * Free a previously-allocated I/O loop.
 *
 * \param loop	I/O loop to free.
 */
IOAPI void
ioloop_free(struct ioloop *loop);

/**
 * Run an I/O loop exactly once.
 *
 * \param loop	I/O loop to run.
 * \return	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
IOAPI int
ioloop_once(struct ioloop *loop);

/**
 * Run an I/O loop until ioloop_break() is called.
 *
 * \param loop	The event loop to run.
 * \return	When ioloop_break() is called, 0 is returned. If an error
 *		occurs before ioloop_break() is called, -1 is returned and
 *		\e errno is set to indicate the error.
 */
IOAPI int
ioloop_run(struct ioloop *loop);

/**
 * Make a previous call to ioloop_run() return.
 *
 * \param loop	I/O loop to break.
 */
IOAPI void
ioloop_break(struct ioloop *loop);

IO_END_DECLS

#endif /* IO_LOOP_H */
