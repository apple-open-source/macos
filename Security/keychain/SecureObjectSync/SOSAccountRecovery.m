/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
//  SOSAccountRecovery.c
//  Security
//

#include <AssertMacros.h>
#include "SOSAccountPriv.h"
#include "SOSCloudKeychainClient.h"

#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"

#include "keychain/SecureObjectSync/SOSInternal.h"
#include "SecADWrapper.h"

#include "keychain/SecureObjectSync/SOSRecoveryKeyBag.h"
#include "keychain/SecureObjectSync/SOSRingRecovery.h"

CFStringRef kRecoveryRingKey = CFSTR("recoveryKeyBag");

// When a recovery key is harvested from a recovery ring it's sent here
bool SOSAccountSetRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccount* account, SOSRecoveryKeyBagRef rkbg, CFErrorRef *error) {
    bool result = false;
    if(rkbg == NULL) {
        result = SOSAccountClearValue(account, kRecoveryRingKey, error);
    } else {
        CFDataRef recoverKeyData = SOSRecoveryKeyBagGetKeyData(rkbg, NULL);
        if(CFEqualSafe(recoverKeyData, SOSRKNullKey())) {
            result = SOSAccountClearValue(account, kRecoveryRingKey, error);
        } else {
            CFDataRef rkbg_as_data = SOSRecoveryKeyBagCopyEncoded(rkbg, error);
            result = rkbg_as_data && SOSAccountSetValue(account, kRecoveryRingKey, rkbg_as_data, error);
            CFReleaseNull(rkbg_as_data);
        }
    }
    return result;
}

SOSRecoveryKeyBagRef SOSAccountCopyRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccount* account, CFErrorRef *error) {
    SOSRecoveryKeyBagRef retval = NULL;
    CFDataRef rkbg_as_data = asData(SOSAccountGetValue(account, kRecoveryRingKey, error), error);
    require_quiet(rkbg_as_data, errOut);
    retval = SOSRecoveryKeyBagCreateFromData(allocator, rkbg_as_data, error);
errOut:
    return retval;
}

CFDataRef SOSAccountCopyRecoveryPublic(CFAllocatorRef allocator, SOSAccount* account, CFErrorRef *error) {
    SOSRecoveryKeyBagRef rkbg = SOSAccountCopyRecoveryKeyBagEntry(allocator, account, error);
    CFDataRef recKey = NULL;
    require_quiet(rkbg, errOut);
    CFDataRef tmpKey = SOSRecoveryKeyBagGetKeyData(rkbg, error);
    require_quiet(tmpKey, errOut);
    require_quiet(!CFEqualSafe(tmpKey, SOSRKNullKey()), errOut);
    recKey = CFDataCreateCopy(kCFAllocatorDefault, tmpKey);
errOut:
    CFReleaseNull(rkbg);
    if(!recKey) {
        if(error && !(*error)) SOSErrorCreate(kSOSErrorNoKey, error, NULL, CFSTR("No recovery key available"));
    }
    return recKey;
}

static bool SOSAccountUpdateRecoveryRing(SOSAccount* account, CFErrorRef *error,
                                         SOSRingRef (^modify)(SOSRingRef existing, CFErrorRef *error)) {
    bool result = SOSAccountUpdateNamedRing(account, kSOSRecoveryRing, error, ^SOSRingRef(CFStringRef ringName, CFErrorRef *error) {
        return SOSRingCreate(ringName, (__bridge CFStringRef)(account.peerID), kSOSRingRecovery, error);
    }, modify);
    
    return result;
}

bool SOSAccountRemoveRecoveryKey(SOSAccount* account, CFErrorRef *error) {
    return SOSAccountSetRecoveryKey(account, NULL, error);
}


// Recovery Setting From a Local Client goes through here
bool SOSAccountSetRecoveryKey(SOSAccount* account, CFDataRef pubData, CFErrorRef *error) {
    __block bool result = false;
    CFDataRef oldRecoveryKey = NULL;
    SOSRecoveryKeyBagRef rkbg = NULL;

    if(![account isInCircle:error]) return false;
    oldRecoveryKey = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL); // ok to fail here. don't collect error

    CFDataPerformWithHexString(pubData, ^(CFStringRef recoveryKeyString) {
        CFDataPerformWithHexString(oldRecoveryKey, ^(CFStringRef oldRecoveryKeyString) {
            secnotice("recovery", "SetRecoveryPublic: %@ from %@", recoveryKeyString, oldRecoveryKeyString);
        });
    });
    CFReleaseNull(oldRecoveryKey);

    rkbg = SOSRecoveryKeyBagCreateForAccount(kCFAllocatorDefault, (__bridge CFTypeRef)account, pubData, error);
    SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, rkbg, NULL);
    SOSAccountUpdateRecoveryRing(account, error, ^SOSRingRef(SOSRingRef existing, CFErrorRef *error) {
        SOSRingRef newRing = NULL;
        CFMutableSetRef peerInfoIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSCircleForEachValidSyncingPeer(account.trust.trustedCircle, account.accountKey, ^(SOSPeerInfoRef peer) {
            CFSetAddValue(peerInfoIDs, SOSPeerInfoGetPeerID(peer));
        });
        SOSRingSetPeerIDs(existing, peerInfoIDs);
        if(rkbg) {
            SOSRingSetRecoveryKeyBag(existing, account.fullPeerInfo, rkbg, error);
        } else {
            SOSRecoveryKeyBagRef ringrkbg = SOSRecoveryKeyBagCreateForAccount(kCFAllocatorDefault, (__bridge CFTypeRef)account, SOSRKNullKey(), error);
            SOSRingSetRecoveryKeyBag(existing, account.fullPeerInfo, ringrkbg, error);
            CFRelease(ringrkbg);
        }
        SOSRingGenerationSign(existing, NULL, account.trust.fullPeerInfo, error);
        newRing = CFRetainSafe(existing);
        return newRing;
    });
    CFReleaseNull(rkbg);

    if(SOSPeerInfoHasBackupKey(account.trust.peerInfo)) {
        SOSAccountProcessBackupRings(account, error);
    }
    account.circle_rings_retirements_need_attention = true;
    result = true;

    SOSClearErrorIfTrue(result, error);
    if (!result) {
        // if we're failing and something above failed to give an error - make a generic one.
        if(error && !(*error)) SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("Failed to set Recovery Key"));
        secnotice("recovery", "SetRecoveryPublic Failed: %@", error ? (CFTypeRef) *error : (CFTypeRef) CFSTR("No error space"));
    }
    return result;
}

bool SOSAccountRecoveryKeyIsInBackupAndCurrentInView(SOSAccount* account, CFStringRef viewname) {
    bool result = false;
    CFErrorRef bsError = NULL;
    CFDataRef backupSliceData = NULL;
    SOSRingRef ring = NULL;
    SOSBackupSliceKeyBagRef backupSlice = NULL;

    CFDataRef recoveryKeyFromAccount = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL);
    require_quiet(recoveryKeyFromAccount, errOut);

    CFStringRef ringName = SOSBackupCopyRingNameForView(viewname);
    ring = [account.trust copyRing:ringName err:&bsError];
    CFReleaseNull(ringName);
    
    require_quiet(ring, errOut);
    
    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, &bsError);
    require_quiet(backupSliceData, errOut);
    
    backupSlice = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, &bsError);
    require_quiet(backupSlice, errOut);

    result = SOSBKSBPrefixedKeyIsInKeyBag(backupSlice, bskbRkbgPrefix, recoveryKeyFromAccount);
    CFReleaseNull(backupSlice);
errOut:
    CFReleaseNull(ring);
    CFReleaseNull(recoveryKeyFromAccount);
    
    if (bsError) {
        secnotice("backup", "Failed to find BKSB: %@, %@ (%@)", backupSliceData, backupSlice, bsError);
    }
    CFReleaseNull(bsError);
    return result;

}

static void sosRecoveryAlertAndNotify(SOSAccount* account, SOSRecoveryKeyBagRef oldRingRKBG, SOSRecoveryKeyBagRef ringRKBG) {
    secnotice("recovery", "Recovery Key changed: old %@ new %@", oldRingRKBG, ringRKBG);
    notify_post(kSOSCCRecoveryKeyChanged);
}

void SOSAccountEnsureRecoveryRing(SOSAccount* account) {
    dispatch_assert_queue(account.queue);

    static SOSRecoveryKeyBagRef oldRingRKBG = NULL;
    SOSRecoveryKeyBagRef acctRKBG = SOSAccountCopyRecoveryKeyBagEntry(kCFAllocatorDefault, account, NULL);
    if(!CFEqualSafe(acctRKBG, oldRingRKBG)) {
        sosRecoveryAlertAndNotify(account, oldRingRKBG, acctRKBG);
        CFRetainAssign(oldRingRKBG, acctRKBG);
    }
}
