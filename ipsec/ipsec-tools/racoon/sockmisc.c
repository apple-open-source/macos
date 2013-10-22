/* $Id: sockmisc.c,v 1.17.4.4 2005/10/04 09:54:27 manubsd Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>
#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else
#include <netinet6/ipsec.h>
#endif

#if defined(INET6) && !defined(INET6_ADVAPI) && \
	defined(IP_RECVDSTADDR) && !defined(IPV6_RECVDSTADDR)
#define IPV6_RECVDSTADDR IP_RECVDSTADDR
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include "var.h"
#include "misc.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"
#include "gcmalloc.h"
#include "libpfkey.h"

#ifndef IP_IPSEC_POLICY
#define IP_IPSEC_POLICY 16	/* XXX: from linux/in.h */
#endif

#ifndef IPV6_IPSEC_POLICY
#define IPV6_IPSEC_POLICY 34	/* XXX: from linux/???.h per
				   "Tom Lendacky" <toml@us.ibm.com> */
#endif

const int niflags = 0;

/*
 * compare two sockaddr without port number.
 * OUT:	0: equal.
 *	1: not equal.
 */
int
cmpsaddrwop(addr1, addr2)
	const struct sockaddr_storage *addr1;
	const struct sockaddr_storage *addr2;
{
	caddr_t sa1, sa2;

	if (addr1 == 0 && addr2 == 0)
		return 0;
	if (addr1 == 0 || addr2 == 0)
		return 1;

	if (addr1->ss_len != addr2->ss_len
	 || addr1->ss_family != addr2->ss_family)
		return 1;
	switch (addr1->ss_family) {
	case AF_INET:
		sa1 = (caddr_t)&((struct sockaddr_in *)addr1)->sin_addr;
		sa2 = (caddr_t)&((struct sockaddr_in *)addr2)->sin_addr;
		if (memcmp(sa1, sa2, sizeof(struct in_addr)) != 0)
			return 1;
		break;
#ifdef INET6
	case AF_INET6:
		sa1 = (caddr_t)&((struct sockaddr_in6 *)addr1)->sin6_addr;
		sa2 = (caddr_t)&((struct sockaddr_in6 *)addr2)->sin6_addr;
		if (memcmp(sa1, sa2, sizeof(struct in6_addr)) != 0)
			return 1;
		if (((struct sockaddr_in6 *)addr1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)addr2)->sin6_scope_id)
			return 1;
		break;
#endif
	default:
		return 1;
	}

	return 0;
}

/*
 * compare two sockaddr without port number using prefix.
 * OUT:	0: equal.
 *	1: not equal.
 */
int
cmpsaddrwop_withprefix(const struct sockaddr_storage *addr1, const struct sockaddr_storage *addr2, int prefix)
{
    u_int32_t mask;
    int i;
    
	if (addr1 == 0 && addr2 == 0)
		return 0;
	if (addr1 == 0 || addr2 == 0)
		return 1;
    
	if (addr1->ss_len != addr2->ss_len
        || addr1->ss_family != addr2->ss_family)
		return 1;
	switch (addr1->ss_family) {
        case AF_INET:
            mask = ~0;
            mask <<= 32-prefix;
            if ((((struct sockaddr_in *)addr1)->sin_addr.s_addr & htonl(mask)) != 
                (((struct sockaddr_in *)addr2)->sin_addr.s_addr & htonl(mask)))
                return 1;
            break;
#ifdef INET6
        case AF_INET6:
            for (i = 0; i < 4; i++) {
                if (prefix >= 32) {
                    mask = ~0;
                    prefix -= 32;
                } else if (prefix == 0)
                    mask = 0;
                else {
                    mask = ~0;
                    mask <<= 32-prefix;
                    prefix = 0;
                }
                if ((((struct sockaddr_in6 *)addr1)->sin6_addr.__u6_addr.__u6_addr32[i] & htonl(mask)) != 
                    (((struct sockaddr_in6 *)addr2)->sin6_addr.__u6_addr.__u6_addr32[i] & htonl(mask)))
                    return 1;
            }
            if (((struct sockaddr_in6 *)addr1)->sin6_scope_id !=
                ((struct sockaddr_in6 *)addr2)->sin6_scope_id)
                return 1;
            break;
#endif
        default:
            return 1;
	}
    
	return 0;
}


/*
 * compare two sockaddr with port, taking care wildcard.
 * addr1 is a subject address, addr2 is in a database entry.
 * OUT:	0: equal.
 *	1: not equal.
 */
int
cmpsaddrwild(addr1, addr2)
	const struct sockaddr_storage *addr1;
	const struct sockaddr_storage *addr2;
{
	caddr_t sa1, sa2;
	u_short port1, port2;

	if (addr1 == 0 && addr2 == 0)
		return 0;
	if (addr1 == 0 || addr2 == 0)
		return 1;

	if (addr1->ss_len != addr2->ss_len
	 || addr1->ss_family != addr2->ss_family)
		return 1;

	switch (addr1->ss_family) {
	case AF_INET:
		sa1 = (caddr_t)&((struct sockaddr_in *)addr1)->sin_addr;
		sa2 = (caddr_t)&((struct sockaddr_in *)addr2)->sin_addr;
		port1 = ((struct sockaddr_in *)addr1)->sin_port;
		port2 = ((struct sockaddr_in *)addr2)->sin_port;
		if (!(port1 == IPSEC_PORT_ANY ||
		      port2 == IPSEC_PORT_ANY ||
		      port1 == port2))
			return 1;
		if (memcmp(sa1, sa2, sizeof(struct in_addr)) != 0)
			return 1;
		break;
#ifdef INET6
	case AF_INET6:
		sa1 = (caddr_t)&((struct sockaddr_in6 *)addr1)->sin6_addr;
		sa2 = (caddr_t)&((struct sockaddr_in6 *)addr2)->sin6_addr;
		port1 = ((struct sockaddr_in6 *)addr1)->sin6_port;
		port2 = ((struct sockaddr_in6 *)addr2)->sin6_port;
		if (!(port1 == IPSEC_PORT_ANY ||
		      port2 == IPSEC_PORT_ANY ||
		      port1 == port2))
			return 1;
		if (memcmp(sa1, sa2, sizeof(struct in6_addr)) != 0)
			return 1;
		if (((struct sockaddr_in6 *)addr1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)addr2)->sin6_scope_id)
			return 1;
		break;
#endif
	default:
		return 1;
	}

	return 0;
}

/*
 * compare two sockaddr with strict match on port.
 * OUT:	0: equal.
 *	1: not equal.
 */
int
cmpsaddrstrict(const struct sockaddr_storage *addr1, const struct sockaddr_storage *addr2)
{
	caddr_t sa1, sa2;
	u_short port1, port2;

	if (addr1 == 0 && addr2 == 0)
		return 0;
	if (addr1 == 0 || addr2 == 0)
		return 1;

	if (addr1->ss_len != addr2->ss_len
	 || addr1->ss_family != addr2->ss_family)
		return 1;

	switch (addr1->ss_family) {
	case AF_INET:
		sa1 = (caddr_t)&((struct sockaddr_in *)addr1)->sin_addr;
		sa2 = (caddr_t)&((struct sockaddr_in *)addr2)->sin_addr;
		port1 = ((struct sockaddr_in *)addr1)->sin_port;
		port2 = ((struct sockaddr_in *)addr2)->sin_port;
		if (port1 != port2)
			return 1;
		if (memcmp(sa1, sa2, sizeof(struct in_addr)) != 0)
			return 1;
		break;
#ifdef INET6
	case AF_INET6:
		sa1 = (caddr_t)&((struct sockaddr_in6 *)addr1)->sin6_addr;
		sa2 = (caddr_t)&((struct sockaddr_in6 *)addr2)->sin6_addr;
		port1 = ((struct sockaddr_in6 *)addr1)->sin6_port;
		port2 = ((struct sockaddr_in6 *)addr2)->sin6_port;
		if (port1 != port2)
			return 1;
		if (memcmp(sa1, sa2, sizeof(struct in6_addr)) != 0)
			return 1;
		if (((struct sockaddr_in6 *)addr1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)addr2)->sin6_scope_id)
			return 1;
		break;
#endif
	default:
		return 1;
	}

	return 0;
}

/*
 * compare two sockaddr with strict match on port using prefix.
 * OUT:	0: equal.
 *	1: not equal.
 */
int
cmpsaddrstrict_withprefix(const struct sockaddr_storage *addr1, const struct sockaddr_storage *addr2, int prefix)
{
	u_short port1, port2;
    u_int32_t mask;
    int i;
    
	if (addr1 == 0 && addr2 == 0)
		return 0;
	if (addr1 == 0 || addr2 == 0)
		return 1;
    
	if (addr1->ss_len != addr2->ss_len
        || addr1->ss_family != addr2->ss_family)
		return 1;
    
	switch (addr1->ss_family) {
        case AF_INET:  
            port1 = ((struct sockaddr_in *)addr1)->sin_port;
            port2 = ((struct sockaddr_in *)addr2)->sin_port;
            if (port1 != port2)
                return 1;
            mask = ~0;
            mask <<= 32-prefix;
            if ((((struct sockaddr_in *)addr1)->sin_addr.s_addr & htonl(mask)) != 
                (((struct sockaddr_in *)addr2)->sin_addr.s_addr & htonl(mask)))
                return 1;
            break;
#ifdef INET6
        case AF_INET6:    
            port1 = ((struct sockaddr_in6 *)addr1)->sin6_port;
            port2 = ((struct sockaddr_in6 *)addr2)->sin6_port;
            if (port1 != port2)
                return 1;
            for (i = 0; i < 4; i++) {
                if (prefix >= 32) {
                    mask = ~0;
                    prefix -= 32;
                } else if (prefix == 0)
                    mask = 0;
                else {
                    mask = ~0;
                    mask <<= 32-prefix;
                    prefix = 0;
                }
                if ((((struct sockaddr_in6 *)addr1)->sin6_addr.__u6_addr.__u6_addr32[i] & htonl(mask)) != 
                    (((struct sockaddr_in6 *)addr2)->sin6_addr.__u6_addr.__u6_addr32[i] & htonl(mask)))
                    return 1;
            }            
            if (((struct sockaddr_in6 *)addr1)->sin6_scope_id !=
                ((struct sockaddr_in6 *)addr2)->sin6_scope_id)
                return 1;
            break;
#endif
        default:
            return 1;
	}
    
	return 0;
}


/* get local address against the destination. */
struct sockaddr_storage *
getlocaladdr(struct sockaddr *remote)
{
	struct sockaddr_storage *local;
	u_int local_len = sizeof(struct sockaddr);
	int s;	/* for dummy connection */

	/* allocate buffer */
	if ((local = racoon_calloc(1, local_len)) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get address buffer.\n");
		goto err;
	}
	
	/* get real interface received packet */
	if ((s = socket(remote->sa_family, SOCK_DGRAM, 0)) < 0) {
		plog(ASL_LEVEL_ERR, 
			"socket (%s)\n", strerror(errno));
		goto err;
	}

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
		plog(ASL_LEVEL_ERR, "failed to put localaddr socket in non-blocking mode\n");
	}
    
	setsockopt_bypass(s, remote->sa_family);
	
	if (connect(s, remote, sysdep_sa_len(remote)) < 0) {
		plog(ASL_LEVEL_ERR, 
			"connect (%s)\n", strerror(errno));
		close(s);
		goto err;
	}

	if (getsockname(s, (struct sockaddr *)local, &local_len) < 0) {
		plog(ASL_LEVEL_ERR, 
			"getsockname (%s)\n", strerror(errno));
		close(s);
		return NULL;
	}

	close(s);
	return local;

    err:
	if (local != NULL)
		racoon_free(local);
	return NULL;
}

/*
 * Receive packet, with src/dst information.  It is assumed that necessary
 * setsockopt() have already performed on socket.
 */
int
recvfromto(int s,
           void *buf,
           size_t buflen,
           int flags,
           struct sockaddr_storage *from,
           socklen_t *fromlen,
           struct sockaddr_storage *to,
           u_int *tolen)
{
	int otolen;
	ssize_t len;
	struct sockaddr_storage ss;
	struct msghdr m;
	struct cmsghdr *cm, *cm_prev;
	struct iovec iov[2];
    u_int32_t cmsgbuf[256/sizeof(u_int32_t)];       // Wcast-align fix - force 32 bit alignment
#if defined(INET6) && defined(INET6_ADVAPI)
	struct in6_pktinfo *pi;
#endif /*INET6_ADVAPI*/
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif

	len = sizeof(ss);
	if (getsockname(s, (struct sockaddr *)&ss, (socklen_t*)&len) < 0) {
		plog(ASL_LEVEL_ERR, 
			"getsockname (%s)\n", strerror(errno));
		return -1;
	}

	m.msg_name = (caddr_t)from;
	m.msg_namelen = *fromlen;
	iov[0].iov_base = (caddr_t)buf;
	iov[0].iov_len = buflen;
	m.msg_iov = iov;
	m.msg_iovlen = 1;
	memset(cmsgbuf, 0, sizeof(cmsgbuf));
	cm = (struct cmsghdr *)cmsgbuf;
	m.msg_control = (caddr_t)cm;
	m.msg_controllen = sizeof(cmsgbuf);
	while ((len = recvmsg(s, &m, flags)) < 0) {
		if (errno == EINTR)
			continue;
		plog(ASL_LEVEL_ERR, "recvmsg (%s)\n", strerror(errno));
		return -1;
	}
	if (len == 0) {
		return 0;
	}
	*fromlen = m.msg_namelen;

	otolen = *tolen;
	*tolen = 0;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&m), cm_prev = NULL;
	     m.msg_controllen != 0 && cm && cm != cm_prev;
	     cm_prev = cm, cm = (struct cmsghdr *)CMSG_NXTHDR(&m, cm)) {
#if 0
		plog(ASL_LEVEL_ERR, 
			"cmsg %d %d\n", cm->cmsg_level, cm->cmsg_type);)
#endif
#if defined(INET6) && defined(INET6_ADVAPI)
		if (ss.ss_family == AF_INET6
		 && cm->cmsg_level == IPPROTO_IPV6
		 && cm->cmsg_type == IPV6_PKTINFO
		 && otolen >= sizeof(*sin6)) {
			pi = ALIGNED_CAST(struct in6_pktinfo *)(CMSG_DATA(cm));
			*tolen = sizeof(*sin6);
			sin6 = (struct sockaddr_in6 *)to;
			memset(sin6, 0, sizeof(*sin6));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_len = sizeof(*sin6);
			memcpy(&sin6->sin6_addr, &pi->ipi6_addr,
				sizeof(sin6->sin6_addr));
			/* XXX other cases, such as site-local? */
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				sin6->sin6_scope_id = pi->ipi6_ifindex;
			else
				sin6->sin6_scope_id = 0;
			sin6->sin6_port =
				((struct sockaddr_in6 *)&ss)->sin6_port;
			otolen = -1;	/* "to" already set */
			continue;
		}
#endif
#if defined(INET6) && defined(IPV6_RECVDSTADDR)
		if (ss.ss_family == AF_INET6
		      && cm->cmsg_level == IPPROTO_IPV6
		      && cm->cmsg_type == IPV6_RECVDSTADDR
		      && otolen >= sizeof(*sin6)) {
			*tolen = sizeof(*sin6);
			sin6 = (struct sockaddr_in6 *)to;
			memset(sin6, 0, sizeof(*sin6));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_len = sizeof(*sin6);
			memcpy(&sin6->sin6_addr, CMSG_DATA(cm),
				sizeof(sin6->sin6_addr));
			sin6->sin6_port =
				((struct sockaddr_in6 *)&ss)->sin6_port;
			otolen = -1;	/* "to" already set */
			continue;
		}
#endif
		if (ss.ss_family == AF_INET
		 && cm->cmsg_level == IPPROTO_IP
		 && cm->cmsg_type == IP_RECVDSTADDR
		 && otolen >= sizeof(*sin)) {
			*tolen = sizeof(*sin);
			sin = (struct sockaddr_in *)to;
			memset(sin, 0, sizeof(*sin));
			sin->sin_family = AF_INET;
			sin->sin_len = sizeof(*sin);
			memcpy(&sin->sin_addr, CMSG_DATA(cm),
				sizeof(sin->sin_addr));
			sin->sin_port = ((struct sockaddr_in *)&ss)->sin_port;
			otolen = -1;	/* "to" already set */
			continue;
		}
	}
    plogdump(ASL_LEVEL_DEBUG, buf, buflen, "@@@@@@ data from readmsg:\n");
	return len;
}

/* send packet, with fixing src/dst address pair. */
int
sendfromto(s, buf, buflen, src, dst, cnt)
	int s, cnt;
	const void *buf;
	size_t buflen;
	struct sockaddr_storage *src;
	struct sockaddr_storage *dst;
{
	struct sockaddr_storage ss;
	int len;
	int i;

	if (src->ss_family != dst->ss_family) {
		plog(ASL_LEVEL_ERR, 
			"address family mismatch\n");
		return -1;
	}

	len = sizeof(ss);
	if (getsockname(s, (struct sockaddr *)&ss, (socklen_t*)&len) < 0) {
		plog(ASL_LEVEL_ERR, 
			"getsockname (%s)\n", strerror(errno));
		return -1;
	}

	plog(ASL_LEVEL_DEBUG, 
		"sockname %s\n", saddr2str((struct sockaddr *)&ss));
	plog(ASL_LEVEL_DEBUG, 
		"send packet from %s\n", saddr2str((struct sockaddr *)src));
	plog(ASL_LEVEL_DEBUG, 
		"send packet to %s\n", saddr2str((struct sockaddr *)dst));

	if (src->ss_family != ss.ss_family) {
		plog(ASL_LEVEL_ERR, 
			"address family mismatch\n");
		return -1;
	}

	switch (src->ss_family) {
#if defined(INET6) && defined(INET6_ADVAPI)
	case AF_INET6:
	    {
		struct msghdr m;
		struct cmsghdr *cm;
		struct iovec iov[2];
        u_int32_t cmsgbuf[256/sizeof(u_int32_t)];   // Wcast-align fix - force 32 bit alignment
		struct in6_pktinfo *pi;
		int ifindex;
		struct sockaddr_in6 src6, dst6;

		memcpy(&src6, src, sizeof(src6));
		memcpy(&dst6, dst, sizeof(dst6));

		/* XXX take care of other cases, such as site-local */
		ifindex = 0;
		if (IN6_IS_ADDR_LINKLOCAL(&src6.sin6_addr)
		 || IN6_IS_ADDR_MULTICAST(&src6.sin6_addr)) {
			ifindex = src6.sin6_scope_id;	/*???*/
		}

		/* XXX some sanity check on dst6.sin6_scope_id */

		/* flowinfo for IKE?  mmm, maybe useful but for now make it 0 */
		src6.sin6_flowinfo = dst6.sin6_flowinfo = 0;

		memset(&m, 0, sizeof(m));
		m.msg_name = (caddr_t)&dst6;
		m.msg_namelen = sizeof(dst6);
		iov[0].iov_base = (char *)buf;
		iov[0].iov_len = buflen;
		m.msg_iov = iov;
		m.msg_iovlen = 1;

		memset(cmsgbuf, 0, sizeof(cmsgbuf));
		cm = (struct cmsghdr *)cmsgbuf;
		m.msg_control = (caddr_t)cm;
		m.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));

		cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_PKTINFO;
		pi = ALIGNED_CAST(struct in6_pktinfo *)CMSG_DATA(cm);
		memcpy(&pi->ipi6_addr, &src6.sin6_addr, sizeof(src6.sin6_addr));
		pi->ipi6_ifindex = ifindex;

		plog(ASL_LEVEL_DEBUG, 
			"src6 %s %d\n",
			saddr2str((struct sockaddr *)&src6),
			src6.sin6_scope_id);
		plog(ASL_LEVEL_DEBUG, 
			"dst6 %s %d\n",
			saddr2str((struct sockaddr *)&dst6),
			dst6.sin6_scope_id);
            
		for (i = 0; i < cnt; i++) {
			len = sendmsg(s, &m, 0 /*MSG_DONTROUTE*/);
			if (len < 0) {
				plog(ASL_LEVEL_ERR, 
					"sendmsg (%s)\n", strerror(errno));
				if (errno != EHOSTUNREACH && errno != ENETDOWN && errno != ENETUNREACH) {
				        return -1;
				}
				// <rdar://problem/6609744> treat these failures like
				// packet loss, in case the network interface is flaky
				len = 0;
			}
			plog(ASL_LEVEL_DEBUG, 
				"%d times of %d bytes message will be sent "
				"to %s\n",
				i + 1, len, saddr2str((struct sockaddr *)dst));
		}

		return len;
	    }
#endif
	default:
	    {
		int needclose = 0;
		int sendsock;

		if (ss.ss_family == src->ss_family && memcmp(&ss, src, sysdep_sa_len((struct sockaddr *)src)) == 0) {
			sendsock = s;
			needclose = 0;
		} else {
			int yes = 1;
			/*
			 * Use newly opened socket for sending packets.
			 * NOTE: this is unsafe, because if the peer is quick enough
			 * the packet from the peer may be queued into sendsock.
			 * Better approach is to prepare bind'ed udp sockets for
			 * each of the interface addresses.
			 */
			sendsock = socket(src->ss_family, SOCK_DGRAM, 0);
			if (sendsock < 0) {
				plog(ASL_LEVEL_ERR, 
					"socket (%s)\n", strerror(errno));
				return -1;
			}
			if (fcntl(sendsock, F_SETFL, O_NONBLOCK) == -1) {
				plog(ASL_LEVEL_ERR, "failed to put sendsock socket in non-blocking mode\n");
			}
			if (setsockopt(sendsock, SOL_SOCKET,
				       SO_REUSEPORT,
				       (void *)&yes, sizeof(yes)) < 0) {
				plog(ASL_LEVEL_ERR, 
					"setsockopt SO_REUSEPORT (%s)\n", 
					strerror(errno));
				close(sendsock);
				return -1;
			}
#ifdef IPV6_USE_MIN_MTU
			if (src->ss_family == AF_INET6 &&
			    setsockopt(sendsock, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
			    (void *)&yes, sizeof(yes)) < 0) {
				plog(ASL_LEVEL_ERR, 
					"setsockopt IPV6_USE_MIN_MTU (%s)\n", 
					strerror(errno));
				close(sendsock);
				return -1;
			}
#endif
			if (setsockopt_bypass(sendsock, src->ss_family) < 0) {
				close(sendsock);
				return -1;
			}

			if (bind(sendsock, (struct sockaddr *)src, sysdep_sa_len((struct sockaddr *)src)) < 0) {
				plog(ASL_LEVEL_ERR, 
					"bind 1 (%s)\n", strerror(errno));
				close(sendsock);
				return -1;
			}
			needclose = 1;
		}
 
        plogdump(ASL_LEVEL_DEBUG, (void*)buf, buflen, "@@@@@@ data being sent:\n");
		
            for (i = 0; i < cnt; i++) {
			len = sendto(sendsock, buf, buflen, 0, (struct sockaddr *)dst, sysdep_sa_len((struct sockaddr *)dst));
			if (len < 0) {
				plog(ASL_LEVEL_ERR, 
					"sendto (%s)\n", strerror(errno));
				if (errno != EHOSTUNREACH && errno != ENETDOWN && errno != ENETUNREACH) {
				        if (needclose)
					        close(sendsock);
					return -1;
				}
				plog(ASL_LEVEL_ERR, 
					"treating socket error (%s) like packet loss\n", strerror(errno));
				// else treat these failures like a packet loss
				len = 0;
			}
			plog(ASL_LEVEL_DEBUG, 
				"%d times of %d bytes message will be sent "
				"to %s\n",
				i + 1, len, saddr2str((struct sockaddr *)dst));
		}
		//plog(ASL_LEVEL_DEBUG, "sent %d bytes", buflen);

		if (needclose)
			close(sendsock);

		return len;
	    }
	}
}

int
setsockopt_bypass(int so, int family)
{
	int level;
	char *buf;
	char *policy;

	switch (family) {
	case AF_INET:
		level = IPPROTO_IP;
		break;
#ifdef INET6
	case AF_INET6:
		level = IPPROTO_IPV6;
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"unsupported address family %d\n", family);
		return -1;
	}

	policy = "in bypass";
	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"ipsec_set_policy (%s)\n",
			ipsec_strerror());
		return -1;
	}
	if (setsockopt(so, level,
	               (level == IPPROTO_IP ?
	                         IP_IPSEC_POLICY : IPV6_IPSEC_POLICY),
	               buf, ipsec_get_policylen(buf)) < 0) {
		plog(ASL_LEVEL_ERR, 
			"setsockopt IP_IPSEC_POLICY (%s)\n",
			strerror(errno));
		return -1;
	}
	racoon_free(buf);

	policy = "out bypass";
	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"ipsec_set_policy (%s)\n",
			ipsec_strerror());
		return -1;
	}
	if (setsockopt(so, level,
	               (level == IPPROTO_IP ?
	                         IP_IPSEC_POLICY : IPV6_IPSEC_POLICY),
	               buf, ipsec_get_policylen(buf)) < 0) {
		plog(ASL_LEVEL_ERR, 
			"setsockopt IP_IPSEC_POLICY (%s)\n",
			strerror(errno));
		return -1;
	}
	racoon_free(buf);

	return 0;
}

struct sockaddr_storage *
newsaddr(int len)
{
	struct sockaddr_storage *new;

	if ((new = racoon_calloc(1, sizeof(*new))) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"%s\n", strerror(errno)); 
		goto out;
	}
	/* initial */
	new->ss_len = len;
out:
	return new;
}

struct sockaddr_storage *
dupsaddr(struct sockaddr_storage *addr)
{
	struct sockaddr_storage *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL) {
		plog(ASL_LEVEL_ERR, "%s\n", strerror(errno)); 
		return NULL;
	}

	memcpy(new, addr, addr->ss_len);
    
	return new;
}

char *
saddr2str(const struct sockaddr *saddr)
{
	static char buf[NI_MAXHOST + NI_MAXSERV + 10];
	char addr[NI_MAXHOST], port[NI_MAXSERV];

	if (saddr == NULL) {
		buf[0] = '\0';
		return buf;
	}

	if (saddr->sa_family == AF_UNSPEC)
		snprintf (buf, sizeof(buf), "%s", "anonymous");
	else {
		GETNAMEINFO(saddr, addr, port);
		snprintf(buf, sizeof(buf), "%s[%s]", addr, port);
	}

	return buf;
}

char *
saddr2str_with_prefix(const struct sockaddr *saddr, int prefix)
{
	static char buf[NI_MAXHOST + NI_MAXSERV + 10];
	char addr[NI_MAXHOST], port[NI_MAXSERV];
    
	if (saddr == NULL) {
		buf[0] = '\0';
		return buf;
	}
    
	if (saddr->sa_family == AF_UNSPEC)
		snprintf (buf, sizeof(buf), "%s", "anonymous");
	else {
		GETNAMEINFO(saddr, addr, port);
		snprintf(buf, sizeof(buf), "%s/%d[%s]", addr, prefix, port);
	}
    
	return buf;
}


char *
saddrwop2str(const struct sockaddr *saddr)
{
	static char buf[NI_MAXHOST + NI_MAXSERV + 10];
	char addr[NI_MAXHOST];

	if (saddr == NULL) {
		buf[0] = '\0';
		return buf;
	}
	
	GETNAMEINFO_NULL(saddr, addr);
	snprintf(buf, sizeof(buf), "%s", addr);

	return buf;
}

char *
naddrwop2str(const struct netaddr *naddr)
{
	static char buf[NI_MAXHOST + 10];
	static const struct sockaddr sa_any;	/* this is initialized to all zeros */
	
	if (naddr == NULL) {
		buf[0] = '\0';
		return buf;
	}
	
	if (memcmp(&naddr->sa, &sa_any, sizeof(sa_any)) == 0)
		snprintf(buf, sizeof(buf), "%s", "any");
	else {
		snprintf(buf, sizeof(buf), "%s", saddrwop2str((struct sockaddr *)&naddr->sa.sa));
		snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "/%ld", naddr->prefix);
	}
	return buf;
}

char *
naddrwop2str_fromto(const char *format, const struct netaddr *saddr,
		    const struct netaddr *daddr)
{
	static char buf[2*(NI_MAXHOST + NI_MAXSERV + 10) + 100];
	char *src, *dst;

	src = racoon_strdup(naddrwop2str(saddr));
	dst = racoon_strdup(naddrwop2str(daddr));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);
	/* WARNING: Be careful about the format string! Don't 
	   ever pass in something that a user can modify!!! */
	snprintf (buf, sizeof(buf), format, src, dst);
	racoon_free (src);
	racoon_free (dst);

	return buf;
}

char *
saddr2str_fromto(format, saddr, daddr)
	const char *format;
	const struct sockaddr *saddr;
	const struct sockaddr *daddr;
{
	static char buf[2*(NI_MAXHOST + NI_MAXSERV + 10) + 100];
	char *src, *dst;

	if (saddr) {
		src = racoon_strdup(saddr2str(saddr));
		STRDUP_FATAL(src);
	} else {
		src = NULL;
	}
	if (daddr) {
		dst = racoon_strdup(saddr2str(daddr));
		STRDUP_FATAL(dst);
	} else {
		dst = NULL;
	}
	/* WARNING: Be careful about the format string! Don't 
	   ever pass in something that a user can modify!!! */
	snprintf (buf, sizeof(buf), format, src? src:"[null]", dst? dst:"[null]");
	if (src) {
		racoon_free (src);
	}
	if (dst) {
		racoon_free (dst);
	}

	return buf;
}

struct sockaddr_storage *
str2saddr(char *host, char *port)
{
	struct addrinfo hints, *res;
	struct sockaddr_storage *saddr;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo(host, port, &hints, &res);
	if (error != 0) {
		plog(ASL_LEVEL_ERR, 
			"getaddrinfo(%s%s%s): %s\n",
			host, port ? "," : "", port ? port : "",
			gai_strerror(error));
		return NULL;
	}
	if (res->ai_next != NULL) {
		plog(ASL_LEVEL_WARNING, 
			"getaddrinfo(%s%s%s): "
			"resolved to multiple address, "
			"taking the first one\n",
			host, port ? "," : "", port ? port : "");
	}
	saddr = newsaddr(sizeof(*saddr));
	if (saddr == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to allocate buffer.\n");
		freeaddrinfo(res);
		return NULL;
	}
	memcpy(saddr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	return saddr;
}

void
mask_sockaddr(a, b, l)
	struct sockaddr_storage *a;
	const struct sockaddr_storage *b;
	size_t l;
{
	size_t i;
	u_int8_t *p, alen;

	switch (b->ss_family) {
	case AF_INET:
		alen = sizeof(struct in_addr);
		p = (u_int8_t *)&((struct sockaddr_in *)a)->sin_addr;
		break;
#ifdef INET6
	case AF_INET6:
		alen = sizeof(struct in6_addr);
		p = (u_int8_t *)&((struct sockaddr_in6 *)a)->sin6_addr;
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid address family: %d\n", b->ss_family);
		exit(1);
	}

	if ((alen << 3) < l) {
		plog(ASL_LEVEL_ERR, 
			"unexpected inconsistency: %d %zu\n", b->ss_family, l);
		exit(1);
	}

	memcpy(a, b, sysdep_sa_len((struct sockaddr *)b));
	p[l / 8] &= (0xff00 >> (l % 8)) & 0xff;
	for (i = l / 8 + 1; i < alen; i++)
		p[i] = 0x00;
}

/* Compute a score describing how "accurate" a netaddr is for a given sockaddr.
 * Examples:
 * 	Return values for address 10.20.30.40 [port 500] and given netaddresses...
 * 		10.10.0.0/16	=> -1	... doesn't match
 * 		0.0.0.0/0	=> 0	... matches, but only 0 bits.
 * 		10.20.0.0/16	=> 16	... 16 bits match
 * 		10.20.30.0/24	=> 24	... guess what ;-)
 * 		10.20.30.40/32	=> 32	... whole address match
 * 		10.20.30.40:500	=> 33	... both address and port match
 * 		10.20.30.40:501	=> -1	... port doesn't match and isn't 0 (=any)
 */
int
naddr_score(const struct netaddr *naddr, const struct sockaddr_storage *saddr)
{
	static const struct netaddr naddr_any;	/* initialized to all-zeros */
	struct sockaddr_storage sa;
	u_int16_t naddr_port, saddr_port;
	int port_score;

	if (!naddr || !saddr) {
		plog(ASL_LEVEL_ERR, 
		     "Call with null args: naddr=%p, saddr=%p\n",
		     naddr, saddr);
		return -1;
	}

	/* Wildcard address matches, but only 0 bits. */
	if (memcmp(naddr, &naddr_any, sizeof(naddr_any)) == 0)
		return 0;

	/* If families don't match we really can't do much... */
	if (naddr->sa.sa.ss_family != saddr->ss_family)
		return -1;
	
	/* If port check fail don't bother to check addresses. */
	naddr_port = extract_port(&naddr->sa.sa);
	saddr_port = extract_port(saddr);
	if (naddr_port == 0 || saddr_port == 0)	/* wildcard match */
		port_score = 0;
	else if (naddr_port == saddr_port)	/* exact match */
		port_score = 1;
	else					/* mismatch :-) */
		return -1;

	/* Here it comes - compare network addresses. */
	mask_sockaddr(&sa, saddr, naddr->prefix);
	if (loglevel >= ASL_LEVEL_DEBUG) {	/* debug only */
		char *a1, *a2, *a3;
		a1 = racoon_strdup(naddrwop2str(naddr));
		a2 = racoon_strdup(saddrwop2str((struct sockaddr *)saddr));
		a3 = racoon_strdup(saddrwop2str((struct sockaddr *)&sa));
		STRDUP_FATAL(a1);
		STRDUP_FATAL(a2);
		STRDUP_FATAL(a3);
		plog(ASL_LEVEL_DEBUG, 
		     "naddr=%s, saddr=%s (masked=%s)\n",
		     a1, a2, a3);
		free(a1);
		free(a2);
		free(a3);
	}
	if (cmpsaddrwop(&sa, &naddr->sa.sa) == 0)
		return naddr->prefix + port_score;

	return -1;
}

/* Some useful functions for sockaddr_storage port manipulations. */
u_int16_t
extract_port (const struct sockaddr_storage *addr)
{
  u_int16_t port = -1;
  
  if (!addr)
    return port;

  switch (addr->ss_family) {
    case AF_INET:
      port = ((struct sockaddr_in *)addr)->sin_port;
      break;
    case AF_INET6:
      port = ((struct sockaddr_in6 *)addr)->sin6_port;
      break;
    default:
      plog(ASL_LEVEL_ERR, "unknown AF: %u\n", addr->ss_family);
      break;
  }

  return ntohs(port);
}

u_int16_t *
get_port_ptr (struct sockaddr_storage *addr)
{
  u_int16_t *port_ptr;

  if (!addr)
    return NULL;

  switch (addr->ss_family) {
    case AF_INET:
      port_ptr = &(((struct sockaddr_in *)addr)->sin_port);
      break;
    case AF_INET6:
      port_ptr = &(((struct sockaddr_in6 *)addr)->sin6_port);
      break;
    default:
      plog(ASL_LEVEL_ERR, "unknown AF: %u\n", addr->ss_family);
      return NULL;
      break;
  }

  return port_ptr;
}

u_int16_t *
set_port (struct sockaddr_storage *addr, u_int16_t new_port)
{
  u_int16_t *port_ptr;

  port_ptr = get_port_ptr (addr);

  if (port_ptr)
    *port_ptr = htons(new_port);

  return port_ptr;
}
