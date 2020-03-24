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


#include <AssertMacros.h>

#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSPeerInfoDER.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"

#include "keychain/SecureObjectSync/SOSCircle.h"

#include "keychain/SecureObjectSync/SOSInternal.h"

#include <Security/SecKeyPriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecOTR.h>
#include <CoreFoundation/CFArray.h>
#include <dispatch/dispatch.h>
#include <Security/SecFramework.h>

#include <stdlib.h>
#include <assert.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include <CoreFoundation/CoreFoundation.h>

#include "utilities/iOSforOSX.h"

#include <AssertMacros.h>

#include <utilities/SecCFError.h>

// for OS X
#ifdef __cplusplus
extern "C" {
#endif

//---- missing

extern OSStatus SecKeyCopyPublicBytes(SecKeyRef key, CFDataRef* publicBytes);
extern SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey);
#ifdef __cplusplus
}
#endif

struct __OpaqueSOSFullPeerInfo {
    CFRuntimeBase          _base;
    
    SOSPeerInfoRef         peer_info;
    CFDataRef              key_ref;
    CFDataRef              octagon_peer_signing_key_ref;
    CFDataRef              octagon_peer_encryption_key_ref;
};

CFGiblisWithHashFor(SOSFullPeerInfo);



CFStringRef kSOSFullPeerInfoDescriptionKey = CFSTR("SOSFullPeerInfoDescription");
CFStringRef kSOSFullPeerInfoSignatureKey = CFSTR("SOSFullPeerInfoSignature");
CFStringRef kSOSFullPeerInfoNameKey = CFSTR("SOSFullPeerInfoName");


bool SOSFullPeerInfoUpdate(SOSFullPeerInfoRef fullPeerInfo, CFErrorRef *error, SOSPeerInfoRef (^create_modification)(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error)) {
    bool result = false;
    
    SOSPeerInfoRef newPeer = NULL;
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(fullPeerInfo, error);
    require_quiet(device_key, fail);
    
    newPeer = create_modification(fullPeerInfo->peer_info, device_key, error);
    require_quiet(newPeer, fail);

    CFTransferRetained(fullPeerInfo->peer_info, newPeer);
    result = true;
    
fail:
    CFReleaseNull(device_key);
    CFReleaseNull(newPeer);
    return result;
}

bool SOSFullPeerInfoUpdateToThisPeer(SOSFullPeerInfoRef peer, SOSPeerInfoRef pi, CFErrorRef *error) {
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSign(key, pi, error) ? CFRetainSafe(pi): NULL;
    });
}

SOSFullPeerInfoRef SOSFullPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt,
                                         CFDataRef backupKey,
                                         SecKeyRef signingKey,
                                         SecKeyRef octagonPeerSigningKey,
                                         SecKeyRef octagonPeerEncryptionKey,
                                         CFErrorRef* error) {
    return SOSFullPeerInfoCreateWithViews(allocator, gestalt, backupKey, NULL, signingKey,
                                          octagonPeerSigningKey, octagonPeerEncryptionKey, error);
}

SOSFullPeerInfoRef SOSFullPeerInfoCreateWithViews(CFAllocatorRef allocator,
                                                  CFDictionaryRef gestalt, CFDataRef backupKey, CFSetRef initialViews,
                                                  SecKeyRef signingKey,
                                                  SecKeyRef octagonPeerSigningKey,
                                                  SecKeyRef octagonPeerEncryptionKey,
                                                  CFErrorRef* error) {

    SOSFullPeerInfoRef result = NULL;
    SOSFullPeerInfoRef fpi = CFTypeAllocate(SOSFullPeerInfo, struct __OpaqueSOSFullPeerInfo, allocator);

    CFStringRef IDSID = CFSTR("");
    CFStringRef transportType = SOSTransportMessageTypeKVS;
    CFBooleanRef preferIDS = kCFBooleanFalse;
    CFBooleanRef preferIDSFragmentation = kCFBooleanTrue;
    CFBooleanRef preferACKModel = kCFBooleanTrue;

    fpi->peer_info = SOSPeerInfoCreateWithTransportAndViews(allocator, gestalt, backupKey,
                                                            IDSID, transportType, preferIDS,
                                                            preferIDSFragmentation, preferACKModel, initialViews,
                                                            signingKey, octagonPeerSigningKey, octagonPeerEncryptionKey, error);
    require_quiet(fpi->peer_info, exit);

    OSStatus status = SecKeyCopyPersistentRef(signingKey, &fpi->key_ref);
    require_quiet(SecError(status, error, CFSTR("Inflating persistent ref")), exit);
    
    status = SecKeyCopyPersistentRef(octagonPeerSigningKey, &fpi->octagon_peer_signing_key_ref);
    require_quiet(SecError(status, error, CFSTR("Inflating octagon peer signing persistent ref")), exit);
    status = SecKeyCopyPersistentRef(octagonPeerSigningKey, &fpi->octagon_peer_encryption_key_ref);
    require_quiet(SecError(status, error, CFSTR("Inflating octagon peer encryption persistent ref")), exit);

    CFTransferRetained(result, fpi);

exit:
    CFReleaseNull(fpi);
    return result;
}

SOSFullPeerInfoRef SOSFullPeerInfoCopyFullPeerInfo(SOSFullPeerInfoRef toCopy) {
    SOSFullPeerInfoRef retval = NULL;
    SOSFullPeerInfoRef fpi = CFTypeAllocate(SOSFullPeerInfo, struct __OpaqueSOSFullPeerInfo, kCFAllocatorDefault);
    SOSPeerInfoRef piToCopy = SOSFullPeerInfoGetPeerInfo(toCopy);
    
    require_quiet(piToCopy, errOut);
    require_quiet(fpi, errOut);
    fpi->peer_info = SOSPeerInfoCreateCopy(kCFAllocatorDefault, piToCopy, NULL);
    require_quiet(fpi->peer_info, errOut);
    fpi->key_ref = CFRetainSafe(toCopy->key_ref);
    CFTransferRetained(retval, fpi);

errOut:
    CFReleaseNull(fpi);
    return retval;
}

bool SOSFullPeerInfoUpdateOctagonKeys(SOSFullPeerInfoRef peer, SecKeyRef octagonSigningKey, SecKeyRef octagonEncryptionKey, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetOctagonKeys(kCFAllocatorDefault, peer, octagonSigningKey, octagonEncryptionKey, key, error);
    });
}

bool SOSFullPeerInfoUpdateOctagonSigningKey(SOSFullPeerInfoRef peer, SecKeyRef octagonSigningKey, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetOctagonSigningKey(kCFAllocatorDefault, peer, octagonSigningKey, key, error);
    });
}

bool SOSFullPeerInfoUpdateOctagonEncryptionKey(SOSFullPeerInfoRef peer, SecKeyRef octagonEncryptionKey, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetOctagonEncryptionKey(kCFAllocatorDefault, peer, octagonEncryptionKey, key, error);
    });
}



CFDataRef SOSPeerInfoCopyData(SOSPeerInfoRef pi, CFErrorRef *error)
{
    CFTypeRef vData = NULL;
    SecKeyRef pubKey = SOSPeerInfoCopyPubKey(pi, error);
    CFDictionaryRef query = NULL;
    require_quiet(pubKey, exit);
    

    CFDataRef public_key_hash = SecKeyCopyPublicKeyHash(pubKey);

    query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecClass,                 kSecClassKey,
                                                         kSecAttrKeyClass,          kSecAttrKeyClassPrivate,
                                                         kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                                                         kSecAttrApplicationLabel,  public_key_hash,
                                                         kSecReturnData,            kCFBooleanTrue,
                                                         NULL);
    CFReleaseNull(public_key_hash);

    require_quiet(SecError(SecItemCopyMatching(query, &vData),error ,
                           CFSTR("Error finding persistent ref to key from public: %@"), pubKey), exit);

exit:
    CFReleaseNull(query);
    CFReleaseNull(pubKey);
    if (vData == NULL) {
        secnotice("fpi","no private key found");
    }
    return (CFDataRef)vData;
}

SOSFullPeerInfoRef SOSFullPeerInfoCreateCloudIdentity(CFAllocatorRef allocator, SOSPeerInfoRef peer, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = CFTypeAllocate(SOSFullPeerInfo, struct __OpaqueSOSFullPeerInfo, allocator);
    
    SecKeyRef pubKey = NULL;
    
    fpi->peer_info = peer;
    CFRetainSafe(fpi->peer_info);
    if (fpi->peer_info == NULL) {
        CFReleaseNull(fpi);
        goto exit;
    }

    pubKey = SOSPeerInfoCopyPubKey(peer, error);
    require_quiet(pubKey, exit);
    
    fpi->key_ref = SecKeyCreatePersistentRefToMatchingPrivateKey(pubKey, error);
    
    if (fpi->key_ref == NULL) {
        CFReleaseNull(fpi);
        goto exit;
    }
    
exit:
    CFReleaseNull(pubKey);
    return fpi;
}


SOSFullPeerInfoRef SOSFullPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end) {
    SOSFullPeerInfoRef fpi = CFTypeAllocate(SOSFullPeerInfo, struct __OpaqueSOSFullPeerInfo, allocator);
    
    const uint8_t *sequence_end;
    
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    CFReleaseNull(fpi->peer_info);
    fpi->peer_info = SOSPeerInfoCreateFromDER(allocator, error, der_p, der_end);
    require_quiet(fpi->peer_info != NULL, fail);

    *der_p = der_decode_data(allocator, kCFPropertyListImmutable, &fpi->key_ref, error, *der_p, sequence_end);
    require_quiet(*der_p != NULL, fail);
    
    return fpi;

fail:
    CFReleaseNull(fpi);
    return NULL;
}

SOSFullPeerInfoRef SOSFullPeerInfoCreateFromData(CFAllocatorRef allocator, CFDataRef fullPeerData, CFErrorRef *error)
{
    if(!fullPeerData) return NULL;
    size_t size = CFDataGetLength(fullPeerData);
    const uint8_t *der = CFDataGetBytePtr(fullPeerData);
    SOSFullPeerInfoRef inflated = SOSFullPeerInfoCreateFromDER(allocator, error, &der, der + size);
    return inflated;
}

static void SOSFullPeerInfoDestroy(CFTypeRef aObj) {
    SOSFullPeerInfoRef fpi = (SOSFullPeerInfoRef) aObj;
    
    CFReleaseNull(fpi->peer_info);
    CFReleaseNull(fpi->key_ref);
    CFReleaseNull(fpi->octagon_peer_signing_key_ref);
    CFReleaseNull(fpi->octagon_peer_encryption_key_ref);
}

static Boolean SOSFullPeerInfoCompare(CFTypeRef lhs, CFTypeRef rhs) {
    SOSFullPeerInfoRef lpeer = (SOSFullPeerInfoRef) lhs;
    SOSFullPeerInfoRef rpeer = (SOSFullPeerInfoRef) rhs;
    
    if (!CFEqual(lpeer->peer_info, rpeer->peer_info))
        return false;
    
    if (CFEqual(lpeer->key_ref, rpeer->key_ref))
        return true;
    
    SecKeyRef lpk = SOSFullPeerInfoCopyDeviceKey(lpeer, NULL);
    SecKeyRef rpk = SOSFullPeerInfoCopyDeviceKey(rpeer, NULL);
    
    bool match = lpk && rpk && CFEqual(lpk, rpk);
    
    CFReleaseNull(lpk);
    CFReleaseNull(rpk);
    
    return match;
}

static CFHashCode   SOSFullPeerInfoHash(CFTypeRef cf) {
    SOSFullPeerInfoRef peer = (SOSFullPeerInfoRef) cf;

    return CFHash(peer->peer_info);
}

static CFStringRef SOSFullPeerInfoCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSFullPeerInfoRef fpi = (SOSFullPeerInfoRef) aObj;

    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSFullPeerInfo@%p: \"%@\">"), fpi, fpi->peer_info);
}

bool SOSFullPeerInfoUpdateGestalt(SOSFullPeerInfoRef peer, CFDictionaryRef gestalt, CFErrorRef* error)
{
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoCopyWithGestaltUpdate(kCFAllocatorDefault, peer, gestalt, key, error);
    });
}

bool SOSFullPeerInfoUpdateV2Dictionary(SOSFullPeerInfoRef peer, CFDictionaryRef newv2dict, CFErrorRef* error)
{
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoCopyWithV2DictionaryUpdate(kCFAllocatorDefault, peer,
                                                newv2dict, key, error);
    });
}

bool SOSFullPeerInfoUpdateBackupKey(SOSFullPeerInfoRef peer, CFDataRef backupKey, CFErrorRef* error)
{
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoCopyWithBackupKeyUpdate(kCFAllocatorDefault, peer, backupKey, key, error);
    });
}

bool SOSFullPeerInfoReplaceEscrowRecords(SOSFullPeerInfoRef peer, CFDictionaryRef escrowRecords, CFErrorRef* error)
{
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoCopyWithReplacedEscrowRecords(kCFAllocatorDefault, peer, escrowRecords, key, error);
    });
}

SOSViewResultCode SOSFullPeerInfoUpdateViews(SOSFullPeerInfoRef peer, SOSViewActionCode action, CFStringRef viewname, CFErrorRef* error)
{
    __block SOSViewResultCode retval = kSOSCCGeneralViewError;
    
    secnotice("viewChange", "%s view %@", SOSViewsXlateAction(action), viewname);
    
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoCopyWithViewsChange(kCFAllocatorDefault, peer, action, viewname, &retval, key, error);
    }) ? retval : kSOSCCGeneralViewError;
}

static CFMutableSetRef SOSFullPeerInfoCopyViewUpdate(SOSFullPeerInfoRef peer, CFSetRef minimumViews, CFSetRef excludedViews) {
    CFSetRef enabledViews = SOSPeerInfoCopyEnabledViews(peer->peer_info);
    CFMutableSetRef newViews = SOSPeerInfoCopyEnabledViews(peer->peer_info);

    if (isSet(minimumViews)) {
        CFSetUnion(newViews, minimumViews);
    }
    if (isSet(excludedViews)) {
        CFSetSubtract(newViews, excludedViews);
    }

    if (CFEqualSafe(newViews, enabledViews)) {
        CFReleaseNull(newViews);
    }

    CFReleaseNull(enabledViews);
    return newViews;
}

static bool SOSFullPeerInfoNeedsViewUpdate(SOSFullPeerInfoRef peer, CFSetRef minimumViews, CFSetRef excludedViews) {
    CFSetRef updatedViews = SOSFullPeerInfoCopyViewUpdate(peer, minimumViews, excludedViews);
    bool needsUpdate = (updatedViews != NULL);
    CFReleaseNull(updatedViews);
    return needsUpdate;
}

static bool sosFullPeerInfoRequiresUpdate(SOSFullPeerInfoRef peer, CFSetRef minimumViews, CFSetRef excludedViews) {
    
    if(!SOSPeerInfoVersionIsCurrent(peer->peer_info)) return true;
    if(!SOSPeerInfoSerialNumberIsSet(peer->peer_info)) return true;
    if(SOSFullPeerInfoNeedsViewUpdate(peer, minimumViews, excludedViews)) return true;

    return false;
}

// Returning false indicates we don't need to upgrade.
bool SOSFullPeerInfoUpdateToCurrent(SOSFullPeerInfoRef peer, CFSetRef minimumViews, CFSetRef excludedViews) {
    bool success = false;

    CFMutableSetRef newViews = NULL;
    CFErrorRef copyError = NULL;
    CFErrorRef createError = NULL;
    SecKeyRef device_key = NULL;

    require_quiet(sosFullPeerInfoRequiresUpdate(peer, minimumViews, excludedViews), errOut);

    newViews = SOSFullPeerInfoCopyViewUpdate(peer, minimumViews, excludedViews);

    device_key = SOSFullPeerInfoCopyDeviceKey(peer, &copyError);
    require_action_quiet(device_key, errOut,
                         secnotice("upgrade", "SOSFullPeerInfoCopyDeviceKey failed: %@", copyError));
    
    SOSPeerInfoRef newPeer = SOSPeerInfoCreateCurrentCopy(kCFAllocatorDefault, peer->peer_info,
                                                          NULL, NULL, kCFBooleanFalse, kCFBooleanTrue, kCFBooleanTrue, newViews,
                                                          device_key, &createError);
    require_action_quiet(newPeer, errOut,
                         secnotice("upgrade", "Peer info v2 create copy failed: %@", createError));

    CFTransferRetained(peer->peer_info, newPeer);

    success = true;

errOut:
    CFReleaseNull(newViews);
    CFReleaseNull(copyError);
    CFReleaseNull(createError);
    CFReleaseNull(device_key);
    return success;
}

SOSViewResultCode SOSFullPeerInfoViewStatus(SOSFullPeerInfoRef peer, CFStringRef viewname, CFErrorRef *error)
{
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(peer);
    if(!pi) return kSOSCCGeneralViewError;
    return SOSPeerInfoViewStatus(pi, viewname, error);
}

SOSPeerInfoRef SOSFullPeerInfoGetPeerInfo(SOSFullPeerInfoRef fullPeer) {
    return fullPeer?fullPeer->peer_info:NULL;
}

// MARK: Private Key Retrieval and Existence

SecKeyRef SOSFullPeerInfoCopyPubKey(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    SecKeyRef retval = NULL;
    require_quiet(fpi, errOut);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    require_quiet(pi, errOut);
    retval = SOSPeerInfoCopyPubKey(pi, error);

errOut:
    return retval;
}

SecKeyRef SOSFullPeerInfoCopyOctagonPublicSigningKey(SOSFullPeerInfoRef fpi, CFErrorRef* error)
{
    SecKeyRef retval = NULL;
    require_quiet(fpi, errOut);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    require_quiet(pi, errOut);
    retval = SOSPeerInfoCopyOctagonSigningPublicKey(pi, error);
errOut:
    return retval;
}

SecKeyRef SOSFullPeerInfoCopyOctagonPublicEncryptionKey(SOSFullPeerInfoRef fpi, CFErrorRef* error)
{
    SecKeyRef retval = NULL;
    require_quiet(fpi, errOut);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    require_quiet(pi, errOut);
    retval = SOSPeerInfoCopyOctagonEncryptionPublicKey(pi, error);

errOut:
    return retval;
}


static SecKeyRef SOSFullPeerInfoCopyMatchingPrivateKey(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    SecKeyRef retval = NULL;

    SecKeyRef pub = SOSFullPeerInfoCopyPubKey(fpi, error);
    require_quiet(pub, exit);
    retval = SecKeyCopyMatchingPrivateKey(pub, error);
exit:
    CFReleaseNull(pub);
    return retval;
}

static SecKeyRef SOSFullPeerInfoCopyMatchingOctagonSigningPrivateKey(SOSFullPeerInfoRef fpi, CFErrorRef* error)
{
    SecKeyRef retval = NULL;
    SecKeyRef pub = SOSFullPeerInfoCopyOctagonPublicSigningKey(fpi, error);
    require_quiet(pub, exit);
    retval = SecKeyCopyMatchingPrivateKey(pub, error);
    
exit:
    CFReleaseNull(pub);
    return retval;
}
static SecKeyRef SOSFullPeerInfoCopyMatchingOctagonEncryptionPrivateKey(SOSFullPeerInfoRef fpi, CFErrorRef* error)
{
    SecKeyRef retval = NULL;
    SecKeyRef pub = SOSFullPeerInfoCopyOctagonPublicEncryptionKey(fpi, error);
    require_quiet(pub, exit);
    retval = SecKeyCopyMatchingPrivateKey(pub, error);

exit:
    CFReleaseNull(pub);
    return retval;
}


static OSStatus SOSFullPeerInfoGetMatchingPrivateKeyStatus(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    OSStatus retval = errSecParam;
    SecKeyRef pub = SOSFullPeerInfoCopyPubKey(fpi, error);
    require_quiet(pub, exit);
    retval = SecKeyGetMatchingPrivateKeyStatus(pub, error);

exit:
    CFReleaseNull(pub);
    return retval;
}

bool SOSFullPeerInfoValidate(SOSFullPeerInfoRef peer, CFErrorRef* error) {
    OSStatus result = SOSFullPeerInfoGetMatchingPrivateKeyStatus(peer, error);
    if(result == errSecSuccess) return true;
    return false;
}

bool SOSFullPeerInfoPrivKeyExists(SOSFullPeerInfoRef peer) {
    OSStatus result = SOSFullPeerInfoGetMatchingPrivateKeyStatus(peer, NULL);
    if(result == errSecItemNotFound || result == errSecParam) return false;
    return true;
}

bool SOSFullPeerInfoPurgePersistentKey(SOSFullPeerInfoRef fpi, CFErrorRef* error) {
    bool result = false;
    CFDictionaryRef privQuery = NULL;
    CFMutableDictionaryRef query = NULL;
    CFDictionaryRef octagonPrivQuery = NULL;
    CFMutableDictionaryRef octagonQuery = NULL;

    SecKeyRef pub = SOSFullPeerInfoCopyPubKey(fpi, error);
    SecKeyRef octagonSigningPub = SOSFullPeerInfoCopyOctagonPublicSigningKey(fpi, error);
    SecKeyRef octagonEncryptionPub = SOSFullPeerInfoCopyOctagonPublicEncryptionKey(fpi, error);
    require_quiet(pub, fail);
    // iCloud Identities doesn't have either signing or encryption key here. Don't fail if they're not present.

    privQuery = CreatePrivateKeyMatchingQuery(pub, false);
    query = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, privQuery);
    CFDictionaryAddValue(query, kSecUseTombstones, kCFBooleanFalse);

    result = SecError(SecItemDelete(query), error, CFSTR("Deleting while purging"));

    // do the same thing to also purge the octagon sync signing key
    if(octagonSigningPub) {
        octagonPrivQuery = CreatePrivateKeyMatchingQuery(octagonSigningPub, false);
        octagonQuery = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, octagonPrivQuery);
        CFDictionaryAddValue(octagonQuery, kSecUseTombstones, kCFBooleanFalse);

        result &= SecError(SecItemDelete(octagonQuery), error, CFSTR("Deleting signing key while purging"));
    }

    CFReleaseNull(octagonPrivQuery);
    CFReleaseNull(octagonQuery);

    // do the same thing to also purge the octagon encryption key
    if(octagonEncryptionPub) {
        octagonPrivQuery = CreatePrivateKeyMatchingQuery(octagonEncryptionPub, false);
        octagonQuery = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, octagonPrivQuery);
        CFDictionaryAddValue(octagonQuery, kSecUseTombstones, kCFBooleanFalse);

        result &= SecError(SecItemDelete(octagonQuery), error, CFSTR("Deleting encryption key while purging"));
    }

fail:
    CFReleaseNull(privQuery);
    CFReleaseNull(query);
    CFReleaseNull(pub);
    CFReleaseNull(octagonPrivQuery);
    CFReleaseNull(octagonQuery);
    CFReleaseNull(octagonSigningPub);
    CFReleaseNull(octagonEncryptionPub);
    return result;
}

SecKeyRef  SOSFullPeerInfoCopyDeviceKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error)
{
    return SOSFullPeerInfoCopyMatchingPrivateKey(fullPeer, error);
}

SecKeyRef SOSFullPeerInfoCopyOctagonSigningKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error)
{
    return SOSFullPeerInfoCopyMatchingOctagonSigningPrivateKey(fullPeer, error);
}

SecKeyRef SOSFullPeerInfoCopyOctagonEncryptionKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error)
{
    return SOSFullPeerInfoCopyMatchingOctagonEncryptionPrivateKey(fullPeer, error);
}


//
// MARK: Encode and decode
//
size_t      SOSFullPeerInfoGetDEREncodedSize(SOSFullPeerInfoRef peer, CFErrorRef *error)
{
    size_t peer_size = SOSPeerInfoGetDEREncodedSize(peer->peer_info, error);
    if (peer_size == 0)
        return 0;

    size_t ref_size = der_sizeof_data(peer->key_ref, error);
    if (ref_size == 0)
        return 0;

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        peer_size + ref_size);
}

uint8_t* SOSFullPeerInfoEncodeToDER(SOSFullPeerInfoRef peer, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           SOSPeerInfoEncodeToDER(peer->peer_info, error, der,
           der_encode_data(peer->key_ref, error, der, der_end)));
}

CFDataRef SOSFullPeerInfoCopyEncodedData(SOSFullPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error)
{
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSFullPeerInfoGetDEREncodedSize(peer, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSFullPeerInfoEncodeToDER(peer, error, buffer, (uint8_t *) buffer + size);
    });
}

bool SOSFullPeerInfoPromoteToApplication(SOSFullPeerInfoRef fpi, SecKeyRef user_key, CFErrorRef *error)
{
    bool success = false;
    SOSPeerInfoRef old_pi = NULL;

    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(fpi, error);
    require_quiet(device_key, exit);

    old_pi = fpi->peer_info;
    fpi->peer_info = SOSPeerInfoCopyAsApplication(old_pi, user_key, device_key, error);

    require_action_quiet(fpi->peer_info, exit, fpi->peer_info = old_pi; old_pi = NULL);

    success = true;

exit:
    CFReleaseSafe(old_pi);
    CFReleaseSafe(device_key);
    return success;
}

bool SOSFullPeerInfoUpgradeSignatures(SOSFullPeerInfoRef fpi, SecKeyRef user_key, CFErrorRef *error)
{
    bool success = false;
    SOSPeerInfoRef old_pi = NULL;
    
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(fpi, error);
    require_quiet(device_key, exit);
    
    old_pi = fpi->peer_info;
    fpi->peer_info = SOSPeerInfoUpgradeSignatures(NULL, user_key, device_key, old_pi, error);
    
    require_action_quiet(fpi->peer_info, exit, fpi->peer_info = old_pi; old_pi = NULL);
    
    success = true;
    
exit:
    CFReleaseSafe(old_pi);
    CFReleaseSafe(device_key);
    return success;
}

//
//
//

SOSPeerInfoRef SOSFullPeerInfoPromoteToRetiredAndCopy(SOSFullPeerInfoRef fpi, CFErrorRef *error)
{
    SOSPeerInfoRef peer_to_free = NULL;
    SOSPeerInfoRef retired_peer = NULL;
    SecKeyRef key = SOSFullPeerInfoCopyDeviceKey(fpi, error);
    require_quiet(key, error_out);

    retired_peer = SOSPeerInfoCreateRetirementTicket(NULL, key, fpi->peer_info, error);

    require_quiet(retired_peer, error_out);

    peer_to_free = fpi->peer_info;
    fpi->peer_info = retired_peer;
    CFRetainSafe(fpi->peer_info);

error_out:
    CFReleaseNull(key);
    CFReleaseNull(peer_to_free);
    return retired_peer;
}


bool SOSFullPeerInfoPing(SOSFullPeerInfoRef peer, CFErrorRef* error) {
    bool retval = false;
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(peer, error);
    require_quiet(device_key, fail);
    SOSPeerInfoRef newPeer = SOSPeerInfoCopyWithPing(kCFAllocatorDefault, peer->peer_info, device_key, error);
    require_quiet(newPeer, fail);
    
    CFReleaseNull(peer->peer_info);
    peer->peer_info = newPeer;
    newPeer = NULL;
    retval = true;
fail:
    CFReleaseNull(device_key);
    return retval;
}


