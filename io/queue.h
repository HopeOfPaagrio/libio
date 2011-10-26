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

#ifndef IO_QUEUE_H
#define IO_QUEUE_H

#include <io/defs.h>

IO_BEGIN_DECLS

/**
 * I/O queue operations.
 */
struct ioqueue_ops {
	/**
	 * Free resources allocated to a I/O queue.
	 *
	 * \param queue	I/O queue to free.
	 * \returns	On success, 0 is returned. Otherwise, -1 is returned
	 *		and \e errno is set to indicate the error.
	 *		Regardless, the resources allocated to the datagram
	 *		queue are freed.
	 */
	int
	(*done)(struct ioqueue *queue);

	/**
	 * Get the maximum datagram size that can be sent through an I/O
	 * queue.
	 *
	 * \param queue	I/O queue to operate on.
	 * \returns	On success, the maximum size of a datagram is
	 *		returned. Otherwise, -1 is returned and \e errno is
	 *		set to indicate the error.
	 */
	ssize_t
	(*maxsize)(struct ioqueue *queue);

	/**
	 * Get the size of the next available datagram in an I/O queue.
	 *
	 * \param queue	I/O queue to operate on.
	 * \note	In some cases, the size of the next available
	 *		datagram may be less than the value returned by this
	 *		function, but it is never greater.
	 */
	ssize_t
	(*nextsize)(struct ioqueue *queue);

	/**
	 * Send a datagram through an I/O queue.
	 *
	 * \param queue	I/O queue to send the datagram through.
	 * \param nbufs	Number of buffers.
	 * \param bufs	Array of buffers holding the datagram to send.
	 * \param to	If not \c NULL, the endpoint to send the datagram
	 *		to. Otherwise, the datagram is sent to the default
	 *		endpoint.
	 * \returns	On success, the length of the datagram sent is
	 *		returned. Otherwise, -1 is returned and \e errno is
	 *		set to indicate the error.
	 */
	ssize_t
	(*send)(struct ioqueue *queue, size_t nbufs, const struct iobuf *bufs,
	        struct ioendpoint *to);

	/**
	 * Receive a datagram from an I/O queue.
	 *
	 * \param queue	I/O queue from which to receive the datagram.
	 * \param nbufs	Number of buffers.
	 * \param bufs	Array of buffers to place the datagram contents.
	 * \param from	If not \c NULL, a pointer to the location to store
	 *		the endpoint the datagram was received from. The
	 *		caller is responsible for calling
	 *		ioendpoint_release() to free the endpoint.
	 * \returns	On success, the length of the received datagram is returned.
	 *		Otherwise, -1 is returned and \e errno is set to indicate
	 *		the error.
	 */
	ssize_t
	(*recv)(struct ioqueue *queue, size_t nbufs, const struct iobuf *bufs,
	        struct ioendpoint **from);

	/**
	 * Create an I/O event for a I/O queue that is triggered whenever a
	 * datagram can be sent through that queue.
	 *
	 * \param queue	I/O queue to create the event for.
	 * \param cb	Callback to call whenever a datagram can be sent.
	 * \param arg	Callback argument.
	 * \returns	On success, a newly-allocated event is returned.
	 *		Otherwise, \c NULL is returned and \e errno is set
	 *		to indicate the error.
	 */
	struct ioevent *
	(*send_event)(struct ioqueue *queue, ioevent_cb_t *cb, void *arg,
	              enum ioevent_opt opt);

	/**
	 * Create an I/O event for a I/O queue that is triggered whenever a
	 * datagram can be received from that queue.
	 *
	 * \param queue	I/O queue to create the event for.
	 * \param cb	Callback to call whenever a datagram can be received.
	 * \param arg	Callback argument.
	 * \returns	On success, a newly-allocated event is returned.
	 *		Otherwise, \c NULL is returned and \e errno is set
	 *		to indicate the error.
	 */
	struct ioevent *
	(*recv_event)(struct ioqueue *queue, ioevent_cb_t *cb, void *arg,
	              enum ioevent_opt opt);

	/**
	 * Get the value of a parameter.
	 *
	 * \param queue	I/O queue to configure.
	 * \param param	Configuration parameter to get.
	 * \param value	Where to store the parameter value.
	 * \returns	On success, 0 is returned. Otherwise, -1 is returned
	 *		and \e errno is set to indicate the error.
	 */
	int
	(*get)(struct ioqueue *queue, const struct ioparam *param,
	       uintptr_t *value);

	/**
	 * Set the value of a parameter.
	 *
	 * \param queue	I/O queue to configure.
	 * \param param	Configuration parameter to set.
	 * \param value	New value of the parameter.
	 * \returns	On success, 0 is returned. Otherwise, -1 is returned
	 *		and \e errno is set to indicate the error.
	 */
	int
	(*set)(struct ioqueue *queue, const struct ioparam *param,
	       uintptr_t value);
};

/**
 * I/O queue structure.
 */
struct ioqueue {
	const struct ioqueue_ops*ops;		/**< Operations structure. */
};

/**
 * Free a I/O queue.
 *
 * \param queue	I/O queue to free.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error. Regardless, the
 *		resources allocated to the I/O queue are freed.
 */
IOAPI int
ioqueue_free(struct ioqueue *queue);

/**
 * Get the maximum datagram size that can be sent through an I/O queue.
 *
 * \param queue	I/O queue to operate on.
 * \returns	On success, the maximum size of a datagram is returned.
 *		Otherwise, -1 is returned and \e errno is set to indicate
 *		the error.
 */
IOAPI ssize_t
ioqueue_maxsize(struct ioqueue *queue);

/**
 * Get the size of the next available datagram in an I/O queue.
 *
 * \param queue	I/O queue to operate on.
 * \returns	On success, the maximum size of the next available datagram
 *		is returned. Otherwise, -1 is returned and \e errno is set
 *		to indicate the error.
 * \note	In some cases, the size of the next available datagram may
 *		be less than the value returned by this function, but it is
 *		never greater.
 */
IOAPI ssize_t
ioqueue_nextsize(struct ioqueue *queue);

/**
 * Send a datagram through an I/O queue.
 *
 * \param queue	I/O queue through which to send the datagram.
 * \param buf	Buffer holding the datagram to send.
 * \param len	Length of the buffer.
 * \param to	If not \c NULL, the endpoint to send the datagram to.
 *		Otherwise, the datagram is sent to the default endpoint.
 * \returns	On success, the length of the datagram sent is returned.
 *		Otherwise, -1 is returned and \e errno is set to indicate
 *		the error.
 */
IOAPI ssize_t
ioqueue_send(struct ioqueue *queue, const void *buf, size_t len,
             struct ioendpoint *to);

/**
 * Send a datagram through an I/O queue.
 *
 * \param queue	I/O queue through which to send the datagram.
 * \param nbufs	Number of buffers.
 * \param bufs	Array of buffers holding the datagram to send.
 * \param to	If not \c NULL, the endpoint to send the datagram to.
 *		Otherwise, the datagram is sent to the default endpoint.
 * \returns	On success, the length of the datagram sent is returned.
 *		Otherwise, -1 is returned and \e errno is set to indicate
 *		the error.
 */
IOAPI ssize_t
ioqueue_sendv(struct ioqueue *queue, size_t nbufs, const struct iobuf *bufs,
              struct ioendpoint *to);

/**
 * Receive a datagram from an I/O queue.
 *
 * \param queue	I/O queue from which to receive the datagram.
 * \param buf	Buffer to place the datagram contents.
 * \param len	Length of the buffer.
 * \param from	If not \c NULL, a pointer to the location to store the
 *		endpoint the datagram was received from. The caller is
 *		responsible for calling ioendpoint_release() to free the
 *		endpoint.
 * \returns	On success, the length of the received datagram is returned.
 *		Otherwise, -1 is returned and \e errno is set to indicate
 *		the error.
 */
IOAPI ssize_t
ioqueue_recv(struct ioqueue *queue, void *buf, size_t len,
             struct ioendpoint **from);

/**
 * Receive a datagram from an I/O queue and allocate a buffer for it.
 *
 * \param queue	I/O queue from which to receive the datagram.
 * \param buf	Buffer to allocate and set to the datagram's contents. The
 *		caller becomes responsible for freeing the memory pointed to
 *		by the \e base member of \a buf.
 * \param from	If not \c NULL, a pointer to the location to store the
 *		endpoint the datagram was received from. The caller is
 *		responsible for calling ioendpoint_release() to free the
 *		endpoint.
 * \returns	On success, the length of the received datagram is returned.
 *		Otherwise, -1 is returned and \e errno is set to indicate
 *		the error.
 */
IOAPI ssize_t
ioqueue_recva(struct ioqueue *queue, struct iobuf *buf,
              struct ioendpoint **from);

/**
 * Receive a datagram from an I/O queue.
 *
 * \param queue	I/O queue from which to receive the datagram.
 * \param nbufs	Number of buffers.
 * \param bufs	Array of buffers to place the datagram contents.
 * \param from	If not \c NULL, a pointer to the location to store the
 *		endpoint the datagram was received from. The caller is
 *		responsible for calling ioendpoint_release() to free the
 *		endpoint.
 * \returns	On success, the length of the received datagram is returned.
 *		Otherwise, -1 is returned and \e errno is set to indicate
 *		the error.
 */
IOAPI ssize_t
ioqueue_recvv(struct ioqueue *queue, size_t nbufs, const struct iobuf *bufs,
              struct ioendpoint **from);

/**
 * Create an I/O event for a I/O queue that is triggered whenever a
 * datagram can be sent through that queue.
 *
 * \param queue	I/O queue to create the event for.
 * \param cb	Callback to call whenever a datagram can be sent.
 * \param arg	Callback argument.
 * \returns	On success, a newly-allocated event is returned. Otherwise,
 *		\c NULL is returned and \e errno is set to indicate the
 *		error.
 */
IOAPI struct ioevent *
ioqueue_send_event(struct ioqueue *queue, ioevent_cb_t *cb, void *arg,
                   enum ioevent_opt opt);

/**
 * Create an I/O event for a I/O queue that is triggered whenever a
 * datagram can be received from that queue.
 *
 * \param queue	I/O queue to create the event for.
 * \param cb	Callback to call whenever a datagram can be received.
 * \param arg	Callback argument.
 * \returns	On success, a newly-allocated event is returned. Otherwise,
 *		\c NULL is returned and \e errno is set to indicate the
 *		error.
 */
IOAPI struct ioevent *
ioqueue_recv_event(struct ioqueue *queue, ioevent_cb_t *cb, void *arg,
                   enum ioevent_opt opt);

/**
 * Get the value of a parameter.
 *
 * \param queue	I/O queue to configure.
 * \param param	Configuration parameter to get.
 * \param value	Where to store the parameter value.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
IOAPI int
ioqueue_get(struct ioqueue *queue, const struct ioparam *param, uintptr_t *value);

/**
 * Set the value of a parameter.
 *
 * \param queue	I/O queue to configure.
 * \param param	Configuration parameter to set.
 * \param value	New value of the parameter.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
IOAPI int
ioqueue_set(struct ioqueue *queue, const struct ioparam *param, uintptr_t value);

/**
 * Join a multicast group.
 *
 * \param queue	Queue to operate on.
 * \param group	Endpoint describing the multicast group to join.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
#define ioqueue_mcast_join(queue, group)                                    \
	ioqueue_set((queue), &ioqueue_mcast_join,                           \
	            (uintptr_t) (struct ioevent *) (group))

IOAPI const struct ioparam
ioqueue_mcast_join;

/**
 * Leave a multicast group.
 *
 * \param queue	Queue to operate on.
 * \param group	Endpoint describing the multicast group to leave.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
#define ioqueue_mcast_leave(queue, group)                                   \
	ioqueue_set((queue), &ioqueue_mcast_leave,                          \
	            (uintptr_t) (struct ioevent *) (group))

IOAPI const struct ioparam
ioqueue_mcast_leave;

/**
 * Set or clear the flag indicating whether multicast traffic originating
 * from this queue should loop back to this queue.
 *
 * \param queue	Queue to operate on.
 * \param value	Value of the flag.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
#define ioqueue_mcast_loop(queue, value)                                    \
	ioqueue_set((queue), &ioqueue_mcast_loop, (value)? true : false)

IOAPI const struct ioparam
ioqueue_mcast_loop;

IO_END_DECLS

#endif /* IO_QUEUE_H */
