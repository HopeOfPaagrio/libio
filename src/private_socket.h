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

#ifndef PRIVATE_SOCKET_H
#define PRIVATE_SOCKET_H

#include <io/endpoint.h>
#include "private.h"

#include <sys/socket.h>

/*
 * Socket endpoint
 */
struct ioendpoint_socket {
	struct ioendpoint	 endpoint;
	struct sockaddr_storage	 addr;
	socklen_t		 addrlen;
};

#endif /* PRIVATE_SOCKET_H */
