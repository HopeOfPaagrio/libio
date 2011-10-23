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
ioendpoint_alloc_socket(const struct sockaddr *addr);

IO_END_DECLS

#endif /* IO_SOCKET_H */
