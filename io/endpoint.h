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

#ifndef IO_ENDPOINT_H
#define IO_ENDPOINT_H

#include <io/defs.h>

IO_BEGIN_DECLS

/**
 * Endpoint operations.
 */
struct ioendpoint_ops {
	/**
	 * Free resources allocated to an endpoint.
	 *
	 * \param endp	Endpoint to free.
	 */
	void
	(*done)(struct ioendpoint *endp);

	/**
	 * Format an endpoint to a string.
	 *
	 * \param endp	Endpoint to format.
	 * \param buf	Buffer to format the endpoint in.
	 * \param len	Length of the buffer.
	 * \returns	Length of the resulting string.
	 * \note	To obtain the required buffer size, pass \a buf as
	 *		\c NULL and \a len as 0.
	 */
	size_t
	(*format)(struct ioendpoint *endp, char *buf, size_t len);
};

/**
 * Endpoint base structure.
 */
struct ioendpoint {
	const struct ioendpoint_ops	*ops;	/**< Endpoint operations. */
	unsigned int			 refs;	/**< Number of references. */
	char				*str;	/**< String representation. */
};

/**
 * Increase the reference count of an endpoint.
 *
 * \param endp	Endpoint to operate on (which may be \c NULL).
 * \returns	The endpoint operated on.
 */
IOAPI struct ioendpoint *
ioendpoint_retain(struct ioendpoint *endp);

/**
 * Decrease the reference count of an endpoint and free it if it reaches zero.
 *
 * \param endp	Endpoint to operate on (which may be \c NULL).
 */
IOAPI void
ioendpoint_release(struct ioendpoint *endp);

/**
 * Format an endpoint to a string.
 *
 * \param endp	Endpoint to format.
 * \returns	On success, a string representation of the endpoint.
 *		Otherwise, \c NULL is returned and \c errno is set to
 *		indicate the error.
 * \note	The endpoint remains owner of the returned string.
 */
IOAPI const char *
ioendpoint_format(struct ioendpoint *endp);

IO_END_DECLS

#endif /* IO_ENDPOINT_H */
