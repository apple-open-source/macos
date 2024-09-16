/*
 * Copyright (c) 2002-2024 Apple Inc. All rights reserved.
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

/*	$KAME: rtadvd.h,v 1.26 2003/08/05 12:34:23 itojun Exp $	*/

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
#include "rtadvd_logging.h"

#define	ELM_MALLOC(p, error_action)					\
	do {								\
		p = malloc(sizeof(*p));					\
		if (p == NULL) {					\
			errorlog("<%s> malloc failed: %s",	\
			    __func__, strerror(errno));			\
			error_action;					\
		}							\
		memset(p, 0, sizeof(*p));				\
	} while(0)

#define	ROUTEINFO	1

#define ALLNODES "ff02::1"
#define ALLROUTERS_LINK "ff02::2"
#define ALLROUTERS_SITE "ff05::2"
#define ANY "::"
#define RTSOLLEN 8

/* protocol constants and default values */
#define DEF_MAXRTRADVINTERVAL 600
#define DEF_ADVLINKMTU 0
#define DEF_ADVREACHABLETIME 0
#define DEF_ADVRETRANSTIMER 0
#define DEF_ADVCURHOPLIMIT 64
#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800

#define MAXROUTERLIFETIME 9000
#define MIN_MAXINTERVAL 4
#define MAX_MAXINTERVAL 1800
#define MIN_MININTERVAL 3
#define MAXREACHABLETIME 3600000

#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3
#define MAX_FINAL_RTR_ADVERTISEMENTS      1
#define MIN_DELAY_BETWEEN_RAS             3
#define MAX_RA_DELAY_TIME                 500000 /* usec */

#define PREFIX_FROM_KERNEL 1
#define PREFIX_FROM_CONFIG 2
#define PREFIX_FROM_DYNAMIC 3

typedef uint8_t pref64_prefix_length_t;
typedef uint8_t pref64_plc_t;
typedef uint16_t pref64_scaled_lifetime_plc_t;

struct prefix {
	struct prefix *next;	/* forward link */
	struct prefix *prev;	/* previous link */

	struct rainfo *rainfo;	/* back pointer to the interface */

	struct rtadvd_timer *timer; /* expiration timer.  used when a prefix
				     * derived from the kernel is deleted.
				     */

	u_int32_t validlifetime; /* AdvValidLifetime */
	long	vltimeexpire;	/* expiration of vltime; decrement case only */
	u_int32_t preflifetime;	/* AdvPreferredLifetime */
	long	pltimeexpire;	/* expiration of pltime; decrement case only */
	u_int onlinkflg;	/* bool: AdvOnLinkFlag */
	u_int autoconfflg;	/* bool: AdvAutonomousFlag */
	int prefixlen;
	int origin;		/* from kernel or config */
	struct in6_addr prefix;
};

#ifdef ROUTEINFO
struct rtinfo {
	struct rtinfo *prev;	/* previous link */
	struct rtinfo *next;	/* forward link */

	u_int32_t ltime;	/* route lifetime */
	u_int rtpref;		/* route preference */
	int prefixlen;
	struct in6_addr prefix;
};
#endif

struct soliciter {
	struct soliciter *next;
	struct sockaddr_in6 addr;
};

struct rdnss {
	struct rdnss *next;	/* forward link */
	struct rdnss *prev;	/* previous link */
	
	struct in6_addr addr;
};

struct dnssl {
	struct dnssl *next;
	struct dnssl *prev;

	char domain[1];
};

struct	rainfo {
	/* pointer for list */
	struct	rainfo *next;

	/* timer related parameters */
	struct rtadvd_timer *timer;
	int initcounter; /* counter for the first few advertisements */
	struct timeval lastsent; /* timestamp when the latest RA was sent */
	int waiting;		/* number of RS waiting for RA */

	/* interface information */
	int	ifindex;
	int	advlinkopt;	/* bool: whether include link-layer addr opt */
	struct sockaddr_dl *sdl;
	char	ifname[16];
	int	phymtu;		/* mtu of the physical interface */

	/* Router configuration variables */
	u_short lifetime;	/* AdvDefaultLifetime */
	u_int	maxinterval;	/* MaxRtrAdvInterval */
	u_int	mininterval;	/* MinRtrAdvInterval */
	int 	managedflg;	/* AdvManagedFlag */
	int	otherflg;	/* AdvOtherConfigFlag */

	int	rtpref;		/* router preference */
	u_int32_t linkmtu;	/* AdvLinkMTU */
	u_int32_t reachabletime; /* AdvReachableTime */
	u_int32_t retranstimer;	/* AdvRetransTimer */
	u_int	hoplimit;	/* AdvCurHopLimit */
	struct prefix prefix;	/* AdvPrefixList(link head) */
	int	pfxs;		/* number of prefixes */
	long	clockskew;	/* used for consisitency check of lifetimes */

#ifdef ROUTEINFO
	struct rtinfo route;	/* route information option (link head) */
	int	routes;		/* number of route information options */
#endif

    	/* Recursive DNS Servers RFC5006 */
	struct rdnss rdnss_list;
	int rdnss_length;
	u_int32_t rdnss_lifetime;

	/* DNS Search List RFC6106 */
	struct dnssl dnssl_list;
	int dnssl_length;
	u_int32_t dnssl_lifetime;
	u_int32_t dnssl_option_length;

	/* Captive Portal RFC 7710 */
	char *	capport;
	u_int32_t capport_length;
	u_int32_t capport_option_length;

	/* PREF64 (NAT64 prefix) RFC 8781 */
	bool pref64_specified;
	pref64_prefix_length_t pref64_prefix_length;
	pref64_scaled_lifetime_plc_t pref64_lifetime_plc;
	struct in6_addr pref64_addr;

	/* Provisioning Domain RFC 8801 */
	u_int8_t pvd_flags_and_delay[2];
	u_int16_t pvd_seqnr;
	char *pvd_id;
	u_int32_t pvd_id_length;
	u_int32_t pvd_option_length;

	/* actual RA packet data and its length */
	size_t ra_datalen;
	u_char *ra_data;

	/* statistics */
	u_quad_t raoutput;	/* number of RAs sent */
	u_quad_t rainput;	/* number of RAs received */
	u_quad_t rainconsistent; /* number of RAs inconsistent with ours */
	u_quad_t rsinput;	/* number of RSs received */

	/* info about soliciter */
	struct soliciter *soliciter;	/* recent solication source */
};

struct rtadvd_timer *ra_timeout(void *);
void ra_timer_update(void *, struct timeval *);

int prefix_match(struct in6_addr *, int, struct in6_addr *, int);
struct rainfo *if_indextorainfo(int);
struct prefix *find_prefix(struct rainfo *, struct in6_addr *, int);

extern struct in6_addr in6a_site_allrouters;
