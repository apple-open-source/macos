/*
 * Copyright (c) 2006-2007,2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 * iCloudKeychainTrace.h - log statistics for iCloud Keychain usage
 */

#include <CoreFoundation/CoreFoundation.h>

const CFStringRef kCloudKeychainNumberOfSyncingConflicts
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
const CFStringRef kCloudKeychainNumberOfTimesSyncFailed
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
const CFStringRef kCloudKeychainNumberOfConflictsResolved
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
const CFStringRef kCloudKeychainNumberOfTimesSyncedWithPeers
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

bool SetCloudKeychainTraceValueForKey(CFStringRef key, int64_t value)	
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
	
void* BeginCloudKeychainLoggingTransaction(void)
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
	
bool AddKeyValuePairToKeychainLoggingTransaction(void* token, CFStringRef key, int64_t value)
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
		
void CloseCloudKeychainLoggingTransaction(void* token)
	__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

	



