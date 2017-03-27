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

// #include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSViews.h>

#include "SOSInternal.h"
#include "SecADWrapper.h"



#include <Security/SecureObjectSync/SOSRecoveryKeyBag.h>
#include <Security/SecureObjectSync/SOSRingRecovery.h>

CFStringRef kRecoveryRingKey = CFSTR("recoveryKeyBag");

bool SOSAccountSetRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccountRef account, SOSRecoveryKeyBagRef rkbg, CFErrorRef *error) {
    CFDataRef rkbg_as_data = NULL;
    bool result = false;
    rkbg_as_data = SOSRecoveryKeyBagCopyEncoded(rkbg, error);
    result = rkbg_as_data && SOSAccountSetValue(account, kRecoveryRingKey, rkbg_as_data, error);
    CFReleaseNull(rkbg_as_data);
    return result;
}

SOSRecoveryKeyBagRef SOSAccountCopyRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccountRef account, CFErrorRef *error) {
    SOSRecoveryKeyBagRef retval = NULL;
    CFDataRef rkbg_as_data = asData(SOSAccountGetValue(account, kRecoveryRingKey, error), error);
    require_quiet(rkbg_as_data, errOut);
    retval = SOSRecoveryKeyBagCreateFromData(allocator, rkbg_as_data, error);
errOut:
    return retval;
}

SOSRecoveryKeyBagRef SOSAccountCopyRecoveryKeyBag(CFAllocatorRef allocator, SOSAccountRef account, CFErrorRef *error) {
    SOSRingRef recRing = NULL;
    SOSRecoveryKeyBagRef rkbg = NULL;
    require_action_quiet(account, errOut, SOSCreateError(kSOSErrorParam, CFSTR("No Account Object"), NULL, error));
    recRing = SOSAccountCopyRingNamed(account, kSOSRecoveryRing, error);
    require_quiet(recRing, errOut);
    rkbg = SOSRingCopyRecoveryKeyBag(recRing, error);
errOut:
    CFReleaseNull(recRing);
    return rkbg;
}

CFDataRef SOSAccountCopyRecoveryPublic(CFAllocatorRef allocator, SOSAccountRef account, CFErrorRef *error) {
    SOSRecoveryKeyBagRef rkbg = SOSAccountCopyRecoveryKeyBag(allocator, account, error);
    CFDataRef recKey = NULL;
    require_quiet(rkbg, errOut);
    CFDataRef tmpKey = SOSRecoveryKeyBagGetKeyData(rkbg, error);
    if(tmpKey) recKey = CFDataCreateCopy(kCFAllocatorDefault, tmpKey);
errOut:
    CFReleaseNull(rkbg);
    return recKey;
}

static bool SOSAccountUpdateRecoveryRing(SOSAccountRef account, CFErrorRef *error,
                                         SOSRingRef (^modify)(SOSRingRef existing, CFErrorRef *error)) {
    bool result = SOSAccountUpdateNamedRing(account, kSOSRecoveryRing, error, ^SOSRingRef(CFStringRef ringName, CFErrorRef *error) {
        return SOSRingCreate(ringName, SOSAccountGetMyPeerID(account), kSOSRingRecovery, error);
    }, modify);
    
    return result;
}

static bool SOSAccountSetKeybagForRecoveryRing(SOSAccountRef account, SOSRecoveryKeyBagRef keyBag, CFErrorRef *error) {
    bool result = SOSAccountUpdateRecoveryRing(account, error, ^SOSRingRef(SOSRingRef existing, CFErrorRef *error) {
        SOSRingRef newRing = NULL;
        CFSetRef peerSet = SOSAccountCopyPeerSetMatching(account, ^bool(SOSPeerInfoRef peer) {
            return true;
        });
        CFMutableSetRef cleared = CFSetCreateMutableForCFTypes(NULL);
        
        SOSRingSetPeerIDs(existing, cleared);
        SOSRingAddAll(existing, peerSet);
        
        require_quiet(SOSRingSetRecoveryKeyBag(existing, SOSAccountGetMyFullPeerInfo(account), keyBag, error), exit);
        
        newRing = CFRetainSafe(existing);
    exit:
        CFReleaseNull(cleared);
        return newRing;
    });
    
    SOSClearErrorIfTrue(result, error);
    
    if (!result) {
        secnotice("recovery", "Got error setting keybag for recovery : %@", error ? (CFTypeRef) *error : (CFTypeRef) CFSTR("No error space."));
    }
    return result;
}


bool SOSAccountRemoveRecoveryKey(SOSAccountRef account, CFErrorRef *error) {
    bool result = SOSAccountSetKeybagForRecoveryRing(account, NULL, error);
    SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, NULL, NULL);
    account->circle_rings_retirements_need_attention = true;
    return result;
}

bool SOSAccountSetRecoveryKey(SOSAccountRef account, CFDataRef pubData, CFErrorRef *error) {
    __block bool result = false;
    CFDataRef oldRecoveryKey = NULL;
    SOSRecoveryKeyBagRef rkbg = NULL;
    
    require_quiet(SOSAccountIsInCircle(account, error), exit);
    oldRecoveryKey = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL); // ok to fail here. don't collect error
    require_action_quiet(!CFEqualSafe(pubData, oldRecoveryKey), exit, result = true);

    CFDataPerformWithHexString(pubData, ^(CFStringRef recoveryKeyString) {
        CFDataPerformWithHexString(oldRecoveryKey, ^(CFStringRef oldRecoveryKeyString) {
            secnotice("recovery", "SetRecoveryPublic: %@ from %@", recoveryKeyString, oldRecoveryKeyString);
        });
    });
    
    rkbg = SOSRecoveryKeyBagCreateForAccount(kCFAllocatorDefault, account, pubData, error);
    require_quiet(rkbg, exit);

    result = SOSAccountSetKeybagForRecoveryRing(account, rkbg, error);
    SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, rkbg, NULL);

    account->circle_rings_retirements_need_attention = true;

exit:
    CFReleaseNull(oldRecoveryKey);
    CFReleaseNull(rkbg);
    SOSClearErrorIfTrue(result, error);
    if (!result) {
        secnotice("recovery", "SetRecoveryPublic Failed: %@", error ? (CFTypeRef) *error : (CFTypeRef) CFSTR("No error space"));
    }
    return result;
}

bool SOSAccountRecoveryKeyIsInBackupAndCurrentInView(SOSAccountRef account, CFStringRef viewname) {
    bool result = false;
    CFErrorRef bsError = NULL;
    CFDataRef backupSliceData = NULL;
    SOSRingRef ring = NULL;
    SOSBackupSliceKeyBagRef backupSlice = NULL;

    CFDataRef rkbg = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL);
    require_quiet(rkbg, errOut);
    
    CFStringRef ringName = SOSBackupCopyRingNameForView(viewname);
    ring = SOSAccountCopyRing(account, ringName, &bsError);
    CFReleaseNull(ringName);
    
    require_quiet(ring, errOut);
    
    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, &bsError);
    require_quiet(backupSliceData, errOut);
    
    backupSlice = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, &bsError);
    require_quiet(backupSlice, errOut);

    result = SOSBKSBPrefixedKeyIsInKeyBag(backupSlice, bskbRkbgPrefix, rkbg);
    CFReleaseNull(backupSlice);
errOut:
    CFReleaseNull(ring);
    CFReleaseNull(rkbg);
    
    if (bsError) {
        secnotice("backup", "Failed to find BKSB: %@, %@ (%@)", backupSliceData, backupSlice, bsError);
    }
    CFReleaseNull(bsError);
    return result;

}

static void sosRecoveryAlertAndNotify(SOSAccountRef account, SOSRecoveryKeyBagRef oldRingRKBG, SOSRecoveryKeyBagRef ringRKBG) {
    secnotice("recovery", "Recovery Key changed: old %@ new %@", oldRingRKBG, ringRKBG);
    notify_post(kSOSCCRecoveryKeyChanged);
}

void SOSAccountEnsureRecoveryRing(SOSAccountRef account) {
    static SOSRecoveryKeyBagRef oldRingRKBG = NULL;
    bool inCircle = SOSAccountIsInCircle(account, NULL);
    CFStringRef accountDSID = SOSAccountGetValue(account, kSOSDSIDKey, NULL); // murfxxx this needs to be consulted still
    SOSRecoveryKeyBagRef acctRKBG = SOSAccountCopyRecoveryKeyBagEntry(kCFAllocatorDefault, account, NULL);
    SOSRecoveryKeyBagRef ringRKBG = SOSAccountCopyRecoveryKeyBag(kCFAllocatorDefault, account, NULL);
    if(!SOSRecoveryKeyBagDSIDIs(ringRKBG, accountDSID)) CFReleaseNull(ringRKBG);
    if(!SOSRecoveryKeyBagDSIDIs(acctRKBG, accountDSID)) CFReleaseNull(acctRKBG);
    
    if(inCircle && acctRKBG == NULL && ringRKBG == NULL) {
        // Nothing to do at this time - notify if this is a change down below.
    } else if(inCircle && acctRKBG == NULL) { // then we have a ringRKBG
        secnotice("recovery", "Harvesting account recovery key from ring");
        SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, ringRKBG, NULL);
    } else if(ringRKBG == NULL) {
        // Nothing to do at this time - notify if this is a change down below.
        secnotice("recovery", "Account has a recovery key, but none found in recovery ring");
    } else if(!CFEqual(acctRKBG, ringRKBG)) {
        secnotice("recovery", "Harvesting account recovery key from ring");
        SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, ringRKBG, NULL);
    }
    
    if(!CFEqualSafe(oldRingRKBG, ringRKBG)) {
        sosRecoveryAlertAndNotify(account, oldRingRKBG, ringRKBG);
        CFTransferRetained(oldRingRKBG, ringRKBG);
    }
    
    CFReleaseNull(ringRKBG);
    CFReleaseNull(acctRKBG);
}
