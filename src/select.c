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

#include <io/event.h>

#include "private.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>

#define FDSETSIZE(x)	((((x) + (NFDBITS - 1)) / NFDBITS) * sizeof(fd_mask))

struct ioloop_select {
	struct ioloop		 loop;

	int			 maxfd;		/* highest fd attached */
	unsigned int		 capacity;	/* how much room there is */

	struct ioevent_fd	**readev,	/* attached events */
				**writeev;

	fd_set			*readset,	/* file descriptor sets */
				*readset_out,
				*writeset,
				*writeset_out;
};

static int	 init(struct ioloop *);
static void	 done(struct ioloop *);
static int	 attach(struct ioloop *, struct ioevent *);
static int	 detach(struct ioloop *, struct ioevent *);
static int	 go(struct ioloop *, const struct timeval *);

const struct iobackend
iobackend_select = {
	.name	= "select",
	.kinds	= IOEVENT_READ | IOEVENT_WRITE /* | IOEVENT_TIMER | IOEVENT_SIGNAL */,
	.loopsz	= sizeof(struct ioloop_select),
	.init	= init,
	.done	= done,
	.attach	= attach,
	.detach	= detach,
	.go	= go
};

static int
init(struct ioloop *loop)
{
	struct ioloop_select *sel = (struct ioloop_select *) loop;

	/* initialise */
	sel->maxfd = -1;

	return 0;
}

static void
done(struct ioloop *loop)
{
	struct ioloop_select *sel = (struct ioloop_select *) loop;
	int i;

	/* detach all events */
	for (i = 0; i <= sel->maxfd; i++) {
		if (sel->readev[i] != NULL)
			ioevent_detach((struct ioevent *) sel->readev[i]);
		if (sel->writeev[i] != NULL)
			ioevent_detach((struct ioevent *) sel->writeev[i]);
	}

	/* release arrays */
	free(sel->readev);
	free(sel->writeev);
	free(sel->readset);
	free(sel->readset_out);
	free(sel->writeset);
	free(sel->writeset_out);
}

static int
resize_array(void **ptr, unsigned int oldsz, unsigned int newsz,
             unsigned int div, unsigned int mul)
{
	unsigned int o, n;
	void *new;

	/* calculate the old size */
	if (div != 1)
		o = (oldsz + div - 1) / div;
	else
		o = oldsz;
	o *= mul;

	/* calculate the new size */
	if (div != 1)
		n = (newsz + div - 1) / div;
	else
		n = newsz;
	n *= mul;

	/* does it change? */
	if (o != n) {
		/* resize the array */
		if ((new = realloc(*ptr, n)) == NULL)
			return -1;
		*ptr = new;
	}

	/* clear out the new area, which is uninitialised */
	memset((char *) new + o, '\0', n - o);

	return 0;
}

static int
resize(struct ioloop_select *sel, int fd)
{
	unsigned int newsz;

	/* determine the new size */
	newsz = sel->capacity;
	if (newsz == 0)
		newsz = NFDBITS;
	while (newsz <= (unsigned int) fd)
		newsz *= 2;

	/* resize everything */
	if (resize_array((void **) &sel->readev, sel->capacity, newsz, 1, sizeof(struct ioevent_fd *)) < 0 ||
	    resize_array((void **) &sel->writeev, sel->capacity, newsz, 1, sizeof(struct ioevent_fd *)) < 0 ||
	    resize_array((void **) &sel->readset, sel->capacity, newsz, NFDBITS, sizeof(fd_mask)) < 0 ||
	    resize_array((void **) &sel->readset_out, sel->capacity, newsz, NFDBITS, sizeof(fd_mask)) < 0 ||
	    resize_array((void **) &sel->writeset, sel->capacity, newsz, NFDBITS, sizeof(fd_mask)) < 0 ||
	    resize_array((void **) &sel->writeset_out, sel->capacity, newsz, NFDBITS, sizeof(fd_mask)) < 0)
		return -1;

	/* and we're done */
	sel->capacity = newsz;

	return 0;
}

static int
attach(struct ioloop *loop, struct ioevent *event)
{
	struct ioloop_select	*sel = (struct ioloop_select *) loop;
	struct ioevent_fd	*evf = (struct ioevent_fd *) event;
	struct ioevent_fd	**evs;
	fd_set			*set;

	/* make room for this event */
	if ((unsigned int) evf->fd >= sel->capacity &&
	    resize(sel, evf->fd) < 0)
		return -1;

	/* determine where to add it */
	if (event->kind == IOEVENT_READ) {
		evs = sel->readev;
		set = sel->readset;
	} else if (event->kind == IOEVENT_WRITE) {
		evs = sel->writeev;
		set = sel->writeset;
	} else {
		assert(!"can't happen");
	}

	/* check for duplicate attachments */
	if (evs[evf->fd] != NULL) {
		errno = EBUSY;
		return -1;
	}

	/* attach */
	evs[evf->fd] = evf;
	FD_SET(evf->fd, set);

	/* record the largest fd */
	if (evf->fd > sel->maxfd)
		sel->maxfd = evf->fd;

	return 0;
}

static int
detach(struct ioloop *loop, struct ioevent *event)
{
	struct ioloop_select	*sel = (struct ioloop_select *) loop;
	struct ioevent_fd	*evf = (struct ioevent_fd *) event;
	struct ioevent_fd	**evs;
	fd_set			*set;

	/* determine where to remove it */
	if (event->kind == IOEVENT_READ) {
		evs = sel->readev;
		set = sel->readset;
	} else if (event->kind == IOEVENT_WRITE) {
		evs = sel->writeev;
		set = sel->writeset;
	} else {
		return 0;
	}

	/* check for invalid detachments */
	if (evs[evf->fd] != evf) {
		errno = EINVAL;
		return -1;
	}

	/* detach */
	evs[evf->fd] = NULL;
	FD_CLR(evf->fd, set);

	/* update largest fd */
	if (evf->fd == sel->maxfd) {
		do
			sel->maxfd--;
		while (sel->maxfd >= 0 &&
		       sel->readev[sel->maxfd] == NULL &&
		       sel->writeev[sel->maxfd] == NULL);
	}

	return 0;
}

static int
go(struct ioloop *loop, const struct timeval *timeout)
{
	struct ioloop_select *sel = (struct ioloop_select *) loop;
	int n, fd;

	/* set up fd sets */
	memcpy(sel->readset_out, sel->readset, FDSETSIZE(sel->maxfd + 1));
	memcpy(sel->writeset_out, sel->writeset, FDSETSIZE(sel->maxfd + 1));

	/* select modifies its timeout argument, so if we have one we need
	 * to copy it locally */
	if (timeout != NULL) {
		struct timeval tv;

		tv = *timeout;
		n = select(sel->maxfd + 1, sel->readset_out,
		    sel->writeset_out, NULL, &tv);
	} else {
		n = select(sel->maxfd + 1, sel->readset_out,
		    sel->writeset_out, NULL, NULL);
	}

	/* handle the result */
	if (n < 0)
		return -1;
	if (n == 0)
		return 0;

	/* process events */
	for (fd = 0; fd <= sel->maxfd; fd++) {
		if (FD_ISSET(fd, sel->readset_out))
			ioevent_queue((struct ioevent *) sel->readev[fd]);

		if (FD_ISSET(fd, sel->writeset_out))
			ioevent_queue((struct ioevent *) sel->writeev[fd]);
	}

	return 0;
}
