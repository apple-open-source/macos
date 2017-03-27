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
//  SOSRecoveryKeyBag.c
//  sec
//

#include "SOSRecoveryKeyBag.h"
#include "AssertMacros.h"
#include <Security/SecureObjectSync/SOSGenCount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecAKSWrappers.h>
#include <utilities/SecBuffer.h>
#include <utilities/SecCFError.h>
#include <utilities/der_set.h>
#include <utilities/der_plist_internal.h>
#include <Security/SecRandom.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccrng.h>

#include <limits.h>

#include "SOSInternal.h"

#define CURRENT_RKB_VERSION 1

//
// MARK: Type creation
//


struct __OpaqueSOSRecoveryKeyBag {
    CFRuntimeBase   _base;
    CFStringRef     accountDSID;
    SOSGenCountRef  generation;
    uint64_t         rkbVersion;
    CFDataRef       recoveryKeyBag;
};



static void SOSRecoveryKeyBagDestroy(CFTypeRef aObj) {
    SOSRecoveryKeyBagRef rb = (SOSRecoveryKeyBagRef) aObj;
    
    CFReleaseNull(rb->accountDSID);
    CFReleaseNull(rb->generation);
    CFReleaseNull(rb->recoveryKeyBag);
}

static CFStringRef SOSRecoveryKeyBagCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSRecoveryKeyBagRef rb = (SOSRecoveryKeyBagRef) aObj;
    CFStringRef gcString = SOSGenerationCountCopyDescription(rb->generation);
    CFStringRef rkbID = SOSCopyIDOfDataBufferWithLength(rb->recoveryKeyBag, 8, NULL);

    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    CFStringAppendFormat(description, NULL, CFSTR("<SOSRecoveryKeyBag@%p DSID: %@ version: %d  gencount: %@  RecoveryKeyID: %@ "), rb, rb->accountDSID, (int) rb->rkbVersion, gcString, rkbID);
    CFStringAppend(description, CFSTR(">"));
    
    CFReleaseNull(gcString);
    CFReleaseNull(rkbID);
    return description;
}

CFGiblisFor(SOSRecoveryKeyBag);

// Der encoding/decoding
const uint8_t* der_decode_RecoveryKeyBag(CFAllocatorRef allocator,
                                         SOSRecoveryKeyBagRef* RecoveryKeyBag, CFErrorRef *error,
                                         const uint8_t* der, const uint8_t *der_end) {
    if (der == NULL) return der;
    const uint8_t *result = NULL;
    
    SOSRecoveryKeyBagRef rb = CFTypeAllocate(SOSRecoveryKeyBag, struct __OpaqueSOSRecoveryKeyBag, allocator);
    require_quiet(SecAllocationError(rb, error, CFSTR("Recovery bag allocation failed")), fail);
    
    const uint8_t *sequence_end = NULL;
    der = ccder_decode_sequence_tl(&sequence_end, der, der_end);
    require_quiet(sequence_end == der_end, fail);
    
    der = der_decode_string(kCFAllocatorDefault, kCFPropertyListImmutable, &rb->accountDSID, error, der, sequence_end);
    rb->generation = SOSGenCountCreateFromDER(kCFAllocatorDefault, error, &der, sequence_end);
    der = ccder_decode_uint64(&rb->rkbVersion, der, sequence_end);
    der = der_decode_data(allocator, kCFPropertyListImmutable, &rb->recoveryKeyBag, error, der, sequence_end);
    
    require_quiet(SecRequirementError(der == der_end, error, CFSTR("Extra space in sequence")), fail);
    if (RecoveryKeyBag) CFTransferRetained(*RecoveryKeyBag, rb);
    result = der;
fail:
    CFReleaseNull(rb);
    return result;
}

size_t der_sizeof_RecoveryKeyBag(SOSRecoveryKeyBagRef RecoveryKeyBag, CFErrorRef *error) {
    size_t result = 0;
    if(RecoveryKeyBag && RecoveryKeyBag->recoveryKeyBag && RecoveryKeyBag->accountDSID && RecoveryKeyBag->generation) {
        size_t partSize = der_sizeof_string(RecoveryKeyBag->accountDSID, NULL);
        partSize += SOSGenCountGetDEREncodedSize(RecoveryKeyBag->generation, NULL);
        partSize += ccder_sizeof_uint64(RecoveryKeyBag->rkbVersion);
        partSize += der_sizeof_data(RecoveryKeyBag->recoveryKeyBag, NULL);
        result = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, partSize);
    } else {
        SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Null RecoveryKeyBag"), NULL, error);
    }
    return result;
}

uint8_t* der_encode_RecoveryKeyBag(SOSRecoveryKeyBagRef RecoveryKeyBag, CFErrorRef *error,
                                   const uint8_t *der, uint8_t *der_end) {
    uint8_t *result = NULL;
    if (der_end == NULL) return der_end;
    if(RecoveryKeyBag && RecoveryKeyBag->recoveryKeyBag && RecoveryKeyBag->accountDSID && RecoveryKeyBag->generation) {
        der_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                    der_encode_string(RecoveryKeyBag->accountDSID, error, der,
                    SOSGenCountEncodeToDER(RecoveryKeyBag->generation, error, der,
                    ccder_encode_uint64(RecoveryKeyBag->rkbVersion, der,
                    der_encode_data(RecoveryKeyBag->recoveryKeyBag, error, der, der_end)))));
        
        require_quiet(der_end == der, errOut);
        result = der_end;
    } else {
        SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Null RecoveryKeyBag"), NULL, error);
    }
    
errOut:
    return result;
}

SOSRecoveryKeyBagRef SOSRecoveryKeyBagCreateForAccount(CFAllocatorRef allocator, SOSAccountRef account, CFDataRef pubData, CFErrorRef *error) {
    SOSRecoveryKeyBagRef retval = NULL;
    SOSGenCountRef gencount = NULL;
    require_action_quiet(account, errOut, SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Null Account Object"), NULL, error));
    CFStringRef dsid = asString(SOSAccountGetValue(account, kSOSDSIDKey, NULL), error);
    gencount = SOSGenerationCreate();
    
    require_action_quiet(pubData && dsid && gencount, errOut, SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Couldn't get recovery keybag components"), NULL, error));
    retval = CFTypeAllocate(SOSRecoveryKeyBag, struct __OpaqueSOSRecoveryKeyBag, allocator);
    require_action_quiet(retval, errOut, SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Couldn't get memory for recoveryKeyBag"), NULL, error));
    retval->rkbVersion = CURRENT_RKB_VERSION;
    retval->accountDSID = CFStringCreateCopy(allocator, dsid);
    CFRetainAssign(retval->generation, gencount);
    retval->recoveryKeyBag = CFDataCreateCopy(allocator, pubData);
errOut:
    CFReleaseNull(gencount);
    return retval;
}

CFDataRef SOSRecoveryKeyCopyKeyForAccount(CFAllocatorRef allocator, SOSAccountRef account, SOSRecoveryKeyBagRef recoveryKeyBag, CFErrorRef *error) {
    CFDataRef retval = NULL;
    require_action_quiet(recoveryKeyBag && recoveryKeyBag->recoveryKeyBag && recoveryKeyBag->accountDSID,
                         errOut, SOSCreateError(kSOSErrorDecodeFailure, CFSTR("Null recoveryKeyBag Object"), NULL, error));
    CFStringRef dsid = asString(SOSAccountGetValue(account, kSOSDSIDKey, NULL), error);
    require_action_quiet(dsid, errOut, SOSCreateError(kSOSErrorDecodeFailure, CFSTR("No DSID in Account"), NULL, error));
    require_action_quiet(CFEqual(dsid, recoveryKeyBag->accountDSID), errOut, SOSCreateError(kSOSErrorDecodeFailure, CFSTR("Account/RecoveryKeybag DSID miss-match"), NULL, error));
    retval = CFDataCreateCopy(allocator, recoveryKeyBag->recoveryKeyBag);
errOut:
    return retval;
}


CFDataRef SOSRecoveryKeyBagCopyEncoded(SOSRecoveryKeyBagRef RecoveryKeyBag, CFErrorRef* error) {
    CFDataRef result = NULL;
    CFMutableDataRef encoded = NULL;
    
    size_t encodedSize = der_sizeof_RecoveryKeyBag(RecoveryKeyBag, error);
    require_quiet(encodedSize, fail);
    
    encoded = CFDataCreateMutableWithScratch(kCFAllocatorDefault, encodedSize);
    require_quiet(SecAllocationError(encoded, error, CFSTR("Failed to create scratch")), fail);
    
    uint8_t *encode_to = CFDataGetMutableBytePtr(encoded);
    uint8_t *encode_to_end = encode_to + CFDataGetLength(encoded);
    require_quiet(encode_to == der_encode_RecoveryKeyBag(RecoveryKeyBag, error, encode_to, encode_to_end), fail);
    
    CFTransferRetained(result, encoded);
    
fail:
    CFReleaseSafe(encoded);
    return result;
}



SOSRecoveryKeyBagRef SOSRecoveryKeyBagCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error) {
    SOSRecoveryKeyBagRef result = NULL;
    SOSRecoveryKeyBagRef decodedBag = NULL;
    
    const uint8_t *der = CFDataGetBytePtr(data);
    const uint8_t *der_end = der + CFDataGetLength(data);
    
    der = der_decode_RecoveryKeyBag(allocator, &decodedBag, error, der, der_end);
    
    require_quiet(SecRequirementError(der == der_end, error, CFSTR("Didn't consume all data supplied")), fail);
    
    CFTransferRetained(result, decodedBag);
    
fail:
    CFReleaseNull(decodedBag);
    return result;
}

CFDataRef SOSRecoveryKeyBagGetKeyData(SOSRecoveryKeyBagRef rkbg, CFErrorRef *error) {
    return rkbg->recoveryKeyBag;
}

bool SOSRecoveryKeyBagDSIDIs(SOSRecoveryKeyBagRef rkbg, CFStringRef dsid) {
    if(!rkbg) return false;
    return CFEqualSafe(rkbg->accountDSID, dsid);
}




