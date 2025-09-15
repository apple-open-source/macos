/*
 * Copyright (c) 2018, 2025 Apple Inc. All rights reserved.
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
 * IPConfigurationUtil.h
 * - API to communicate with IPConfiguration agent to perform various tasks
 */

/* 
 * Modification History
 *
 * March 29, 2018 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#ifndef _IPCONFIGURATIONUTIL_H
#define _IPCONFIGURATIONUTIL_H

#include <CoreFoundation/CFString.h>

Boolean
IPConfigurationForgetNetwork(CFStringRef ifname, CFStringRef ssid);

/*
 * Function: IPConfigurationCopyIPv4RouterInformation
 * Purpose:
 *   Return the MAC address and IPv4 address of the router for the
 *   specified interface `ifname`.
 * Returns:
 *   If successful, returns a non-NULL string representation of the
 *   router's MAC address, sets `*ret_ip` to a string representation of
 *   the router's IPv4 address. Use `CFRelease()` to release the strings.
 *
 *   If unsuccessful, returns NULL and sets *ret_ip to NULL.
 * Note:
 *   Caller must have the "com.apple.IPConfiguration.get-information"
 *   boolean entitlement.
 */
CFStringRef
IPConfigurationCopyIPv4RouterInformation(CFStringRef ifname,
					 CFStringRef * ret_ip);
#endif /* _IPCONFIGURATIONUTIL_H */
