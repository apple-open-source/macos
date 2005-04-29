/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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

/*
 * pkinit_apple_cert_store.c - PKINIT certificate storage/retrieval utilities, 
 *			       MAC OS X version
 *
 * Created 26 May 2004 by Doug Mitchell at Apple.
 */
 
#include "pkinit_cert_store.h"
#include "pkinit_asn1.h"
#include "pkinit_apple_utils.h"
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <assert.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <CommonCrypto/CommonDigest.h>
#include <sys/errno.h>

/*
 * Client cert info is stored in preferences with this following parameters:
 *
 * key      = kPkinitClientCertKey
 * appID    = kPkinitClientCertApp
 * username = kCFPreferencesCurrentUser
 * hostname = kCFPreferencesAnyHost   
 *
 * The stored property list is a CFDictionary. Keys in the dictionary are
 * principal names (e.g. foobar@REALM.LOCAL). 
 *
 * Values in the dictionary are raw data containing the DER-encoded issuer and
 * serial number of the certificate. 
 *
 * When obtaining a PKINIT cert, if an entry in the CFDictionary for the specified
 * principal is not found, the entry for the default will be used if it's there.
 */

/* 
 * NOTE: ANSI C code requires an Apple-Custom -fconstant-cfstrings CFLAGS to 
 * use CFSTR in a const declaration so we just declare the C strings here. 
 */
#define kPkinitClientCertKey		"KRBClientCert"
#define kPkinitClientCertApp		"edu.mit.Kerberos.pkinit"

/*
 * KDC cert stored in this keychain. It's linked to systemkeychain so that if
 * a root process tries to unlock it, it auto-unlocks.
 */
#define KDC_KEYCHAIN    "/var/db/krb5kdc/kdc.keychain"

/* 
 * Given a certificate, obtain the DER-encoded issuer and serial number. Result
 * is mallocd and must be freed by caller. 
 */
static OSStatus pkinit_get_cert_issuer_sn(
    SecCertificateRef certRef, 
    CSSM_DATA *issuerSerial)		// mallocd and RETURNED
{
    OSStatus ortn;
    CSSM_DATA certData;
    CSSM_CL_HANDLE clHand;
    CSSM_HANDLE resultHand;
    uint32 numFields;
    CSSM_DATA_PTR issuer = NULL;
    CSSM_DATA_PTR serial = NULL;
    krb5_data INIT_KDATA(issuerKrb);
    krb5_data INIT_KDATA(serialKrb);
    krb5_data INIT_KDATA(issuerSerialKrb);
    
    assert(certRef != NULL);
    assert(issuerSerial != NULL);
    
    /* break the cert down into CDSA-layer components */
    ortn = SecCertificateGetData(certRef, &certData);
    if(ortn) {
	pkiCssmErr("SecCertificateGetData", ortn);
	return ortn;
    }
    ortn = SecCertificateGetCLHandle(certRef, &clHand);
    if(ortn) {
	pkiCssmErr("SecCertificateGetData", ortn);
	return ortn;
    }
    
    /* get the DER encoded issuer (not normalized) */
    ortn = CSSM_CL_CertGetFirstFieldValue(clHand, &certData, 
	&CSSMOID_X509V1IssuerNameStd, &resultHand, &numFields, &issuer);
    if(ortn) {
	pkiCssmErr("CSSM_CL_CertGetFirstFieldValue(issuer)", ortn);
	return ortn;
    }
    /* subsequent errors to errOut: */
    CSSM_CL_CertAbortQuery(clHand, resultHand);
    
    /* get the serial number */
    ortn = CSSM_CL_CertGetFirstFieldValue(clHand, &certData, &CSSMOID_X509V1SerialNumber,
	&resultHand, &numFields, &serial);
    if(ortn) {
	pkiCssmErr("CSSM_CL_CertGetFirstFieldValue(serial)", ortn);
	goto errOut;
    }
    CSSM_CL_CertAbortQuery(clHand, resultHand);
    
    /* encode them together */
    PKI_CSSM_TO_KRB_DATA(issuer, &issuerKrb);
    PKI_CSSM_TO_KRB_DATA(serial, &serialKrb);
    ortn = pkinit_issuer_serial_encode(&issuerKrb, &serialKrb, &issuerSerialKrb);
    if(ortn) {
	pkiCssmErr("pkinit_issuer_serial_encode", ortn);
	goto errOut;
    }
    
    /* transfer ownership to caller */
    PKI_KRB_TO_CSSM_DATA(&issuerSerialKrb, issuerSerial);
errOut:
    if(issuer) {
	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1IssuerNameStd, issuer);
    }
    if(serial) {
	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SerialNumber, serial);
    }
    return ortn;
}

/* 
 * Determine if specified identity's cert's issuer and serial number match the
 * provided issuer and serial number. Returns nonzero on match, else returns zero.
 */
static int pkinit_issuer_sn_match(
    SecIdentityRef idRef, 
    const CSSM_DATA *matchIssuerSerial)
{
    OSStatus ortn;
    SecCertificateRef certRef = NULL;
    CSSM_DATA INIT_CDATA(certIssuerSerial);
    int ourRtn = 0;

    assert(idRef != NULL);
    assert(matchIssuerSerial != NULL);
    
    /* Get this cert's issuer/serial number */
    ortn = SecIdentityCopyCertificate(idRef, &certRef);
    if(ortn) {
	pkiCssmErr("SecIdentityCopyCertificate", ortn);
	return 0;
    }
    /* subsequent errors to errOut: */
    ortn = pkinit_get_cert_issuer_sn(certRef, &certIssuerSerial);
    if(ortn) {
	pkiCssmErr("SecIdentityCopyCertificate", ortn);
	goto errOut;
    }
    ourRtn = pkiCompareCssmData(matchIssuerSerial, &certIssuerSerial) ? 1 : 0;
errOut:
    if(certRef != NULL) {
	CFRelease(certRef);
    }
    if(certIssuerSerial.Data != NULL) {
	free(certIssuerSerial.Data);
    }
    return ourRtn;
}

/*
 * Search specified keychain/array/NULL (NULL meaning the default search list) for
 * an Identity matching specified key usage and optional Issuer/Serial number. 
 * If issuer/serial is specified and no identities match, or if no identities found
 * matching specified Key usage, errSecItemNotFound is returned.
 *
 * Caller must CFRelease a non-NULL returned idRef. 
 */
static OSStatus pkinit_search_ident(
    CFTypeRef		keychainOrArray,
    CSSM_KEYUSE		keyUsage,
    const CSSM_DATA     *issuerSerial,  // optional
    SecIdentityRef      *foundId)	// RETURNED
{
    OSStatus ortn;
    SecIdentityRef idRef = NULL;
    SecIdentitySearchRef srchRef = NULL;
    
    ortn = SecIdentitySearchCreate(keychainOrArray, keyUsage, &srchRef);
    if(ortn) {
	pkiCssmErr("SecIdentitySearchCreate", ortn);
	return ortn;
    }
    do {
	ortn = SecIdentitySearchCopyNext(srchRef, &idRef);
	if(ortn != noErr) {
	    break;
	}
	if(issuerSerial == NULL) {
	    /* no match needed, we're done - this is the KDC cert case */
	    break;
	}
	else if(pkinit_issuer_sn_match(idRef, issuerSerial)) {
	    /* match, we're done */
	    break;
	}
	/* finished with this one */
	CFRelease(idRef);
	idRef = NULL;
    } while(ortn == noErr);
    
    CFRelease(srchRef);
    if(idRef == NULL) {
	return errSecItemNotFound;
    }
    else {
	*foundId = idRef;
	return noErr;
    }
}

/*
 * In Mac OS terms, get the keychain on which a given identity resides. 
 */
static krb5_error_code pkinit_cert_to_db(
    pkinit_signing_cert_t   idRef,
    pkinit_cert_db_t	    *dbRef)
{
    SecKeychainRef kcRef = NULL;
    SecKeyRef keyRef = NULL;
    OSStatus ortn;

    /* that's an identity - get the associated key's keychain */
    ortn = SecIdentityCopyPrivateKey((SecIdentityRef)idRef, &keyRef);
    if(ortn) {
	pkiCssmErr("SecIdentityCopyPrivateKey", ortn);
	return ortn;
    }
    ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)keyRef, &kcRef);
    if(ortn) {
	pkiCssmErr("SecKeychainItemCopyKeychain", ortn);
    }
    else {
	*dbRef = (pkinit_cert_db_t)kcRef;
    }
    CFRelease(keyRef);
    return ortn;
}

/* 
 * Obtain the CFDictionary representing this user's PKINIT client cert prefs, if it 
 * exists. Returns noErr or errSecItemNotFound as appropriate. 
 */
static OSStatus pkinit_get_pref_dict(
    CFDictionaryRef *dict)
{
    CFDictionaryRef theDict;
    theDict = (CFDictionaryRef)CFPreferencesCopyValue(CFSTR(kPkinitClientCertKey),
	CFSTR(kPkinitClientCertApp), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if(theDict == NULL) {
	pkiDebug("pkinit_get_pref_dict: no kPkinitClientCertKey\n");
	return errSecItemNotFound;
    }
    if(CFGetTypeID(theDict) != CFDictionaryGetTypeID()) {
	pkiDebug("pkinit_get_pref_dict: bad kPkinitClientCertKey pref\n");
	CFRelease(theDict);
	return errSecItemNotFound;
    }
    *dict = theDict;
    return noErr;
}

#pragma mark --- Public client side functions ---

/*
 * Obtain signing cert for specified principal. On successful (non-NULL) return, 
 * caller must eventually release the cert with pkinit_release_cert().
 */
krb5_error_code pkinit_get_client_cert(
    const char		    *principal,     // full principal string
    pkinit_signing_cert_t   *client_cert)
{
    CFDataRef issuerSerial = NULL;
    CSSM_DATA issuerSerialData;
    SecIdentityRef idRef = NULL;
    OSStatus ortn;
    CFDictionaryRef theDict = NULL;
    
    if(principal == NULL) {
	return errSecItemNotFound;
    }
    
    /* Is there a stored preference for PKINIT certs for this user? */
    ortn = pkinit_get_pref_dict(&theDict);
    if(ortn) {
	return ortn;
    }
    
    /* Entry in the dictionary for specified principal? */
    CFStringRef cfPrinc = CFStringCreateWithCString(NULL, principal, 
	kCFStringEncodingASCII);
    issuerSerial = (CFDataRef)CFDictionaryGetValue(theDict, cfPrinc);
    CFRelease(cfPrinc);
    if(issuerSerial == NULL) {
	pkiDebug("pkinit_get_client_cert: no identity found\n");
	ortn = errSecItemNotFound;
	goto errOut;
    }
    if(CFGetTypeID(issuerSerial) != CFDataGetTypeID()) {
	pkiDebug("pkinit_get_client_cert: bad kPkinitClientCertKey value\n");
	ortn = errSecItemNotFound;
	goto errOut;
    }
    
    issuerSerialData.Data = (uint8 *)CFDataGetBytePtr(issuerSerial);
    issuerSerialData.Length = CFDataGetLength(issuerSerial);
    
    /* find a cert with that issuer/serial number in default search list */
    ortn = pkinit_search_ident(NULL, CSSM_KEYUSE_SIGN | CSSM_KEYUSE_ENCRYPT, 
	&issuerSerialData, &idRef);
    if(ortn) {
	pkiDebug("pkinit_get_client_cert: no identity found!\n");
	pkiCssmErr("pkinit_search_ident", ortn);
    }
    else {
	*client_cert = (pkinit_signing_cert_t)idRef;
    }
errOut:
    if(theDict) {
	CFRelease(theDict);
    }
    return ortn;
}

/*
 * Store the specified certificate (or, more likely, some platform-dependent
 * reference to it) as the specified principal's signing cert. Passing
 * in NULL for the client_cert has the effect of deleting the relevant entry
 * in the cert storage.
 */
krb5_error_code pkinit_set_client_cert(
    const char		    *principal,     // full principal string
    pkinit_signing_cert_t   client_cert)
{
    SecIdentityRef idRef = (SecIdentityRef)client_cert;
    OSStatus ortn;
    CSSM_DATA issuerSerial = {0, NULL};
    CFDataRef cfIssuerSerial = NULL;
    CFDictionaryRef existDict = NULL;
    CFMutableDictionaryRef newDict = NULL;
    SecCertificateRef certRef = NULL;
    CFStringRef keyStr = NULL;

    if(idRef != NULL) {
	if(CFGetTypeID(idRef) != SecIdentityGetTypeID()) {
	    return paramErr;
	}
    
	/* Get the cert */
	ortn = SecIdentityCopyCertificate(idRef, &certRef);
	if(ortn) {
	    pkiCssmErr("SecIdentityCopyCertificate", ortn);
	    return ortn;
	}
	
	/* Cook up DER-encoded issuer/serial number */
	ortn = pkinit_get_cert_issuer_sn(certRef, &issuerSerial);
	if(ortn) {
	    goto errOut;
	}
    }
    
    /* 
     * Obtain the existing pref for kPkinitClientCertKey as a CFDictionary, or
     * cook up a new one. 
     */
    ortn = pkinit_get_pref_dict(&existDict);
    if(ortn == noErr) {
	/* dup to a mutable dictionary */
	newDict = CFDictionaryCreateMutableCopy(NULL, 0, existDict);
    }
    else {
	if(idRef == NULL) {
	    /* no existing entry, nothing to delete, we're done */
	    return noErr;
	}
	newDict = CFDictionaryCreateMutable(NULL, 0,
	    &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    if(newDict == NULL) {
	ortn = ENOMEM;
	goto errOut;
    }

    /* issuer / serial number ==> that dictionary */
    keyStr = CFStringCreateWithCString(NULL, principal, kCFStringEncodingASCII);
    if(idRef == NULL) {
	CFDictionaryRemoveValue(newDict, keyStr);
    }
    else {
	cfIssuerSerial = CFDataCreate(NULL, issuerSerial.Data, issuerSerial.Length);
	CFDictionarySetValue(newDict, keyStr, cfIssuerSerial);
    }
    
    /* dictionary ==> prefs */
    CFPreferencesSetValue(CFSTR(kPkinitClientCertKey), newDict, 
	CFSTR(kPkinitClientCertApp), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if(CFPreferencesSynchronize(CFSTR(kPkinitClientCertApp), kCFPreferencesCurrentUser, 
	    kCFPreferencesAnyHost)) {
	ortn = noErr;
    }
    else {
	ortn = wrPermErr;   /* any better ideas? */
    }
errOut:
    if(certRef) {
	CFRelease(certRef);
    }   
    if(cfIssuerSerial) {
	CFRelease(cfIssuerSerial);
    }
    if(issuerSerial.Data) {
	free(issuerSerial.Data);
    }
    if(existDict) {
	CFRelease(existDict);
    }
    if(newDict) {
	CFRelease(newDict);
    }
    if(keyStr) {
	CFRelease(keyStr);
    }
    return ortn;
}

/* 
 * Obtain a reference to the client's cert database. Specify either principal
 * name or client_cert as obtained from pkinit_get_client_cert().
 */
krb5_error_code pkinit_get_client_cert_db(
    const char		    *principal,     // full principal string
    pkinit_signing_cert_t   client_cert,    // optional, from pkinit_get_client_cert()
    pkinit_cert_db_t	    *client_cert_db)   // RETURNED
{
    krb5_error_code krtn;
    pkinit_signing_cert_t local_cert;
    
    assert((client_cert != NULL) || (principal != NULL));
    if(client_cert == NULL) {
	/* caller didn't provide, look it up */
	krtn = pkinit_get_client_cert(principal, &local_cert);
	if(krtn) {
	    return krtn;
	}
    }
    else {
	/* easy case */
	local_cert = client_cert;
    }
    krtn = pkinit_cert_to_db(local_cert, client_cert_db);
    if(client_cert == NULL) {
	pkinit_release_cert(local_cert);
    }
    return krtn;
}

#pragma mark --- Public server side functions ---

/* 
 * Due to Radar 3680128, the KDC keychain does NOT auto-unlock. We unlock it here
 * with a hard coded password. Don't even think of actually shipping this code. 
 */
#define KDC_KEYCHAIN_MANUAL_UNLOCK  0
#if     KDC_KEYCHAIN_MANUAL_UNLOCK
#define KDC_KC_PWD  "password"
#endif

/*
 * Obtain the KDC signing cert.
 */
krb5_error_code pkinit_get_kdc_cert(
    pkinit_signing_cert_t *kdc_cert)
{
    SecIdentityRef idRef = NULL;
    SecKeychainRef kcRef = NULL;
    OSStatus ortn;
    
    ortn = SecKeychainOpen(KDC_KEYCHAIN, &kcRef);
    if(ortn) {
	pkiCssmErr("SecKeychainOpen", ortn);
	return ortn;
    }
    #if KDC_KEYCHAIN_MANUAL_UNLOCK
    SecKeychainUnlock(kcRef, strlen(KDC_KC_PWD), KDC_KC_PWD, TRUE);
    #endif
    ortn = pkinit_search_ident(kcRef, CSSM_KEYUSE_SIGN, NULL, &idRef);
    if(ortn) {
	pkiDebug("pkinit_get_kdc_cert: no identity found!\n");
	pkiCssmErr("pkinit_search_ident", ortn);
    }
    else {
	*kdc_cert = (pkinit_signing_cert_t)idRef;
    }
    CFRelease(kcRef);
    return ortn;
}

/* 
 * Obtain a reference to the KDC's cert database.
 */
krb5_error_code pkinit_get_kdc_cert_db(
    pkinit_cert_db_t   *kdc_cert_db)
{
    pkinit_signing_cert_t kdcCert = NULL;
    krb5_error_code krtn;
    
    krtn = pkinit_get_kdc_cert(&kdcCert);
    if(krtn) {
	return krtn;
    }
    krtn = pkinit_cert_to_db(kdcCert, kdc_cert_db);
    pkinit_release_cert(kdcCert);
    return krtn;
}

/*
 * Release certificate references obtained via pkinit_get_client_cert() and
 * pkinit_get_kdc_cert().
 */
void pkinit_release_cert(
    pkinit_signing_cert_t   cert)
{
    if(cert == NULL) {
	return;
    }
    CFRelease((CFTypeRef)cert);
}

/*
 * Release database references obtained via pkinit_get_client_cert_db() and
 * pkinit_get_kdc_cert_db().
 */
extern void pkinit_release_cert_db(
    pkinit_cert_db_t	    cert_db)
{
    if(cert_db == NULL) {
	return;
    }
    CFRelease((CFTypeRef)cert_db);
}


/* 
 * Obtain a mallocd C-string representation of a certificate's SHA1 digest. 
 * Only error is a NULL return indicating memory failure. 
 * Caller must free the returned string.
 */
char *pkinit_cert_hash_str(
    const krb5_data *cert)
{
    CC_SHA1_CTX ctx;
    char *outstr;
    char *cpOut;
    unsigned char digest[CC_SHA1_DIGEST_LENGTH];
    unsigned dex;
    
    assert(cert != NULL);
    CC_SHA1_Init(&ctx);
    CC_SHA1_Update(&ctx, cert->data, cert->length);
    CC_SHA1_Final(digest, &ctx);
    
    outstr = (char *)malloc((2 * CC_SHA1_DIGEST_LENGTH) + 1);
    if(outstr == NULL) {
	return NULL;
    }
    cpOut = outstr;
    for(dex=0; dex<CC_SHA1_DIGEST_LENGTH; dex++) {
	sprintf(cpOut, "%02X", (unsigned)(digest[dex]));
	cpOut += 2;
    }
    *cpOut = '\0';
    return outstr;
}

