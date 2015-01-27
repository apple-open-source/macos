/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
 * SOSCircle.c -  Implementation of the secure object syncing transport
 */

#include <AssertMacros.h>

#include <CoreFoundation/CFArray.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSPeerInfoCollections.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

#include <utilities/SecCFWrappers.h>
//#include "ckdUtilities.h"

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>

#include <corecrypto/ccder.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>

#include <stdlib.h>
#include <assert.h>

enum {
    kOnlyCompatibleVersion = 1, // Sometime in the future this name will be improved to reflect history.
    
    kAlwaysIncompatibleVersion = UINT64_MAX,
};

struct __OpaqueSOSCircle {
    CFRuntimeBase _base;
    
    CFStringRef name;
    CFNumberRef generation;
    CFMutableSetRef peers;
    CFMutableSetRef applicants;
    CFMutableSetRef rejected_applicants;

    CFMutableDictionaryRef signatures;
};

CFGiblisWithCompareFor(SOSCircle);

SOSCircleRef SOSCircleCreate(CFAllocatorRef allocator, CFStringRef name, CFErrorRef *error) {
    SOSCircleRef c = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);
    int64_t gen = 1;

    assert(name);
    
    c->name = CFStringCreateCopy(allocator, name);
    c->generation = CFNumberCreate(allocator, kCFNumberSInt64Type, &gen);
    c->peers = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    c->applicants = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    c->rejected_applicants = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    c->signatures = CFDictionaryCreateMutableForCFTypes(allocator);

    return c;
}

static CFNumberRef SOSCircleGenerationCopy(CFNumberRef generation) {
    int64_t value;
    CFAllocatorRef allocator = CFGetAllocator(generation);
    CFNumberGetValue(generation, kCFNumberSInt64Type, &value);
    return CFNumberCreate(allocator, kCFNumberSInt64Type, &value);
}

static CFMutableSetRef CFSetOfPeerInfoDeepCopy(CFAllocatorRef allocator, CFSetRef peerInfoSet)
{
   __block CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    CFSetForEach(peerInfoSet, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        CFErrorRef localError = NULL;
        SOSPeerInfoRef copiedPeer = SOSPeerInfoCreateCopy(allocator, pi, &localError);
        if (copiedPeer) {
            CFSetAddValue(result, copiedPeer);
        } else {
            secerror("Failed to copy peer: %@ (%@)", pi, localError);
        }
        CFReleaseSafe(copiedPeer);
        CFReleaseSafe(localError);
    });
    return result;
}

SOSCircleRef SOSCircleCopyCircle(CFAllocatorRef allocator, SOSCircleRef otherCircle, CFErrorRef *error)
{
    SOSCircleRef c = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);

    assert(otherCircle);
    c->name = CFStringCreateCopy(allocator, otherCircle->name);
    c->generation = SOSCircleGenerationCopy(otherCircle->generation);

    c->peers = CFSetOfPeerInfoDeepCopy(allocator, otherCircle->peers);
    c->applicants = CFSetOfPeerInfoDeepCopy(allocator, otherCircle->applicants);
    c->rejected_applicants = CFSetOfPeerInfoDeepCopy(allocator, otherCircle->rejected_applicants);

    c->signatures = CFDictionaryCreateMutableCopy(allocator, 0, otherCircle->signatures);
    
    return c;
}

static inline
void SOSCircleAssertStable(SOSCircleRef circle)
{
    assert(circle);
    assert(circle->name);
    assert(circle->generation);
    assert(circle->peers);
    assert(circle->applicants);
    assert(circle->rejected_applicants);
    assert(circle->signatures);
}

static inline
SOSCircleRef SOSCircleConvertAndAssertStable(CFTypeRef circleAsType)
{
    if (CFGetTypeID(circleAsType) != SOSCircleGetTypeID())
        return NULL;

    SOSCircleRef circle = (SOSCircleRef) circleAsType;

    SOSCircleAssertStable(circle);

    return circle;
}

static Boolean SOSCircleCompare(CFTypeRef lhs, CFTypeRef rhs) {
    if (CFGetTypeID(lhs) != SOSCircleGetTypeID()
     || CFGetTypeID(rhs) != SOSCircleGetTypeID())
        return false;

    SOSCircleRef left = SOSCircleConvertAndAssertStable(lhs);
    SOSCircleRef right = SOSCircleConvertAndAssertStable(rhs);

    // TODO: we should be doing set equality for peers and applicants.
    return NULL != left && NULL != right
        && CFEqualSafe(left->generation, right->generation)
        && SOSPeerInfoSetContainsIdenticalPeers(left->peers, right->peers)
        && SOSPeerInfoSetContainsIdenticalPeers(left->applicants, right->applicants)
        && SOSPeerInfoSetContainsIdenticalPeers(left->rejected_applicants, right->rejected_applicants)
        && CFEqualSafe(left->signatures, right->signatures);
}

static CFMutableArrayRef CFSetCopyValuesCFArray(CFSetRef set)
{
    CFIndex count = CFSetGetCount(set);

    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    if (count > 0) {
        const void * values[count];
        CFSetGetValues(set, values);
        for (int current = 0; current < count; ++current) {
            CFArrayAppendValue(result, values[current]);
        }
    }
    
    return result;
}

static bool SOSCircleDigestArray(const struct ccdigest_info *di, CFMutableArrayRef array, void *hash_result, CFErrorRef *error)
{
    __block bool success = true;
    ccdigest_di_decl(di, array_digest);
    const void * a_digest = array_digest;

    ccdigest_init(di, array_digest);
    CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)), SOSPeerInfoCompareByID, SOSPeerCmpPubKeyHash);
    CFArrayForEach(array, ^(const void *peer) {
        if (!SOSPeerInfoUpdateDigestWithPublicKeyBytes((SOSPeerInfoRef)peer, di, a_digest, error))
            success = false;
    });
    ccdigest_final(di, array_digest, hash_result);

    return success;
}

static bool SOSCircleDigestSet(const struct ccdigest_info *di, CFMutableSetRef set, void *hash_result, CFErrorRef *error)
{
    CFMutableArrayRef values = CFSetCopyValuesCFArray(set);
    
    bool result = SOSCircleDigestArray(di, values, hash_result, error);
    
    CFReleaseSafe(values);
    
    return result;
}


static bool SOSCircleHash(const struct ccdigest_info *di, SOSCircleRef circle, void *hash_result, CFErrorRef *error) {
    ccdigest_di_decl(di, circle_digest);
    ccdigest_init(di, circle_digest);
    int64_t gen = SOSCircleGetGenerationSint(circle);
    ccdigest_update(di, circle_digest, sizeof(gen), &gen);
    
    SOSCircleDigestSet(di, circle->peers, hash_result, error);
    ccdigest_update(di, circle_digest, di->output_size, hash_result);
    ccdigest_final(di, circle_digest, hash_result);
    return true;
}

static bool SOSCircleSetSignature(SOSCircleRef circle, SecKeyRef pubkey, CFDataRef signature, CFErrorRef *error) {
    bool result = false;
    
    CFStringRef pubKeyID = SOSCopyIDOfKey(pubkey, error);
    require_quiet(pubKeyID, fail);
    CFDictionarySetValue(circle->signatures, pubKeyID, signature);
    result = true;

fail:
    CFReleaseSafe(pubKeyID);
    return result;
}

static bool SOSCircleRemoveSignatures(SOSCircleRef circle, CFErrorRef *error) {
    CFDictionaryRemoveAllValues(circle->signatures);
    return true;
}

static CFDataRef SOSCircleGetSignature(SOSCircleRef circle, SecKeyRef pubkey, CFErrorRef *error) {    
    CFStringRef pubKeyID = SOSCopyIDOfKey(pubkey, error);
    CFDataRef result = NULL;
    require_quiet(pubKeyID, fail);

    CFTypeRef value = (CFDataRef)CFDictionaryGetValue(circle->signatures, pubKeyID);
    
    if (isData(value)) result = (CFDataRef) value;

fail:
    CFReleaseSafe(pubKeyID);
    return result;
}

bool SOSCircleSign(SOSCircleRef circle, SecKeyRef privKey, CFErrorRef *error) {
    if (!privKey) return false; // Really assertion but not always true for now.
    CFAllocatorRef allocator = CFGetAllocator(circle);
    uint8_t tmp[4096];
    size_t tmplen = 4096;
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hash_result[di->output_size];
    
    SOSCircleHash(di, circle, hash_result, error);
    OSStatus stat =  SecKeyRawSign(privKey, kSecPaddingNone, hash_result, di->output_size, tmp, &tmplen);
    if(stat) {
        // TODO - Create a CFErrorRef;
        secerror("Bad Circle SecKeyRawSign, stat: %ld", (long)stat);
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Circle SecKeyRawSign"), (error != NULL) ? *error : NULL, error);
        return false;
    };
    CFDataRef signature = CFDataCreate(allocator, tmp, tmplen);
    SecKeyRef publicKey = SecKeyCreatePublicFromPrivate(privKey);
    SOSCircleSetSignature(circle, publicKey, signature, error);
    CFReleaseNull(publicKey);
    CFRelease(signature);
    return true;
}

bool SOSCircleVerifySignatureExists(SOSCircleRef circle, SecKeyRef pubKey, CFErrorRef *error) {
    if(!pubKey) {
        // TODO ErrorRef
        secerror("SOSCircleVerifySignatureExists no pubKey");
        SOSCreateError(kSOSErrorBadFormat, CFSTR("SOSCircleVerifySignatureExists no pubKey"), (error != NULL) ? *error : NULL, error);
        return false;
    }
    CFDataRef signature = SOSCircleGetSignature(circle, pubKey, error);
    return NULL != signature;
}

bool SOSCircleVerify(SOSCircleRef circle, SecKeyRef pubKey, CFErrorRef *error) {
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hash_result[di->output_size];
    
    SOSCircleHash(di, circle, hash_result, error);

    CFDataRef signature = SOSCircleGetSignature(circle, pubKey, error);
    if(!signature) return false;

    return SecKeyRawVerify(pubKey, kSecPaddingNone, hash_result, di->output_size,
                           CFDataGetBytePtr(signature), CFDataGetLength(signature)) == errSecSuccess;
}

bool SOSCircleVerifyPeerSigned(SOSCircleRef circle, SOSPeerInfoRef peer, CFErrorRef *error) {
    SecKeyRef pub_key = SOSPeerInfoCopyPubKey(peer);
    bool result = SOSCircleVerify(circle, pub_key, error);
    CFReleaseSafe(pub_key);
    return result;
}

static void CFSetRemoveAllPassing(CFMutableSetRef set, bool (^test)(const void *) ){
    CFMutableArrayRef toBeRemoved = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);

    CFSetForEach(set, ^(const void *value) {
        if (test(value))
            CFArrayAppendValue(toBeRemoved, value);
    });
    
    CFArrayForEach(toBeRemoved, ^(const void *value) {
        CFSetRemoveValue(set, value);
    });
    CFReleaseNull(toBeRemoved);
}

static void SOSCircleRejectNonValidApplicants(SOSCircleRef circle, SecKeyRef pubkey) {
    CFMutableSetRef applicants = SOSCircleCopyApplicants(circle, NULL);
    CFSetForEach(applicants, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        if(!SOSPeerInfoApplicationVerify(pi, pubkey, NULL)) {
            CFSetRemoveValue(circle->applicants, pi);
            CFSetSetValue(circle->rejected_applicants, pi);
        }
    });
    CFReleaseNull(applicants);
}

static SOSPeerInfoRef SOSCircleCopyPeerInfo(SOSCircleRef circle, CFStringRef peer_id, CFErrorRef *error) {
    __block SOSPeerInfoRef result = NULL;
    
    CFSetForEach(circle->peers, ^(const void *value) {
        if (result == NULL) {
            SOSPeerInfoRef tpi = (SOSPeerInfoRef)value;
            if (CFEqual(SOSPeerInfoGetPeerID(tpi), peer_id))
                result = tpi;
        }
    });
    
    CFRetainSafe(result);
    return result;
}

static bool SOSCircleUpgradePeerInfo(SOSCircleRef circle, SecKeyRef user_approver, SOSFullPeerInfoRef peerinfo) {
    bool retval = false;
    SecKeyRef userPubKey = SecKeyCreatePublicFromPrivate(user_approver);
    SOSPeerInfoRef fpi_pi = SOSFullPeerInfoGetPeerInfo(peerinfo);
    SOSPeerInfoRef pi = SOSCircleCopyPeerInfo(circle, SOSPeerInfoGetPeerID(fpi_pi), NULL);
    require_quiet(pi, out);
    require_quiet(SOSPeerInfoApplicationVerify(pi, userPubKey, NULL), re_sign);
    CFReleaseNull(userPubKey);
    CFReleaseNull(pi);
    return true;

re_sign:
    secerror("SOSCircleGenerationSign: Upgraded peer's Application Signature");
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(peerinfo, NULL);
    require_quiet(device_key, out);
    SOSPeerInfoRef new_pi = SOSPeerInfoCopyAsApplication(pi, user_approver, device_key, NULL);
    if(SOSCircleUpdatePeerInfo(circle, new_pi))
        retval = true;
    CFReleaseNull(new_pi);
    CFReleaseNull(device_key);
out:
    CFReleaseNull(userPubKey);
    CFReleaseNull(pi);
    return retval;
}

bool SOSCircleGenerationSign(SOSCircleRef circle, SecKeyRef user_approver, SOSFullPeerInfoRef peerinfo, CFErrorRef *error) {
    SecKeyRef ourKey = SOSFullPeerInfoCopyDeviceKey(peerinfo, error);
    SecKeyRef publicKey = NULL;
    require_quiet(ourKey, fail);
    
    // Check if we're using an invalid peerinfo for this op.  There are cases where we might not be "upgraded".
    require_quiet(SOSCircleUpgradePeerInfo(circle, user_approver, peerinfo), fail);
    SOSCircleRemoveRetired(circle, error); // Prune off retirees since we're signing this one
    CFSetRemoveAllValues(circle->rejected_applicants); // Dump rejects so we clean them up sometime.
    publicKey = SecKeyCreatePublicFromPrivate(user_approver);
    SOSCircleRejectNonValidApplicants(circle, publicKey);
    SOSCircleGenerationIncrement(circle);
    require_quiet(SOSCircleRemoveSignatures(circle, error), fail);
    require_quiet(SOSCircleSign(circle, user_approver, error), fail);
    require_quiet(SOSCircleSign(circle, ourKey, error), fail);
    
    CFReleaseNull(ourKey);
    CFReleaseNull(publicKey);
    return true;
    
fail:
    CFReleaseNull(ourKey);
    CFReleaseNull(publicKey);
    return false;
}

bool SOSCircleGenerationUpdate(SOSCircleRef circle, SecKeyRef user_approver, SOSFullPeerInfoRef peerinfo, CFErrorRef *error) {

    return SOSCircleGenerationSign(circle, user_approver, peerinfo, error);

#if 0
    bool success = false;

    SecKeyRef ourKey = SOSFullPeerInfoCopyDeviceKey(peerinfo, error);
    require_quiet(ourKey, fail);

    require_quiet(SOSCircleSign(circle, user_approver, error), fail);
    require_quiet(SOSCircleSign(circle, ourKey, error), fail);

    success = true;

fail:
    CFReleaseNull(ourKey);
    return success;
#endif
}

bool SOSCircleConcordanceSign(SOSCircleRef circle, SOSFullPeerInfoRef peerinfo, CFErrorRef *error) {
    bool success = false;
    SecKeyRef ourKey = SOSFullPeerInfoCopyDeviceKey(peerinfo, error);
    require_quiet(ourKey, exit);
    
    success = SOSCircleSign(circle, ourKey, error);

exit:
    CFReleaseNull(ourKey);
    return success;
}

static inline SOSConcordanceStatus CheckPeerStatus(SOSCircleRef circle, SOSPeerInfoRef peer, SecKeyRef user_public_key, CFErrorRef *error) {
    SOSConcordanceStatus result = kSOSConcordanceNoPeer;
    SecKeyRef pubKey = SOSPeerInfoCopyPubKey(peer);

    require_action_quiet(SOSCircleHasActiveValidPeer(circle, peer, user_public_key, error), exit, result = kSOSConcordanceNoPeer);
    require_action_quiet(SOSCircleVerifySignatureExists(circle, pubKey, error), exit, result = kSOSConcordanceNoPeerSig);
    require_action_quiet(SOSCircleVerify(circle, pubKey, error), exit, result = kSOSConcordanceBadPeerSig);

    result = kSOSConcordanceTrusted;
    
exit:
    CFReleaseNull(pubKey);
    return result;
}

static inline SOSConcordanceStatus CombineStatus(SOSConcordanceStatus status1, SOSConcordanceStatus status2)
{
    if (status1 == kSOSConcordanceTrusted || status2 == kSOSConcordanceTrusted)
        return kSOSConcordanceTrusted;
    
    if (status1 == kSOSConcordanceBadPeerSig || status2 == kSOSConcordanceBadPeerSig)
        return kSOSConcordanceBadPeerSig;
    
    if (status1 == kSOSConcordanceNoPeerSig || status2 == kSOSConcordanceNoPeerSig)
        return kSOSConcordanceNoPeerSig;

    return status1;
}

static inline bool SOSCircleIsEmpty(SOSCircleRef circle) {
    return SOSCircleCountPeers(circle) == 0;
}

static inline bool SOSCircleIsOffering(SOSCircleRef circle) {
    return SOSCircleCountPeers(circle) == 1;
}

__unused static inline bool SOSCircleIsResignOffering(SOSCircleRef circle, SecKeyRef pubkey) {
    return SOSCircleCountActiveValidPeers(circle, pubkey) == 1;
}

static inline SOSConcordanceStatus GetSignersStatus(SOSCircleRef signers_circle, SOSCircleRef status_circle,
                                                    SecKeyRef user_pubKey, SOSPeerInfoRef exclude, CFErrorRef *error) {
    CFStringRef excluded_id = exclude ? SOSPeerInfoGetPeerID(exclude) : NULL;

    __block SOSConcordanceStatus status = kSOSConcordanceNoPeer;
    SOSCircleForEachActivePeer(signers_circle, ^(SOSPeerInfoRef peer) {
        SOSConcordanceStatus peerStatus = CheckPeerStatus(status_circle, peer, user_pubKey, error);

        if (peerStatus == kSOSConcordanceNoPeerSig &&
            (CFEqualSafe(SOSPeerInfoGetPeerID(peer), excluded_id) || SOSPeerInfoIsCloudIdentity(peer)))
            peerStatus = kSOSConcordanceNoPeer;

        status = CombineStatus(status, peerStatus); // TODO: Use multiple error gathering.
    });

    return status;
}

static inline bool isOlderGeneration(SOSCircleRef current, SOSCircleRef proposed) {
    return CFNumberCompare(current->generation, proposed->generation, NULL) == kCFCompareGreaterThan;
}

bool SOSCircleSharedTrustedPeers(SOSCircleRef current, SOSCircleRef proposed, SOSPeerInfoRef me) {
    __block bool retval = false;
    SOSCircleForEachPeer(current, ^(SOSPeerInfoRef peer) {
        if(!CFEqual(me, peer) && SOSCircleHasPeer(proposed, peer, NULL)) retval = true;
    });
    return retval;
}

static void SOSCircleUpgradePeersByCircle(SOSCircleRef known_circle, SOSCircleRef proposed_circle) {
    SOSCircleForEachPeer(known_circle, ^(SOSPeerInfoRef known_peer) {
        SOSPeerInfoRef proposed_peer = SOSCircleCopyPeerInfo(proposed_circle, SOSPeerInfoGetPeerID(known_peer), NULL);
        if(proposed_peer && CFEqualSafe(proposed_peer, known_peer) != 0) {
            SOSCircleUpdatePeerInfo(known_circle, proposed_peer);
        }
    });
}

SOSConcordanceStatus SOSCircleConcordanceTrust(SOSCircleRef known_circle, SOSCircleRef proposed_circle,
                                               SecKeyRef known_pubkey, SecKeyRef user_pubkey,
                                               SOSPeerInfoRef exclude, CFErrorRef *error) {
    if(user_pubkey == NULL) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Concordance with no public key"), NULL, error);
        return kSOSConcordanceNoUserKey; //TODO: - needs to return an error
    }

    if (SOSCircleIsEmpty(proposed_circle)) {
        return kSOSConcordanceTrusted;
    }
    
    if(!SOSCircleVerifySignatureExists(proposed_circle, user_pubkey, error)) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("No public signature"), (error != NULL) ? *error : NULL, error);
        return kSOSConcordanceNoUserSig;
    }
    
    if(!SOSCircleVerify(proposed_circle, user_pubkey, error)) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("Bad public signature"), (error != NULL) ? *error : NULL, error);
        return kSOSConcordanceBadUserSig;
    }

    if (SOSCircleIsEmpty(known_circle) || SOSCircleIsOffering(proposed_circle)) {
        return GetSignersStatus(proposed_circle, proposed_circle, user_pubkey, NULL, error);
    }

    if(isOlderGeneration(known_circle, proposed_circle)) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Bad generation"), NULL, error);
        return kSOSConcordanceGenOld;
    }
    
    
    if(!SOSCircleVerify(known_circle, user_pubkey, error)) {
        SOSCircleUpgradePeersByCircle(known_circle, proposed_circle);
    }
    
    if(known_pubkey == NULL) known_pubkey = user_pubkey;
    if(!SOSCircleVerify(known_circle, known_pubkey, error)) known_pubkey = user_pubkey;
    return GetSignersStatus(known_circle, proposed_circle, known_pubkey, exclude, error);
}


static const uint8_t* der_decode_mutable_dictionary(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                    CFMutableDictionaryRef* dictionary, CFErrorRef *error,
                                                    const uint8_t* der, const uint8_t *der_end)
{
    CFDictionaryRef theDict;
    const uint8_t* result = der_decode_dictionary(allocator, mutability, &theDict, error, der, der_end);

    if (result != NULL)
        *dictionary = (CFMutableDictionaryRef)theDict;

    return result;
}


SOSCircleRef SOSCircleCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end) {
    SOSCircleRef cir = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);

    const uint8_t *sequence_end;

    cir->name = NULL;
    cir->generation = NULL;
    cir->peers = NULL;
    cir->applicants = NULL;
    cir->rejected_applicants = NULL;
    cir->signatures = NULL;

    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    require_action_quiet(sequence_end != NULL, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Circle DER"), (error != NULL) ? *error : NULL, error));
    
    // Version first.
    uint64_t version = 0;
    *der_p = ccder_decode_uint64(&version, *der_p, der_end);
    
    require_action_quiet(version == kOnlyCompatibleVersion, fail,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Bad Circle Version"), NULL, error));

    *der_p = der_decode_string(allocator, 0, &cir->name, error, *der_p, sequence_end);
    *der_p = der_decode_number(allocator, 0, &cir->generation, error, *der_p, sequence_end);

    cir->peers = SOSPeerInfoSetCreateFromArrayDER(allocator, &kSOSPeerSetCallbacks, error, der_p, sequence_end);
    cir->applicants = SOSPeerInfoSetCreateFromArrayDER(allocator, &kSOSPeerSetCallbacks, error, der_p, sequence_end);
    cir->rejected_applicants = SOSPeerInfoSetCreateFromArrayDER(allocator, &kSOSPeerSetCallbacks, error, der_p, sequence_end);

    *der_p = der_decode_mutable_dictionary(allocator, kCFPropertyListMutableContainersAndLeaves,
                                           &cir->signatures, error, *der_p, sequence_end);

    require_action_quiet(*der_p == sequence_end, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Circle DER"), (error != NULL) ? *error : NULL, error));

    return cir;
    
fail:
    CFReleaseNull(cir);
    return NULL;
}

SOSCircleRef SOSCircleCreateFromData(CFAllocatorRef allocator, CFDataRef circleData, CFErrorRef *error)
{    
    size_t size = CFDataGetLength(circleData);
    const uint8_t *der = CFDataGetBytePtr(circleData);
    SOSCircleRef inflated = SOSCircleCreateFromDER(allocator, error, &der, der + size);
    return inflated;
}

size_t SOSCircleGetDEREncodedSize(SOSCircleRef cir, CFErrorRef *error) {
    SOSCircleAssertStable(cir);
    size_t total_payload = 0;

    require_quiet(accumulate_size(&total_payload, ccder_sizeof_uint64(kOnlyCompatibleVersion)),                        fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_string(cir->name, error)),                                fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_number(cir->generation, error)),                          fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoSetGetDEREncodedArraySize(cir->peers, error)),            fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoSetGetDEREncodedArraySize(cir->applicants, error)),          fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoSetGetDEREncodedArraySize(cir->rejected_applicants, error)), fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_dictionary((CFDictionaryRef) cir->signatures, error)),    fail);

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);
    
fail:
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSCircleEncodeToDER(SOSCircleRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    SOSCircleAssertStable(cir);

    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           ccder_encode_uint64(kOnlyCompatibleVersion, der,
           der_encode_string(cir->name, error, der,
           der_encode_number(cir->generation, error, der,
           SOSPeerInfoSetEncodeToArrayDER(cir->peers, error, der,
           SOSPeerInfoSetEncodeToArrayDER(cir->applicants, error, der,
           SOSPeerInfoSetEncodeToArrayDER(cir->rejected_applicants, error, der,
           der_encode_dictionary((CFDictionaryRef) cir->signatures, error, der, der_end))))))));
}

CFDataRef SOSCircleCreateIncompatibleCircleDER(CFErrorRef* error)
{
    size_t total_payload = 0;
    size_t encoded_size = 0;
    uint8_t* der = 0;
    uint8_t* der_end = 0;
    CFMutableDataRef result = NULL;
    
    require_quiet(accumulate_size(&total_payload, ccder_sizeof_uint64(kAlwaysIncompatibleVersion)), fail);
    
    encoded_size = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);

    result = CFDataCreateMutableWithScratch(kCFAllocatorDefault, encoded_size);
    
    der = CFDataGetMutableBytePtr(result);
    der_end = der + CFDataGetLength(result);
    
    der_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
              ccder_encode_uint64(kAlwaysIncompatibleVersion, der, der_end));
 
fail:
    if (der == NULL || der != der_end)
        CFReleaseNull(result);

    return result;
}


CFDataRef SOSCircleCopyEncodedData(SOSCircleRef circle, CFAllocatorRef allocator, CFErrorRef *error)
{
    size_t size = SOSCircleGetDEREncodedSize(circle, error);
    if (size == 0)
        return NULL;
    uint8_t buffer[size];
    uint8_t* start = SOSCircleEncodeToDER(circle, error, buffer, buffer + sizeof(buffer));
    CFDataRef result = CFDataCreate(kCFAllocatorDefault, start, size);
    return result;
}

static void SOSCircleDestroy(CFTypeRef aObj) {
    SOSCircleRef c = (SOSCircleRef) aObj;

    CFReleaseNull(c->name);
    CFReleaseNull(c->generation);
    CFReleaseNull(c->peers);
    CFReleaseNull(c->applicants);
    CFReleaseNull(c->rejected_applicants);
    CFReleaseNull(c->signatures);
}

static CFStringRef SOSCircleCopyDescription(CFTypeRef aObj) {
    SOSCircleRef c = (SOSCircleRef) aObj;
    
    SOSCircleAssertStable(c);


    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendFormat(description, NULL, CFSTR("<SOSCircle@%p: '%@' P:["), c, c->name);

    __block CFStringRef separator = CFSTR("");
    SOSCircleForEachPeer(c, ^(SOSPeerInfoRef peer) {
        CFStringRef sig = NULL;
        if (SOSCircleVerifyPeerSigned(c, peer, NULL)) {
            sig = CFSTR("√");
        } else {
            SecKeyRef pub_key = SOSPeerInfoCopyPubKey(peer);
            CFDataRef signature = SOSCircleGetSignature(c, pub_key, NULL);
            sig = (signature == NULL) ? CFSTR("-") : CFSTR("?");
            CFReleaseNull(pub_key);
        }

        CFStringAppendFormat(description, NULL, CFSTR("%@%@ %@"), separator, peer, sig);
        separator = CFSTR(",");
    });

    CFStringAppend(description, CFSTR("], A:["));
    separator = CFSTR("");
    SOSCircleForEachApplicant(c, ^(SOSPeerInfoRef peer) {
        CFStringAppendFormat(description, NULL, CFSTR("%@%@"), separator, peer);
        separator = CFSTR(",");
    });

    CFStringAppend(description, CFSTR("], R:["));
    separator = CFSTR("");
    CFSetForEach(c->rejected_applicants, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
        CFStringAppendFormat(description, NULL, CFSTR("%@%@"), separator, peer);
        separator = CFSTR(",");
    });
    CFStringAppend(description, CFSTR("]>"));

    return description;
}

CFStringRef SOSCircleGetName(SOSCircleRef circle) {
    assert(circle);
    assert(circle->name);
    return circle->name;
}

const char *SOSCircleGetNameC(SOSCircleRef circle) {
    CFStringRef name = SOSCircleGetName(circle);
    if (!name)
        return strdup("");
    return CFStringToCString(name);
}

CFNumberRef SOSCircleGetGeneration(SOSCircleRef circle) {
    assert(circle);
    assert(circle->generation);
    return circle->generation;
}

int64_t SOSCircleGetGenerationSint(SOSCircleRef circle) {
    CFNumberRef gen = SOSCircleGetGeneration(circle);
    int64_t value;
    if(!gen) return 0;
    CFNumberGetValue(gen, kCFNumberSInt64Type, &value);
    return value;
}

void SOSCircleGenerationSetValue(SOSCircleRef circle, int64_t value)
{
    CFAllocatorRef allocator = CFGetAllocator(circle->generation);
    CFAssignRetained(circle->generation, CFNumberCreate(allocator, kCFNumberSInt64Type, &value));
}


static int64_t GenerationSetHighBits(int64_t value, int32_t high_31)
{
    value &= 0xFFFFFFFF; // Keep the low 32 bits.
    value |= ((int64_t) high_31) << 32;

    return value;
}


void SOSCircleGenerationIncrement(SOSCircleRef circle) {
    int64_t value = SOSCircleGetGenerationSint(circle);

    if ((value >> 32) == 0) {
        uint32_t seconds = CFAbsoluteTimeGetCurrent(); // seconds
        value = GenerationSetHighBits(value, (seconds >> 1));
    }

    value++;

    SOSCircleGenerationSetValue(circle, value);
}

int SOSCircleCountPeers(SOSCircleRef circle) {
    SOSCircleAssertStable(circle);
    __block int count = 0;
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        ++count;
    });
    return count;
}

int SOSCircleCountActivePeers(SOSCircleRef circle) {
    SOSCircleAssertStable(circle);
    __block int count = 0;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        ++count;
    });
    return count;
}

int SOSCircleCountActiveValidPeers(SOSCircleRef circle, SecKeyRef pubkey) {
    SOSCircleAssertStable(circle);
    __block int count = 0;
    SOSCircleForEachActiveValidPeer(circle, pubkey, ^(SOSPeerInfoRef peer) {
        ++count;
    });
    return count;
}

int SOSCircleCountRetiredPeers(SOSCircleRef circle) {
    SOSCircleAssertStable(circle);
    __block int count = 0;
    SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
        ++count;
    });
    return count;
}

int SOSCircleCountApplicants(SOSCircleRef circle) {
    SOSCircleAssertStable(circle);
    
    return (int)CFSetGetCount(circle->applicants);
}

bool SOSCircleHasApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    
    return CFSetContainsValue(circle->applicants, peerInfo);
}

CFMutableSetRef SOSCircleCopyApplicants(SOSCircleRef circle, CFAllocatorRef allocator) {
    SOSCircleAssertStable(circle);
    
    return CFSetCreateMutableCopy(allocator, 0, circle->applicants);
}

int SOSCircleCountRejectedApplicants(SOSCircleRef circle) {
    SOSCircleAssertStable(circle);
    
    return (int)CFSetGetCount(circle->rejected_applicants);
}

bool SOSCircleHasRejectedApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    return CFSetContainsValue(circle->rejected_applicants, peerInfo);
}

SOSPeerInfoRef SOSCircleCopyRejectedApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    return CFRetainSafe((SOSPeerInfoRef)CFSetGetValue(circle->rejected_applicants, peerInfo));
}

CFMutableArrayRef SOSCircleCopyRejectedApplicants(SOSCircleRef circle, CFAllocatorRef allocator) {
    SOSCircleAssertStable(circle);
    
    return CFSetCopyValuesCFArray(circle->rejected_applicants);
}


bool SOSCircleHasPeerWithID(SOSCircleRef circle, CFStringRef peerid, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    __block bool found = false;
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        if(peerid && peer && CFEqualSafe(peerid, SOSPeerInfoGetPeerID(peer))) found = true;
    });
    return found;
}

SOSPeerInfoRef SOSCircleCopyPeerWithID(SOSCircleRef circle, CFStringRef peerid, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    __block SOSPeerInfoRef found = false;
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        if(peerid && peer && CFEqualSafe(peerid, SOSPeerInfoGetPeerID(peer))) found = peer;
    });
    return found;
}

bool SOSCircleHasPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    if(!peerInfo) return false;
    return SOSCircleHasPeerWithID(circle, SOSPeerInfoGetPeerID(peerInfo), error);
}

bool SOSCircleHasActivePeerWithID(SOSCircleRef circle, CFStringRef peerid, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    __block bool found = false;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(peerid && peer && CFEqualSafe(peerid, SOSPeerInfoGetPeerID(peer))) found = true;
    });
    return found;
}

bool SOSCircleHasActivePeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    if(!peerInfo) return false;
    return SOSCircleHasActivePeerWithID(circle, SOSPeerInfoGetPeerID(peerInfo), error);
}

bool SOSCircleHasActiveValidPeerWithID(SOSCircleRef circle, CFStringRef peerid, SecKeyRef user_public_key, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    __block bool found = false;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(peerid && peer && CFEqualSafe(peerid, SOSPeerInfoGetPeerID(peer)) && SOSPeerInfoApplicationVerify(peer, user_public_key, NULL)) found = true;
    });
    return found;
}

bool SOSCircleHasActiveValidPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, SecKeyRef user_public_key, CFErrorRef *error) {
    if(!peerInfo) return false;
    return SOSCircleHasActiveValidPeerWithID(circle, SOSPeerInfoGetPeerID(peerInfo), user_public_key, error);
}


bool SOSCircleResetToEmpty(SOSCircleRef circle, CFErrorRef *error) {
    CFSetRemoveAllValues(circle->applicants);
    CFSetRemoveAllValues(circle->rejected_applicants);
    CFSetRemoveAllValues(circle->peers);
    CFDictionaryRemoveAllValues(circle->signatures);

    int64_t old_value = SOSCircleGetGenerationSint(circle);

    SOSCircleGenerationSetValue(circle, 0);
    SOSCircleGenerationIncrement(circle); // Increment to get high bits set.

    int64_t new_value = SOSCircleGetGenerationSint(circle);

    if (new_value <= old_value) {
        int64_t new_new_value = GenerationSetHighBits(new_value, (old_value >> 32) + 1);
        SOSCircleGenerationSetValue(circle, new_new_value);
    }

    return true;
}

bool SOSCircleResetToOffering(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error){

    return SOSCircleResetToEmpty(circle, error)
        && SOSCircleRequestAdmission(circle, user_privkey, requestor, error)
        && SOSCircleAcceptRequest(circle, user_privkey, requestor, SOSFullPeerInfoGetPeerInfo(requestor), error);
}

bool SOSCircleRemoveRetired(SOSCircleRef circle, CFErrorRef *error) {
    CFSetRemoveAllPassing(circle->peers,  ^ bool (const void *element) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) element;
        
        return SOSPeerInfoIsRetirementTicket(peer);
    });
    
    return true;
}

static bool SOSCircleRecordAdmissionRequest(SOSCircleRef circle, SecKeyRef user_pubkey, SOSPeerInfoRef requestorPeerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    
    bool isPeer = SOSCircleHasPeer(circle, requestorPeerInfo, error);
    
    require_action_quiet(!isPeer, fail, SOSCreateError(kSOSErrorAlreadyPeer, CFSTR("Cannot request admission when already a peer"), NULL, error));
    
    CFSetRemoveValue(circle->rejected_applicants, requestorPeerInfo); // We remove from rejected list, in case?
    CFSetSetValue(circle->applicants, requestorPeerInfo);
    
    return true;
    
fail:
    return false;
    
}

bool SOSCircleRequestReadmission(SOSCircleRef circle, SecKeyRef user_pubkey, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;
    
    require_quiet(SOSPeerInfoApplicationVerify(peer, user_pubkey, error), fail);
    success = SOSCircleRecordAdmissionRequest(circle, user_pubkey, peer, error);
fail:
    return success;
}

bool SOSCircleRequestAdmission(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool success = false;
    
    SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_action_quiet(user_pubkey, fail, SOSCreateError(kSOSErrorBadKey, CFSTR("No public key for key"), NULL, error));

    require(SOSFullPeerInfoPromoteToApplication(requestor, user_privkey, error), fail);
    
    success = SOSCircleRecordAdmissionRequest(circle, user_pubkey, SOSFullPeerInfoGetPeerInfo(requestor), error);
fail:
    CFReleaseNull(user_pubkey);
    return success;
}


bool SOSCircleUpdatePeerInfo(SOSCircleRef circle, SOSPeerInfoRef replacement_peer_info) {
    if(!replacement_peer_info) return false;
    CFTypeRef old = CFSetGetValue(circle->peers, replacement_peer_info);
    bool replace = old && !CFEqualSafe(old, replacement_peer_info);
    if (replace)
        CFSetReplaceValue(circle->peers, replacement_peer_info);
    
    return replace;
}
      
bool SOSCircleRemovePeer(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, SOSPeerInfoRef peer_to_remove, CFErrorRef *error) {
    SOSPeerInfoRef requestor_peer_info = SOSFullPeerInfoGetPeerInfo(requestor);
        
    if (SOSCircleHasApplicant(circle, peer_to_remove, error)) {
        return SOSCircleRejectRequest(circle, requestor, peer_to_remove, error);
    }

    if (!SOSCircleHasPeer(circle, requestor_peer_info, error)) {
        SOSCreateError(kSOSErrorAlreadyPeer, CFSTR("Must be peer to remove peer"), NULL, error);
        return false;
    }

    CFSetRemoveValue(circle->peers, peer_to_remove);

    SOSCircleGenerationSign(circle, user_privkey, requestor, error);

    return true;
}

bool SOSCircleAcceptRequest(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    SecKeyRef publicKey = NULL;
    bool result = false;

    require_action_quiet(CFSetContainsValue(circle->applicants, peerInfo), fail,
                         SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot accept non-applicant"), NULL, error));
    
    publicKey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_quiet(SOSPeerInfoApplicationVerify(peerInfo, publicKey, error), fail);

    CFSetRemoveValue(circle->applicants, peerInfo);
    CFSetSetValue(circle->peers, peerInfo);
    
    result = SOSCircleGenerationSign(circle, user_privkey, device_approver, error);
    secnotice("circle", "Accepted %@", peerInfo);

fail:
    CFReleaseNull(publicKey);
    return result;
}

bool SOSCircleWithdrawRequest(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    CFSetRemoveValue(circle->applicants, peerInfo);

    return true;
}

bool SOSCircleRemoveRejectedPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    
    CFSetRemoveValue(circle->rejected_applicants, peerInfo);
    
    return true;
}


bool SOSCircleRejectRequest(SOSCircleRef circle, SOSFullPeerInfoRef device_rejector,
                            SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    if (CFEqual(SOSPeerInfoGetPeerID(peerInfo), SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(device_rejector))))
        return SOSCircleWithdrawRequest(circle, peerInfo, error);

    if (!CFSetContainsValue(circle->applicants, peerInfo)) {
        SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot reject non-applicant"), NULL, error);
        return false;
    }
    
    CFSetRemoveValue(circle->applicants, peerInfo);
    CFSetSetValue(circle->rejected_applicants, peerInfo);
    
    // TODO: Maybe we sign the rejection with device_rejector.
    
    return true;
}

bool SOSCircleAcceptRequests(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver,
                             CFErrorRef *error) {
    // Returns true if we accepted someone and therefore have to post the circle back to KVS
    __block bool result = false;
    
    SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
        if (!SOSCircleAcceptRequest(circle, user_privkey, device_approver, peer, error))
            printf("error in SOSCircleAcceptRequest\n");
        else {
            secnotice("circle", "Accepted peer: %@", peer);
            result = true;
        }
    });
    
    if (result) {
        SOSCircleGenerationSign(circle, user_privkey, device_approver, error);
        secnotice("circle", "Countersigned accepted requests");
    }

    return result;
}

bool SOSCirclePeerSigUpdate(SOSCircleRef circle, SecKeyRef userPrivKey, SOSFullPeerInfoRef fpi,
                             CFErrorRef *error) {
    // Returns true if we accepted someone and therefore have to post the circle back to KVS
    __block bool result = false;
    SecKeyRef userPubKey = SecKeyCreatePublicFromPrivate(userPrivKey);

    // We're going to remove any applicants using a mismatched user key.
    SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
        if(!SOSPeerInfoApplicationVerify(peer, userPubKey, NULL)) {
            if(!SOSCircleRejectRequest(circle, fpi, peer, NULL)) {
                // do we care?
            }
        }
    });
    
    result = SOSCircleUpdatePeerInfo(circle, SOSFullPeerInfoGetPeerInfo(fpi));
    
    if (result) {
        SOSCircleGenerationSign(circle, userPrivKey, fpi, error);
        secnotice("circle", "Generation signed updated signatures on peerinfo");
    }
    
    return result;
}

static inline void SOSCircleForEachPeerMatching(SOSCircleRef circle,
                                                void (^action)(SOSPeerInfoRef peer),
                                                bool (^condition)(SOSPeerInfoRef peer)) {
    CFSetForEach(circle->peers, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
        if (condition(peer))
            action(peer);
    });
}

static inline bool isHiddenPeer(SOSPeerInfoRef peer) {
    return SOSPeerInfoIsRetirementTicket(peer) || SOSPeerInfoIsCloudIdentity(peer);
}

void SOSCircleForEachPeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer)) {
    SOSCircleForEachPeerMatching(circle, action, ^bool(SOSPeerInfoRef peer) {
        return !isHiddenPeer(peer);
    });
}

void SOSCircleForEachRetiredPeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer)) {
    SOSCircleForEachPeerMatching(circle, action, ^bool(SOSPeerInfoRef peer) {
        return SOSPeerInfoIsRetirementTicket(peer);
    });
}

void SOSCircleForEachActivePeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer)) {
    SOSCircleForEachPeerMatching(circle, action, ^bool(SOSPeerInfoRef peer) {
        return true;
    });
}

void SOSCircleForEachActiveValidPeer(SOSCircleRef circle, SecKeyRef user_public_key, void (^action)(SOSPeerInfoRef peer)) {
    SOSCircleForEachPeerMatching(circle, action, ^bool(SOSPeerInfoRef peer) {
        return SOSPeerInfoApplicationVerify(peer, user_public_key, NULL);
    });
}

void SOSCircleForEachApplicant(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer)) {
    CFSetForEach(circle->applicants, ^(const void*value) { action((SOSPeerInfoRef) value); } );
}


CFMutableSetRef SOSCircleCopyPeers(SOSCircleRef circle, CFAllocatorRef allocator) {
    SOSCircleAssertStable(circle);
    
    CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        CFSetAddValue(result, peer);
    });
    
    return result;
}

bool SOSCircleAppendConcurringPeers(SOSCircleRef circle, CFMutableArrayRef appendHere, CFErrorRef *error) {
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        CFErrorRef localError = NULL;
        if (SOSCircleVerifyPeerSigned(circle, peer, &localError)) {
            CFArrayAppendValue(appendHere, peer);
        } else if (error != NULL) {
            secerror("Error checking concurrence: %@", localError);
        }
        CFReleaseNull(localError);
    });

    return true;
}

CFMutableArrayRef SOSCircleCopyConcurringPeers(SOSCircleRef circle, CFErrorRef* error) {
    SOSCircleAssertStable(circle);

    CFMutableArrayRef concurringPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    if (!SOSCircleAppendConcurringPeers(circle, concurringPeers, error))
        CFReleaseNull(concurringPeers);

    return concurringPeers;
}

SOSFullPeerInfoRef SOSCircleGetiCloudFullPeerInfoRef(SOSCircleRef circle) {
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoIsCloudIdentity(peer)) {
            if (cloud_full_peer == NULL) {
                CFErrorRef localError = NULL;
                cloud_full_peer = SOSFullPeerInfoCreateCloudIdentity(kCFAllocatorDefault, peer, &localError);
                
                if (localError) {
                    secerror("Found cloud peer in circle but can't make full peer: %@", localError);
                    CFReleaseNull(localError);
                }
                
            } else {
                secerror("More than one cloud identity found in circle: %@", circle);
            }
        }
    });
    return cloud_full_peer;
}
