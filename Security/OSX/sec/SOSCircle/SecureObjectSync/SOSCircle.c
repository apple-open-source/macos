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
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSGenCount.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSGenCount.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

#include <utilities/SecCFWrappers.h>
#include <Security/SecureObjectSync/SOSCirclePriv.h>

//#include "ckdUtilities.h"

#include <corecrypto/ccder.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>

#include <stdlib.h>
#include <assert.h>

CFGiblisWithCompareFor(SOSCircle);

SOSCircleRef SOSCircleCreate(CFAllocatorRef allocator, CFStringRef name, CFErrorRef *error) {
    SOSCircleRef c = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);
    assert(name);
    
    c->name = CFStringCreateCopy(allocator, name);
    c->generation = SOSGenerationCreate();
    c->peers = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    c->applicants = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    c->rejected_applicants = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    c->signatures = CFDictionaryCreateMutableForCFTypes(allocator);
    return c;
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
    c->generation = SOSGenerationCopy(otherCircle->generation);

    c->peers = CFSetOfPeerInfoDeepCopy(allocator, otherCircle->peers);
    c->applicants = CFSetOfPeerInfoDeepCopy(allocator, otherCircle->applicants);
    c->rejected_applicants = CFSetOfPeerInfoDeepCopy(allocator, otherCircle->rejected_applicants);

    c->signatures = CFDictionaryCreateMutableCopy(allocator, 0, otherCircle->signatures);
    
    return c;
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

static bool SOSCircleConcordanceRingSign(SOSCircleRef circle, SecKeyRef privKey, CFErrorRef *error) {
    secnotice("Development", "SOSCircleEnsureRingConsistency requires ring signing op", NULL);
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
            CFSetTransferObject(pi, circle->applicants, circle->rejected_applicants);
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
    secnotice("circle", "SOSCircleGenerationSign: Upgraded peer's Application Signature");
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

static bool SOSCircleEnsureRingConsistency(SOSCircleRef circle, CFErrorRef *error) {
    secnotice("Development", "SOSCircleEnsureRingConsistency requires ring membership and generation count consistency check", NULL);
    return true;
}

bool SOSCircleSignOldStyleResetToOfferingCircle(SOSCircleRef circle, SOSFullPeerInfoRef peerinfo, SecKeyRef user_approver, CFErrorRef *error){
    
    SecKeyRef ourKey = SOSFullPeerInfoCopyDeviceKey(peerinfo, error);
    SecKeyRef publicKey = NULL;
    require_quiet(ourKey, fail);
    
    // Check if we're using an invalid peerinfo for this op.  There are cases where we might not be "upgraded".
    require_quiet(SOSCircleUpgradePeerInfo(circle, user_approver, peerinfo), fail);
    SOSCircleRemoveRetired(circle, error); // Prune off retirees since we're signing this one
    CFSetRemoveAllValues(circle->rejected_applicants); // Dump rejects so we clean them up sometime.
    publicKey = SecKeyCreatePublicFromPrivate(user_approver);
    SOSCircleRejectNonValidApplicants(circle, publicKey);
    require_quiet(SOSCircleEnsureRingConsistency(circle, error), fail);
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


bool SOSCircleGenerationSign(SOSCircleRef circle, SecKeyRef user_approver, SOSFullPeerInfoRef peerinfo, CFErrorRef *error) {
    SecKeyRef publicKey = NULL;

    SOSCircleRemoveRetired(circle, error); // Prune off retirees since we're signing this one
    CFSetRemoveAllValues(circle->rejected_applicants); // Dump rejects so we clean them up sometime.
    publicKey = SecKeyCreatePublicFromPrivate(user_approver);
    SOSCircleRejectNonValidApplicants(circle, publicKey);
    SOSCircleGenerationIncrement(circle);
    require_quiet(SOSCircleEnsureRingConsistency(circle, error), fail);
    require_quiet(SOSCircleRemoveSignatures(circle, error), fail);

    if (SOSCircleCountPeers(circle) != 0) {
        SecKeyRef ourKey = SOSFullPeerInfoCopyDeviceKey(peerinfo, error);
        require_quiet(ourKey, fail);

        // Check if we're using an invalid peerinfo for this op.  There are cases where we might not be "upgraded".
        require_quiet(SOSCircleUpgradePeerInfo(circle, user_approver, peerinfo), fail);

        require_quiet(SOSCircleSign(circle, user_approver, error), fail);
        require_quiet(SOSCircleSign(circle, ourKey, error), fail);
        CFReleaseNull(ourKey);
    }

    CFReleaseNull(publicKey);
    return true;
    
fail:
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
    SOSCircleConcordanceRingSign(circle, ourKey, error);

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

static inline bool SOSCircleHasDegenerateGeneration(SOSCircleRef deGenCircle){
    int testPtr;
    CFNumberRef genCountTest = SOSCircleGetGeneration(deGenCircle);
    CFNumberGetValue(genCountTest, kCFNumberCFIndexType, &testPtr);
    return (testPtr== 0);
}


static inline bool SOSCircleIsDegenerateReset(SOSCircleRef deGenCircle){
    return SOSCircleHasDegenerateGeneration(deGenCircle) && SOSCircleIsEmpty(deGenCircle);
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

// Is proposed older than current?
static inline bool isOlderGeneration(SOSCircleRef current, SOSCircleRef proposed) {
    return CFNumberCompare(current->generation, proposed->generation, NULL) == kCFCompareGreaterThan;
}

static inline bool SOSCircleIsValidReset(SOSCircleRef current, SOSCircleRef proposed) {
    return (!isOlderGeneration(current, proposed)) && SOSCircleIsEmpty(proposed); // is current older or equal to  proposed and is proposed empty
}


bool SOSCircleSharedTrustedPeers(SOSCircleRef current, SOSCircleRef proposed, SOSPeerInfoRef me) {
    __block bool retval = false;
    SOSCircleForEachPeer(current, ^(SOSPeerInfoRef peer) {
        if(!CFEqual(me, peer) && SOSCircleHasPeer(proposed, peer, NULL)) retval = true;
    });
    return retval;
}


SOSConcordanceStatus SOSCircleConcordanceTrust(SOSCircleRef known_circle, SOSCircleRef proposed_circle,
                                               SecKeyRef known_pubkey, SecKeyRef user_pubkey,
                                               SOSPeerInfoRef me, CFErrorRef *error) {
    if(user_pubkey == NULL) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Concordance with no public key"), NULL, error);
        return kSOSConcordanceNoUserKey; //TODO: - needs to return an error
    }
    
    if(SOSCircleIsDegenerateReset(proposed_circle)) {
        return kSOSConcordanceTrusted;
    }

    if (SOSCircleIsValidReset(known_circle, proposed_circle)) {
        return kSOSConcordanceTrusted;
    }
    
    if(!SOSCircleVerifySignatureExists(proposed_circle, user_pubkey, error)) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("No public signature"), (error != NULL) ? *error : NULL, error);
        return kSOSConcordanceNoUserSig;
    }
    
    if(!SOSCircleVerify(proposed_circle, user_pubkey, error)) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("Bad public signature"), (error != NULL) ? *error : NULL, error);
        debugDumpCircle(CFSTR("proposed_circle"), proposed_circle);
        return kSOSConcordanceBadUserSig;
    }

    if (SOSCircleIsEmpty(known_circle)) {
        return GetSignersStatus(proposed_circle, proposed_circle, user_pubkey, NULL, error);
    }
    
    if(SOSCircleHasDegenerateGeneration(proposed_circle) && SOSCircleIsOffering(proposed_circle)){
        return GetSignersStatus(proposed_circle, proposed_circle, user_pubkey, NULL, error);
    }
    
    if(isOlderGeneration(known_circle, proposed_circle)) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Bad generation"), NULL, error);
        debugDumpCircle(CFSTR("isOlderGeneration known_circle"), known_circle);
        debugDumpCircle(CFSTR("isOlderGeneration proposed_circle"), proposed_circle);
        return kSOSConcordanceGenOld;
    }
    
    if(SOSCircleIsOffering(proposed_circle)){
        return GetSignersStatus(proposed_circle, proposed_circle, user_pubkey, NULL, error);
    }

    return GetSignersStatus(known_circle, proposed_circle, user_pubkey, me, error);
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

static CFMutableStringRef defaultDescription(CFTypeRef aObj){
    SOSCircleRef c = (SOSCircleRef) aObj;

    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);

    SOSGenerationCountWithDescription(c->generation, ^(CFStringRef genDescription) {
        CFStringAppendFormat(description, NULL, CFSTR("<SOSCircle@%p: '%@' %@ P:["), c, c->name, genDescription);
    });

    __block CFStringRef separator = CFSTR("");
    SOSCircleForEachActivePeer(c, ^(SOSPeerInfoRef peer) {
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
    
    //applicants
    CFStringAppend(description, CFSTR("], A:["));
    separator = CFSTR("");
    if(CFSetGetCount(c->applicants) == 0 )
        CFStringAppendFormat(description, NULL, CFSTR("-"));
    else{
        
        SOSCircleForEachApplicant(c, ^(SOSPeerInfoRef peer) {
            CFStringAppendFormat(description, NULL, CFSTR("%@%@"), separator, peer);
            separator = CFSTR(",");
        });
    }
    
    //rejected
    CFStringAppend(description, CFSTR("], R:["));
    separator = CFSTR("");
    if(CFSetGetCount(c->rejected_applicants) == 0)
        CFStringAppendFormat(description, NULL, CFSTR("-"));
    else{
        CFSetForEach(c->rejected_applicants, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
            CFStringAppendFormat(description, NULL, CFSTR("%@%@"), separator, peer);
            separator = CFSTR(",");
        });
    }
    CFStringAppend(description, CFSTR("]>"));
    return description;
    
}
static CFMutableStringRef descriptionWithFormatOptions(CFTypeRef aObj, CFDictionaryRef formatOptions){
    SOSCircleRef c = (SOSCircleRef) aObj;

    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);

    if(CFDictionaryContainsKey(formatOptions, CFSTR("SyncD"))) {
        CFStringRef generationDescription = SOSGenerationCountCopyDescription(c->generation);
        CFStringAppendFormat(description, NULL, CFSTR("<C: gen:'%@' %@>\n"), generationDescription, c->name);
        CFReleaseNull(generationDescription);
        __block CFStringRef separator = CFSTR("\t\t");
        SOSCircleForEachActivePeer(c, ^(SOSPeerInfoRef peer) {
            CFStringRef sig = NULL;
            if (SOSCircleVerifyPeerSigned(c, peer, NULL)) {
                sig = CFSTR("√");
            } else {
                SecKeyRef pub_key = SOSPeerInfoCopyPubKey(peer);
                CFDataRef signature = SOSCircleGetSignature(c, pub_key, NULL);
                sig = (signature == NULL) ? CFSTR("-") : CFSTR("?");
                CFReleaseNull(pub_key);
            }
            
            CFStringAppendFormat(description, formatOptions, CFSTR("%@%@ %@"), separator, peer, sig);
            separator = CFSTR("\n\t\t");
        });
        CFStringAppend(description, CFSTR("\n\t\t<A:["));
        separator = CFSTR("");
        
        //applicants list
        if(CFSetGetCount(c->applicants) == 0 )
            CFStringAppendFormat(description, NULL, CFSTR("-"));
        else{
            
            SOSCircleForEachApplicant(c, ^(SOSPeerInfoRef peer) {
                CFStringAppendFormat(description, formatOptions, CFSTR("%@A: %@"), separator, peer);
                separator = CFSTR("\n\t\t\t");
            });
        }
        //rejected list
        CFStringAppend(description, CFSTR("]> \n\t\t<R:["));
        separator = CFSTR("");
        if(CFSetGetCount(c->rejected_applicants) == 0)
            CFStringAppendFormat(description, NULL, CFSTR("-"));
        else{
            CFSetForEach(c->rejected_applicants, ^(const void *value) {
                SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
                CFStringAppendFormat(description, formatOptions, CFSTR("%@R: %@"), separator, peer);
                separator = CFSTR("\n\t\t");
            });
        }
        CFStringAppend(description, CFSTR("]>"));
    }

    else{
        CFReleaseNull(description);
        description = defaultDescription(aObj);
    }
    
    return description;
    
}


static CFStringRef SOSCircleCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSCircleRef c = (SOSCircleRef) aObj;
    SOSCircleAssertStable(c);
    CFMutableStringRef description = NULL;
    
    if(formatOptions != NULL){
        description = descriptionWithFormatOptions(aObj, formatOptions);
    }
    else{
        description = defaultDescription(aObj);
    }
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

SOSGenCountRef SOSCircleGetGeneration(SOSCircleRef circle) {
    assert(circle);
    assert(circle->generation);
    return circle->generation;
}

void SOSCircleSetGeneration(SOSCircleRef circle, SOSGenCountRef gencount) {
    assert(circle);
    CFReleaseNull(circle->generation);
    circle->generation = CFRetainSafe(gencount);
}

int64_t SOSCircleGetGenerationSint(SOSCircleRef circle) {
    SOSGenCountRef gen = SOSCircleGetGeneration(circle);
    return SOSGetGenerationSint(gen);
}

void SOSCircleGenerationSetValue(SOSCircleRef circle, int64_t value) {
    CFAssignRetained(circle->generation, SOSGenerationCreateWithValue(value));
}

void SOSCircleGenerationIncrement(SOSCircleRef circle) {
    SOSGenCountRef old = circle->generation;
    circle->generation = SOSGenerationIncrementAndCreate(old);
    CFReleaseNull(old);
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
    __block SOSPeerInfoRef found = NULL;
    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        if(peerid && peer && CFEqualSafe(peerid, SOSPeerInfoGetPeerID(peer))) found = peer;
    });
    return found ? SOSPeerInfoCreateCopy(kCFAllocatorDefault, found, NULL) : NULL;
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
    SOSGenCountRef oldGen = SOSCircleGetGeneration(circle);
    SOSGenCountRef newGen = SOSGenerationCreateWithBaseline(oldGen);
    SOSCircleSetGeneration(circle, newGen);
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
    
    CFSetTransferObject(requestorPeerInfo, circle->rejected_applicants, circle->applicants);
    
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

static bool sosCircleUpdatePeerInfoSet(CFMutableSetRef theSet, SOSPeerInfoRef replacement_peer_info) {
    CFTypeRef old = NULL;
    if(!replacement_peer_info) return false;
    if(!(old = CFSetGetValue(theSet, replacement_peer_info))) return false;
    if(CFEqualSafe(old, replacement_peer_info)) return false;
    CFSetReplaceValue(theSet, replacement_peer_info);
    return true;
}

bool SOSCircleUpdatePeerInfo(SOSCircleRef circle, SOSPeerInfoRef replacement_peer_info) {
    if(sosCircleUpdatePeerInfoSet(circle->peers, replacement_peer_info)) return true;
    if(sosCircleUpdatePeerInfoSet(circle->applicants, replacement_peer_info)) return true;
    if(sosCircleUpdatePeerInfoSet(circle->rejected_applicants, replacement_peer_info)) return true;
    return false;
}

static bool SOSCircleRemovePeerInternal(SOSCircleRef circle, SOSFullPeerInfoRef requestor, SOSPeerInfoRef peer_to_remove, CFErrorRef *error) {
    SOSPeerInfoRef requestor_peer_info = SOSFullPeerInfoGetPeerInfo(requestor);

    if (SOSCircleHasPeer(circle, peer_to_remove, NULL)) {
        if (!SOSCircleHasPeer(circle, requestor_peer_info, error)) {
            SOSCreateError(kSOSErrorAlreadyPeer, CFSTR("Must be peer to remove peer"), NULL, error);
            return false;
        }
        CFSetRemoveValue(circle->peers, peer_to_remove);
    }

    if (SOSCircleHasApplicant(circle, peer_to_remove, error)) {
        return SOSCircleRejectRequest(circle, requestor, peer_to_remove, error);
    }

    return true;
}

bool SOSCircleRemovePeers(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFSetRef peersToRemove, CFErrorRef *error) {

    bool success = false;

    __block bool removed_all = true;
    CFSetForEach(peersToRemove, ^(const void *value) {
        SOSPeerInfoRef peerInfo = asSOSPeerInfo(value);
        if (peerInfo) {
            removed_all &= SOSCircleRemovePeerInternal(circle, requestor, peerInfo, error);
        }
    });

    require_quiet(removed_all, exit);

    require_quiet(SOSCircleGenerationSign(circle, user_privkey, requestor, error), exit);

    success = true;

exit:
    return success;
}

bool SOSCircleRemovePeer(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, SOSPeerInfoRef peer_to_remove, CFErrorRef *error) {
    bool success = false;

    require_quiet(SOSCircleRemovePeerInternal(circle, requestor, peer_to_remove, error), exit);

    require_quiet(SOSCircleGenerationSign(circle, user_privkey, requestor, error), exit);

    success = true;
exit:
    return success;
}

bool SOSCircleAcceptRequest(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSCircleAssertStable(circle);

    SecKeyRef publicKey = NULL;
    bool result = false;

    require_action_quiet(CFSetContainsValue(circle->applicants, peerInfo), fail,
                         SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot accept non-applicant"), NULL, error));
    
    publicKey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_quiet(SOSPeerInfoApplicationVerify(peerInfo, publicKey, error), fail);

    CFSetTransferObject(peerInfo, circle->applicants, circle->peers);
    
    result = SOSCircleGenerationSign(circle, user_privkey, device_approver, error);

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
    
    CFSetTransferObject(peerInfo, circle->applicants, circle->rejected_applicants);
    
    // TODO: Maybe we sign the rejection with device_rejector.
    
    return true;
}

bool SOSCircleAcceptRequests(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver,
                             CFErrorRef *error) {
    // Returns true if we accepted someone and therefore have to post the circle back to KVS
    __block bool result = false;
    
    SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
        if (!SOSCircleAcceptRequest(circle, user_privkey, device_approver, peer, error)) {
            secnotice("circle", "error in SOSCircleAcceptRequest\n");
        } else {
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

void SOSCircleForEachValidPeer(SOSCircleRef circle, SecKeyRef user_public_key, void (^action)(SOSPeerInfoRef peer)) {
    SOSCircleForEachPeerMatching(circle, action, ^bool(SOSPeerInfoRef peer) {
        return !isHiddenPeer(peer) && SOSPeerInfoApplicationVerify(peer, user_public_key, NULL);
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
            SOSPeerInfoRef peerInfo = SOSPeerInfoCreateCopy(kCFAllocatorDefault, peer, error);
            CFArrayAppendValue(appendHere, peerInfo);
            CFRelease(peerInfo);
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

SOSFullPeerInfoRef SOSCircleCopyiCloudFullPeerInfoRef(SOSCircleRef circle, CFErrorRef *error) {
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;
    __block CFErrorRef searchError = NULL;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoIsCloudIdentity(peer)) {
            if (cloud_full_peer == NULL) {
                if (searchError) {
                    secerror("More than one cloud identity found, first had error, trying new one.");
                }
                CFReleaseNull(searchError);
                cloud_full_peer = SOSFullPeerInfoCreateCloudIdentity(kCFAllocatorDefault, peer, &searchError);
                if (!cloud_full_peer) {
                    secnotice("icloud-identity", "Failed to make FullPeer for iCloud Identity: %@ (%@)", cloud_full_peer, searchError);
                }
            } else {
                secerror("Additional cloud identity found in circle after successful creation: %@", circle);
            }
        }
    });
    // If we didn't find one at all, report the error.
    if (cloud_full_peer == NULL && searchError == NULL) {
        SOSErrorCreate(kSOSErrorNoiCloudPeer, &searchError, NULL, CFSTR("No iCloud identity PeerInfo found in circle"));
        secnotice("icloud-identity", "No iCloud identity PeerInfo found in circle");
    }
    if (error) {
        CFTransferRetained(*error, searchError);
    }
    CFReleaseNull(searchError);
    return cloud_full_peer;
}

void debugDumpCircle(CFStringRef message, SOSCircleRef circle) {
    CFErrorRef error;

    secinfo("circledebug", "%@: %@", message, circle);
    if (!circle)
        return;

    CFDataRef derdata = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, &error);
    if (derdata) {
        CFStringRef hex = CFDataCopyHexString(derdata);
        secinfo("circledebug", "Full contents: %@", hex);
        if (hex) CFRelease(hex);
        CFRelease(derdata);
    }
}
