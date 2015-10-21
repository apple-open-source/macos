/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#ifndef sec_iCloudTrace_h
#define sec_iCloudTrace_h
#include <CoreFoundation/CoreFoundation.h>

struct _SecServerKeyStats;

extern const CFStringRef kNumberOfiCloudKeychainPeers
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern const CFStringRef kNumberOfiCloudKeychainItemsBeingSynced
	__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern const CFStringRef kNumbrerOfiCloudKeychainSyncingConflicts
	__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern const CFStringRef kNumberOfiCloudKeychainTimesSyncFailed
	__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern const CFStringRef kNumberOfiCloudKeychainConflictsResolved
	__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern const CFStringRef kNumberOfiCloudKeychainTimesSyncedWithPeers
	__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);

void CloudKeychainTrace(CFIndex num_peers, size_t num_items,
                        const struct _SecServerKeyStats *genpStats,
                        const struct _SecServerKeyStats *inetStats,
                        const struct _SecServerKeyStats *keysStats);

#endif
