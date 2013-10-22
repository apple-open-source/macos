/*
 * Copyright (c) 2012, 2013 Apple Inc.  All Rights Reserved.
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

#include <SystemConfiguration/SCPrivate.h>

#define kIsNULL				CFSTR("IsNULL")	/* CFBoolean */

#if	((__MAC_OS_X_VERSION_MIN_REQUIRED >= 1080) || (__IPHONE_OS_VERSION_MIN_REQUIRED >= 60000))

#define my_log(__level, fmt, ...)	SCLoggerLog(my_log_get_logger(), __level, CFSTR(fmt), ## __VA_ARGS__)
SCLoggerRef my_log_get_logger();

/*
 * IPv4 Service Dict keys: IPv4DictRoutes, IPv4DictService
 *
 * The IPv4 service dictionary contains two sub-dictionaries:
 *   	Routes		IPv4RouteList
 *      Service		dictionary containing kSCEntNetIPv4 keys
 */
#define kIPv4DictRoutes 		CFSTR("Routes")
#define	kIPv4DictService		CFSTR("Service")

boolean_t
cfstring_to_ip(CFStringRef str, struct in_addr * ip_p);

boolean_t
cfstring_to_ip6(CFStringRef str, struct in6_addr * ip6_p);

#else	// ((__MAC_OS_X_VERSION_MIN_REQUIRED >= 1080) || (__IPHONE_OS_VERSION_MIN_REQUIRED >= 60000))

#define my_log(__level, fmt, ...)	SCLog(TRUE, __level, CFSTR(fmt), ## __VA_ARGS__)

#endif	// ((__MAC_OS_X_VERSION_MIN_REQUIRED >= 1080) || (__IPHONE_OS_VERSION_MIN_REQUIRED >= 60000))

#endif	// _IP_PLUGIN_H
