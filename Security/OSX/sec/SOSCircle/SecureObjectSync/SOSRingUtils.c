/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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


/*
 * SOSRingUtils.c - Functions for building rings
 */

#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFWrappers.h>

//#include "ckdUtilities.h"

#include <corecrypto/ccder.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>


#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>

#include <stdlib.h>
#include <assert.h>

#include "SOSRing.h"
#include "SOSRingUtils.h"

CFGiblisWithCompareFor(SOSRing);

/* unSignedInformation Dictionary Keys */
CFStringRef sApplicantsKey              = CFSTR("Applicants");
CFStringRef sRejectionsKey              = CFSTR("Rejections");
CFStringRef sLastPeerToModifyKey        = CFSTR("LastModifier");

/* signedInformation Dictionary Keys */
CFStringRef sPeerIDsKey                 = CFSTR("PeerIDs");
CFStringRef sPayloadKey                 = CFSTR("Payload");
CFStringRef sBackupViewSetKey           = CFSTR("BackupViews");
CFStringRef sGenerationKey              = CFSTR("Generation");
CFStringRef sNameKey                    = CFSTR("RingName");
CFStringRef sTypeKey                    = CFSTR("RingType");
CFStringRef sIdentifierKey              = CFSTR("Identifier");
CFStringRef sRingVersionKey              = CFSTR("RingVersion");

#define RINGVERSION 1

SOSRingRef SOSRingAllocate(void) {
    return (SOSRingRef) CFTypeAllocate(SOSRing, struct __OpaqueSOSRing, ALLOCATOR);
}

__unused static bool addValueToDict(CFMutableDictionaryRef thedict, CFStringRef key, CFTypeRef value) {
    if(!value) return false;
    CFDictionaryAddValue(thedict, key, value);
    return true;
}

static bool setValueInDict(CFMutableDictionaryRef thedict, CFStringRef key, CFTypeRef value) {
    if(!value) return false;
    CFDictionarySetValue(thedict, key, value);
    return true;
}

static CFMutableSetRef CFSetCreateMutableForSOSPeerIDs(void) {
    return CFSetCreateMutable(ALLOCATOR, 0, &kCFTypeSetCallBacks);
}


static inline
SOSRingRef SOSRingConvertAndAssertStable(CFTypeRef ringAsType) {
    if (CFGetTypeID(ringAsType) != SOSRingGetTypeID())
        return NULL;

    SOSRingRef ring = (SOSRingRef) ringAsType;

    SOSRingAssertStable(ring);

    return ring;
}

// MARK: Ring Name

CFStringRef SOSRingGetName(SOSRingRef ring) {
    assert(ring);
    assert(ring->signedInformation);
    return asString(CFDictionaryGetValue(ring->signedInformation, sNameKey), NULL);
}

const char *SOSRingGetNameC(SOSRingRef ring) {
    CFStringRef name = asString(SOSRingGetName(ring), NULL);
    if (!name)
        return strdup("");
    return CFStringToCString(name);
}

static inline bool SOSRingSetName(SOSRingRef ring, CFStringRef name) {
    assert(ring);
    assert(ring->signedInformation);
    return setValueInDict(ring->signedInformation, sNameKey, name);
}

// MARK: Ring Type

static bool SOSRingCheckType(SOSRingType type, CFErrorRef *error) {
    if(type < kSOSRingTypeCount) return true;
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Bad Ring Type Specification"), (error != NULL) ? *error : NULL, error);
    return false;
}

uint32_t SOSRingGetType(SOSRingRef ring) {
    uint32_t retval = kSOSRingTypeError; // Error return
    SOSRingAssertStable(ring);
    if(!ring->signedInformation) return retval;
    CFNumberRef ringtype = (CFNumberRef) CFDictionaryGetValue(ring->signedInformation, sTypeKey);
    CFNumberGetValue(ringtype, kCFNumberSInt32Type, &retval);
    return retval;
}

static inline bool SOSRingSetType(SOSRingRef ring, uint32_t ringtype) {
    bool retval = false;
    CFNumberRef cfrtype = NULL;
    SOSRingAssertStable(ring);
    require_action_quiet(SOSRingCheckType(ringtype, NULL), errOut, secnotice("ring", "Bad ring type specification"));
    cfrtype = CFNumberCreate(ALLOCATOR, kCFNumberSInt32Type, &ringtype);
    retval = setValueInDict(ring->signedInformation, sTypeKey, cfrtype);
errOut:
    CFReleaseNull(cfrtype);
    return retval;
}

// MARK: Version

uint32_t SOSRingGetVersion(SOSRingRef ring) {
    uint32_t version = 0;
    assert(ring);
    assert(ring->signedInformation);
    CFNumberRef cfversion = CFDictionaryGetValue(ring->signedInformation, sRingVersionKey);
    require_action_quiet(cfversion, errOut, secnotice("ring", "Could not create version number"));
    CFNumberGetValue(cfversion, kCFNumberSInt32Type, &version);
errOut:
    return version;
}

static inline bool SOSRingSetVersion(SOSRingRef ring) {
    assert(ring);
    assert(ring->signedInformation);
    int32_t thisversion = RINGVERSION;
    CFNumberRef version = CFNumberCreate(ALLOCATOR, kCFNumberSInt32Type, &thisversion);
    require_action_quiet(version, errOut, secnotice("ring", "Could not create version number"));
    CFDictionarySetValue(ring->signedInformation, sRingVersionKey, version);
    CFReleaseNull(version);
    return true;
errOut:
    return false;
}

// MARK: Identifier

CFStringRef SOSRingGetIdentifier(SOSRingRef ring) {
    assert(ring);
    assert(ring->signedInformation);
    return CFDictionaryGetValue(ring->signedInformation, sIdentifierKey);
}

static inline bool SOSRingSetIdentifier(SOSRingRef ring) {
    assert(ring);
    assert(ring->signedInformation);
    bool retval = false;
    CFStringRef identifier = NULL;
    CFUUIDRef uuid = CFUUIDCreate(ALLOCATOR);
    require_action_quiet(uuid, errOut, secnotice("ring", "Could not create ring identifier"));
    identifier = CFUUIDCreateString(ALLOCATOR, uuid);
    CFDictionarySetValue(ring->signedInformation, sIdentifierKey, identifier);
    retval = true;
errOut:
    CFReleaseNull(uuid);
    CFReleaseNull(identifier);
    return retval;
}

// MARK: Ring Identity

bool SOSRingIsSame(SOSRingRef ring1, SOSRingRef ring2) {
    CFStringRef name1 = SOSRingGetName(ring1);
    CFStringRef name2 = SOSRingGetName(ring2);
    require_action_quiet(name1 && name2, errOut, secnotice("ring", "Cannot get both names to consider rings the same"));
    if(CFEqualSafe(name1, name2) != true) return false;

#if 0
    // Not considering this for now - upgraded version can still be the "same ring".
    uint32_t version1 = SOSRingGetVersion(ring1);
    uint32_t version2 = SOSRingGetVersion(ring2);
    require_action_quiet(version1 && version2, errOut, secnotice("ring", "Cannot get both versions to consider rings the same"));
    if(version1 != version2) return false;
#endif

    uint32_t type1 = SOSRingGetType(ring1);
    uint32_t type2 = SOSRingGetVersion(ring2);
    require_action_quiet(type1 && type2, errOut, secnotice("ring", "Cannot get both types to consider rings the same"));
    if(type1 != type2) return false;

    CFStringRef identifier1 = SOSRingGetIdentifier(ring1);
    CFStringRef identifier2 = SOSRingGetIdentifier(ring2);
    require_action_quiet(identifier1 && identifier2, errOut, secnotice("ring", "Cannot get both identifiers to consider rings the same"));
    if(CFEqualSafe(identifier1, identifier2) != true) return false;

    return true;
errOut:
    return false;

}

static Boolean SOSRingCompare(CFTypeRef lhs, CFTypeRef rhs) {
    if (CFGetTypeID(lhs) != SOSRingGetTypeID()
        || CFGetTypeID(rhs) != SOSRingGetTypeID())
        return false;

    SOSRingRef left = SOSRingConvertAndAssertStable(lhs);
    SOSRingRef right = SOSRingConvertAndAssertStable(rhs);

    return NULL != left && NULL != right
    && CFEqualSafe(left->unSignedInformation, right->unSignedInformation)
    && CFEqualSafe(left->signedInformation, right->signedInformation)
    && CFEqualSafe(left->data, right->data)
    && CFEqualSafe(left->signatures, right->signatures);
}


// MARK: Ring Generation Count

SOSGenCountRef SOSRingGetGeneration(SOSRingRef ring) {
    assert(ring);
    assert(ring->signedInformation);
    return CFDictionaryGetValue(ring->signedInformation, sGenerationKey);
}

static inline bool SOSRingSetGeneration(SOSRingRef ring, SOSGenCountRef gen) {
    assert(ring);
    assert(ring->signedInformation);
    return setValueInDict(ring->signedInformation, sGenerationKey, gen);
}

void SOSRingGenerationIncrement(SOSRingRef ring) {
    SOSGenCountRef gen = SOSRingGetGeneration(ring);
    SOSGenCountRef newgen = SOSGenerationIncrementAndCreate(gen);
    SOSRingSetGeneration(ring, newgen);
    CFReleaseNull(newgen);
}

bool SOSRingIsOlderGeneration(SOSRingRef olderRing, SOSRingRef newerRing) {
    SOSGenCountRef old = SOSRingGetGeneration(olderRing);
    SOSGenCountRef new = SOSRingGetGeneration(newerRing);
    return SOSGenerationIsOlder(old, new);
}

void SOSRingGenerationCreateWithBaseline(SOSRingRef newring, SOSRingRef baseline) {
    if(!newring) return;
    SOSGenCountRef gen = SOSGenerationCreateWithBaseline(SOSRingGetGeneration(baseline));
    SOSRingSetGeneration(newring, gen);
    CFReleaseNull(gen);
}

// MARK: Last Modifier
CFStringRef SOSRingGetLastModifier(SOSRingRef ring) {
    assert(ring);
    assert(ring->unSignedInformation);
    return CFDictionaryGetValue(ring->unSignedInformation, sLastPeerToModifyKey);
}

bool SOSRingSetLastModifier(SOSRingRef ring, CFStringRef peerID) {
    assert(ring);
    assert(ring->unSignedInformation);
    return setValueInDict(ring->unSignedInformation, sLastPeerToModifyKey, peerID);
}


// MARK: Ring Applicants

CFMutableSetRef SOSRingGetApplicants(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return (CFMutableSetRef) CFDictionaryGetValue(ring->unSignedInformation, sApplicantsKey);
}

bool SOSRingSetApplicants(SOSRingRef ring, CFMutableSetRef applicants) {
    SOSRingAssertStable(ring);
    return setValueInDict(ring->unSignedInformation, sApplicantsKey, applicants);
}

int SOSRingCountApplicants(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return (int)CFSetGetCount(SOSRingGetApplicants(ring));
}

bool SOSRingHasApplicant(SOSRingRef ring, CFStringRef peerID) {
    SOSRingAssertStable(ring);
    return CFSetContainsValue(SOSRingGetApplicants(ring), peerID);
}

CFMutableSetRef SOSRingCopyApplicants(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    CFSetRef applicants = SOSRingGetApplicants(ring);
    return CFSetCreateMutableCopy(ALLOCATOR, 0, applicants);
}

bool SOSRingAddApplicant(SOSRingRef ring, CFStringRef peerid) {
    CFMutableSetRef applicants = SOSRingGetApplicants(ring);
    CFSetAddValue(applicants, peerid);
    return true;
}

bool SOSRingRemoveApplicant(SOSRingRef ring, CFStringRef peerid) {
    CFMutableSetRef applicants = SOSRingGetApplicants(ring);
    CFSetRemoveValue(applicants, peerid);
    return true;
}

// MARK: Ring Rejections

static inline CFMutableSetRef SOSRingGetRejections(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return (CFMutableSetRef) CFDictionaryGetValue(ring->unSignedInformation, sRejectionsKey);
}

static inline bool SOSRingSetRejections(SOSRingRef ring, CFMutableSetRef rejections) {
    SOSRingAssertStable(ring);
    return setValueInDict(ring->unSignedInformation, sRejectionsKey, rejections);
}

int SOSRingCountRejections(SOSRingRef ring) {
    CFSetRef rejects = SOSRingGetRejections(ring);
    return (int)CFSetGetCount(rejects);
}

bool SOSRingHasRejection(SOSRingRef ring, CFStringRef peerID) {
    SOSRingAssertStable(ring);
    return CFSetContainsValue(SOSRingGetRejections(ring), peerID);
}

CFMutableSetRef SOSRingCopyRejections(SOSRingRef ring) {
    CFSetRef rejects = SOSRingGetRejections(ring);
    return CFSetCreateMutableCopy(ALLOCATOR, 0, rejects);
}


bool SOSRingAddRejection(SOSRingRef ring, CFStringRef peerid) {
    CFMutableSetRef rejects = SOSRingGetRejections(ring);
    CFSetAddValue(rejects, peerid);
    return true;
}

bool SOSRingRemoveRejection(SOSRingRef ring, CFStringRef peerid) {
    CFMutableSetRef rejects = SOSRingGetRejections(ring);
    CFSetRemoveValue(rejects, peerid);
    return true;
}

// MARK: Ring Payload

CFDataRef SOSRingGetPayload_Internal(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return (CFDataRef) CFDictionaryGetValue(ring->signedInformation, sPayloadKey);
}

bool SOSRingSetPayload_Internal(SOSRingRef ring, CFDataRef payload) {
    SOSRingAssertStable(ring);
    return setValueInDict(ring->signedInformation, sPayloadKey, payload);
}

// MARK: Ring Backup Viewset


CFSetRef SOSRingGetBackupViewset_Internal(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return asSet(CFDictionaryGetValue(ring->signedInformation, sBackupViewSetKey), NULL);
}

bool SOSRingSetBackupViewset_Internal(SOSRingRef ring, CFSetRef viewSet) {
    SOSRingAssertStable(ring);
    return setValueInDict(ring->signedInformation, sBackupViewSetKey, viewSet);
}



// MARK: Ring PeerIDs

static inline CFMutableSetRef SOSRingGetPeerIDs(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return (CFMutableSetRef) asSet(CFDictionaryGetValue(ring->signedInformation, sPeerIDsKey), NULL);
}

bool SOSRingSetPeerIDs(SOSRingRef ring, CFMutableSetRef peers) {
    SOSRingAssertStable(ring);
    return setValueInDict(ring->signedInformation, sPeerIDsKey, peers);
}

int SOSRingCountPeerIDs(SOSRingRef ring) {
    CFSetRef peerIDs = SOSRingGetPeerIDs(ring);
    return (int)CFSetGetCount(peerIDs);
}


bool SOSRingHasPeerID(SOSRingRef ring, CFStringRef peerID) {
    SOSRingAssertStable(ring);
    return CFSetContainsValue(SOSRingGetPeerIDs(ring), peerID);
}

CFMutableSetRef SOSRingCopyPeerIDs(SOSRingRef ring) {
    CFSetRef peerIDs = SOSRingGetPeerIDs(ring);
    return CFSetCreateMutableCopy(ALLOCATOR, 0, peerIDs);
}

void SOSRingAddAll(SOSRingRef ring, CFSetRef peerInfosOrIDs) {
    CFSetForEach(peerInfosOrIDs, ^(const void *value) {
        CFStringRef peerID = value;

        if (isSOSPeerInfo(value))
            peerID = SOSPeerInfoGetPeerID((SOSPeerInfoRef)value);

        if (isString(peerID))
            SOSRingAddPeerID(ring, peerID);
    });
}

bool SOSRingAddPeerID(SOSRingRef ring, CFStringRef peerid) {
    CFMutableSetRef peerIDs = SOSRingGetPeerIDs(ring);
    CFSetAddValue(peerIDs, peerid);
    return true;
}

bool SOSRingRemovePeerID(SOSRingRef ring, CFStringRef peerid) {
    CFMutableSetRef peerIDs = SOSRingGetPeerIDs(ring);
    CFSetRemoveValue(peerIDs, peerid);
    return true;
}

void SOSRingForEachPeerID(SOSRingRef ring, void (^action)(CFStringRef peerID)) {
    SOSRingAssertStable(ring);
    CFMutableSetRef peerIDs = SOSRingGetPeerIDs(ring);
    if(!peerIDs) return;
    CFSetForEach(peerIDs, ^(const void*value) {
        CFStringRef peerID = (CFStringRef) value;
            action(peerID);
    });
}

// MARK: SOSRing Ops

SOSRingRef SOSRingCreate_Internal(CFStringRef name, SOSRingType type, CFErrorRef *error) {
    SOSRingRef r = SOSRingAllocate();
    SOSGenCountRef gen = SOSGenerationCreate();

    require_action_quiet(name, errout0,
        SOSCreateError(kSOSErrorNoCircleName, CFSTR("No ring name"), NULL, error));
    require_action_quiet(SOSRingCheckType(type, error), errout0,
        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Unknown ring type"), NULL, error));
    require_action_quiet((r->unSignedInformation = CFDictionaryCreateMutableForCFTypes(ALLOCATOR)), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate unsigned information area"), NULL, error));
    require_action_quiet((r->signedInformation = CFDictionaryCreateMutableForCFTypes(ALLOCATOR)), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate signed information area"), NULL, error));
    require_action_quiet((r->signatures = CFDictionaryCreateMutableForCFTypes(ALLOCATOR)), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate signature area"), NULL, error));
    require_action_quiet((r->data = CFDictionaryCreateMutableForCFTypes(ALLOCATOR)), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate data area"), NULL, error));

    require_action_quiet(SOSRingSetName(r, name), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate ring name area"), NULL, error));
    require_action_quiet(SOSRingSetType(r, type), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate ring type"), NULL, error));
    require_action_quiet(SOSRingSetVersion(r), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate ring version"), NULL, error));
    require_action_quiet(SOSRingSetIdentifier(r), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate ring identifier"), NULL, error));
    
    CFMutableSetRef peerIDs = CFSetCreateMutableForSOSPeerIDs();
    require_action_quiet(SOSRingSetApplicants(r, peerIDs), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate applicant area"), NULL, error));
    CFReleaseNull(peerIDs);
    
    CFMutableSetRef rejectedIDs = CFSetCreateMutableForSOSPeerIDs();
    require_action_quiet(SOSRingSetRejections(r, rejectedIDs), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate rejection area"), NULL, error));
    CFReleaseNull(rejectedIDs);
    
    require_action_quiet(SOSRingSetGeneration(r, gen), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate generation count"), NULL, error));
    
    peerIDs = CFSetCreateMutableForSOSPeerIDs();
    require_action_quiet(SOSRingSetPeerIDs(r, peerIDs), errout,
        SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Failed to allocate PeerID"), NULL, error));
    CFReleaseNull(gen);
    CFReleaseNull(peerIDs);
    
    return r;
errout:
    CFReleaseNull(r->unSignedInformation);
    CFReleaseNull(r->signedInformation);
    CFReleaseNull(r->signatures);
    CFReleaseNull(r->data);
errout0:
    CFReleaseNull(gen);
    CFReleaseNull(r);
    return NULL;
}


static void SOSRingDestroy(CFTypeRef aObj) {
    SOSRingRef c = (SOSRingRef) aObj;

    CFReleaseNull(c->unSignedInformation);
    CFReleaseNull(c->signedInformation);
    CFReleaseNull(c->data);
    CFReleaseNull(c->signatures);
}


SOSRingRef SOSRingCopyRing(SOSRingRef original, CFErrorRef *error) {
    SOSRingRef r = CFTypeAllocate(SOSRing, struct __OpaqueSOSRing, ALLOCATOR);

    assert(original);
    r->unSignedInformation = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, original->unSignedInformation);
    r->signedInformation = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, original->signedInformation);
    r->signatures = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, original->signatures);
    r->data = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, original->data);

    return r;
}

bool SOSRingIsEmpty_Internal(SOSRingRef ring) {
    return CFSetGetCount(SOSRingGetPeerIDs(ring)) == 0;
}

bool SOSRingIsOffering_Internal(SOSRingRef ring) {
    return SOSRingCountPeers(ring) == 1;
}

bool SOSRingResetToEmpty_Internal(SOSRingRef ring, CFErrorRef *error) {
    SOSGenCountRef gen = NULL;
    CFSetRemoveAllValues(SOSRingGetApplicants(ring));
    CFSetRemoveAllValues(SOSRingGetRejections(ring));
    CFSetRemoveAllValues(SOSRingGetPeerIDs(ring));
    CFDictionaryRemoveAllValues(ring->signatures);
    SOSRingSetGeneration(ring, gen = SOSGenerationCreate());
    CFReleaseNull(gen);
    return true;
}

// MARK: PeerIDs in Ring

#if 0
static inline bool isHiddenPeer(SOSPeerInfoRef peer) {
    return SOSPeerInfoIsRetirementTicket(peer) || SOSPeerInfoIsCloudIdentity(peer);
}
#endif

int SOSRingCountPeers(SOSRingRef ring) {
    SOSRingAssertStable(ring);
    return (int) CFSetGetCount(SOSRingGetPeerIDs(ring));
}


bool SOSRingHasPeerWithID(SOSRingRef ring, CFStringRef peerid, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    return CFSetContainsValue(SOSRingGetPeerIDs(ring), peerid);
}

// MARK: Ring Signatures


static inline CFDictionaryRef SOSRingGetSignatures(SOSRingRef ring) {
    return ring->signatures;
}

static inline CFDataRef SOSRingGetSignatureForPeerID(SOSRingRef ring, CFStringRef peerID) {
    if(!ring || !peerID) return NULL;
    CFDataRef result = NULL;
    CFTypeRef value = (CFDataRef)CFDictionaryGetValue(SOSRingGetSignatures(ring), peerID);
    if (isData(value)) result = (CFDataRef) value;
    return result;
}

static CFDataRef SOSRingCreateHash(const struct ccdigest_info *di, SOSRingRef ring, CFErrorRef *error) {
    uint8_t hash_result[di->output_size];

    size_t dersize = der_sizeof_plist(ring->signedInformation, error);
    if(dersize == 0) {
        return false;
    }
    uint8_t der[dersize];
    der_encode_plist(ring->signedInformation, error, der, der+dersize);

    ccdigest(di, dersize, der, hash_result);
    return CFDataCreate(NULL, hash_result, di->output_size);
}

static bool SOSRingSetSignature(SOSRingRef ring, SecKeyRef privKey, CFDataRef signature, CFErrorRef *error) {
    bool result = false;
    SecKeyRef pubkey = SecKeyCreatePublicFromPrivate(privKey);
    CFStringRef pubKeyID = SOSCopyIDOfKey(pubkey, error);
    require_quiet(pubKeyID, fail);
    CFDictionarySetValue(ring->signatures, pubKeyID, signature);
    result = true;
fail:
    CFReleaseSafe(pubkey);
    CFReleaseSafe(pubKeyID);
    return result;
}

static bool SOSRingRemoveSignatures(SOSRingRef ring, CFErrorRef *error) {
    CFDictionaryRemoveAllValues(ring->signatures);
    return true;
}

static CFDataRef SOSCopySignedHash(SecKeyRef privKey, CFDataRef hash, CFErrorRef *error) {
    size_t siglen = SecKeyGetSize(privKey, kSecKeySignatureSize)+16;
    uint8_t sig[siglen];
    OSStatus stat =  SecKeyRawSign(privKey, kSecPaddingNone, CFDataGetBytePtr(hash), CFDataGetLength(hash), sig, &siglen);
    if(stat) {
        return NULL;
    }
    return CFDataCreate(NULL, sig, siglen);
}

static bool SOSRingSign(SOSRingRef ring, SecKeyRef privKey, CFErrorRef *error) {
    if (!ring || !privKey) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("SOSRingSign Lacking ring or private key"),
            (error != NULL) ? *error : NULL, error);
        return false;
    }
    const struct ccdigest_info *di = ccsha256_di();
    CFDataRef hash = SOSRingCreateHash(di, ring, error);
    CFDataRef signature = SOSCopySignedHash(privKey, hash, error);
    SOSRingSetSignature(ring, privKey, signature, error);
    CFRelease(signature);
    CFReleaseNull(hash);
    return true;
}

bool SOSRingVerifySignatureExists(SOSRingRef ring, SecKeyRef pubKey, CFErrorRef *error) {
    CFStringRef pubKeyID = SOSCopyIDOfKey(pubKey, error);
    CFDataRef signature = SOSRingGetSignatureForPeerID(ring, pubKeyID);
    CFReleaseNull(pubKeyID);
    return NULL != signature;
}

bool SOSRingVerify(SOSRingRef ring, SecKeyRef pubKey, CFErrorRef *error) {
    CFStringRef pubKeyID = SOSCopyIDOfKey(pubKey, error);
    CFDataRef signature = SOSRingGetSignatureForPeerID(ring, pubKeyID);
    CFReleaseNull(pubKeyID);
    if(!signature) return false;
    CFDataRef hash = SOSRingCreateHash(ccsha256_di(), ring, error);
    bool success = SecKeyRawVerify(pubKey, kSecPaddingNone, CFDataGetBytePtr(hash), CFDataGetLength(hash),
                           CFDataGetBytePtr(signature), CFDataGetLength(signature)) == errSecSuccess;
    CFReleaseNull(hash);
    return success;
}

bool SOSRingVerifyPeerSigned(SOSRingRef ring, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool result = false;
    SecKeyRef pubkey = SOSPeerInfoCopyPubKey(peer, error);
    require_quiet(pubkey, fail);

    result = SOSRingVerify(ring, pubkey, error);

fail:
    CFReleaseSafe(pubkey);
    return result;
}

static bool SOSRingEnsureRingConsistency(SOSRingRef ring, CFErrorRef *error) {
    secnotice("Development", "SOSRingEnsureRingConsistency requires ring membership and generation count consistency check", NULL);
    return true;
}

bool SOSRingGenerationSign_Internal(SOSRingRef ring, SecKeyRef privKey, CFErrorRef *error) {
    if(!privKey || !ring) return false;
    bool retval = false;
    SOSRingGenerationIncrement(ring);
    require_quiet(SOSRingEnsureRingConsistency(ring, error), fail);
    require_quiet(SOSRingRemoveSignatures(ring, error), fail);
    require_quiet(SOSRingSign(ring, privKey, error), fail);
    retval = true;
fail:
    return retval;
}

// MARK: Concordance

bool SOSRingConcordanceSign_Internal(SOSRingRef ring, SecKeyRef privKey, CFErrorRef *error) {
    if(!privKey || !ring) return false;
    bool retval = false;
    require_quiet(SOSRingSign(ring, privKey, error), fail);
    retval = true;
fail:
    return retval;
}



// MARK: Debugging

static inline void CFSetForEachPeerID(CFSetRef set, void (^operation)(CFStringRef peerID)) {
    CFSetForEach(set, ^(const void *value) {
        CFStringRef peerID = (CFStringRef) value;
        operation(peerID);
    });
}

static CFStringRef CreateCommaSeparatedPeerIDs(CFSetRef peers) {
    CFMutableStringRef result = CFStringCreateMutable(kCFAllocatorDefault, 0);

    __block bool addSeparator = false;

    CFSetForEachPeerID(peers, ^(CFStringRef peerID) {
        if (addSeparator) {
            CFStringAppendCString(result, ", ", kCFStringEncodingUTF8);
        }
        CFStringAppend(result, peerID);

        addSeparator = true;
    });

    return result;
}

CFDictionaryRef SOSRingCopyPeerIDList(SOSRingRef ring) {
    CFStringRef peerIDS = CreateCommaSeparatedPeerIDs(SOSRingGetPeerIDs(ring));
    CFStringRef applicantIDs = CreateCommaSeparatedPeerIDs(SOSRingGetApplicants(ring));
    CFStringRef rejectIDs = CreateCommaSeparatedPeerIDs(SOSRingGetRejections(ring));
    CFDictionaryRef list = CFDictionaryCreateForCFTypes(ALLOCATOR,
                                        CFSTR("MEMBER"), peerIDS,
                                        CFSTR("APPLICANTS"), applicantIDs,
                                        CFSTR("REJECTS"), rejectIDs,
                                        NULL);
    
    CFReleaseNull(peerIDS);
    CFReleaseNull(applicantIDs);
    CFReleaseNull(rejectIDs);
    return list;
}

 CFStringRef SOSRingCopySignerList(SOSRingRef ring) {
    __block bool addSeparator = false;
   CFMutableStringRef signers = CFStringCreateMutable(ALLOCATOR, 0);
    CFDictionaryForEach(ring->signatures, ^(const void *key, const void *value) {
        CFStringRef peerID = (CFStringRef) key;
        if (addSeparator)
            CFStringAppendCString(signers, ", ", kCFStringEncodingUTF8);
        CFStringAppend(signers, peerID);
        addSeparator = true;
    });
    return signers;
}

static CFStringRef SOSRingCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOpts) {
    SOSRingRef ring = (SOSRingRef) aObj;

    SOSRingAssertStable(ring);

    CFDictionaryRef peers = SOSRingCopyPeerIDList(ring);
    CFStringRef signers = SOSRingCopySignerList(ring);

    CFDataRef payload = SOSRingGetPayload(ring, NULL);

    CFStringRef gcString = SOSGenerationCountCopyDescription(SOSRingGetGeneration(ring));

    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);

    CFStringAppendFormat(description, formatOpts, CFSTR("<SOSRing@%p: '%@', Version %u, "), ring, SOSRingGetName(ring), SOSRingGetVersion(ring));
    CFStringAppendFormat(description, formatOpts, CFSTR("UUID: %@, "), SOSRingGetIdentifier(ring));
    SOSGenerationCountWithDescription(SOSRingGetGeneration(ring), ^(CFStringRef gcString) {
        CFStringAppendFormat(description, formatOpts, CFSTR("Gen: %@, "), gcString);
    });
    CFStringAppendFormat(description, formatOpts, CFSTR("Mod: %@, "), SOSRingGetLastModifier(ring));
    
    CFStringAppendFormat(description, formatOpts, CFSTR("D: %ld "), payload ? CFDataGetLength(payload) : 0);

    SOSBackupSliceKeyBagRef payloadAsBSKB = SOSRingCopyBackupSliceKeyBag(ring, NULL);

    if (payloadAsBSKB) {
        CFStringAppendFormat(description, formatOpts, CFSTR("%@ "), payloadAsBSKB);
    }

    CFReleaseSafe(payloadAsBSKB);

    CFStringAppendFormat(description, formatOpts, CFSTR("P: [%@], "), CFDictionaryGetValue(peers, CFSTR("MEMBER")));
    CFStringAppendFormat(description, formatOpts, CFSTR("A: [%@], "), CFDictionaryGetValue(peers, CFSTR("APPLICANTS")));
    CFStringAppendFormat(description, formatOpts, CFSTR("R: [%@], "), CFDictionaryGetValue(peers, CFSTR("REJECTS")));
    CFStringAppendFormat(description, formatOpts, CFSTR("S: [%@]>"), signers);

    CFReleaseNull(gcString);
    CFReleaseNull(peers);
    CFReleaseNull(signers);
    CFReleaseNull(peers);
    
    return description;
}

//
// Peer Retirement
//

#define SIGLEN 128
#if 0
static CFDataRef sosSignHash(SecKeyRef privkey, const struct ccdigest_info *di, uint8_t *hbuf) {
    OSStatus stat;
    size_t siglen = SIGLEN;
    uint8_t sig[siglen];
    if((stat = SecKeyRawSign(privkey, kSecPaddingNone, hbuf, di->output_size, sig, &siglen)) != 0) {
        return NULL;
    }
    return CFDataCreate(NULL, sig, (CFIndex)siglen);
}
#endif
#if 0
static void WithBufferSpace(size_t space, void (^action)(uint8_t *buffer, size_t length)) {
    if (space == 0) {
        action(NULL, space);
    } else if (space <= 2048) {
        uint8_t buffer[space];

        action(buffer, space);
    } else {
        uint8_t* buffer = malloc(space);

        action(buffer, space);

        free(buffer);
    }
}

static CFDataRef CFDictionaryHashCreate(CFDictionaryRef dict, CFErrorRef *error) {

    __block CFDataRef result = NULL;

    require_quiet(dict, errOut);

    WithBufferSpace(der_sizeof_dictionary(dict, error), ^(uint8_t *der, size_t len) {
        if (len > 0) {
            const struct ccdigest_info *di = ccsha256_di();
            uint8_t hash_result[di->output_size];
            der_encode_dictionary(dict, error, der, der+len);

            ccdigest(di, len, der, hash_result);
            result = CFDataCreate(ALLOCATOR, hash_result, di->output_size);
        }
    });

errOut:
    return NULL;
}
#endif
/*
 CFDictionary:
    signatures: CFDictionary of key = hash(pubkey), value = signature(privkey, (DER(payload))
    payload: CFDictionary passed in


 */
#if 0
static CFStringRef sPayload = CFSTR("payload");
static CFStringRef sSignature = CFSTR("signature");

static bool SOSCFSignedDictionarySetSignature(SecKeyRef priv, CFDictionaryRef sd, CFErrorRef *error) {
    CFDictionaryRef payload = CFDictionaryGetValue(sd, sPayload);
    CFMutableDictionaryRef signatures = (CFMutableDictionaryRef) CFDictionaryGetValue(sd, sSignature);
    CFDataRef hash = CFDictionaryHashCreate(payload, error);
    CFDataRef signature = SOSHashSign(priv, hash, error);
    CFReleaseNull(hash);
    CFStringRef pubhash = SOSCopyIDOfKey(priv, error);
    require_quiet(signature && pubhash, errOut);
    CFDictionaryAddValue(signatures, pubhash, signature);
    return true;
errOut:
    return false;
}
#endif
#if 0
static CFDictionaryRef SOSCFSignedDictionaryCreate(SecKeyRef priv, CFDictionaryRef payload, CFErrorRef *error) {
    CFMutableDictionaryRef signatures = CFDictionaryCreateMutableForCFTypes(ALLOCATOR);
    CFMutableDictionaryRef retval = CFDictionaryCreateMutableForCFTypes(ALLOCATOR);
    require_quiet(signatures && retval, errOut);

    CFDictionaryAddValue(retval, sSignature, signatures);
    CFDictionaryAddValue(retval, sPayload, payload);
    SOSCFSignedDictionarySetSignature(priv, retval, error);
    return retval;
errOut:
    CFReleaseNull(signatures);
    CFReleaseNull(retval);
    return NULL;
}
#endif

#if 0
CFDictionaryRef SOSRingCreateRetirementTicket(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    CFDictionaryRef retval = NULL;
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(fpi));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(fpi, error);
    CFDataRef resignationDate = SOSDateCreate();
    GENCOUNT!!!

    CFDataRef sig = SOSDERAndSignStuff(priv, keys, values, 2, error);
    retval = CFDictionaryCreate(ALLOCATOR, <#const void **keys#>, <#const void **values#>, <#CFIndex numValues#>, <#const CFDictionaryKeyCallBacks *keyCallBacks#>, <#const CFDictionaryValueCallBacks *valueCallBacks#>)
    return pi;

exit_stage_right:
    CFReleaseNull(priv);
    CFReleaseNull(resignationDate);
    return retval;
}
#endif
