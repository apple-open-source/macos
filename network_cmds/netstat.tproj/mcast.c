/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 2007 Bruce M. Simpson <bms@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>

/*
 * Print the running system's current multicast group memberships.
 * As this relies on getifmaddrs(), it may not be used with a core file.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_mib.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <ifaddrs.h>
#include <sysexits.h>

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>


#include "netstat.h"

union sockunion {
	struct sockaddr_storage	ss;
	struct sockaddr		sa;
	struct sockaddr_dl	sdl;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};
typedef union sockunion sockunion_t;

/*
 * This may have been defined in <net/if.h>.  Note that if <net/if.h> is
 * to be included it must be included before this header file.
 */
#ifndef	ifa_broadaddr
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
#endif

struct ifmaddrs {
	struct ifmaddrs	*ifma_next;
	struct sockaddr	*ifma_name;
	struct sockaddr	*ifma_addr;
	struct sockaddr	*ifma_lladdr;
};

void ifmalist_dump_af(const struct ifmaddrs * const ifmap, int const af);

#define	SALIGN	(sizeof(uint32_t) - 1)
#define	SA_RLEN(sa)	(sa ? ((sa)->sa_len ? (((sa)->sa_len + SALIGN) & ~SALIGN) : \
			    (SALIGN + 1)) : 0)
#define	MAX_SYSCTL_TRY	5
#define	RTA_MASKS	(RTA_GATEWAY | RTA_IFP | RTA_IFA)

int getifmaddrs(struct ifmaddrs **);
void freeifmaddrs(struct ifmaddrs *);


int
getifmaddrs(struct ifmaddrs **pif)
{
	int icnt = 1;
	int dcnt = 0;
	int ntry = 0;
	size_t len;
	size_t needed;
	int mib[6];
	int i;
	char *buf;
	char *data;
	char *next;
	char *p;
	struct ifma_msghdr2 *ifmam;
	struct ifmaddrs *ifa, *ift;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;             /* protocol */
	mib[3] = 0;             /* wildcard address family */
	mib[4] = NET_RT_IFLIST2;
	mib[5] = 0;             /* no flags */
	do {
		if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
			return (-1);
		if ((buf = malloc(needed)) == NULL)
			return (-1);
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
			if (errno != ENOMEM || ++ntry >= MAX_SYSCTL_TRY) {
				free(buf);
				return (-1);
			}
			free(buf);
			buf = NULL;
		} 
	} while (buf == NULL);

	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_NEWMADDR2:
			ifmam = (struct ifma_msghdr2 *)(void *)rtm;
			if ((ifmam->ifmam_addrs & RTA_IFA) == 0)
				break;
			icnt++;
			p = (char *)(ifmam + 1);
			for (i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifmam->ifmam_addrs &
				    (1 << i)) == 0)
					continue;
				sa = (struct sockaddr *)(void *)p;
				len = SA_RLEN(sa);
				dcnt += len;
				p += len;
			}
			break;
		}
	}

	data = malloc(sizeof(struct ifmaddrs) * icnt + dcnt);
	if (data == NULL) {
		free(buf);
		return (-1);
	}

	ifa = (struct ifmaddrs *)(void *)data;
	data += sizeof(struct ifmaddrs) * icnt;

	memset(ifa, 0, sizeof(struct ifmaddrs) * icnt);
	ift = ifa;

	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		switch (rtm->rtm_type) {
		case RTM_NEWMADDR2:
			ifmam = (struct ifma_msghdr2 *)(void *)rtm;
			if ((ifmam->ifmam_addrs & RTA_IFA) == 0)
				break;

			p = (char *)(ifmam + 1);
			for (i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifmam->ifmam_addrs &
				    (1 << i)) == 0)
					continue;
				sa = (struct sockaddr *)(void *)p;
				len = SA_RLEN(sa);
				switch (i) {
				case RTAX_GATEWAY:
					ift->ifma_lladdr =
					    (struct sockaddr *)(void *)data;
					memcpy(data, p, len);
					data += len;
					break;

				case RTAX_IFP:
					ift->ifma_name =
					    (struct sockaddr *)(void *)data;
					memcpy(data, p, len);
					data += len;
					break;

				case RTAX_IFA:
					ift->ifma_addr =
					    (struct sockaddr *)(void *)data;
					memcpy(data, p, len);
					data += len;
					break;

				default:
					data += len;
					break;
				}
				p += len;
			}
			ift->ifma_next = ift + 1;
			ift = ift->ifma_next;
			break;
		}
	}

	free(buf);

	if (ift > ifa) {
		ift--;
		ift->ifma_next = NULL;
		*pif = ifa;
	} else {
		*pif = NULL;
		free(ifa);
	}
	return (0);
}

void
freeifmaddrs(struct ifmaddrs *ifmp)
{

	free(ifmp);
}

void
ifmalist_dump_af(const struct ifmaddrs * const ifmap, int const af)
{
	const struct ifmaddrs *ifma;
	sockunion_t *psa;
	char myifname[IFNAMSIZ];
#ifdef INET6
	char addrbuf[INET6_ADDRSTRLEN];
#endif
	char *pcolon;
	char *pafname, *pifname, *plladdr = NULL, *pgroup = NULL;
#ifdef INET6
	void *in6addr;
#endif

	switch (af) {
	case AF_INET:
		pafname = "IPv4";
		break;
#ifdef INET6
	case AF_INET6:
		pafname = "IPv6";
		break;
#endif
	case AF_LINK:
		pafname = "Link-layer";
		break;
	default:
		return;		/* XXX */
	}

	fprintf(stdout, "%s Multicast Group Memberships\n", pafname);
	fprintf(stdout, "%-20s\t%-16s\t%s\n", "Group", "Link-layer Address",
	    "Netif");

	for (ifma = ifmap; ifma; ifma = ifma->ifma_next) {

		if (ifma->ifma_name == NULL || ifma->ifma_addr == NULL)
			continue;

		/* Group address */
		psa = (sockunion_t *)ifma->ifma_addr;
		if (psa->sa.sa_family != af)
			continue;

		switch (psa->sa.sa_family) {
		case AF_INET:
			pgroup = inet_ntoa(psa->sin.sin_addr);
			break;
#ifdef INET6
		case AF_INET6:
			in6addr = &psa->sin6.sin6_addr;
			inet_ntop(psa->sa.sa_family, in6addr, addrbuf,
			    sizeof(addrbuf));
			pgroup = addrbuf;
			break;
#endif
		case AF_LINK:
			if ((psa->sdl.sdl_alen == ETHER_ADDR_LEN) ||
			    (psa->sdl.sdl_type == IFT_ETHER)) {
				pgroup =
ether_ntoa((struct ether_addr *)&psa->sdl.sdl_data);
#ifdef notyet
			} else {
				pgroup = addr2ascii(AF_LINK,
				    &psa->sdl,
				    sizeof(struct sockaddr_dl),
				    addrbuf);
#endif
			}
			break;
		default:
			continue;	/* XXX */
		}

		/* Link-layer mapping, if any */
		psa = (sockunion_t *)ifma->ifma_lladdr;
		if (psa != NULL) {
			if (psa->sa.sa_family == AF_LINK) {
				if ((psa->sdl.sdl_alen == ETHER_ADDR_LEN) ||
				    (psa->sdl.sdl_type == IFT_ETHER)) {
					/* IEEE 802 */
					plladdr =
ether_ntoa((struct ether_addr *)&psa->sdl.sdl_data);
#ifdef notyet
				} else {
					/* something more exotic */
					plladdr = addr2ascii(AF_LINK,
					    &psa->sdl,
					    sizeof(struct sockaddr_dl),
					    addrbuf);
#endif
				}
			} else {
				int i;
				
				/* not a link-layer address */
				plladdr = "<invalid>";
				
				for (i = 0; psa->sa.sa_len > 2 && i < psa->sa.sa_len - 2; i++)
					printf("0x%x ", psa->sa.sa_data[i]);
				printf("\n");
			}
		} else {
			plladdr = "<none>";
		}

		/* Interface upon which the membership exists */
		psa = (sockunion_t *)ifma->ifma_name;
		if (psa != NULL && psa->sa.sa_family == AF_LINK) {
			strlcpy(myifname, link_ntoa(&psa->sdl), IFNAMSIZ);
			pcolon = strchr(myifname, ':');
			if (pcolon)
				*pcolon = '\0';
			pifname = myifname;
		} else {
			pifname = "";
		}

		fprintf(stdout, "%-20s\t%-16s\t%s\n", pgroup, plladdr, pifname);
	}
}

void
ifmalist_dump(void)
{
	struct ifmaddrs *ifmap;

	if (getifmaddrs(&ifmap))
		err(EX_OSERR, "getifmaddrs");

	ifmalist_dump_af(ifmap, AF_LINK);
	fputs("\n", stdout);
	ifmalist_dump_af(ifmap, AF_INET);
#ifdef INET6
	fputs("\n", stdout);
	ifmalist_dump_af(ifmap, AF_INET6);
#endif

	freeifmaddrs(ifmap);
}

