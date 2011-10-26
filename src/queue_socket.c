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

#include <io/queue.h>
#include <io/socket.h>
#include "private_socket.h"

#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#if defined(AF_INET) || defined(AF_INET6)
# include <arpa/inet.h>
# include <netinet/in.h>
#endif

#ifdef AF_UNIX
# include <sys/un.h>
#endif

const struct ioparam
ioqueue_socket_v6only = {
	.name	= "ioqueue_socket_v6only"
};

const struct ioparam
ioqueue_socket_mcast_hops = {
	.name	= "ioqueue_socket_mcast_hops"
};

const struct ioparam
ioqueue_socket_reuselocal = {
	.name	= "ioqueue_socket_reuselocal"
};

/*
 * Socket I/O queue
 */
struct ioqueue_socket {
	struct ioqueue	 queue;
	int		 af;
	int		 sock;
};

static int		 socket_done(struct ioqueue *);
static ssize_t		 socket_maxsize(struct ioqueue *);
static ssize_t		 socket_nextsize(struct ioqueue *);
static ssize_t		 socket_send(struct ioqueue *, size_t,
			     const struct iobuf *, struct ioendpoint *);
static ssize_t		 socket_recv(struct ioqueue *, size_t,
			     const struct iobuf *, struct ioendpoint **);
static struct ioevent	*socket_send_event(struct ioqueue *, ioevent_cb_t *,
			     void *, enum ioevent_opt);
static struct ioevent	*socket_recv_event(struct ioqueue *, ioevent_cb_t *,
			     void *, enum ioevent_opt);
static int		 socket_get(struct ioqueue *,
			     const struct ioparam *, uintptr_t *);
static int		 socket_set(struct ioqueue *,
			     const struct ioparam *, uintptr_t);

static const struct ioqueue_ops
socket_ops = {
	.done		= socket_done,
	.maxsize	= socket_maxsize,
	.nextsize	= socket_nextsize,
	.send		= socket_send,
	.recv		= socket_recv,
	.send_event	= socket_send_event,
	.recv_event	= socket_recv_event,
	.get		= socket_get,
	.set		= socket_set
};

static int
socket_done(struct ioqueue *q)
{
	struct ioqueue_socket *queue = (struct ioqueue_socket *) q;

	return close(queue->sock);
}

static ssize_t
socket_maxsize(struct ioqueue *q)
{
	struct ioqueue_socket	*queue = (struct ioqueue_socket *) q;
	int			 val;
	socklen_t		 len;

	len = sizeof(val);
	if (getsockopt(queue->sock, SOL_SOCKET, SO_SNDBUF, &val, &len) < 0)
		return -1;

	return val;
}

static ssize_t
socket_nextsize(struct ioqueue *q)
{
	struct ioqueue_socket	*queue = (struct ioqueue_socket *) q;
	int			 val;

	val = 0;
	if (ioctl(queue->sock, FIONREAD, &val) < 0)
		return -1;

	return val;
}

static ssize_t
socket_send(struct ioqueue *q, size_t nbufs, const struct iobuf *bufs,
            struct ioendpoint *t)
{
	struct ioqueue_socket	*queue = (struct ioqueue_socket *) q;
	struct iovec		*iov;
	size_t			 i;
	ssize_t			 size;

	/* convert buffers */
	iov = alloca(nbufs * sizeof(*iov));
	for (i = 0; i < nbufs; i++) {
		iov[i].iov_base = bufs[i].base;
		iov[i].iov_len = bufs[i].len;
	}

	if (t != NULL) {
		struct ioendpoint_socket *to;
		struct msghdr msghdr;

		/* convert endpoint address */
		to = (struct ioendpoint_socket *)
		    ioendpoint_convert(t, &ioendpoint_socket_ops);
		if (to == NULL) {
			errno = EAFNOSUPPORT;
			return -1;
		}

		/* set up the message header */
		memset(&msghdr, '\0', sizeof(msghdr));
		msghdr.msg_name = &to->addr;
		msghdr.msg_namelen = to->addrlen;
		msghdr.msg_iov = iov;
		msghdr.msg_iovlen = nbufs;

		/* perform the send */
		size = sendmsg(queue->sock, &msghdr, 0);

		/* release the endpoint address */
		ioendpoint_release((struct ioendpoint *) to);
	} else {
		/* perform the send */
		size = writev(queue->sock, iov, nbufs);
	}

	return size;
}

static ssize_t
socket_recv(struct ioqueue *q, size_t nbufs, const struct iobuf *bufs,
            struct ioendpoint **f)
{
	struct ioqueue_socket	*queue = (struct ioqueue_socket *) q;
	struct iovec		*iov;
	size_t			 i;
	ssize_t			 size;

	/* convert buffers */
	iov = alloca(nbufs * sizeof(*iov));
	for (i = 0; i < nbufs; i++) {
		iov[i].iov_base = bufs[i].base;
		iov[i].iov_len = bufs[i].len;
	}

	if (f != NULL) {
		struct ioendpoint_socket *from;
		struct msghdr msghdr;

		/* create the endpoint to hold the sender address */
		from = (struct ioendpoint_socket *)
		    ioendpoint_alloc(&ioendpoint_socket_ops);
		if (from == NULL)
			return -1;

		/* set up the message header */
		memset(&msghdr, '\0', sizeof(msghdr));
		msghdr.msg_name = &from->addr;
		msghdr.msg_namelen = sizeof(from->addr);
		msghdr.msg_iov = iov;
		msghdr.msg_iovlen = nbufs;

		/* perform the receive */
		size = recvmsg(queue->sock, &msghdr, 0);
		if (size < 0) {
			ioendpoint_release((struct ioendpoint *) from);
			return -1;
		}

		/* set the address length of the address */
		switch (from->addr.ss_family) {
#ifdef AF_INET
		case AF_INET:
			from->addrlen = sizeof(struct sockaddr_in);
			break;
#endif

#ifdef AF_INET6
		case AF_INET6:
			from->addrlen = sizeof(struct sockaddr_in6);
			break;
#endif

#ifdef AF_UNIX
		case AF_UNIX:
			from->addrlen = sizeof(struct sockaddr_un);
			break;
#endif

		default:
			assert(!"can't happen");
		}

		*f = (struct ioendpoint *) from;
	} else {
		/* perform the receive */
		size = readv(queue->sock, iov, nbufs);
	}

	return size;
}

static struct ioevent *
socket_send_event(struct ioqueue *q, ioevent_cb_t *cb, void *arg,
                  enum ioevent_opt opt)
{
	struct ioqueue_socket *queue = (struct ioqueue_socket *) q;

	return ioevent_write(queue->sock, cb, arg, opt);
}

static struct ioevent *
socket_recv_event(struct ioqueue *q, ioevent_cb_t *cb, void *arg,
                  enum ioevent_opt opt)
{
	struct ioqueue_socket *queue = (struct ioqueue_socket *) q;

	return ioevent_read(queue->sock, cb, arg, opt);
}

static int
socket_get(struct ioqueue *q, const struct ioparam *param, uintptr_t *value)
{
	struct ioqueue_socket *queue = (struct ioqueue_socket *) q;
	socklen_t l;

	/* get V6ONLY flag */
	if (param == &ioqueue_socket_v6only) {
		int v;

		l = sizeof(v);
		if (getsockopt(queue->sock, IPPROTO_IPV6, IPV6_V6ONLY,
		               &v, &l) < 0)
			return -1;
		*value = v;

		return 0;
	}

	/* get multicast loop flag */
	if (param == &ioqueue_mcast_loop) {
		int v;

		l = sizeof(v);
		switch (queue->af) {
#ifdef AF_INET
		case AF_INET:
			if (getsockopt(queue->sock, IPPROTO_IP,
			               IP_MULTICAST_LOOP, &v, &l) < 0)
				return -1;
			break;
#endif

#ifdef AF_INET6
		case AF_INET6:
			if (getsockopt(queue->sock, IPPROTO_IPV6,
			               IPV6_MULTICAST_LOOP, &v, &l) < 0)
				return -1;
			break;
#endif

		default:
			goto error;
		}
		*value = v;

		return 0;
	}

	/* get multicast hop limit */
	if (param == &ioqueue_socket_mcast_hops) {
		int v;

		l = sizeof(v);
		switch (queue->af) {
#ifdef AF_INET
		case AF_INET:
			if (getsockopt(queue->sock, IPPROTO_IP,
			               IP_MULTICAST_TTL, &v, &l) < 0)
				return -1;
			break;
#endif

#ifdef AF_INET6
		case AF_INET6:
			if (getsockopt(queue->sock, IPPROTO_IPV6,
			               IPV6_MULTICAST_HOPS, &v, &l) < 0)
				return -1;
			break;
#endif

		default:
			goto error;
		}
		*value = v;

		return 0;
	}

error:
	errno = ENOTSUP;

	return -1;
}

static int
socket_set(struct ioqueue *q, const struct ioparam *param, uintptr_t value)
{
	struct ioqueue_socket *queue = (struct ioqueue_socket *) q;

	/* set address re-use flag */
	if (param == &ioqueue_socket_reuselocal) {
		int v = value? 1 : 0;

		return setsockopt(queue->sock, SOL_SOCKET, SO_REUSEADDR,
		                  &v, sizeof(v));
	}

	/* set V6ONLY flag */
	if (param == &ioqueue_socket_v6only) {
		int v = value? 1 : 0;

		return setsockopt(queue->sock, IPPROTO_IPV6, IPV6_V6ONLY,
		                  &v, sizeof(v));
	}

	/* set multicast loop flag */
	if (param == &ioqueue_mcast_loop) {
		int v = value? 1 : 0;

		switch (queue->af) {
#ifdef AF_INET
		case AF_INET:
			return setsockopt(queue->sock, IPPROTO_IP,
			                  IP_MULTICAST_LOOP, &v, sizeof(v));
#endif

#ifdef AF_INET6
		case AF_INET6:
			return setsockopt(queue->sock, IPPROTO_IPV6,
			                  IPV6_MULTICAST_LOOP, &v, sizeof(v));
#endif
		}
	}

	/* set multicast hop limit */
	if (param == &ioqueue_socket_mcast_hops) {
		int v = value & 0xff;

		switch (queue->af) {
#ifdef AF_INET
		case AF_INET:
			return setsockopt(queue->sock, IPPROTO_IP,
			                  IP_MULTICAST_TTL, &v, sizeof(v));
#endif

#ifdef AF_INET6
		case AF_INET6:
			return setsockopt(queue->sock, IPPROTO_IPV6,
			                  IPV6_MULTICAST_HOPS, &v, sizeof(v));
#endif
		}
	}

	/* join or leave multicast group */
	if (param == &ioqueue_mcast_join || param == &ioqueue_mcast_leave) {
		struct ioendpoint_socket	*group;
		int				 r;

		/* get group and convert to proper endpoint */
		group = (struct ioendpoint_socket *)
		    ioendpoint_convert((struct ioendpoint *) value,
		    &ioendpoint_socket_ops);
		if (group == NULL) {
			errno = EAFNOSUPPORT;
			return -1;
		}

		/* handle address family */
		switch (group->addr.ss_family) {
#ifdef AF_INET
		case AF_INET: {
			struct ip_mreq mreq;

			/* set up the request */
			memset(&mreq, '\0', sizeof(mreq));
			mreq.imr_multiaddr =
			    ((struct sockaddr_in *) &group->addr)->sin_addr;

			/* go for it */
			r = setsockopt(queue->sock, IPPROTO_IP,
			    param == &ioqueue_mcast_join? IP_ADD_MEMBERSHIP :
			    IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
			break;
		}
#endif

#ifdef AF_INET6
		case AF_INET6: {
			struct ipv6_mreq mreq;

			/* set up the request */
			mreq.ipv6mr_multiaddr =
			    ((struct sockaddr_in6 *) &group->addr)->sin6_addr;
			mreq.ipv6mr_interface = 0;

			/* go for it */
			r = setsockopt(queue->sock, IPPROTO_IPV6,
			    param == &ioqueue_mcast_join? IPV6_JOIN_GROUP :
			    IPV6_LEAVE_GROUP, &mreq, sizeof(mreq));
			break;
		}
#endif

		default:
			errno = EAFNOSUPPORT;
			r = -1;
		}

		/* release the group address */
		ioendpoint_release((struct ioendpoint *) group);

		return r;
	}

	errno = ENOTSUP;

	return -1;
}

struct ioqueue *
ioqueue_alloc_socket(int af, struct ioendpoint *t, struct ioendpoint *f,
                     const struct ioparam_init *inits, size_t ninits)
{
	struct ioqueue_socket	*queue = NULL;
	struct ioendpoint_socket*to = NULL,
				*from = NULL;
	size_t			 i;

	/* convert endpoints to something we can deal with */
	if (t != NULL &&
	    (to = (struct ioendpoint_socket *)
	     ioendpoint_convert(t, &ioendpoint_socket_ops)) == NULL) {
		errno = EAFNOSUPPORT;
		goto error;
	}

	if (f != NULL &&
	    (from = (struct ioendpoint_socket *)
	     ioendpoint_convert(f, &ioendpoint_socket_ops)) == NULL) {
		errno = EAFNOSUPPORT;
		goto error;
	}

	/* determine address family */
	if (af == AF_UNSPEC) {
		if (to != NULL) {
			af = to->addr.ss_family;
		} else if (from != NULL) {
			af = from->addr.ss_family;
		} else {
			errno = EINVAL;
			goto error;
		}
	}

	/* allocate a new queue and initialise it */
	queue = calloc(1, sizeof(*queue));
	if (queue == NULL)
		return NULL;

	queue->queue.ops = &socket_ops;
	queue->af = af;

	/* create the socket */
	queue->sock = socket(af, SOCK_DGRAM, 0);
	if (queue->sock < 0)
		goto error;

	/* process all initialisation parameters */
	for (i = 0; i < ninits; i++) {
		if (ioqueue_set((struct ioqueue *) queue, inits[i].param,
		    inits[i].value) < 0)
			goto error;
	}

	/* bind the from address */
	if (from != NULL &&
	    bind(queue->sock, (struct sockaddr *) &from->addr, from->addrlen) < 0)
		goto error;

	/* connect to the to address */
	if (to != NULL &&
	    connect(queue->sock, (struct sockaddr *) &to->addr, to->addrlen) < 0)
		goto error;

	return (struct ioqueue *) queue;

error:
	ioendpoint_release((struct ioendpoint *) to);
	ioendpoint_release((struct ioendpoint *) from);
	if (queue != NULL && queue->sock > 0)
		close(queue->sock);
	free(queue);

	return NULL;
}
