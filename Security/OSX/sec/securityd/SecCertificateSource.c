/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
#include <Security/SecItem.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecPolicyInternal.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include <securityd/SecTrustServer.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecTrustStoreServer.h>
#include <securityd/SecCAIssuerRequest.h>

#include "OTATrustUtilities.h"
#include "SecCertificateSource.h"

/********************************************************
 ***************** OTA Trust support ********************
 ********************************************************/


//#ifndef SECITEM_SHIM_OSX

static CFArrayRef subject_to_anchors(CFDataRef nic);
static CFArrayRef CopyCertsFromIndices(CFArrayRef offsets);

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
//#endif // SECITEM_SHIM_OSX

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
                                  SecCertificateRef certificate) {
    return source->contains(source, certificate);
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

static CF_RETURNS_RETAINED CFTypeRef SecItemCertificateSourceResultsPost(CFTypeRef raw_results) {
    if (isArray(raw_results)) {
        CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, CFArrayGetCount(raw_results), &kCFTypeArrayCallBacks);
        CFArrayForEach(raw_results, ^(const void *value) {
            SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, value);
            if (cert) {
                CFArrayAppendValue(result, cert);
                CFRelease(cert);
            }
        });
        return result;
    } else if (isData(raw_results)) {
        return SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)raw_results);
    }
    return NULL;
}

static bool SecItemCertificateSourceCopyParents(
                                                SecCertificateSourceRef source, SecCertificateRef certificate,
                                                void *context, SecCertificateSourceParents callback) {
    SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    /* FIXME: Search for things other than just subject of our issuer if we
     have a subjectID or authorityKeyIdentifier. */
    CFDataRef normalizedIssuer =
    SecCertificateGetNormalizedIssuerContent(certificate);
    const void *keys[] = {
        kSecClass,
        kSecReturnData,
        kSecMatchLimit,
        kSecAttrSubject
    },
    *values[] = {
        kSecClassCertificate,
        kCFBooleanTrue,
        kSecMatchLimitAll,
        normalizedIssuer
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4,
                                               NULL, NULL);
    CFTypeRef results = NULL;
    SecurityClient client = {
        .task = NULL,
        .accessGroups = msource->accessGroups,
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
    };

    /* We can make this async or run this on a queue now easily. */
    CFErrorRef localError = NULL;
    if (!_SecItemCopyMatching(query, &client, &results, &localError)) {
        if (localError && (CFErrorGetCode(localError) != errSecItemNotFound)) {
            secdebug("trust", "_SecItemCopyMatching: %@", localError);
        }
        CFReleaseSafe(localError);
    }
    CFRelease(query);
    CFTypeRef certs = SecItemCertificateSourceResultsPost(results);
    CFReleaseSafe(results);
    callback(context, certs);
    CFReleaseSafe(certs);
    return true;
}

static bool SecItemCertificateSourceContains(SecCertificateSourceRef source,
                                             SecCertificateRef certificate) {
    SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    /* Look up a certificate by issuer and serial number. */
    CFDataRef normalizedIssuer = SecCertificateGetNormalizedIssuerContent(certificate);
    CFRetainSafe(normalizedIssuer);
    CFDataRef serialNumber =
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
    SecCertificateCopySerialNumber(certificate, NULL);
#else
    SecCertificateCopySerialNumber(certificate);
#endif
    const void *keys[] = {
        kSecClass,
        kSecMatchLimit,
        kSecAttrIssuer,
        kSecAttrSerialNumber
    },
    *values[] = {
        kSecClassCertificate,
        kSecMatchLimitOne,
        normalizedIssuer,
        serialNumber
    };
    SecurityClient client = {
        .task = NULL,
        .accessGroups = msource->accessGroups,
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4, NULL, NULL);
    CFErrorRef localError = NULL;
    CFTypeRef results = NULL;
    bool ok = _SecItemCopyMatching(query, &client, &results, &localError);
    CFReleaseSafe(query);
    CFReleaseSafe(serialNumber);
    CFReleaseSafe(normalizedIssuer);
    CFReleaseSafe(results);
    if (!ok) {
        if (CFErrorGetCode(localError) != errSecItemNotFound) {
            secdebug("trust", "_SecItemCopyMatching: %@", localError);
        }
        CFReleaseSafe(localError);
        return false;
    }
    return true;
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
// MARK: SecSystemAnchorSource
/********************************************************
 *********** SecSystemAnchorSource object ************
 ********************************************************/

static bool SecSystemAnchorSourceCopyParents(
                                             SecCertificateSourceRef source, SecCertificateRef certificate,
                                             void *context, SecCertificateSourceParents callback) {
    //#ifndef SECITEM_SHIM_OSX
    CFArrayRef parents = NULL;
    CFArrayRef anchors = NULL;
    SecOTAPKIRef otapkiref = NULL;

    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
     It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */

    otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    require_quiet(otapkiref, errOut);
    anchors = subject_to_anchors(nic);
    require_quiet(anchors, errOut);
    parents = CopyCertsFromIndices(anchors);

errOut:
    callback(context, parents);
    CFReleaseSafe(parents);
    CFReleaseSafe(otapkiref);
    //#endif // SECITEM_SHIM_OSX
    return true;
}

static CFArrayRef SecSystemAnchorSourceCopyUsageConstraints(SecCertificateSourceRef __unused source,
                                                            SecCertificateRef __unused certificate)
{
    CFMutableArrayRef result = NULL;
    CFMutableDictionaryRef options = NULL, hashConstraint = NULL, trustRoot = NULL;
    CFNumberRef trustResult = NULL;

    require_quiet(options = CFDictionaryCreateMutable(NULL, 1,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks),
                  out);
    require_quiet(hashConstraint = CFDictionaryCreateMutable(NULL, 1,
                                                             &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks),
                  out);
    require_quiet(trustRoot = CFDictionaryCreateMutable(NULL, 1,
                                                        &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks),
                  out);

    uint32_t temp = kSecTrustSettingsResultTrustRoot;
    require_quiet(trustResult = CFNumberCreate(NULL, kCFNumberSInt32Type, &temp), out);
    CFDictionaryAddValue(trustRoot, kSecTrustSettingsResult, trustResult);

    CFDictionaryAddValue(options, kSecPolicyCheckSystemTrustedWeakHash, kCFBooleanTrue);
    CFDictionaryAddValue(hashConstraint, kSecTrustSettingsPolicyOptions, options);

    require_quiet(result = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks), out);
    CFArrayAppendValue(result, hashConstraint);
    CFArrayAppendValue(result, trustRoot);

out:
    CFReleaseNull(options);
    CFReleaseNull(trustResult);
    CFReleaseNull(trustRoot);
    CFReleaseNull(hashConstraint);
    return result;
}

static bool SecSystemAnchorSourceContains(SecCertificateSourceRef source,
                                          SecCertificateRef certificate) {
    bool result = false;
    CFArrayRef anchors = NULL;
    SecOTAPKIRef otapkiref = NULL;
    CFArrayRef cert_datas = NULL;

    CFDataRef nic = SecCertificateGetNormalizedSubjectContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
     It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */

    otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    require_quiet(otapkiref, errOut);
    anchors = subject_to_anchors(nic);
    require_quiet(anchors, errOut);
    cert_datas = CopyCertDataFromIndices(anchors);
    require_quiet(cert_datas, errOut);

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

                if (aCert_Length == cert_length)
                {
                    if (!memcmp(cert_data_ptr, aCert_Data_Ptr, cert_length))
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

struct SecCertificateSource _kSecSystemAnchorSource = {
    SecSystemAnchorSourceCopyParents,
    SecSystemAnchorSourceCopyUsageConstraints,
    SecSystemAnchorSourceContains
};

const SecCertificateSourceRef kSecSystemAnchorSource = &_kSecSystemAnchorSource;


#if TARGET_OS_IPHONE
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
    CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
    if (!digest)
        return NULL;
    CFArrayRef usageConstraints = NULL;
    bool ok = _SecTrustStoreCopyUsageConstraints(SecTrustStoreForDomain(kSecTrustStoreDomainUser),
                                                 digest, &usageConstraints, NULL);
    return (ok) ? usageConstraints : NULL;
}

static bool SecUserAnchorSourceContains(SecCertificateSourceRef source,
                                        SecCertificateRef certificate) {
    return SecTrustStoreContains(SecTrustStoreForDomain(kSecTrustStoreDomainUser),
                                 certificate);
}

struct SecCertificateSource _kSecUserAnchorSource = {
    SecUserAnchorSourceCopyParents,
    SecUserAnchorSourceCopyUsageConstraints,
    SecUserAnchorSourceContains
};

const SecCertificateSourceRef kSecUserAnchorSource = &_kSecUserAnchorSource;
#endif

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

static bool SecMemoryCertificateSourceCopyParents(
                                                  SecCertificateSourceRef source, SecCertificateRef certificate,
                                                  void *context, SecCertificateSourceParents callback) {
    SecMemoryCertificateSourceRef msource =
    (SecMemoryCertificateSourceRef)source;
    CFDataRef normalizedIssuer =
    SecCertificateGetNormalizedIssuerContent(certificate);
    CFArrayRef parents = (normalizedIssuer) ? CFDictionaryGetValue(msource->subjects,
                                                                   normalizedIssuer) : NULL;
    /* FIXME filter parents by subjectID if certificate has an
     authorityKeyIdentifier. */
    secdebug("trust", "%@ parents -> %@", certificate, parents);
    callback(context, parents);
    return true;
}

static bool SecMemoryCertificateSourceContains(SecCertificateSourceRef source,
                                               SecCertificateRef certificate) {
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

static void SecMemoryCertificateSourceApplierFunction(const void *value,
                                                      void *context) {
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
static bool SecCAIssuerCertificateSourceCopyParents(
                                                    SecCertificateSourceRef source, SecCertificateRef certificate,
                                                    void *context, SecCertificateSourceParents callback) {
    return SecCAIssuerCopyParents(certificate, SecPathBuilderGetQueue((SecPathBuilderRef)context), context, callback);
}

static bool SecCAIssuerCertificateSourceContains(
                                                 SecCertificateSourceRef source, SecCertificateRef certificate) {
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

static bool SecLegacyCertificateSourceCopyParents(
                                                  SecCertificateSourceRef source, SecCertificateRef certificate,
                                                  void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = SecItemCopyParentCertificates(certificate, NULL);
    callback(context, parents);
    CFReleaseSafe(parents);
    return true;
}

static bool SecLegacyCertificateSourceContains(
                                               SecCertificateSourceRef source, SecCertificateRef certificate) {
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
    CFArrayRef parents = SecItemCopyParentCertificates(certificate, NULL);
    CFArrayRef trusted = NULL;
    if (parents == NULL) {
        goto finish;
    }
    /* Get the custom anchors which have been trusted in the user and admin domains.
     * We don't need system domain roots here, since SecSystemAnchorSource provides those.
     */
    OSStatus status = SecTrustSettingsCopyCertificatesForUserAdminDomains(&trusted);
    if (status == errSecSuccess && trusted) {
        CFIndex index, count = CFArrayGetCount(parents);
        for (index = 0; index < count; index++) {
            SecCertificateRef parent = (SecCertificateRef)CFArrayGetValueAtIndex(parents, index);
            if (parent && CFArrayContainsValue(trusted, CFRangeMake(0, CFArrayGetCount(trusted)), parent)) {
                CFArrayAppendValue(anchors, parent);
            }
        }
    }

finish:
    callback(context, anchors);
    CFReleaseSafe(anchors);
    CFReleaseSafe(parents);
    CFReleaseSafe(trusted);
    return true;
}

static CFArrayRef SecLegacyAnchorSourceCopyUsageConstraints(SecCertificateSourceRef source,
                                                            SecCertificateRef certificate) {
    CFArrayRef result = NULL;
    CFArrayRef userTrustSettings = NULL, adminTrustSettings = NULL;

    OSStatus status = SecTrustSettingsCopyTrustSettings(certificate,
                                                        kSecTrustSettingsDomainUser,
                                                        &userTrustSettings);
    if ((status == errSecSuccess) && (userTrustSettings != NULL)) {
        result = CFRetain(userTrustSettings);
    }

    status = SecTrustSettingsCopyTrustSettings(certificate,
                                               kSecTrustSettingsDomainAdmin,
                                               &adminTrustSettings);
    /* user trust settings overrule admin trust settings */
    if ((status == errSecSuccess) && (adminTrustSettings != NULL) && (result == NULL)) {
        result = CFRetain(adminTrustSettings);
    }

    CFReleaseNull(userTrustSettings);
    CFReleaseNull(adminTrustSettings);
    return result;
}

static bool SecLegacyAnchorSourceContains(SecCertificateSourceRef source,
                                          SecCertificateRef certificate) {
    if (certificate == NULL) {
        return false;
    }
    CFArrayRef trusted = NULL;
    bool result = false;
    OSStatus status = SecTrustSettingsCopyCertificatesForUserAdminDomains(&trusted);
    if ((status == errSecSuccess) && (trusted != NULL)) {
        CFIndex index, count = CFArrayGetCount(trusted);
        for (index = 0; index < count; index++) {
            SecCertificateRef anchor = (SecCertificateRef)CFArrayGetValueAtIndex(trusted, index);
            if (anchor && CFEqual(anchor, certificate)) {
                result = true;
                break;
            }
        }
    }
    CFReleaseSafe(trusted);
    return result;
}

struct SecCertificateSource _kSecLegacyAnchorSource = {
    SecLegacyAnchorSourceCopyParents,
    SecLegacyAnchorSourceCopyUsageConstraints,
    SecLegacyAnchorSourceContains
};

const SecCertificateSourceRef kSecLegacyAnchorSource = &_kSecLegacyAnchorSource;

#endif /* SecLegacyAnchorSource */
