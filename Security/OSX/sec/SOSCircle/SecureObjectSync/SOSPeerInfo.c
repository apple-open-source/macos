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

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSInternal.h>
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

CFGiblisWithHashFor(SOSPeerInfo);


const CFStringRef kPIUserDefinedDeviceNameKey   = CFSTR("ComputerName");
const CFStringRef kPIDeviceModelNameKey         = CFSTR("ModelName");
const CFStringRef kPIMessageProtocolVersionKey  = CFSTR("MessageProtocolVersion");
const CFStringRef kPIOSVersionKey               = CFSTR("OSVersion");

// Description Dictionary Entries
static CFStringRef sPublicKeyKey        = CFSTR("PublicSigningKey");
const CFStringRef sGestaltKey          = CFSTR("DeviceGestalt");
const CFStringRef sVersionKey          = CFSTR("ConflictVersion");
static CFStringRef sCloudIdentityKey    = CFSTR("CloudIdentity");
static CFStringRef sApplicationDate     = CFSTR("ApplicationDate");
static CFStringRef sApplicationUsig     = CFSTR("ApplicationUsig");
static CFStringRef sRetirementDate      = CFSTR("RetirementDate");

// Peerinfo Entries
CFStringRef kSOSPeerInfoDescriptionKey = CFSTR("SOSPeerInfoDescription");
CFStringRef kSOSPeerInfoSignatureKey = CFSTR("SOSPeerInfoSignature");
CFStringRef kSOSPeerInfoNameKey = CFSTR("SOSPeerInfoName");

//Peer Info V2 Dictionary IDS keys
CFStringRef sPreferIDS                  = CFSTR("PreferIDS");
CFStringRef sPreferIDSFragmentation     = CFSTR("PreferIDFragmentation");
CFStringRef sPreferIDSACKModel          = CFSTR("PreferIDSAckModel");
CFStringRef sTransportType              = CFSTR("TransportType");
CFStringRef sDeviceID                   = CFSTR("DeviceID");

const CFStringRef peerIDLengthKey             = CFSTR("idLength");

SOSPeerInfoRef SOSPeerInfoAllocate(CFAllocatorRef allocator) {
    return  CFTypeAllocate(SOSPeerInfo, struct __OpaqueSOSPeerInfo, allocator);
}

SecKeyRef SOSPeerInfoCopyPubKey(SOSPeerInfoRef peer, CFErrorRef* error) {
    SecKeyRef result = NULL;

    CFDataRef pubKeyBytes = asData(CFDictionaryGetValue(peer->description, sPublicKeyKey), error);
    require_quiet(pubKeyBytes, fail);

    CFAllocatorRef allocator = CFGetAllocator(peer);
    result = SecKeyCreateFromPublicData(allocator, kSecECDSAAlgorithmID, pubKeyBytes);

    require_quiet(SecAllocationError(result, error, CFSTR("Failed to create public key from data %@"), pubKeyBytes), fail);

fail:
    return result;
}

CFDataRef SOSPeerInfoGetAutoAcceptInfo(SOSPeerInfoRef peer) {
	CFDataRef pubKeyBytes = NULL;

	pubKeyBytes = CFDictionaryGetValue(peer->description, sPublicKeyKey);
	if (!pubKeyBytes || CFGetTypeID(pubKeyBytes) != CFDataGetTypeID()) {
		pubKeyBytes = NULL;
	}

	return pubKeyBytes;
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
static CFDataRef sosCopySignedHash(SecKeyRef privkey, const struct ccdigest_info *di, uint8_t *hbuf) {
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

bool SOSPeerInfoSign(SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool status = false;
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];
    CFDataRef newSignature = NULL;
    
    require_action_quiet(SOSDescriptionHash(peer, di, hbuf, error), fail,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to hash description for peer"), NULL, error));
    
    newSignature = sosCopySignedHash(privKey, di, hbuf);
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
bool SOSPeerInfoVerify(SOSPeerInfoRef peer, CFErrorRef *error) {
    bool result = false;
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];

    SecKeyRef pubKey = SOSPeerInfoCopyPubKey(peer, error);
    require_quiet(pubKey, error_out);

    require_quiet(SOSDescriptionHash(peer, di, hbuf, error), error_out);

    require_action_quiet(sosVerifyHash(pubKey, di, hbuf, peer->signature), error_out,
                         SOSErrorCreate(kSOSErrorBadSignature, error, NULL,
                                        CFSTR("Signature didn't verify for %@"), peer));
    result = true;

error_out:
    CFReleaseNull(pubKey);
    return result;
}

void SOSPeerInfoSetVersionNumber(SOSPeerInfoRef pi, int version) {
    pi->version = version;
    CFNumberRef versionNumber = CFNumberCreateWithCFIndex(NULL, pi->version);
    CFDictionarySetValue(pi->description, sVersionKey,   versionNumber);
    CFReleaseNull(versionNumber);
}

static SOSPeerInfoRef SOSPeerInfoCreate_Internal(CFAllocatorRef allocator,
                                                 CFDictionaryRef gestalt, CFDataRef backup_key,
                                                 CFStringRef IDSID, CFStringRef transportType, CFBooleanRef preferIDS,
                                                 CFBooleanRef preferFragmentation, CFBooleanRef preferAckModel,
                                                 CFSetRef enabledViews,
                                                 SecKeyRef signingKey, CFErrorRef* error,
                                                 void (^ description_modifier)(CFMutableDictionaryRef description)) {
    SOSPeerInfoRef pi = CFTypeAllocate(SOSPeerInfo, struct __OpaqueSOSPeerInfo, allocator);
    pi->gestalt = gestalt;
    CFRetain(pi->gestalt);
    
    pi->version = SOSPeerInfoGetPeerProtocolVersion(pi);
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
    
    
    pi->id = SOSCopyIDOfKey(publicKey, error);
    CFReleaseNull(publicKey);
    
    require_quiet(pi->id, exit);
    
    // ================ V2 Additions Start
    
    if(!SOSPeerInfoUpdateToV2(pi, error)) {
        CFReleaseNull(pi);
        goto exit;
    }
    
    // V2DictionarySetValue handles NULL as remove
    if (backup_key != NULL) SOSPeerInfoV2DictionarySetValue(pi, sBackupKeyKey, backup_key);
    SOSPeerInfoV2DictionarySetValue(pi, sDeviceID, IDSID);
    SOSPeerInfoV2DictionarySetValue(pi, sTransportType, transportType);
    SOSPeerInfoV2DictionarySetValue(pi, sPreferIDS, preferIDS);
    SOSPeerInfoV2DictionarySetValue(pi, sPreferIDSFragmentation, preferFragmentation);
    SOSPeerInfoV2DictionarySetValue(pi, sPreferIDSACKModel, preferAckModel);
    SOSPeerInfoV2DictionarySetValue(pi, sViewsKey, enabledViews);

    // ================ V2 Additions End
    
    if (!SOSPeerInfoSign(signingKey, pi, error)) {
        CFReleaseNull(pi);
        goto exit;
    }

exit:
    CFReleaseNull(versionNumber);
    CFReleaseNull(publicBytes);
    return pi;
}

SOSPeerInfoRef SOSPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt, CFDataRef backup_key, SecKeyRef signingKey, CFErrorRef* error) {
    return SOSPeerInfoCreate_Internal(allocator, gestalt, backup_key, NULL, NULL, NULL, NULL, NULL, NULL, signingKey, error, ^(CFMutableDictionaryRef description) {});
}

SOSPeerInfoRef SOSPeerInfoCreateWithTransportAndViews(CFAllocatorRef allocator, CFDictionaryRef gestalt, CFDataRef backup_key,
                                                      CFStringRef IDSID, CFStringRef transportType, CFBooleanRef preferIDS,
                                                      CFBooleanRef preferFragmentation, CFBooleanRef preferAckModel, CFSetRef enabledViews, SecKeyRef signingKey, CFErrorRef* error)
{
    return SOSPeerInfoCreate_Internal(allocator, gestalt, backup_key, IDSID, transportType, preferIDS, preferFragmentation, preferAckModel, enabledViews, signingKey, error, ^(CFMutableDictionaryRef description) {});
}


SOSPeerInfoRef SOSPeerInfoCreateCloudIdentity(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error) {
    return SOSPeerInfoCreate_Internal(allocator, gestalt, NULL, NULL, NULL, NULL, NULL, NULL, NULL, signingKey, error, ^(CFMutableDictionaryRef description) {
        CFDictionarySetValue(description, sCloudIdentityKey, kCFBooleanTrue);
    });

}


SOSPeerInfoRef SOSPeerInfoCreateCopy(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFErrorRef* error) {
    if(!toCopy) return NULL;
    SOSPeerInfoRef pi = CFTypeAllocate(SOSPeerInfo, struct __OpaqueSOSPeerInfo, allocator);

    pi->description = CFDictionaryCreateMutableCopy(allocator, 0, toCopy->description);
    pi->signature = CFDataCreateCopy(allocator, toCopy->signature);

    pi->gestalt = CFDictionaryCreateCopy(allocator, toCopy->gestalt);
    pi->id = CFStringCreateCopy(allocator, toCopy->id);

    pi->version = toCopy->version;
    if(!SOSPeerInfoVersionHasV2Data(pi)) SOSPeerInfoExpandV2Data(pi, error);

    return pi;
}


bool SOSPeerInfoVersionIsCurrent(SOSPeerInfoRef pi) {
    return pi->version >= PEERINFO_CURRENT_VERSION;
}

bool SOSPeerInfoVersionHasV2Data(SOSPeerInfoRef pi) {
    return pi->version >= kSOSPeerV2BaseVersion;
}

SOSPeerInfoRef SOSPeerInfoCreateCurrentCopy(CFAllocatorRef allocator, SOSPeerInfoRef toCopy,
                                            CFStringRef IDSID, CFStringRef transportType, CFBooleanRef preferIDS, CFBooleanRef preferFragmentation, CFBooleanRef preferAckModel, CFSetRef enabledViews,
                                            SecKeyRef signingKey, CFErrorRef* error) {
    
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(allocator, toCopy, error);
    if(!SOSPeerInfoVersionHasV2Data(pi)) SOSPeerInfoUpdateToV2(pi, error);
    
    //SOSPeerInfoSetSerialNumber(pi);

    if (IDSID) {
        SOSPeerInfoV2DictionarySetValue(pi, sDeviceID, IDSID);
    }
    if (transportType) {
        SOSPeerInfoV2DictionarySetValue(pi, sTransportType, transportType);
    }
    if (preferIDS) {
        SOSPeerInfoV2DictionarySetValue(pi, sPreferIDS, preferIDS);
    }
    if (preferFragmentation) {
        SOSPeerInfoV2DictionarySetValue(pi, sPreferIDSFragmentation, preferFragmentation);
    }
    if (preferAckModel) {
        SOSPeerInfoV2DictionarySetValue(pi, sPreferIDSACKModel, preferAckModel);
    }
    if (enabledViews) {
        SOSPeerInfoV2DictionarySetValue(pi, sViewsKey, enabledViews);
    }

    if(!SOSPeerInfoSign(signingKey, pi, error)) {
        CFReleaseNull(pi);
    }

    return pi;
}


static SOSPeerInfoRef SOSPeerInfoCopyWithModification(CFAllocatorRef allocator, SOSPeerInfoRef original,
                                                      SecKeyRef signingKey, CFErrorRef *error,
                                                      bool (^modification)(SOSPeerInfoRef peerToModify, CFErrorRef *error)) {

    SOSPeerInfoRef result = NULL;
    SOSPeerInfoRef copy = SOSPeerInfoCreateCopy(allocator, original, error);

    require_quiet(modification(copy, error), fail);

    require_quiet(SOSPeerInfoSign(signingKey, copy, error), fail);

    CFTransferRetained(result, copy);

fail:
    CFReleaseNull(copy);
    return result;

}

SOSPeerInfoRef SOSPeerInfoCopyWithGestaltUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error) {
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
        if(!gestalt || !peerToModify) return false;
        CFRetainAssign(peerToModify->gestalt, gestalt);
        CFDictionarySetValue(peerToModify->description, sGestaltKey, peerToModify->gestalt);
        return true;

    });
}


SOSPeerInfoRef SOSPeerInfoCopyWithBackupKeyUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDataRef backupKey, SecKeyRef signingKey, CFErrorRef *error) {
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
        if (backupKey != NULL)
            SOSPeerInfoV2DictionarySetValue(peerToModify, sBackupKeyKey, backupKey);
        else
            SOSPeerInfoV2DictionaryRemoveValue(peerToModify, sBackupKeyKey);
        return true;
    });
}

static CFDictionaryRef SOSPeerInfoUpdateAndCopyRecord(SOSPeerInfoRef peer, CFStringRef dsid, CFDictionaryRef escrowRecord){
   
    CFMutableDictionaryRef existingEscrowRecords = SOSPeerInfoCopyEscrowRecord(peer);
    
    if(escrowRecord == NULL && existingEscrowRecords != NULL)
    {
        CFDictionaryRemoveValue(existingEscrowRecords, dsid);
        return existingEscrowRecords;
    }
    
    if(existingEscrowRecords == NULL)
        existingEscrowRecords = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionarySetValue(existingEscrowRecords, dsid, escrowRecord);
    
    return existingEscrowRecords;
}


SOSPeerInfoRef SOSPeerInfoCopyWithEscrowRecordUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFStringRef dsid, CFDictionaryRef escrowRecord, SecKeyRef signingKey, CFErrorRef *error) {
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
                                               
            CFDictionaryRef updatedEscrowRecords = SOSPeerInfoUpdateAndCopyRecord(peerToModify, dsid, escrowRecord);
            SOSPeerInfoV2DictionarySetValue(peerToModify, sEscrowRecord, updatedEscrowRecords);
            CFReleaseNull(updatedEscrowRecords);
            return true;
    });
}

SOSPeerInfoRef SOSPeerInfoCopyWithReplacedEscrowRecords(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef escrowRecords, SecKeyRef signingKey, CFErrorRef *error) {
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
            if(escrowRecords != NULL)
                SOSPeerInfoV2DictionarySetValue(peerToModify, sEscrowRecord, escrowRecords);
            
            return true;
    });
}

CFDataRef SOSPeerInfoCopyBackupKey(SOSPeerInfoRef peer) {
    return SOSPeerInfoV2DictionaryCopyData(peer, sBackupKeyKey);
}

CFMutableDictionaryRef SOSPeerInfoCopyEscrowRecord(SOSPeerInfoRef peer){
    return SOSPeerInfoV2DictionaryCopyDictionary(peer, sEscrowRecord);
}

bool SOSPeerInfoHasBackupKey(SOSPeerInfoRef peer) {
    CFDataRef bk = SOSPeerInfoCopyBackupKey(peer);
    bool success = bk != NULL;
    CFReleaseNull(bk);
    return success;
}

SOSPeerInfoRef SOSPeerInfoCopyWithViewsChange(CFAllocatorRef allocator, SOSPeerInfoRef toCopy,
                                              SOSViewActionCode action, CFStringRef viewname, SOSViewResultCode *retval,
                                              SecKeyRef signingKey, CFErrorRef* error) {
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(allocator, toCopy, error);
    if(action == kSOSCCViewEnable) {
        *retval = SOSViewsEnable(pi, viewname, error);
        require((kSOSCCViewMember == *retval), exit);
    } else if(action == kSOSCCViewDisable) {
        *retval = SOSViewsDisable(pi, viewname, error);
        require((kSOSCCViewNotMember == *retval), exit);
    }
    
    require_action_quiet(SOSPeerInfoSign(signingKey, pi, error), exit, *retval = kSOSCCGeneralViewError);
    return pi;

exit:
    CFReleaseNull(pi);
    return NULL;
}


CFStringRef sPingKey                   = CFSTR("Ping");

SOSPeerInfoRef SOSPeerInfoCopyWithPing(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, SecKeyRef signingKey, CFErrorRef* error) {
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(allocator, toCopy, error);
    CFDataRef ping = CFDataCreateWithRandomBytes(8);
    SOSPeerInfoV2DictionarySetValue(pi, sPingKey, ping);
    SecKeyRef pub_key = SOSPeerInfoCopyPubKey(pi, error);
    require_quiet(pub_key, exit);
    pi->id = SOSCopyIDOfKey(pub_key, error);
    require_quiet(pi->id, exit);
    require_action_quiet(SOSPeerInfoSign(signingKey, pi, error), exit, CFReleaseNull(pi));
exit:
    CFReleaseNull(ping);
    CFReleaseNull(pub_key);
    return pi;
}


SOSViewResultCode SOSPeerInfoViewStatus(SOSPeerInfoRef pi, CFStringRef view, CFErrorRef *error) {
    return SOSViewsQuery(pi, view, error);
}


SOSPeerInfoRef SOSPeerInfoCopyWithSecurityPropertyChange(CFAllocatorRef allocator, SOSPeerInfoRef toCopy,
                                                         SOSSecurityPropertyActionCode action, CFStringRef property, SOSSecurityPropertyResultCode *retval,
                                                         SecKeyRef signingKey, CFErrorRef* error) {
    SOSPeerInfoRef pi = SOSPeerInfoCreateCopy(allocator, toCopy, error);
    if(action == kSOSCCSecurityPropertyEnable) {
        *retval = SOSSecurityPropertyEnable(pi, property, error);
        require((kSOSCCSecurityPropertyValid == *retval), exit);
    } else if(action == kSOSCCSecurityPropertyDisable) {
        *retval = SOSSecurityPropertyDisable(pi, property, error);
        require((kSOSCCSecurityPropertyNotValid == *retval), exit);
    }
    
    require_action_quiet(SOSPeerInfoSign(signingKey, pi, error), exit, *retval = kSOSCCGeneralViewError);
    return pi;
    
exit:
    CFReleaseNull(pi);
    return NULL;
}

SOSViewResultCode SOSPeerInfoSecurityPropertyStatus(SOSPeerInfoRef pi, CFStringRef property, CFErrorRef *error) {
        return SOSSecurityPropertyQuery(pi, property, error);
    }



static void SOSPeerInfoDestroy(CFTypeRef aObj) {
    SOSPeerInfoRef pi = (SOSPeerInfoRef) aObj;
    
    if(!pi) return;
    CFReleaseNull(pi->description);
    CFReleaseNull(pi->signature);
    CFReleaseNull(pi->gestalt);
    CFReleaseNull(pi->id);
    if(pi->v2Dictionary) CFReleaseNull(pi->v2Dictionary);
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


static char boolToChars(bool val, char truechar, char falsechar) {
    return val? truechar: falsechar;
}

static CFStringRef isKnown(CFStringRef ref) {
    return ref? ref: CFSTR("Unknown ");
}

static CFStringRef copyDescriptionWithFormatOptions(CFTypeRef aObj, CFDictionaryRef formatOptions){
    
    SOSPeerInfoRef pi = (SOSPeerInfoRef) aObj;
    if(!pi) return NULL;

    CFStringRef description = NULL;
    // Get the format options we care about:
    bool retired = SOSPeerInfoIsRetirementTicket(pi);
    bool selfValid  = SOSPeerInfoVerify(pi, NULL);
    bool backingUp = SOSPeerInfoHasBackupKey(pi);
    bool isKVS = SOSPeerInfoKVSOnly(pi);
    CFStringRef osVersion = CFDictionaryGetValue(pi->gestalt, kPIOSVersionKey);
    CFStringRef tmp = SOSPeerInfoV2DictionaryCopyString(pi, sDeviceID);
    CFStringRef deviceID = CFStringCreateTruncatedCopy(tmp, 8);
    CFReleaseNull(tmp);
    CFStringRef serialNum = SOSPeerInfoCopySerialNumber(pi);
    CFStringRef peerID = CFStringCreateTruncatedCopy(SOSPeerInfoGetPeerID(pi), 8);

    // Calculate the truncated length

    CFStringRef objectPrefix = CFStringCreateWithFormat(kCFAllocatorDefault, formatOptions, CFSTR("PI@%p"), pi);
    
    description = CFStringCreateWithFormat(kCFAllocatorDefault, formatOptions,
                                           CFSTR("<%@: [name: %20@] [%c%c%c%c%c%c%c] [type: %-20@] [spid: %8@] [os: %10@] [devid: %10@] [serial: %12@]"),
                                             objectPrefix,
                                             isKnown(SOSPeerInfoGetPeerName(pi)),
                                             '-',
                                             '-',
                                             boolToChars(selfValid, 'S', 's'),
                                             boolToChars(retired, 'R', 'r'),
                                             boolToChars(backingUp, 'B', 'b'),
                                             boolToChars(isKVS, 'K', 'I'),
                                             '-',
                                             isKnown(SOSPeerInfoGetPeerDeviceType(pi)),  isKnown(peerID),
                                             isKnown(osVersion), isKnown(deviceID), isKnown(serialNum));


    CFReleaseNull(peerID);
    CFReleaseNull(deviceID);
    CFReleaseNull(serialNum);
    CFReleaseNull(objectPrefix);

    return description;
}

static CFStringRef SOSPeerInfoCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    
    CFStringRef description = NULL;

    description = copyDescriptionWithFormatOptions(aObj, formatOptions);

    return description;
}

void SOSPeerInfoLogState(char *category, SOSPeerInfoRef pi, SecKeyRef pubKey, CFStringRef myPID, char sigchr) {
    if(!pi) return;
    bool appValid = SOSPeerInfoApplicationVerify(pi, pubKey, NULL);
    bool retired = SOSPeerInfoIsRetirementTicket(pi);
    bool selfValid  = SOSPeerInfoVerify(pi, NULL);
    bool backingUp = SOSPeerInfoHasBackupKey(pi);
    bool isMe = CFEqualSafe(SOSPeerInfoGetPeerID(pi), myPID) == true;
    bool isKVS = SOSPeerInfoKVSOnly(pi);
    CFStringRef osVersion = CFDictionaryGetValue(pi->gestalt, kPIOSVersionKey);
    CFStringRef tmp = SOSPeerInfoV2DictionaryCopyString(pi, sDeviceID);
    CFStringRef deviceID = CFStringCreateTruncatedCopy(tmp, 8);
    CFReleaseNull(tmp);
    CFStringRef serialNum = SOSPeerInfoCopySerialNumber(pi);
    CFStringRef peerID = CFStringCreateTruncatedCopy(SOSPeerInfoGetPeerID(pi), 8);

    secnotice(category, "PI:    [name: %-20@] [%c%c%c%c%c%c%c] [type: %-20@] [spid: %8@] [os: %10@] [devid: %10@] [serial: %12@]", isKnown(SOSPeerInfoGetPeerName(pi)),
              boolToChars(isMe, 'M', 'm'),
              boolToChars(appValid, 'A', 'a'),
              boolToChars(selfValid, 'S', 's'),
              boolToChars(retired, 'R', 'r'),
              boolToChars(backingUp, 'B', 'b'),
              boolToChars(isKVS, 'K', 'I'),
              sigchr,
              isKnown(SOSPeerInfoGetPeerDeviceType(pi)),  isKnown(peerID),
              isKnown(osVersion), isKnown(deviceID), isKnown(serialNum));
    
    CFReleaseNull(peerID);
    CFReleaseNull(deviceID);
    CFReleaseNull(serialNum);
}

CFDictionaryRef SOSPeerInfoCopyPeerGestalt(SOSPeerInfoRef pi) {
    CFRetain(pi->gestalt);
    return pi->gestalt;
}

CFDictionaryRef SOSPeerGetGestalt(SOSPeerInfoRef pi){
    return pi->gestalt;
}

CFStringRef SOSPeerInfoGetPeerName(SOSPeerInfoRef peer) {
    return SOSPeerInfoLookupGestaltValue(peer, kPIUserDefinedDeviceNameKey);
}

CFStringRef SOSPeerInfoGetPeerDeviceType(SOSPeerInfoRef peer) {
    return SOSPeerInfoLookupGestaltValue(peer, kPIDeviceModelNameKey);
}

CFIndex SOSPeerInfoGetPeerProtocolVersion(SOSPeerInfoRef peer) {
    CFIndex version = PEERINFO_CURRENT_VERSION;
    CFTypeRef val = SOSPeerInfoLookupGestaltValue(peer, kPIMessageProtocolVersionKey);
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

bool SOSPeerInfoPeerIDEqual(SOSPeerInfoRef pi, CFStringRef myPeerID) {
    return CFEqualSafe(myPeerID, SOSPeerInfoGetPeerID(pi));
}

CFIndex SOSPeerInfoGetVersion(SOSPeerInfoRef pi) {
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
    if(SOSPeerInfoVersionHasV2Data(peer)) SOSPeerInfoPackV2Data(peer);
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
    CFDataRef appdate = asData(CFDictionaryGetValue(pi->description, sApplicationDate), NULL);
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
    
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t hbuf[di->output_size];
    CFDataRef usersig = NULL;
    
    CFDataRef creationDate = sosCreateDate();
    CFDictionarySetValue(pi->description, sApplicationDate, creationDate);
    CFReleaseNull(creationDate);

    // Create User Application Signature
    require_action_quiet(sospeer_application_hash(pi, di, hbuf), fail,
                         SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Failed to create hash for peer applicant"), NULL, error));
    
    usersig = sosCopySignedHash(userkey, di, hbuf);
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



//
// Gestalt helpers
//

CFStringRef SOSPeerGestaltGetName(CFDictionaryRef gestalt) {
    CFStringRef name = SOSPeerGestaltGetAnswer(gestalt, kPIUserDefinedDeviceNameKey);
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

CFBooleanRef SOSPeerInfoCopyIDSPreference(SOSPeerInfoRef peer){
    CFBooleanRef preference = (CFBooleanRef)SOSPeerInfoV2DictionaryCopyBoolean(peer, sPreferIDS);
    return (preference ? preference : CFRetain(kCFBooleanFalse));
}


SOSPeerInfoRef SOSPeerInfoSetIDSPreference(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFBooleanRef preference, SecKeyRef signingKey, CFErrorRef *error){
            return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                               ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
                                                   SOSPeerInfoV2DictionarySetValue(peerToModify, sPreferIDS, preference);
                                                   return true;
                                               });
}

CFBooleanRef SOSPeerInfoCopyIDSFragmentationPreference(SOSPeerInfoRef peer){
    CFBooleanRef preference = (CFBooleanRef)SOSPeerInfoV2DictionaryCopyBoolean(peer, sPreferIDSFragmentation);
    return (preference ? preference : CFRetain(kCFBooleanFalse));
}

CFBooleanRef SOSPeerInfoCopyIDSACKModelPreference(SOSPeerInfoRef peer){
    CFBooleanRef preference = (CFBooleanRef)SOSPeerInfoV2DictionaryCopyBoolean(peer, sPreferIDSACKModel);
    return (preference ? preference : CFRetain(kCFBooleanFalse));
}

SOSPeerInfoRef SOSPeerInfoSetIDSFragmentationPreference(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFBooleanRef preference, SecKeyRef signingKey, CFErrorRef *error){
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
                                               SOSPeerInfoV2DictionarySetValue(peerToModify, sPreferIDSFragmentation, preference);
                                               return true;
                                           });
}

SOSPeerInfoRef SOSPeerInfoSetIDSACKModelPreference(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFBooleanRef preference, SecKeyRef signingKey, CFErrorRef *error){
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
                                               SOSPeerInfoV2DictionarySetValue(peerToModify, sPreferIDSACKModel, preference);
                                               return true;
                                           });
}

bool SOSPeerInfoTransportTypeIs(SOSPeerInfoRef pi, CFStringRef transportType) {
    return SOSPeerInfoV2DictionaryHasStringValue(pi, sTransportType, transportType);
}

CFStringRef SOSPeerInfoCopyTransportType(SOSPeerInfoRef peer){
    CFStringRef transportType = (CFStringRef)SOSPeerInfoV2DictionaryCopyString(peer, sTransportType);
    return (transportType ? transportType : CFRetain(SOSTransportMessageTypeKVS));
}

SOSPeerInfoRef SOSPeerInfoSetTransportType(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFStringRef transportType, SecKeyRef signingKey, CFErrorRef *error){
    
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
                                               SOSPeerInfoV2DictionarySetValue(peerToModify, sTransportType, transportType);
                                               return true;
                                           });
}

bool SOSPeerInfoKVSOnly(SOSPeerInfoRef pi) {
    CFStringRef transportType = SOSPeerInfoCopyTransportType(pi);
    bool retval = CFEqualSafe(transportType, SOSTransportMessageTypeKVS);
    CFReleaseNull(transportType);
    return retval;
}

bool SOSPeerInfoHasDeviceID(SOSPeerInfoRef peer) {
    return SOSPeerInfoV2DictionaryHasString(peer, sDeviceID);
}

CFStringRef SOSPeerInfoCopyDeviceID(SOSPeerInfoRef peer){
    return (CFStringRef)SOSPeerInfoV2DictionaryCopyString(peer, sDeviceID);
}

SOSPeerInfoRef SOSPeerInfoSetDeviceID(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFStringRef IDS, SecKeyRef signingKey, CFErrorRef *error){
    
    return SOSPeerInfoCopyWithModification(allocator, toCopy, signingKey, error,
                                           ^bool(SOSPeerInfoRef peerToModify, CFErrorRef *error) {
                                               SOSPeerInfoV2DictionarySetValue(peerToModify, sDeviceID, IDS);
                                               return true;
                                           });
}

bool SOSPeerInfoShouldUseIDSTransport(SOSPeerInfoRef myPeer, SOSPeerInfoRef theirPeer){
    return SOSPeerInfoHasDeviceID(myPeer) && SOSPeerInfoTransportTypeIs(myPeer, SOSTransportMessageTypeIDSV2) &&
           SOSPeerInfoHasDeviceID(theirPeer) && SOSPeerInfoTransportTypeIs(theirPeer, SOSTransportMessageTypeIDSV2);
}

bool SOSPeerInfoShouldUseIDSMessageFragmentation(SOSPeerInfoRef myPeer, SOSPeerInfoRef theirPeer){
    
    bool success = false;
    
    CFBooleanRef myPreference = SOSPeerInfoCopyIDSFragmentationPreference(myPeer);
    
    CFBooleanRef theirPreference = SOSPeerInfoCopyIDSFragmentationPreference(theirPeer);
    secnotice("IDS Transport", "mypreference: %@, theirpreference: %@", myPreference, theirPreference);
    if((myPreference == kCFBooleanTrue && theirPreference == kCFBooleanTrue))
        success = true;
    
    CFReleaseNull(myPreference);
    CFReleaseNull(theirPreference);
    return success;
}

bool SOSPeerInfoShouldUseACKModel(SOSPeerInfoRef myPeer, SOSPeerInfoRef theirPeer){
    bool success = false;

    CFBooleanRef myPreference = SOSPeerInfoCopyIDSACKModelPreference(myPeer);

    CFBooleanRef theirPreference = SOSPeerInfoCopyIDSACKModelPreference(theirPeer);
    secnotice("IDS Transport", "mypreference: %@, theirpreference: %@", myPreference, theirPreference);
    if((myPreference == kCFBooleanTrue && theirPreference == kCFBooleanTrue))
        success = true;

    CFReleaseNull(myPreference);
    CFReleaseNull(theirPreference);
    return success;

}

SOSPeerInfoDeviceClass SOSPeerInfoGetClass(SOSPeerInfoRef pi) {
    static CFDictionaryRef devID2Class = NULL;
    static dispatch_once_t onceToken = 0;
    
    dispatch_once(&onceToken, ^{
        CFNumberRef cfSOSPeerInfo_macOS = CFNumberCreateWithCFIndex(kCFAllocatorDefault, SOSPeerInfo_macOS);
        CFNumberRef cfSOSPeerInfo_iOS = CFNumberCreateWithCFIndex(kCFAllocatorDefault, SOSPeerInfo_iOS);
        CFNumberRef cfSOSPeerInfo_iCloud = CFNumberCreateWithCFIndex(kCFAllocatorDefault, SOSPeerInfo_iCloud);
        // CFNumberRef cfSOSPeerInfo_watchOS = CFNumberCreateWithCFIndex(kCFAllocatorDefault, SOSPeerInfo_watchOS);
        // CFNumberRef cfSOSPeerInfo_tvOS = CFNumberCreateWithCFIndex(kCFAllocatorDefault, SOSPeerInfo_tvOS);

        devID2Class =     CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       CFSTR("Mac Pro"), cfSOSPeerInfo_macOS,
                                                       CFSTR("MacBook"), cfSOSPeerInfo_macOS,
                                                       CFSTR("MacBook Pro"), cfSOSPeerInfo_macOS,
                                                       CFSTR("iCloud"), cfSOSPeerInfo_iCloud,
                                                       CFSTR("iMac"), cfSOSPeerInfo_macOS,
                                                       CFSTR("iPad"), cfSOSPeerInfo_iOS,
                                                       CFSTR("iPhone"), cfSOSPeerInfo_iOS,
                                                       CFSTR("iPod touch"), cfSOSPeerInfo_iOS,
                                                       NULL);
        CFReleaseNull(cfSOSPeerInfo_macOS);
        CFReleaseNull(cfSOSPeerInfo_iOS);
        CFReleaseNull(cfSOSPeerInfo_iCloud);
    });
    SOSPeerInfoDeviceClass retval = SOSPeerInfo_unknown;
    CFStringRef dt = SOSPeerInfoGetPeerDeviceType(pi);
    require_quiet(dt, errOut);
    CFNumberRef classNum = CFDictionaryGetValue(devID2Class, dt);
    require_quiet(classNum, errOut);
    CFIndex tmp;
    require_quiet(CFNumberGetValue(classNum, kCFNumberCFIndexType, &tmp), errOut);
    retval = (SOSPeerInfoDeviceClass) tmp;
errOut:
    return retval;
}
