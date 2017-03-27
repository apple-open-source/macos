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
//  SOSRingRecovery.c
//  sec
//

#include "SOSRingRecovery.h"
#include "SOSRingBackup.h"

#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSRecoveryKeyBag.h>

#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFWrappers.h>

#include <stdlib.h>
#include <assert.h>

#include "SOSRingUtils.h"
#include "SOSRingTypes.h"
#include "SOSRingBasic.h"

// MARK: Recovery Ring Ops

static SOSRingRef SOSRingCreate_Recovery(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error) {
    return SOSRingCreate_ForType(name, kSOSRingRecovery, myPeerID, error);
}



ringFuncStruct recovery = {
    "Recovery",
    1,
    SOSRingCreate_Recovery,
    SOSRingResetToEmpty_Basic,
    SOSRingResetToOffering_Basic,
    SOSRingDeviceIsInRing_Basic,
    SOSRingApply_Basic,
    SOSRingWithdraw_Basic,
    SOSRingGenerationSign_Basic,
    SOSRingConcordanceSign_Basic,
    SOSRingPeerKeyConcordanceTrust,
    NULL,
    NULL,
    SOSRingSetPayload_Basic,
    SOSRingGetPayload_Basic,
};


static bool isRecoveryRing(SOSRingRef ring, CFErrorRef *error) {
    SOSRingType type = SOSRingGetType(ring);
    require_quiet(kSOSRingRecovery == type, errOut);
    return true;
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not recovery ring type"), NULL, error);
    return false;
}

bool SOSRingSetRecoveryKeyBag(SOSRingRef ring, SOSFullPeerInfoRef fpi, SOSRecoveryKeyBagRef rkbg, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    CFDataRef rkbg_as_data = NULL;
    bool result = false;
    require_quiet(isRecoveryRing(ring, error), errOut);
    
    rkbg_as_data = SOSRecoveryKeyBagCopyEncoded(rkbg, error);
    result = rkbg_as_data &&
    SOSRingSetPayload(ring, NULL, rkbg_as_data, fpi, error);
errOut:
    CFReleaseNull(rkbg_as_data);
    return result;
}

SOSRecoveryKeyBagRef SOSRingCopyRecoveryKeyBag(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    
    CFDataRef rkbg_as_data = NULL;
    SOSRecoveryKeyBagRef result = NULL;
    require_quiet(isRecoveryRing(ring, error), errOut);
    
    rkbg_as_data = SOSRingGetPayload(ring, error);
    require_quiet(rkbg_as_data, errOut);
    
    result = SOSRecoveryKeyBagCreateFromData(kCFAllocatorDefault, rkbg_as_data, error);
    
errOut:
    return result;
}


