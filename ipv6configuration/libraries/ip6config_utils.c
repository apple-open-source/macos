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

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <ifaddrs.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFDictionary.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include "ip6config_utils.h"
#include "globals.h"

const struct sockaddr_in6	S_blank_sin6 = { sizeof(S_blank_sin6), AF_INET6 };

#define S6_ADDR_LEN 16

/*
 * Internet Utilities
 * ******************
 */
__private_extern__ int
inet6_dgram_socket()
{
    return (socket(AF_INET6, SOCK_DGRAM, 0));
}

__private_extern__ int
inet6_routing_socket()
{
    return (socket(PF_ROUTE, SOCK_RAW, 0));
}

__private_extern__ int
cfstring_to_numeric(int family, CFStringRef str, void * addr)
{
    char	buf[128];
    CFIndex	l;
    int		n;
    CFRange	range;
    int		err = -1;

    if (str == NULL || addr == NULL)
	return (err);

    range = CFRangeMake(0, CFStringGetLength(str));
    n = CFStringGetBytes(str, range, kCFStringEncodingMacRoman,
			 0, FALSE, (UInt8 *)buf, sizeof(buf), &l);
    buf[l] = '\0';
    err = inet_pton(family, buf, addr);
    switch (err) {
	case 1: {
	    /* success */
	    err = 0;
	    break;
	}
	case -1: {
	    /* system error */
	    err = errno;
	    break;
	}
	case 0: {
	    /* inet_pton error */
	    err = -1;
	    break;
	}
	default: {
	    err = -1;
	    break;
	}
    }

    return (err);
}

/* was in6_len2mask() from netinet6/in6.c */
__private_extern__ void
prefixLen2mask(struct in6_addr * mask, int len)
{
    int i;

    bzero(mask, sizeof(*mask));
    for (i = 0; i < len / 8; i++)
	mask->s6_addr[i] = 0xff;
    if (len % 8)
	mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
}

/* was in6_mask2len(), from netinet6/in6.c */
__private_extern__ int
prefixmask2len(struct in6_addr * mask, u_char * lim0)
{
    int x = 0, y;
    u_char *lim = lim0, *p;

    if (lim0 == NULL ||
	lim0 - (u_char *)mask > sizeof(*mask)) /* ignore the scope_id part */
	lim = (u_char *)mask + sizeof(*mask);
    for (p = (u_char *)mask; p < lim; x++, p++) {
	if (*p != 0xff)
	    break;
    }
    y = 0;
    if (p < lim) {
	for (y = 0; y < 8; y++) {
	    if ((*p & (0x80 >> y)) == 0)
		break;
	}
    }

    /*
     * when the limit pointer is given, do a stricter check on the
     * remaining bits.
     */
    if (p < lim) {
	if (y != 0 && (*p & (0x00ff >> y)) != 0)
	    return(-1);
	for (p = p + 1; p < lim; p++)
	    if (*p != 0)
		return(-1);
    }

    return x * 8 + y;
}

__private_extern__ int
ifflags_set(int s, char * name, short flags)
{
    struct in6_ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret < 0) {
	return (ret);
    }
    ifr.ifr_flags |= flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

static int
get_llocal_addr(const char * name, struct sockaddr_in6 *sin6)
{
    struct ifaddrs	*ifap, *ifa;
    struct sockaddr_in6	*tmp_sin6;

    if (getifaddrs(&ifap) != 0) {
	return(-1);
    }

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	if (strncmp(ifa->ifa_name, name, strlen(name)) != 0)
	    continue;
	if (ifa->ifa_addr->sa_family != AF_INET6)
	    continue;
	tmp_sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
	if (!IN6_IS_ADDR_LINKLOCAL(&tmp_sin6->sin6_addr))
	    continue;
	*sin6 = *tmp_sin6;
	freeifaddrs(ifap);
	return (0);
    }

    freeifaddrs(ifap);
    return (-1);
}

/* get_llocal_if_addr_flags(): gets the flags of the linklocal address
 * for the given interface.
 */
__private_extern__ int
get_llocal_if_addr_flags(const char * name, short * flags)
{
    struct in6_ifreq 	ifr6;
    struct sockaddr_in6	sin6;
    int s;

    if ((s = inet6_dgram_socket()) < 0) {
	return(-1);
    }

    if(get_llocal_addr(name, &sin6) != 0) {
	my_log(LOG_DEBUG, "get_llocal_if_addr_flags %s: error getting linklocal address",
	       name);
	close(s);
	return (-1);
    }

    memset(&ifr6, 0, sizeof(ifr6));
    strcpy(ifr6.ifr_name, name);
    memcpy(&ifr6.ifr_ifru.ifru_addr, &sin6, sin6.sin6_len);
    if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
	my_log(LOG_DEBUG,
	       "get_llocal_if_addr_flags: ioctl(SIOCGIFAFLAG_IN6): %s",
	       strerror(errno));
	close(s);
	return(-1);
    }

    *flags = ifr6.ifr_ifru.ifru_flags6;
    close(s);
    return 0;
}

__private_extern__ int
inet6_difaddr(int s, char * name, const struct in6_addr * addr)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    if (addr) {
	ifr.ifr_ifru.ifru_addr.sin6_family = AF_INET6;
	ifr.ifr_ifru.ifru_addr.sin6_len = sizeof(struct sockaddr_in6);
	memcpy(&(ifr.ifr_ifru.ifru_addr.sin6_addr), addr, sizeof(struct in6_addr));
    }
    return (ioctl(s, SIOCDIFADDR_IN6, &ifr));
}

__private_extern__ int
inet6_aifaddr(int s, char * name, const struct in6_addr * addr,
	     const struct in6_addr * dstaddr,
	     const struct in6_addr * prefixmask)
{
    struct in6_aliasreq	ifra_in6;

    bzero(&ifra_in6, sizeof(ifra_in6));
    strncpy(ifra_in6.ifra_name, name, sizeof(ifra_in6.ifra_name));
    ifra_in6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
    ifra_in6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
    if (addr) {
	ifra_in6.ifra_addr.sin6_family = AF_INET6;
	ifra_in6.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	memcpy(&(ifra_in6.ifra_addr.sin6_addr), addr, sizeof(struct in6_addr));
    }
    if (dstaddr) {
	ifra_in6.ifra_dstaddr.sin6_family = AF_INET6;
	ifra_in6.ifra_dstaddr.sin6_len = sizeof(struct sockaddr_in6);
	memcpy(&(ifra_in6.ifra_dstaddr.sin6_addr), dstaddr, sizeof(struct in6_addr));
    }
    if (prefixmask) {
	ifra_in6.ifra_prefixmask.sin6_family = AF_INET6;
	ifra_in6.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	memcpy(&(ifra_in6.ifra_prefixmask.sin6_addr), prefixmask, sizeof(struct in6_addr));
    }

    return (ioctl(s, SIOCAIFADDR_IN6, &ifra_in6));
}

__private_extern__ int
getinet6sysctl(int code)
{
    int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
    int value;
    size_t size;

    mib[3] = code;
    size = sizeof(value);
    if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0) < 0)
	return -1;
    else
	return value;
}

__private_extern__ int
setinet6sysctl(int code, int value)
{
    int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
    int setval = value;
    size_t size;

    mib[3] = code;
    size = sizeof(setval);
    if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), NULL, 0, &setval, size) < 0)
	return -1;
    else
	return 0;
}

#define ROUNDUP8(a) (1 + (((a) - 1) | 7))

__private_extern__ int
lladdropt_length(link_addr_t * link)
{
    switch(link->type) {
	case IFT_ETHER:
#ifdef IFT_IEEE80211
	case IFT_IEEE80211:
#endif
	    return(ROUNDUP8(ETHER_ADDR_LEN + 2));
	 default:
	    return(0);
    }
}

__private_extern__ void
lladdropt_fill(link_addr_t * link, struct nd_opt_hdr *ndopt)
{
    char *addr;

    ndopt->nd_opt_type = ND_OPT_SOURCE_LINKADDR; /* fixed */

    switch(link->type) {
	case IFT_ETHER:
#ifdef IFT_IEEE80211
	case IFT_IEEE80211:
#endif
	    ndopt->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
	    addr = (char *)(ndopt + 1);
	    memcpy(addr, link->addr, link->alen);
	    break;
	 default:
	    my_log(LOG_ERR,
	       "lladdropt_fill: unsupported link type(%d)", link->type);
	    break;
    }

    return;
}


/*
 * CF Utilities
 * ************
 */

__private_extern__ void
my_CFRelease(void * t)
{
    void * * obj = (void * *)t;
    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

__private_extern__ void
my_CFArrayAppendUniqueValue(CFMutableArrayRef arr, CFTypeRef new)
{
    int count;
    int i;

    count = CFArrayGetCount(arr);
    for (i = 0; i < count; i++) {
	CFStringRef element = CFArrayGetValueAtIndex(arr, i);
	if (CFEqual(element, new)) {
	    return;
	}
    }
    CFArrayAppendValue(arr, new);
    return;
}


/*
 * Miscellaneous Utilities
 * ***********************
 */

/* These are common functions shared by the various files in this project */
__private_extern__ void
my_log(int priority, const char *message, ...)
{
    va_list 	ap;

    if (priority == LOG_DEBUG) {
	if (G_verbose == FALSE)
	    return;
	priority = LOG_NOTICE;
    }
    else if (priority == LOG_INFO) {
        priority = LOG_NOTICE;
    }
    va_start(ap, message);
    if (G_scd_session == NULL) {
	vsyslog(priority, message, ap);
    }
    else {
	char	buffer[256];

	vsnprintf(buffer, sizeof(buffer), message, ap);
	SCLog(TRUE, priority, CFSTR("%s"), buffer);
    }
    return;
}

__private_extern__ int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex	l;
    CFIndex	n;
    CFRange	range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    n = CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			 0, FALSE, (UInt8 *)str, len, &l);
    str[l] = '\0';
    return (l);
}

/*
 * Function: random_range
 * Purpose:
 *   Return a random number in the given range.
 */
__private_extern__ long
random_range(long bottom, long top)
{
    long ret;
    long number = top - bottom + 1;
    long range_size = LONG_MAX / number;
    if (range_size == 0)
	return (bottom);
    ret = (random() / range_size) + bottom;
    return (ret);
}

