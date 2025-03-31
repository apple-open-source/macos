/*-
 * SPDX-License-Identifier: (ISC AND BSD-3-Clause)
 *
 * Portions Copyright (C) 2004-2009  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1996-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 1985, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char sccsid[] = "@(#)res_send.c	8.1 (Berkeley) 6/4/93";
static const char rcsid[] = "$Id: res_send.c,v 1.22 2009/01/22 23:49:23 tbox Exp $";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*! \file
 * \brief
 * Send query to name server and wait for reply.
 */

#include "port_before.h"
#ifndef __APPLE__
#if !defined(USE_KQUEUE) && !defined(USE_POLL)
#include "fd_setsize.h"
#endif
#else
/*
 * internal_recvfrom uses RFC 2292 API (IPV6_PKTINFO)
 * __APPLE_USE_RFC_2292 selects the appropriate API in <netinet6/in6.h>
 */
#define __APPLE_USE_RFC_2292
#endif /* ! __APPLE__ */

#ifndef __APPLE__
#include "namespace.h"
#endif /* ! __APPLE__ */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#ifdef __APPLE__
#include <arpa/nameser_compat.h>
#endif
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/fcntl.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <notify.h>
#include <notify_private.h>
#include <pthread.h>
#endif	/* __APPLE__ */

#ifndef __APPLE__
#include <isc/eventlib.h>
#endif /* ! __APPLE__ */

#include "port_after.h"

#ifdef USE_KQUEUE
#include <sys/event.h>
#else
#ifdef USE_POLL
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <poll.h>
#endif /* USE_POLL */
#endif

#ifndef __APPLE__
#include "un-namespace.h"
#endif /* ! __APPLE__ */

#include "res_debug.h"
#include "res_private.h"

#define EXT(res) ((res)->_u._ext)

#if !defined(USE_POLL) && !defined(USE_KQUEUE)
static const int highestFD = FD_SETSIZE - 1;
#endif

#ifdef __APPLE__
/* Map prefixed symbols that exist in FreeBSD */
#define _sendto	sendto     
#define _recvfrom	recvfrom
#define _socket		socket
#define _setsockopt	setsockopt
#define _connect	connect
#define _close		close
#define _getpeername	getpeername
#define _writev		writev
#define _read		read

/* Options.  Leave them on. */
#define CANNOT_CONNECT_DGRAM
#define MULTICAST

#define MAX_HOOK_RETRIES 42

/* port randomization */
#define RANDOM_BIND_MAX_TRIES 16
#define RANDOM_BIND_FIRST IPPORT_HIFIRSTAUTO
#define RANDOM_BIND_LAST IPPORT_HILASTAUTO
#endif	/* __APPLE__ */

/* Forward. */

static int		get_salen(const struct sockaddr *);
#ifndef __APPLE__
static struct sockaddr * get_nsaddr(res_state, size_t);
#endif
static int		send_vc(res_state, const u_char *, int,
				u_char *, int, int *, int,
				struct sockaddr *, int *,
				int,
				int *);
static int		send_dg(res_state,
#ifdef USE_KQUEUE
				int kq,
#endif
				const u_char *, int,
				u_char *, int, int *, int, int,
				int *, int *,
				struct sockaddr *, int *,
				int,
				int *);
static void		Aerror(const res_state, FILE *, const char *, int,
			       const struct sockaddr *, int);
static void		Perror(const res_state, FILE *, const char *, int);
static int		sock_eq(struct sockaddr *, struct sockaddr *);
#if defined(NEED_PSELECT) && !defined(USE_POLL) && !defined(USE_KQUEUE)
static int		pselect(int, void *, void *, void *,
				struct timespec *,
				const sigset_t *);
#endif
void res_pquery(const res_state, const u_char *, int, FILE *);

static const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;

/* Public. */

/*%
 *	looks up "ina" in _res.ns_addr_list[]
 *
 * returns:
 *\li	0  : not found
 *\li	>0 : found
 *
 * author:
 *\li	paul vixie, 29may94
 */
#ifdef __APPLE__
__attribute__((__visibility__("hidden")))
#endif	/* __APPLE__ */
int
res_ourserver_p(const res_state statp, const struct sockaddr *sa) {
	const struct sockaddr_in *inp, *srv;
	const struct sockaddr_in6 *in6p, *srv6;
	int ns;

	switch (sa->sa_family) {
	case AF_INET:
		inp = (const struct sockaddr_in *)sa;
		for (ns = 0;  ns < statp->nscount;  ns++) {
			srv = (struct sockaddr_in *)get_nsaddr(statp, ns);
			if (srv->sin_family == inp->sin_family &&
			    srv->sin_port == inp->sin_port &&
			    (srv->sin_addr.s_addr == INADDR_ANY ||
			     srv->sin_addr.s_addr == inp->sin_addr.s_addr))
				return (1);
		}
		break;
	case AF_INET6:
		if (EXT(statp).ext == NULL)
			break;
		in6p = (const struct sockaddr_in6 *)sa;
		for (ns = 0;  ns < statp->nscount;  ns++) {
			srv6 = (struct sockaddr_in6 *)get_nsaddr(statp, ns);
			if (srv6->sin6_family == in6p->sin6_family &&
			    srv6->sin6_port == in6p->sin6_port &&
#ifdef HAVE_SIN6_SCOPE_ID
			    (srv6->sin6_scope_id == 0 ||
			     srv6->sin6_scope_id == in6p->sin6_scope_id) &&
#endif
			    (IN6_IS_ADDR_UNSPECIFIED(&srv6->sin6_addr) ||
			     IN6_ARE_ADDR_EQUAL(&srv6->sin6_addr, &in6p->sin6_addr)))
				return (1);
		}
		break;
	default:
		break;
	}
	return (0);
}

/*%
 *	look for (name,type,class) in the query section of packet (buf,eom)
 *
 * requires:
 *\li	buf + HFIXEDSZ <= eom
 *
 * returns:
 *\li	-1 : format error
 *\li	0  : not found
 *\li	>0 : found
 *
 * author:
 *\li	paul vixie, 29may94
 */
int
res_nameinquery(const char *name, int type, int class,
		const u_char *buf, const u_char *eom)
{
	const u_char *cp = buf + HFIXEDSZ;
	int qdcount = ntohs(((const HEADER*)buf)->qdcount);

	while (qdcount-- > 0) {
		char tname[MAXDNAME+1];
		int n, ttype, tclass;

		n = dn_expand(buf, eom, cp, tname, sizeof tname);
		if (n < 0)
			return (-1);
		cp += n;
		if (cp + 2 * INT16SZ > eom)
			return (-1);
		ttype = ns_get16(cp); cp += INT16SZ;
		tclass = ns_get16(cp); cp += INT16SZ;
		if (ttype == type && tclass == class &&
		    ns_samename(tname, name) == 1)
			return (1);
	}
	return (0);
}

/*%
 *	is there a 1:1 mapping of (name,type,class)
 *	in (buf1,eom1) and (buf2,eom2)?
 *
 * returns:
 *\li	-1 : format error
 *\li	0  : not a 1:1 mapping
 *\li	>0 : is a 1:1 mapping
 *
 * author:
 *\li	paul vixie, 29may94
 */
int
res_queriesmatch(const u_char *buf1, const u_char *eom1,
		 const u_char *buf2, const u_char *eom2)
{
	const u_char *cp = buf1 + HFIXEDSZ;
	int qdcount = ntohs(((const HEADER*)buf1)->qdcount);

	if (buf1 + HFIXEDSZ > eom1 || buf2 + HFIXEDSZ > eom2)
		return (-1);

	/*
	 * Only header section present in replies to
	 * dynamic update packets.
	 */
	if ((((const HEADER *)buf1)->opcode == ns_o_update) &&
	    (((const HEADER *)buf2)->opcode == ns_o_update))
		return (1);

	if (qdcount != ntohs(((const HEADER*)buf2)->qdcount))
		return (0);
	while (qdcount-- > 0) {
		char tname[MAXDNAME+1];
		int n, ttype, tclass;

		n = dn_expand(buf1, eom1, cp, tname, sizeof tname);
		if (n < 0)
			return (-1);
		cp += n;
		if (cp + 2 * INT16SZ > eom1)
			return (-1);
		ttype = ns_get16(cp);	cp += INT16SZ;
		tclass = ns_get16(cp); cp += INT16SZ;
		if (!res_nameinquery(tname, ttype, tclass, buf2, eom2))
			return (0);
	}
	return (1);
}

/* interrupt mechanism is shared with res_query.c */
__attribute__((__visibility__("hidden"))) int interrupt_pipe_enabled = 0;
__attribute__((__visibility__("hidden"))) pthread_key_t interrupt_pipe_key;

static int
bind_random(int sock, int family)
{
	int i, status;
	uint16_t src_port;
	struct sockaddr_storage local;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sockaddr *sa;

	src_port = 0;
	status = -1;

	for (i = 0; (i < RANDOM_BIND_MAX_TRIES) && (status < 0); i++) {
		/* random port in the range RANDOM_BIND_FIRST to RANDOM_BIND_LAST */
		src_port = (res_randomid() % (RANDOM_BIND_LAST - RANDOM_BIND_FIRST)) + RANDOM_BIND_FIRST;
		memset(&local, 0, sizeof(local));
		switch (family) {
		case AF_INET:
			sin = (struct sockaddr_in *)&local;
			sin->sin_len = sizeof(struct sockaddr_in);
			sin->sin_family = family;
			sin->sin_port = htons(src_port);
			break;
		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&local;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			sin6->sin6_family = family;
			sin6->sin6_port = htons(src_port);
			break;
		default:
			errno = EOPNOTSUPP;
			return -1;
		}

		sa = (struct sockaddr *)&local;
		status = bind(sock, sa, sa->sa_len);
	}

	return status;
}

__attribute__((__visibility__("hidden")))
void
res_delete_interrupt_token(void *token)
{
	int *interrupt_pipe;

	interrupt_pipe = token;
	if (interrupt_pipe == NULL)
		return;

	if (interrupt_pipe[0] >= 0) {
		close(interrupt_pipe[0]);
		interrupt_pipe[0] = -1;
	}

	if (interrupt_pipe[1] >= 0) {
		close(interrupt_pipe[1]);
		interrupt_pipe[1] = -1;
	}

	pthread_setspecific(interrupt_pipe_key, NULL);
	free(interrupt_pipe);
}

__attribute__((__visibility__("hidden")))
void *
res_init_interrupt_token(void)
{
	int *interrupt_pipe;

	interrupt_pipe = (int *)malloc(2 * sizeof(int));
	if (interrupt_pipe == NULL)
		return NULL;

	if (pipe(interrupt_pipe) < 0) {
		/* this shouldn't happen */
		interrupt_pipe[0] = -1;
		interrupt_pipe[1] = -1;
	} else {
		fcntl(interrupt_pipe[0], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
		fcntl(interrupt_pipe[1], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
	}

	pthread_setspecific(interrupt_pipe_key, interrupt_pipe);

	return interrupt_pipe;
}

__attribute__((__visibility__("hidden")))
void
res_interrupt_requests_enable(void)
{
	interrupt_pipe_enabled = 1;
	pthread_key_create(&interrupt_pipe_key, NULL);
}

__attribute__((__visibility__("hidden")))
void
res_interrupt_requests_disable(void)
{
	interrupt_pipe_enabled = 0;
	pthread_key_delete(interrupt_pipe_key);
}

__attribute__((__visibility__("hidden")))
void
res_interrupt_request(void *token)
{
	int oldwrite;
	int *interrupt_pipe;

	interrupt_pipe = token;

	if (interrupt_pipe == NULL || interrupt_pipe_enabled == 0)
		return;

	oldwrite = interrupt_pipe[1];
	interrupt_pipe[1] = -1;

	if (oldwrite >= 0)
		close(oldwrite);
}

static struct iovec
evConsIovec(void *buf, size_t cnt)
{
	struct iovec ret;

	memset(&ret, 0xf5, sizeof ret);
	ret.iov_base = buf;
	ret.iov_len = cnt;
	return (ret);
}

static struct timespec
evConsTime(time_t sec, long nsec)
{
	struct timespec x;

	x.tv_sec = sec;
	x.tv_nsec = nsec;
	return (x);
}

static struct timespec
evTimeSpec(struct timeval tv)
{
	struct timespec ts;

	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	return (ts);
}

static struct timespec
evNowTime(void)
{
	struct timeval now;

	if (gettimeofday(&now, NULL) < 0)
		return (evConsTime(0, 0));
	return (evTimeSpec(now));
}

#define BILLION 1000000000
static struct timespec
evAddTime(struct timespec addend1, struct timespec addend2)
{
	struct timespec x;

	x.tv_sec = addend1.tv_sec + addend2.tv_sec;
	x.tv_nsec = addend1.tv_nsec + addend2.tv_nsec;
	if (x.tv_nsec >= BILLION) {
		x.tv_sec++;
		x.tv_nsec -= BILLION;
	}

	return (x);
}

static struct timespec
evSubTime(struct timespec minuend, struct timespec subtrahend)
{
	struct timespec x;

	x.tv_sec = minuend.tv_sec - subtrahend.tv_sec;
	if (minuend.tv_nsec >= subtrahend.tv_nsec)
		x.tv_nsec = minuend.tv_nsec - subtrahend.tv_nsec;
	else {
		x.tv_nsec = BILLION - subtrahend.tv_nsec + minuend.tv_nsec;
		x.tv_sec--;
	}

	return (x);
}

static int
evCmpTime(struct timespec a, struct timespec b)
{
	long x = a.tv_sec - b.tv_sec;

	if (x == 0L) x = a.tv_nsec - b.tv_nsec;
	return (x < 0L ? (-1) : x > 0L ? (1) : (0));
}

#ifdef __APPLE__
static void
_notify_safe_cancel(int notify_token)
{
	if (notify_token != -1)
		notify_cancel(notify_token);
}
#endif /* __APPLE__ */

#ifdef __APPLE__
__attribute__((__visibility__("hidden")))
#endif	/* __APPLE__ */
int
res_nsend_2(res_state statp, const u_char *buf, int buflen, u_char *ans, int anssiz, struct sockaddr *from, int *fromlen)
{
	int len, status;

	len = anssiz;
	status = dns_res_send(statp, buf, buflen, ans, &len, from, fromlen);
	if (status != ns_r_noerror)
		len = -1;
	return len;
}

int
res_nsend(res_state statp, const u_char *buf, int buflen, u_char *ans, int anssiz)
{
	struct sockaddr_storage from;
	int fromlen;

	fromlen = sizeof(struct sockaddr_storage);

	return res_nsend_2(statp, buf, buflen, ans, anssiz, (struct sockaddr *)&from, &fromlen);
}

#ifdef __APPLE__
__attribute__((__visibility__("hidden")))
int
dns_res_send(res_state statp,
	     const u_char *buf, int buflen, u_char *ans, int *anssizp,
	     struct sockaddr *from, int *fromlen)
{
	int gotsomewhere, terrno, tries, v_circuit, resplen, ns, n;
#ifdef USE_KQUEUE
	int kq;
#endif
	char abuf[NI_MAXHOST];

	char *notify_name;
	int notify_token, status, send_status;
	int anssiz = *anssizp;

	/* No name servers or res_init() failure */
	if (statp->nscount == 0 || EXT(statp).ext == NULL) {
		errno = ESRCH;
		return DNS_RES_STATUS_INVALID_RES_STATE;
	}
	if (anssiz < HFIXEDSZ) {
		errno = EINVAL;
		return DNS_RES_STATUS_INVALID_ARGUMENT;
	}
	DprintQ((statp->options & RES_DEBUG) || (statp->pfcode & RES_PRF_QUERY),
		";; res_send()", buf, buflen);
	v_circuit = (statp->options & RES_USEVC) || buflen > PACKETSZ;
	gotsomewhere = 0;
	terrno = ETIMEDOUT;

#ifdef USE_KQUEUE
	if ((kq = kqueue()) < 0) {
		Perror(statp, stderr, "kqueue", errno);
		return (-1);
	}
#endif

	send_status = ns_r_noerror;

	/*
	 * If the ns_addr_list in the resolver context has changed, then
	 * invalidate our cached copy and the associated timing data.
	 */
	if (EXT(statp).nscount != 0) {
		int needclose = 0;
		struct sockaddr_storage peer;
		ISC_SOCKLEN_T peerlen;

		if (EXT(statp).nscount != statp->nscount)
			needclose++;
		else
			for (ns = 0; ns < statp->nscount; ns++) {
				if (statp->nsaddr_list[ns].sin_family &&
				    !sock_eq((struct sockaddr *)&statp->nsaddr_list[ns],
					     (struct sockaddr *)&EXT(statp).ext->nsaddrs[ns])) {
					needclose++;
					break;
				}

				if (EXT(statp).nssocks[ns] == -1)
					continue;
				peerlen = sizeof(peer);
				if (_getpeername(EXT(statp).nssocks[ns],
				    (struct sockaddr *)&peer, &peerlen) < 0) {
					needclose++;
					break;
				}
				if (!sock_eq((struct sockaddr *)&peer,
				    get_nsaddr(statp, ns))) {
					needclose++;
					break;
				}
			}
		if (needclose) {
			res_nclose(statp);
			EXT(statp).nscount = 0;
		}
	}

	/*
	 * Maybe initialize our private copy of the ns_addr_list.
	 */
	if (EXT(statp).nscount == 0) {
		for (ns = 0; ns < statp->nscount; ns++) {
			EXT(statp).nstimes[ns] = RES_MAXTIME;
			EXT(statp).nssocks[ns] = -1;
			if (!statp->nsaddr_list[ns].sin_family)
				continue;
			EXT(statp).ext->nsaddrs[ns].sin =
				 statp->nsaddr_list[ns];
		}
		EXT(statp).nscount = statp->nscount;
	}

	/*
	 * Some resolvers want to even out the load on their nameservers.
	 * Note that RES_BLAST overrides RES_ROTATE.
	 */
	if ((statp->options & RES_ROTATE) != 0U &&
	    (statp->options & RES_BLAST) == 0U) {
		union res_sockaddr_union inu;
		struct sockaddr_in ina;
		int lastns = statp->nscount - 1;
		int fd;
		u_int16_t nstime;

		if (EXT(statp).ext != NULL)
			inu = EXT(statp).ext->nsaddrs[0];
		ina = statp->nsaddr_list[0];
		fd = EXT(statp).nssocks[0];
		nstime = EXT(statp).nstimes[0];
		for (ns = 0; ns < lastns; ns++) {
			if (EXT(statp).ext != NULL)
				EXT(statp).ext->nsaddrs[ns] =
					EXT(statp).ext->nsaddrs[ns + 1];
			statp->nsaddr_list[ns] = statp->nsaddr_list[ns + 1];
			EXT(statp).nssocks[ns] = EXT(statp).nssocks[ns + 1];
			EXT(statp).nstimes[ns] = EXT(statp).nstimes[ns + 1];
		}
		if (EXT(statp).ext != NULL)
			EXT(statp).ext->nsaddrs[lastns] = inu;
		statp->nsaddr_list[lastns] = ina;
		EXT(statp).nssocks[lastns] = fd;
		EXT(statp).nstimes[lastns] = nstime;
	}

	/*
	 * Get notification token
	 * we use a self-notification token to allow a caller
	 * to signal the thread doing this DNS query to quit.
	 */
	notify_name = NULL;
	notify_token = -1;

	asprintf(&notify_name, "self.thread.%lu", (unsigned long)pthread_self());
	if (notify_name != NULL) {
		status = notify_register_plain(notify_name, &notify_token);
		free(notify_name);
	}

	/*
	 * Send request, RETRY times, or until successful.
	 */
	for (tries = 0; tries < statp->retry; tries++) {
	    for (ns = 0; ns < statp->nscount; ns++) {
		struct sockaddr *nsap;
		int nsaplen;
		nsap = get_nsaddr(statp, ns);
		nsaplen = get_salen(nsap);
		statp->_flags &= ~RES_F_LASTMASK;
		statp->_flags |= (ns << RES_F_LASTSHIFT);
 same_ns:
		if (statp->qhook) {
			int done = 0, loops = 0;

			do {
				res_sendhookact act;

				act = (*statp->qhook)(&nsap, &buf, &buflen,
						      ans, anssiz, &resplen);
				switch (act) {
				case res_goahead:
					done = 1;
					break;
				case res_nextns:
					res_nclose(statp);
					goto next_ns;
				case res_done:
#ifdef USE_KQUEUE
					_close(kq);
#endif
					*anssizp = resplen;
					return ns_r_noerror;
				case res_modified:
					/* give the hook another try */
					if (++loops < MAX_HOOK_RETRIES)
						break;
					/*FALLTHROUGH*/
				case res_error:
					/*FALLTHROUGH*/
				default:
					send_status = DNS_RES_STATUS_CANCELLED;
					goto fail;
				}
			} while (!done);
		}

		if (res_check_if_exit_requested(statp, notify_token)) {
			send_status = DNS_RES_STATUS_CANCELLED;
			goto fail;
		}

		DprintC(((statp->options & RES_DEBUG) &&
			getnameinfo(nsap, nsaplen, abuf, sizeof(abuf),
				    NULL, 0, niflags) == 0),
			";; Querying server (# %d) address = %s",
			ns + 1, abuf);

		send_status = ns_r_noerror;

		if (v_circuit) {
			/* Use VC; at most one attempt per server. */
			tries = statp->retry;
			n = send_vc(statp, buf, buflen, ans, anssiz, &terrno,
				    ns, from, fromlen, notify_token, &send_status);
			if (n < 0)
				goto fail;
			if (n == 0)
				goto next_ns;
			resplen = n;
		} else {
			/* Use datagrams. */
			n = send_dg(statp,
#ifdef USE_KQUEUE
				    kq,
#endif
				    buf, buflen, ans, anssiz, &terrno,
				    ns, tries, &v_circuit, &gotsomewhere,
				    from, fromlen, notify_token, &send_status);
			if (n < 0)
				goto fail;
			if (n == 0)
				goto next_ns;
			if (v_circuit)
				goto same_ns;
			resplen = n;
		}

		DprintC((statp->options & RES_DEBUG) ||
			((statp->pfcode & RES_PRF_REPLY) &&
			(statp->pfcode & RES_PRF_HEAD1)),
			";; got answer:");
		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			"",
			ans, (resplen > anssiz) ? anssiz : resplen);

		/*
		 * If we have temporarily opened a virtual circuit,
		 * or if we haven't been asked to keep a socket open,
		 * close the socket.
		 */
		if ((v_circuit && (statp->options & RES_USEVC) == 0U) ||
		    (statp->options & RES_STAYOPEN) == 0U) {
			res_nclose(statp);
		}
		if (statp->rhook) {
			int done = 0, loops = 0;

			do {
				res_sendhookact act;

				act = (*statp->rhook)(nsap, buf, buflen,
						      ans, anssiz, &resplen);
				switch (act) {
				case res_goahead:
				case res_done:
					done = 1;
					break;
				case res_nextns:
					res_nclose(statp);
					goto next_ns;
				case res_modified:
					/* give the hook another try */
					if (++loops < MAX_HOOK_RETRIES)
						break;
					/*FALLTHROUGH*/
				case res_error:
					/*FALLTHROUGH*/
				default:
					send_status = DNS_RES_STATUS_CANCELLED;
					goto fail;
				}
			} while (!done);

		}
#ifdef USE_KQUEUE
		_close(kq);
#endif
		_notify_safe_cancel(notify_token);
		*anssizp = resplen;
		return ns_r_noerror;

 next_ns: ;
	    } /* foreach ns*/
	} /* foreach retry*/

	res_nclose(statp);
#ifdef USE_KQUEUE
	_close(kq);
#endif
	_notify_safe_cancel(notify_token);

	if (!v_circuit) {
		/* used datagrams */
		if (!gotsomewhere) {
			errno = ECONNREFUSED;	/*%< no nameservers found */
			return DNS_RES_STATUS_CONNECTION_REFUSED;
		} else {
			errno = ETIMEDOUT;	/*%< no answer obtained */
			return DNS_RES_STATUS_TIMEOUT;
		}
	} else
		errno = terrno;
	return send_status;
 fail:
	res_nclose(statp);
#ifdef USE_KQUEUE
	_close(kq);
#endif
	_notify_safe_cancel(notify_token);
	return send_status;
}
#endif /* __APPLE__ */

/* Private */

static int
get_salen(const struct sockaddr *sa)
{

#ifdef HAVE_SA_LEN
	/* There are people do not set sa_len.  Be forgiving to them. */
	if (sa->sa_len)
		return (sa->sa_len);
#endif

	if (sa->sa_family == AF_INET)
		return (sizeof(struct sockaddr_in));
	else if (sa->sa_family == AF_INET6)
		return (sizeof(struct sockaddr_in6));
	else
		return (0);	/*%< unknown, die on connect */
}

/*%
 * pick appropriate nsaddr_list for use.  see res_init() for initialization.
 */
#ifdef __APPLE__
__attribute__((__visibility__("hidden")))
struct sockaddr *
#else
static struct sockaddr *
#endif	/* __APPLE__ */
get_nsaddr(res_state statp, size_t n)
{

	if (!statp->nsaddr_list[n].sin_family && EXT(statp).ext) {
		/*
		 * - EXT(statp).ext->nsaddrs[n] holds an address that is larger
		 *   than struct sockaddr, and
		 * - user code did not update statp->nsaddr_list[n].
		 */
		return (struct sockaddr *)(void *)&EXT(statp).ext->nsaddrs[n];
	} else {
		/*
		 * - user code updated statp->nsaddr_list[n], or
		 * - statp->nsaddr_list[n] has the same content as
		 *   EXT(statp).ext->nsaddrs[n].
		 */
		return (struct sockaddr *)(void *)&statp->nsaddr_list[n];
	}
}

/*
 * From lookupd Thread.h.  We use this to signal threads to quit, since
 * pthread_cancel() doesn't work.
 */
#define ThreadStateExitRequested 4
#ifdef __APPLE__
__attribute__((__visibility__("hidden")))
#endif	/* __APPLE__ */
int
res_check_if_exit_requested(res_state statp, int notify_token)
{
	if (notify_token != -1) {
		uint64_t exit_requested = 0;
		notify_get_state(notify_token, &exit_requested);
		if (exit_requested == ThreadStateExitRequested) {
			Dprint(";; cancelled");
			return 1;
		}
	}
	return 0;
}

static int
send_vc(res_state statp,
	const u_char *buf, int buflen, u_char *ans, int anssiz,
	int *terrno, int ns,
	struct sockaddr *from, int *fromlen,
	int notify_token,
	int *send_status)
{
	const HEADER *hp = (const HEADER *) buf;
	HEADER *anhp = (HEADER *) ans;
	struct sockaddr *nsap;
	int nsaplen;
	int truncating, connreset, resplen;
	ssize_t n;
	struct iovec iov[2];
	u_short len;
	u_char *cp;
	void *tmp;
#ifdef SO_NOSIGPIPE
	int on = 1;
#endif

	if (ans == NULL || terrno == NULL || send_status == NULL)
		return (-1);

	nsap = get_nsaddr(statp, ns);
	nsaplen = get_salen(nsap);

	connreset = 0;
 
vc_same_ns:
	if (res_check_if_exit_requested(statp, notify_token)) {
		*terrno = EINTR;
		*send_status = DNS_RES_STATUS_CANCELLED;
		return (-1);
	}

	truncating = 0;

	/* Are we still talking to whom we want to talk? */
	if (statp->_vcsock >= 0 && (statp->_flags & RES_F_VC) != 0) {
		struct sockaddr_storage peer;
		ISC_SOCKLEN_T size = sizeof peer;

		if (getpeername(statp->_vcsock,
				(struct sockaddr *)&peer, &size) < 0 ||
		    !sock_eq((struct sockaddr *)&peer, nsap)) {
			res_nclose(statp);
			statp->_flags &= ~RES_F_VC;
		}
	}

	if ((statp->_vcsock < 0) || ((statp->_flags & RES_F_VC) == 0)) {
		if (statp->_vcsock >= 0)
			res_nclose(statp);

		statp->_vcsock = socket(nsap->sa_family, SOCK_STREAM,
		    0);
#if !defined(USE_POLL) && !defined(USE_KQUEUE)
		if (statp->_vcsock > highestFD) {
			res_nclose(statp);
			errno = ENOTSOCK;
		}
#endif
		if (statp->_vcsock < 0) {
			switch (errno) {
			case EPROTONOSUPPORT:
#ifdef EPFNOSUPPORT
			case EPFNOSUPPORT:
#endif
			case EAFNOSUPPORT:
				Perror(statp, stderr, "socket(vc)", errno);
				return (0);
			default:
				*terrno = errno;
				*send_status = DNS_RES_STATUS_SYSTEM_ERROR;
				Perror(statp, stderr, "socket(vc)", errno);
				return (-1);
			}
		}
#ifdef SO_NOSIGPIPE
		/*
		 * Disable generation of SIGPIPE when writing to a closed
		 * socket.  Write should return -1 and set errno to EPIPE
		 * instead.
		 *
		 * Push on even if setsockopt(SO_NOSIGPIPE) fails.
		 */
		(void)_setsockopt(statp->_vcsock, SOL_SOCKET, SO_NOSIGPIPE, &on,
				 sizeof(on));
#endif
		errno = 0;
		if (_connect(statp->_vcsock, nsap, nsaplen) < 0) {
			*terrno = errno;
			*send_status = DNS_RES_STATUS_CONNECTION_REFUSED;
			Aerror(statp, stderr, "connect(vc)", errno, nsap, nsaplen);
			res_nclose(statp);
			return (0);
		}

		statp->_flags |= RES_F_VC;
	}

	/*
	 * Send length & message
	 */
	ns_put16((u_short)buflen, (u_char*)&len);
	iov[0] = evConsIovec(&len, NS_INT16SZ);
	DE_CONST(buf, tmp);
	iov[1] = evConsIovec(tmp, buflen);
	if (_writev(statp->_vcsock, iov, 2) != (NS_INT16SZ + buflen)) {
		*terrno = errno;
		*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
		Perror(statp, stderr, "write failed", errno);
		res_nclose(statp);
		return (0);
	}

	/*
	 * Receive length & response
	 */
 read_len:

	if (res_check_if_exit_requested(statp, notify_token)) {
		*terrno = EINTR;
		*send_status = DNS_RES_STATUS_CANCELLED;
		return (-1);
	}

	cp = ans;
	len = NS_INT16SZ;
	while ((n = _read(statp->_vcsock, (char *)cp, (int)len)) > 0) {
		cp += n;
		if ((len -= n) <= 0)
			break;
	}
	if (n <= 0) {
		*terrno = errno;
		Perror(statp, stderr, "read failed", errno);
		res_nclose(statp);

		/*
		 * A long running process might get its TCP
		 * connection reset if the remote server was
		 * restarted.  Requery the server instead of
		 * trying a new one.  When there is only one
		 * server, this means that a query might work
		 * instead of failing.  We only allow one reset
		 * per query to prevent looping.
		 */
		if (*terrno == ECONNRESET && !connreset) {
			connreset = 1;
			res_nclose(statp);
			goto vc_same_ns;
		}

		*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
		res_nclose(statp);
		return (0);
	}

	resplen = ns_get16(ans);
	if (resplen > anssiz) {
		Dprint(";; response truncated");
		truncating = 1;
		len = anssiz;
	}
	else
		len = resplen;
	if (len < NS_HFIXEDSZ) {
		/*
		 * Undersized message.
		 */
		Dprint(";; undersized: %d", len);
		*terrno = EMSGSIZE;
		*send_status = DNS_RES_STATUS_INVALID_REPLY;
		res_nclose(statp);
		return (0);
	}
	cp = ans;
	while (len != 0 &&
	    (n = _read(statp->_vcsock, (char *)cp, (int)len)) > 0) {
		cp += n;
		len -= n;
	}
	if (n <= 0) {
		*terrno = errno;
		*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
		Perror(statp, stderr, "read(vc)", errno);
		res_nclose(statp);
		return (0);
	}
	if (truncating) {
		/*
		 * Flush rest of answer so connection stays in synch.
		 */
		anhp->tc = 1;
		len = resplen - anssiz;
		while (len != 0) {
			char junk[NS_PACKETSZ];

			n = _read(statp->_vcsock, junk,
			    (len > sizeof junk) ? sizeof junk : len);
			if (n > 0)
				len -= n;
			else
				break;
		}
	}

	/*
	 * If the calling applicating has bailed out of
	 * a previous call and failed to arrange to have
	 * the circuit closed or the server has got
	 * itself confused, then drop the packet and
	 * wait for the correct one.
	 */
	if (hp->id != anhp->id) {
		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			";; old answer (unexpected):",
			ans, (resplen > anssiz) ? anssiz : resplen);
		goto read_len;
	}

	/*
	 * All is well, or the error is fatal.  Signal that the
	 * next nameserver ought not be tried.
	 */

	*fromlen = sizeof(nsap);
	memcpy(from, &nsap, *fromlen);
	return (resplen);
}

static ssize_t
internal_recvfrom(int s, void *buf, size_t len, struct sockaddr *from,
	int *fromlen, int *iface)
{
	struct sockaddr_dl *sdl;
	struct iovec databuffers = { buf, len };
	struct msghdr msg;
	ssize_t n;
	struct cmsghdr *cmp;
	char ancillary[1024], ifname[IF_NAMESIZE];
	struct in6_pktinfo *ip6_info;
	struct sockaddr_in *s4;
	struct sockaddr_in6 *s6;

	if (buf == NULL || len == 0 || from == 0 || fromlen == NULL ||
		*fromlen < sizeof(struct sockaddr) || iface == NULL) {
		errno = EINVAL;
		return -1;
	}

	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_name = (caddr_t)from;
	msg.msg_namelen = *fromlen;
	msg.msg_iov = &databuffers;
	msg.msg_iovlen = 1;
	msg.msg_control = (caddr_t)&ancillary;
	msg.msg_controllen = sizeof(ancillary);

	/* Receive the data */
	n = recvmsg(s, &msg, 0);
	if (n < 0)
		return n;
	if ((msg.msg_controllen < sizeof(struct cmsghdr)) || (msg.msg_flags & MSG_CTRUNC)) {
		errno = EINVAL;
		return -1;
	}

	*fromlen = msg.msg_namelen;

	s4 = (struct sockaddr_in *)from;
	s6 = (struct sockaddr_in6 *)from;

	for (cmp = CMSG_FIRSTHDR(&msg); cmp; cmp = CMSG_NXTHDR(&msg, cmp))
	{
		if ((cmp->cmsg_level == IPPROTO_IP) && (cmp->cmsg_type == IP_RECVIF))
		{
			sdl = (struct sockaddr_dl *)CMSG_DATA(cmp);
			if (sdl->sdl_nlen < IF_NAMESIZE)
			{
				memcpy(ifname, sdl->sdl_data, sdl->sdl_nlen);
				ifname[sdl->sdl_nlen] = 0;
				*iface = if_nametoindex(ifname);
			}
		}
		else if ((cmp->cmsg_level == IPPROTO_IPV6) && (cmp->cmsg_type == IPV6_PKTINFO))
		{
			ip6_info = (struct in6_pktinfo *)CMSG_DATA(cmp);
			*iface = ip6_info->ipi6_ifindex;
		}
	}

	return n;
}

static int
send_dg(res_state statp,
#ifdef USE_KQUEUE
	int kq,
#endif
	const u_char *buf, int buflen, u_char *ans,
	int anssiz, int *terrno, int ns, int tries, int *v_circuit,
	int *gotsomewhere,
	struct sockaddr *from, int *fromlen,
	int notify_token,
	int *send_status)
{
	const HEADER *hp = (const HEADER *) buf;
	HEADER *anhp = (HEADER *) ans;
	const struct sockaddr *nsap;
	int nsaplen;
	struct timespec now, timeout, finish;
#ifndef __APPLE__
	struct sockaddr_storage from;
	ISC_SOCKLEN_T fromlen;
#endif
	int resplen, seconds, n, s;
#ifdef USE_KQUEUE
	struct kevent kv;
#else
#ifdef USE_POLL
	int     polltimeout;
	struct pollfd   pollfd;
#else
	fd_set dsmask;
#endif
#endif
	int iface, rif;
	int *interrupt_pipe;
#ifdef MULTICAST
	int multicast;
#endif
	int nfds;

	if (ans == NULL || terrno == NULL || v_circuit == NULL ||
	    gotsomewhere == NULL || fromlen == NULL || send_status == NULL) {
		return (-1);
	}

	interrupt_pipe = NULL;

	nsap = get_nsaddr(statp, ns);
	nsaplen = get_salen(nsap);
	if (EXT(statp).nssocks[ns] == -1) {
		EXT(statp).nssocks[ns] = _socket(nsap->sa_family,
		    SOCK_DGRAM, 0);
#if !defined(USE_POLL) && !defined(USE_KQUEUE)
		if (EXT(statp).nssocks[ns] > highestFD) {
			res_nclose(statp);
			errno = ENOTSOCK;
		}
#endif
		if (EXT(statp).nssocks[ns] < 0) {
			switch (errno) {
			case EPROTONOSUPPORT:
#ifdef EPFNOSUPPORT
			case EPFNOSUPPORT:
#endif
			case EAFNOSUPPORT:
				Perror(statp, stderr, "socket(dg)", errno);
				return (0);
			default:
				*terrno = errno;
				*send_status = DNS_RES_STATUS_SYSTEM_ERROR;
				Perror(statp, stderr, "socket(dg)", errno);
				return (-1);
			}
		}
#ifndef CANNOT_CONNECT_DGRAM
		/*
		 * On a 4.3BSD+ machine (client and server,
		 * actually), sending to a nameserver datagram
		 * port with no nameserver will cause an
		 * ICMP port unreachable message to be returned.
		 * If our datagram socket is "connected" to the
		 * server, we get an ECONNREFUSED error on the next
		 * socket operation, and select returns if the
		 * error message is received.  We can thus detect
		 * the absence of a nameserver without timing out.
		 *
		 * When the option "insecure1" is specified, we'd
		 * rather expect to see responses from an "unknown"
		 * address.  In order to let the kernel accept such
		 * responses, do not connect the socket here.
		 * XXX: or do we need an explicit option to disable
		 * connecting?
		 */
		if (!(statp->options & RES_INSECURE1) &&
		    _connect(EXT(statp).nssocks[ns], nsap, nsaplen) < 0) {
			Aerror(statp, stderr, "connect(dg)", errno, nsap,
			    nsaplen);
			res_nclose(statp);
			return (0);
		}
#endif /* !CANNOT_CONNECT_DGRAM */
		if (bind_random(EXT(statp).nssocks[ns] , nsap->sa_family) != 0)
			return (-1);
		Dprint(";; new DG socket");
	}

	s = EXT(statp).nssocks[ns];
	rif = 1;
	setsockopt(s, IPPROTO_IP, IP_RECVIF, &rif, sizeof(int));
	setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, &rif, sizeof(int));

#ifdef MULTICAST
	multicast = 0;

	if ((nsap->sa_family == AF_INET) &&
		(IN_MULTICAST(ntohl(((struct sockaddr_in *)nsap)->sin_addr.s_addr)))) {
		multicast = AF_INET;
	} else if ((nsap->sa_family == AF_INET6) &&
		(IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6 *)nsap)->sin6_addr))) {
		multicast = AF_INET6;
	}

	if (multicast != 0)
	{
		struct ifaddrs *ifa, *p;
		struct sockaddr_in *sin4;
		struct sockaddr_in6 *sin6;
		int i, ifnum;

		if (getifaddrs(&ifa) < 0)
		{
				Aerror(statp, stderr, "getifaddrs", errno, nsap, nsaplen);
				res_nclose(statp);
				return DNS_RES_STATUS_SYSTEM_ERROR;
		}

		for (p = ifa; p != NULL; p = p->ifa_next)
		{
			if (p->ifa_addr == NULL) continue;
			if ((p->ifa_flags & IFF_UP) == 0) continue;
			if (p->ifa_addr->sa_family != multicast) continue;
			if ((p->ifa_flags & IFF_MULTICAST) == 0) continue;
			if ((p->ifa_flags & IFF_POINTOPOINT) != 0)
			{
				if ((multicast == AF_INET) &&
						(ntohl(((struct sockaddr_in *)nsap)->sin_addr.s_addr) <=
						INADDR_MAX_LOCAL_GROUP))
					continue;
			}

			sin4 = (struct sockaddr_in *)p->ifa_addr;
			sin6 = (struct sockaddr_in6 *)p->ifa_addr;
			i = -1;
			if (multicast == AF_INET) {
				i = setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
					&sin4->sin_addr, sizeof(sin4->sin_addr));
			} else if (multicast == AF_INET6) {
				ifnum = if_nametoindex(p->ifa_name);
				((struct sockaddr_in6 *)nsap)->sin6_scope_id = ifnum;
				i = setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF,
					&ifnum, sizeof(ifnum));
			}

			if (i < 0)
			{
				Aerror(statp, stderr, "setsockopt", errno, nsap, nsaplen);
				if (multicast == AF_INET6)
					((struct sockaddr_in6 *)nsap)->sin6_scope_id = 0;

				continue;
			}

			if (sendto(s, (const char*)buf, buflen, 0, nsap, nsaplen) != buflen)
			{
				Aerror(statp, stderr, "sendto", errno, nsap, nsaplen);
				if (multicast == AF_INET6)
					((struct sockaddr_in6 *)nsap)->sin6_scope_id = 0;
				continue;
			}

			if (multicast == AF_INET6) ((struct sockaddr_in6 *)nsap)->sin6_scope_id = 0;
		}


		freeifaddrs(ifa);   
	}
	else
	{
#endif /* MULTICAST */

#ifndef CANNOT_CONNECT_DGRAM
	if (statp->options & RES_INSECURE1) {
		if (_sendto(s,
		    (const char*)buf, buflen, 0, nsap, nsaplen) != buflen) {
			Aerror(statp, stderr, "sendto", errno, nsap, nsaplen);
			res_nclose(statp);
			*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
			return (0);
		}
	} else if (send(s, (const char*)buf, buflen, 0) != buflen) {
		Perror(statp, stderr, "send", errno);
		res_nclose(statp);
		*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
		return (0);
	}
#else /* !CANNOT_CONNECT_DGRAM */
	if (_sendto(s, (const char*)buf, buflen, 0, nsap, nsaplen) != buflen) {
		Aerror(statp, stderr, "sendto", errno, nsap, nsaplen);
		res_nclose(statp);
		*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
		return (0);
	}
#endif /* !CANNOT_CONNECT_DGRAM */

#ifdef MULTICAST
	}
#endif /* MULTICAST */

	/*
	 * Wait for reply.
	 */
	seconds = (statp->retrans << tries);
	if (ns > 0)
		seconds /= statp->nscount;
	if (seconds <= 0)
		seconds = 1;
	now = evNowTime();
	timeout = evConsTime(seconds, 0);
	finish = evAddTime(now, timeout);

	if (interrupt_pipe_enabled != 0)
		interrupt_pipe = pthread_getspecific(interrupt_pipe_key);
	goto nonow;

wait:
	now = evNowTime();
nonow:
	if (res_check_if_exit_requested(statp, notify_token)) {
		*send_status = DNS_RES_STATUS_CANCELLED;
		return (-1);
	}

#ifndef USE_POLL
	if (evCmpTime(finish, now) > 0)
		timeout = evSubTime(finish, now);
	else
		timeout = evConsTime(0, 0);
#ifdef USE_KQUEUE
	EV_SET(&kv, s, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, 0);
	n = _kevent(kq, &kv, 1, &kv, 1, &timeout);
#else
	FD_ZERO(&dsmask);
	FD_SET(s, &dsmask);

	nfds = s + 1;
	if ((interrupt_pipe_enabled != 0) && (interrupt_pipe != NULL))
	{
		if (interrupt_pipe[0] >= 0)
		{
			FD_SET(interrupt_pipe[0], &dsmask);
			nfds = MAX(s, interrupt_pipe[0]) + 1;
		}
	}

	n = pselect(nfds, &dsmask, NULL, NULL, &timeout, NULL);
#endif
#else
	timeout = evSubTime(finish, now);
	if (timeout.tv_sec < 0)
		timeout = evConsTime(0, 0);
	polltimeout = 1000*timeout.tv_sec +
		timeout.tv_nsec/1000000;
	pollfd.fd = s;
	pollfd.events = POLLRDNORM;
	n = _poll(&pollfd, 1, polltimeout);
#endif /* USE_POLL */

	if (n == 0) {
		Dprint(";; timeout");
		*gotsomewhere = 1;
		*send_status = DNS_RES_STATUS_TIMEOUT;
		return (0);
	}
	if (n < 0) {
		if (errno == EINTR)
			goto wait;
#ifdef USE_KQUEUE
		Perror(statp, stderr, "kevent", errno);
#else
#ifndef USE_POLL
		Perror(statp, stderr, "select", errno);
#else
		Perror(statp, stderr, "poll", errno);
#endif /* USE_POLL */
#endif
		res_nclose(statp);
		*send_status = DNS_RES_STATUS_SYSTEM_ERROR;
		return (0);
	}
#ifdef USE_KQUEUE
	if (kv.ident != s)
		goto wait;
#endif
	/* socket s and/or interrupt pipe got data */
	if (interrupt_pipe_enabled != 0 && interrupt_pipe != NULL &&
	    (interrupt_pipe[0] < 0 || FD_ISSET(interrupt_pipe[0], &dsmask))) {
		Dprint(";; cancelled");
		*send_status = DNS_RES_STATUS_CANCELLED;
		return (-1);
	}

	errno = 0;
	iface = 0;
	resplen = (int)internal_recvfrom(s, (char *)ans, anssiz, from, fromlen, &iface);
	if (resplen <= 0) {
		Perror(statp, stderr, "recvfrom", errno);
		res_nclose(statp);
		*send_status = DNS_RES_STATUS_CONNECTION_FAILED;
		return (0);
	}

	if (nsap->sa_family == AF_INET)
		memcpy(((struct sockaddr_in *)from)->sin_zero, &iface, 4);
	else if (nsap->sa_family == AF_INET6)
		((struct sockaddr_in6 *)from)->sin6_scope_id = iface;

	*gotsomewhere = 1;
	if (resplen < HFIXEDSZ) {
		/*
		 * Undersized message.
		 */
		Dprint(";; undersized: %d", resplen);
		*terrno = EMSGSIZE;
		*send_status = DNS_RES_STATUS_INVALID_REPLY;
		res_nclose(statp);
		return (0);
	}
	if (hp->id != anhp->id) {
		/*
		 * response from old query, ignore it.
		 * XXX - potential security hazard could
		 *	 be detected here.
		 */
		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			";; old answer:",
			ans, (resplen > anssiz) ? anssiz : resplen);
		goto wait;
	}

#ifdef MULTICAST
	if (multicast == 0)
	{
#endif /* MULTICAST */

	if (!(statp->options & RES_INSECURE1) &&
	    !res_ourserver_p(statp, from)) {
		/*
		 * response from wrong server? ignore it.
		 * XXX - potential security hazard could
		 *	 be detected here.
		 */
		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			";; not our server:",
			ans, (resplen > anssiz) ? anssiz : resplen);
		goto wait;
	}

#ifdef MULTICAST
	}
#endif /* MULTICAST */

#ifdef RES_USE_EDNS0
	if (anhp->rcode == FORMERR && (statp->options & RES_USE_EDNS0) != 0U) {
		/*
		 * Do not retry if the server do not understand EDNS0.
		 * The case has to be captured here, as FORMERR packet do not
		 * carry query section, hence res_queriesmatch() returns 0.
		 */
		DprintQ(statp->options & RES_DEBUG,
			"server rejected query with EDNS0:",
			ans, (resplen > anssiz) ? anssiz : resplen);
		/* record the error */
		statp->_flags |= RES_F_EDNS0ERR;
		res_nclose(statp);
		*send_status = DNS_RES_STATUS_CONNECTION_REFUSED;
		return (0);
	}
#endif
	if (!(statp->options & RES_INSECURE2) &&
	    !res_queriesmatch(buf, buf + buflen,
			      ans, ans + anssiz)) {
		/*
		 * response contains wrong query? ignore it.
		 * XXX - potential security hazard could
		 *	 be detected here.
		 */
		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			";; wrong query name:",
			ans, (resplen > anssiz) ? anssiz : resplen);
		res_nclose(statp);
		*send_status = DNS_RES_STATUS_INVALID_REPLY;
		return (0);
	}
	if (anhp->rcode == SERVFAIL ||
	    anhp->rcode == NOTIMP ||
	    anhp->rcode == REFUSED) {
		DprintQ(statp->options & RES_DEBUG,
			"server rejected query:",
			ans, (resplen > anssiz) ? anssiz : resplen);
		res_nclose(statp);
		/* don't retry if called from dig */
		if (!statp->pfcode) {
			*send_status = anhp->rcode;
			return (0);
		}
	}
	if (!(statp->options & RES_IGNTC) && anhp->tc) {
		/*
		 * To get the rest of answer,
		 * use TCP with same server.
		 */
		Dprint(";; truncated answer");
		*v_circuit = 1;
		res_nclose(statp);
		return (1);
	}
	/*
	 * All is well, or the error is fatal.  Signal that the
	 * next nameserver ought not be tried.
	 */
	return (resplen);
}

static void
Aerror(const res_state statp, FILE *file, const char *string, int error,
       const struct sockaddr *address, int alen)
{
	int save = errno;
	char hbuf[NI_MAXHOST];
	char sbuf[NI_MAXSERV];

	if ((statp->options & RES_DEBUG) != 0U) {
		if (getnameinfo(address, alen, hbuf, sizeof(hbuf),
		    sbuf, sizeof(sbuf), niflags)) {
			strncpy(hbuf, "?", sizeof(hbuf) - 1);
			hbuf[sizeof(hbuf) - 1] = '\0';
			strncpy(sbuf, "?", sizeof(sbuf) - 1);
			sbuf[sizeof(sbuf) - 1] = '\0';
		}
		fprintf(file, "res_send: %s ([%s].%s): %s\n",
			string, hbuf, sbuf, strerror(error));
	}
	errno = save;
}

static void
Perror(const res_state statp, FILE *file, const char *string, int error) {
	int save = errno;

	if ((statp->options & RES_DEBUG) != 0U)
		fprintf(file, "res_send: %s: %s\n",
			string, strerror(error));
	errno = save;
}

static int
sock_eq(struct sockaddr *a, struct sockaddr *b) {
	struct sockaddr_in *a4, *b4;
	struct sockaddr_in6 *a6, *b6;

	if (a->sa_family != b->sa_family)
		return 0;
	switch (a->sa_family) {
	case AF_INET:
		a4 = (struct sockaddr_in *)a;
		b4 = (struct sockaddr_in *)b;
		return a4->sin_port == b4->sin_port &&
		    a4->sin_addr.s_addr == b4->sin_addr.s_addr;
	case AF_INET6:
		a6 = (struct sockaddr_in6 *)a;
		b6 = (struct sockaddr_in6 *)b;
		return a6->sin6_port == b6->sin6_port &&
#ifdef HAVE_SIN6_SCOPE_ID
		    a6->sin6_scope_id == b6->sin6_scope_id &&
#endif
		    IN6_ARE_ADDR_EQUAL(&a6->sin6_addr, &b6->sin6_addr);
	default:
		return 0;
	}
}

#if defined(NEED_PSELECT) && !defined(USE_POLL) && !defined(USE_KQUEUE)
/* XXX needs to move to the porting library. */
static int
pselect(int nfds, void *rfds, void *wfds, void *efds,
	struct timespec *tsp, const sigset_t *sigmask)
{
	struct timeval tv, *tvp;
	sigset_t sigs;
	int n;

	if (tsp) {
		tvp = &tv;
		tv = evTimeVal(*tsp);
	} else
		tvp = NULL;
	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &sigs);
	n = select(nfds, rfds, wfds, efds, tvp);
	if (sigmask)
		sigprocmask(SIG_SETMASK, &sigs, NULL);
	if (tsp)
		*tsp = evTimeSpec(tv);
	return (n);
}
#endif
