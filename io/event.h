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

#ifndef IO_EVENT_H
#define IO_EVENT_H

#include <io/defs.h>
#include <sys/time.h>

IO_BEGIN_DECLS

/**
 * Event options.
 */
enum ioevent_opt {
	IOEVENT_ONCE	= 0x01,	/**< Detach from loop after dispatch. */
	IOEVENT_FREE	= 0x02	/**< Free event when detached from loop. */
};

/**
 * Allocate an event monitoring a file for readability. The event can be
 * attached to any I/O loop that was allocated with \c #IOEVENT_READ set.
 *
 * \param fd	File descriptor to monitor.
 * \param cb	Callback to invoke when the file descriptor becomes ready
 *		for reading.
 * \param arg	Additional argument to pass to \a cb.
 * \param opt	Event options.
 * \return	On success, a pointer to a newly allocated event is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioevent *
ioevent_read(int fd, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt);
	
/**
 * Allocate an event monitoring a file for writability. The event can be
 * attached to any I/O loop that was allocated with \c #IOEVENT_WRITE set.
 *
 * \param fd	File descriptor to monitor.
 * \param cb	Callback to invoke when the file descriptor becomes ready
 *		for writing.
 * \param arg	Additional argument to pass to \a cb.
 * \param opt	Event options.
 * \return	On success, a pointer to a newly allocated event is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioevent *
ioevent_write(int fd, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt);

/**
 * Allocate an event which is dispatched when a timer times out. The event
 * can be attached to any I/O loop that was allocated with \c #IOEVENT_TIMER
 * set.
 *
 * \param tv	Interval from now at which to dispatch the timer.
 * \param cb	Callback to invoke when the timer times out.
 * \param arg	additional argument to pass to \a cb.
 * \param opt	Event options.
 * \return	On success, a pointer to a newly allocated event is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioevent *
ioevent_timer(const struct timeval *tv, ioevent_cb_t *cb, void *arg,
              enum ioevent_opt opt);

/**
 * Allocate an event which is dispatched when a specified signal is
 * delivered to the current process. The event can be attached to any event
 * loop that was allocated with \c #IOEVENT_SIGNAL set.
 *
 * \param sig	Signal to monitor.
 * \param cb	Callback to invoke when the signal is delivered.
 * \param arg	Additional argument to pass to \a cb.
 * \param opt	Event options.
 * \return	On success, a pointer to a newly allocated event is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioevent *
ioevent_signal(int sig, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt);

/**
 * Allocate an event which is dispatched when the specified child process
 * terminates. The event can be attached to any I/O loop that was allocated
 * with \c #IOEVENT_CHILD set.
 *
 * \param child	Process ID of the child to monitor.
 * \param cb	Callback to invoke when the child terminates.
 * \param arg	Additional argument to pass to \a cb.
 * \param opt	Event options.
 * \return	On success, a pointer to a newly allocated event is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioevent *
ioevent_child(pid_t child, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt);

/**
 * Allocate an event which is dispatched when a flag is set. The event can
 * be attached to any I/O loop that was allocated with \c #IOEVENT_FLAG
 * set.
 *
 * \param flag	Pointer to the flag to monitor.
 * \param cb	Callback to invoke when the flag is set.
 * \param arg	Additional argument to pass to \a cb.
 * \param opt	Event options.
 * \return	On success, a pointer to a newly allocated event is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 */
IOAPI struct ioevent *
ioevent_flag(bool *flag, ioevent_cb_t *cb, void *arg, enum ioevent_opt opt);

/**
 * Free an event. If still attached to an I/O loop, the event is detached.
 *
 * \param event	Event to free.
 * \see		ioevent_detach()
 */
IOAPI void
ioevent_free(struct ioevent *event);

/**
 * Attach an event to an I/O loop. The event can only be attached to the I/O
 * loop if the I/O loop was allocated with the \link ioevent_kind event's
 * kind\endlink. If the event was allocated with the option \c #IOEVENT_ONCE
 * set, the event is automatically detached from the I/O loop after it is
 * dispatched.
 *
 * \param event	Event to attach.
 * \param loop	Loop to attach it to.
 * \return	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 * \see		ioevent_detach()
 */
IOAPI int
ioevent_attach(struct ioevent *event, struct ioloop *loop);

/**
 * Detach an event from an I/O loop. If the event was allocated with the
 * option \c #IOEVENT_FREE set, the event is freed after being successfully
 * detached.
 *
 * \param event	Event to detach.
 * \return	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 * \see		ioevent_attach()
 */
IOAPI int
ioevent_detach(struct ioevent *event);

IO_END_DECLS

#endif /* IO_EVENT_H */
