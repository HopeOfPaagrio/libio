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

#include <io/endpoint.h>
#include <io/socket.h>
#include "private.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#if defined(AF_INET) || defined(AF_INET6)
# include <arpa/inet.h>
# include <netinet/in.h>
#endif

#ifdef AF_UNIX
# include <sys/un.h>
#endif

struct ioendpoint_socket {
	struct ioendpoint	 endpoint;
	struct sockaddr_storage	 addr;
	socklen_t		 addrlen;
};

static void	 socket_done(struct ioendpoint *);
static size_t	 socket_format(struct ioendpoint *, char *, size_t);

const struct ioendpoint_ops
ioendpoint_socket_ops = {
	.done	= socket_done,
	.format	= socket_format
};

static void
socket_done(struct ioendpoint *UNUSED(endp))
{
	/* nothing special */
}

static size_t
socket_format(struct ioendpoint *e, char *buf, size_t len)
{
	struct ioendpoint_socket *endp = (struct ioendpoint_socket *) e;

	/* determine required length */
	if (buf == NULL) {
		switch (endp->addr.ss_family) {
#ifdef AF_INET
		case AF_INET:
			/* 123.123.123.123:12345 */
			return 4 * 3 + 3 * 1 + 1 + 5 + 1;
#endif

#ifdef AF_INET6
		case AF_INET6:
			/* [1234:1234:1234:1234:1234:1234:1234:1234]:12345 */
			return 1 + 8 * 4 + 7 * 1 + 2 + 5 + 1;
#endif

#ifdef AF_UNIX
		case AF_UNIX:
			/* unix:/path */
			return 5 + nitems(((struct sockaddr_un *) &endp->addr)->sun_path) + 1;
#endif
		}

		return 0;
	}

	/* format the address */
	switch (endp->addr.ss_family) {
#ifdef AF_INET
	case AF_INET:
		/* 123.123.123.123:12345 */
		inet_ntop(AF_INET, &((struct sockaddr_in *) &endp->addr)->sin_addr,
		    buf, len);

		buf += strlen(buf);
		sprintf(buf, ":%d", ntohs(((struct sockaddr_in *) &endp->addr)->sin_port));
		break;
#endif

#ifdef AF_INET6
	case AF_INET6:
		/* [1234:1234:1234:1234:1234:1234:1234:1234]:12345 */
		*(buf++) = '[';
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *) &endp->addr)->sin6_addr,
		    buf, --len);

		buf += strlen(buf);
		sprintf(buf, "]:%d", ntohs(((struct sockaddr_in6 *) &endp->addr)->sin6_port));
		break;
#endif

#ifdef AF_UNIX
	case AF_UNIX:
		memcpy(buf, "unix:", 5);
		memcpy(buf + 5, ((struct sockaddr_un *) &endp->addr)->sun_path,
		    nitems(((struct sockaddr_un *) &endp->addr)->sun_path));
		buf[5 + nitems(((struct sockaddr_un *) &endp->addr)->sun_path)] = '\0';
		break;
#endif
	}

	return 0;
}

struct ioendpoint *
ioendpoint_alloc_socket(const struct sockaddr *addr)
{
	struct ioendpoint_socket *endp;

	/* allocate a new endpoint */
	endp = calloc(1, sizeof(*endp));
	if (endp == NULL)
		return NULL;

	endp->endpoint.ops = &ioendpoint_socket_ops;
	endp->endpoint.refs = 1;

	/* perform address-family-dependent initialisation */
	switch (addr->sa_family) {
#ifdef AF_INET
	case AF_INET:
		*((struct sockaddr_in *) &endp->addr) = *(struct sockaddr_in *) addr;
		endp->addrlen = sizeof(struct sockaddr_in);
		break;
#endif

#ifdef AF_INET6
	case AF_INET6:
		*((struct sockaddr_in6 *) &endp->addr) = *(struct sockaddr_in6 *) addr;
		endp->addrlen = sizeof(struct sockaddr_in6);
		break;
#endif

#ifdef AF_UNIX
	case AF_UNIX:
		*((struct sockaddr_un *) &endp->addr) = *(struct sockaddr_un *) addr;
		endp->addrlen = sizeof(struct sockaddr_un);
		break;
#endif

	default:
		free(endp);
		errno = EINVAL;
		return NULL;
	}

	return (struct ioendpoint *) endp;
}
