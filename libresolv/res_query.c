/*-
 * SPDX-License-Identifier: (ISC AND BSD-3-Clause)
 *
 * Portions Copyright (C) 2004, 2005, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1996-2001, 2003  Internet Software Consortium.
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
 * Copyright (c) 1988, 1993
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
static const char sccsid[] = "@(#)res_query.c	8.1 (Berkeley) 6/4/93";
static const char rcsid[] = "$Id: res_query.c,v 1.11 2008/11/14 02:36:51 marka Exp $";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "port_before.h"
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <dns_sd.h>
#include <notify.h>
#include <notify_private.h>
#include <pthread.h>
#include "res_private.h"
#endif /* __APPLE__ */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#ifdef __APPLE__
#include <arpa/nameser_compat.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "port_after.h"

#include "res_debug.h"

#if PACKETSZ > 1024
#define MAXPACKET	PACKETSZ
#else
#define MAXPACKET	1024
#endif

#ifdef __APPLE__
/* The interrupt mechanism is implemented in res_send.c */
extern int interrupt_pipe_enabled;
extern pthread_key_t interrupt_pipe_key;

#define BILLION 1000000000

/* 
 * Length of a reverse DNS IPv6 address query name, e.g.
 * "9.4.a.f.c.e.e.f.e.e.1.5.4.1.4.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.e.f.ip6.arpa" 
 */
#define IPv6_REVERSE_LEN 72

/* index of the trailing char that must be "8", "9", "A", "a", "b", or "B" */
#define IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR 58

/* index of low-order nibble of embedded scope id */
#define IPv6_REVERSE_LINK_LOCAL_SCOPE_ID_LOW 48

const static uint8_t hexval[128] =
{
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/*  0 - 15 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 16 - 31 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 32 - 47 */
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,		/* 48 - 63 */
	0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 64 - 79 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 80 - 95 */
	0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 96 - 111 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0		/* 112 - 127 */
};

struct res_query_context
{
	u_char *answer;
	size_t anslen;
	size_t ansmaxlen;
	uint16_t lastanstype;
	uint32_t ifnum;
	uint32_t res_flags;
	DNSServiceFlags flags;
	DNSServiceErrorType error;
};

static void
res_query_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t ifIndex,
	DNSServiceErrorType errorCode, const char *fullname, uint16_t rrtype,
	uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl,
	void *ctx)
{
	struct res_query_context *context;
	int n;
	size_t buflen;
	u_char *dnlist[2], *cp;
	HEADER *ans;
	struct in6_addr a6;

	context = (struct res_query_context *)ctx;

	context->flags = flags;
	context->error = errorCode;

	if (errorCode != kDNSServiceErr_NoError)
	{
		DprintC(context->res_flags & RES_DEBUG,
			";; res_query_mDNSResponder callback [%s %hu %hu]: error %u",
			fullname, rrtype, rrclass, errorCode);
		return;
	}

	buflen = context->ansmaxlen - context->anslen;
	if (buflen < NS_HFIXEDSZ)
	{
		DprintC(context->res_flags & RES_DEBUG,
			";; res_query_mDNSResponder callback [%s %hu %hu]: malformed reply",
			fullname, rrtype, rrclass);
		return;
	}

	dnlist[0] = context->answer + NS_HFIXEDSZ;
	dnlist[1] = NULL;

	cp = context->answer + context->anslen;

	n = dn_comp((char *)fullname, cp, (int)buflen, dnlist, &dnlist[1]);
	if (n < 0)
	{
		DprintC(context->res_flags & RES_DEBUG,
			";; res_query_mDNSResponder callback [%s %hu %hu]: name mismatch",
			fullname, rrtype, rrclass);
		return;
	}

	/*
	 * Check that there is enough space in the buffer for the resource name (n),
	 * the resource record data (rdlen) and the resource record header (10).
	 */
	if (buflen < n + rdlen + 10)
	{
		DprintC(context->res_flags & RES_DEBUG,
			";; res_query_mDNSResponder callback [%s %hu %hu]: insufficient buffer space for reply",
			fullname, rrtype, rrclass);
		return;
	}
	
	DprintC(context->res_flags & RES_DEBUG,
		";; res_query_mDNSResponder callback for %s %hu %hu",
		fullname, rrtype, rrclass);

	cp += n;
	buflen -= n;

	putshort(rrtype, cp);
	cp += sizeof(uint16_t);

	putshort(rrclass, cp);
	cp += sizeof(uint16_t);

	putlong(ttl, cp);
	cp += sizeof(uint32_t);

	putshort(rdlen, cp);
	cp += sizeof(uint16_t);

	memcpy(cp, rdata, rdlen);
	cp += rdlen;

	ans = (HEADER *)context->answer;
	ans->ancount = htons(ntohs(ans->ancount) + 1);

	context->anslen = (size_t)(cp - context->answer);

	context->lastanstype = rrtype;

	/* 
	 * Save the interface number for the first AAAA record for link-local addresses.
	 * It's used by getaddrinfo to set the scope id.
	 */
	if ((context->ifnum == 0) && (rrtype == ns_t_aaaa))
	{
		memset(&a6, 0, sizeof(struct in6_addr));
		memcpy(&a6, rdata, sizeof(struct in6_addr));
		if (IN6_IS_ADDR_LINKLOCAL(&a6))
			context->ifnum = ifIndex;
	}
}

static void
h_errno_for_dnssd_err(DNSServiceErrorType dnssd_err, int *h_errno_err)
{
	if (h_errno_err == NULL)
		return;

	switch (dnssd_err)
	{
		case kDNSServiceErr_NoError:
			*h_errno_err = NETDB_SUCCESS;
			break;
		case kDNSServiceErr_Unknown:
			*h_errno_err = NO_RECOVERY;
			break;
		case kDNSServiceErr_NoSuchRecord:
			*h_errno_err = NO_DATA;
			break;
		case kDNSServiceErr_NoSuchName:
			*h_errno_err = HOST_NOT_FOUND;
			break;
		case kDNSServiceErr_NoMemory:
		default:
			*h_errno_err = NETDB_INTERNAL;
			break;
	}
}

static int
_is_rev_link_local(const char *name)
{
	size_t len;
	int i;

	if (name == NULL) return 0;

	len = strlen(name);
	/* name must be at least IPv6_REVERSE_LEN bytes long and may have a
	 * trailing '.', which will be removed in the next step. */
	if (len == 0 || len != IPv6_REVERSE_LEN ||
	    len != IPv6_REVERSE_LEN+1)
		return 0;

	/* check for trailing '.' */
	if (name[len - 1] == '.')
		len--;

	i = IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR;
	if ((name[i] != '8') && (name[i] != '9') && (name[i] != 'A') &&
		(name[i] != 'a') && (name[i] != 'B') && (name[i] != 'b'))
		return 0;

	i = IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR + 1;
	if (strncasecmp(name + i, ".e.f.ip6.arpa", 13)) return 0;

	for (i = 0; i < IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR; i += 2)
	{
		if (name[i] < '0') return 0;
		if ((name[i] > '9') && (name[i] < 'A')) return 0;
		if ((name[i] > 'F') && (name[i] < 'a')) return 0;
		if (name[i] > 'f') return 0;
		if (name[i + 1] != '.') return 0;
	}

	return 1;
}

__attribute__((__visibility__("hidden")))
int
res_query_mDNSResponder(res_state statp, const char *name, int class, int type,
	u_char *answer, int anslen, struct sockaddr *from, uint32_t *fromlen)
{
	DNSServiceRef sdRef;
	DNSServiceErrorType result;
	struct res_query_context context;
	int i, kq, n, wait, cancelled, notify_token, status;
	struct kevent mevent, ievent, event;
	struct timeval ctv;
	struct timespec now, finish, timeout;
	HEADER *ans;
	uint32_t iface;
	uint16_t nibble;
	char *qname, *notify_name;
	int *interrupt_pipe;

	interrupt_pipe = NULL;
	result = 0;
	kq = -1;
	ans = (HEADER *)answer;
	cancelled = 0;
	ans->rcode = 0;

	memset(&context, 0, sizeof(struct res_query_context));

	/* Build a dummy DNS header with question for the answer */
	context.res_flags = (uint32_t)statp->options;
	context.answer = answer;
	context.ansmaxlen = anslen;
	context.anslen = res_nmkquery(statp, ns_o_query, name, class, type,
		NULL, 0, NULL, answer, anslen);
	if (context.anslen <= 0)
		return 0;

	/* Mark DNS packet as a response */
	ans->qr = 1;
	ans->qr = htons(ans->qr);

	/* Pull out Scope ID in link-local reverse queries */
	qname = strdup(name);
	if (qname == NULL)
	{
		h_errno = NO_RECOVERY;
		errno = ENOMEM;
		return -1;
	}
	iface = 0;
	if (_is_rev_link_local(name))
	{
		/* _is_rev_link_local rejects chars > 127 so it's safe to index into hexval */
		i = IPv6_REVERSE_LINK_LOCAL_SCOPE_ID_LOW;
		nibble = hexval[(uint32_t)name[i]];
		iface = nibble;

		i += 2;
		nibble = hexval[(uint32_t)name[i]];
		iface += (nibble << 4);

		i += 2;
		nibble = hexval[(uint32_t)name[i]];
		iface += (nibble << 8);

		i += 2;
		nibble = hexval[(uint32_t)name[i]];
		iface += (nibble << 12);

		if (iface != 0)
		{
			i = IPv6_REVERSE_LINK_LOCAL_SCOPE_ID_LOW;
			qname[i] = '0';
			qname[i + 2] = '0';
			qname[i + 4] = '0';
			qname[i + 6] = '0';
		}
	}

	Dprint(";; res_query_mDNSResponder");

	result = DNSServiceQueryRecord(&sdRef, kDNSServiceFlagsReturnIntermediates,
			iface, qname, type, class, res_query_callback, &context);
	free(qname);

	if (result != 0) return 0;

	/* Use a kqueue to wait for a response from mDNSResponder */
	kq = kqueue();
	if (kq == -1)
		return 0;

	/* determine the maximum time to wait for a result */
	gettimeofday(&ctv, NULL);

	/* N.B. statp->retrans is actually the total timeount in seconds */
	timeout.tv_sec = statp->retrans;
	timeout.tv_nsec = 0;

	finish.tv_sec = ctv.tv_sec + statp->retrans;
	finish.tv_nsec = ctv.tv_usec * 1000;

	/* add mdns reply FD to kqueue */
	EV_SET(&mevent, DNSServiceRefSockFD(sdRef), EVFILT_READ, EV_ADD, 0, 0, 0);
	n = kevent(kq, &mevent, 1, NULL, 0, NULL);
	if (n != 0) {
		close(kq);
		return 0;
	}

	/* add interrupt pipe to kqueue if interrupt is enabled */
	if (interrupt_pipe_enabled != 0)
	{
		interrupt_pipe = pthread_getspecific(interrupt_pipe_key);
		if (interrupt_pipe != NULL)
		{
			if (interrupt_pipe[0] >= 0)
			{
				EV_SET(&ievent, interrupt_pipe[0], EVFILT_READ,
					EV_ADD, 0, 0, (void *)name);
				/*
				 * XXX: Allow this to fail silently (should never
				 * happen, but it would only break interrupts.
				 */
				n = kevent(kq, &ievent, 1, NULL, 0, NULL);
			}
		}
	}

	/*
	 * Get notification token
	 * we use a self-notification token to allow a caller
	 * to signal the thread doing this DNS query to quit.
	 */
	notify_name = NULL;
	notify_token = -1;

	asprintf(&notify_name, "self.thread.%lu", (unsigned long)pthread_self());
	if (notify_name != NULL) 
	{
		status = notify_register_plain(notify_name, &notify_token);
		free(notify_name);
	}

	wait = 1;
	while (wait == 1)
	{
		memset(&event, 0, sizeof(struct kevent));
		n = kevent(kq, NULL, 0, &event, 1, &timeout);

		if (res_check_if_exit_requested(statp, notify_token))
		{
			cancelled = 1;
			break;
		}

		if (n < 0)
		{
			if (errno == EINTR) goto keep_waiting;
			h_errno = NO_RECOVERY;
			wait = 0;
		}
		else if ((n == 0) && (ans->ancount == 0))
		{
			h_errno = TRY_AGAIN;
			wait = 0;
		}
		else if (event.udata == (void *)name)
		{
			/* interrupted */
			Dprint(";; cancelled");
			cancelled = 1;
			break;
		}
		else
		{
			result = DNSServiceProcessResult(sdRef);
			if ((result != 0) || (context.error != 0))
			{
				if (result == 0)
					result = context.error;
				h_errno_for_dnssd_err(result, &h_errno);
				wait = 0;
			}

			if ((ans->ancount > 0) &&
					((context.flags & kDNSServiceFlagsMoreComing) == 0) &&
					((context.lastanstype != ns_t_cname) ||
					(type == ns_t_cname)))
				wait = 0;
		}

	keep_waiting:

		if (wait == 1)
		{
			/* calculate remaining timeout */
			gettimeofday(&ctv, NULL);

			now.tv_sec = ctv.tv_sec;
			now.tv_nsec = ctv.tv_usec * 1000;

			timeout.tv_sec = finish.tv_sec - now.tv_sec;
			if (finish.tv_nsec >= now.tv_nsec)
			{
				timeout.tv_nsec = finish.tv_nsec - now.tv_nsec;
			}
			else
			{
				timeout.tv_nsec = BILLION - now.tv_nsec + finish.tv_nsec;
				timeout.tv_sec--;
			}
		}
	}

	if (notify_token != -1)
		notify_cancel(notify_token);
	DNSServiceRefDeallocate(sdRef);
	close(kq);

	if ((ans->ancount == 0) || (cancelled == 1)) {
		errno = (cancelled == 1) ? EINTR : EAGAIN;
		context.anslen = -1;
	}

	if ((from != NULL) && (fromlen != NULL) && (context.ifnum != 0))
	{
		((struct sockaddr_in6 *)from)->sin6_len = sizeof(struct sockaddr_in6);
		((struct sockaddr_in6 *)from)->sin6_family = AF_INET6;
		((struct sockaddr_in6 *)from)->sin6_addr.__u6_addr.__u6_addr8[15] = 1;
		((struct sockaddr_in6 *)from)->sin6_scope_id = context.ifnum;
		*fromlen = sizeof(struct sockaddr_in6);
	}

	return (int)context.anslen;
}

static int
res_soa_minimum(const u_char *msg, int len)
{
	ns_msg handle;
	const u_char *eom;
	uint32_t i;
	int32_t b;
	int min, soa_min;

	eom = msg + len;

	handle._msg = msg;
	handle._eom = eom;

	if (msg + NS_INT16SZ > eom) return -1;
	NS_GET16(handle._id, msg);

	if (msg + NS_INT16SZ > eom) return -1;
	NS_GET16(handle._flags, msg);

	for (i = 0; i < ns_s_max; i++)
	{
		if (msg + NS_INT16SZ > eom) return -1;
		NS_GET16(handle._counts[i], msg);
	}

	if (handle._counts[ns_s_ns] == 0) return -1;

	/* Skip forward to nameserver section */
	for (i = 0; i < ns_s_ns; i++)
	{
		if (handle._counts[i] == 0) handle._sections[i] = NULL;
		else
		{
			b = ns_skiprr(msg, eom, (ns_sect)i, handle._counts[i]);
			if (b < 0) return -1;

			handle._sections[i] = msg;
			msg += b;
		}
	}

	min = -1;
	for (i = 0; i < handle._counts[ns_s_ns]; i++)
	{
		b = ns_skiprr(msg, eom, ns_s_ns, 1);
		if (b < 0) return -1;

		memcpy(&soa_min, msg + b - sizeof(int32_t), sizeof(int32_t));
		soa_min = ntohl(soa_min);
		if ((i == 0) || (soa_min < min)) min = soa_min;
		msg += b;
	}

	return min;
}

/*
 * Formulate a normal query, send, and await answer.
 * Returned answer is placed in supplied buffer "answer".
 * Perform preliminary check of answer, returning success only
 * if no error is indicated and the answer count is nonzero.
 * Return the size of the response on success, -1 on error.
 * Error number is left in H_ERRNO.
 *
 * Caller must parse answer and determine whether it answers the question.
 */
__attribute__((__visibility__("hidden"))) int
res_nquery_soa_min(res_state statp, const char *name, int class, int type,
	u_char *answer, int anslen, struct sockaddr *from, int *fromlen,int *min)
{
	u_char buf[MAXPACKET];
	HEADER *hp = (HEADER *) answer;
	int n;
	u_int oflags;

	if (min != NULL) *min = -1;

	oflags = statp->_flags;

again:
	__h_errno_set(statp, 0);
	hp->rcode = ns_r_noerror;	/* default */

	Dprint(";; res_query(%s, %d, %d)", name, class, type);

	n = res_nmkquery(statp, ns_o_query, name, class, type, NULL, 0, NULL,
		buf, sizeof(buf));
#ifdef RES_USE_EDNS0
	if (n > 0 && (statp->_flags & RES_F_EDNS0ERR) == 0 &&
			(statp->options & (RES_USE_EDNS0|RES_USE_DNSSEC)) != 0)
		n = res_nopt(statp, n, buf, sizeof(buf), anslen);
#endif
	if (n < 0)
	{
		Dprint(";; res_query: mkquery failed");
		__h_errno_set(statp, NO_RECOVERY);
		return (n);
	}

	n = res_nsend_2(statp, buf, n, answer, anslen, from, fromlen);
	if (n < 0)
	{
#ifdef RES_USE_EDNS0
		/* if the query choked with EDNS0, retry without EDNS0 */
		if ((statp->options & (RES_USE_EDNS0|RES_USE_DNSSEC)) != 0 &&
				((oflags ^ statp->_flags) & RES_F_EDNS0ERR) != 0)
		{
			statp->_flags |= RES_F_EDNS0ERR;
			Dprint(";; res_nquery: retry without EDNS0");
			goto again;
		}
#endif
		Dprint(";; res_query: send error");
		__h_errno_set(statp, TRY_AGAIN);
		return (n);
	}

	if ((hp->rcode == ns_r_nxdomain) || ((hp->rcode == ns_r_noerror) && (ntohs(hp->ancount) == 0)))
	{
		if (min != NULL)
		{
			*min = res_soa_minimum(answer, anslen);
			Dprint(";; res_nquery: SOA minimum TTL = %d", *min);
		}
	}

	if (hp->rcode != ns_r_noerror || ntohs(hp->ancount) == 0)
	{
		Dprint(";; rcode = %d, ancount=%d", hp->rcode,
		       ntohs(hp->ancount));
		switch (hp->rcode)
		{
			case ns_r_nxdomain:
				__h_errno_set(statp, HOST_NOT_FOUND);
				break;
			case ns_r_servfail:
				__h_errno_set(statp, TRY_AGAIN);
				break;
			case ns_r_noerror:
				__h_errno_set(statp, NO_DATA);
				break;
			case ns_r_formerr:
			case ns_r_notimpl:
			case ns_r_refused:
			default:
				__h_errno_set(statp, NO_RECOVERY);
				break;
		}

		return (-1);
	}

	return (n);
}

__attribute__((__visibility__("hidden")))
int
res_nquery_2(res_state statp, const char *name, int class, int type,
	u_char *answer, int anslen, struct sockaddr *from, int *fromlen)
{
	return res_nquery_soa_min(statp, name, class, type, answer, anslen,
		from, fromlen, NULL);
}

/*
 * Perform a call on res_query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
__attribute__((__visibility__("hidden")))
int
res_nquerydomain_2(res_state statp, const char *name, const char *domain,
	int class, int type, u_char *answer, int anslen, struct sockaddr *from,
	int *fromlen)
{
	char nbuf[NS_MAXDNAME];
	const char *longname = nbuf;
	size_t n, d;

	Dprint(";; res_nquerydomain(%s, %s, %d, %d)",
	       name, domain?domain:"<Nil>", class, type);
	if (domain == NULL)
	{
		/*
		 * Check for trailing '.';
		 * copy without '.' if present.
		 */
		n = strlen(name);
		if (n >= NS_MAXDNAME)
		{
			__h_errno_set(statp, NO_RECOVERY);
			return (-1);
		}

		n--;
		if (n >= 0 && name[n] == '.')
		{
			strlcpy(nbuf, name, n);
		}
		else
		{
			longname = name;
		}
	}
	else
	{
		n = strlen(name);
		d = strlen(domain);
		if (n + d + 1 >= NS_MAXDNAME)
		{
			__h_errno_set(statp, NO_RECOVERY);
			return (-1);
		}

		sprintf(nbuf, "%s.%s", name, domain);
	}

	return (res_nquery_2(statp, longname, class, type, answer, anslen, from, fromlen));
}

/*
 * Formulate a normal query, send, and retrieve answer in supplied buffer.
 * Return the size of the response on success, -1 on error.
 * If enabled, implement search rules until answer or unrecoverable failure
 * is detected.  Error code, if any, is left in H_ERRNO.
 */
__attribute__((__visibility__("hidden")))
int
res_nsearch_2(res_state statp, const char *name, int class, int type,
	u_char *answer, int anslen, struct sockaddr *from, int *fromlen)
{
	const char *cp, * const *domain;
	HEADER *hp = (HEADER *) answer;
	char tmp[NS_MAXDNAME];
	u_int dots;
	int trailing_dot, ret, saved_herrno;
	int got_nodata = 0, got_servfail = 0, root_on_list = 0;
	int tried_as_is = 0;
	int searched = 0;

	errno = 0;
	__h_errno_set(statp, HOST_NOT_FOUND);  /* True if we never query. */

	dots = 0;
	for (cp = name; *cp != '\0'; cp++) dots += (*cp == '.');

	trailing_dot = 0;
	if (cp > name && *--cp == '.') trailing_dot++;

	/* If there aren't any dots, it could be a user-level alias. */
	if (!dots && (cp = res_hostalias(statp, name, tmp, sizeof tmp))!= NULL)
		return (res_nquery(statp, cp, class, type, answer, anslen));

	/*
	 * If there are enough dots in the name, let's just give it a
	 * try 'as is'. The threshold can be set with the "ndots" option.
	 * Also, query 'as is', if there is a trailing dot in the name.
	 */
	saved_herrno = -1;
	if (dots >= statp->ndots || trailing_dot)
	{
		ret = res_nquerydomain_2(statp, name, NULL, class, type, answer,
			anslen, from, fromlen);
		if (ret > 0 || trailing_dot)
			return (ret);
		saved_herrno = h_errno;
		tried_as_is++;
	}

	/*
	 * We do at least one level of search if
	 *	- there is no dot and RES_DEFNAME is set, or
	 *	- there is at least one dot, there is no trailing dot,
	 *	  and RES_DNSRCH is set.
	 */
	if ((!dots && (statp->options & RES_DEFNAMES) != 0) ||
		(dots && !trailing_dot && (statp->options & RES_DNSRCH) != 0))
	{
		int done = 0;

		for (domain = (const char * const *)statp->dnsrch; *domain && !done; domain++)
		{
			searched = 1;

			if (domain[0][0] == '\0' || (domain[0][0] == '.' &&
					domain[0][1] == '\0'))
				root_on_list++;

			ret = res_nquerydomain_2(statp, name, *domain, class,
				type, answer, anslen, from, fromlen);
			if (ret > 0)
				return (ret);

			/*
			 * If no server present, give up.
			 * If name isn't found in this domain,
			 * keep trying higher domains in the search list
			 * (if that's enabled).
			 * On a NO_DATA error, keep trying, otherwise
			 * a wildcard entry of another type could keep us
			 * from finding this entry higher in the domain.
			 * If we get some other error (negative answer or
			 * server failure), then stop searching up,
			 * but try the input name below in case it's
			 * fully-qualified.
			 */
			if (errno == ECONNREFUSED)
			{
				__h_errno_set(statp, TRY_AGAIN);
				return (-1);
			}

			switch (statp->res_h_errno)
			{
				case NO_DATA:
					got_nodata++;
					/* FALLTHROUGH */
				case HOST_NOT_FOUND:
					/* keep trying */
					break;
				case TRY_AGAIN:
					if (hp->rcode == ns_r_refused)
					{
						/* try next search element, if any */
						got_servfail++;
						break;
					}
					/* FALLTHROUGH */
				default:
					/* anything else implies that we're done */
					break;
			}
		}
	}

	/*
	 * If the query has not already been tried as is then try it
	 * unless RES_NOTLDQUERY is set and there were no dots.
	 */
	if ((dots || !searched || (statp->options & RES_NOTLDQUERY) == 0) &&
			!(tried_as_is || root_on_list))
	{
		ret = res_nquerydomain_2(statp, name, NULL, class, type, answer,
				anslen, from, fromlen);
		if (ret > 0)
			return (ret);
	}

	/* if we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's H_ERRNO
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless H_ERRNO, that being the one from
	 * the last DNSRCH we did.
	 */
	if (saved_herrno != -1)
		__h_errno_set(statp, saved_herrno);
	else if (got_nodata)
		__h_errno_set(statp, NO_DATA);
	else if (got_servfail)
		__h_errno_set(statp, TRY_AGAIN);
	return (-1);
}

__attribute__((__visibility__("hidden")))
int
__res_nsearch_list_2(res_state statp, const char *name,	int class, int type,
	u_char *answer, int anslen, struct sockaddr *from, int *fromlen,
	int nsearch, char **search)
{
	const char *cp, *domain;
	HEADER *hp = (HEADER *) answer;
	char tmp[NS_MAXDNAME];
	u_int dots;
	int trailing_dot, ret, saved_herrno, i;
	int got_nodata = 0, got_servfail = 0, root_on_list = 0;
	int tried_as_is = 0;
	int searched = 0;

	errno = 0;
	__h_errno_set(statp, HOST_NOT_FOUND);  /* True if we never query. */

	dots = 0;
	for (cp = name; *cp != '\0'; cp++) dots += (*cp == '.');

	trailing_dot = 0;
	if (cp > name && *--cp == '.') trailing_dot++;

	/* If there aren't any dots, it could be a user-level alias. */
	if (!dots && (cp = res_hostalias(statp, name, tmp, sizeof tmp)) != NULL)
		return (res_nquery(statp, cp, class, type, answer, anslen));

	/*
	 * If there are enough dots in the name, let's just give it a
	 * try 'as is'. The threshold can be set with the "ndots" option.
	 * Also, query 'as is', if there is a trailing dot in the name.
	 */
	saved_herrno = -1;
	if (dots >= statp->ndots || trailing_dot)
	{
		ret = res_nquerydomain(statp, name, NULL, class, type, answer, anslen);
		if (ret > 0 || trailing_dot) return ret;
		saved_herrno = h_errno;
		tried_as_is++;
	}

	/*
	 * We do at least one level of search if
	 *	- there is no dot and RES_DEFNAME is set, or
	 *	- there is at least one dot, there is no trailing dot,
	 *	  and RES_DNSRCH is set.
	 */
	if ((!dots && (statp->options & RES_DEFNAMES) != 0) ||
		(dots && !trailing_dot && (statp->options & RES_DNSRCH) != 0))
	{
		for (i = 0; i < nsearch; i++)
		{
			domain = search[i];
			searched = 1;

			if (domain[0] == '\0' || (domain[0] == '.' && domain[1] == '\0'))
				root_on_list++;

			ret = res_nquerydomain_2(statp, name, domain, class, type,
					answer, anslen, from, fromlen);
			if (ret > 0)
				return ret;

			/*
			 * If no server present, give up.
			 * If name isn't found in this domain,
			 * keep trying higher domains in the search list
			 * (if that's enabled).
			 * On a NO_DATA error, keep trying, otherwise
			 * a wildcard entry of another type could keep us
			 * from finding this entry higher in the domain.
			 * If we get some other error (negative answer or
			 * server failure), then stop searching up,
			 * but try the input name below in case it's
			 * fully-qualified.
			 */
			if (errno == ECONNREFUSED)
			{
				__h_errno_set(statp, TRY_AGAIN);
				return -1;
			}

			switch (statp->res_h_errno)
			{
				case NO_DATA:
					got_nodata++;
					/* FALLTHROUGH */
				case HOST_NOT_FOUND:
					/* keep trying */
					break;
				case TRY_AGAIN:
					if (hp->rcode == ns_r_refused)
					{
						/* try next search element, if any */
						got_servfail++;
						break;
					}
					/* FALLTHROUGH */
				default:
					/* anything else implies that we're done */
					break;
			}
		}
	}

	/*
	 * If the query has not already been tried as is then try it
	 * unless RES_NOTLDQUERY is set and there were no dots.
	 */
	if ((dots || !searched || (statp->options & RES_NOTLDQUERY) == 0) &&
			!(tried_as_is || root_on_list))
	{
		ret = res_nquerydomain_2(statp, name, NULL, class, type, answer,
				anslen, from, fromlen);
		if (ret > 0)
			return ret;
	}

	/*
	 * we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's H_ERRNO
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless H_ERRNO, that being the one from
	 * the last DNSRCH we did.
	 */
	if (saved_herrno != -1)
		__h_errno_set(statp, saved_herrno);
	else if(got_nodata)
		__h_errno_set(statp, NO_DATA);
	else if (got_servfail)
		__h_errno_set(statp, TRY_AGAIN);
	return -1;
}
#endif /* __APPLE__ */

/*%
 * Formulate a normal query, send, and await answer.
 * Returned answer is placed in supplied buffer "answer".
 * Perform preliminary check of answer, returning success only
 * if no error is indicated and the answer count is nonzero.
 * Return the size of the response on success, -1 on error.
 * Error number is left in H_ERRNO.
 *
 * Caller must parse answer and determine whether it answers the question.
 */
int
res_nquery(res_state statp,
	   const char *name,	/*%< domain name */
	   int class, int type,	/*%< class and type of query */
	   u_char *answer,	/*%< buffer to put answer */
	   int anslen)		/*%< size of answer buffer */
{
	u_char buf[MAXPACKET];
	HEADER *hp = (HEADER *) answer;
	u_int oflags;
	u_char *rdata;
	int n;

	oflags = statp->_flags;

again:
	hp->rcode = NOERROR;	/*%< default */
	Dprint(";; res_query(%s, %d, %d)", name, class, type);

	n = res_nmkquery(statp, QUERY, name, class, type, NULL, 0, NULL,
			 buf, sizeof(buf));
#ifdef RES_USE_EDNS0
	if (n > 0 && (statp->_flags & RES_F_EDNS0ERR) == 0 &&
	    (statp->options & (RES_USE_EDNS0|RES_USE_DNSSEC|RES_NSID))) {
		n = res_nopt(statp, n, buf, sizeof(buf), anslen);
		if (n > 0 && (statp->options & RES_NSID) != 0U) {
			rdata = &buf[n];
			n = res_nopt_rdata(statp, n, buf, sizeof(buf), rdata,
					   NS_OPT_NSID, 0, NULL);
		}
	}
#endif
	if (n < 0) {
		Dprint(";; res_query: mkquery failed");
		RES_SET_H_ERRNO(statp, NO_RECOVERY);
		return (n);
	}

	n = res_nsend(statp, buf, n, answer, anslen);
	if (n < 0) {
#ifdef RES_USE_EDNS0
		/* if the query choked with EDNS0, retry without EDNS0 */
		if ((statp->options & (RES_USE_EDNS0|RES_USE_DNSSEC)) != 0U &&
		    ((oflags ^ statp->_flags) & RES_F_EDNS0ERR) != 0) {
			statp->_flags |= RES_F_EDNS0ERR;
			Dprint(";; res_nquery: retry without EDNS0");
			goto again;
		}
#endif
		Dprint(";; res_query: send error");
		RES_SET_H_ERRNO(statp, TRY_AGAIN);
		return (n);
	}

	if (hp->rcode != NOERROR || ntohs(hp->ancount) == 0) {
		Dprint(";; rcode = (%s), counts = an:%d ns:%d ar:%d",
		       p_rcode(hp->rcode),
		       ntohs(hp->ancount),
		       ntohs(hp->nscount),
		       ntohs(hp->arcount));
		switch (hp->rcode) {
		case NXDOMAIN:
			RES_SET_H_ERRNO(statp, HOST_NOT_FOUND);
			break;
		case SERVFAIL:
			RES_SET_H_ERRNO(statp, TRY_AGAIN);
			break;
		case NOERROR:
			RES_SET_H_ERRNO(statp, NO_DATA);
			break;
		case FORMERR:
		case NOTIMP:
		case REFUSED:
		default:
			RES_SET_H_ERRNO(statp, NO_RECOVERY);
			break;
		}
		return (-1);
	}
	return (n);
}

/*%
 * Formulate a normal query, send, and retrieve answer in supplied buffer.
 * Return the size of the response on success, -1 on error.
 * If enabled, implement search rules until answer or unrecoverable failure
 * is detected.  Error code, if any, is left in H_ERRNO.
 */
int
res_nsearch(res_state statp,
	    const char *name,	/*%< domain name */
	    int class, int type,	/*%< class and type of query */
	    u_char *answer,	/*%< buffer to put answer */
	    int anslen)		/*%< size of answer */
{
	const char *cp, * const *domain;
	HEADER *hp = (HEADER *) answer;
	char tmp[NS_MAXDNAME];
	u_int dots;
	int trailing_dot, ret, saved_herrno;
	int got_nodata = 0, got_servfail = 0, root_on_list = 0;
	int tried_as_is = 0;
	int searched = 0;

	errno = 0;
	RES_SET_H_ERRNO(statp, HOST_NOT_FOUND);  /*%< True if we never query. */
	dots = 0;
	for (cp = name; *cp != '\0'; cp++)
		dots += (*cp == '.');
	trailing_dot = 0;
	if (cp > name && *--cp == '.')
		trailing_dot++;

	/* If there aren't any dots, it could be a user-level alias. */
	if (!dots && (cp = res_hostalias(statp, name, tmp, sizeof tmp))!= NULL)
		return (res_nquery(statp, cp, class, type, answer, anslen));

	/*
	 * If there are enough dots in the name, let's just give it a
	 * try 'as is'. The threshold can be set with the "ndots" option.
	 * Also, query 'as is', if there is a trailing dot in the name.
	 */
	saved_herrno = -1;
	if (dots >= statp->ndots || trailing_dot) {
		ret = res_nquerydomain(statp, name, NULL, class, type,
					 answer, anslen);
		if (ret > 0 || trailing_dot)
			return (ret);
		if (errno == ECONNREFUSED) {
			RES_SET_H_ERRNO(statp, TRY_AGAIN);
			return (-1);
		}
		switch (statp->res_h_errno) {
		case NO_DATA:
		case HOST_NOT_FOUND:
			break;
		case TRY_AGAIN:
			if (hp->rcode == SERVFAIL)
				break;
			/* FALLTHROUGH */
		default:
			return (-1);
		}
		saved_herrno = statp->res_h_errno;
		tried_as_is++;
	}

	/*
	 * We do at least one level of search if
	 *	- there is no dot and RES_DEFNAME is set, or
	 *	- there is at least one dot, there is no trailing dot,
	 *	  and RES_DNSRCH is set.
	 */
	if ((!dots && (statp->options & RES_DEFNAMES) != 0U) ||
	    (dots && !trailing_dot && (statp->options & RES_DNSRCH) != 0U)) {
		int done = 0;

		for (domain = (const char * const *)statp->dnsrch;
		     *domain && !done;
		     domain++) {
			searched = 1;

			if (domain[0][0] == '\0' ||
			    (domain[0][0] == '.' && domain[0][1] == '\0'))
				root_on_list++;

			if (root_on_list && tried_as_is)
				continue;

			ret = res_nquerydomain(statp, name, *domain,
					       class, type,
					       answer, anslen);
			if (ret > 0)
				return (ret);

			/*
			 * If no server present, give up.
			 * If name isn't found in this domain,
			 * keep trying higher domains in the search list
			 * (if that's enabled).
			 * On a NO_DATA error, keep trying, otherwise
			 * a wildcard entry of another type could keep us
			 * from finding this entry higher in the domain.
			 * If we get some other error (negative answer or
			 * server failure), then stop searching up,
			 * but try the input name below in case it's
			 * fully-qualified.
			 */
			if (errno == ECONNREFUSED) {
				RES_SET_H_ERRNO(statp, TRY_AGAIN);
				return (-1);
			}

			switch (statp->res_h_errno) {
			case NO_DATA:
				got_nodata++;
				/* FALLTHROUGH */
			case HOST_NOT_FOUND:
				/* keep trying */
				break;
			case TRY_AGAIN:
				/*
				 * This can occur due to a server failure
				 * (that is, all listed servers have failed),
				 * or all listed servers have timed out.
				 * ((HEADER *)answer)->rcode may not be set
				 * to SERVFAIL in the case of a timeout.
				 *
				 * Either way we must return TRY_AGAIN in
				 * order to avoid non-deterministic
				 * return codes.
				 * For example, loaded name servers or races
				 * against network startup/validation (dhcp,
				 * ppp, etc) can cause the search to timeout
				 * on one search element, e.g. 'fu.bar.com',
				 * and return a definitive failure on the
				 * next search element, e.g. 'fu.'.
				 */
				got_servfail++;
				if (hp->rcode == SERVFAIL) {
					/* try next search element, if any */
					break;
				}
				/* FALLTHROUGH */
			default:
				/* anything else implies that we're done */
				done++;
				break;
			}

			/* if we got here for some reason other than DNSRCH,
			 * we only wanted one iteration of the loop, so stop.
			 */
			if ((statp->options & RES_DNSRCH) == 0U)
				done++;
		}
	}

	switch (statp->res_h_errno) {
	case NO_DATA:
	case HOST_NOT_FOUND:
		break;
	case TRY_AGAIN:
		if (hp->rcode == SERVFAIL)
			break;
		/* FALLTHROUGH */
	default:
		goto giveup;
	}

	/*
	 * If the query has not already been tried as is then try it
	 * unless RES_NOTLDQUERY is set and there were no dots.
	 */
	if ((dots || !searched || (statp->options & RES_NOTLDQUERY) == 0U) &&
	    !(tried_as_is || root_on_list)) {
		ret = res_nquerydomain(statp, name, NULL, class, type,
				       answer, anslen);
		if (ret > 0)
			return (ret);
	}

	/* if we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's H_ERRNO
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless H_ERRNO, that being the one from
	 * the last DNSRCH we did.
	 */
giveup:
	if (saved_herrno != -1)
		RES_SET_H_ERRNO(statp, saved_herrno);
	else if (got_nodata)
		RES_SET_H_ERRNO(statp, NO_DATA);
	else if (got_servfail)
		RES_SET_H_ERRNO(statp, TRY_AGAIN);
	return (-1);
}

/*%
 * Perform a call on res_query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
int
res_nquerydomain(res_state statp,
	    const char *name,
	    const char *domain,
	    int class, int type,	/*%< class and type of query */
	    u_char *answer,		/*%< buffer to put answer */
	    int anslen)		/*%< size of answer */
{
	char nbuf[MAXDNAME];
	const char *longname = nbuf;
	size_t n, d;

	Dprint(";; res_nquerydomain(%s, %s, %d, %d)",
	       name, domain?domain:"<Nil>", class, type);
	if (domain == NULL) {
		/*
		 * Check for trailing '.';
		 * copy without '.' if present.
		 */
		n = strlen(name);
		if (n >= MAXDNAME) {
			RES_SET_H_ERRNO(statp, NO_RECOVERY);
			return (-1);
		}
		n--;
		if (n >= 0 && name[n] == '.') {
			strncpy(nbuf, name, n);
			nbuf[n] = '\0';
		} else
			longname = name;
	} else {
		n = strlen(name);
		d = strlen(domain);
		if (n + d + 1 >= MAXDNAME) {
			RES_SET_H_ERRNO(statp, NO_RECOVERY);
			return (-1);
		}
		sprintf(nbuf, "%s.%s", name, domain);
	}
	return (res_nquery(statp, longname, class, type, answer, anslen));
}

const char *
res_hostalias(const res_state statp, const char *name, char *dst, size_t siz) {
	char *file, *cp1, *cp2;
	char buf[BUFSIZ];
	FILE *fp;

	if (statp->options & RES_NOALIASES)
		return (NULL);
	if (issetugid())
		return (NULL);
	file = getenv("HOSTALIASES");
	if (file == NULL || (fp = fopen(file, "re")) == NULL)
		return (NULL);
	setbuf(fp, NULL);
	buf[sizeof(buf) - 1] = '\0';
	while (fgets(buf, sizeof(buf), fp)) {
		for (cp1 = buf; *cp1 && !isspace((unsigned char)*cp1); ++cp1)
			;
		if (!*cp1)
			break;
		*cp1 = '\0';
		if (ns_samename(buf, name) == 1) {
			while (isspace((unsigned char)*++cp1))
				;
			if (!*cp1)
				break;
			for (cp2 = cp1 + 1; *cp2 &&
			     !isspace((unsigned char)*cp2); ++cp2)
				;
			*cp2 = '\0';
			strncpy(dst, cp1, siz - 1);
			dst[siz - 1] = '\0';
			fclose(fp);
			return (dst);
		}
	}
	fclose(fp);
	return (NULL);
}

/*! \file */
