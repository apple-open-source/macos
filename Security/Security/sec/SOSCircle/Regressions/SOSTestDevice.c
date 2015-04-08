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


// Test syncing between SecItemDataSource and SOSTestDataSource

#include "SOSTestDevice.h"
#include "SOSTestDataSource.h"
#include <test/testmore.h>

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
#include <Security/SecBase64.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <corecrypto/ccsha2.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecItemDataSource.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecIOFormat.h>

#include <stdint.h>
#include <AssertMacros.h>

CFStringRef SOSMessageCopyDigestHex(SOSMessageRef message) {
    uint8_t digest[CCSHA1_OUTPUT_SIZE];
    // TODO: Pass in real sequenceNumber.
    CFDataRef msgData = SOSMessageCreateData(message, 0, NULL);
    if (!msgData) return NULL;
    ccdigest(ccsha1_di(), CFDataGetLength(msgData), CFDataGetBytePtr(msgData), digest);
    CFMutableStringRef hex = CFStringCreateMutable(0, 2 * sizeof(digest));
    for (unsigned int ix = 0; ix < sizeof(digest); ++ix) {
        CFStringAppendFormat(hex, 0, CFSTR("%02X"), digest[ix]);
    }
    CFReleaseSafe(msgData);
    return hex;
}

static void SOSTestDeviceDestroy(CFTypeRef cf) {
    SOSTestDeviceRef td = (SOSTestDeviceRef)cf;
    CFReleaseSafe(td->peers);
    if (td->ds)
        SOSDataSourceRelease(td->ds, NULL);
    if (td->dsf)
        td->dsf->release(td->dsf);
    CFReleaseSafe(td->db);
}

CFStringRef SOSTestDeviceGetID(SOSTestDeviceRef td) {
    CFStringRef engineID = NULL;
    SOSEngineRef engine = SOSDataSourceGetSharedEngine(td->ds, NULL);
    if (engine)
        engineID = SOSEngineGetMyID(engine);
    return engineID;
}

void SOSTestDeviceForEachPeerID(SOSTestDeviceRef td, void(^peerBlock)(CFStringRef peerID, bool *stop)) {
    SOSPeerRef peer;
    bool stop = false;
    CFArrayForEachC(td->peers, peer) {
        peerBlock(SOSPeerGetID(peer), &stop);
        if (stop)
            break;
    }
}

static CFStringRef SOSTestDeviceCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSTestDeviceRef td = (SOSTestDeviceRef)cf;
    CFMutableStringRef result = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<SOSTestDevice %@"), td->ds->engine);
    SOSTestDeviceForEachPeerID(td, ^(CFStringRef peerID, bool *stop) {
        SOSPeerRef peer = SOSPeerCreateWithEngine(td->ds->engine, peerID);
        CFStringAppendFormat(result, NULL, CFSTR("\n%@"), peer);
        CFReleaseSafe(peer);
    });
    CFStringAppendFormat(result, NULL, CFSTR(">"));
    return result;
}

CFGiblisFor(SOSTestDevice)

static SOSTestDeviceRef SOSTestDeviceCreateInternal(CFAllocatorRef allocator, CFStringRef engineID) {
    SOSTestDeviceRef td = CFTypeAllocate(SOSTestDevice, struct __OpaqueSOSTestDevice, allocator);
    td->peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    td->mute = false;
    return td;
}

SOSTestDeviceRef SOSTestDeviceCreateWithDb(CFAllocatorRef allocator, CFStringRef engineID, SecDbRef db) {
    setup("create device");
    SOSTestDeviceRef td = SOSTestDeviceCreateInternal(allocator, engineID);
    CFRetainAssign(td->db, db);
    td->dsf = SecItemDataSourceFactoryGetShared(td->db);
    CFArrayRef ds_names = td->dsf->copy_names(td->dsf);
    CFErrorRef error = NULL;
    if (ds_names && CFArrayGetCount(ds_names) > 0) {
        CFStringRef sname = CFArrayGetValueAtIndex(ds_names, 0);
        ok (td->ds = td->dsf->create_datasource(td->dsf, sname, &error), "%@ create datasource \"%@\" [error: %@]", engineID, sname, error);
        CFReleaseNull(error);
    }
    CFReleaseNull(ds_names);
    assert(td->ds); // Shut up static analyzer and test generally run in debug mode anyway
    if (td->ds)
        SOSEngineCircleChanged(SOSDataSourceGetSharedEngine(td->ds, NULL), engineID, NULL, NULL);
    return td;
}

SOSTestDeviceRef SOSTestDeviceCreateWithDbNamed(CFAllocatorRef allocator, CFStringRef engineID, CFStringRef dbName) {
    CFURLRef url = SecCopyURLForFileInKeychainDirectory(dbName);
    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    SecDbRef db = SecKeychainDbCreate(path);
    SOSTestDeviceRef td = SOSTestDeviceCreateWithDb(allocator, engineID, db);
    CFReleaseSafe(db);
    CFReleaseSafe(path);
    CFReleaseSafe(url);
    return td;
}

SOSTestDeviceRef SOSTestDeviceCreateWithTestDataSource(CFAllocatorRef allocator, CFStringRef engineID) {
    setup("create device");
    SOSTestDeviceRef td = SOSTestDeviceCreateInternal(allocator, engineID);

    td->ds = SOSTestDataSourceCreate();
    CFErrorRef error = NULL;
    ok(td->ds->engine = SOSEngineCreate(td->ds, &error), "create engine: %@", error);
    SOSEngineCircleChanged(td->ds->engine, engineID, NULL, NULL);
    CFReleaseNull(error);
    return td;
}

SOSTestDeviceRef SOSTestDeviceSetPeerIDs(SOSTestDeviceRef td, CFArrayRef peerIDs, CFIndex version) {
    setup("create device");
    CFStringRef engineID = SOSTestDeviceGetID(td);
    CFStringRef peerID;
    CFMutableArrayRef trustedPeersIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFErrorRef error = NULL;
    CFArrayForEachC(peerIDs, peerID) {
        if (isString(peerID) && !CFEqualSafe(peerID, engineID)) {
            SOSPeerRef peer;
            ok(peer = SOSPeerCreateSimple(td->ds->engine, peerID, version, &error), "create peer: %@", error);
            CFReleaseNull(error);
            CFArrayAppendValue(td->peers, peer);
            CFArrayAppendValue(trustedPeersIDs, peerID);
        }
    }

    SOSEngineCircleChanged(td->ds->engine, engineID, trustedPeersIDs, NULL);
    CFArrayForEachC(trustedPeersIDs, peerID) {
        ok(SOSEnginePeerDidConnect(td->ds->engine, peerID, &error), "tell %@ %@ connected: %@", engineID, peerID, error);
        CFReleaseNull(error);
    }
    CFReleaseSafe(trustedPeersIDs);
    return td;
}

SOSTestDeviceRef SOSTestDeviceSetMute(SOSTestDeviceRef td, bool mute) {
    td->mute = mute;
    return td;
}

bool SOSTestDeviceIsMute(SOSTestDeviceRef td) {
    return td->mute;
}

CFDataRef SOSTestDeviceCreateMessage(SOSTestDeviceRef td, CFStringRef peerID) {
    setup("create message");
    CFErrorRef error = NULL;
    SOSEnginePeerMessageSentBlock sent = NULL;
    CFDataRef msgData;
    ok(msgData = SOSEngineCreateMessageToSyncToPeer(td->ds->engine, peerID, &sent, &error),
       "create message to %@: %@", peerID, error);
    if (sent)
        sent(true);

    return msgData;
}

#if 0
CFDictionaryRef SOSTestDeviceCreateMessages(SOSTestDeviceRef td) {
    CFTypeRef peer = NULL;
    CFMutableDictionaryRef messages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayForEachC(td->peers, peer) {
        CFStringRef peerID = SOSPeerGetID((SOSPeerRef)peer);
        CFDataRef msg = SOSTestDeviceCreateMessage(td, peerID);
        if (msg) {
            CFDictionaryAddValue(messages, peerID, msg);
            CFRelease(msg);
        }
    }
    return messages;
}
#endif

bool SOSTestDeviceHandleMessage(SOSTestDeviceRef td, CFStringRef peerID, CFDataRef msgData) {
    setup("handle message");
    if (!msgData) return false;
    CFErrorRef error = NULL;
    bool handled;
    SOSMessageRef message;

    ok(message = SOSMessageCreateWithData(kCFAllocatorDefault, msgData, &error), "decode message %@: %@", msgData, error);
    CFReleaseNull(error);
    pass("handeling %@->%@ %@", peerID, SOSEngineGetMyID(SOSDataSourceGetSharedEngine(td->ds, &error)), message);
    ok(handled = SOSEngineHandleMessage(SOSDataSourceGetSharedEngine(td->ds, &error), peerID, msgData, &error),
       "handled from %@ %@: %@", peerID, message, error);
    CFReleaseNull(error);

    CFReleaseNull(message);
    return handled;
}

void SOSTestDeviceAddGenericItem(SOSTestDeviceRef td, CFStringRef account, CFStringRef server) {
    __block CFErrorRef error = NULL;
    if (!SOSDataSourceWithAPI(td->ds, true, &error, ^(SOSTransactionRef txn, bool *commit) {
        SOSObjectRef object = SOSDataSourceCreateGenericItem(td->ds, account, server);
        ok(SOSDataSourceMergeObject(td->ds, txn, object, NULL, &error), "%@ added API object %@", SOSTestDeviceGetID(td), error ? (CFTypeRef)error : (CFTypeRef)CFSTR("ok"));
        CFReleaseSafe(object);
        CFReleaseNull(error);
    }))
        fail("ds transaction %@", error);
    CFReleaseNull(error);
}

void SOSTestDeviceAddRemoteGenericItem(SOSTestDeviceRef td, CFStringRef account, CFStringRef server) {
    __block CFErrorRef error = NULL;
    if (!SOSDataSourceWithAPI(td->ds, false, &error, ^(SOSTransactionRef txn, bool *commit) {
        SOSObjectRef object = SOSDataSourceCreateGenericItem(td->ds, account, server);
        ok(SOSDataSourceMergeObject(td->ds, txn, object, NULL, &error), "%@ added remote object %@", SOSTestDeviceGetID(td), error ? (CFTypeRef)error : (CFTypeRef)CFSTR("ok"));
        CFReleaseSafe(object);
        CFReleaseNull(error);
    }))
        fail("ds transaction %@", error);
    CFReleaseNull(error);
}

bool SOSTestDeviceAddGenericItems(SOSTestDeviceRef td, CFIndex count, CFStringRef account, CFStringRef server) {
    __block bool didAdd = false;
    __block CFErrorRef error = NULL;
    if (!SOSDataSourceWithAPI(td->ds, true, &error, ^(SOSTransactionRef txn, bool *commit) {
        bool success = true;
        CFIndex ix = 0;
        for (; success && ix < count; ++ix) {
            CFStringRef accountStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@%" PRIdCFIndex), account, ix);
            SOSObjectRef object = SOSDataSourceCreateGenericItem(td->ds, accountStr, server);
            success = SOSDataSourceMergeObject(td->ds, txn, object, NULL, &error);
            CFReleaseSafe(object);
        }
        ok(success, "%@ added %" PRIdCFIndex " API objects %@", SOSTestDeviceGetID(td), ix, error ? (CFTypeRef)error : (CFTypeRef)CFSTR("ok"));
        didAdd = success && ix == count;
        CFReleaseNull(error);
    }))
        fail("ds transaction %@", error);
    CFReleaseNull(error);
    return didAdd;
}

CFMutableDictionaryRef SOSTestDeviceListCreate(bool realDb, CFIndex version, CFArrayRef deviceIDs) {
    CFMutableDictionaryRef testDevices = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFStringRef deviceID;
    CFArrayForEachC(deviceIDs, deviceID) {
        SOSTestDeviceRef device;
        if (!realDb)
            device = SOSTestDeviceCreateWithTestDataSource(kCFAllocatorDefault, deviceID);
        else
            device = SOSTestDeviceCreateWithDbNamed(kCFAllocatorDefault, deviceID, deviceID);
        SOSTestDeviceSetPeerIDs(device, deviceIDs, version);
        CFDictionarySetValue(testDevices, deviceID, device);
        CFReleaseSafe(device);
    }
    CFDictionarySetValue(testDevices, CFSTR("@devicesIDs"), deviceIDs);
    return testDevices;
}

void SOSTestDeviceListSync(const char *name, const char *test_directive, const char *test_reason, CFMutableDictionaryRef testDevices, bool(^pre)(SOSTestDeviceRef source, SOSTestDeviceRef dest), bool(^post)(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message)) {
    CFArrayRef deviceIDs = (CFArrayRef)CFDictionaryGetValue(testDevices, CFSTR("@devicesIDs"));
    const CFIndex edgeCount = CFArrayGetCount(deviceIDs) * (CFArrayGetCount(deviceIDs) - 1);
    CFIndex deviceIX = 0;
    __block CFIndex noMsgSentCount = 0;
    __block CFIndex msgSentSinceLastChangeCount = 0;
    __block bool done = false;
    do {
        CFStringRef sourceID = (CFStringRef)CFArrayGetValueAtIndex(deviceIDs, deviceIX++);
        if (deviceIX >= CFArrayGetCount(deviceIDs))
            deviceIX = 0;

        SOSTestDeviceRef source = (SOSTestDeviceRef)CFDictionaryGetValue(testDevices, sourceID);
        SOSTestDeviceForEachPeerID(source, ^(CFStringRef destID, bool *stop) {
            SOSTestDeviceRef dest = (SOSTestDeviceRef)CFDictionaryGetValue(testDevices, destID);
            if (dest) {
                if (pre) {
                    if (pre(source, dest))
                        msgSentSinceLastChangeCount = 0;
                }
                CFDataRef msg = SOSTestDeviceCreateMessage(source, destID);
                SOSMessageRef message = NULL;
                bool handled = false;
                msgSentSinceLastChangeCount++;
                if (msg && CFDataGetLength(msg) > 0) {
                    if (!source->mute) {
                        handled = SOSTestDeviceHandleMessage(dest, sourceID, msg);
                        if (handled) {
                            noMsgSentCount = 0;
                        }

                        CFErrorRef error = NULL;
                        message = SOSMessageCreateWithData(kCFAllocatorDefault, msg, &error);
                        ok(handled, "%s %@->%@ %@", name, sourceID, destID, message);
                        CFReleaseNull(error);
                    }
                } else {
                    SOSManifestRef sourceManifest = SOSEngineCopyManifest(SOSDataSourceGetSharedEngine(source->ds, NULL), NULL);
                    pass("%s %@->%@ done L:%@", name, sourceID, destID, sourceManifest);
                    CFReleaseSafe(sourceManifest);
                    noMsgSentCount++;
                    //msgSentSinceLastChangeCount = 0;
                }
                CFReleaseSafe(msg);
                if (post) {
                    if (post(source, dest, message))
                        msgSentSinceLastChangeCount = 0;
                }
                CFReleaseNull(message);
            }
            if (noMsgSentCount >= edgeCount) {
                *stop = done = true;
            } else if (msgSentSinceLastChangeCount >= 9 * edgeCount + 1) {
                fail("%s %" PRIdCFIndex" peers never stopped syncing %" PRIdCFIndex" messages since last change", name, CFArrayGetCount(deviceIDs), msgSentSinceLastChangeCount);
                *stop = done = true;
            }
        });
    } while (!done);
}

bool SOSTestDeviceListInSync(const char *name, const char *test_directive, const char *test_reason, CFMutableDictionaryRef testDevices) {
    bool inSync = true;
    CFArrayRef deviceIDs = (CFArrayRef)CFDictionaryGetValue(testDevices, CFSTR("@devicesIDs"));
    CFStringRef sourceID = NULL;
    SOSManifestRef sourceManifest = NULL;
    CFStringRef currentID;
    CFArrayForEachC(deviceIDs, currentID) {
        SOSTestDeviceRef source = (SOSTestDeviceRef)CFDictionaryGetValue(testDevices, currentID);
        SOSManifestRef manifest = SOSEngineCopyManifest(SOSDataSourceGetSharedEngine(source->ds, NULL), NULL);
        if (!sourceManifest) {
            sourceManifest = CFRetainSafe(manifest);
            sourceID = currentID;
        } else if (!CFEqual(manifest, sourceManifest)) {
            fail("%s %@ manifest %@ != %@ manifest %@", name, currentID, manifest, sourceID, sourceManifest);
            inSync = false;
        }
        CFReleaseSafe(manifest);
    }
    CFReleaseSafe(sourceManifest);
    if (inSync)
        pass("%s all peers in sync", name);
    return inSync;
}

void SOSTestDeviceListTestSync(const char *name, const char *test_directive, const char *test_reason, CFIndex version, bool use_db,
                               bool(^pre)(SOSTestDeviceRef source, SOSTestDeviceRef dest),
                               bool(^post)(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message), ...) {
    va_list args;
    va_start(args, post);
    // Optionally prefix each peer with name to make them more unique.
    CFArrayRef deviceIDs = CFArrayCreateForVC(kCFAllocatorDefault, &kCFTypeArrayCallBacks, args);
    CFMutableDictionaryRef testDevices = SOSTestDeviceListCreate(use_db, version, deviceIDs);
    CFReleaseSafe(deviceIDs);
    SOSTestDeviceListSync(name, test_directive, test_reason, testDevices, pre, post);
    SOSTestDeviceListInSync(name, test_directive, test_reason, testDevices);
    CFReleaseSafe(testDevices);
}
