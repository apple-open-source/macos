//
//  SOSFullPeerInfo.c
//  sec
//
//  Created by Mitch Adler on 10/26/12.
//
//

#include <AssertMacros.h>

#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSCircle.h>

#include <SecureObjectSync/SOSInternal.h>

#include <Security/SecKeyPriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecOTR.h>
#include <CoreFoundation/CFArray.h>
#include <dispatch/dispatch.h>

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
};

CFGiblisWithHashFor(SOSFullPeerInfo);


static CFStringRef sPublicKeyKey = CFSTR("PublicSigningKey");
static CFStringRef sNameKey      = CFSTR("DeviceName");
static CFStringRef sVersionKey   = CFSTR("ConflictVersion");

CFStringRef kSOSFullPeerInfoDescriptionKey = CFSTR("SOSFullPeerInfoDescription");
CFStringRef kSOSFullPeerInfoSignatureKey = CFSTR("SOSFullPeerInfoSignature");
CFStringRef kSOSFullPeerInfoNameKey = CFSTR("SOSFullPeerInfoName");

SOSFullPeerInfoRef SOSFullPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = CFTypeAllocate(SOSFullPeerInfo, struct __OpaqueSOSFullPeerInfo, allocator);

    fpi->peer_info = SOSPeerInfoCreate(allocator, gestalt, signingKey, error);
    if (fpi->peer_info == NULL) {
        CFReleaseNull(fpi);
        goto exit;
    }
    
    OSStatus result = SecKeyCopyPersistentRef(signingKey, &fpi->key_ref);

    if (result != errSecSuccess) {
        CFReleaseNull(fpi);
        goto exit;
    }

exit:
    return fpi;
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

    pubKey = SOSPeerInfoCopyPubKey(peer);
    
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
    size_t size = CFDataGetLength(fullPeerData);
    const uint8_t *der = CFDataGetBytePtr(fullPeerData);
    SOSFullPeerInfoRef inflated = SOSFullPeerInfoCreateFromDER(allocator, error, &der, der + size);
    return inflated;
}

static void SOSFullPeerInfoDestroy(CFTypeRef aObj) {
    SOSFullPeerInfoRef fpi = (SOSFullPeerInfoRef) aObj;
    
    CFReleaseNull(fpi->peer_info);
    CFReleaseNull(fpi->key_ref);
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

static CFStringRef SOSFullPeerInfoCopyDescription(CFTypeRef aObj) {
    SOSFullPeerInfoRef fpi = (SOSFullPeerInfoRef) aObj;

    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSFullPeerInfo@%p: \"%@\">"), fpi, fpi->peer_info);
}

bool SOSFullPeerInfoUpdateGestalt(SOSFullPeerInfoRef peer, CFDictionaryRef gestalt, CFErrorRef* error)
{
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(peer, error);
    require_quiet(device_key, fail);
    
    SOSPeerInfoRef newPeer = SOSPeerInfoCopyWithGestaltUpdate(kCFAllocatorDefault, peer->peer_info,
                                                              gestalt, device_key, error);
    
    require_quiet(newPeer, fail);
    
    CFReleaseNull(peer->peer_info);
    peer->peer_info = newPeer;
    newPeer = NULL;

    CFReleaseNull(device_key);
    return true;

fail:
    CFReleaseNull(device_key);
    return false;
}


bool SOSFullPeerInfoValidate(SOSFullPeerInfoRef peer, CFErrorRef* error) {
    return true;
}

bool SOSFullPeerInfoPurgePersistentKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error) {
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecValuePersistentRef, fullPeer->key_ref,
                                                         kSecUseTombstones, kCFBooleanFalse,
                                                         NULL);
    SecItemDelete(query);
    CFReleaseNull(query);
    
    return true;
}

SOSPeerInfoRef SOSFullPeerInfoGetPeerInfo(SOSFullPeerInfoRef fullPeer)
{
    return fullPeer?fullPeer->peer_info:NULL;
}

SecKeyRef  SOSFullPeerInfoCopyDeviceKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error)
{
    SecKeyRef device_key = NULL;

    require(fullPeer->key_ref, fail);

    OSStatus result = SecKeyFindWithPersistentRef(fullPeer->key_ref, &device_key);

    require_action_quiet(result == errSecSuccess, fail, SecError(result, error, CFSTR("Finding Persistent Ref")));

    return device_key;
        
fail:
    CFReleaseNull(device_key);
    return NULL;
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
    size_t size = SOSFullPeerInfoGetDEREncodedSize(peer, error);
    if (size == 0)
        return NULL;
    uint8_t buffer[size];
    uint8_t* start = SOSFullPeerInfoEncodeToDER(peer, error, buffer, buffer + sizeof(buffer));
    CFDataRef result = CFDataCreate(kCFAllocatorDefault, start, size);
    return result;
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



