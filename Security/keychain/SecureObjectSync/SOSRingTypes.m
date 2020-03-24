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
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <AssertMacros.h>

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
    if(!SOSRingValidType(type)){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return NULL;
    }
    if(!(ringTypes[type]->sosRingCreate)){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return NULL;
    }
    return ringTypes[type]->sosRingCreate(name, myPeerID, error);
}

bool SOSRingResetToEmpty(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!SOSRingValidType(type)){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    if(!ringTypes[type]->sosRingResetToEmpty){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    return ringTypes[type]->sosRingResetToEmpty(ring, myPeerID, error);
}

bool SOSRingGenerationSign(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!(SOSRingValidType(type))){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    if(!(ringTypes[type]->sosRingGenerationSign)){
        return true;
    }
    return ringTypes[type]->sosRingGenerationSign(ring, user_privkey, requestor, error);
}

bool SOSRingConcordanceSign(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!(SOSRingValidType(type))){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    if(!(ringTypes[type]->sosRingConcordanceSign)){
        return true;
    }
    return ringTypes[type]->sosRingConcordanceSign(ring, requestor, error);
}

SOSConcordanceStatus SOSRingConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers,
                                             SOSRingRef knownRing, SOSRingRef proposedRing,
                                             SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                             CFStringRef excludePeerID, CFErrorRef *error) {
    SOSRingAssertStable(knownRing);
    SOSRingAssertStable(proposedRing);
    SOSRingType type1 = SOSRingGetType(knownRing);
    SOSRingType type2 = SOSRingGetType(proposedRing);
    if(!(SOSRingValidType(type1))){
        return kSOSConcordanceError;
    }
    if(!(SOSRingValidType(type2))){
        return kSOSConcordanceError;
    }
    if((type1 != type2)){
        return kSOSConcordanceError;
    }

    secnotice("ring", "concordance trust (%s)", ringTypes[type1]->typeName);
    secnotice("ring", "    knownRing: %@", knownRing);
    secnotice("ring", " proposedRing: %@", proposedRing);
    CFStringRef knownKeyID = SOSCopyIDOfKeyWithLength(knownPubkey, 8, NULL);
    CFStringRef userKeyID = SOSCopyIDOfKeyWithLength(userPubkey, 8, NULL);
    CFStringRef mypeerSPID = CFStringCreateTruncatedCopy(excludePeerID, 8);
    
    secnotice("ring", "knownkey: %@ userkey: %@ myPeerID: %@", knownKeyID, userKeyID, mypeerSPID);
    CFReleaseNull(knownKeyID);
    CFReleaseNull(userKeyID);
    CFReleaseNull(mypeerSPID);

    if(!(ringTypes[type1]->sosRingConcordanceTrust)){
        return kSOSConcordanceError;
    }
    return ringTypes[type1]->sosRingConcordanceTrust(me, peers, knownRing, proposedRing, knownPubkey, userPubkey, excludePeerID, error);
}

bool SOSRingAccept(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!(SOSRingValidType(type))){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    if(!(ringTypes[type]->sosRingAccept)){
         return true;
    }
    return ringTypes[type]->sosRingAccept(ring, user_privkey, requestor, error);
}

bool SOSRingReject(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!(SOSRingValidType(type))){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    if(!(ringTypes[type]->sosRingReject)){
        return true;
    }
    return ringTypes[type]->sosRingReject(ring, user_privkey, requestor, error);
}

bool SOSRingSetPayload(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!(SOSRingValidType(type))){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    if(!(ringTypes[type]->sosRingSetPayload)){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    }
    return ringTypes[type]->sosRingSetPayload(ring, user_privkey, payload, requestor, error);
}

CFDataRef SOSRingGetPayload(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!SOSRingValidType(type)){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return NULL;
    }
    if(!ringTypes[type]->sosRingGetPayload){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return NULL;
    };
    return ringTypes[type]->sosRingGetPayload(ring, error);
}

CFSetRef SOSRingGetBackupViewset(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(kSOSRingBackup != type){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not backup ring type"), NULL, error);
        return NULL;
    }
    return SOSRingGetBackupViewset_Internal(ring);
}

// This returns one string with the view for the ring - we never used multi-view rings
CFStringRef SOSRingGetBackupView(SOSRingRef ring, CFErrorRef *error) {
    __block CFStringRef result = NULL;
    CFSetRef allTheViews = SOSRingGetBackupViewset(ring, error);
    if(allTheViews) {
        if(CFSetGetCount(allTheViews) == 1) {
            CFSetForEach(allTheViews, ^(const void *value) {
                result = asString(value, error);
            });
        }
    } else {
        SOSCreateError(kSOSErrorParam, CFSTR("Wrong set count for one return"), NULL, error);
    }
    return result;
}

static bool isBackupRing(SOSRingRef ring, CFErrorRef *error) {
    SOSRingType type = SOSRingGetType(ring);
    if(kSOSRingBackup != type){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not backup ring type"), NULL, error);
        return false;
    }
    return true;
}

bool SOSRingSetBackupKeyBag(SOSRingRef ring, SOSFullPeerInfoRef fpi, CFSetRef viewSet, SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    CFDataRef bskb_as_data = NULL;
    bool result = false;
    if(!isBackupRing(ring, error)){
        CFReleaseNull(bskb_as_data);
        return result;
    }

    bskb_as_data = SOSBSKBCopyEncoded(bskb, error);
    result = bskb_as_data &&
             SOSRingSetBackupViewset_Internal(ring, viewSet) &&
             SOSRingSetPayload(ring, NULL, bskb_as_data, fpi, error);
    CFReleaseNull(bskb_as_data);
    return result;
}

SOSBackupSliceKeyBagRef SOSRingCopyBackupSliceKeyBag(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    CFDataRef bskb_as_data = NULL;
    SOSBackupSliceKeyBagRef result = NULL;
    if(!isBackupRing(ring, error)){
        return result;
    }

    bskb_as_data = SOSRingGetPayload(ring, error);
    if(!bskb_as_data){
        return result;
    }
    result = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, bskb_as_data, error);
    return result;
}

bool SOSRingPKTrusted(SOSRingRef ring, SecKeyRef pubkey, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    if(!(SOSRingValidType(type))){
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
        return false;
    };
    return SOSRingVerify(ring, pubkey, error);
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
