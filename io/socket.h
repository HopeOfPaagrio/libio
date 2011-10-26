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

#ifndef IO_SOCKET_H
#define IO_SOCKET_H

#include <io/defs.h>

IO_BEGIN_DECLS

/*
 * Actually in <sys/socket.h>, but this is for convenience
 */
struct sockaddr;
struct sockaddr_storage;

/**
 * Socket endpoint operations.
 */
IOAPI const struct ioendpoint_ops
ioendpoint_socket_ops;

/**
 * Allocate a new endpoint holding a socket address.
 *
 * \param addr	Socket address to create an endpoint for.
 * \returns	On success, a pointer to a newly-allocated endpoint is
 *		returned. Otherwise, \c NULL is returned and \e errno is set
 *		to indicate the error.
 * \note	The caller is responsible for calling ioendpoint_release()
 *		to free the endpoint.
 */
IOAPI struct ioendpoint *
ioendpoint_alloc_sockaddr(const struct sockaddr *addr);

/**
 * Convert a socket endpoint to a socket address.
 *
 * \param endp	Endpoint holding a socket address.
 * \param addr	Location to store the endpoint's socket address into.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
IOAPI int
ioendpoint_sockaddr(struct ioendpoint *endp, struct sockaddr_storage *addr);

/**
 * Allocate a new I/O queue communicating over a socket.
 *
 * \param af	Address family for the socket, or AF_UNSPEC if this is to be
 *		gained from the \a to or \a from endpoints.
 * \param to	Default endpoint to send datagrams to, or \c NULL not to set
 *		a default endpoint.
 * \param from	Local endpoint to send datagrams from, or \c NULL not to set
 *		a specific local endpoint (one will be assigned).
 * \returns	On success, a pointer to a new I/O queue is returned.
 *		Otherwise, \c NULL is returned and \e errno is set to
 *		indicate the error.
 */
IOAPI struct ioqueue *
ioqueue_alloc_socket(int af, struct ioendpoint *to, struct ioendpoint *from,
                     const struct ioparam_init *inits, size_t ninits);

/**
 * Set or clear the flag indicating whether an IPv6 socket should only
 * accept IPv6 traffic.
 *
 * \param queue	Queue to operate on.
 * \param value	Value of the flag.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
#define ioqueue_socket_v6only(queue, value)                                 \
	ioqueue_set((queue), &ioqueue_socket_v6only, (value)? true : false)

IOAPI const struct ioparam
ioqueue_socket_v6only;

/**
 * Set or clear the number of hops multicast traffic may take.
 *
 * \param queue	Queue to operate on.
 * \param hops	Number of hops.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
#define ioqueue_socket_mcast_hops(queue, hops)                              \
	ioqueue_set((queue), &ioqueue_socket_mcast_hops, (int) (hops))

IOAPI const struct ioparam
ioqueue_socket_mcast_hops;

/**
 * Set or clear whether the local endpoint should be re-used.
 *
 * \param queue	Queue to operate on.
 * \param value	Value of the flag.
 * \returns	On success, 0 is returned. Otherwise, -1 is returned and \e
 *		errno is set to indicate the error.
 */
#define ioqueue_socket_reuselocal(queue, value)                             \
	ioqueue_set((queue), &ioqueue_socket_reuselocal,                    \
	            (value)? true : false)

IOAPI const struct ioparam
ioqueue_socket_reuselocal;

IO_END_DECLS

#endif /* IO_SOCKET_H */
