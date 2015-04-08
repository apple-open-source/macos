/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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

#include <SecureObjectSync/SOSMessage.h>
#include <utilities/SecDb.h>
#include <CoreFoundation/CFRuntime.h>

typedef struct __OpaqueSOSTestDevice *SOSTestDeviceRef;

struct __OpaqueSOSTestDevice {
    CFRuntimeBase _base;
    SecDbRef db;
    SOSDataSourceFactoryRef dsf;
    SOSDataSourceRef ds;
    CFMutableArrayRef peers;
    bool mute;
};

CFStringRef SOSMessageCopyDigestHex(SOSMessageRef message);

CFStringRef SOSTestDeviceGetID(SOSTestDeviceRef td);
void SOSTestDeviceForEachPeerID(SOSTestDeviceRef td, void(^peerBlock)(CFStringRef peerID, bool *stop));
SOSTestDeviceRef SOSTestDeviceCreateWithDb(CFAllocatorRef allocator, CFStringRef engineID, SecDbRef db);
SOSTestDeviceRef SOSTestDeviceCreateWithDbNamed(CFAllocatorRef allocator, CFStringRef engineID, CFStringRef dbName);
SOSTestDeviceRef SOSTestDeviceCreateWithTestDataSource(CFAllocatorRef allocator, CFStringRef engineID);
SOSTestDeviceRef SOSTestDeviceSetPeerIDs(SOSTestDeviceRef td, CFArrayRef peerIDs, CFIndex version);

SOSTestDeviceRef SOSTestDeviceSetMute(SOSTestDeviceRef td, bool mute);
bool SOSTestDeviceIsMute(SOSTestDeviceRef td);

CFDataRef SOSTestDeviceCreateMessage(SOSTestDeviceRef td, CFStringRef peerID);

bool SOSTestDeviceHandleMessage(SOSTestDeviceRef td, CFStringRef peerID, CFDataRef msgData);

void SOSTestDeviceAddGenericItem(SOSTestDeviceRef td, CFStringRef account, CFStringRef server);
void SOSTestDeviceAddRemoteGenericItem(SOSTestDeviceRef td, CFStringRef account, CFStringRef server);
bool SOSTestDeviceAddGenericItems(SOSTestDeviceRef td, CFIndex count, CFStringRef account, CFStringRef server);

CFMutableDictionaryRef SOSTestDeviceListCreate(bool realDb, CFIndex version, CFArrayRef deviceIDs);

void SOSTestDeviceListSync(const char *name, const char *test_directive, const char *test_reason, CFMutableDictionaryRef testDevices, bool(^pre)(SOSTestDeviceRef source, SOSTestDeviceRef dest), bool(^post)(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message));

bool SOSTestDeviceListInSync(const char *name, const char *test_directive, const char *test_reason, CFMutableDictionaryRef testDevices);

void SOSTestDeviceListTestSync(const char *name, const char *test_directive, const char *test_reason, CFIndex version, bool use_db,
                               bool(^pre)(SOSTestDeviceRef source, SOSTestDeviceRef dest),
                               bool(^post)(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message), ...);
