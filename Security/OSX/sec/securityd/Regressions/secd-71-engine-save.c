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


// Test save and restore of SOSEngine states

#include <SOSCircle/Regressions/SOSTestDevice.h>
#include <SOSCircle/Regressions/SOSTestDataSource.h>
#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"

#include <Security/SecureObjectSync/SOSEnginePriv.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecBase64.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <corecrypto/ccsha2.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecItemDataSource.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecFileLocations.h>

#include <AssertMacros.h>
#include <stdint.h>

static int kTestTestCount = 8;

/*
 Attributes for a v0 engine-state genp item

 MANGO-iPhone:~ mobile$ security item class=genp,acct=engine-state
 acct       : engine-state
 agrp       : com.apple.security.sos
 cdat       : 2016-04-18 20:40:33 +0000
 mdat       : 2016-04-18 20:40:33 +0000
 musr       : //
 pdmn       : dk
 svce       : SOSDataSource-ak
 sync       : 0
 tomb       : 0
 */

#include "secd-71-engine-save-sample1.h"

static bool verifyV2EngineState(SOSDataSourceRef ds, CFStringRef myPeerID) {
    bool rx = false;
    CFErrorRef error = NULL;
    SOSTransactionRef txn = NULL;
    CFDataRef basicEngineState = NULL;
    CFDictionaryRef engineState = NULL;

    SKIP: {
        CFDataRef basicEngineState = SOSDataSourceCopyStateWithKey(ds, kSOSEngineStatev2, kSecAttrAccessibleAlwaysPrivate, txn, &error);
        skip("Failed to get V2 engine state", 2, basicEngineState);
        ok(basicEngineState, "SOSDataSourceCopyStateWithKey:kSOSEngineStatev2");
        engineState = derStateToDictionaryCopy(basicEngineState, &error);
        skip("Failed to DER decode V2 engine state", 1, basicEngineState);
        CFStringRef engID = (CFStringRef)asString(CFDictionaryGetValue(engineState, CFSTR("id")), &error);
        ok(CFEqualSafe(myPeerID, engID),"Check myPeerID");
        rx = true;
    }
    
    CFReleaseSafe(basicEngineState);
    CFReleaseSafe(engineState);
    CFReleaseSafe(error);
    return rx;
}

static bool verifyV2PeerStates(SOSDataSourceRef ds, CFStringRef myPeerID, CFArrayRef peers) {
    bool rx = false;
    __block CFErrorRef error = NULL;
    SOSTransactionRef txn = NULL;
    CFDictionaryRef peerStateDict = NULL;
    CFDataRef data = NULL;
    __block CFIndex peerCount = CFArrayGetCount(peers) - 1; // drop myPeerID

    SKIP: {
        data = SOSDataSourceCopyStateWithKey(ds, kSOSEnginePeerStates, kSOSEngineProtectionDomainClassD, txn, &error);
        skip("Failed to get V2 peerStates", 3, data);

        peerStateDict = derStateToDictionaryCopy(data, &error);
        skip("Failed to DER decode V2 peerStates", 2, peerStateDict);
        ok(peerStateDict, "SOSDataSourceCopyStateWithKey:kSOSEnginePeerStates");

        // Check that each peer passed in exists in peerStateDict
        CFArrayForEach(peers, ^(const void *key) {
            CFStringRef peerID = (CFStringRef)asString(key, &error);
            if (!CFEqualSafe(myPeerID, peerID)) {
                if (CFDictionaryContainsKey(peerStateDict, peerID))
                    peerCount--;
            }
        });
        ok(peerCount==0,"Peers exist in peer list (%ld)", (CFArrayGetCount(peers) - 1 - peerCount));
        rx = true;
    }

    CFReleaseSafe(peerStateDict);
    CFReleaseSafe(data);
    CFReleaseSafe(error);
    return rx;
}

static bool checkV2EngineStates(SOSTestDeviceRef td, CFStringRef myPeerID, CFArrayRef peers) {
    bool rx = true;
    CFErrorRef error = NULL;
    SOSTransactionRef txn = NULL;
    CFDictionaryRef manifestCache = NULL;
    CFDataRef data = NULL;
//  CFMutableDictionaryRef codersDict = NULL;   // SOSEngineLoadCoders

    rx &= verifyV2EngineState(td->ds, myPeerID);

    data = SOSDataSourceCopyStateWithKey(td->ds, kSOSEngineManifestCache, kSOSEngineProtectionDomainClassD, txn, &error);
    manifestCache = derStateToDictionaryCopy(data, &error);
    CFReleaseNull(data);

    rx &= verifyV2PeerStates(td->ds, myPeerID, peers);

    CFReleaseSafe(manifestCache);
    return rx;
}

static void testSaveRestore(void) {
    CFMutableArrayRef deviceIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(deviceIDs, CFSTR("lemon"));
    CFArrayAppendValue(deviceIDs, CFSTR("lime"));
    CFArrayAppendValue(deviceIDs, CFSTR("orange"));
    bool bx = false;
    int version = 2;
    CFErrorRef error = NULL;
    __block int devIdx = 0;
    CFMutableDictionaryRef testDevices = SOSTestDeviceListCreate(false, version, deviceIDs, ^(SOSDataSourceRef ds) {
        // This block is called before SOSEngineLoad
        if (devIdx == 0) {
            // Test migration of v0 to v2 engine state
            CFDataRef engineStateData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, es_mango_bin, es_mango_bin_len, kCFAllocatorNull);
            SOSTestDeviceAddV0EngineStateWithData(ds, engineStateData);
            CFReleaseSafe(engineStateData);
        }
        devIdx++;
    });

    CFStringRef sourceID = (CFStringRef)CFArrayGetValueAtIndex(deviceIDs, 0);
    SOSTestDeviceRef source = (SOSTestDeviceRef)CFDictionaryGetValue(testDevices, sourceID);

    ok(SOSTestDeviceEngineSave(source, &error),"SOSTestDeviceEngineSave: %@",error);

    bx = SOSTestDeviceEngineLoad(source, &error);
    ok(bx,"SOSTestEngineLoad: %@",error);

    bx = checkV2EngineStates(source, sourceID, deviceIDs);
    ok(bx,"getV2EngineStates: %@",error);

    bx = SOSTestDeviceEngineSave(source, &error);
    ok(bx,"SOSTestDeviceEngineSave v2: %@",error);

    SOSTestDeviceDestroyEngine(testDevices);
    CFReleaseSafe(deviceIDs);
    CFReleaseSafe(testDevices);
    CFReleaseSafe(error);
}
                                                                                        
int secd_71_engine_save(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    testSaveRestore();
    
    return 0;
}
