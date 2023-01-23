/*
 * Copyright (c) 2008-2011,2013,2015 Apple Inc. All Rights Reserved.
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

#include <stdio.h>

#include "cert.h"
#include "cryptohi.h"

#include "cmspriv.h"
#include "cmstpriv.h"
#include "secoid.h"

#include <libDER/DER_Decode.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#include <Security/SecBase.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecCertificateInternal.h>
#include <Security/SecIdentity.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecPolicy.h>
#include <Security/oidsalg.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include <CommonCrypto/CommonDigest.h>

SECStatus CERT_VerifyCert(SecKeychainRef keychainOrArray __unused,
                          CFArrayRef certs,
                          CFTypeRef policies,
                          CFAbsoluteTime stime,
                          SecTrustRef* trustRef)
{
    SecTrustRef trust = NULL;
    OSStatus rv;

    rv = SecTrustCreateWithCertificates(certs, policies, &trust);
    if (rv) {
        goto loser;
    }

    CFDateRef verifyDate = CFDateCreate(NULL, stime);
    rv = SecTrustSetVerifyDate(trust, verifyDate);
    CFReleaseNull(verifyDate);
    if (rv) {
        goto loser;
    }

    if (trustRef) {
        *trustRef = trust;
    } else {
        SecTrustResultType result;
        /* The caller doesn't want a SecTrust object, so let's evaluate it for them. */
        rv = SecTrustEvaluate(trust, &result);
        if (rv) {
            goto loser;
        }

        switch (result) {
            case kSecTrustResultProceed:
            case kSecTrustResultUnspecified:
                /* TP Verification succeeded and there was either a UserTurst entry
                 telling us to procceed, or no user trust setting was specified. */
                CFReleaseNull(trust);
                break;
            default:
                PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
                rv = SECFailure;
                goto loser;
        }
    }

    return SECSuccess;
loser:
    CFReleaseNull(trust);
    return rv;
}

static CF_RETURNS_RETAINED CFTypeRef CERT_FindItemInAllAvailableKeychains(CFDictionaryRef query)
{
    CFTypeRef item = NULL;
    CFMutableDictionaryRef q = NULL;
    CFDictionaryRef whoAmI = NULL;
    CFErrorRef error = NULL;
    CFDataRef musr = NULL;
    const uint8_t activeUserUuid[16] = "\xA7\x5A\x3A\x35\xA5\x57\x4B\x10\xBE\x2E\x83\x94\x7E\x4A\x34\x72";

    /* Do the standard keychain query */
    require_quiet(errSecItemNotFound == SecItemCopyMatching(query, &item), out);

    /* No item found. Can caller use the system keychain? */
    whoAmI = _SecSecuritydCopyWhoAmI(&error);
    require_quiet(NULL == error && whoAmI && CFDictionaryGetValue(whoAmI, CFSTR("status")), out);
    musr = CFDictionaryGetValue(whoAmI, CFSTR("musr"));
    /* Caller has system-keychain entitlement, is in multi-user mode, and is an active user. */
    if (CFDictionaryGetValue(whoAmI, CFSTR("system-keychain")) && musr &&
        (16 == CFDataGetLength(musr)) &&
        (0 == memcmp(activeUserUuid, CFDataGetBytePtr(musr), 12))) {
        q = CFDictionaryCreateMutableCopy(NULL, CFDictionaryGetCount(query) + 1, query);
        CFDictionaryAddValue(q, kSecUseSystemKeychain, kCFBooleanTrue);
        SecItemCopyMatching(q, &item);
    }

out:
    CFReleaseNull(q);
    CFReleaseNull(whoAmI);
    CFReleaseNull(error);

    return item;
}

SecCertificateRef CERT_FindUserCertByUsage(SecKeychainRef keychainOrArray,
                                           char* nickname,
                                           SECCertUsage usage,
                                           Boolean validOnly,
                                           void* proto_win)
{
    CFStringRef nickname_cfstr =
        CFStringCreateWithCString(kCFAllocatorDefault, nickname, kCFStringEncodingUTF8);
    const void* keys[] = {kSecClass, kSecAttrLabel};
    const void* values[] = {kSecClassCertificate, nickname_cfstr};
    CFDictionaryRef query = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, sizeof(keys) / sizeof(*keys), NULL, NULL);
    CFTypeRef result = NULL;
    result = CERT_FindItemInAllAvailableKeychains(query);
    CFReleaseNull(query);
    CFReleaseNull(nickname_cfstr);
    return (SecCertificateRef)result;
}

CF_RETURNS_RETAINED CFArrayRef CERT_CertChainFromCert(SecCertificateRef cert,
                                                      SECCertUsage usage,
                                                      Boolean includeRoot,
                                                      Boolean mustIncludeRoot)
{
    CFArrayRef certs = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef wrappedCert = NULL;

    policy = SecPolicyCreateBasicX509();
    if (!policy) {
        goto out;
    }

    wrappedCert = CERT_CertListFromCert(cert);
    if (SecTrustCreateWithCertificates(wrappedCert, policy, &trust)) {
        goto out;
    }

    SecTrustResultType result;
    if (SecTrustEvaluate(trust, &result)) {
        goto out;
    }
    certs = SecTrustCopyCertificateChain(trust);
    CFIndex count = certs ? CFArrayGetCount(certs) : 0;

    /* If we weren't able to build a chain to a self-signed cert, warn. */
    Boolean isSelfSigned = false;
    SecCertificateRef lastCert = certs ? (SecCertificateRef)CFArrayGetValueAtIndex(certs, count - 1) : NULL;
    if (lastCert && (0 == SecCertificateIsSelfSigned(lastCert, &isSelfSigned)) && !isSelfSigned) {
        CFStringRef commonName = NULL;
        (void)SecCertificateCopyCommonName(cert, &commonName);
        fprintf(stderr,
                "Warning: unable to build chain to self-signed root for signer \"%s\"\n",
                commonName ? CFStringGetCStringPtr(commonName, kCFStringEncodingUTF8) : "");
        CFReleaseNull(commonName);

        // we don't have a root, so if the caller required one, fail
        if (mustIncludeRoot) {
            CFReleaseNull(certs);
            goto out;
        }
    }

    /* We don't drop the root if there is only 1 certificate in the chain. */
    if (isSelfSigned && !includeRoot && count > 1) {
        CFMutableArrayRef nonRootChain = CFArrayCreateMutableCopy(NULL, count, certs);
        CFArrayRemoveValueAtIndex(nonRootChain, count - 1);
        CFAssignRetained(certs, nonRootChain);
    }

out:
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(wrappedCert);

    return certs;
}

CF_RETURNS_RETAINED CFArrayRef CERT_CertListFromCert(SecCertificateRef cert)
{
    const void* value = cert;
    return cert ? CFArrayCreate(NULL, &value, 1, &kCFTypeArrayCallBacks) : NULL;
}

CFArrayRef CERT_DupCertList(CFArrayRef oldList)
{
    CFRetain(oldList);
    return oldList;
}

// Extract a public key object from a SubjectPublicKeyInfo
SecPublicKeyRef CERT_ExtractPublicKey(SecCertificateRef cert)
{
    return SecCertificateCopyKey(cert);
}

// Extract the issuer and serial number from a certificate
SecCmsIssuerAndSN* CERT_GetCertIssuerAndSN(PRArenaPool* pl, SecCertificateRef cert)
{
    SecCmsIssuerAndSN* certIssuerAndSN;

    void* mark;
    mark = PORT_ArenaMark(pl);
    CFDataRef issuer_data = SecCertificateCopyIssuerSequence(cert);
    CFDataRef serial_data = SecCertificateCopySerialNumberData(cert, NULL);
    if (!issuer_data || CFDataGetLength(issuer_data) < 0 ||
        !serial_data || CFDataGetLength(serial_data) < 0) {
        goto loser;
    }

    SecAsn1Item serialNumber = {.Length = (size_t)CFDataGetLength(serial_data),
                                .Data = (uint8_t*)CFDataGetBytePtr(serial_data)};
    SecAsn1Item issuer = {.Length = (size_t)CFDataGetLength(issuer_data),
                          .Data = (uint8_t*)CFDataGetBytePtr(issuer_data)};

    /* Allocate the SecCmsIssuerAndSN struct. */
    certIssuerAndSN = (SecCmsIssuerAndSN*)PORT_ArenaZAlloc(pl, sizeof(SecCmsIssuerAndSN));
    if (certIssuerAndSN == NULL) {
        goto loser;
    }

    /* Copy the issuer. */
    certIssuerAndSN->derIssuer.Data = (uint8_t*)PORT_ArenaAlloc(pl, issuer.Length);
    if (!certIssuerAndSN->derIssuer.Data) {
        goto loser;
    }
    PORT_Memcpy(certIssuerAndSN->derIssuer.Data, issuer.Data, issuer.Length);
    certIssuerAndSN->derIssuer.Length = issuer.Length;

    /* Copy the serialNumber. */
    certIssuerAndSN->serialNumber.Data = (uint8_t*)PORT_ArenaAlloc(pl, serialNumber.Length);
    if (!certIssuerAndSN->serialNumber.Data) {
        goto loser;
    }
    PORT_Memcpy(certIssuerAndSN->serialNumber.Data, serialNumber.Data, serialNumber.Length);
    certIssuerAndSN->serialNumber.Length = serialNumber.Length;

    CFReleaseNull(serial_data);
    CFReleaseNull(issuer_data);

    PORT_ArenaUnmark(pl, mark);

    return certIssuerAndSN;

loser:
    CFReleaseNull(serial_data);
    CFReleaseNull(issuer_data);
    PORT_ArenaRelease(pl, mark);
    PORT_SetError(SEC_INTERNAL_ONLY);

    return NULL;
}

// find the smime symmetric capabilities profile for a given cert
SecAsn1Item* CERT_FindSMimeProfile(SecCertificateRef cert)
{
    return NULL;
}

// Generate a certificate key from the issuer and serialnumber, then look it up in the database.
// Return the cert if found. "issuerAndSN" is the issuer and serial number to look for
static CF_RETURNS_RETAINED CFTypeRef CERT_FindByIssuerAndSN(CFTypeRef keychainOrArray,
                                                            CFTypeRef class,
                                                            const SecCmsIssuerAndSN* issuerAndSN)
{
    CFTypeRef ident = NULL;
    CFDictionaryRef query = NULL;
    CFDataRef issuer = NULL;
    if (issuerAndSN->serialNumber.Length > LONG_MAX) {
        return NULL;
    }
    CFDataRef serial = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, issuerAndSN->serialNumber.Data, (CFIndex)issuerAndSN->serialNumber.Length, kCFAllocatorNull);

    DERItem der_issuer = {issuerAndSN->derIssuer.Data, issuerAndSN->derIssuer.Length};
    DERDecodedInfo content;
    require_noerr_quiet(DERDecodeItem(&der_issuer, &content), out);
    require_quiet(issuer = createNormalizedX501Name(kCFAllocatorDefault, &content.content), out);

    if (keychainOrArray && (CFGetTypeID(keychainOrArray) == CFArrayGetTypeID()) &&
        CFEqualSafe(class, kSecClassCertificate)) {
        CFIndex c, count = CFArrayGetCount((CFArrayRef)keychainOrArray);
        for (c = 0; c < count; c++) {
            SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex((CFArrayRef)keychainOrArray, c);
            CFDataRef nic = (cert) ? SecCertificateGetNormalizedIssuerContent(cert) : NULL;
            if (nic && CFEqualSafe(nic, issuer)) {
                CFDataRef cert_serial = SecCertificateCopySerialNumberData(cert, NULL);
                if (cert_serial) {
                    bool found = CFEqualSafe(cert_serial, serial);
                    CFReleaseNull(cert_serial);
                    if (found) {
                        CFRetainSafe(cert);
                        ident = cert;
                        goto out;
                    }
                }
            }
        }
    }

    const void* keys[] = {kSecClass, kSecAttrIssuer, kSecAttrSerialNumber, kSecReturnRef};
    const void* values[] = {class, issuer, serial, kCFBooleanTrue};
    query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys) / sizeof(*keys), NULL, NULL);
    ident = CERT_FindItemInAllAvailableKeychains(query);

out:
    CFReleaseNull(query);
    CFReleaseNull(issuer);
    CFReleaseNull(serial);

    return ident;
}

SecIdentityRef CERT_FindIdentityByIssuerAndSN(CFTypeRef keychainOrArray, const SecCmsIssuerAndSN* issuerAndSN)
{
    return (SecIdentityRef)CERT_FindByIssuerAndSN(keychainOrArray, kSecClassIdentity, issuerAndSN);
}

SecCertificateRef CERT_FindCertificateByIssuerAndSN(CFTypeRef keychainOrArray,
                                                    const SecCmsIssuerAndSN* issuerAndSN)
{
    return (SecCertificateRef)CERT_FindByIssuerAndSN(keychainOrArray, kSecClassCertificate, issuerAndSN);
}

// Generate a certificate key from the Subject Key ID, then look it up in the database.
// Return the cert if found. "subjKeyID" is the Subject Key ID to look for
static CF_RETURNS_RETAINED CFTypeRef CERT_FindBySubjectKeyID(CFTypeRef keychainOrArray,
                                                             CFTypeRef class,
                                                             const SecAsn1Item* subjKeyID)
{
    CFTypeRef ident = NULL;
    CFDictionaryRef query = NULL;

    if (!subjKeyID || !subjKeyID->Data || !subjKeyID->Length || subjKeyID->Length > LONG_MAX) {
        return NULL;
    }

    CFDataRef subjectkeyid = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, subjKeyID->Data, (CFIndex)subjKeyID->Length, kCFAllocatorNull);
    if (keychainOrArray && (CFGetTypeID(keychainOrArray) == CFArrayGetTypeID()) &&
        CFEqualSafe(class, kSecClassCertificate)) {
        CFIndex c, count = CFArrayGetCount((CFArrayRef)keychainOrArray);
        for (c = 0; c < count; c++) {
            SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex((CFArrayRef)keychainOrArray, c);
            CFDataRef skid = (cert) ? SecCertificateGetSubjectKeyID(cert) : NULL;
            if (skid && CFEqualSafe(skid, subjectkeyid)) {
                CFRetainSafe(cert);
                ident = cert;
                goto out;
            }
        }
    }

    const void* keys[] = {kSecClass, kSecAttrSubjectKeyID, kSecReturnRef};
    const void* values[] = {class, subjectkeyid, kCFBooleanTrue};
    query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys) / sizeof(*keys), NULL, NULL);
    ident = CERT_FindItemInAllAvailableKeychains(query);

out:
    CFReleaseNull(query);
    CFReleaseNull(subjectkeyid);

    return ident;
}

SecIdentityRef CERT_FindIdentityBySubjectKeyID(CFTypeRef keychainOrArray, const SecAsn1Item* subjKeyID)
{
    return (SecIdentityRef)CERT_FindBySubjectKeyID(keychainOrArray, kSecClassIdentity, subjKeyID);
}

SecCertificateRef CERT_FindCertificateBySubjectKeyID(CFTypeRef keychainOrArray, const SecAsn1Item* subjKeyID)
{
    return (SecCertificateRef)CERT_FindBySubjectKeyID(keychainOrArray, kSecClassCertificate, subjKeyID);
}


SecPublicKeyRef SECKEY_CopyPublicKey(SecPublicKeyRef pubKey)
{
    CFRetainSafe(pubKey);
    return pubKey;
}

void SECKEY_DestroyPublicKey(SecPublicKeyRef CF_CONSUMED pubKey)
{
    CFReleaseNull(pubKey);
}

SecPublicKeyRef SECKEY_CopyPrivateKey(SecPublicKeyRef privKey)
{
    CFRetainSafe(privKey);
    return privKey;
}

void SECKEY_DestroyPrivateKey(SecPublicKeyRef privKey)
{
    CFReleaseNull(privKey);
}

void CERT_DestroyCertificate(SecCertificateRef cert)
{
    CFReleaseNull(cert);
}

SecCertificateRef CERT_DupCertificate(SecCertificateRef cert)
{
    CFRetainSafe(cert);
    return cert;
}

SECStatus WRAP_PubWrapSymKey(SecPublicKeyRef publickey, SecSymmetricKeyRef bulkkey, SecAsn1Item* encKey)
{
    if (CFDataGetLength(bulkkey) < 0) {
        return SECFailure;
    }
    return SecKeyEncrypt(publickey,
                         kSecPaddingPKCS1,
                         CFDataGetBytePtr(bulkkey),
                         (size_t)CFDataGetLength(bulkkey),
                         encKey->Data,
                         &encKey->Length);
}

#define MAX_KEY_SIZE 8192 / 8
SecSymmetricKeyRef WRAP_PubUnwrapSymKey(SecPrivateKeyRef privkey, const SecAsn1Item* encKey, SECOidTag bulkalgtag)
{
    size_t bulkkey_size = encKey->Length;
    if (bulkkey_size > MAX_KEY_SIZE) {
        return NULL;
    }

    uint8_t* bulkkey_buffer = (uint8_t*)malloc(bulkkey_size);
    if (!bulkkey_buffer) {
        return NULL;
    }
    if (SecKeyDecrypt(privkey, kSecPaddingPKCS1, encKey->Data, encKey->Length, bulkkey_buffer, &bulkkey_size)) {
        return NULL;
    }

    CFDataRef bulkkey = CFDataCreateWithBytesNoCopy(
        kCFAllocatorDefault, bulkkey_buffer, (CFIndex)bulkkey_size, kCFAllocatorMalloc);
    return (SecSymmetricKeyRef)bulkkey;
}


bool CERT_CheckIssuerAndSerial(SecCertificateRef cert, SecAsn1Item* issuer, SecAsn1Item* serial)
{
    do {
        CFDataRef cert_issuer = SecCertificateCopyIssuerSequence(cert);
        if (!cert_issuer) {
            break;
        }
        if ((issuer->Length != (size_t)CFDataGetLength(cert_issuer)) ||
            memcmp(issuer->Data, CFDataGetBytePtr(cert_issuer), issuer->Length)) {
            CFReleaseNull(cert_issuer);
            break;
        }
        CFReleaseNull(cert_issuer);
        CFDataRef cert_serial = SecCertificateCopySerialNumberData(cert, NULL);
        if (!cert_serial) {
            break;
        }
        if ((serial->Length != (size_t)CFDataGetLength(cert_serial)) ||
            memcmp(serial->Data, CFDataGetBytePtr(cert_serial), serial->Length)) {
            CFReleaseNull(cert_serial);
            break;
        }
        CFReleaseNull(cert_serial);
        return true;
    } while (0);
    return false;
}
