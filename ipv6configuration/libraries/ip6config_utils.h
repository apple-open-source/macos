#ifndef _INET6_UTILS_H_
#define _INET6_UTILS_H_
/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* ip6config_utils.h:
 *		utility functions
 */

#include <mach/boolean.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <net/if_dl.h>
#include <arpa/inet.h>

#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>

#include "configthreads_common.h"

/*
 * Miscellaneous Utilities
 *
 */

#define USECS_PER_SEC	1000000

void 				my_log(int priority, const char *message, ...);
int				cfstring_to_cstring(CFStringRef cfstr, char * str, int len);
long				random_range(long bottom, long top);

/*
 * Internet utilities
 *
 */

#define IP6_FORMAT	"%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define IP6_CH(ip6, i)	(((u_char *)(ip6))[i])
#define IP6_LIST(ip6)	IP6_CH((ip6)->s6_addr, 0),IP6_CH((ip6)->s6_addr, 1), \
			IP6_CH((ip6)->s6_addr, 2),IP6_CH((ip6)->s6_addr, 3), \
			IP6_CH((ip6)->s6_addr, 4),IP6_CH((ip6)->s6_addr, 5), \
			IP6_CH((ip6)->s6_addr, 6),IP6_CH((ip6)->s6_addr, 7), \
			IP6_CH((ip6)->s6_addr, 8),IP6_CH((ip6)->s6_addr, 9), \
			IP6_CH((ip6)->s6_addr, 10),IP6_CH((ip6)->s6_addr, 11), \
			IP6_CH((ip6)->s6_addr, 12),IP6_CH((ip6)->s6_addr, 13), \
			IP6_CH((ip6)->s6_addr, 14),IP6_CH((ip6)->s6_addr, 15)

#define USE_NEW_API	0
#define IN6_INFINITE_LIFETIME		0xffffffff

/*
 * Function: ip_valid
 * Purpose:
 *   Perform some cursory checks on the IP address
 */
static __inline__ boolean_t
ip_valid(struct in6_addr * ip)
{
    /* can't be multicast or unspec */
    if (IN6_IS_ADDR_UNSPECIFIED(ip) || IN6_IS_ADDR_MULTICAST(ip))
	return (FALSE);
    return (TRUE);
}

int		inet6_dgram_socket();
int		inet6_routing_socket();
int		cfstring_to_numeric(int family, CFStringRef str, void * addr);
void		prefixLen2mask(struct in6_addr * mask, int len);
int		prefixmask2len(struct in6_addr * mask, u_char * lim0);
int		ifflags_set(int s, char * name, short flags);
int		get_llocal_if_addr_flags(const char * name, short * flags);
int		inet6_difaddr(int s, char * name, const struct in6_addr * addr);
int		inet6_aifaddr(int s, char * name, const struct in6_addr * addr,
			      const struct in6_addr * dstaddr, const struct in6_addr * prefixmask);
int		getinet6sysctl(int code);
int		setinet6sysctl(int code, int value);
int		lladdropt_length(link_addr_t * link);
void		lladdropt_fill(link_addr_t * link, struct nd_opt_hdr *ndopt);

/*
 * CF Utilities
 *
 */

void		my_CFRelease(void * t);
void		my_CFArrayAppendUniqueValue(CFMutableArrayRef arr, CFTypeRef new);


#endif /* _INET6_UTILS_H_ */
