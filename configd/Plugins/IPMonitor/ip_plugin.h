/*
 * Copyright (c) 2012-2023 Apple Inc.  All Rights Reserved.
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

/*
 * ip_plugin.h
 * - header for IPMonitor.bundle
 */

#ifndef	_IP_PLUGIN_H
#define	_IP_PLUGIN_H

#include <net/if.h>
#include <netinet/in.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPrivate.h>

#define kLoopbackInterface	"lo0"

typedef unsigned int	IFIndex;	/* interface index */

#ifdef TEST_IPV4_ROUTELIST
#define TEST_ROUTELIST
#endif

#ifdef TEST_IPV6_ROUTELIST
#define TEST_ROUTELIST
#endif

#ifndef countof
#define countof(array)	(sizeof(array) / sizeof((array)[0]))
#endif

#define kIsNULL				CFSTR("IsNULL")	/* CFBoolean */

#ifndef	my_log
  #ifdef TEST_ROUTELIST
    #define	my_log(__level, __format, ...)	SCPrint(TRUE, stdout, CFSTR(__format "\n"), ## __VA_ARGS__)
  #else /* TEST_ROUTELIST */
    #define	my_log(__level, __format, ...)	SC_log(__level, __format, ## __VA_ARGS__)
  #endif /* TEST_ROUTELIST */
#endif	// !my_log

os_log_t
__log_IPMonitor(void);

boolean_t
cfstring_to_ip(CFStringRef str, struct in_addr * ip_p);

boolean_t
cfstring_to_ip6(CFStringRef str, struct in6_addr * ip6_p);

unsigned int
my_if_nametoindex(const char * ifname);

const char *
my_if_indextoname(unsigned int idx, char if_name[IFNAMSIZ]);

void
my_if_freenameindex(void);

void
my_if_nameindex(void);

IFIndex
effective_ifindex_get(const char * ifname, IFIndex ifindex);

void
effective_ifindex_free(void);

IFIndex
lo0_ifindex(void);

const char *
my_if_indextoname2(IFIndex ifindex, char ifname[IFNAMSIZ]);

int
open_inet_dgram_socket(void);

void
close_inet_dgram_socket(void);

int
open_inet6_dgram_socket(void);

void
close_inet6_dgram_socket(void);

int
siocdradd_in6(int s, int if_index, const struct in6_addr * addr, u_char flags);

int
siocdrdel_in6(int s, int if_index, const struct in6_addr * addr);

int
siocsifnetsignature(int s, const char * ifname, int af,
		    const uint8_t * signature, size_t signature_length);

boolean_t
set_ipv6_default_interface(IFIndex ifindex);

boolean_t
service_is_routable(CFDictionaryRef service_dict, int af);

boolean_t
service_is_scoped_only(CFDictionaryRef service_dict);

boolean_t
check_if_service_expensive(CFStringRef serviceID);

CFDictionaryRef
ipv4_dict_create(CFDictionaryRef state_dict);

CFDictionaryRef
ipv6_dict_create(CFDictionaryRef state_dict);

#endif	// _IP_PLUGIN_H
