/*
 * Copyright (c) 2016-2017,2024 Apple Inc. All Rights Reserved.
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
 *
 * SecCertificateSource.c - certificate sources for trust evaluation engine
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <AssertMacros.h>

#include <CommonCrypto/CommonDigest.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemInternal.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAppleAnchorPriv.h>

#include "trust/trustd/SecTrustServer.h"
#include "keychain/securityd/SecItemServer.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "trust/trustd/SecCAIssuerRequest.h"
#include "trust/trustd/SecAnchorCache.h"

#include "OTATrustUtilities.h"
#include "SecCertificateSource.h"

/********************************************************
 ***************** OTA Trust support ********************
 ********************************************************/

static CFArrayRef subject_to_anchors(CFDataRef nic)
{
    CFArrayRef result = NULL;

    if (NULL == nic)
    {
        return result;
    }

    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref)
    {
        return result;
    }

    CFDictionaryRef lookupTable = SecOTAPKICopyAnchorLookupTable(otapkiref);
    CFRelease(otapkiref);

    if (NULL == lookupTable)
    {
        return result;
    }

    unsigned char subject_digest[CC_SHA1_DIGEST_LENGTH];
    memset(subject_digest, 0, CC_SHA1_DIGEST_LENGTH);

    (void)CC_SHA1(CFDataGetBytePtr(nic), (CC_LONG)CFDataGetLength(nic), subject_digest);
    CFDataRef sha1Digest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, subject_digest, CC_SHA1_DIGEST_LENGTH, kCFAllocatorNull);


    result = (CFArrayRef)CFDictionaryGetValue(lookupTable, sha1Digest);
    CFReleaseSafe(lookupTable);
    CFReleaseSafe(sha1Digest);

    return result;
}

static CFArrayRef copy_anchor_offsets(void)
{
    CFMutableArrayRef result = NULL;
    CFDictionaryRef lookupTable = NULL;
    CFTypeRef *values = NULL;
    SecOTAPKIRef otapkiref = NULL;

    result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!result) { secerror("Unable to allocate anchor array"); }
    __Require_Quiet(result, errOut);

    otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (!otapkiref) { secerror("Unable to retrieve current OTAPKIRef"); }
    __Require_Quiet(otapkiref, errOut);

    lookupTable = SecOTAPKICopyAnchorLookupTable(otapkiref);
    if (!lookupTable) { secerror("Unable to retrieve anchor lookup table"); }
    __Require_Quiet(lookupTable, errOut);

    CFIndex ix, jx, count = CFDictionaryGetCount(lookupTable);
    bool countOk = (count >= 1 && count <= 8192);
    if (!countOk) { secerror("Unexpected system store count: %lld", (long long)count); }
    __Require_Quiet(countOk, errOut);

    values = (CFTypeRef*)calloc((size_t)count, sizeof(CFTypeRef*));
    if (!values) { secerror("Failed to allocate buffer for %lld values", (long long)count); }
    __Require_Quiet(values, errOut);

    CFDictionaryGetKeysAndValues(lookupTable, NULL, (const void **)values);
    for (ix = 0; ix < count; ix++) {
        CFArrayRef offsets = (CFArrayRef)values[ix];
        if (offsets) {
            if (CFGetTypeID(offsets) != CFArrayGetTypeID()) {
                secerror("Failed to get CFArray type in values, skipping item");
            } else {
                CFIndex offsetCount = CFArrayGetCount(offsets);
                for (jx = 0; jx < offsetCount; jx++) {
                    CFTypeRef value = CFArrayGetValueAtIndex(offsets, jx);
                    if (value) {
                        CFArrayAppendValue(result, value);
                    }
                }
            }
        }
    }

errOut:
    free(values);
    CFReleaseNull(lookupTable);
    CFReleaseNull(otapkiref);
    return result;
}

static CFArrayRef CopyCertDataFromIndices(CFArrayRef offsets)
{
    CFMutableArrayRef result = NULL;

    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref)
    {
        return result;
    }

    const char* anchorTable = SecOTAPKIGetAnchorTable(otapkiref);
    if (NULL == anchorTable)
    {
        CFReleaseSafe(otapkiref);
        return result;
    }

    CFIndex num_offsets = CFArrayGetCount(offsets);

    result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    for (CFIndex idx = 0; idx < num_offsets; idx++)
    {
        CFNumberRef offset = (CFNumberRef)CFArrayGetValueAtIndex(offsets, idx);
        uint32_t offset_value = 0;
        if (CFNumberGetValue(offset, kCFNumberSInt32Type, &offset_value))
        {
            char* pDataPtr = (char *)(anchorTable + offset_value);
            //int32_t record_length = *((int32_t * )pDataPtr);
            //record_length = record_length;
            pDataPtr += sizeof(uint32_t);

            int32_t cert_data_length = *((int32_t * )pDataPtr);
            pDataPtr += sizeof(uint32_t);

            CFDataRef cert_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)pDataPtr,
                                                              cert_data_length, kCFAllocatorNull);
            if (NULL != cert_data)
            {
                CFArrayAppendValue(result, cert_data);
                CFReleaseSafe(cert_data);
            }
        }
    }
    CFReleaseSafe(otapkiref);
    return result;
}

static CFArrayRef CopyCertsFromIndices(CFArrayRef offsets)
{
    CFMutableArrayRef result = NULL;

    CFArrayRef cert_data_array = CopyCertDataFromIndices(offsets);

    if (NULL != cert_data_array)
    {
        result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFIndex num_cert_datas = CFArrayGetCount(cert_data_array);
        for (CFIndex idx = 0; idx < num_cert_datas; idx++)
        {
            CFDataRef cert_data = (CFDataRef)CFArrayGetValueAtIndex(cert_data_array, idx);
            if (NULL != cert_data)
            {
                SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, cert_data);
                if (NULL != cert)
                {
                    CFArrayAppendValue(result, cert);
                    CFRelease(cert);
                }
            }
        }
        CFRelease(cert_data_array);
    }
    return result;

}

// CopyAnchorRecordsForKey returns an array containing one or more
// dictionary records for anchors matching the input lookup key,
// or NULL if no anchor records matched the key.
//
static _Nullable CFArrayRef
CopyAnchorRecordsForKey(CFStringRef anchorLookupKey) {
    CFDictionaryRef anchorLookupTable = NULL;
    CFArrayRef anchorRecords = NULL;

    anchorLookupTable = SecOTAPKICopyConstrainedAnchorLookupTable();
    __Require_Quiet(isDictionary(anchorLookupTable), errOut);
    __Require_Quiet(isString(anchorLookupKey), errOut);
    anchorRecords = (CFArrayRef)CFDictionaryGetValue(anchorLookupTable, anchorLookupKey);
    CFRetainSafe(anchorRecords);
    __Require_Quiet(isArray(anchorRecords), errOut);

errOut:
    CFReleaseSafe(anchorLookupTable);
    return anchorRecords;
}

static _Nullable CFArrayRef
CopyAnchorRecordsForKeyWithHash(CFStringRef anchorLookupKey, CFDataRef hash, CFStringRef key) {
    CFArrayRef anchorRecords = NULL;
    CFStringRef hashKey = NULL;
    CFMutableArrayRef matchingAnchorRecords = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    __Require_Quiet(isData(hash), errOut);
    hashKey = CFDataCopyHexString(hash);
    __Require_Quiet(isString(hashKey), errOut);

    __Require_Quiet(anchorLookupKey, errOut);
    anchorRecords = CopyAnchorRecordsForKey(anchorLookupKey);
    __Require_Quiet(isArray(anchorRecords), errOut);

    CFIndex idx, count = CFArrayGetCount(anchorRecords);
    for (idx = 0; idx < count; idx++) {
        CFDictionaryRef record = CFArrayGetValueAtIndex(anchorRecords, idx);
        if (isDictionary(record)) {
            CFStringRef recordHash = (CFStringRef)CFDictionaryGetValue(record, key);
            if (isString(recordHash) &&
                kCFCompareEqualTo == CFStringCompare(recordHash, hashKey, 0)) {
                CFArrayAppendValue(matchingAnchorRecords, record);
            }
        }
    }

errOut:
    CFReleaseSafe(anchorRecords);
    CFReleaseSafe(hashKey);
    if (CFArrayGetCount(matchingAnchorRecords) > 0) {
        return matchingAnchorRecords;
    } else {
        CFReleaseNull(matchingAnchorRecords);
        return NULL;
    }
}

// CopyAnchorRecordForCertificate returns the dictionary record
// whose sha2 value matches the digest of the input certificate,
// or NULL if there is no match.
//
_Nullable CFArrayRef
CopyAnchorRecordsForCertificate(SecCertificateRef certificate) {
    CFArrayRef anchorRecords = NULL;
    CFStringRef anchorLookupKey = NULL;
    CFDataRef certificateHash = NULL;

    __Require_Quiet(certificate, errOut);
    certificateHash = SecCertificateCopySHA256Digest(certificate);
    __Require_Quiet(isData(certificateHash), errOut);
    anchorLookupKey = SecCertificateCopyAnchorLookupKey(certificate);
    __Require_Quiet(anchorLookupKey, errOut);

    anchorRecords = CopyAnchorRecordsForKeyWithHash(anchorLookupKey, certificateHash, CFSTR("sha2"));

errOut:
    CFReleaseSafe(anchorLookupKey);
    CFReleaseSafe(certificateHash);
    return anchorRecords;
}

// CopyAnchorRecordForCertificate returns the dictionary record
// whose spki hahsh value matches the digest of the input certificate,
// or NULL if there is no match.
_Nullable CFArrayRef
CopyAnchorRecordsForSPKI(SecCertificateRef certificate) {
    CFArrayRef anchorRecords = NULL;
    CFStringRef anchorLookupKey = NULL;
    CFDataRef spkiHash = NULL;

    __Require_Quiet(certificate, errOut);
    spkiHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(certificate);
    __Require_Quiet(isData(spkiHash), errOut);
    anchorLookupKey = SecCertificateCopyAnchorLookupKey(certificate);
    __Require_Quiet(anchorLookupKey, errOut);

    anchorRecords = CopyAnchorRecordsForKeyWithHash(anchorLookupKey, spkiHash, CFSTR("spki-sha2"));

errOut:
    CFReleaseSafe(anchorLookupKey);
    CFReleaseSafe(spkiHash);
    return anchorRecords;
}

static CFMutableArrayRef CopyUsageConstraintsForSystemAnchor(void) {
    CFMutableArrayRef result = NULL;
    CFMutableDictionaryRef options = NULL, strengthConstraints = NULL, trustRoot = NULL;
    CFNumberRef trustResult = NULL;

    __Require_Quiet(options = CFDictionaryCreateMutable(NULL, 1,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks),
                  out);
    __Require_Quiet(strengthConstraints = CFDictionaryCreateMutable(NULL, 1,
                                                             &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks),
                  out);

    CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedWeakHash, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedWeakKey, kCFBooleanTrue);
    CFDictionaryAddValue(strengthConstraints, kSecTrustSettingsPolicyOptions, options);

    __Require_Quiet(result = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks), out);
    CFArrayAppendValue(result, strengthConstraints);

out:
    CFReleaseNull(options);
    CFReleaseNull(trustResult);
    CFReleaseNull(trustRoot);
    CFReleaseNull(strengthConstraints);
    return result;
}

static CFArrayRef CopyUsageConstraintsForUnconstrainedSystemAnchor(void) {
    CFMutableArrayRef result = NULL;
    CFMutableDictionaryRef trustRoot = NULL;
    CFNumberRef trustResult = NULL;

    __Require_Quiet(trustRoot = CFDictionaryCreateMutable(NULL, 1,
                                                        &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks),
                  out);

    uint32_t temp = kSecTrustSettingsResultTrustRoot;
    __Require_Quiet(trustResult = CFNumberCreate(NULL, kCFNumberSInt32Type, &temp), out);
    CFDictionaryAddValue(trustRoot, kSecTrustSettingsResult, trustResult);

    __Require_Quiet(result = CopyUsageConstraintsForSystemAnchor(), out);
    CFArrayAppendValue(result, trustRoot);

out:
    CFReleaseNull(trustResult);
    CFReleaseNull(trustRoot);
    return result;
}

static CFStringRef ConvertPolicyOidToCompatibilityOid(CFStringRef policyOid) {
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleSSLServer, 0) ||
        kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleSSLClient, 0)) {
        return kSecPolicyAppleSSL;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleEAPServer, 0) ||
        kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleEAPClient, 0)) {
        return kSecPolicyAppleEAP;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleIPSecServer, 0) ||
        kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleIPSecClient, 0)) {
        return kSecPolicyAppleIPsec;
    }
    return policyOid;
}

static _Nullable CFStringRef ConvertPolicyOidToPolicyName(CFStringRef policyOid) {
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleSSLServer, 0)) {
        return kSecPolicyNameSSLServer;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleSSLClient, 0)) {
        return kSecPolicyNameSSLClient;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleEAPServer, 0)) {
        return kSecPolicyNameEAPServer;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleEAPClient, 0)) {
        return kSecPolicyNameEAPClient;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleIPSecServer, 0)) {
        return kSecPolicyNameIPSecServer;
    }
    if (kCFCompareEqualTo == CFStringCompare(policyOid, kSecPolicyAppleIPSecClient, 0)) {
        return kSecPolicyNameIPSecClient;
    }
    return NULL;
}

static _Nullable CFArrayRef
CreatePolicyTrustSettingsForAnchorRecord(CFDictionaryRef anchorRecord, CFNumberRef trustResult) {
    CFMutableArrayRef result = NULL;

    __Require_Quiet(result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);
    /* Add policy constraints if specified (regardless of anchor type) */
    CFArrayRef policyOids = (CFArrayRef)CFDictionaryGetValue(anchorRecord, CFSTR("oids"));
    CFIndex numPolicyOids = (isArray(policyOids)) ? CFArrayGetCount(policyOids) : 0;
    if (numPolicyOids > 0) {
        for (CFIndex idx = 0; idx < numPolicyOids; idx++) {
            CFStringRef policyOidStr = (CFStringRef)CFArrayGetValueAtIndex(policyOids, idx);
            if (!isString(policyOidStr)) { continue; }
            CFMutableDictionaryRef trustSetting = NULL;
            __Require_Quiet(trustSetting = CFDictionaryCreateMutable(NULL, 0,
                                                                   &kCFTypeDictionaryKeyCallBacks,
                                                                   &kCFTypeDictionaryValueCallBacks),
                          errOut);

            /* get the compatibility policy oid/names to match the user/admin trust store formulation of constraints */
            CFStringRef compatibilityPolicyOidStr =  ConvertPolicyOidToCompatibilityOid(policyOidStr);
            CFStringRef compatibilityPolicyName = ConvertPolicyOidToPolicyName(policyOidStr);
            CFDictionaryAddValue(trustSetting, kSecTrustSettingsPolicy, compatibilityPolicyOidStr);
            if (compatibilityPolicyName) {
                CFDictionaryAddValue(trustSetting, kSecTrustSettingsPolicyName, compatibilityPolicyName);
            }
            CFDictionaryAddValue(trustSetting, kSecTrustSettingsResult, trustResult);
            CFArrayAppendValue(result, trustSetting);
            CFReleaseNull(trustSetting);
        }
    }

    /* For system anchors add system constraints */
    CFStringRef anchorType = (CFStringRef)CFDictionaryGetValue(anchorRecord, CFSTR("type"));
    if (isString(anchorType) &&
        (kCFCompareEqualTo == CFStringCompare(anchorType, kSecAnchorTypeSystem, 0) ||
         kCFCompareEqualTo == CFStringCompare(anchorType, kSecAnchorTypeSystemTEST, 0))) {
        CFArrayRef systemConstraints = NULL;
        if (numPolicyOids > 0) {
            /* constrained system roots only need the policy options */
            systemConstraints = CopyUsageConstraintsForSystemAnchor();
        } else {
            /* unconstrained system roots need both the policy options and the unconstrained trusted root */
            systemConstraints = CopyUsageConstraintsForUnconstrainedSystemAnchor();
        }

        if (systemConstraints) {
            CFArrayAppendArray(result, systemConstraints, CFRangeMake(0, CFArrayGetCount(systemConstraints)));
            CFReleaseNull(systemConstraints);
        }
    }

    // Intentionally returns an empty array for anchor records with no specified OIDs (as both mean un-constrained)

errOut:
    return result;
}

// CopyUsageConstraintsForCertificate creates an array of per-policy
// dictionary constraints if an anchor record is found for the given
// certificate and it contains them. If the anchor record is found but
// has no policy constraints, then create an empty array if the type
// is a system anchor. Otherwise, if no constraints are present and the
// anchor is not a system anchor, return NULL.
//
_Nullable CFArrayRef
CopyUsageConstraintsForCertificate(SecCertificateRef certificate) {
    CFMutableArrayRef result = NULL;
    CFNumberRef trustResult = NULL;
    CFArrayRef anchorRecords = NULL;

    /* Hardcoded Apple Anchors have no usage constraints */
    if (SecIsAppleTrustAnchor(certificate, kSecAppleTrustAnchorFlagsIncludeTestAnchors)) {
        result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        return result;
    }

    // trust result value depends on whether the CA is self-signed
    uint32_t trustResultValue = kSecTrustSettingsResultTrustRoot;
    if (!SecCertificateIsSelfSignedCA(certificate)) {
        trustResultValue = kSecTrustSettingsResultTrustAsRoot;
    }
    __Require_Quiet(trustResult = CFNumberCreate(NULL, kCFNumberSInt32Type, &trustResultValue), errOut);

    __Require_Quiet(anchorRecords = CopyAnchorRecordsForSPKI(certificate), errOut);
    CFIndex numRecords = CFArrayGetCount(anchorRecords);
    __Require_Quiet(numRecords > 0, errOut);

    __Require_Quiet(result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);
    for (CFIndex recordIX = 0; recordIX < numRecords; recordIX++) {
        CFDictionaryRef anchorRecord = CFArrayGetValueAtIndex(anchorRecords, recordIX);
        if (!isDictionary(anchorRecord)) {
            continue;
        }

        CFArrayRef trustSettings = CreatePolicyTrustSettingsForAnchorRecord(anchorRecord, trustResult);
        if (trustSettings) {
            CFArrayAppendAll(result, trustSettings);
            CFReleaseNull(trustSettings);
        }
    }

errOut:
    CFReleaseNull(trustResult);
    CFReleaseNull(anchorRecords);
    return result;
}

/********************************************************
 *************** END OTA Trust support ******************
 ********************************************************/

/********************************************************
 ************ SecCertificateSource object ***************
 ********************************************************/

bool SecCertificateSourceCopyParents(SecCertificateSourceRef source,
                                     SecCertificateRef certificate,
                                     void *context, SecCertificateSourceParents callback) {
    return source->copyParents(source, certificate, context, callback);
}

CFArrayRef SecCertificateSourceCopyUsageConstraints(SecCertificateSourceRef source,
                                                    SecCertificateRef certificate) {
    if (source->copyUsageConstraints) {
        return source->copyUsageConstraints(source, certificate);
    } else {
        return NULL;
    }
}

bool SecCertificateSourceContains(SecCertificateSourceRef source,
                                  SecCertificateRef certificate,
                                  SecPVCRef pvc) {
    return source->contains(source, certificate, pvc);
}

// MARK: -
// MARK: SecItemCertificateSource
/********************************************************
 *********** SecItemCertificateSource object ************
 ********************************************************/
struct SecItemCertificateSource {
    struct SecCertificateSource base;
    CFArrayRef accessGroups;
};
typedef struct SecItemCertificateSource *SecItemCertificateSourceRef;

static CF_RETURNS_RETAINED CFArrayRef _Nullable SecItemCertificateSourceResultsPost(CFTypeRef raw_results) {
    CFMutableArrayRef result = NULL;
    if (isArray(raw_results)) {
        result = CFArrayCreateMutable(kCFAllocatorDefault, CFArrayGetCount(raw_results), &kCFTypeArrayCallBacks);
        CFArrayForEach(raw_results, ^(const void *value) {
            SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, value);
            if (cert) {
                CFArrayAppendValue(result, cert);
                CFRelease(cert);
            }
        });
    } else if (isData(raw_results)) {
        result = CFArrayCreateMutable(kCFAllocatorDefault, CFArrayGetCount(raw_results), &kCFTypeArrayCallBacks);
        SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)raw_results);
        if (cert) {
            CFArrayAppendValue(result, cert);
            CFRelease(cert);
        }
    }
    return result;
}

static bool SecItemCertificateSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                                void *context, SecCertificateSourceParents callback) {
    SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    CFDataRef normalizedIssuer = SecCertificateGetNormalizedIssuerContent(certificate);

    CFErrorRef localError = NULL;
    CFArrayRef results = SecItemCopyParentCertificates_ios(normalizedIssuer, msource->accessGroups, &localError);
    if (!results) {
        if (localError && (CFErrorGetCode(localError) != errSecItemNotFound)) {
            secdebug("trust", "SecItemCopyParentCertificates_ios: %@", localError);
        }
        CFReleaseSafe(localError);
    }
    CFArrayRef certs = SecItemCertificateSourceResultsPost(results);
    CFReleaseSafe(results);
    callback(context, certs);
    CFReleaseSafe(certs);
    return true;
}

static bool SecItemCertificateSourceContains(SecCertificateSourceRef source,
                                             SecCertificateRef certificate,
                                             SecPVCRef pvc) {
    SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    /* Look up a certificate by issuer and serial number. */
    CFDataRef normalizedIssuer = SecCertificateGetNormalizedIssuerContent(certificate);
    CFRetainSafe(normalizedIssuer);
    CFErrorRef localError = NULL;
    CFDataRef serialNumber = SecCertificateCopySerialNumberData(certificate, &localError);
    bool result = SecItemCertificateExists(normalizedIssuer, serialNumber, msource->accessGroups, &localError);
    if (localError) {
        if (CFErrorGetCode(localError) != errSecItemNotFound) {
            secdebug("trust", "SecItemCertificateExists_ios: %@", localError);
        }
        CFReleaseSafe(localError);
    }
    CFReleaseSafe(serialNumber);
    CFReleaseSafe(normalizedIssuer);
    return result;
}

SecCertificateSourceRef SecItemCertificateSourceCreate(CFArrayRef accessGroups) {
    SecItemCertificateSourceRef result = (SecItemCertificateSourceRef)malloc(sizeof(*result));
    result->base.copyParents = SecItemCertificateSourceCopyParents;
    result->base.copyUsageConstraints = NULL;
    result->base.contains = SecItemCertificateSourceContains;
    result->accessGroups = accessGroups;
    CFRetainSafe(accessGroups);
    return (SecCertificateSourceRef)result;
}

void SecItemCertificateSourceDestroy(SecCertificateSourceRef source) {
    SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    CFReleaseSafe(msource->accessGroups);
    free(msource);
}

// MARK: -
// MARK: SecSystemConstrainedAnchorSource
/****************************************************************
 *********** SecSystemConstrainedAnchorSource object ************
 ****************************************************************/

static bool SecSystemConstrainedAnchorSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate, void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = NULL;
    CFDataRef normalizedIssuerHash = NULL;
    CFStringRef anchorLookupKey = NULL;

    CFDataRef normalizedIssuerContent = SecCertificateGetNormalizedIssuerContent(certificate);
    __Require_Quiet(normalizedIssuerContent, errOut);
    normalizedIssuerHash = CFDataCreateWithHash(kCFAllocatorDefault, ccsha1_di(), CFDataGetBytePtr(normalizedIssuerContent), CFDataGetLength(normalizedIssuerContent));
    __Require_Quiet(normalizedIssuerHash, errOut);
    anchorLookupKey = CFDataCopyHexString(normalizedIssuerHash);
    __Require_Quiet(anchorLookupKey, errOut);

    parents = SecAnchorCacheCopyParentCertificates(anchorLookupKey);

errOut:
    callback(context, parents);
    CFReleaseSafe(parents);
    CFReleaseSafe(normalizedIssuerHash);
    CFReleaseSafe(anchorLookupKey);
    return true;
}

CFArrayRef SecSystemConstrainedAnchorSourceCopyUsageConstraints(SecCertificateSourceRef __unused source, SecCertificateRef certificate)
{
    /* CopyUsageConstraints uses anchor records by _key_ instead of by certificate so that
     * SecPolicyCheckQWAC will apply "QWAC-ness" to cross-signed anchors. SecPathBuilderIsAnchor
     * does not rely on CopyUsageConstraints only sending the constraints only for the _certificate_
     * because it uses Contains first, which filters AnchorRecords for the certificate by policy. */
    return CopyUsageConstraintsForCertificate(certificate);
}

static bool SecSystemConstrainedAnchorSourceContains(SecCertificateSourceRef source,
                                                     SecCertificateRef certificate,
                                                     SecPVCRef pvc) {
    /* We use the certificate here so we know whether the _certificate_ is in the source
     * and permitted by the policy. SecPathBuilderIsAnchor uses this _before_ applying
     * the usage constraints by _key_, so we filter the certificate anchor records
     * by policy to determine "Anchor-ness". */
    bool result = false;
    CFArrayRef anchorRecords = CopyAnchorRecordsForCertificate(certificate);
    __Require_Quiet(isArray(anchorRecords), errOut);

    /* Determine whether policy allows this anchor record. If no policy specified use the basic policy. */
    SecPolicyRef policy = pvc ?  (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, 0) : NULL;
    CFStringRef policyId = policy ? SecPolicyGetOidString(policy) : kSecPolicyAppleX509Basic;
    CFArrayRef permittedAnchorRecords = SecAnchorPolicyPermittedAnchorRecords(anchorRecords, policyId);
    if (permittedAnchorRecords) {
        CFReleaseNull(permittedAnchorRecords);
        result = true;
    }

errOut:
    CFReleaseSafe(anchorRecords);
    return result;
}

bool SecSystemConstrainedAnchorSourceContainsAnchorByKey(SecCertificateRef certificate) {
    bool result = false;
    CFArrayRef anchorRecord = CopyAnchorRecordsForSPKI(certificate);
    __Require_Quiet(isArray(anchorRecord), errOut);
    result = true;

errOut:
    CFReleaseSafe(anchorRecord);
    return result;
}

struct SecCertificateSource _kSecSystemConstrainedAnchorSource = {
    SecSystemConstrainedAnchorSourceCopyParents,
    SecSystemConstrainedAnchorSourceCopyUsageConstraints,
    SecSystemConstrainedAnchorSourceContains
};

const SecCertificateSourceRef kSecSystemConstrainedAnchorSource = &_kSecSystemConstrainedAnchorSource;

// MARK: -
// MARK: SecAppleAnchorSource
/****************************************************************
 *********** SecAppleAnchorSource object ************
 ****************************************************************/
static CFArrayRef SecAppleAnchorSourceCopyUsageConstraints(SecCertificateSourceRef __unused source, SecCertificateRef certificate)
{
    CFMutableArrayRef result = NULL;
    CFNumberRef trustResult = NULL;
    CFArrayRef anchorRecords = NULL;

    /* Hardcoded Apple Anchors have no usage constraints */
    if (SecIsAppleTrustAnchor(certificate, kSecAppleTrustAnchorFlagsIncludeTestAnchors)) {
        result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        return result;
    }

    if (!SecAnchorCacheIsAppleAnchor(certificate)) {
        return NULL;
    }

    uint32_t trustResultValue = kSecTrustSettingsResultTrustRoot;
    __Require_Quiet(trustResult = CFNumberCreate(NULL, kCFNumberSInt32Type, &trustResultValue), errOut);

    __Require_Quiet(anchorRecords = CopyAnchorRecordsForSPKI(certificate), errOut);
    CFIndex numRecords = CFArrayGetCount(anchorRecords);
    __Require_Quiet(numRecords > 0, errOut);

    __Require_Quiet(result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);
    for (CFIndex recordIX = 0; recordIX < numRecords; recordIX++) {
        CFDictionaryRef anchorRecord = CFArrayGetValueAtIndex(anchorRecords, recordIX);
        if (!isDictionary(anchorRecord)) {
            continue;
        }

        // Add any policy constraints specified
        CFArrayRef trustSettings = CreatePolicyTrustSettingsForAnchorRecord(anchorRecord, trustResult);
        if (trustSettings) {
            CFArrayAppendAll(result, trustSettings);
            CFReleaseNull(trustSettings);
        }
    }

errOut:
    CFReleaseNull(trustResult);
    CFReleaseNull(anchorRecords);
    return result;
}

static bool SecAppleAnchorSourceContains(SecCertificateSourceRef source,
                                                     SecCertificateRef certificate,
                                                     SecPVCRef pvc) {
    /* If this is one of the hard-coded anchors it is trusted for everything */
    if (SecIsAppleTrustAnchor(certificate, kSecAppleTrustAnchorFlagsIncludeTestAnchors)) {
        return true;
    }

    if (!SecAnchorCacheIsAppleAnchor(certificate)) {
        return false;
    }

    bool result = false;
    CFArrayRef anchorRecords = CopyAnchorRecordsForCertificate(certificate);
    __Require_Quiet(isArray(anchorRecords), errOut);

    /* Determine whether policy allows this anchor record. If no policy specified use the basic policy. */
    SecPolicyRef policy = pvc ?  (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, 0) : NULL;
    CFStringRef policyId = policy ? SecPolicyGetOidString(policy) : kSecPolicyAppleX509Basic;
    CFArrayRef permittedAnchorRecords = SecAnchorPolicyPermittedAnchorRecords(anchorRecords, policyId);
    if (permittedAnchorRecords) {
        CFReleaseNull(permittedAnchorRecords);
        result = true;
    }

errOut:
    CFReleaseSafe(anchorRecords);
    return result;
}

struct SecCertificateSource _kSecAppleAnchorSource = {
    /* We can re-use the Anchor Cache to fetch parents because it contains both Apple anchors and system anchors */
    SecSystemConstrainedAnchorSourceCopyParents,
    SecAppleAnchorSourceCopyUsageConstraints,
    SecAppleAnchorSourceContains
};

const SecCertificateSourceRef kSecAppleAnchorSource = &_kSecAppleAnchorSource;

// MARK: -
// MARK: SecSystemAnchorSource
/********************************************************
 *********** SecSystemAnchorSource object ************
 ********************************************************/

static bool SecSystemAnchorSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                             void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = NULL;
    CFArrayRef anchors = NULL;

    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
     It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */

    anchors = subject_to_anchors(nic);
    __Require_Quiet(anchors, errOut);
    parents = CopyCertsFromIndices(anchors);

errOut:
    callback(context, parents);
    CFReleaseSafe(parents);
    return true;
}

static CFArrayRef SecSystemAnchorSourceCopyUsageConstraints(SecCertificateSourceRef __unused source,
                                                            SecCertificateRef __unused certificate)
{
    return CopyUsageConstraintsForUnconstrainedSystemAnchor();
}

static bool SecSystemAnchorSourceContains(SecCertificateSourceRef source,
                                          SecCertificateRef certificate,
                                          SecPVCRef pvc) {
    bool result = false;
    CFArrayRef anchors = NULL;
    SecOTAPKIRef otapkiref = NULL;
    CFArrayRef cert_datas = NULL;

    CFDataRef nic = SecCertificateGetNormalizedSubjectContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
     It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */

    otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    __Require_Quiet(otapkiref, errOut);
    anchors = subject_to_anchors(nic);
    __Require_Quiet(anchors, errOut);
    cert_datas = CopyCertDataFromIndices(anchors);
    __Require_Quiet(cert_datas, errOut);

    CFIndex cert_length = SecCertificateGetLength(certificate);
    const UInt8 *cert_data_ptr = SecCertificateGetBytePtr(certificate);

    CFIndex num_cert_datas = CFArrayGetCount(cert_datas);
    for (CFIndex idx = 0; idx < num_cert_datas; idx++)
    {
        CFDataRef cert_data = (CFDataRef)CFArrayGetValueAtIndex(cert_datas, idx);

        if (NULL != cert_data)
        {
            if (CFGetTypeID(cert_data) == CFDataGetTypeID())
            {
                CFIndex  aCert_Length = CFDataGetLength(cert_data);
                const UInt8*  aCert_Data_Ptr = CFDataGetBytePtr(cert_data);

                if (aCert_Length == cert_length && cert_length > 0)
                {
                    if (!memcmp(cert_data_ptr, aCert_Data_Ptr, (size_t)cert_length))
                    {
                        result = true;
                        break;
                    }
                }
            }
        }
    }

errOut:
    CFReleaseSafe(cert_datas);
    CFReleaseSafe(otapkiref);
    return result;
}

CFArrayRef SecSystemAnchorSourceCopyCertificates(void) {
    CFArrayRef anchors = NULL;
    SecOTAPKIRef otapkiref = NULL;
    CFArrayRef certDatas = NULL;

    otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    __Require_Quiet(otapkiref, errOut);
    anchors = copy_anchor_offsets();
    __Require_Quiet(anchors, errOut);
    certDatas = CopyCertDataFromIndices(anchors);
    __Require_Quiet(certDatas, errOut);

errOut:
    CFReleaseNull(anchors);
    CFReleaseNull(otapkiref);
    return certDatas;
}

struct SecCertificateSource _kSecSystemAnchorSource = {
    SecSystemAnchorSourceCopyParents,
    SecSystemAnchorSourceCopyUsageConstraints,
    SecSystemAnchorSourceContains
};

const SecCertificateSourceRef kSecSystemAnchorSource = &_kSecSystemAnchorSource;

// MARK: -
// MARK: SecUserAnchorSource
/********************************************************
 ************* SecUserAnchorSource object ***************
 ********************************************************/
static bool SecUserAnchorSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                           void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = SecTrustStoreCopyParents(SecTrustStoreForDomain(kSecTrustStoreDomainUser),
                                                  certificate, NULL);
    callback(context, parents);
    CFReleaseSafe(parents);
    return true;
}

static CFArrayRef SecUserAnchorSourceCopyUsageConstraints(SecCertificateSourceRef source,
                                                          SecCertificateRef certificate) {
    CFArrayRef usageConstraints = NULL;
    bool ok = _SecTrustStoreCopyUsageConstraints(SecTrustStoreForDomain(kSecTrustStoreDomainUser),
                                                 certificate, &usageConstraints, NULL);
    if (ok) {
        return usageConstraints;
    } else {
        CFReleaseNull(usageConstraints);
        return NULL;
    }
}

static bool SecUserAnchorSourceContains(SecCertificateSourceRef source,
                                        SecCertificateRef certificate,
                                        SecPVCRef pvc) {
    return SecTrustStoreContains(SecTrustStoreForDomain(kSecTrustStoreDomainUser),
                                 certificate);
}

struct SecCertificateSource _kSecUserAnchorSource = {
    SecUserAnchorSourceCopyParents,
    SecUserAnchorSourceCopyUsageConstraints,
    SecUserAnchorSourceContains
};

const SecCertificateSourceRef kSecUserAnchorSource = &_kSecUserAnchorSource;

// MARK: -
// MARK: SecMemoryCertificateSource
/********************************************************
 ********** SecMemoryCertificateSource object ***********
 ********************************************************/
struct SecMemoryCertificateSource {
    struct SecCertificateSource base;
    CFMutableSetRef certificates;
    CFMutableDictionaryRef subjects;
};
typedef struct SecMemoryCertificateSource *SecMemoryCertificateSourceRef;

static bool SecMemoryCertificateSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                                  void *context, SecCertificateSourceParents callback) {
    SecMemoryCertificateSourceRef msource =
    (SecMemoryCertificateSourceRef)source;
    CFDataRef normalizedIssuer =
    SecCertificateGetNormalizedIssuerContent(certificate);
    CFArrayRef parents = (normalizedIssuer) ? CFDictionaryGetValue(msource->subjects,
                                                                   normalizedIssuer) : NULL;
    /* FIXME filter parents by subjectID if certificate has an
     authorityKeyIdentifier. */
    secdebug("trust", "%{private}@ parents -> %{private}@", certificate, parents);
    callback(context, parents);
    return true;
}

static bool SecMemoryCertificateSourceContains(SecCertificateSourceRef source,
                                               SecCertificateRef certificate,
                                               SecPVCRef pvc) {
    SecMemoryCertificateSourceRef msource =
    (SecMemoryCertificateSourceRef)source;
    return CFSetContainsValue(msource->certificates, certificate);
}

static void dictAddValueToArrayForKey(CFMutableDictionaryRef dict,
                                      const void *key, const void *value) {
    if (!key)
        return;

    CFMutableArrayRef values =
    (CFMutableArrayRef)CFDictionaryGetValue(dict, key);
    if (!values) {
        values = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                      &kCFTypeArrayCallBacks);
        CFDictionaryAddValue(dict, key, values);
        CFRelease(values);
    }

    if (values)
        CFArrayAppendValue(values, value);
}

static void SecMemoryCertificateSourceApplierFunction(const void *value, void *context) {
    SecMemoryCertificateSourceRef msource =
    (SecMemoryCertificateSourceRef)context;
    SecCertificateRef certificate = (SecCertificateRef)value;

    /* CFSet's API has no way to combine these 2 operations into 1 sadly. */
    if (CFSetContainsValue(msource->certificates, certificate))
        return;
    CFSetAddValue(msource->certificates, certificate);

    CFDataRef key = SecCertificateGetNormalizedSubjectContent(certificate);
    dictAddValueToArrayForKey(msource->subjects, key, value);
}

SecCertificateSourceRef SecMemoryCertificateSourceCreate(CFArrayRef certificates) {
    SecMemoryCertificateSourceRef result = (SecMemoryCertificateSourceRef)
    malloc(sizeof(*result));
    result->base.copyParents = SecMemoryCertificateSourceCopyParents;
    result->base.copyUsageConstraints = NULL;
    result->base.contains = SecMemoryCertificateSourceContains;
    CFIndex count = CFArrayGetCount(certificates);
    result->certificates = CFSetCreateMutable(kCFAllocatorDefault, count,
                                              &kCFTypeSetCallBacks);
    result->subjects = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                 count, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    CFRange range = { 0, count };
    CFArrayApplyFunction(certificates, range,
                         SecMemoryCertificateSourceApplierFunction, result);

    return (SecCertificateSourceRef)result;
}

void SecMemoryCertificateSourceDestroy(SecCertificateSourceRef source) {
    SecMemoryCertificateSourceRef msource =
    (SecMemoryCertificateSourceRef)source;
    CFRelease(msource->certificates);
    CFRelease(msource->subjects);
    free(msource);
}

// MARK: -
// MARK: SecCAIssuerCertificateSource
/********************************************************
 ********* SecCAIssuerCertificateSource object **********
 ********************************************************/
static bool SecCAIssuerCertificateSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                                    void *context, SecCertificateSourceParents callback) {
    /* Some expired certs have dead domains. Let's not check them. */
    SecPathBuilderRef builder = (SecPathBuilderRef)context;
    CFAbsoluteTime verifyDate = SecPathBuilderGetVerifyTime(builder);
    if (SecPathBuilderHasTemporalParentChecks(builder) && !SecCertificateIsValid(certificate, verifyDate)) {
        secinfo("async", "skipping CAIssuer fetch for expired %@", certificate);
        callback(context, NULL);
        return true;
    }
    return SecCAIssuerCopyParents(certificate, context, callback);
}

static bool SecCAIssuerCertificateSourceContains(SecCertificateSourceRef source,
                                                 SecCertificateRef certificate,
                                                 SecPVCRef pvc) {
    return false;
}

struct SecCertificateSource _kSecCAIssuerSource = {
    SecCAIssuerCertificateSourceCopyParents,
    NULL,
    SecCAIssuerCertificateSourceContains
};

const SecCertificateSourceRef kSecCAIssuerSource = &_kSecCAIssuerSource;

#if TARGET_OS_OSX
#include <Security/SecItemPriv.h>
// MARK: -
// MARK: SecLegacyCertificateSource
/********************************************************
 ********** SecLegacyCertificateSource object ***********
 ********************************************************/

static bool SecLegacyCertificateSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                                  void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = SecItemCopyParentCertificates_osx(certificate, NULL);
    callback(context, parents);
    CFReleaseSafe(parents);
    return true;
}

static bool SecLegacyCertificateSourceContains(SecCertificateSourceRef source,
                                               SecCertificateRef certificate,
                                               SecPVCRef pvc) {
    SecCertificateRef cert = SecItemCopyStoredCertificate(certificate, NULL);
    bool result = (cert) ? true : false;
    CFReleaseSafe(cert);
    return result;
}

struct SecCertificateSource _kSecLegacyCertificateSource = {
    SecLegacyCertificateSourceCopyParents,
    NULL,
    SecLegacyCertificateSourceContains
};

const SecCertificateSourceRef kSecLegacyCertificateSource = &_kSecLegacyCertificateSource;

#endif /* SecLegacyCertificateSource */

#if TARGET_OS_OSX
// MARK: -
// MARK: SecLegacyAnchorSource
/********************************************************
 ************ SecLegacyAnchorSource object **************
 ********************************************************/

static bool SecLegacyAnchorSourceCopyParents(SecCertificateSourceRef source, SecCertificateRef certificate,
                                             void *context, SecCertificateSourceParents callback) {
    CFMutableArrayRef anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFArrayRef trusted = NULL;
    CFDataRef normalizedIssuer = SecCertificateCopyNormalizedIssuerSequence(certificate);
    if (!normalizedIssuer) {
        goto finish;
    }

    /* Get the custom anchors which have been trusted in the user and admin domains.
     * We don't need system domain roots here, since SecSystemAnchorSource provides those.
     */
    OSStatus status = SecTrustSettingsCopyCertificatesForUserAdminDomains(&trusted);
    if (status == errSecSuccess && trusted) {
        CFIndex index, count = CFArrayGetCount(trusted);
        for (index = 0; index < count; index++) {
            SecCertificateRef potentialParent = (SecCertificateRef)CFArrayGetValueAtIndex(trusted, index);
            CFDataRef normalizedSubject = SecCertificateCopyNormalizedSubjectSequence(potentialParent);
            if (CFEqualSafe(normalizedIssuer, normalizedSubject)) {
                CFArrayAppendValue(anchors, potentialParent);
            }
            CFReleaseSafe(normalizedSubject);
        }
    }

finish:
    callback(context, anchors);
    CFReleaseSafe(anchors);
    CFReleaseSafe(trusted);
    CFReleaseSafe(normalizedIssuer);
    return true;
}

static CFArrayRef SecLegacyAnchorSourceCopyUsageConstraints(SecCertificateSourceRef source,
                                                            SecCertificateRef certificate) {
    CFArrayRef result = NULL;
    CFArrayRef userTrustSettings = NULL, adminTrustSettings = NULL;

    OSStatus status = SecTrustSettingsCopyTrustSettings_Cached(certificate,
                                                               kSecTrustSettingsDomainAdmin,
                                                               &adminTrustSettings);
    if ((status == errSecSuccess) && (adminTrustSettings != NULL)) {
        /* admin trust settings overrule user trust settings (rdar://37052515) */
        return adminTrustSettings;
    }

    status =  SecTrustSettingsCopyTrustSettings_Cached(certificate,
                                                       kSecTrustSettingsDomainUser,
                                                       &userTrustSettings);
    if (status == errSecSuccess) {
        result = CFRetainSafe(userTrustSettings);
    }

    CFReleaseNull(userTrustSettings);
    CFReleaseNull(adminTrustSettings);
    return result;
}

static bool SecLegacyAnchorSourceContains(SecCertificateSourceRef source,
                                          SecCertificateRef certificate,
                                          SecPVCRef pvc) {
    if (certificate == NULL) {
        return false;
    }

    if (SecTrustSettingsUserAdminDomainsContain(certificate)) {
        return true;
    } else {
        CFArrayRef trusted = NULL;
        bool result = false;
        OSStatus status = SecTrustSettingsCopyCertificatesForUserAdminDomains(&trusted);
        if ((status == errSecSuccess) && (trusted != NULL)) {
            if ((CFArrayGetCount(trusted) > 0) && CFGetTypeID(CFArrayGetValueAtIndex(trusted, 0)) != CFGetTypeID(certificate)) {
                /* This fallback should only happen if trustd and the Security framework are using different SecCertificate TypeIDs.
                 * This occurs in TrustTests where we rebuild SecCertificate.c for code coverage purposes, so we end up with
                 * two registered SecCertificate types. So we'll make a SecCertificate of our type. */
                CFIndex index, count = CFArrayGetCount(trusted);
                for (index = 0; index < count; index++) {
                    SecCertificateRef anchor = (SecCertificateRef)CFRetainSafe(CFArrayGetValueAtIndex(trusted, index));
                    if (anchor && (CFGetTypeID(anchor) != CFGetTypeID(certificate))) {
                        SecCertificateRef temp = SecCertificateCreateWithBytes(NULL, SecCertificateGetBytePtr(anchor), SecCertificateGetLength(anchor));
                        CFAssignRetained(anchor, temp);
                    }
                    if (anchor && CFEqual(anchor, certificate)) {
                        result = true;
                    }
                    CFReleaseNull(anchor);
                    if (result) {
                        break;
                    }
                }
            }
            CFReleaseSafe(trusted);
            return result;
        }
    }
    return false;
}

struct SecCertificateSource _kSecLegacyAnchorSource = {
    SecLegacyAnchorSourceCopyParents,
    SecLegacyAnchorSourceCopyUsageConstraints,
    SecLegacyAnchorSourceContains
};

const SecCertificateSourceRef kSecLegacyAnchorSource = &_kSecLegacyAnchorSource;

#endif /* SecLegacyAnchorSource */
