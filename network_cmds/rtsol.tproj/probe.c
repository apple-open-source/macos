/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*	$KAME: probe.c,v 1.10 2000/08/13 06:14:59 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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
 *
 * $FreeBSD: src/usr.sbin/rtsold/probe.c,v 1.2.2.3 2001/07/03 11:02:16 ume Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/queue.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <sys/sysctl.h>

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>

#include "rtsold.h"

static int
getsocket(int *sockp, int proto)
{
	int sock;

	if (*sockp >= 0) {
		return (0);
	}

	if ((sock = socket(AF_INET6, SOCK_RAW, proto)) < 0) {
		return (-1);
	}

	*sockp = sock;

	return (0);
}

static ssize_t
psendpacket(int sock, struct sockaddr_in6 *dst, uint32_t ifindex, int hoplimit,
	const void *data, size_t len)
{
	uint8_t cmsg[CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		CMSG_SPACE(sizeof(int))];
	struct msghdr hdr;
	struct iovec iov;
	struct in6_pktinfo *pi;
	struct cmsghdr *cm;

	memset(&hdr, 0, sizeof(hdr));
	hdr.msg_name = dst;
	hdr.msg_namelen = sizeof(*dst);
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	hdr.msg_control = cmsg;
	hdr.msg_controllen = sizeof(cmsg);

	iov.iov_base = __DECONST(void *, data);
	iov.iov_len = len;

	/* Specify the outbound interface. */
	cm = CMSG_FIRSTHDR(&hdr);
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)(void *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));   /*XXX*/
	pi->ipi6_ifindex = ifindex;

	/* Specify the hop limit of the packet for safety. */
	cm = CMSG_NXTHDR(&hdr, cm);
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_HOPLIMIT;
	cm->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));

	return (sendmsg(sock, &hdr, 0));
}


/*
 * Probe if each router in the default router list is still alive.
 */
int
defrouter_probe(uint32_t ifindex, uint32_t linkid)
{
    static int probesock = -1;
    struct sockaddr_in6 dst;
    struct in6_defrouter *p, *ep;
    char *buf;
    size_t len;
    int mib[4];
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];


	if (ifindex == 0) {
		return (0);
	}
	if (getsocket(&probesock, IPPROTO_NONE) != 0) {
		return (-1);
	}

    mib[0] = CTL_NET;
    mib[1] = PF_INET6;
    mib[2] = IPPROTO_ICMPV6;
    mib[3] = ICMPV6CTL_ND6_DRLIST;
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), NULL, &len, NULL, 0) < 0) {
		return (-1);
	}
	if (len == 0) {
		return (0);
	}

    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    dst.sin6_len = sizeof(dst);

    buf = malloc(len);
	if (buf == NULL) {
		return (-1);
	}

	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), buf, &len, NULL, 0) < 0) {
		return (-1);
	}

	ep = (struct in6_defrouter *)(void *)(buf + len);
    for (p = (struct in6_defrouter *)(void *)buf; p < ep; p++) {
		if (ifindex != p->if_index) {
			continue;
		}
		if (!IN6_IS_ADDR_LINKLOCAL(&p->rtaddr.sin6_addr)) {
			continue;
		}

        dst.sin6_addr = p->rtaddr.sin6_addr;
        dst.sin6_scope_id = linkid;

		warnmsg(LOG_DEBUG, __FUNCTION__, "probe a router %s on %s",
	             inet_ntop(AF_INET6, &dst.sin6_addr, (char *)ntopbuf, INET6_ADDRSTRLEN),
	             if_indextoname(ifindex, (char *)ifnamebuf));

        (void)psendpacket(probesock, &dst, ifindex, 1, NULL, 0);
    }
    free(buf);

    return (0);
}
