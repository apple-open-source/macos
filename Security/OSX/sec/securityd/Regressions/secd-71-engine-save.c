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

#include <Security/SecureObjectSync/SOSEngine.h>
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

static int kTestTestCount = 28 + 1; // +1 for secd_test_setup_temp_keychain

#include "secd-71-engine-save-sample1.h"

static bool addEngineStateWithData(CFDataRef engineStateData) {
    /*
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

    CFMutableDictionaryRef item = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(item, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(item, kSecAttrAccount, CFSTR("engine-state"));
    CFDictionarySetValue(item, kSecAttrAccessGroup, CFSTR("com.apple.security.sos"));
    CFDictionarySetValue(item, kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate);
    CFDictionarySetValue(item, kSecAttrService, CFSTR("SOSDataSource-ak"));
    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionarySetValue(item, kSecValueData, engineStateData);

    CFErrorRef localError = NULL;
    OSStatus status = noErr;
    is_status(status = SecItemAdd(item, (CFTypeRef *)&localError), errSecSuccess, "add v0 engine-state");
    CFReleaseSafe(item);
    CFReleaseSafe(localError);
    return status == noErr;
}

#if 0
static void testsync2(const char *name,  const char *test_directive, const char *test_reason, void (^aliceInit)(SOSDataSourceRef ds), void (^bobInit)(SOSDataSourceRef ds), CFStringRef msg, ...) {
    __block int iteration=0;
    SOSTestDeviceListTestSync(name, test_directive, test_reason, kSOSPeerVersion, false, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        if (iteration == 96) {
            pass("%@ before message", source);
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        iteration++;
        if (iteration == 60) {
            pass("%@ before addition", source);
            //SOSTestDeviceAddGenericItem(source, CFSTR("test_account"), CFSTR("test service"));
            SOSTestDeviceAddRemoteGenericItem(source, CFSTR("test_account"), CFSTR("test service"));
            pass("%@ after addition", source);
            return true;
        }
        return false;
    }, CFSTR("alice"), CFSTR("bob"), CFSTR("claire"), CFSTR("dave"),CFSTR("edward"), CFSTR("frank"), CFSTR("gary"), NULL);
}
#endif

static void testsync2p(void) {
    __block int iteration = 0;
    SOSTestDeviceListTestSync("testsync2p", test_directive, test_reason, 0, false, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        // Add 10 items in first 10 sync messages
        if (iteration <= 10) {
            CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
            SOSTestDeviceAddGenericItem(source, account, CFSTR("testsync2p"));
            CFReleaseSafe(account);
            return true;
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        return false;
    }, CFSTR("Atestsync2p"), CFSTR("Btestsync2p"), NULL);
}

static void savetests(void) {
    ok(true,"message");
//    SOSEngineSave(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error)
    testsync2p();
}

int secd_71_engine_save(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    /* custom keychain dir */
  //  secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    secd_test_setup_temp_keychain(__FUNCTION__, ^{
        CFStringRef keychain_path_cf = __SecKeychainCopyPath();

        CFDataRef engineStateData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, es_mango_bin, es_mango_bin_len, kCFAllocatorNull);
        ok(addEngineStateWithData(engineStateData),"failed to add v0 engine state");
        CFReleaseSafe(engineStateData);
        CFReleaseSafe(keychain_path_cf);
    });

    // TODO: use call that prepopulates keychain (block for above)
    ok(sizeof(es_mango_bin)== es_mango_bin_len,"bad mango");
    savetests();
    
    return 0;
}
