/*
 * Copyright (c) 2015-2016 Apple Inc. All Rights Reserved.
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
//  SOSRingTypes.c
//

#include "SOSRing.h"
#include "SOSRingTypes.h"
#include "SOSRingBasic.h"
#include "SOSRingBackup.h"
#include "SOSRingRecovery.h"
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>

// These need to track the ring type enums in SOSRingTypes.h
static ringFuncs ringTypes[] = {
    &basic,     // kSOSRingBase
    &backup,    // kSOSRingBackup
    NULL,       // kSOSRingPeerKeyed
    NULL,       // kSOSRingEntropyKeyed
    NULL,       // kSOSRingPKKeyed
    &recovery,  // kSOSRingRecovery
};
static const size_t typecount = sizeof(ringTypes) / sizeof(ringFuncs);

static bool SOSRingValidType(SOSRingType type) {
    if(type >= typecount || ringTypes[type] == NULL) return false;
    return true;
}

// MARK: Exported Functions


SOSRingRef SOSRingCreate(CFStringRef name, CFStringRef myPeerID, SOSRingType type, CFErrorRef *error) {
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingCreate, errOut);
    return ringTypes[type]->sosRingCreate(name, myPeerID, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return NULL;
}

bool SOSRingResetToEmpty(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingResetToEmpty, errOut);
    return ringTypes[type]->sosRingResetToEmpty(ring, myPeerID, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

bool SOSRingResetToOffering(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingResetToOffering, errOut);
    return ringTypes[type]->sosRingResetToOffering(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

SOSRingStatus SOSRingDeviceIsInRing(SOSRingRef ring, CFStringRef peerID) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingDeviceIsInRing, errOut);
    return ringTypes[type]->sosRingDeviceIsInRing(ring, peerID);
errOut:
    return kSOSRingError;
}

bool SOSRingApply(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingApply, shortCircuit);
    require_quiet(SOSPeerInfoApplicationVerify(SOSFullPeerInfoGetPeerInfo(fpi), user_pubkey, error), errOut2);

    return ringTypes[type]->sosRingApply(ring, user_pubkey, fpi, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
errOut2:
    SOSCreateError(kSOSErrorBadSignature, CFSTR("FullPeerInfo fails userkey signature check"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingWithdraw(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingWithdraw, shortCircuit);
    return ringTypes[type]->sosRingWithdraw(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingGenerationSign(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingGenerationSign, shortCircuit);
    return ringTypes[type]->sosRingGenerationSign(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingConcordanceSign(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingConcordanceSign, shortCircuit);
    return ringTypes[type]->sosRingConcordanceSign(ring, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

SOSConcordanceStatus SOSRingConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers,
                                             SOSRingRef knownRing, SOSRingRef proposedRing,
                                             SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                             CFStringRef excludePeerID, CFErrorRef *error) {
    SOSRingAssertStable(knownRing);
    SOSRingAssertStable(proposedRing);
    SOSRingType type1 = SOSRingGetType(knownRing);
    SOSRingType type2 = SOSRingGetType(proposedRing);
    require(SOSRingValidType(type1), errOut);
    require(SOSRingValidType(type2), errOut);
    require(type1 == type2, errOut);

    secnotice("ring", "concordance trust (%s) knownRing: %@ proposedRing: %@ knownkey: %@ userkey: %@ excluded: %@",
              ringTypes[type1]->typeName, knownRing, proposedRing, knownPubkey, userPubkey, excludePeerID);

    require(ringTypes[type1]->sosRingConcordanceTrust, errOut);
    return ringTypes[type1]->sosRingConcordanceTrust(me, peers, knownRing, proposedRing, knownPubkey, userPubkey, excludePeerID, error);
errOut:
    return kSOSConcordanceError;
}

bool SOSRingAccept(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingAccept, shortCircuit);
    return ringTypes[type]->sosRingAccept(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingReject(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingReject, shortCircuit);
    return ringTypes[type]->sosRingReject(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingSetPayload(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingSetPayload, errOut);
    return ringTypes[type]->sosRingSetPayload(ring, user_privkey, payload, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

CFDataRef SOSRingGetPayload(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingGetPayload, errOut);
    return ringTypes[type]->sosRingGetPayload(ring, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return NULL;
}

CFSetRef SOSRingGetBackupViewset(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(kSOSRingBackup == type, errOut);
    return SOSRingGetBackupViewset_Internal(ring);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not backup ring type"), NULL, error);
    return NULL;
}

static bool isBackupRing(SOSRingRef ring, CFErrorRef *error) {
    SOSRingType type = SOSRingGetType(ring);
    require_quiet(kSOSRingBackup == type, errOut);
    return true;
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not backup ring type"), NULL, error);
    return false;
}

bool SOSRingSetBackupKeyBag(SOSRingRef ring, SOSFullPeerInfoRef fpi, CFSetRef viewSet, SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    CFDataRef bskb_as_data = NULL;
    bool result = false;
    require_quiet(isBackupRing(ring, error), errOut);

    bskb_as_data = SOSBSKBCopyEncoded(bskb, error);
    result = bskb_as_data &&
             SOSRingSetBackupViewset_Internal(ring, viewSet) &&
             SOSRingSetPayload(ring, NULL, bskb_as_data, fpi, error);
errOut:
    CFReleaseNull(bskb_as_data);
    return result;
}

SOSBackupSliceKeyBagRef SOSRingCopyBackupSliceKeyBag(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    CFDataRef bskb_as_data = NULL;
    SOSBackupSliceKeyBagRef result = NULL;
    require_quiet(isBackupRing(ring, error), errOut);

    bskb_as_data = SOSRingGetPayload(ring, error);
    require_quiet(bskb_as_data, errOut);

    result = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, bskb_as_data, error);

errOut:
    return result;
}


bool SOSRingPKTrusted(SOSRingRef ring, SecKeyRef pubkey, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    return SOSRingVerify(ring, pubkey, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

bool SOSRingPeerTrusted(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool retval = false;
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(requestor);
    SecKeyRef pubkey = SOSPeerInfoCopyPubKey(pi, error);
    require_quiet(pubkey, exit);
    retval = SOSRingPKTrusted(ring, pubkey, error);
exit:
    CFReleaseNull(pubkey);
    return retval;
}
