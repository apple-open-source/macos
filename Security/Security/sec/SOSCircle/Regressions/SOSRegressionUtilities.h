/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

//
//  SOSRegressionUtilities.h
//

#ifndef sec_SOSRegressionUtilities_h
#define sec_SOSRegressionUtilities_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFError.h>
#include <Security/SecKey.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>

__BEGIN_DECLS

CFStringRef myMacAddress(void);
const char *cfabsoluteTimeToString(CFAbsoluteTime abstime);
const char *cfabsoluteTimeToStringLocal(CFAbsoluteTime abstime);
bool XPCServiceInstalled(void);

void registerForKVSNotifications(const void *observer, CFStringRef name, CFNotificationCallback callBack);
void unregisterFromKVSNotifications(const void *observer);

bool testPutObjectInCloudAndSync(CFStringRef key, CFTypeRef object, CFErrorRef *error, dispatch_group_t dgroup, dispatch_queue_t processQueue);
bool testPutObjectInCloud(CFStringRef key, CFTypeRef object, CFErrorRef *error, dispatch_group_t dgroup, dispatch_queue_t processQueue);

CFTypeRef testGetObjectFromCloud(CFStringRef key, dispatch_queue_t processQueue, dispatch_group_t dgroup);
CFTypeRef testGetObjectsFromCloud(CFArrayRef keys, dispatch_queue_t processQueue, dispatch_group_t dgroup);

bool testSynchronize(dispatch_queue_t processQueue, dispatch_group_t dgroup);
bool testClearAll(dispatch_queue_t processQueue, dispatch_group_t dgroup);

//
// MARK: Peer Info helpers
//   These generate keys for your and create info objects with that name.
//

CFDictionaryRef SOSCreatePeerGestaltFromName(CFStringRef name);

SOSPeerInfoRef SOSCreatePeerInfoFromName(CFStringRef name, SecKeyRef* outSigningKey, CFErrorRef *error);
SOSFullPeerInfoRef SOSCreateFullPeerInfoFromName(CFStringRef name, SecKeyRef* outSigningKey, CFErrorRef *error);

__END_DECLS

#endif /* sec_SOSRegressionUtilities_h */

