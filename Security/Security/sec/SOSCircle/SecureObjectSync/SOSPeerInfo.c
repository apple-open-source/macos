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
#include <TargetConditionals.h>

#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfoInternal.h>
#include <SecureObjectSync/SOSCircle.h>

#include <SecureObjectSync/SOSInternal.h>
#include <ipc/securityd_client.h>

#include <CoreFoundation/CFArray.h>
#include <dispatch/dispatch.h>

#include <stdlib.h>
#include <assert.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include <utilities/SecXPCError.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>

#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDate.h>

#include <xpc/xpc.h>

#if TARGET_OS_IPHONE || TARGET_OS_EMBEDDED
#include <MobileGestalt.h>
#endif

#include <Security/SecBase64.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecOTR.h>
#include <Security/SecuritydXPC.h>

#if 0//TARGET_OS_MAC // TODO: this function is the only one that causes secd to need to link against Security.framework on OSX

__BEGIN_DECLS
SecKeyRef _SecKeyCreateFromPublicData(CFAllocatorRef allocator, CFIndex algorithmID, CFDataRef publicBytes);
__END_DECLS

#endif  /* TARGET_OS_MAC */

struct __OpaqueSOSPeerInfo {
    CFRuntimeBase          _base;
    
    //
    CFMutableDictionaryRef description;
    CFDataRef              signature;
    
    // Cached data
    CFDictionaryRef        gestalt;
    CFStringRef            id;
    CFIndex                version;
    
    CFStringRef            transportType;
    CFStringRef            deviceID;
};

CFGiblisWithHashFor(SOSPeerInfo);

CFStringRef kPIUserDefinedDeviceName = CFSTR("ComputerName");
CFStringRef kPIDeviceModelName = CFSTR("ModelName");
CFStringRef kPIMessageProtocolVersion = CFSTR("MessageProtocolVersion");

// Description Dictionary Entries
static CFStringRef sPublicKeyKey = CFSTR("PublicSigningKey");
static CFStringRef sGestaltKey   = CFSTR("DeviceGestalt");
static CFStringRef sVersionKey   = CFSTR("ConflictVersion");
static CFStringRef sCloudIdentityKey   = CFSTR("CloudIdentity");
static CFStringRef sApplicationDate   = CFSTR("ApplicationDate");
static CFStringRef sApplicationUsig   = CFSTR("ApplicationUsig");
static CFStringRef sRetirementDate   = CFSTR("RetirementDate");

// Peerinfo Entries
CFStringRef kSOSPeerInfoDescriptionKey = CFSTR("SOSPeerInfoDescription");
CFStringRef kSOSPeerInfoSignatureKey = CFSTR("SOSPeerInfoSignature");
CFStringRef kSOSPeerInfoNameKey = CFSTR("SOSPeerInfoName");


SecKeyRef SOSPeerInfoCopyPubKey(SOSPeerInfoRef peer) {
    CFDataRef pubKeyBytes = CFDictionaryGetValue(peer->description, sPublicKeyKey);
    if (pubKeyBytes == NULL)
        return NULL;
    CFAllocatorRef allocator = CFGetAllocator(peer);
    SecKeyRef pubKey = SecKeyCreateFromPublicData(allocator, kSecECDSAAlgorithmID, pubKeyBytes);
    return pubKey;
}


static bool SOSDescriptionHash(SOSPeerInfoRef peer, const struct ccdigest_info *di, void *hashresult, CFErrorRef *error) {
    ccdigest_di_decl(di, ctx);
    ccdigest_init(di, ctx);
    void *ctx_p = ctx;
    if(!SOSPeerInfoUpdateDigestWithDescription(peer, di, ctx_p, error)) return false;
    ccdigest_final(di, ctx, hashresult);
    return true;
}


#define SIGLEN 128
static CFDataRef sosSignHash(SecKeyRef privkey, const struct ccdigest_info *di, uint8_t *hbuf) {
    OSStatus stat;
    size_t siglen = SIGLEN;
    uint8_t sig[siglen];
    if((stat = SecKeyRawSign(privkey, kSecPaddingNone, hbuf, di->output_size, sig, &siglen)) != 0) {
        return NULL;
    }
    return CFDataCreate(NULL, sig, (CFIndex)siglen);
}

static bool sosVerifyHash(SecKeyRef pubkey, const struct ccdigest_info *di, uint8_t *hbuf, CFDataRef signature) {
    return SecKeyRawVerify(pubkey, kSecPaddingNone, hbuf, di->output_size,
                           CFDataGetBytePtr(signature), CFDataGetLength(signature)) == errSecSuccess;
}

static bool SOSPeerInfoSign(SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool status = false;
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];
    CFDataRef newSignature = NULL;
    
    require_action_quiet(SOSDescriptionHash(peer, di, hbuf, error), fail,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to hash description for peer"), NULL, error));
    
    newSignature = sosSignHash(privKey, di, hbuf);
    require_action_quiet(newSignature, fail, SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to sign peerinfo for peer"), NULL, error));

    CFReleaseNull(peer->signature);
    peer->signature = newSignature;
    newSignature = NULL;
    status = true;

fail:
    CFReleaseNull(newSignature);
    return status;
}

// Return true (1) if the signature verifies.
static bool SOSPeerInfoVerify(SOSPeerInfoRef peer, CFErrorRef *error) {
    bool result = false;
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];

    SecKeyRef pubKey = SOSPeerInfoCopyPubKey(peer);
    require_action_quiet(pubKey, error_out,
                         SOSErrorCreate(kSOSErrorNoKey, error, NULL,
                                        CFSTR("Couldn't find pub key for %@"), peer));

    require_quiet(SOSDescriptionHash(peer, di, hbuf, error), error_out);

    require_action_quiet(sosVerifyHash(pubKey, di, hbuf, peer->signature), error_out,
                         SOSErrorCreate(kSOSErrorBadSignature, error, NULL,
                                        CFSTR("Signature didn't verify for %@"), peer));
    result = true;

error_out:
    CFReleaseNull(pubKey);
    return result;
}

static SOSPeerInfoRef SOSPeerInfoCreate_Internal(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error, void (^ description_modifier)(CFMutableDictionaryRef description)) {
    SOSPeerInfoRef pi = CFTypeAllocate(SOSPeerInfo, struct __OpaqueSOSPeerInfo, allocator);
    pi->gestalt = gestalt;
    CFRetain(pi->gestalt);
    
    pi->version = SOSPeerInfoGetPeerProtocolVersion(pi);
    pi->transportType = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("KVS"));
    CFDataRef publicBytes = NULL;
    CFNumberRef versionNumber = NULL;

    SecKeyRef publicKey = SecKeyCreatePublicFromPrivate(signingKey);
    if (publicKey == NULL) {
        SOSCreateError(kSOSErrorBadKey, CFSTR("Unable to get public"), NULL, error);
        CFReleaseNull(pi);
        goto exit;
    }

    OSStatus result = SecKeyCopyPublicBytes(publicKey, &publicBytes);
    
    if (result != errSecSuccess) {
        SOSCreateError(kSOSErrorBadKey, CFSTR("Failed to export public bytes"), NULL, error);
        CFReleaseNull(pi);
        goto exit;
    }
    
    pi->signature = CFDataCreateMutable(allocator, 0);
    
    versionNumber = CFNumberCreateWithCFIndex(NULL, pi->version);
    pi->description = CFDictionaryCreateMutableForCFTypesWith(allocator,
                                                              sVersionKey,   versionNumber,
                                                              sPublicKeyKey, publicBytes,
                                                              sGestaltKey,   pi->gestalt,
                                                              NULL);
    description_modifier(pi->description);
    
    CFStringRef deviceName = CFDictionaryGetValue(pi->gestalt, kPIUserDefinedDeviceName);
    CFStringRef modelName = CFDictionaryGetValue(pi->gestalt, kPIDeviceModelName);
    CFStringRef ID = pi->id;
    CFMutableStringRef description = CFStringCreateMutableCopy(kCFAllocatorDefault, CFStringGetLength(deviceName), deviceName);
    if(modelName){
        CFStringAppend(description, CFSTR(", "));
        CFStringAppend(description, modelName);
    }
    if(ID){
        CFStringAppend(description, CFSTR(", "));
        CFStringAppend(description, ID);
    }
    
    pi->deviceID = CFStringCreateCopy(kCFAllocatorDefault, description);
    CFReleaseNull(description);
    
    pi->id = SOSCopyIDOfKey(publicKey, error);
    CFReleaseNull(publicKey);

    require_quiet(pi->id, exit);

    if (!SOSPeerInfoSign(signingKey, pi, error)) {
        CFReleaseNull(pi);
        goto exit;
    }

exit:
    CFReleaseNull(versionNumber);
    CFReleaseNull(publicBytes);
    return pi;
}

SOSPeerInfoRef SOSPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error) {
    return SOSPeerInfoCreate_Internal(allocator, gestalt, signingKey, error, ^(CFMutableDictionaryRef description) {});
}

SOSPeerInfoRef SOSPeerInfoCreateCloudIdentity(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error) {
    return SOSPeerInfoCreate_Internal(allocator, gestalt, signingKey, error, ^(CFMutableDictionaryRef description) {
        CFDictionarySetValue(description, sCloudIdentityKey, kCFBooleanTrue);
    });

}


SOSPeerInfoRef SOSPeerInfoCreateCopy(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFErrorRef* error) {
    SOSPeerInfoRef pi = CFTypeAllocate(SOSPeerInfo, struct __OpaqueSOSPeerInfo, allocator);
    
    pi->description = CFDictionaryCreateMutableCopy(allocator, 0, toCopy->description);
    pi->signature = CFDataCreateCopy(allocator, toCopy->signature);
    
    pi->gestalt = CFDictionaryCreateCopy(allocator, toCopy->gestalt);
    pi->id = CFStringCreateCopy(allocator, toCopy->id);
    pi->transportType = CFStringCreateCopy(allocator, toCopy->transportType);
    pi->deviceID = CFStringCreateCopy(kCFAllocatorDefault, toCopy->deviceID);
    pi->version = toCopy->version;

    return pi;
}

SOSPeerInfoRef SOSPeerInfoCopyWithGestaltUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error) {
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(allocator, toCopy, error);

    CFRetainSafe(gestalt);
    CFReleaseNull(pi->gestalt);
    pi->gestalt = gestalt;

    CFDictionarySetValue(pi->description, sGestaltKey, pi->gestalt);

    SecKeyRef pub_key = SOSPeerInfoCopyPubKey(pi);
   
    pi->id = SOSCopyIDOfKey(pub_key, error);
    require_quiet(pi->id, exit);

    require_action_quiet(SOSPeerInfoSign(signingKey, pi, error), exit, CFReleaseNull(pi));

exit:
    CFReleaseNull(pub_key);
    return pi;
}

SOSPeerInfoRef SOSPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end) {
    SOSPeerInfoRef pi = CFTypeAllocate(SOSPeerInfo, struct __OpaqueSOSPeerInfo, allocator);
    SecKeyRef pubKey = NULL;

    const uint8_t *sequence_end;

    CFPropertyListRef pl = NULL;
    
    pi->gestalt = NULL;
    pi->version = 0; // TODO: Encode this in the DER
    pi->transportType = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("KVS"));
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = der_decode_plist(allocator, kCFPropertyListImmutable, &pl, error, *der_p, sequence_end);
    *der_p = der_decode_data(allocator, kCFPropertyListImmutable, &pi->signature, error, *der_p, sequence_end);
        
    if (*der_p == NULL || *der_p != sequence_end) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Format of Peer Info DER"), NULL, error);
        goto fail;
    }

    if (CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(pl));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected dictionary got %@"), description);
        CFReleaseSafe(description);
        goto fail;
    }
    
    pi->description = (CFMutableDictionaryRef) pl;
    CFRetain(pi->description);
    CFReleaseNull(pl);
    
    CFNumberRef versionNumber = CFDictionaryGetValue(pi->description, sVersionKey);

    if (versionNumber) {
        if (CFGetTypeID(versionNumber) != CFNumberGetTypeID()) {
            CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(versionNumber));
            SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                     CFSTR("Expected (version) number got %@"), description);
            CFReleaseSafe(description);
            goto fail;
        }
        CFNumberGetValue(versionNumber, kCFNumberCFIndexType, &pi->version);
    }

    CFDictionaryRef gestalt = CFDictionaryGetValue(pi->description, sGestaltKey);
    
    if (gestalt == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("gestalt key missing"));
        goto fail;
    }
    
    if (!isDictionary(gestalt)) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(gestalt));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected dictionary got %@"), description);
        CFReleaseSafe(description);
        goto fail;
    }

    pi->gestalt = gestalt;
    CFRetain(pi->gestalt);

    pubKey = SOSPeerInfoCopyPubKey(pi);
    require_quiet(pubKey, fail);

    pi->id = SOSCopyIDOfKey(pubKey, error);
    require_quiet(pi->id, fail);

    if(!SOSPeerInfoVerify(pi, error)) {
        SOSCreateErrorWithFormat(kSOSErrorBadSignature, NULL, error, NULL, CFSTR("Signature doesn't validate"));
        if (error)
            secerror("Can't validate PeerInfo: %@", *error);
        goto fail;
    }
    
    CFStringRef deviceName = CFDictionaryGetValue(pi->gestalt, kPIUserDefinedDeviceName);
    CFStringRef modelName = CFDictionaryGetValue(pi->gestalt, kPIDeviceModelName);
    CFStringRef ID = pi->id;
    CFMutableStringRef description = CFStringCreateMutableCopy(kCFAllocatorDefault, CFStringGetLength(deviceName), deviceName);
    if(modelName){
        CFStringAppend(description, CFSTR(", "));
        CFStringAppend(description, modelName);
    }
    if(ID){
        CFStringAppend(description, CFSTR(", "));
        CFStringAppend(description, ID);
    }
    
    pi->deviceID = CFStringCreateCopy(kCFAllocatorDefault, description);
    CFReleaseNull(description);
    CFReleaseNull(pubKey);
    return pi;

fail:
    CFReleaseNull(pi);
    CFReleaseNull(pl);
    CFReleaseNull(pubKey);

    return NULL;
}

SOSPeerInfoRef SOSPeerInfoCreateFromData(CFAllocatorRef allocator, CFErrorRef* error,
                                        CFDataRef peerinfo_data) {
    const uint8_t *der = CFDataGetBytePtr(peerinfo_data);
    CFIndex len = CFDataGetLength(peerinfo_data);
    return SOSPeerInfoCreateFromDER(NULL, error, &der, der+len);
}

static void SOSPeerInfoDestroy(CFTypeRef aObj) {
    SOSPeerInfoRef pi = (SOSPeerInfoRef) aObj;
    
    if(!pi) return;
    CFReleaseNull(pi->description);
    CFReleaseNull(pi->signature);
    CFReleaseNull(pi->gestalt);
    CFReleaseNull(pi->id);
    CFReleaseNull(pi->deviceID);
}

static Boolean SOSPeerInfoCompare(CFTypeRef lhs, CFTypeRef rhs) {
    SOSPeerInfoRef lpeer = (SOSPeerInfoRef) lhs;
    SOSPeerInfoRef rpeer = (SOSPeerInfoRef) rhs;
    if(!lpeer || !rpeer) return false;
    return CFEqualSafe(lpeer->description, rpeer->description) && CFEqualSafe(lpeer->signature, rpeer->signature);
}


CFComparisonResult SOSPeerInfoCompareByID(const void *val1, const void *val2, void *context) {
    // The code below is necessary but not sufficient; not returning a CFComparisonResult
    // It probably is OK to say that a NULL is <  <non-NULL>
    if (val1 == NULL || val2 == NULL) {
	    ptrdiff_t dv = val1 - val2;
		return dv < 0 ? kCFCompareLessThan : dv == 0 ? kCFCompareEqualTo : kCFCompareGreaterThan;
    }

	CFStringRef v1 = SOSPeerInfoGetPeerID((SOSPeerInfoRef) val1);
	CFStringRef v2 = SOSPeerInfoGetPeerID((SOSPeerInfoRef) val2);
    if (v1 == NULL || v2 == NULL) {
	    ptrdiff_t dv = (const void *)v1 - (const void *)v2;
        return dv < 0 ? kCFCompareLessThan : dv == 0 ? kCFCompareEqualTo : kCFCompareGreaterThan;
    }

    return CFStringCompare(v1, v2, 0);
}

static CFHashCode SOSPeerInfoHash(CFTypeRef cf) {
    SOSPeerInfoRef peer = (SOSPeerInfoRef) cf;

    return CFHash(peer->description) ^ CFHash(peer->signature);
}

static CFStringRef SOSPeerInfoCopyDescription(CFTypeRef aObj) {
    SOSPeerInfoRef pi = (SOSPeerInfoRef) aObj;
    CFIndex version = SOSPeerInfoGetPeerProtocolVersion(pi);
    
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSPeerInfo@%p: %s '%@' %@ %@ %ld>"),
                                    pi,
                                    SOSPeerInfoIsRetirementTicket(pi) ? "R" : "-",
                                    CFDictionaryGetValue(pi->gestalt, kPIUserDefinedDeviceName),
                                    CFDictionaryGetValue(pi->gestalt, kPIDeviceModelName),
                                    pi->id,
                                    version);
}

CFDictionaryRef SOSPeerInfoCopyPeerGestalt(SOSPeerInfoRef pi) {
    CFRetain(pi->gestalt);
    return pi->gestalt;
}

CFStringRef SOSPeerInfoGetTransportType(SOSPeerInfoRef peer){
    return peer->transportType;
}

CFStringRef SOSPeerInfoGetDeviceID(SOSPeerInfoRef peer){
    return peer->deviceID;
}

void SOSPeerInfoSetDeviceID(SOSPeerInfoRef peer, CFStringRef IDS){
    CFReleaseNull(peer->deviceID);
    peer->deviceID = CFStringCreateCopy(kCFAllocatorDefault, IDS);
}

CFStringRef SOSPeerInfoGetPeerName(SOSPeerInfoRef peer) {
    return SOSPeerInfoLookupGestaltValue(peer, kPIUserDefinedDeviceName);
}

CFStringRef SOSPeerInfoGetPeerDeviceType(SOSPeerInfoRef peer) {
    return SOSPeerInfoLookupGestaltValue(peer, kPIDeviceModelName);
}

CFIndex SOSPeerInfoGetPeerProtocolVersion(SOSPeerInfoRef peer) {
    CFIndex version = 0;
    CFTypeRef val = SOSPeerInfoLookupGestaltValue(peer, kPIMessageProtocolVersion);
    if (val && CFGetTypeID(val) == CFNumberGetTypeID())
        CFNumberGetValue(val, kCFNumberCFIndexType, &version);
    return version;
}

CFTypeRef SOSPeerInfoLookupGestaltValue(SOSPeerInfoRef pi, CFStringRef key) {
    return CFDictionaryGetValue(pi->gestalt, key);
}

CFStringRef SOSPeerInfoGetPeerID(SOSPeerInfoRef pi) {
    return pi ? pi->id : NULL;
}

CFIndex SOSPeerInfoGetVersion(SOSPeerInfoRef pi) {
    // TODO: Encode this in the DER.
    return pi->version;
}

bool SOSPeerInfoUpdateDigestWithPublicKeyBytes(SOSPeerInfoRef peer, const struct ccdigest_info *di,
                                               ccdigest_ctx_t ctx, CFErrorRef *error) {
    CFDataRef pubKeyBytes = CFDictionaryGetValue(peer->description, sPublicKeyKey);
    
    if(!pubKeyBytes) {
        SOSCreateErrorWithFormat(kSOSErrorEncodeFailure, NULL, error, NULL, CFSTR("Digest failed – no public key"));
        return false;
    }
    
    ccdigest_update(di, ctx, CFDataGetLength(pubKeyBytes), CFDataGetBytePtr(pubKeyBytes));
    
    return true;
}

bool SOSPeerInfoUpdateDigestWithDescription(SOSPeerInfoRef peer, const struct ccdigest_info *di,
                                            ccdigest_ctx_t ctx, CFErrorRef *error) {
    size_t description_size = der_sizeof_plist(peer->description, error);
    uint8_t data_begin[description_size];
    uint8_t *data_end = data_begin + description_size;
    uint8_t *encoded = der_encode_plist(peer->description, error, data_begin, data_end);
    
    if(!encoded) {
        SOSCreateErrorWithFormat(kSOSErrorEncodeFailure, NULL, error, NULL, CFSTR("Description encode failed"));
        return false;
    }
    
    ccdigest_update(di, ctx, description_size, data_begin);
    
    return true;
}


static CFDataRef sosCreateDate() {
    CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    size_t bufsiz = der_sizeof_date(now, NULL);
    uint8_t buf[bufsiz];
    der_encode_date(now, NULL, buf, buf+bufsiz);
    CFReleaseNull(now);
    return CFDataCreate(NULL, buf, bufsiz);
}

static CFDateRef sosCreateCFDate(CFDataRef sosdate) {
    CFDateRef date;
    der_decode_date(NULL, 0, &date, NULL, CFDataGetBytePtr(sosdate),
                    CFDataGetBytePtr(sosdate) + CFDataGetLength(sosdate));
    return date;
}

static bool sospeer_application_hash(SOSPeerInfoRef pi, const struct ccdigest_info *di, uint8_t *hbuf) {
    CFDataRef appdate = CFDictionaryGetValue(pi->description, sApplicationDate);
    if(!appdate) return false;
    ccdigest_di_decl(di, ctx);
    ccdigest_init(di, ctx);
    ccdigest_update(di, ctx, CFDataGetLength(appdate), CFDataGetBytePtr(appdate));
    if (!SOSPeerInfoUpdateDigestWithPublicKeyBytes(pi, di, ctx, NULL)) return false;
    ccdigest_final(di, ctx, hbuf);
    return true;
}

SOSPeerInfoRef SOSPeerInfoCopyAsApplication(SOSPeerInfoRef original, SecKeyRef userkey, SecKeyRef peerkey, CFErrorRef *error) {
    SOSPeerInfoRef result = NULL;
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(kCFAllocatorDefault, original, error);
    pi->transportType = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("KVS"));
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];
    CFDataRef usersig = NULL;
    
    CFDataRef creationDate = sosCreateDate();
    CFDictionarySetValue(pi->description, sApplicationDate, creationDate);
    CFReleaseNull(creationDate);

    // Create User Application Signature
    require_action_quiet(sospeer_application_hash(pi, di, hbuf), fail,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to create hash for peer applicant"), NULL, error));
    
    usersig = sosSignHash(userkey, di, hbuf);
    require_action_quiet(usersig, fail,
                        SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to sign public key hash for peer"), NULL, error));

    CFDictionarySetValue(pi->description, sApplicationUsig, usersig);
    
    require_quiet(SOSPeerInfoSign(peerkey, pi, error), fail);

    result = pi;
    pi = NULL;

fail:
    CFReleaseNull(usersig);
    CFReleaseNull(pi);
    return result;
}

bool SOSPeerInfoApplicationVerify(SOSPeerInfoRef pi, SecKeyRef userkey, CFErrorRef *error) {
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];
    bool result = false;

    CFDataRef usig = CFDictionaryGetValue(pi->description, sApplicationUsig);
    require_action_quiet(usig, exit,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Peer is not an applicant"), NULL, error));
    // Verify User Application Signature
    require_action_quiet(sospeer_application_hash(pi, di, hbuf), exit,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to create hash for peer applicant"), NULL, error));
    require_action_quiet(sosVerifyHash(userkey, di, hbuf, usig), exit,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("user signature of public key hash fails to verify"), NULL, error));

    result = SOSPeerInfoVerify(pi, error);

exit:
    return result;
}


static CF_RETURNS_RETAINED CFDateRef sosPeerInfoGetDate(SOSPeerInfoRef pi, CFStringRef entry) {
    if(!pi) return NULL;
    CFDataRef sosdate = CFDictionaryGetValue(pi->description, entry);
    if(!sosdate) return NULL;
    CFDateRef date = sosCreateCFDate(sosdate);
    
    return date;
}

CF_RETURNS_RETAINED CFDateRef SOSPeerInfoGetApplicationDate(SOSPeerInfoRef pi) {
    return sosPeerInfoGetDate(pi, sApplicationDate);
}

CF_RETURNS_RETAINED CFDateRef SOSPeerInfoGetRetirementDate(SOSPeerInfoRef pi) {
    return sosPeerInfoGetDate(pi, sRetirementDate);
}


size_t SOSPeerInfoGetDEREncodedSize(SOSPeerInfoRef peer, CFErrorRef *error) {
    size_t plist_size = der_sizeof_plist(peer->description, error);
    if (plist_size == 0)
        return 0;
    
    size_t signature_size = der_sizeof_data(peer->signature, error);
    if (signature_size == 0)
        return 0;
    
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        plist_size + signature_size);
}

uint8_t* SOSPeerInfoEncodeToDER(SOSPeerInfoRef peer, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                       der_encode_plist(peer->description, error, der,
                                       der_encode_data(peer->signature, error, der, der_end)));
}

CFDataRef SOSPeerInfoCopyEncodedData(SOSPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error) {
    size_t size = SOSPeerInfoGetDEREncodedSize(peer, error);
    if (size == 0) return NULL;

    uint8_t buffer[size];
    uint8_t* start = SOSPeerInfoEncodeToDER(peer, error, buffer, buffer + sizeof(buffer));
    CFDataRef result = CFDataCreate(kCFAllocatorDefault, start, size);
    return result;
}


//
// Gestalt helpers
//

CFStringRef SOSPeerGestaltGetName(CFDictionaryRef gestalt) {
    CFStringRef name = SOSPeerGestaltGetAnswer(gestalt, kPIUserDefinedDeviceName);
    return isString(name) ? name : NULL;
}

CFTypeRef SOSPeerGestaltGetAnswer(CFDictionaryRef gestalt, CFStringRef question) {
    return gestalt ? CFDictionaryGetValue(gestalt, question) : NULL;
}

//
// Peer Retirement
//


SOSPeerInfoRef SOSPeerInfoCreateRetirementTicket(CFAllocatorRef allocator, SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error) {
    // Copy PeerInfo
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(allocator, peer, error);

    require(pi, fail);

    // Fill out Resignation Date
    CFDataRef resignationDate = sosCreateDate();
    CFDictionaryAddValue(pi->description, sRetirementDate, resignationDate);
    CFReleaseNull(resignationDate);

    require(SOSPeerInfoSign(privKey, pi, error), fail);

    return pi;

fail:
    CFReleaseNull(pi);
    return NULL;
}

CFStringRef SOSPeerInfoInspectRetirementTicket(SOSPeerInfoRef pi, CFErrorRef *error) {
    CFStringRef retval = NULL;
    CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    CFDateRef retirement = NULL;
    
    require_quiet(SOSPeerInfoVerify(pi, error), err);

    retirement = sosCreateCFDate(CFDictionaryGetValue(pi->description, sRetirementDate));

    require_action_quiet(retirement, err,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Peer is not retired"), NULL, error));

    require_action_quiet(CFDateCompare(now, retirement, NULL) == kCFCompareGreaterThan, err,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Retirement date is after current date"), NULL, error));

    retval = SOSPeerInfoGetPeerID(pi);

err:
    CFReleaseNull(now);
    CFReleaseNull(retirement);
    return retval;
}

bool SOSPeerInfoRetireRetirementTicket(size_t max_seconds, SOSPeerInfoRef pi) {
    CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    CFDateRef retirement = sosCreateCFDate(CFDictionaryGetValue(pi->description, sRetirementDate));
    CFTimeInterval timediff = CFDateGetTimeIntervalSinceDate(now, retirement); // diff in seconds
    CFReleaseNull(now);
    CFReleaseNull(retirement);
    if(timediff > (max_seconds)) return true;
    return false;
}

bool SOSPeerInfoIsRetirementTicket(SOSPeerInfoRef pi) {
    CFDataRef flag = CFDictionaryGetValue(pi->description, sRetirementDate);
    return flag != NULL;
}

bool SOSPeerInfoIsCloudIdentity(SOSPeerInfoRef pi) {
    CFTypeRef value = CFDictionaryGetValue(pi->description, sCloudIdentityKey);
    return CFEqualSafe(value, kCFBooleanTrue);
}

SOSPeerInfoRef SOSPeerInfoUpgradeSignatures(CFAllocatorRef allocator, SecKeyRef privKey, SecKeyRef peerKey, SOSPeerInfoRef peer, CFErrorRef *error) {
    SecKeyRef pubKey = SecKeyCreatePublicFromPrivate(privKey);
    SOSPeerInfoRef retval = NULL;
    
    retval = SOSPeerInfoCopyAsApplication(peer, privKey, peerKey, error);
    CFReleaseNull(pubKey);
    return retval;
}

