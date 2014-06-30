/*
 * Created by Michael Brouwer on 6/22/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
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
    CFMutableArrayRef peers;
    CFMutableArrayRef applicants;
    CFMutableArrayRef rejected_applicants;

    CFMutableDictionaryRef signatures;
};

CFGiblisWithCompareFor(SOSCircle);

// Move the next 2 lines to SOSPeer if we need it.
static void SOSPeerRelease(CFAllocatorRef allocator, const void *value) {
    SOSPeerDispose((SOSPeerRef)value);
}

static const CFDictionaryValueCallBacks dispose_peer_callbacks = { .release = SOSPeerRelease };

SOSCircleRef SOSCircleCreate(CFAllocatorRef allocator, CFStringRef name, CFErrorRef *error) {
    SOSCircleRef c = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);
    int64_t gen = 1;

    assert(name);
    
    c->name = CFStringCreateCopy(allocator, name);
    c->generation = CFNumberCreate(allocator, kCFNumberSInt64Type, &gen);
    c->peers = CFArrayCreateMutableForCFTypes(allocator);
    c->applicants = CFArrayCreateMutableForCFTypes(allocator);
    c->rejected_applicants = CFArrayCreateMutableForCFTypes(allocator);
    c->signatures = CFDictionaryCreateMutableForCFTypes(allocator);

    return c;
}

static CFNumberRef SOSCircleGenerationCopy(CFNumberRef generation) {
    int64_t value;
    CFAllocatorRef allocator = CFGetAllocator(generation);
    CFNumberGetValue(generation, kCFNumberSInt64Type, &value);
    return CFNumberCreate(allocator, kCFNumberSInt64Type, &value);
}

SOSCircleRef SOSCircleCopyCircle(CFAllocatorRef allocator, SOSCircleRef otherCircle, CFErrorRef *error)
{
    SOSCircleRef c = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);

    assert(otherCircle);
    c->name = CFStringCreateCopy(allocator, otherCircle->name);
    c->generation = SOSCircleGenerationCopy(otherCircle->generation);
    c->peers = CFArrayCreateMutableCopy(allocator, 0, otherCircle->peers);
    c->applicants = CFArrayCreateMutableCopy(allocator, 0, otherCircle->applicants);
    c->rejected_applicants = CFArrayCreateMutableCopy(allocator, 0, otherCircle->rejected_applicants);
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
        && CFEqual(left->generation, right->generation)
        && CFEqual(left->peers, right->peers)
        && CFEqual(left->applicants, right->applicants)
        && CFEqual(left->rejected_applicants, right->rejected_applicants)
        && CFEqual(left->signatures, right->signatures);
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

static bool SOSCircleHash(const struct ccdigest_info *di, SOSCircleRef circle, void *hash_result, CFErrorRef *error) {
    ccdigest_di_decl(di, circle_digest);
    ccdigest_init(di, circle_digest);
    int64_t gen = SOSCircleGetGenerationSint(circle);
    ccdigest_update(di, circle_digest, sizeof(gen), &gen);
    
    SOSCircleDigestArray(di, circle->peers, hash_result, error);
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


static CFIndex CFArrayRemoveAllPassing(CFMutableArrayRef array, bool (^test)(const void *) ){
    CFIndex numberRemoved = 0;
    
    CFIndex position =  0;
    while (position < CFArrayGetCount(array) && !test(CFArrayGetValueAtIndex(array, position)))
        ++position;
    
    while (position < CFArrayGetCount(array)) {
        CFArrayRemoveValueAtIndex(array, position);
        ++numberRemoved;
        while (position < CFArrayGetCount(array) && !test(CFArrayGetValueAtIndex(array, position)))
            ++position;
    }
    
    return numberRemoved;
}

static CFIndex CFArrayRemoveAllWithMatchingID(CFMutableArrayRef array, SOSPeerInfoRef peerInfo) {
    CFStringRef peer_id = SOSPeerInfoGetPeerID(peerInfo);
    if (!peer_id) return 0;
    
    return CFArrayRemoveAllPassing(array,  ^ bool (const void *element) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) element;
        
        return CFEqual(peer_id, SOSPeerInfoGetPeerID(peer));
    });
}

static void SOSCircleRejectNonValidApplicants(SOSCircleRef circle, SecKeyRef pubkey) {
    CFArrayRef applicants = SOSCircleCopyApplicants(circle, NULL);
    CFArrayForEach(applicants, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        if(!SOSPeerInfoApplicationVerify(pi, pubkey, NULL)) {
            CFArrayRemoveAllWithMatchingID(circle->applicants, pi);
            CFArrayAppendValue(circle->rejected_applicants, pi);
        }
    });
}

bool SOSCircleGenerationSign(SOSCircleRef circle, SecKeyRef user_approver, SOSFullPeerInfoRef peerinfo, CFErrorRef *error) {
    
    SecKeyRef ourKey = SOSFullPeerInfoCopyDeviceKey(peerinfo, error);
    require_quiet(ourKey, fail);
    
    SOSCircleRemoveRetired(circle, error); // Prune off retirees since we're signing this one
    CFArrayRemoveAllValues(circle->rejected_applicants); // Dump rejects so we clean them up sometime.
    SOSCircleRejectNonValidApplicants(circle, SecKeyCreatePublicFromPrivate(user_approver));
    SOSCircleGenerationIncrement(circle);
    require_quiet(SOSCircleRemoveSignatures(circle, error), fail);
    require_quiet(SOSCircleSign(circle, user_approver, error), fail);
    require_quiet(SOSCircleSign(circle, ourKey, error), fail);
    
    CFReleaseNull(ourKey);
    return true;
    
fail:
    CFReleaseNull(ourKey);
    return false;
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

static inline SOSConcordanceStatus CheckPeerStatus(SOSCircleRef circle, SOSPeerInfoRef peer, CFErrorRef *error) {
    SOSConcordanceStatus result = kSOSConcordanceNoPeer;
    SecKeyRef pubKey = SOSPeerInfoCopyPubKey(peer);

    require_action_quiet(SOSCircleHasActivePeer(circle, peer, error), exit, result = kSOSConcordanceNoPeer);
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

static inline bool SOSCircleIsResignOffering(SOSCircleRef circle, SecKeyRef pubkey) {
    return SOSCircleCountActiveValidPeers(circle, pubkey) == 1;
}

static inline SOSConcordanceStatus GetSignersStatus(SOSCircleRef signers_circle, SOSCircleRef status_circle,
                                                    SecKeyRef user_pubKey, SOSPeerInfoRef exclude, CFErrorRef *error) {
    CFStringRef excluded_id = exclude ? SOSPeerInfoGetPeerID(exclude) : NULL;

    __block SOSConcordanceStatus status = kSOSConcordanceNoPeer;
    SOSCircleForEachActiveValidPeer(signers_circle, user_pubKey, ^(SOSPeerInfoRef peer) {
        SOSConcordanceStatus peerStatus = CheckPeerStatus(status_circle, peer, error);

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

    cir->peers = SOSPeerInfoArrayCreateFromDER(allocator, error, der_p, sequence_end);
    cir->applicants = SOSPeerInfoArrayCreateFromDER(allocator, error, der_p, sequence_end);
    cir->rejected_applicants = SOSPeerInfoArrayCreateFromDER(allocator, error, der_p, sequence_end);

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
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoArrayGetDEREncodedSize(cir->peers, error)),               fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoArrayGetDEREncodedSize(cir->applicants, error)),          fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoArrayGetDEREncodedSize(cir->rejected_applicants, error)), fail);
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
           SOSPeerInfoArrayEncodeToDER(cir->peers, error, der,
           SOSPeerInfoArrayEncodeToDER(cir->applicants, error, der,
           SOSPeerInfoArrayEncodeToDER(cir->rejected_applicants, error, der,
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

    return CFStringCreateWithFormat(NULL, NULL,
                                    CFSTR("<SOSCircle@%p: [ \nName: %@, \nPeers: %@,\nApplicants: %@,\nRejects: %@,\nSignatures: %@\n ] >"),
                                    c, c->name, c->peers, c->applicants, c->rejected_applicants, c->signatures);
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

void SOSCircleGenerationIncrement(SOSCircleRef circle) {
    CFAllocatorRef allocator = CFGetAllocator(circle->generation);
    int64_t value = SOSCircleGetGenerationSint(circle);
    value++;
    circle->generation = CFNumberCreate(allocator, kCFNumberSInt64Type, &value);
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
    
    return (int)CFArrayGetCount(circle->applicants);
}

bool SOSCircleHasApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    
    return CFArrayHasValueMatching(circle->applicants, ^bool(const void *value) {
        return SOSPeerInfoCompareByID(value, peerInfo, NULL) == 0;
    });
}

CFMutableArrayRef SOSCircleCopyApplicants(SOSCircleRef circle, CFAllocatorRef allocator) {
    SOSCircleAssertStable(circle);
    
    return CFArrayCreateMutableCopy(allocator, 0, circle->applicants);
}

int SOSCircleCountRejectedApplicants(SOSCircleRef circle) {
    SOSCircleAssertStable(circle);
    
    return (int)CFArrayGetCount(circle->rejected_applicants);
}

bool SOSCircleHasRejectedApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    return CFArrayHasValueMatching(circle->rejected_applicants, ^bool(const void *value) {
        return SOSPeerInfoCompareByID(value, peerInfo, NULL) == 0;
    });
}

SOSPeerInfoRef SOSCircleCopyRejectedApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    return (SOSPeerInfoRef) CFArrayGetValueMatching(circle->rejected_applicants, ^bool(const void *value) {
        return SOSPeerInfoCompareByID(value, peerInfo, NULL) == 0;
    });
}

CFMutableArrayRef SOSCircleCopyRejectedApplicants(SOSCircleRef circle, CFAllocatorRef allocator) {
    SOSCircleAssertStable(circle);
    
    return CFArrayCreateMutableCopy(allocator, 0, circle->rejected_applicants);
}


bool SOSCircleHasPeerWithID(SOSCircleRef circle, CFStringRef peerid, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    __block bool found = false;
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        if(peerid && peer && CFEqualSafe(peerid, SOSPeerInfoGetPeerID(peer))) found = true;
    });
    return found;
}

bool SOSCircleHasPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
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



bool SOSCircleResetToEmpty(SOSCircleRef circle, CFErrorRef *error) {
    CFArrayRemoveAllValues(circle->applicants);
    CFArrayRemoveAllValues(circle->peers);
    CFDictionaryRemoveAllValues(circle->signatures);

    return true;
}

bool SOSCircleResetToOffering(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error){

    return SOSCircleResetToEmpty(circle, error)
        && SOSCircleRequestAdmission(circle, user_privkey, requestor, error)
        && SOSCircleAcceptRequest(circle, user_privkey, requestor, SOSFullPeerInfoGetPeerInfo(requestor), error);
}

CFIndex SOSCircleRemoveRetired(SOSCircleRef circle, CFErrorRef *error) {
    CFIndex n = CFArrayRemoveAllPassing(circle->peers,  ^ bool (const void *element) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) element;
        
        return SOSPeerInfoIsRetirementTicket(peer);
    });
    
    return n;
}

static bool SOSCircleRecordAdmission(SOSCircleRef circle, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    
    bool isPeer = SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(requestor), error);
    
    require_action_quiet(!isPeer, fail, SOSCreateError(kSOSErrorAlreadyPeer, CFSTR("Cannot request admission when already a peer"), NULL, error));
    
    CFIndex total = CFArrayRemoveAllWithMatchingID(circle->applicants, SOSFullPeerInfoGetPeerInfo(requestor));
    
    (void) total; // Suppress unused warning in release code.
    assert(total <= 1); // We should at most be in the list once.

    total = CFArrayRemoveAllWithMatchingID(circle->rejected_applicants, SOSFullPeerInfoGetPeerInfo(requestor));
    
    (void) total; // Suppress unused warning in release code.
    assert(total <= 1); // We should at most be in the list once.
   
    
    // Refetch the current PeerInfo as the promtion above can change it.
    CFArrayAppendValue(circle->applicants, SOSFullPeerInfoGetPeerInfo(requestor));
    
    return true;
    
fail:
    return false;
    
}

bool SOSCircleRequestReadmission(SOSCircleRef circle, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool success = false;
    
    SOSPeerInfoRef peer = SOSFullPeerInfoGetPeerInfo(requestor);
    require_quiet(SOSPeerInfoApplicationVerify(peer, user_pubkey, error), fail);
    success = SOSCircleRecordAdmission(circle, user_pubkey, requestor, error);
fail:
    return success;
}

bool SOSCircleRequestAdmission(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool success = false;
    
    SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_action_quiet(user_pubkey, fail, SOSCreateError(kSOSErrorBadKey, CFSTR("No public key for key"), NULL, error));

    require(SOSFullPeerInfoPromoteToApplication(requestor, user_privkey, error), fail);
    
    success = SOSCircleRecordAdmission(circle, user_pubkey, requestor, error);
fail:
    CFReleaseNull(user_pubkey);
    return success;
}


bool SOSCircleUpdatePeerInfo(SOSCircleRef circle, SOSPeerInfoRef replacement_peer_info) {
    __block bool replaced = false;
    CFStringRef replacement_peer_id = SOSPeerInfoGetPeerID(replacement_peer_info);
    
    CFMutableArrayModifyValues(circle->peers, ^const void *(const void *value) {
        if (CFEqual(replacement_peer_id, SOSPeerInfoGetPeerID((SOSPeerInfoRef) value))
         && !CFEqual(replacement_peer_info, value)) {
            replaced = true;
            return replacement_peer_info;
        }
      
        return value;
    });
    return replaced;
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

    CFArrayRemoveAllWithMatchingID(circle->peers, peer_to_remove);

    SOSCircleGenerationSign(circle, user_privkey, requestor, error);

    return true;
}

bool SOSCircleAcceptRequest(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    CFIndex total = CFArrayRemoveAllWithMatchingID(circle->applicants, peerInfo);
    SecKeyRef publicKey = NULL;
    bool result = false;

    require_action_quiet(total != 0, fail, 
                         SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot accept non-applicant"), NULL, error));
    
    publicKey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_quiet(SOSPeerInfoApplicationVerify(peerInfo, publicKey, error), fail);

    assert(total == 1);
    
    CFArrayAppendValue(circle->peers, peerInfo);
    result = SOSCircleGenerationSign(circle, user_privkey, device_approver, error);
    secnotice("circle", "Accepted %@", peerInfo);

fail:
    CFReleaseNull(publicKey);
    return result;
}

bool SOSCircleWithdrawRequest(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

#ifndef NDEBUG
    CFIndex total =
#endif
        CFArrayRemoveAllWithMatchingID(circle->applicants, peerInfo);

    assert(total <= 1);
    
    return true;
}

bool SOSCircleRemoveRejectedPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);
    
#ifndef NDEBUG
    CFIndex total =
#endif
    CFArrayRemoveAllWithMatchingID(circle->rejected_applicants, peerInfo);
    
    assert(total <= 1);
    
    return true;
}


bool SOSCircleRejectRequest(SOSCircleRef circle, SOSFullPeerInfoRef device_rejector,
                            SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    if (CFEqual(SOSPeerInfoGetPeerID(peerInfo), SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(device_rejector))))
        return SOSCircleWithdrawRequest(circle, peerInfo, error);

	CFIndex total = CFArrayRemoveAllWithMatchingID(circle->applicants, peerInfo);
    
    if (total == 0) {
        SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot reject non-applicant"), NULL, error);
        return false;
    }
    assert(total == 1);
    
    CFArrayAppendValue(circle->rejected_applicants, peerInfo);
    
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

SOSPeerInfoRef SOSCircleCopyPeerInfo(SOSCircleRef circle, CFStringRef peer_id, CFErrorRef *error) {
    __block SOSPeerInfoRef result = NULL;

    CFArrayForEach(circle->peers, ^(const void *value) {
        if (result == NULL) {
            SOSPeerInfoRef tpi = (SOSPeerInfoRef)value;
            if (CFEqual(SOSPeerInfoGetPeerID(tpi), peer_id))
                result = tpi;
        }
    });

    CFRetainSafe(result);
    return result;
}


static inline void SOSCircleForEachPeerMatching(SOSCircleRef circle,
                                                void (^action)(SOSPeerInfoRef peer),
                                                bool (^condition)(SOSPeerInfoRef peer)) {
    CFArrayForEach(circle->peers, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
        if (condition(peer))
            action(peer);
    });
}

void SOSCircleForEachPeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer)) {
    SOSCircleForEachPeerMatching(circle, action, ^bool(SOSPeerInfoRef peer) {
        return !SOSPeerInfoIsRetirementTicket(peer) && !SOSPeerInfoIsCloudIdentity(peer);
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
    CFArrayForEach(circle->applicants, ^(const void*value) { action((SOSPeerInfoRef) value); } );
}


CFMutableArrayRef SOSCircleCopyPeers(SOSCircleRef circle, CFAllocatorRef allocator) {
    SOSCircleAssertStable(circle);
    
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(allocator);
    
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        CFArrayAppendValue(result, peer);
    });
    
    return result;
}

CFMutableArrayRef SOSCircleCopyConcurringPeers(SOSCircleRef circle, CFErrorRef* error) {
    SOSCircleAssertStable(circle);

    CFMutableArrayRef concurringPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        CFErrorRef error = NULL;
        if (SOSCircleVerifyPeerSigned(circle, peer, &error)) {
            CFArrayAppendValue(concurringPeers, peer);
        } else if (error != NULL) {
            secerror("Error checking concurrence: %@", error);
        }
        CFReleaseNull(error);
    });

    return concurringPeers;
}


//
// Stuff above this line is really SOSCircleInfo below the line is the active SOSCircle functionality
//

static SOSPeerRef SOSCircleCopyPeer(SOSCircleRef circle, SOSFullPeerInfoRef myRef, SOSPeerSendBlock sendBlock,
                                    CFStringRef peer_id, CFErrorRef *error) {
    SOSPeerRef peer = NULL;
    SOSPeerInfoRef peer_info = SOSCircleCopyPeerInfo(circle, peer_id, error);
    //TODO: if (peer is legit member of us then good otherwise bail) {
    //}
    if (peer_info) {
        peer = SOSPeerCreate(myRef, peer_info, error, sendBlock);
        CFReleaseNull(peer_info);
    }
    return peer;
}


static bool SOSCircleDoWithPeer(SOSFullPeerInfoRef myRef, SOSCircleRef circle, SOSDataSourceFactoryRef factory,
                                SOSPeerSendBlock sendBlock, CFStringRef peer_id, bool readOnly,
                                CFErrorRef* error, bool (^do_action)(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error))
{
    bool success = false;
    SOSEngineRef engine = NULL;
    SOSPeerRef peer = NULL;
    SOSDataSourceRef ds = NULL;

    peer = SOSCircleCopyPeer(circle, myRef, sendBlock, peer_id, error);
    require(peer, exit);

    ds = factory->create_datasource(factory, SOSCircleGetName(circle), readOnly, error);
    require(ds, exit);

    engine = SOSEngineCreate(ds, error); // Hand off DS to engine.
    ds = NULL;
    require(engine, exit);

    success = do_action(engine, peer, error);

exit:
    if (ds)
        ds->release(ds);
    if (engine)
        SOSEngineDispose(engine);
    if (peer)
        SOSPeerDispose(peer);

    return success;
}

bool SOSCircleSyncWithPeer(SOSFullPeerInfoRef myRef, SOSCircleRef circle, SOSDataSourceFactoryRef factory,
                           SOSPeerSendBlock sendBlock, CFStringRef peer_id,
                           CFErrorRef *error)
{
    return SOSCircleDoWithPeer(myRef, circle, factory, sendBlock, peer_id, true, error, ^bool(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
        return SOSPeerStartSync(peer, engine, error) != kSOSPeerCoderFailure;
    });
}

bool SOSCircleHandlePeerMessage(SOSCircleRef circle, SOSFullPeerInfoRef myRef, SOSDataSourceFactoryRef factory,
                                SOSPeerSendBlock sendBlock, CFStringRef peer_id,
                                CFDataRef message, CFErrorRef *error) {
    return SOSCircleDoWithPeer(myRef, circle, factory, sendBlock, peer_id, false, error, ^bool(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
        return SOSPeerHandleMessage(peer, engine, message, error) != kSOSPeerCoderFailure;
    });
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



