/*
 * Copyright (c) 2009-2020 Apple Inc. All rights reserved.
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

/*	$KAME: config.c,v 1.84 2003/08/05 12:34:23 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * Copyright (C) 2011 Hiroki Sato <hrs@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <stddef.h>

#include "rtadvd.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"

static time_t prefix_timo = (60 * 120);	/* 2 hours.
					 * XXX: should be configurable. */
extern struct rainfo *ralist;

static struct rtadvd_timer *prefix_timeout(void *);
static void makeentry(char *, size_t, int, char *);
static int getinet6sysctl(int);
static int encode_domain(char *, u_char *);

void
getconfig(intface)
	char *intface;
{
	int stat, i;
	int rdnss_length;
	int dnssl_length;
	char tbuf[BUFSIZ];
	struct rainfo *rai;
	long val;
	int64_t val64;
	char buf[BUFSIZ];
	char *bp = buf;
	char *addr, *flagstr;
	char *capport;
	static int forwarding = -1;

#define MUSTHAVE(var, cap)	\
    do {								\
	int64_t t;							\
	if ((t = agetnum(cap)) < 0) {					\
		fprintf(stderr, "rtadvd: need %s for interface %s\n",	\
			cap, intface);					\
		exit(1);						\
	}								\
	var = t;							\
     } while (0)
#define MAYHAVE(var, cap, def)	\
     do {								\
	if ((var = agetnum(cap)) < 0)					\
		var = def;						\
     } while (0)

	if ((stat = agetent(tbuf, intface)) <= 0) {
		memset(tbuf, 0, sizeof(tbuf));
		errorlog("<%s> %s isn't defined in the configuration file"
		       " or the configuration file doesn't exist."
		       " Treat it as default",
		        __func__, intface);
	}

	ELM_MALLOC(rai, exit(1));
	rai->prefix.next = rai->prefix.prev = &rai->prefix;
#ifdef ROUTEINFO
	rai->route.next = rai->route.prev = &rai->route;
#endif
	rai->rdnss_list.next = rai->rdnss_list.prev = &rai->rdnss_list;
	rai->dnssl_list.next = rai->dnssl_list.prev = &rai->dnssl_list;

	/* check if we are allowed to forward packets (if not determined) */
	if (forwarding < 0) {
		if ((forwarding = getinet6sysctl(IPV6CTL_FORWARDING)) < 0)
			exit(1);
	}

	/* get interface information */
	if (agetflag("nolladdr"))
		rai->advlinkopt = 0;
	else
		rai->advlinkopt = 1;
	if (rai->advlinkopt) {
		if ((rai->sdl = if_nametosdl(intface)) == NULL) {
			errorlog("<%s> can't get information of %s",
			       __func__, intface);
			exit(1);
		}
		rai->ifindex = rai->sdl->sdl_index;
	} else
		rai->ifindex = if_nametoindex(intface);
	strlcpy(rai->ifname, intface, sizeof(rai->ifname));
	if ((rai->phymtu = if_getmtu(intface)) == 0) {
		rai->phymtu = IPV6_MMTU;
		errorlog("<%s> can't get interface mtu of %s. Treat as %d",
		       __func__, intface, IPV6_MMTU);
	}

	/*
	 * set router configuration variables.
	 */
	MAYHAVE(val, "maxinterval", DEF_MAXRTRADVINTERVAL);
	if (val < MIN_MAXINTERVAL || val > MAX_MAXINTERVAL) {
		errorlog("<%s> maxinterval (%ld) on %s is invalid "
		       "(must be between %u and %u)", __func__, val,
		       intface, MIN_MAXINTERVAL, MAX_MAXINTERVAL);
		exit(1);
	}
	rai->maxinterval = (u_int)val;
	MAYHAVE(val, "mininterval", rai->maxinterval/3);
	if (val < MIN_MININTERVAL || val > (rai->maxinterval * 3) / 4) {
		errorlog("<%s> mininterval (%ld) on %s is invalid "
		       "(must be between %d and %d)",
		       __func__, val, intface, MIN_MININTERVAL,
		       (rai->maxinterval * 3) / 4);
		exit(1);
	}
	rai->mininterval = (u_int)val;

	MAYHAVE(val, "chlim", DEF_ADVCURHOPLIMIT);
	rai->hoplimit = val & 0xff;

	if ((flagstr = (char *)agetstr("raflags", &bp))) {
		val = 0;
		if (strchr(flagstr, 'm'))
			val |= ND_RA_FLAG_MANAGED;
		if (strchr(flagstr, 'o'))
			val |= ND_RA_FLAG_OTHER;
		if (strchr(flagstr, 'h'))
			val |= ND_RA_FLAG_RTPREF_HIGH;
		if (strchr(flagstr, 'l')) {
			if ((val & ND_RA_FLAG_RTPREF_HIGH)) {
				errorlog("<%s> the \'h\' and \'l\'"
				    " router flags are exclusive", __func__);
				exit(1);
			}
			val |= ND_RA_FLAG_RTPREF_LOW;
		}
	} else {
		MAYHAVE(val, "raflags", 0);
	}
	rai->managedflg = val & ND_RA_FLAG_MANAGED;
	rai->otherflg = val & ND_RA_FLAG_OTHER;
#ifndef ND_RA_FLAG_RTPREF_MASK
#define ND_RA_FLAG_RTPREF_MASK	0x18 /* 00011000 */
#define ND_RA_FLAG_RTPREF_RSV	0x10 /* 00010000 */
#endif
	rai->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
	if (rai->rtpref == ND_RA_FLAG_RTPREF_RSV) {
		errorlog("<%s> invalid router preference (%02x) on %s",
		       __func__, rai->rtpref, intface);
		exit(1);
	}

	MAYHAVE(val, "rltime", rai->maxinterval * 3);
	if (val && (val < rai->maxinterval || val > MAXROUTERLIFETIME)) {
		errorlog("<%s> router lifetime (%ld) on %s is invalid "
		       "(must be 0 or between %d and %d)",
		       __func__, val, intface,
		       rai->maxinterval,
		       MAXROUTERLIFETIME);
		exit(1);
	}
	/*
	 * Basically, hosts MUST NOT send Router Advertisement messages at any
	 * time (RFC 2461, Section 6.2.3). However, it would sometimes be
	 * useful to allow hosts to advertise some parameters such as prefix
	 * information and link MTU. Thus, we allow hosts to invoke rtadvd
	 * only when router lifetime (on every advertising interface) is
	 * explicitly set zero. (see also the above section)
	 */
	if (val && forwarding == 0) {
		errorlog("<%s> non zero router lifetime is specified for %s, "
		       "which must not be allowed for hosts.  you must "
		       "change router lifetime or enable IPv6 forwarding.",
		       __func__, intface);
		exit(1);
	}
	rai->lifetime = val & 0xffff;

	MAYHAVE(val, "rtime", DEF_ADVREACHABLETIME);
	if (val < 0 || val > MAXREACHABLETIME) {
		errorlog("<%s> reachable time (%ld) on %s is invalid "
		       "(must be no greater than %d)",
		       __func__, val, intface, MAXREACHABLETIME);
		exit(1);
	}
	rai->reachabletime = (u_int32_t)val;

	MAYHAVE(val64, "retrans", DEF_ADVRETRANSTIMER);
	if (val64 < 0 || val64 > 0xffffffff) {
		errorlog("<%s> retrans time (%lld) on %s out of range",
		       __func__, (long long)val64, intface);
		exit(1);
	}
	rai->retranstimer = (u_int32_t)val64;

	if (agetnum("hapref") != -1 || agetnum("hatime") != -1) {
		errorlog("<%s> mobile-ip6 configuration not supported",
		       __func__);
		exit(1);
	}
	/* prefix information */

	/*
	 * This is an implementation specific parameter to consider
	 * link propagation delays and poorly synchronized clocks when
	 * checking consistency of advertised lifetimes.
	 */
	MAYHAVE(val, "clockskew", 0);
	rai->clockskew = val;

	rai->pfxs = 0;
	for (i = -1; i < MAXPREFIX; i++) {
		struct prefix *pfx;
		char entbuf[256];

		makeentry(entbuf, sizeof(entbuf), i, "addr");
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		/* allocate memory to store prefix information */
		ELM_MALLOC(pfx, exit(1));

		pfx->rainfo = rai;
		pfx->origin = PREFIX_FROM_CONFIG;

		if (inet_pton(AF_INET6, addr, &pfx->prefix) != 1) {
			errorlog("<%s> inet_pton failed for %s",
			       __func__, addr);
			exit(1);
		}
		if (IN6_IS_ADDR_MULTICAST(&pfx->prefix)) {
			errorlog("<%s> multicast prefix (%s) must "
			       "not be advertised on %s",
			       __func__, addr, intface);
			exit(1);
		}
		if (IN6_IS_ADDR_LINKLOCAL(&pfx->prefix))
			noticelog("<%s> link-local prefix (%s) will be"
			       " advertised on %s",
			       __func__, addr, intface);

		makeentry(entbuf, sizeof(entbuf), i, "prefixlen");
		MAYHAVE(val, entbuf, 64);
		if (val < 0 || val > 128) {
			errorlog("<%s> prefixlen (%ld) for %s "
			       "on %s out of range",
			       __func__, val, addr, intface);
			exit(1);
		}
		pfx->prefixlen = (int)val;

		makeentry(entbuf, sizeof(entbuf), i, "pinfoflags");
		if ((flagstr = (char *)agetstr(entbuf, &bp))) {
			val = 0;
			if (strchr(flagstr, 'l'))
				val |= ND_OPT_PI_FLAG_ONLINK;
			if (strchr(flagstr, 'a'))
				val |= ND_OPT_PI_FLAG_AUTO;
		} else {
			MAYHAVE(val, entbuf,
			    (ND_OPT_PI_FLAG_ONLINK|ND_OPT_PI_FLAG_AUTO));
		}
		pfx->onlinkflg = val & ND_OPT_PI_FLAG_ONLINK;
		pfx->autoconfflg = val & ND_OPT_PI_FLAG_AUTO;

		makeentry(entbuf, sizeof(entbuf), i, "vltime");
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			errorlog("<%s> vltime (%lld) for "
			    "%s/%d on %s is out of range",
			    __func__, (long long)val64,
			    addr, pfx->prefixlen, intface);
			exit(1);
		}
		pfx->validlifetime = (u_int32_t)val64;

		makeentry(entbuf, sizeof(entbuf), i, "vltimedecr");
		if (agetflag(entbuf)) {
			struct timeval now;
			gettimeofday(&now, 0);
			pfx->vltimeexpire =
				now.tv_sec + pfx->validlifetime;
		}

		makeentry(entbuf, sizeof(entbuf), i, "pltime");
		MAYHAVE(val64, entbuf, DEF_ADVPREFERREDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			errorlog("<%s> pltime (%lld) for %s/%d on %s "
			    "is out of range",
			    __func__, (long long)val64,
			    addr, pfx->prefixlen, intface);
			exit(1);
		}
		pfx->preflifetime = (u_int32_t)val64;

		makeentry(entbuf, sizeof(entbuf), i, "pltimedecr");
		if (agetflag(entbuf)) {
			struct timeval now;
			gettimeofday(&now, 0);
			pfx->pltimeexpire =
				now.tv_sec + pfx->preflifetime;
		}
		/* link into chain */
		insque(pfx, &rai->prefix);
		rai->pfxs++;
	}
	if (rai->pfxs == 0)
		get_prefix(rai);

	MAYHAVE(val, "mtu", 0);
	if (val < 0 || val > 0xffffffff) {
		errorlog("<%s> mtu (%ld) on %s out of range",
		       __func__, val, intface);
		exit(1);
	}
	rai->linkmtu = (u_int32_t)val;
	if (rai->linkmtu == 0) {
		char *mtustr;

		if ((mtustr = (char *)agetstr("mtu", &bp)) &&
		    strcmp(mtustr, "auto") == 0)
			rai->linkmtu = rai->phymtu;
	}
	else if (rai->linkmtu < IPV6_MMTU || rai->linkmtu > rai->phymtu) {
		errorlog("<%s> advertised link mtu (%lu) on %s is invalid (must "
		       "be between least MTU (%d) and physical link MTU (%d)",
		       __func__, (unsigned long)rai->linkmtu, intface,
		       IPV6_MMTU, rai->phymtu);
		exit(1);
	}

#ifdef SIOCSIFINFO_IN6
	{
		struct in6_ndireq ndi;
		int s;

		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			errorlog("<%s> socket: %s", __func__,
			       strerror(errno));
			exit(1);
		}
		memset(&ndi, 0, sizeof(ndi));
		strlcpy(ndi.ifname, intface, sizeof(ndi.ifname));
		if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&ndi) < 0) {
			infolog("<%s> ioctl:SIOCGIFINFO_IN6 at %s: %s",
			     __func__, intface, strerror(errno));
		}

		/* reflect the RA info to the host variables in kernel */
		ndi.ndi.chlim = rai->hoplimit;
		ndi.ndi.retrans = rai->retranstimer;
		ndi.ndi.basereachable = rai->reachabletime;
		if (ioctl(s, SIOCSIFINFO_IN6, (caddr_t)&ndi) < 0) {
			infolog("<%s> ioctl:SIOCSIFINFO_IN6 at %s: %s",
			     __func__, intface, strerror(errno));
		}
		close(s);
	}
#endif

	/* route information */
#ifdef ROUTEINFO
	rai->routes = 0;
	for (i = -1; i < MAXROUTE; i++) {
		struct rtinfo *rti;
		char entbuf[256], oentbuf[256];

		makeentry(entbuf, sizeof(entbuf), i, "rtprefix");
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrprefix");
			addr = (char *)agetstr(oentbuf, &bp);
			if (addr) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			}
		}
		if (addr == NULL)
			continue;

		/* allocate memory to store prefix information */
		ELM_MALLOC(rti, exit(1));

		/* link into chain */
		insque(rti, &rai->route);
		rai->routes++;

		if (inet_pton(AF_INET6, addr, &rti->prefix) != 1) {
			errorlog( "<%s> inet_pton failed for %s",
			       __func__, addr);
			exit(1);
		}
#if 0
		/*
		 * XXX: currently there's no restriction in route information
		 * prefix according to
		 * draft-ietf-ipngwg-router-selection-00.txt.
		 * However, I think the similar restriction be necessary.
		 */
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (IN6_IS_ADDR_MULTICAST(&rti->prefix)) {
			errorlog("<%s> multicast route (%s) must "
			       "not be advertised on %s",
			       __func__, addr, intface);
			exit(1);
		}
		if (IN6_IS_ADDR_LINKLOCAL(&rti->prefix)) {
			noticelog("<%s> link-local route (%s) will "
			       "be advertised on %s",
			       __func__, addr, intface);
			exit(1);
		}
#endif

		makeentry(entbuf, sizeof(entbuf), i, "rtplen");
		/* XXX: 256 is a magic number for compatibility check. */
		MAYHAVE(val, entbuf, 256);
		if (val == 256) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrplen");
			MAYHAVE(val, oentbuf, 256);
			if (val != 256) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			} else
				val = 64;
		}
		if (val < 0 || val > 128) {
			errorlog("<%s> prefixlen (%ld) for %s on %s "
			       "out of range",
			       __func__, val, addr, intface);
			exit(1);
		}
		rti->prefixlen = (int)val;

		makeentry(entbuf, sizeof(entbuf), i, "rtflags");
		if ((flagstr = (char *)agetstr(entbuf, &bp))) {
			val = 0;
			if (strchr(flagstr, 'h'))
				val |= ND_RA_FLAG_RTPREF_HIGH;
			if (strchr(flagstr, 'l')) {
				if ((val & ND_RA_FLAG_RTPREF_HIGH)) {
					errorlog(
					    "<%s> the \'h\' and \'l\' route"
					    " preferences are exclusive",
					    __func__);
					exit(1);
				}
				val |= ND_RA_FLAG_RTPREF_LOW;
			}
		} else
			MAYHAVE(val, entbuf, 256); /* XXX */
		if (val == 256) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrflags");
			MAYHAVE(val, oentbuf, 256);
			if (val != 256) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			} else
				val = 0;
		}
		rti->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
		if (rti->rtpref == ND_RA_FLAG_RTPREF_RSV) {
			errorlog("<%s> invalid route preference (%02x) "
			       "for %s/%d on %s",
			       __func__, rti->rtpref, addr,
			       rti->prefixlen, intface);
			exit(1);
		}

		/*
		 * Since the spec does not a default value, we should make
		 * this entry mandatory.  However, FreeBSD 4.4 has shipped
		 * with this field being optional, we use the router lifetime
		 * as an ad-hoc default value with a warning message.
		 */
		makeentry(entbuf, sizeof(entbuf), i, "rtltime");
		MAYHAVE(val64, entbuf, -1);
		if (val64 == -1) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrltime");
			MAYHAVE(val64, oentbuf, -1);
			if (val64 != -1) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			} else {
				fprintf(stderr, "%s should be specified "
					"for interface %s.\n",
					entbuf, intface);
				val64 = rai->lifetime;
			}
		}
		if (val64 < 0 || val64 > 0xffffffff) {
			errorlog( "<%s> route lifetime (%lld) for "
			    "%s/%d on %s out of range", __func__,
			    (long long)val64, addr, rti->prefixlen, intface);
			exit(1);
		}
		rti->ltime = (u_int32_t)val64;
	}
#endif

	/* RDNSS option (RFC5006) */
	MAYHAVE(val, "rdnsslifetime", 2 * rai->maxinterval);
	if (val < rai->maxinterval || val > (2 * rai->maxinterval)) {
	    noticelog("<%s> rdnsslifetime (%lu) on %s SHOULD "
		   "be between %u and %u", __func__, val,
		   intface, rai->maxinterval, 2 * rai->maxinterval);
	}
	rai->rdnss_lifetime = val;
	if ((rdnss_length = agetnum("rdnssaddrs")) < 0) {
	    rai->rdnss_length = 0;
	}
	else {
	    rai->rdnss_length = rdnss_length;

	    /* traverse in reverse order so that the queue has correct order */
	    for (i = (rdnss_length - 1); i >= 0; i--) {
		struct rdnss *rdnss;
		char entbuf[256];

		/* allocate memory to store server address information */
		ELM_MALLOC(rdnss, exit(1));
		/* link into chain */
		insque(rdnss, &rai->rdnss_list);

		makeentry(entbuf, sizeof(entbuf), i, "rdnssaddr");
		addr = (char *)agetstr(entbuf, &bp);

		if (addr == NULL && rdnss_length == 1) {
		    makeentry(entbuf, sizeof(entbuf), -1, "rdnssaddr");
                    addr = agetstr(entbuf, &bp);
		}

		if (addr == NULL) {
		    errorlog("<%s> need %s as a DNS server address for "
			   "interface %s",
			   __func__, entbuf, intface);
		    exit(1);
		}

		if (inet_pton(AF_INET6, addr, &rdnss->addr) != 1) {
			errorlog("<%s> inet_pton failed for %s",
			       __func__, addr);
			exit(1);
		}
		if (IN6_IS_ADDR_MULTICAST(&rdnss->addr)) {
			errorlog("<%s> multicast address (%s) must "
			       "not be advertised as recursive DNS server",
			       __func__, addr);
			exit(1);
		}
	    }
	}

	/* DNSSL option (RFC6106) */

	/* Parse the DNSSL lifetime from the config */
	MAYHAVE(val, "dnssllifetime", 2 * rai->maxinterval);
	if (val < rai->maxinterval || val > (2 * rai->maxinterval)) {
	    noticelog("<%s> dnssllifetime (%lu) on %s SHOULD "
		   "be between %u and %u", __func__, val,
		   intface, rai->maxinterval, 2 * rai->maxinterval);
	}
	rai->dnssl_lifetime = val;
	rai->dnssl_option_length = 8; /* 8 bytes for the option header */

	/* Parse the DNSSL domain list from the config */
	if ((dnssl_length = agetnum("dnssldomains")) < 0) {
	    rai->dnssl_length = 0;
	} else {
	    rai->dnssl_length = dnssl_length;

	    for (i = (rai->dnssl_length - 1); i >= 0; i--) {
		unsigned char *dnssl_buf;
		struct dnssl *dnssl;
		int dnssl_len;
		char entbuf[sizeof("dnssldomain") + 20];
		char *domain;
		int domain_len;

		makeentry(entbuf, sizeof(entbuf), i, "dnssldomain");
		domain = agetstr(entbuf, &bp);

		if (domain == NULL && rai->dnssl_length == 1) {
		    makeentry(entbuf, sizeof(entbuf), -1, "dnssldomain");
		    domain = agetstr(entbuf, &bp);
		}

		if (domain == NULL) {
		    errorlog("<%s> need %s as a DNS search domain for "
			   "interface %s",
			   __func__, entbuf, intface);
		    exit(1);
		}

		domain_len = strlen(domain);

		/* Trim off leading dots */
		while (domain_len > 0 && domain[0] == '.') {
		    domain++;
		    domain_len--;
		}

		/* Trim off trailing dots */
		while (domain_len > 0 && domain[domain_len-1] == '.') {
		    domain_len--;
		}

		if (domain_len > 0) {
		    dnssl_len = sizeof(struct dnssl) + domain_len + 1;
		    dnssl_buf = (unsigned char *)malloc(dnssl_len);

		    memset(dnssl_buf, 0, dnssl_len);

		    dnssl = (struct dnssl *)dnssl_buf;
		    insque(dnssl, &rai->dnssl_list);

		    /* Copy the domain name in at the end of the dnssl struct */
		    memcpy(dnssl_buf + offsetof(struct dnssl, domain), domain,
		           domain_len);

		    /* Add 2 for leading length byte and the trailing 0 byte */
		    rai->dnssl_option_length += domain_len + 2;
		}
	    }

	    /* Round up to the next multiple of 8 */
	    rai->dnssl_option_length += (8 - (rai->dnssl_option_length & 0x7));
	}

	/* captive portal */
	capport = agetstr("capport", &bp);
	if (capport != NULL) {
		rai->capport = strdup(capport);
		rai->capport_length = strlen(capport);
		rai->capport_option_length
			= sizeof(struct nd_opt_hdr) + rai->capport_length;
		rai->capport_option_length
			+= (8 - (rai->capport_option_length & 0x7));
	}

	/* okey */
	rai->next = ralist;
	ralist = rai;

	/* construct the sending packet */
	make_packet(rai);

	/* set timer */
	rai->timer = rtadvd_add_timer(ra_timeout, ra_timer_update,
				      rai, rai);
	ra_timer_update((void *)rai, &rai->timer->tm);
	rtadvd_set_timer(&rai->timer->tm, rai->timer);
}

void
get_prefix(struct rainfo *rai)
{
	struct ifaddrs *ifap, *ifa;
	struct prefix *pfx;
	struct in6_addr *a;
	u_char *p, *ep, *m, *lim;
	char ntopbuf[INET6_ADDRSTRLEN];

	if (getifaddrs(&ifap) < 0) {
		errorlog(
		       "<%s> can't get interface addresses",
		       __func__);
		exit(1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		int plen;

		if (strcmp(ifa->ifa_name, rai->ifname) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		a = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
		if (IN6_IS_ADDR_LINKLOCAL(a))
			continue;
		/* get prefix length */
		m = (u_char *)&((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr;
		lim = (u_char *)(ifa->ifa_netmask) + ifa->ifa_netmask->sa_len;
		plen = prefixlen(m, lim);
		if (plen <= 0 || plen > 128) {
			errorlog( "<%s> failed to get prefixlen "
			       "or prefix is invalid",
			       __func__);
			exit(1);
		}
		if (plen == 128)	/* XXX */
			continue;
		if (find_prefix(rai, a, plen)) {
			/* ignore a duplicated prefix. */
			continue;
		}

		/* allocate memory to store prefix info. */
		ELM_MALLOC(pfx, exit(1));
		/* set prefix, sweep bits outside of prefixlen */
		pfx->prefixlen = plen;
		memcpy(&pfx->prefix, a, sizeof(*a));
		p = (u_char *)&pfx->prefix;
		ep = (u_char *)(&pfx->prefix + 1);
		while (m < lim && p < ep)
			*p++ &= *m++;
		while (p < ep)
			*p++ = 0x00;
	        if (!inet_ntop(AF_INET6, &pfx->prefix, ntopbuf,
	            sizeof(ntopbuf))) {
			errorlog("<%s> inet_ntop failed", __func__);
			exit(1);
		}
		debuglog("<%s> add %s/%d to prefix list on %s",
		       __func__, ntopbuf, pfx->prefixlen, rai->ifname);

		/* set other fields with protocol defaults */
		pfx->validlifetime = DEF_ADVVALIDLIFETIME;
		pfx->preflifetime = DEF_ADVPREFERREDLIFETIME;
		pfx->onlinkflg = 1;
		pfx->autoconfflg = 1;
		pfx->origin = PREFIX_FROM_KERNEL;
		pfx->rainfo = rai;

		/* link into chain */
		insque(pfx, &rai->prefix);

		/* counter increment */
		rai->pfxs++;
	}

	freeifaddrs(ifap);
}

static void
makeentry(buf, len, id, string)
	char *buf;
	size_t len;
	int id;
	char *string;
{

	if (id < 0)
		strlcpy(buf, string, len);
	else
		snprintf(buf, len, "%s%d", string, id);
}

/*
 * Add a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must not be in the list.
 * XXX: other parameters of the prefix (e.g. lifetime) should be
 * able to be specified.
 */
static void
add_prefix(struct rainfo *rai, struct in6_prefixreq *ipr)
{
	struct prefix *prefix;
	char ntopbuf[INET6_ADDRSTRLEN];

	ELM_MALLOC(prefix, exit(1));
	prefix->prefix = ipr->ipr_prefix.sin6_addr;
	prefix->prefixlen = ipr->ipr_plen;
	prefix->validlifetime = ipr->ipr_vltime;
	prefix->preflifetime = ipr->ipr_pltime;
	prefix->onlinkflg = ipr->ipr_raf_onlink;
	prefix->autoconfflg = ipr->ipr_raf_auto;
	prefix->origin = PREFIX_FROM_DYNAMIC;
	prefix->rainfo = rai;

	insque(prefix, &rai->prefix);

	debuglog("<%s> new prefix %s/%d was added on %s",
	       __func__, inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr,
				       ntopbuf, INET6_ADDRSTRLEN),
	       ipr->ipr_plen, rai->ifname);

	/* free the previous packet */
	free(rai->ra_data);
	rai->ra_data = NULL;

	/* reconstruct the packet */
	rai->pfxs++;
	make_packet(rai);
}

/*
 * Delete a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must be in the list.
 */
void
delete_prefix(struct prefix *prefix)
{
	char ntopbuf[INET6_ADDRSTRLEN];
	struct rainfo *rai = prefix->rainfo;

	remque(prefix);
	debuglog("<%s> prefix %s/%d was deleted on %s",
	       __func__, inet_ntop(AF_INET6, &prefix->prefix,
				       ntopbuf, INET6_ADDRSTRLEN),
	       prefix->prefixlen, rai->ifname);
	if (prefix->timer)
		rtadvd_remove_timer(&prefix->timer);
	free(prefix);
	rai->pfxs--;
}

void
invalidate_prefix(struct prefix *prefix)
{
	char ntopbuf[INET6_ADDRSTRLEN];
	struct timeval timo;
	struct rainfo *rai = prefix->rainfo;

	if (prefix->timer) {	/* sanity check */
		errorlog("<%s> assumption failure: timer already exists",
		    __func__);
		exit(1);
	}

	debuglog("<%s> prefix %s/%d was invalidated on %s, "
	    "will expire in %ld seconds", __func__,
	    inet_ntop(AF_INET6, &prefix->prefix, ntopbuf, INET6_ADDRSTRLEN),
	    prefix->prefixlen, rai->ifname, (long)prefix_timo);

	/* set the expiration timer */
	prefix->timer = rtadvd_add_timer(prefix_timeout, NULL, prefix, NULL);
	if (prefix->timer == NULL) {
		errorlog("<%s> failed to add a timer for a prefix. "
		    "remove the prefix", __func__);
		delete_prefix(prefix);
		return;
	}
	timo.tv_sec = prefix_timo;
	timo.tv_usec = 0;
	rtadvd_set_timer(&timo, prefix->timer);
}

static struct rtadvd_timer *
prefix_timeout(void *arg)
{
	struct prefix *prefix = (struct prefix *)arg;
	
	delete_prefix(prefix);

	return(NULL);
}

void
update_prefix(struct prefix * prefix)
{
	char ntopbuf[INET6_ADDRSTRLEN];
	struct rainfo *rai = prefix->rainfo;

	if (prefix->timer == NULL) { /* sanity check */
		errorlog("<%s> assumption failure: timer does not exist",
		    __func__);
		exit(1);
	}

	debuglog("<%s> prefix %s/%d was re-enabled on %s",
	    __func__, inet_ntop(AF_INET6, &prefix->prefix, ntopbuf,
	    INET6_ADDRSTRLEN), prefix->prefixlen, rai->ifname);

	/* stop the expiration timer */
	rtadvd_remove_timer(&prefix->timer);
}

/*
 * Try to get an in6_prefixreq contents for a prefix which matches
 * ipr->ipr_prefix and ipr->ipr_plen and belongs to
 * the interface whose name is ipr->ipr_name[].
 */
static int
init_prefix(struct in6_prefixreq *ipr)
{
#if 0
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		errorlog("<%s> socket: %s", __func__,
		       strerror(errno));
		exit(1);
	}

	if (ioctl(s, SIOCGIFPREFIX_IN6, (caddr_t)ipr) < 0) {
		infolog("<%s> ioctl:SIOCGIFPREFIX %s", __func__,
		       strerror(errno));

		ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
		ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
		ipr->ipr_raf_onlink = 1;
		ipr->ipr_raf_auto = 1;
		/* omit other field initialization */
	}
	else if (ipr->ipr_origin < PR_ORIG_RR) {
		char ntopbuf[INET6_ADDRSTRLEN];

		noticelog("<%s> Added prefix(%s)'s origin %d is"
		       "lower than PR_ORIG_RR(router renumbering)."
		       "This should not happen if I am router", __func__,
		       inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr, ntopbuf,
				 sizeof(ntopbuf)), ipr->ipr_origin);
		close(s);
		return 1;
	}

	close(s);
	return 0;
#else
	ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
	ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
	ipr->ipr_raf_onlink = 1;
	ipr->ipr_raf_auto = 1;
	return 0;
#endif
}

void
make_prefix(struct rainfo *rai, int ifindex, struct in6_addr *addr, int plen)
{
	struct in6_prefixreq ipr;

	memset(&ipr, 0, sizeof(ipr));
	if (if_indextoname(ifindex, ipr.ipr_name) == NULL) {
		errorlog("<%s> Prefix added interface No.%d doesn't"
		       "exist. This should not happen! %s", __func__,
		       ifindex, strerror(errno));
		exit(1);
	}
	ipr.ipr_prefix.sin6_len = sizeof(ipr.ipr_prefix);
	ipr.ipr_prefix.sin6_family = AF_INET6;
	ipr.ipr_prefix.sin6_addr = *addr;
	ipr.ipr_plen = plen;

	if (init_prefix(&ipr))
		return; /* init failed by some error */
	add_prefix(rai, &ipr);
}

void
make_packet(struct rainfo *rainfo)
{
	size_t packlen, lladdroptlen = 0;
	u_char *buf;
	struct nd_router_advert *ra;
	struct nd_opt_prefix_info *ndopt_pi;
	struct nd_opt_mtu *ndopt_mtu;
#ifdef ROUTEINFO
	struct nd_opt_route_info *ndopt_rti;
	struct rtinfo *rti;
#endif
	struct prefix *pfx;

	/* calculate total length */
	packlen = sizeof(struct nd_router_advert);
	if (rainfo->advlinkopt) {
		if ((lladdroptlen = lladdropt_length(rainfo->sdl)) == 0) {
			infolog("<%s> link-layer address option has"
			       " null length on %s.  Treat as not included.",
			       __func__, rainfo->ifname);
			rainfo->advlinkopt = 0;
		}
		packlen += lladdroptlen;
	}
	if (rainfo->pfxs)
		packlen += sizeof(struct nd_opt_prefix_info) * rainfo->pfxs;
	if (rainfo->linkmtu)
		packlen += sizeof(struct nd_opt_mtu);
#ifdef ROUTEINFO
	for (rti = rainfo->route.next; rti != &rainfo->route; rti = rti->next)
		packlen += sizeof(struct nd_opt_route_info) + 
			   ((rti->prefixlen + 0x3f) >> 6) * 8;
#endif
	if (rainfo->rdnss_length > 0)
		packlen += 8 + sizeof(struct in6_addr) * rainfo->rdnss_length;

	if (rainfo->dnssl_length > 0) {
		packlen += rainfo->dnssl_option_length;
	}
	if (rainfo->capport_option_length != 0) {
		packlen += rainfo->capport_option_length;
	}

	/* allocate memory for the packet */
	if ((buf = malloc(packlen)) == NULL) {
		errorlog("<%s> can't get enough memory for an RA packet",
		       __func__);
		exit(1);
	}
	if (rainfo->ra_data) {
		/* free the previous packet */
		free(rainfo->ra_data);
		rainfo->ra_data = NULL;
	}
	rainfo->ra_data = buf;
	/* XXX: what if packlen > 576? */
	rainfo->ra_datalen = packlen;

	/*
	 * construct the packet
	 */
	ra = (struct nd_router_advert *)buf;
	ra->nd_ra_type = ND_ROUTER_ADVERT;
	ra->nd_ra_code = 0;
	ra->nd_ra_cksum = 0;
	ra->nd_ra_curhoplimit = (u_int8_t)(0xff & rainfo->hoplimit);
	ra->nd_ra_flags_reserved = 0; /* just in case */
	/*
	 * XXX: the router preference field, which is a 2-bit field, should be
	 * initialized before other fields.
	 */
	ra->nd_ra_flags_reserved = 0xff & rainfo->rtpref;
	ra->nd_ra_flags_reserved |=
		rainfo->managedflg ? ND_RA_FLAG_MANAGED : 0;
	ra->nd_ra_flags_reserved |=
		rainfo->otherflg ? ND_RA_FLAG_OTHER : 0;
	ra->nd_ra_router_lifetime = htons(rainfo->lifetime);
	ra->nd_ra_reachable = htonl(rainfo->reachabletime);
	ra->nd_ra_retransmit = htonl(rainfo->retranstimer);
	buf += sizeof(*ra);

	if (rainfo->advlinkopt) {
		lladdropt_fill(rainfo->sdl, (struct nd_opt_hdr *)buf);
		buf += lladdroptlen;
	}

	if (rainfo->linkmtu) {
		ndopt_mtu = (struct nd_opt_mtu *)buf;
		ndopt_mtu->nd_opt_mtu_type = ND_OPT_MTU;
		ndopt_mtu->nd_opt_mtu_len = 1;
		ndopt_mtu->nd_opt_mtu_reserved = 0;
		ndopt_mtu->nd_opt_mtu_mtu = htonl(rainfo->linkmtu);
		buf += sizeof(struct nd_opt_mtu);
	}

	for (pfx = rainfo->prefix.next;
	     pfx != &rainfo->prefix; pfx = pfx->next) {
		u_int32_t vltime, pltime;
		struct timeval now;

		ndopt_pi = (struct nd_opt_prefix_info *)buf;
		ndopt_pi->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
		ndopt_pi->nd_opt_pi_len = 4;
		ndopt_pi->nd_opt_pi_prefix_len = pfx->prefixlen;
		ndopt_pi->nd_opt_pi_flags_reserved = 0;
		if (pfx->onlinkflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_ONLINK;
		if (pfx->autoconfflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_AUTO;
		if (pfx->timer)
			vltime = 0;
		else {
			if (pfx->vltimeexpire || pfx->pltimeexpire)
				gettimeofday(&now, NULL);
			if (pfx->vltimeexpire == 0)
				vltime = pfx->validlifetime;
			else
				vltime = (pfx->vltimeexpire > now.tv_sec) ?
				    pfx->vltimeexpire - now.tv_sec : 0;
		}
		if (pfx->timer)
			pltime = 0;
		else {
			if (pfx->pltimeexpire == 0)
				pltime = pfx->preflifetime;
			else
				pltime = (pfx->pltimeexpire > now.tv_sec) ? 
				    pfx->pltimeexpire - now.tv_sec : 0;
		}
		if (vltime < pltime) {
			/*
			 * this can happen if vltime is decrement but pltime
			 * is not.
			 */
			pltime = vltime;
		}
		ndopt_pi->nd_opt_pi_valid_time = htonl(vltime);
		ndopt_pi->nd_opt_pi_preferred_time = htonl(pltime);
		ndopt_pi->nd_opt_pi_reserved2 = 0;
		ndopt_pi->nd_opt_pi_prefix = pfx->prefix;

		buf += sizeof(struct nd_opt_prefix_info);
	}

#ifdef ROUTEINFO
	for (rti = rainfo->route.next; rti != &rainfo->route; rti = rti->next) {
		u_int8_t psize = (rti->prefixlen + 0x3f) >> 6;

		ndopt_rti = (struct nd_opt_route_info *)buf;
		ndopt_rti->nd_opt_rti_type = ND_OPT_ROUTE_INFO;
		ndopt_rti->nd_opt_rti_len = 1 + psize;
		ndopt_rti->nd_opt_rti_prefixlen = rti->prefixlen;
		ndopt_rti->nd_opt_rti_flags = 0xff & rti->rtpref;
		ndopt_rti->nd_opt_rti_lifetime = htonl(rti->ltime);
		memcpy(ndopt_rti + 1, &rti->prefix, psize * 8);
		buf += sizeof(struct nd_opt_route_info) + psize * 8;
	}
#endif

	if (rainfo->rdnss_length > 0) {	
	    	struct nd_opt_rdnss *	ndopt_rdnss;
		struct rdnss * 		rdnss;

		ndopt_rdnss = (struct nd_opt_rdnss*) buf;
		ndopt_rdnss->nd_opt_rdnss_type = ND_OPT_RDNSS;
		ndopt_rdnss->nd_opt_rdnss_len = 1 + (rainfo->rdnss_length * 2);
		ndopt_rdnss->nd_opt_rdnss_reserved = 0;
		ndopt_rdnss->nd_opt_rdnss_lifetime = htonl(rainfo->rdnss_lifetime);
		buf += 8;
		
		for (rdnss = rainfo->rdnss_list.next;
		     rdnss != &rainfo->rdnss_list;
		     rdnss = rdnss->next)
		{
			struct in6_addr* addr6 = (struct in6_addr*) buf;
			*addr6 = rdnss->addr;
			buf += sizeof *addr6;
		}
	}

	if (rainfo->dnssl_length > 0) {
		struct nd_opt_dnssl *	dnssl_opt;
		struct dnssl *		dnssl;
		int			domains_length = 0;
		u_char *		cursor = buf;

		memset(cursor, 0, rainfo->dnssl_option_length);

		dnssl_opt = (struct nd_opt_dnssl *)cursor;
		dnssl_opt->nd_opt_dnssl_type = ND_OPT_DNSSL;
		/*
		 * Length is in units of 8 octets. Divide total byte length
		 * of the option by 8.
		 */
		dnssl_opt->nd_opt_dnssl_len = rainfo->dnssl_option_length >> 3;
		dnssl_opt->nd_opt_dnssl_reserved = 0;
		dnssl_opt->nd_opt_dnssl_lifetime =
		    htonl(rainfo->dnssl_lifetime);

		cursor += offsetof(struct nd_opt_dnssl, nd_opt_dnssl_domains);

		for (dnssl = rainfo->dnssl_list.next;
		     dnssl != &rainfo->dnssl_list;
		     dnssl = dnssl->next)
		{
			int encodeLen = encode_domain(dnssl->domain, cursor);
			cursor += encodeLen;
			domains_length += encodeLen;
		}

		buf += rainfo->dnssl_option_length;
	}
	if (rainfo->capport != NULL) {
		struct nd_opt_hdr *	capport_opt;
		u_int32_t		zero_space;

		capport_opt = (struct nd_opt_hdr *)buf;
#ifndef ND_OPT_CAPTIVE_PORTAL
#define ND_OPT_CAPTIVE_PORTAL           37      /* RFC 7710 */
#endif /* ND_OPT_CAPTIVE_PORTAL */
		capport_opt->nd_opt_type = ND_OPT_CAPTIVE_PORTAL;
		capport_opt->nd_opt_len = rainfo->capport_option_length >> 3;
		buf += sizeof(*capport_opt);
		bcopy(rainfo->capport, buf, rainfo->capport_length);
		buf += rainfo->capport_length;
		zero_space = rainfo->capport_option_length
			- rainfo->capport_length;
		if (zero_space > 0) {
			bzero(buf, zero_space);
			buf += zero_space;
		}
	}
	return;
}

static int
getinet6sysctl(int code)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0)
	    < 0) {
		errorlog( "<%s>: failed to get ip6 sysctl(%d): %s",
		       __func__, code,
		       strerror(errno));
		return(-1);
	}
	else
		return(value);
}

/*
 * Encode a domain name into a buffer according to the rules in RFC 1035 section
 * 3.1. Do not use the compression techniques outlined in section 4.1.4.
 */
int
encode_domain(char *domain, u_char *dst)
{
	char *domainCopy = strdup(domain);
	char *input = domainCopy;
	char *label;
	u_char *cursor = dst;

	while ((label = strsep(&input, ".")) != NULL) {
		int label_len = strlen(label) & 0x3f; /* Max length is 63 */
		if (label_len > 0) {
			*cursor = (u_char)label_len;
			cursor++;
			memcpy(cursor, label, label_len);
			cursor += label_len;
		}
	}
	*cursor = 0;
	cursor++;

	free(domainCopy);

	return (cursor - dst);
}
