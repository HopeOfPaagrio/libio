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
#include "private.h"

#include <stdlib.h>

struct ioendpoint *
ioendpoint_retain(struct ioendpoint *endp)
{
	if (endp != NULL)
		endp->refs++;

	return endp;
}

void
ioendpoint_release(struct ioendpoint *endp)
{
	if (endp != NULL && --endp->refs == 0) {
		endp->ops->done(endp);
		free(endp->str);
		free(endp);
	}
}

const char *
ioendpoint_format(struct ioendpoint *endp)
{
	if (endp->str == NULL) {
		size_t len;

		len = endp->ops->format(endp, NULL, 0);
		endp->str = malloc(len);
		if (endp->str == NULL)
			return NULL;

		endp->ops->format(endp, endp->str, len);
	}

	return endp->str;
}
