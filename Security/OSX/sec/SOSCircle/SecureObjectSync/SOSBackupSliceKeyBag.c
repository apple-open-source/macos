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


// Our Header

#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>


// Needed headers for implementation
#include "AssertMacros.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAKSWrappers.h>
#include <utilities/SecBuffer.h>
#include <utilities/SecCFError.h>
#include <utilities/der_set.h>
#include <utilities/der_plist_internal.h>
#include <Security/SecRandom.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <corecrypto/ccec.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccrng.h>

#include <limits.h>

#include "SecRecoveryKey.h"
#include "SOSKeyedPubKeyIdentifier.h"
#include "SOSInternal.h"
#include "SecADWrapper.h"

CFStringRef bskbRkbgPrefix = CFSTR("RK");

//
// MARK: Type creation
//

CFGiblisFor(SOSBackupSliceKeyBag);

const int kAKSBagSecretLength = 32;

struct __OpaqueSOSBackupSliceKeyBag {
    CFRuntimeBase           _base;

    CFDataRef               aks_bag;

    CFSetRef                peers;
    CFDictionaryRef         wrapped_keys;
};

static void SOSBackupSliceKeyBagDestroy(CFTypeRef aObj) {
    SOSBackupSliceKeyBagRef vb = (SOSBackupSliceKeyBagRef) aObj;

    CFReleaseNull(vb->aks_bag);
    CFReleaseNull(vb->peers);
    CFReleaseNull(vb->wrapped_keys);
}

static CFSetRef SOSBackupSliceKeyBagCopyPeerNames(SOSBackupSliceKeyBagRef bksb) {
    CFMutableSetRef retval = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    if(!retval) return NULL;
    CFSetForEach(bksb->peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        CFSetAddValue(retval, SOSPeerInfoGetPeerName(pi));
    });
    return retval;
}

static CFStringRef SOSBackupSliceKeyBagCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSBackupSliceKeyBagRef vb = (SOSBackupSliceKeyBagRef) aObj;

    CFMutableStringRef retval = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    CFSetRef peerIDs = SOSBackupSliceKeyBagCopyPeerNames(vb);
    CFStringSetPerformWithDescription(peerIDs, ^(CFStringRef description) {
        CFStringAppendFormat(retval, NULL, CFSTR("<SOSBackupSliceKeyBag@%p %ld %@"), vb, vb->peers ? CFSetGetCount(vb->peers) : 0, description);
    });
    CFReleaseNull(peerIDs);
    CFStringAppend(retval, CFSTR(">"));

    return retval;
}


//
// MARK: Encode/Decode
//

const uint8_t* der_decode_BackupSliceKeyBag(CFAllocatorRef allocator,
                                            SOSBackupSliceKeyBagRef* BackupSliceKeyBag, CFErrorRef *error,
                                            const uint8_t* der, const uint8_t *der_end) {
    if (der == NULL) return der;

    const uint8_t *result = NULL;
    SOSBackupSliceKeyBagRef vb = CFTypeAllocate(SOSBackupSliceKeyBag, struct __OpaqueSOSBackupSliceKeyBag, allocator);
    require_quiet(SecAllocationError(vb, error, CFSTR("View bag allocation failed")), fail);

    const uint8_t *sequence_end = NULL;
    der = ccder_decode_sequence_tl(&sequence_end, der, der_end);
    require_quiet(sequence_end == der_end, fail);

    der = der_decode_data(kCFAllocatorDefault, kCFPropertyListImmutable, &vb->aks_bag, error, der, sequence_end);
    vb->peers = SOSPeerInfoSetCreateFromArrayDER(kCFAllocatorDefault, &kSOSPeerSetCallbacks, error,
                                                 &der, der_end);
    der = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &vb->wrapped_keys, error, der, sequence_end);

    require_quiet(SecRequirementError(der == der_end, error, CFSTR("Extra space in sequence")), fail);

    if (BackupSliceKeyBag)
        CFTransferRetained(*BackupSliceKeyBag, vb);

    result = der;

fail:
    CFReleaseNull(vb);
    return result;
}

size_t der_sizeof_BackupSliceKeyBag(SOSBackupSliceKeyBagRef BackupSliceKeyBag, CFErrorRef *error) {
    size_t result = 0;

    require_quiet(SecRequirementError(BackupSliceKeyBag != NULL, error, CFSTR("Null BackupSliceKeyBag")), fail);
    require_quiet(BackupSliceKeyBag != NULL, fail); // this is redundant with what happens in SecRequirementError, but the analyzer can't understand that.
    require_quiet(SecRequirementError(BackupSliceKeyBag->aks_bag != NULL, error, CFSTR("null aks_bag in BackupSliceKeyBag")), fail);
    require_quiet(BackupSliceKeyBag->aks_bag != NULL, fail); // this is redundant with what happens in SecRequirementError, but the analyzer can't understand that.

    size_t bag_size = der_sizeof_data(BackupSliceKeyBag->aks_bag, error);
    require_quiet(bag_size, fail);

    size_t peers_size = SOSPeerInfoSetGetDEREncodedArraySize(BackupSliceKeyBag->peers, error);
    require_quiet(peers_size, fail);

    size_t wrapped_keys_size = der_sizeof_dictionary(BackupSliceKeyBag->wrapped_keys, error);
    require_quiet(wrapped_keys_size, fail);

    result = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, bag_size + peers_size + wrapped_keys_size);

fail:
    return result;
}

uint8_t* der_encode_BackupSliceKeyBag(SOSBackupSliceKeyBagRef set, CFErrorRef *error,
                                      const uint8_t *der, uint8_t *der_end) {
    uint8_t *result = NULL;
    if (der_end == NULL) return der_end;

    require_quiet(SecRequirementError(set != NULL, error, CFSTR("Null set passed to encode")), fail);
    require_quiet(set, fail); // Silence the NULL warning.

    require_quiet(SecRequirementError(set->aks_bag != NULL, error, CFSTR("Null set passed to encode")), fail);
    require_quiet(set->aks_bag, fail); // Silence the warning.

    der_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
              der_encode_data(set->aks_bag, error, der,
              SOSPeerInfoSetEncodeToArrayDER(set->peers, error, der,
              der_encode_dictionary(set->wrapped_keys, error, der, der_end))));

    require_quiet(der_end == der, fail);

    result = der_end;
fail:
    return result;
}



SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error) {
    SOSBackupSliceKeyBagRef result = NULL;
    SOSBackupSliceKeyBagRef decodedBag = NULL;

    const uint8_t *der = CFDataGetBytePtr(data);
    const uint8_t *der_end = der + CFDataGetLength(data);

    der = der_decode_BackupSliceKeyBag(allocator, &decodedBag, error, der, der_end);

    require_quiet(SecRequirementError(der == der_end, error, CFSTR("Didn't consume all data supplied")), fail);

    CFTransferRetained(result, decodedBag);

fail:
    CFReleaseNull(decodedBag);
    return result;
}

//
// MARK: Construction
//

bool SOSBSKBIsGoodBackupPublic(CFDataRef publicKey, CFErrorRef *error) {
    bool result = false;

    ccec_pub_ctx_decl_cp(SOSGetBackupKeyCurveParameters(), pub_key);

    int cc_result = ccec_compact_import_pub(SOSGetBackupKeyCurveParameters(), CFDataGetLength(publicKey), CFDataGetBytePtr(publicKey), pub_key);

    require_action_quiet(cc_result == 0, exit, SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unable to decode public key: %@"), publicKey));

    result = true;
exit:
    return result;

}


static CFDataRef SOSCopyECWrapped(CFDataRef publicKey, CFDataRef secret, CFErrorRef *error) {
    CFDataRef result = NULL;

    ccec_pub_ctx_decl_cp(SOSGetBackupKeyCurveParameters(), pub_key);

    int cc_result = ccec_compact_import_pub(SOSGetBackupKeyCurveParameters(), CFDataGetLength(publicKey), CFDataGetBytePtr(publicKey), pub_key);

    require_action_quiet(cc_result == 0, exit, SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unable to decode public key: %@"), publicKey));

    result = SOSCopyECWrappedData(pub_key, secret, error);

exit:
    return result;
}


static CFDictionaryRef SOSBackupSliceKeyBagCopyWrappedKeys(SOSBackupSliceKeyBagRef vb, CFDataRef secret, CFDictionaryRef additionalKeys, CFErrorRef *error) {
    CFDictionaryRef result = NULL;
    CFMutableDictionaryRef wrappedKeys = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    __block bool success = true;
    CFSetForEach(vb->peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        if (isSOSPeerInfo(pi)) {
            CFStringRef id = SOSPeerInfoGetPeerID(pi);
            CFDataRef backupKey = SOSPeerInfoCopyBackupKey(pi);

            if (backupKey) {
                CFErrorRef wrapError = NULL;
                CFDataRef wrappedKey = SOSCopyECWrapped(backupKey, secret, &wrapError);
                if (wrappedKey) {
                    CFDictionaryAddValue(wrappedKeys, id, wrappedKey);
                    CFDataPerformWithHexString(backupKey, ^(CFStringRef backupKeyString) {
                        CFDataPerformWithHexString(wrappedKey, ^(CFStringRef wrappedKeyString) {
                            secnotice("bskb", "Add for id: %@, bk: %@, wrapped: %@", id, backupKeyString, wrappedKeyString);
                        });
                    });
                } else {
                    CFDataPerformWithHexString(backupKey, ^(CFStringRef backupKeyString) {
                        secnotice("bskb", "Failed at id: %@, bk: %@ error: %@", id, backupKeyString, wrapError);
                    });
                    CFErrorPropagate(wrapError, error);
                    success = false;
                }
                CFReleaseNull(wrappedKey);
                CFReleaseNull(backupKey);
            } else {
                secnotice("bskb", "Skipping id %@, no backup key.", id);
            }

        }
    });

    CFDictionaryForEach(additionalKeys, ^(const void *key, const void *value) {
        CFStringRef prefix = asString(key, NULL);
        CFDataRef backupKey = asData(value, NULL);
        if (backupKey) {
            CFDataRef wrappedKey = NULL;
            CFErrorRef localError = NULL;
            CFStringRef id = SOSKeyedPubKeyIdentifierCreateWithData(prefix, backupKey);
            require_quiet(id, done);

            wrappedKey = SOSCopyECWrapped(backupKey, secret, &localError);
            require_quiet(wrappedKey, done);

            CFDictionaryAddValue(wrappedKeys, id, wrappedKey);

        done:
            if (!localError) {
                CFDataPerformWithHexString(backupKey, ^(CFStringRef backupKeyString) {
                    CFDataPerformWithHexString(wrappedKey, ^(CFStringRef wrappedKeyString) {
                        secnotice("bskb", "Add for bk: %@, wrapped: %@", backupKeyString, wrappedKeyString);
                    });
                });
            } else {
                CFDataPerformWithHexString(backupKey, ^(CFStringRef backupKeyString) {
                    secnotice("bskb", "Failed at bk: %@ error: %@", backupKeyString, localError);
                });
                CFErrorPropagate(localError, error);
                success = false;
            }
            CFReleaseNull(wrappedKey);
            CFReleaseNull(id);
        } else {
            secnotice("bskb", "Skipping %@, not data.", value);
        }
    });

    if (success)
        CFTransferRetained(result, wrappedKeys);

    CFReleaseNull(wrappedKeys);
    return result;
}

static bool SOSBackupSliceKeyBagCreateBackupBag(SOSBackupSliceKeyBagRef vb, CFDictionaryRef/*CFDataRef*/ additionalKeys, CFErrorRef* error) {
    CFReleaseNull(vb->aks_bag);

    // Choose a random key.
    PerformWithBufferAndClear(kAKSBagSecretLength, ^(size_t size, uint8_t *buffer) {
        CFDataRef secret = NULL;

        require_quiet(SecError(SecRandomCopyBytes(kSecRandomDefault, size, buffer), error, CFSTR("SecRandom falied!")), fail);

        secret = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer, size, kCFAllocatorNull);

        CFAssignRetained(vb->wrapped_keys, SOSBackupSliceKeyBagCopyWrappedKeys(vb, secret, additionalKeys, error));
        CFAssignRetained(vb->aks_bag, SecAKSCopyBackupBagWithSecret(size, buffer, error));

    fail:
        CFReleaseSafe(secret);
    });

    return vb->aks_bag != NULL;
}


CFDataRef SOSBSKBCopyEncoded(SOSBackupSliceKeyBagRef BackupSliceKeyBag, CFErrorRef* error) {
    CFDataRef result = NULL;
    CFMutableDataRef encoded = NULL;

    size_t encodedSize = der_sizeof_BackupSliceKeyBag(BackupSliceKeyBag, error);
    require_quiet(encodedSize, fail);

    encoded = CFDataCreateMutableWithScratch(kCFAllocatorDefault, encodedSize);
    require_quiet(SecAllocationError(encoded, error, CFSTR("Faild to create scratch")), fail);

    uint8_t *encode_to = CFDataGetMutableBytePtr(encoded);
    uint8_t *encode_to_end = encode_to + CFDataGetLength(encoded);
    require_quiet(encode_to == der_encode_BackupSliceKeyBag(BackupSliceKeyBag, error, encode_to, encode_to_end), fail);

    CFTransferRetained(result, encoded);

fail:
    CFReleaseSafe(encoded);
    return result;
}

static CFSetRef SOSBackupSliceKeyBagCreatePeerSet(CFAllocatorRef allocator, CFSetRef peers) {
    CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(allocator);

    CFSetForEach(peers, ^(const void *value) {
        CFSetAddValue(result, value);
    });

    return result;
}

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreate(CFAllocatorRef allocator, CFSetRef peers, CFErrorRef* error) {
    CFMutableDictionaryRef additionalKeys = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSBackupSliceKeyBagRef result = SOSBackupSliceKeyBagCreateWithAdditionalKeys(allocator, peers, additionalKeys, NULL);

    CFReleaseNull(additionalKeys);

    return result;
}

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreateWithAdditionalKeys(CFAllocatorRef allocator,
                                                                     CFSetRef /*SOSPeerInfoRef*/ peers,
                                                                     CFDictionaryRef /*CFStringRef (prefix) CFDataRef (keydata) */ additionalKeys,
                                                                     CFErrorRef* error) {
    SOSBackupSliceKeyBagRef result = NULL;
    SOSBackupSliceKeyBagRef vb = CFTypeAllocate(SOSBackupSliceKeyBag, struct __OpaqueSOSBackupSliceKeyBag, allocator);
    require_quiet(SecAllocationError(vb, error, CFSTR("View bag allocation failed")), fail);

    require_quiet(SecRequirementError(CFSetGetCount(peers) > 0, error, CFSTR("Need peers")), fail);

    vb->peers = SOSBackupSliceKeyBagCreatePeerSet(allocator, peers);
    vb->wrapped_keys = CFDictionaryCreateMutableForCFTypes(allocator);

    require_quiet(SOSBackupSliceKeyBagCreateBackupBag(vb, additionalKeys, error), fail);

    CFTransferRetained(result, vb);

fail:
    CFReleaseNull(vb);
    return result;
}


SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreateDirect(CFAllocatorRef allocator, CFDataRef aks_bag, CFErrorRef *error)
{
    SOSBackupSliceKeyBagRef result = NULL;
    SOSBackupSliceKeyBagRef vb = CFTypeAllocate(SOSBackupSliceKeyBag, struct __OpaqueSOSBackupSliceKeyBag, allocator);
    require_quiet(SecAllocationError(vb, error, CFSTR("View bag allocation failed")), fail);

    require_quiet(SecRequirementError(aks_bag, error, CFSTR("Need aks bag")), fail);

    vb->aks_bag = CFRetainSafe(aks_bag);
    vb->peers = CFSetCreateMutableForSOSPeerInfosByID(allocator);
    vb->wrapped_keys = CFDictionaryCreateMutableForCFTypes(allocator);

    CFTransferRetained(result, vb);

fail:
    CFReleaseNull(vb);
    return result;
}

//
// MARK: Use
//

bool SOSBSKBIsDirect(SOSBackupSliceKeyBagRef backupSliceKeyBag)
{
    return 0 == CFSetGetCount(backupSliceKeyBag->peers);
}

CFDataRef SOSBSKBCopyAKSBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFErrorRef* error)
{
    return CFRetainSafe(backupSliceKeyBag->aks_bag);
}

CFSetRef SOSBSKBGetPeers(SOSBackupSliceKeyBagRef backupSliceKeyBag){
    return backupSliceKeyBag->peers;
}

int SOSBSKBCountPeers(SOSBackupSliceKeyBagRef backupSliceKeyBag) {
    return (int) CFSetGetCount(backupSliceKeyBag->peers);
}

bool SOSBSKBPeerIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, SOSPeerInfoRef pi) {
    return CFSetGetValue(backupSliceKeyBag->peers, pi) != NULL;
}



bool SOSBKSBKeyIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFDataRef publicKey) {
    bool result = false;
    CFStringRef keyID = SOSCopyIDOfDataBuffer(publicKey, NULL);
    require_quiet(keyID, done);

    result = CFDictionaryContainsKey(backupSliceKeyBag->wrapped_keys, keyID);

done:
    CFReleaseSafe(keyID);
    return result;
}

bool SOSBKSBPrefixedKeyIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFStringRef prefix, CFDataRef publicKey) {
    bool result = false;
    CFStringRef kpkid = SOSKeyedPubKeyIdentifierCreateWithData(prefix, publicKey);
    require_quiet(kpkid, done);
    
    result = CFDictionaryContainsKey(backupSliceKeyBag->wrapped_keys, kpkid);
    
done:
    CFReleaseSafe(kpkid);
    return result;

}



bskb_keybag_handle_t SOSBSKBLoadLocked(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                       CFErrorRef *error)
{
#if !TARGET_HAS_KEYSTORE
    return bad_keybag_handle;
#else
    keybag_handle_t result = bad_keybag_handle;
    keybag_handle_t bag_handle = bad_keybag_handle;

    require_quiet(SecRequirementError(backupSliceKeyBag->aks_bag, error,
                                      CFSTR("No aks bag to load")), exit);
    require_quiet(SecRequirementError(CFDataGetLength(backupSliceKeyBag->aks_bag) < INT_MAX, error,
                                      CFSTR("No aks bag to load")), exit);

    kern_return_t aks_result;
    aks_result = aks_load_bag(CFDataGetBytePtr(backupSliceKeyBag->aks_bag),
                              (int) CFDataGetLength(backupSliceKeyBag->aks_bag),
                              &bag_handle);
    require_quiet(SecKernError(aks_result, error,
                               CFSTR("aks_load_bag failed: %d"), aks_result), exit);

    result = bag_handle;
    bag_handle = bad_keybag_handle;

exit:
    if (bag_handle != bad_keybag_handle) {
        (void) aks_unload_bag(bag_handle);
    }

    return result;
#endif

}

static keybag_handle_t SOSBSKBLoadAndUnlockBagWithSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                           size_t secretSize, const uint8_t *secret,
                                                           CFErrorRef *error)
{
#if !TARGET_HAS_KEYSTORE
    return bad_keybag_handle;
#else
    keybag_handle_t result = bad_keybag_handle;
    keybag_handle_t bag_handle = bad_keybag_handle;

    require_quiet(SecRequirementError(backupSliceKeyBag->aks_bag, error,
                                      CFSTR("No aks bag to load")), exit);
    require_quiet(SecRequirementError(CFDataGetLength(backupSliceKeyBag->aks_bag) < INT_MAX, error,
                                      CFSTR("No aks bag to load")), exit);
    require_quiet(SecRequirementError(secretSize <= INT_MAX, error,
                                      CFSTR("secret too big")), exit);

    kern_return_t aks_result;
    aks_result = aks_load_bag(CFDataGetBytePtr(backupSliceKeyBag->aks_bag),
                              (int) CFDataGetLength(backupSliceKeyBag->aks_bag),
                              &bag_handle);
    require_quiet(SecKernError(aks_result, error,
                               CFSTR("aks_load_bag failed: %d"), aks_result), exit);

    aks_result = aks_unlock_bag(bag_handle, secret, (int) secretSize);
    require_quiet(SecKernError(aks_result, error,
                               CFSTR("failed to unlock bag: %d"), aks_result), exit);

    result = bag_handle;
    bag_handle = bad_keybag_handle;

exit:
    if (bag_handle != bad_keybag_handle) {
        (void) aks_unload_bag(bag_handle);
    }

    return result;
#endif
}

keybag_handle_t SOSBSKBLoadAndUnlockWithPeerIDAndSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                        CFStringRef peerID, CFDataRef peerSecret,
                                                        CFErrorRef *error)
{
    __block keybag_handle_t result = bad_keybag_handle;

    CFDataRef lookedUpData = CFDictionaryGetValue(backupSliceKeyBag->wrapped_keys, peerID);
    require_quiet(SecRequirementError(lookedUpData != NULL, error, CFSTR("%@ has no wrapped key in %@"), peerID, backupSliceKeyBag), exit);

    require_quiet(SOSPerformWithDeviceBackupFullKey(SOSGetBackupKeyCurveParameters(), peerSecret, error, ^(ccec_full_ctx_t fullKey) {
        SOSPerformWithUnwrappedData(fullKey, lookedUpData, error, ^(size_t size, uint8_t *buffer) {
            result = SOSBSKBLoadAndUnlockBagWithSecret(backupSliceKeyBag, size, buffer, error);
        });
    }), exit);

exit:
    return result;
}


keybag_handle_t SOSBSKBLoadAndUnlockWithPeerSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                   SOSPeerInfoRef peer, CFDataRef peerSecret,
                                                   CFErrorRef *error)
{
    return SOSBSKBLoadAndUnlockWithPeerIDAndSecret(backupSliceKeyBag, SOSPeerInfoGetPeerID(peer), peerSecret, error);
}

keybag_handle_t SOSBSKBLoadAndUnlockWithDirectSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                     CFDataRef secret,
                                                     CFErrorRef *error)
{
    keybag_handle_t result = bad_keybag_handle;
    require_quiet(SecRequirementError(SOSBSKBIsDirect(backupSliceKeyBag), error, CFSTR("Not direct bag")), exit);

    result = SOSBSKBLoadAndUnlockBagWithSecret(backupSliceKeyBag,
                                               CFDataGetLength(secret),
                                               CFDataGetBytePtr(secret),
                                               error);

exit:
    return result;
}

#include "SecRecoveryKey.h"

static bool SOSPerformWithRecoveryKeyFullKey(CFDataRef wrappingSecret, CFErrorRef *error, void (^operation)(ccec_full_ctx_t fullKey, CFStringRef keyID)) {
    bool     result = false;

    CFStringRef keyID = NULL;
    SecRecoveryKey *sRecKey = NULL;
    CFDataRef fullKeyBytes = NULL;
    CFDataRef pubKeyBytes = NULL;
    CFStringRef restoreKeySecret = CFStringCreateWithBytes(kCFAllocatorDefault, CFDataGetBytePtr(wrappingSecret), CFDataGetLength(wrappingSecret), kCFStringEncodingUTF8, false);
    require_action_quiet(restoreKeySecret, errOut, SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unable to create key string from data.")));
    sRecKey = SecRKCreateRecoveryKey(restoreKeySecret);
    require_action_quiet(sRecKey, errOut, SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unable to create recovery key from string.")));
    fullKeyBytes = SecRKCopyBackupFullKey(sRecKey);
    pubKeyBytes = SecRKCopyBackupPublicKey(sRecKey);
    require_action_quiet(fullKeyBytes && pubKeyBytes, errOut, SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("Unable to create recovery key public and private keys.")));
    keyID = SOSCopyIDOfDataBuffer(pubKeyBytes, error);
    require_quiet(keyID, errOut);
    {
        size_t keysize = ccec_compact_import_priv_size(CFDataGetLength(fullKeyBytes));
        ccec_const_cp_t cp = ccec_curve_for_length_lookup(keysize, ccec_cp_256(), ccec_cp_384(), ccec_cp_521());
        ccec_full_ctx_decl_cp(cp, fullKey);
        int res = ccec_compact_import_priv(cp, CFDataGetLength(fullKeyBytes), CFDataGetBytePtr(fullKeyBytes), fullKey);
        if(res == 0) {
            operation(fullKey, keyID);
            result = true;
            ccec_full_ctx_clear_cp(cp, fullKey);
        }
    }
    if(!result) SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("Unable to perform crypto operation from fullKeyBytes."));

errOut:
    CFReleaseNull(keyID);
    CFReleaseNull(sRecKey);
    CFReleaseNull(fullKeyBytes);
    CFReleaseNull(pubKeyBytes);
    CFReleaseNull(restoreKeySecret);
    return result;
}

bskb_keybag_handle_t SOSBSKBLoadAndUnlockWithWrappingSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                            CFDataRef wrappingSecret,
                                                            CFErrorRef *error) {
    __block keybag_handle_t result = bad_keybag_handle;

    CFDataRef lookedUpData = SOSBSKBCopyRecoveryKey(backupSliceKeyBag);
    require_quiet(SecRequirementError(lookedUpData != NULL, error, CFSTR("no recovery key found in %@"), backupSliceKeyBag), errOut);

    SOSPerformWithRecoveryKeyFullKey(wrappingSecret, error, ^(ccec_full_ctx_t fullKey, CFStringRef keyID) {
        SOSPerformWithUnwrappedData(fullKey, lookedUpData, error, ^(size_t size, uint8_t *buffer) {
            result = SOSBSKBLoadAndUnlockBagWithSecret(backupSliceKeyBag, size, buffer, error);
        });
    });

errOut:
    CFReleaseSafe(lookedUpData);
    return result;
}

static CFDictionaryRef SOSBSKBCopyAdditionalKeysWithPrefix(CFAllocatorRef allocator, SOSBackupSliceKeyBagRef bskb, CFStringRef prefix) {
    CFMutableDictionaryRef retval = CFDictionaryCreateMutableForCFTypes(allocator);
    if(!retval) return NULL;
    CFDictionaryForEach(bskb->wrapped_keys, ^(const void *key, const void *value) {
        CFStringRef kpkid = asString(key, NULL);
        CFDataRef keyData = asData(value, NULL);
        if(kpkid && keyData && SOSKeyedPubKeyIdentifierIsPrefixed(kpkid)) {
            CFStringRef idPrefix = SOSKeyedPubKeyIdentifierCopyPrefix(kpkid);
            if(CFEqualSafe(idPrefix, prefix)) {
                CFDictionaryAddValue(retval, kpkid, keyData);
            }
            CFReleaseNull(idPrefix);
        }
    });
    return retval;
}

static bool SOSBSKBHasPrefixedKey(SOSBackupSliceKeyBagRef bskb, CFStringRef prefix) {
    CFDictionaryRef keyDict = SOSBSKBCopyAdditionalKeysWithPrefix(kCFAllocatorDefault, bskb, prefix);
    bool haveKeys = CFDictionaryGetCount(keyDict) > 0;
    CFReleaseNull(keyDict);
    return haveKeys;
}

CFDataRef SOSBSKBCopyRecoveryKey(SOSBackupSliceKeyBagRef bskb) {
    CFDictionaryRef keyDict = SOSBSKBCopyAdditionalKeysWithPrefix(kCFAllocatorDefault, bskb, bskbRkbgPrefix);
    if(CFDictionaryGetCount(keyDict) == 1) {
        __block CFDataRef keyData = NULL;
        CFDictionaryForEach(keyDict, ^(const void *key, const void *value) {
            keyData = asData(value, NULL);
        });
        return CFDataCreateCopy(kCFAllocatorDefault, keyData);
    }
    CFReleaseNull(keyDict);
    return NULL;
}

bool SOSBSKBHasRecoveryKey(SOSBackupSliceKeyBagRef bskb) {
    if(SOSBSKBHasPrefixedKey(bskb, bskbRkbgPrefix)) return true;
    // old way for RecoveryKeys
    int keyCount = (int) CFDictionaryGetCount(bskb->wrapped_keys);
    int peerCount = SOSBSKBCountPeers(bskb);
    return !SOSBSKBIsDirect(bskb) && ((keyCount - peerCount) > 0);
}

