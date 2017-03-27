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

#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSCircle.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoDER.h>

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
};

CFGiblisWithHashFor(SOSFullPeerInfo);



CFStringRef kSOSFullPeerInfoDescriptionKey = CFSTR("SOSFullPeerInfoDescription");
CFStringRef kSOSFullPeerInfoSignatureKey = CFSTR("SOSFullPeerInfoSignature");
CFStringRef kSOSFullPeerInfoNameKey = CFSTR("SOSFullPeerInfoName");


static bool SOSFullPeerInfoUpdate(SOSFullPeerInfoRef peer, CFErrorRef *error, SOSPeerInfoRef (^create_modification)(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error)) {
    bool result = false;
    
    SOSPeerInfoRef newPeer = NULL;
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(peer, error);
    require_quiet(device_key, fail);
    
    newPeer = create_modification(peer->peer_info, device_key, error);
    require_quiet(newPeer, fail);
    
    CFTransferRetained(peer->peer_info, newPeer);
    
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
                                         CFDataRef backupKey, SecKeyRef signingKey,
                                         CFErrorRef* error) {
    return SOSFullPeerInfoCreateWithViews(allocator, gestalt, backupKey, NULL, signingKey, error);
}

SOSFullPeerInfoRef SOSFullPeerInfoCreateWithViews(CFAllocatorRef allocator,
                                                  CFDictionaryRef gestalt, CFDataRef backupKey, CFSetRef initialViews,
                                                  SecKeyRef signingKey, CFErrorRef* error) {

    SOSFullPeerInfoRef result = NULL;
    SOSFullPeerInfoRef fpi = CFTypeAllocate(SOSFullPeerInfo, struct __OpaqueSOSFullPeerInfo, allocator);

    CFStringRef IDSID = CFSTR("");
    CFStringRef transportType =SOSTransportMessageTypeIDSV2;
    CFBooleanRef preferIDS = kCFBooleanFalse;
    CFBooleanRef preferIDSFragmentation = kCFBooleanTrue;
    CFBooleanRef preferACKModel = kCFBooleanTrue;

    fpi->peer_info = SOSPeerInfoCreateWithTransportAndViews(allocator, gestalt, backupKey,
                                                            IDSID, transportType, preferIDS,
                                                            preferIDSFragmentation, preferACKModel, initialViews,
                                                            signingKey, error);
    require_quiet(fpi->peer_info, exit);

    OSStatus status = SecKeyCopyPersistentRef(signingKey, &fpi->key_ref);
    require_quiet(SecError(status, error, CFSTR("Inflating persistent ref")), exit);

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
    fpi->key_ref = toCopy->key_ref;
    CFTransferRetained(retval, fpi);

errOut:
    CFReleaseNull(fpi);
    return retval;
}

bool SOSFullPeerInfoUpdateTransportType(SOSFullPeerInfoRef peer, CFStringRef transportType, CFErrorRef* error)
{
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetTransportType(kCFAllocatorDefault, peer, transportType, key, error);
    });
}

bool SOSFullPeerInfoUpdateDeviceID(SOSFullPeerInfoRef peer, CFStringRef deviceID, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetDeviceID(kCFAllocatorDefault, peer, deviceID, key, error);
    });
}

bool SOSFullPeerInfoUpdateTransportPreference(SOSFullPeerInfoRef peer, CFBooleanRef preference, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetIDSPreference(kCFAllocatorDefault, peer, preference, key, error);
    });
}

bool SOSFullPeerInfoUpdateTransportFragmentationPreference(SOSFullPeerInfoRef peer, CFBooleanRef preference, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetIDSFragmentationPreference(kCFAllocatorDefault, peer, preference, key, error);
    });
}

bool SOSFullPeerInfoUpdateTransportAckModelPreference(SOSFullPeerInfoRef peer, CFBooleanRef preference, CFErrorRef* error){
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoSetIDSACKModelPreference(kCFAllocatorDefault, peer, preference, key, error);
    });
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
        return SOSPeerInfoCopyWithGestaltUpdate(kCFAllocatorDefault, peer,
                                                                                                 gestalt, key, error);
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

bool SOSFullPeerInfoAddEscrowRecord(SOSFullPeerInfoRef peer, CFStringRef dsid, CFDictionaryRef escrowRecord, CFErrorRef* error)
{
    return SOSFullPeerInfoUpdate(peer, error, ^SOSPeerInfoRef(SOSPeerInfoRef peer, SecKeyRef key, CFErrorRef *error) {
        return SOSPeerInfoCopyWithEscrowRecordUpdate(kCFAllocatorDefault, peer, dsid, escrowRecord, key, error);
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
    if(!(SOSPeerInfoV2DictionaryHasString(peer->peer_info, sDeviceID)))return true;
    if(!(SOSPeerInfoV2DictionaryHasString(peer->peer_info, sTransportType))) return true;
    if(!(SOSPeerInfoV2DictionaryHasBoolean(peer->peer_info, sPreferIDS))) return true;
    if(!(SOSPeerInfoV2DictionaryHasBoolean(peer->peer_info, sPreferIDSFragmentation))) return true;
    if(!(SOSPeerInfoV2DictionaryHasBoolean(peer->peer_info, sPreferIDSACKModel))) return true;
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


SOSSecurityPropertyResultCode SOSFullPeerInfoUpdateSecurityProperty(SOSFullPeerInfoRef peer, SOSViewActionCode action, CFStringRef property, CFErrorRef* error)
{
    SOSSecurityPropertyResultCode retval = kSOSCCGeneralSecurityPropertyError;
    SecKeyRef device_key = SOSFullPeerInfoCopyDeviceKey(peer, error);
    require_quiet(device_key, fail);
    
    SOSPeerInfoRef newPeer = SOSPeerInfoCopyWithSecurityPropertyChange(kCFAllocatorDefault, peer->peer_info, action, property, &retval, device_key, error);
    
    require_quiet(newPeer, fail);
    
    CFReleaseNull(peer->peer_info);
    peer->peer_info = newPeer;
    newPeer = NULL;
    
fail:
    CFReleaseNull(device_key);
    return retval;
}

SOSSecurityPropertyResultCode SOSFullPeerInfoSecurityPropertyStatus(SOSFullPeerInfoRef peer, CFStringRef property, CFErrorRef *error)
{
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(peer);
    secnotice("secprop", "have pi %s", (pi)? "true": "false");
    if(!pi) return kSOSCCGeneralSecurityPropertyError;
    return SOSPeerInfoSecurityPropertyStatus(pi, property, error);
}


SOSPeerInfoRef SOSFullPeerInfoGetPeerInfo(SOSFullPeerInfoRef fullPeer) {
    return fullPeer?fullPeer->peer_info:NULL;
}

// MARK: Private Key Retrieval and Existence

static SecKeyRef SOSFullPeerInfoCopyPubKey(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    SecKeyRef retval = NULL;
    require_quiet(fpi, errOut);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    require_quiet(pi, errOut);
    retval = SOSPeerInfoCopyPubKey(pi, error);

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

    SecKeyRef pub = SOSFullPeerInfoCopyPubKey(fpi, error);
    require_quiet(pub, fail);

    privQuery = CreatePrivateKeyMatchingQuery(pub, false);
    query = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, privQuery);
    CFDictionaryAddValue(query, kSecUseTombstones, kCFBooleanFalse);

    result = SecError(SecItemDelete(query), error, CFSTR("Deleting while purging"));

fail:
    CFReleaseNull(privQuery);
    CFReleaseNull(query);
    CFReleaseNull(pub);
    return result;
}

SecKeyRef  SOSFullPeerInfoCopyDeviceKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error) {
    return SOSFullPeerInfoCopyMatchingPrivateKey(fullPeer, error);
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


