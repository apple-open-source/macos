/*
 *  crypto-embedded.c
 *  libsecurity_smime
 *
 *  Created by Conrad Sauerwald on 2/7/08.
 *  Copyright (c) 2008-2011,2013 Apple Inc. All Rights Reserved.
 *
 */

#include <stdio.h>

#include "cert.h"
#include "cryptohi.h"

#include "cmstpriv.h"
#include "secoid.h"
#include "cmspriv.h"

#include <libDER/DER_Decode.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#include <Security/SecBase.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/oidsalg.h>
#include <Security/SecPolicy.h>
#include <Security/SecItem.h>
#include <Security/SecIdentity.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecKeyPriv.h>

#include <CommonCrypto/CommonDigest.h>
#include <AssertMacros.h>

SECStatus
CERT_VerifyCert(SecKeychainRef keychainOrArray __unused, CFArrayRef certs,
		CFTypeRef policies, CFAbsoluteTime stime, SecTrustRef *trustRef)
{
    SecTrustRef trust = NULL;
    OSStatus rv;

    rv = SecTrustCreateWithCertificates(certs, policies, &trust);
    if (rv)
        goto loser;

    CFDateRef verifyDate = CFDateCreate(NULL, stime);
    rv = SecTrustSetVerifyDate(trust, verifyDate);
    CFRelease(verifyDate);
    if (rv)
	goto loser;

    if (trustRef)
    {
	*trustRef = trust;
    }
    else
    {
	SecTrustResultType result;
	/* The caller doesn't want a SecTrust object, so let's evaluate it for them. */
	rv = SecTrustEvaluate(trust, &result);
	if (rv)
	    goto loser;

	switch (result)
	{
	case kSecTrustResultProceed:
	case kSecTrustResultUnspecified:
	    /* TP Verification succeeded and there was either a UserTurst entry
	       telling us to procceed, or no user trust setting was specified. */
	    CFRelease(trust);
	    break;
	default:
	    PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
	    rv = SECFailure;
	    goto loser;
	    break;
	}
    }

    return SECSuccess;
loser:
    if (trust)
	CFRelease(trust);

    return rv;
}


SecCertificateRef CERT_FindUserCertByUsage(SecKeychainRef keychainOrArray,
			 char *nickname,SECCertUsage usage,Boolean validOnly,void *proto_win)
{
	CFStringRef nickname_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, nickname, kCFStringEncodingUTF8);
	const void *keys[] = { kSecClass, kSecAttrLabel };
	const void *values[] = { kSecClassCertificate,  nickname_cfstr };
	CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys)/sizeof(*keys), NULL, NULL);
	CFTypeRef result = NULL;
	SecItemCopyMatching(query, &result);
	CFRelease(query);
	CFRelease(nickname_cfstr);
	return (SecCertificateRef)result;
}

CF_RETURNS_RETAINED CFArrayRef CERT_CertChainFromCert(SecCertificateRef cert, SECCertUsage usage, Boolean includeRoot)
{
    CFMutableArrayRef certs = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef wrappedCert = NULL;
    
    policy = SecPolicyCreateBasicX509();
    if (!policy)
        goto out;
    
    wrappedCert = CERT_CertListFromCert(cert);
    if (SecTrustCreateWithCertificates(wrappedCert, policy, &trust))
        goto out;

	SecTrustResultType result;
    if (SecTrustEvaluate(trust, &result))
        goto out;
    CFIndex idx, count = SecTrustGetCertificateCount(trust);
    certs = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    for(idx = 0; idx < count; idx++)
        CFArrayAppendValue(certs, SecTrustGetCertificateAtIndex(trust, idx));
    
out:
    if (trust) CFRelease(trust);
    if (policy) CFRelease(policy);
    if (wrappedCert) CFRelease(wrappedCert);

    return certs;
}

CFArrayRef CERT_CertListFromCert(SecCertificateRef cert)
{
    const void *value = cert;
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
    return SecCertificateCopyPublicKey(cert);
}

// Extract the issuer and serial number from a certificate
SecCmsIssuerAndSN *CERT_GetCertIssuerAndSN(PRArenaPool *pl, SecCertificateRef cert)
{
    SecCmsIssuerAndSN *certIssuerAndSN;

    void *mark;
    mark = PORT_ArenaMark(pl);
    CFDataRef serial_data = NULL;
    CFDataRef issuer_data = SecCertificateCopyIssuerSequence(cert);
    if (!issuer_data)
        goto loser;
    serial_data = SecCertificateCopySerialNumber(cert);
    if (!serial_data)
        goto loser;
    
    SecAsn1Item serialNumber = { CFDataGetLength(serial_data),
        (uint8_t *)CFDataGetBytePtr(serial_data) };
    SecAsn1Item issuer = { CFDataGetLength(issuer_data),
        (uint8_t *)CFDataGetBytePtr(issuer_data) };
    
        /* Allocate the SecCmsIssuerAndSN struct. */
    certIssuerAndSN = (SecCmsIssuerAndSN *)PORT_ArenaZAlloc (pl, sizeof(SecCmsIssuerAndSN));
    if (certIssuerAndSN == NULL)
	goto loser;

    /* Copy the issuer. */
    certIssuerAndSN->derIssuer.Data = (uint8_t *) PORT_ArenaAlloc(pl, issuer.Length);
    if (!certIssuerAndSN->derIssuer.Data)
	goto loser;
    PORT_Memcpy(certIssuerAndSN->derIssuer.Data, issuer.Data, issuer.Length);
    certIssuerAndSN->derIssuer.Length = issuer.Length;

    /* Copy the serialNumber. */
    certIssuerAndSN->serialNumber.Data = (uint8_t *) PORT_ArenaAlloc(pl, serialNumber.Length);
    if (!certIssuerAndSN->serialNumber.Data)
	goto loser;
    PORT_Memcpy(certIssuerAndSN->serialNumber.Data, serialNumber.Data, serialNumber.Length);
    certIssuerAndSN->serialNumber.Length = serialNumber.Length;

    CFRelease(serial_data);
    CFRelease(issuer_data);

    PORT_ArenaUnmark(pl, mark);
    
    return certIssuerAndSN;

loser:
    if (serial_data)
        CFRelease(serial_data);
    if (issuer_data)
        CFRelease(issuer_data);
    PORT_ArenaRelease(pl, mark);
    PORT_SetError(SEC_INTERNAL_ONLY);

    return NULL;
}

// find the smime symmetric capabilities profile for a given cert
SecAsn1Item *CERT_FindSMimeProfile(SecCertificateRef cert)
{
    return NULL;
}

// Generate a certificate key from the issuer and serialnumber, then look it up in the database.
// Return the cert if found. "issuerAndSN" is the issuer and serial number to look for
static CFTypeRef CERT_FindByIssuerAndSN (CFTypeRef keychainOrArray, CFTypeRef class, const SecCmsIssuerAndSN *issuerAndSN)
{
    CFTypeRef ident = NULL;
	CFDictionaryRef query = NULL;
	CFDataRef issuer = NULL;
    CFDataRef serial = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
        issuerAndSN->serialNumber.Data, issuerAndSN->serialNumber.Length, 
		kCFAllocatorNull);
	
	DERItem der_issuer = { issuerAndSN->derIssuer.Data,
		 					issuerAndSN->derIssuer.Length };
	DERDecodedInfo content;	
	require_noerr_quiet(DERDecodeItem(&der_issuer, &content), out);
    require_quiet(issuer = createNormalizedX501Name(kCFAllocatorDefault,
		&content.content), out);

    if (keychainOrArray && (CFGetTypeID(keychainOrArray) == CFArrayGetTypeID()) && CFEqual(class, kSecClassCertificate))
    {
        CFIndex c, count = CFArrayGetCount((CFArrayRef)keychainOrArray);
        for (c = 0; c < count; c++) {
            SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex((CFArrayRef)keychainOrArray, c);
            CFDataRef nic = (cert) ? SecCertificateGetNormalizedIssuerContent(cert) : NULL;
            if (nic && CFEqual(nic, issuer)) {
                CFDataRef cert_serial = SecCertificateCopySerialNumber(cert);
                if (cert_serial) {
		  bool found = CFEqual(cert_serial, serial);
		  CFRelease(cert_serial);
		  if (found) {
                    CFRetain(cert);
                    ident = cert;
                    goto out;
		  }
		}
	    }
	}
    }

	const void *keys[] = { kSecClass, kSecAttrIssuer, kSecAttrSerialNumber, kSecReturnRef };
	const void *values[] = { class, issuer, serial, kCFBooleanTrue };
	query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys)/sizeof(*keys), NULL, NULL);
	require_noerr_quiet(SecItemCopyMatching(query, (CFTypeRef*)&ident), out);

out:
    if (query)
        CFRelease(query);
    if (issuer)
        CFRelease(issuer);
    if (serial)
        CFRelease(serial);

    return ident;
}

SecIdentityRef CERT_FindIdentityByIssuerAndSN (CFTypeRef keychainOrArray, const SecCmsIssuerAndSN *issuerAndSN)
{
    return (SecIdentityRef)CERT_FindByIssuerAndSN(keychainOrArray, kSecClassIdentity, issuerAndSN);
}

SecCertificateRef CERT_FindCertificateByIssuerAndSN (CFTypeRef keychainOrArray, const SecCmsIssuerAndSN *issuerAndSN)
{
    return (SecCertificateRef)CERT_FindByIssuerAndSN(keychainOrArray, kSecClassCertificate, issuerAndSN);
}

SecIdentityRef CERT_FindIdentityBySubjectKeyID (CFTypeRef keychainOrArray __unused, const SecAsn1Item *subjKeyID)
{
    SecIdentityRef ident = NULL;
	CFDictionaryRef query = NULL;
    CFDataRef subjectkeyid = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, subjKeyID->Data, subjKeyID->Length, kCFAllocatorNull);

	const void *keys[] = { kSecClass, kSecAttrSubjectKeyID, kSecReturnRef };
	const void *values[] = { kSecClassIdentity, subjectkeyid, kCFBooleanTrue };
	query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys)/sizeof(*keys), NULL, NULL);
	require_noerr_quiet(SecItemCopyMatching(query, (CFTypeRef*)&ident), out);

out:
    if (query)
        CFRelease(query);
    if (subjectkeyid)
        CFRelease(subjectkeyid);

    return ident;
}



SecPublicKeyRef SECKEY_CopyPublicKey(SecPublicKeyRef pubKey)
{
    CFRetain(pubKey);
    return pubKey;
}

void SECKEY_DestroyPublicKey(SecPublicKeyRef pubKey)
{
    CFRelease(pubKey);
}

SecPublicKeyRef SECKEY_CopyPrivateKey(SecPublicKeyRef privKey)
{
    CFRetain(privKey);
    return privKey;
}

void SECKEY_DestroyPrivateKey(SecPublicKeyRef privKey)
{
    CFRelease(privKey);
}

void CERT_DestroyCertificate(SecCertificateRef cert)
{
    CFRelease(cert);
}

SecCertificateRef CERT_DupCertificate(SecCertificateRef cert)
{
    CFRetain(cert);
    return cert;
}

SECStatus
WRAP_PubWrapSymKey(SecPublicKeyRef publickey,
		   SecSymmetricKeyRef bulkkey,
		   SecAsn1Item * encKey)
{
    return SecKeyEncrypt(publickey, kSecPaddingPKCS1, 
                        CFDataGetBytePtr(bulkkey), CFDataGetLength(bulkkey),
                        encKey->Data, &encKey->Length);
}

SecSymmetricKeyRef
WRAP_PubUnwrapSymKey(SecPrivateKeyRef privkey, const SecAsn1Item *encKey, SECOidTag bulkalgtag)
{
    size_t bulkkey_size = encKey->Length;
    uint8_t bulkkey_buffer[bulkkey_size];
    if (SecKeyDecrypt(privkey, kSecPaddingPKCS1, 
        encKey->Data, encKey->Length, bulkkey_buffer, &bulkkey_size))
            return NULL;

    CFDataRef bulkkey = CFDataCreate(kCFAllocatorDefault, bulkkey_buffer, bulkkey_size);
    return (SecSymmetricKeyRef)bulkkey;
}


bool
CERT_CheckIssuerAndSerial(SecCertificateRef cert, SecAsn1Item *issuer, SecAsn1Item *serial)
{
    do {
        CFDataRef cert_issuer = SecCertificateCopyIssuerSequence(cert);
        if (!cert_issuer)
            break;
        if ((issuer->Length != (size_t)CFDataGetLength(cert_issuer)) ||
            memcmp(issuer->Data, CFDataGetBytePtr(cert_issuer), issuer->Length)) {
                CFRelease(cert_issuer);
                break;
            }
        CFRelease(cert_issuer);
        CFDataRef cert_serial = SecCertificateCopySerialNumber(cert);
        if (!cert_serial)
            break;
        if ((serial->Length != (size_t)CFDataGetLength(cert_serial)) ||
            memcmp(serial->Data, CFDataGetBytePtr(cert_serial), serial->Length)) {
                CFRelease(cert_serial);
                break;
        }
        CFRelease(cert_serial);
        return true;
    } while(0);
    return false;
}
