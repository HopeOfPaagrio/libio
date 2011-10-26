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

#include <io/loop.h>
#include <io/event.h>
#include "private.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/***************************************************************************
 *** Timers ****************************************************************
 ***************************************************************************/

static int
timer_insert(struct ioloop *loop, struct ioevent_timer *evt)
{
	unsigned int base, pos, limit;

	/* find insertion point */
	base = 0;
	for (limit = loop->numtimers; limit != 0; limit /= 2) {
		pos = base + limit / 2;

		/* proceed with right-hand side? */
		if (timercmp(&evt->remain, &loop->timers[pos]->remain, >)) {
			base = pos + 1;
			limit--;
		}
	}

	/* insert here */
	memmove(loop->timers + base + 1, loop->timers + base,
	    (loop->numtimers - base) * sizeof(loop->timers[0]));
	loop->timers[base] = evt;
	loop->numtimers++;

	return 0;
}

static int
timer_remove(struct ioloop *loop, struct ioevent_timer *evt)
{
	unsigned int base, pos, limit;

	/* find insertion point */
	base = 0;
	for (limit = loop->numtimers; limit != 0; limit /= 2) {
		pos = base + limit / 2;

		/* proceed with right-hand side? */
		if (timercmp(&evt->remain, &loop->timers[pos]->remain, >)) {
			base = pos + 1;
			limit--;
		}
	}

	/* we may have found another timer with the same remaining time, so
	 * we'll need to search linearly for the right one */
	while (base < loop->numtimers && loop->timers[base] != evt)
		base++;

	/* did we run out of timers? */
	if (base >= loop->numtimers) {
		errno = EINVAL;
		return -1;
	}

	assert(loop->timers[base] == evt);

	/* remove the timer */
	loop->numtimers--;
	memmove(loop->timers + base, loop->timers + base + 1,
	    (loop->numtimers - base) * sizeof(loop->timers[0]));

	return 0;
}

static int
timer_reset(struct ioevent_timer *evt)
{
	static const struct timeval zero = { 0, 0 };

	if (timer_remove(evt->event.loop, evt) < 0)
		return -1;

	timeradd(&evt->remain, &evt->tv, &evt->remain);
	if (timercmp(&evt->remain, &zero, <) < 0)
		evt->remain = zero;

	if (timer_insert(evt->event.loop, evt) < 0)
		return -1;

	return 0;
}

static int
timer_attach(struct ioloop *loop, struct ioevent_timer *evt)
{
	unsigned int i;

	/* We're attaching a new timer, which means that the timer
	 * debt built until now doesn't apply to that timer.
	 * Therefore, apply the debt to all registered timers (which
	 * doesn't change their relative ordering), and zero it.
	 * Note that it is impossible for timers to get a negative
	 * remaining time: if a timer is expired, then by definition
	 * the timer debt is set to 0. */
	if (loop->timerdebt.tv_sec != 0 || loop->timerdebt.tv_usec != 0) {
		for (i = 0; i < loop->numtimers; i++) {
			struct ioevent_timer *timer = loop->timers[i];
			timersub(&timer->remain, &loop->timerdebt, &timer->remain);
		}

		timerclear(&loop->timerdebt);
	}

	/* make room for the timer */
	if (loop->numtimers + 1 > loop->maxtimers) {
		void		*new;
		unsigned int	 num;

		if (loop->maxtimers == 0)
			num = 4;
		else
			num = loop->maxtimers * 2;
		new = realloc(loop->timers, num * sizeof(loop->timers[0]));
		if (new == NULL)
			return -1;

		loop->timers = new;
		loop->maxtimers = num;
	}

	/* initialise the timer */
	evt->remain = evt->tv;

	return timer_insert(loop, evt);
}

static int
timer_detach(struct ioloop *loop, struct ioevent_timer *evt)
{
	if (timer_remove(loop, evt) < 0)
		return -1;

	/* was this the last timer? clear the timer debt */
	if (loop->numtimers == 0)
		timerclear(&loop->timerdebt);

	return 0;
}

static int
once_more_with_timers(struct ioloop *loop, const struct timeval *start,
                      struct timeval *end)
{
	struct ioevent_timer	*timer = NULL;
	struct timeval		 tv;
	unsigned int		 i;
	struct ioevent_flag	*evf;

	/* check all flags */
	LIST_FOREACH(evf, &loop->flags, flags)
		if (*evf->flag)
			ioevent_queue((struct ioevent *) evf);
	if (!LIST_EMPTY(&loop->dispatchq))
		return 0;

	/* calculate the timeout */
	if (loop->numtimers != 0) {
		timer = loop->timers[0];

		/* take debt into account */
		timersub(&timer->remain, &loop->timerdebt, &tv);

		/* call the backend */
		if (loop->backend->go(loop, &tv) < 0)
			return -1;
	} else {
		/* call the backend */
		if (loop->backend->go(loop, NULL) < 0)
			return -1;
	}

	/* determine how long we waited */
	gettimeofday(end, NULL);

	/* should we update and dispatch timers? */
	if (timer == NULL)
		return 0;
	timersub(end, start, &tv);
	timeradd(&loop->timerdebt, &tv, &loop->timerdebt);
	if (timercmp(&loop->timerdebt, &timer->remain, <))
		return 0;

	/* update all timers */
	for (i = 0; i < loop->numtimers; i++) {
		timer = loop->timers[i];

		/* update remaining time and queue for dispatch if expired */
		timersub(&timer->remain, &loop->timerdebt, &timer->remain);
		if (timer->remain.tv_sec < 0 ||
		    (timer->remain.tv_sec == 0 && timer->remain.tv_usec <= 0))
			ioevent_queue((struct ioevent *) timer);
	}

	timerclear(&loop->timerdebt);

	return 0;
}


/***************************************************************************
 *** Dispatch **************************************************************
 ***************************************************************************/

static void
dispatch(struct ioevent *event)
{
	enum ioevent_opt opt;
	int num;

	/* determine numeric argument */
	switch (event->kind) {
	case IOEVENT_READ:
	case IOEVENT_WRITE:
		num = ((struct ioevent_fd *) event)->fd;
		break;

	default:
		num = -1;
		break;
	}

	/* is this a one-shot event? then detach now, so the callback can
	 * re-attach it if it feels like it */
	opt = event->opt;
	event->opt &= ~IOEVENT_FREE;
	if (event->opt & IOEVENT_ONCE)
		ioevent_detach(event);

	/* invoke the callback */
	if (event != NULL)
		event->cb(num, event->arg);

	/* did the callback detach us? */
	if (!ioevent_attached(event))
		return;

	/* is this an event that was detached and now needs to be freed? */
	if (opt & (IOEVENT_ONCE | IOEVENT_FREE)) {
		ioevent_free(event);
	} else if (event->kind == IOEVENT_TIMER) {
		/* reset timer */
		timer_reset((struct ioevent_timer *) event);
	}
}

static void
dispatch_queued(struct ioloop *loop)
{
	struct ioevent *event;

	/* dispatch all queued events */
	while (!LIST_EMPTY(&loop->dispatchq)) {
		event = LIST_FIRST(&loop->dispatchq, dispatchq);
		LIST_REMOVE_FIRST(&loop->dispatchq, dispatchq);
		event->opt &= ~IOEVENT_QUEUED;

		dispatch(event);
	}
}


/***************************************************************************
 *** Public API ************************************************************
 ***************************************************************************/

struct ioloop *
ioloop_alloc(enum ioevent_kind kinds)
{
	static const struct iobackend *backends[] = {
		&iobackend_select
	};
	struct ioloop	*loop;
	unsigned int	 i;

	/* look for an appropriate backend */
	for (i = 0; i < nitems(backends); i++) {
		/* check if it's supported */
		if (((backends[i]->kinds | IOEVENT_TIMER | IOEVENT_FLAG) & kinds) != kinds)
			continue;

		/* allocate and initialise the loop */
		loop = calloc(1, backends[i]->loopsz);
		if (loop == NULL)
			return NULL;

		loop->kinds = kinds;
		loop->backend = backends[i];

		/* attempt to initialise it */
		if (loop->backend->init(loop) < 0) {
			/* maybe the next one works */
			free(loop);
			continue;
		}

		return loop;
	}

	errno = ENOTSUP;

	return NULL;
}

void
ioloop_free(struct ioloop *loop)
{
	struct ioevent_flag *evf, *next;

	/* tear down the backend */
	loop->backend->done(loop);

	/* detach all timers */
	while (loop->numtimers != 0)
		ioevent_detach((struct ioevent *) loop->timers[loop->numtimers - 1]);
	free(loop->timers);

	/* detach all flags */
	LIST_FOREACH_SAFE(evf, &loop->flags, flags, next)
		ioevent_detach((struct ioevent *) evf);

	free(loop);
}

int
ioloop_once(struct ioloop *loop)
{
	struct timeval start, end;
	int r;

	/* prepare for running */
	if (loop->backend->prep != NULL && 
	    loop->backend->prep(loop) < 0)
		return -1;

	/* run once */
	gettimeofday(&start, NULL);
	r = once_more_with_timers(loop, &start, &end);
	if (r >= 0)
		dispatch_queued(loop);

	/* clean up after running */
	if (loop->backend->clean != NULL && 
	    loop->backend->clean(loop) < 0)
		return -1;

	return r;
}

int
ioloop_run(struct ioloop *loop)
{
	struct timeval tv[2];
	int i, r;

	loop->broken = false;

	/* prepare for running */
	if (loop->backend->prep != NULL && 
	    loop->backend->prep(loop) < 0)
		return -1;

	i = 0;
	gettimeofday(tv + i, NULL);

	/* run until we're done */
	while (loop->num > 0 && !loop->broken) {
		/* wait for events */
		r = once_more_with_timers(loop, tv + i, tv + (!i));
		if (r < 0)
			break;

		/* dispatch events */
		dispatch_queued(loop);

		i = !i;
	}

	/* clean up after running */
	if (loop->backend->clean != NULL && 
	    loop->backend->clean(loop) < 0)
		return -1;

	return r;
}

void
ioloop_break(struct ioloop *loop)
{
	loop->broken = true;
}

int
ioevent_attach(struct ioevent *event, struct ioloop *loop)
{
	/* usage of the event kind must have been explicitly declared by the
	 * call to ioloop_alloc(); this avoids trouble later on if (when) a
	 * backend doesn't support a particular event kind */
	if (!(event->kind & loop->kinds)) {
		errno = ENOTSUP;
		return -1;
	}

	/* sanity check */
	if (ioevent_attached(event)) {
		errno = EBUSY;
		return -1;
	}

	/* attach the event */
	switch (event->kind) {
	case IOEVENT_TIMER:
		if (timer_attach(loop, (struct ioevent_timer *) event) < 0)
			return -1;
		break;

	case IOEVENT_FLAG:
		LIST_INSERT_LAST(&loop->flags, (struct ioevent_flag *) event, flags);
		break;

	default:
		if (loop->backend->attach(loop, event) < 0)
			return -1;
		break;
	}

	event->loop = loop;
	loop->num++;

	return 0;
}

int
ioevent_detach(struct ioevent *event)
{
	/* sanity check */
	if (!ioevent_attached(event)) {
		errno = EINVAL;
		return -1;
	}

	/* detach the event */
	switch (event->kind) {
	case IOEVENT_TIMER:
		if (timer_detach(event->loop, (struct ioevent_timer *) event) < 0)
			return -1;
		break;

	case IOEVENT_FLAG:
		LIST_REMOVE(&event->loop->flags, (struct ioevent_flag *) event, flags);
		break;

	default:
		if (event->loop->backend->detach(event->loop, event) < 0)
			return -1;
		break;
	}

	/* remove from the dispatch queue if queued */
	if (event->opt & IOEVENT_QUEUED)
		LIST_REMOVE(&event->loop->dispatchq, event, dispatchq);

	event->loop->num--;
	event->loop = NULL;

	/* must we free it */
	if (event->opt & IOEVENT_FREE)
		ioevent_free(event);

	return 0;
}

void
ioevent_queue(struct ioevent *event)
{
	/* sanity check: event already queued? */
	if (event->opt & IOEVENT_QUEUED)
		return;

	LIST_INSERT_LAST(&event->loop->dispatchq, event, dispatchq);
	event->opt |= IOEVENT_QUEUED;
}
