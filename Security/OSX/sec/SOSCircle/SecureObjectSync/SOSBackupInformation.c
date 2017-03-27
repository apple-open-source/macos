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
//  SOSBackupInformation.c
//  Security
//

#include "SOSBackupInformation.h"
#include "SOSAccountPriv.h"
#include <CoreFoundation/CFNumber.h>
#include <utilities/SecCFWrappers.h>

const CFStringRef kSOSBkpInfoStatus   = CFSTR("BkpInfoStatus");
const CFStringRef kSOSBkpInfoBSKB   = CFSTR("BkpInfoBSKB");
const CFStringRef kSOSBkpInfoRKBG   = CFSTR("BkpInfoRKBG");

CFDictionaryRef SOSBackupInformation(SOSAccountTransactionRef txn, CFErrorRef *error) {
    CFNumberRef status = NULL;
    int ibkpInfoStatus;
    __block bool havebskbcontent = false;
    CFMutableDictionaryRef retval = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    require_action_quiet(txn && txn->account, errOut, ibkpInfoStatus = noTxnorAcct);
    require_action_quiet(retval, errOut, ibkpInfoStatus = noAlloc);
    require_action_quiet(txn, errOut, ibkpInfoStatus = noTxnorAcct);
    SOSAccountRef account = txn->account;
    require_action_quiet(account->user_public && account->user_public_trusted, errOut, ibkpInfoStatus = noTrustedPubKey);
    CFMutableDictionaryRef bskbders = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountForEachRing(account, ^SOSRingRef(CFStringRef name, SOSRingRef ring) {
        if(SOSRingGetType(ring) == kSOSRingBackup) {
            CFDataRef bskbder = SOSRingGetPayload(ring, NULL);
            if(bskbder) CFDictionaryAddValue(bskbders, name, bskbder);
            havebskbcontent = true;
        } else if(SOSRingGetType(ring) == kSOSRingRecovery) {
            CFDataRef rkbgder = SOSRingGetPayload(ring, NULL);
            if(rkbgder) CFDictionaryAddValue(retval, kSOSBkpInfoRKBG, rkbgder);
        }
        return NULL; // we're reporting -  never changing the ring
    });
    if(havebskbcontent) {
        ibkpInfoStatus = noError;
        CFDictionaryAddValue(retval, kSOSBkpInfoBSKB, bskbders);
    } else {
        ibkpInfoStatus = noBSKBs;
    }
    CFReleaseNull(bskbders);
    
errOut:
    status = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ibkpInfoStatus);
    CFDictionaryAddValue(retval, kSOSBkpInfoStatus, status);
    CFReleaseNull(status);
    return retval;
}
