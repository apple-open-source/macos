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
 * pkinit_apple_asn1.c - ASN.1 encode/decode routines for PKINIT, Mac OS X version
 *
 * Created 19 May 2004 by Doug Mitchell.
 */
 
#include "pkinit_asn1.h"
#include "pkinit_apple_utils.h"
#include <stddef.h>
#include <Security/secasn1t.h>
#include <Security/asn1Templates.h>
#include <Security/nameTemplates.h>
#include <Security/keyTemplates.h>
#include <Security/SecAsn1Coder.h>
#include <sys/errno.h>
#include <assert.h>
#include <strings.h>

#pragma mark ----- utility routines -----

/* malloc a NULL-ed array of pointers of size num+1 */
static void **pkiNssNullArray(
    uint32 num,
    SecAsn1CoderRef coder)
{
    unsigned len = (num + 1) * sizeof(void *);
    void **p = (void **)SecAsn1Malloc(coder, len);
    memset(p, 0, len);
    return p;
}

#pragma mark ====== begin PA-PK-AS-REQ components ======

#pragma mark ----- Checksum -----
 
typedef struct {
    CSSM_DATA   checksumType;
    CSSM_DATA   checksum;
} KRB5_Checksum;

static const SecAsn1Template KRB5_ChecksumTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_Checksum) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_Checksum,checksumType), 
      kSecAsn1IntegerTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_Checksum,checksum), 
      kSecAsn1OctetStringTemplate },
    { 0 }
};

#pragma mark ----- pkAuthenticator -----

typedef struct {
    CSSM_DATA       cusec;			// INTEGER, microseconds
    CSSM_DATA       ctime;			// UTC time (with trailing 'Z')
    CSSM_DATA       nonce;  
    KRB5_Checksum   paChecksum;
} KRB5_PKAuthenticator;

static const SecAsn1Template KRB5_PKAuthenticatorTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_PKAuthenticator) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_PKAuthenticator,cusec), 
      kSecAsn1IntegerTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_PKAuthenticator,ctime), 
      kSecAsn1GeneralizedTimeTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 2,
      offsetof(KRB5_PKAuthenticator,nonce), 
      kSecAsn1IntegerTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 3,
      offsetof(KRB5_PKAuthenticator,paChecksum), 
      KRB5_ChecksumTemplate },
    { 0 }
};

#pragma mark ----- AuthPack -----

typedef struct {
    KRB5_PKAuthenticator		pkAuth;
    CSSM_X509_SUBJECT_PUBLIC_KEY_INFO   *pubKeyInfo;	// OPTIONAL
} KRB5_AuthPack;

static const SecAsn1Template KRB5_AuthPackTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_AuthPack) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_AuthPack,pkAuth), 
      KRB5_PKAuthenticatorTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
	SEC_ASN1_EXPLICIT | SEC_ASN1_POINTER | 1,
      offsetof(KRB5_AuthPack,pubKeyInfo), 
      kSecAsn1SubjectPublicKeyInfoTemplate },
    { 0 }
};

/* 
 * Encode AuthPack, public key version (no Diffie-Hellman components).
 */
krb5_error_code pkinit_auth_pack_encode(
    krb5_timestamp      ctime,      
    krb5_ui_4		cusec,		// microseconds
    krb5_ui_4		nonce,
    const krb5_checksum *checksum,
    krb5_data		*auth_pack)      // mallocd and RETURNED
{
    KRB5_AuthPack localAuthPack;
    SecAsn1CoderRef coder;
    KRB5_Checksum *cksum = &localAuthPack.pkAuth.paChecksum;
    krb5_error_code ourRtn = 0;
    CSSM_DATA ber = {0, NULL};
    OSStatus ortn;
    char *timeStr = NULL;
    
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&localAuthPack, 0, sizeof(localAuthPack));
    if(pkiKrbTimestampToStr(ctime, &timeStr)) {
	ourRtn = -1;
	goto errOut;
    }
    localAuthPack.pkAuth.ctime.Data = (uint8 *)timeStr;
    localAuthPack.pkAuth.ctime.Length = strlen(timeStr);
    if(pkiIntToData(cusec, &localAuthPack.pkAuth.cusec, coder)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    if(pkiIntToData(nonce, &localAuthPack.pkAuth.nonce, coder)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    if(pkiIntToData(checksum->checksum_type, &cksum->checksumType, coder)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    cksum->checksum.Data = (uint8 *)checksum->contents;
    cksum->checksum.Length = checksum->length;
    ortn = SecAsn1EncodeItem(coder, &localAuthPack, KRB5_AuthPackTemplate, &ber);
    if(ortn) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&ber, auth_pack)) {
	ourRtn = ENOMEM;
    }
    else {
	auth_pack->magic = KV5M_AUTHENTICATOR;
	ourRtn = 0;
    }
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

/*
 * Decode AuthPack, public key version (no Diffie-Hellman components).
 */
krb5_error_code pkinit_auth_pack_decode(
    const krb5_data	*auth_pack,     // DER encoded
    krb5_timestamp      *ctime,		// RETURNED
    krb5_ui_4		*cusec,		// microseconds, RETURNED
    krb5_ui_4		*nonce,		// RETURNED
    krb5_checksum       *checksum)      // contents mallocd and RETURNED
{
    KRB5_AuthPack localAuthPack;
    SecAsn1CoderRef coder;
    CSSM_DATA der = {0, NULL};
    krb5_error_code ourRtn = 0;
    KRB5_Checksum *cksum = &localAuthPack.pkAuth.paChecksum;
    
    /* Decode --> localAuthPack */
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    PKI_KRB_TO_CSSM_DATA(auth_pack, &der);
    memset(&localAuthPack, 0, sizeof(localAuthPack));
    if(SecAsn1DecodeData(coder, &der, KRB5_AuthPackTemplate, &localAuthPack)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    
    /* optionally Convert KRB5_AuthPack to caller's params */
    if(ctime) {
	if(pkiTimeStrToKrbTimestamp((char *)localAuthPack.pkAuth.ctime.Data,
		localAuthPack.pkAuth.ctime.Length, ctime)) {
	    ourRtn = ASN1_BAD_FORMAT;
	    goto errOut;
	}
    }
    if(cusec) {
	if(pkiDataToInt(&localAuthPack.pkAuth.cusec, (krb5_int32 *)cusec)) {
	    ourRtn = ASN1_BAD_FORMAT;
	    goto errOut;
	}
    }
    if(nonce) {
	if(pkiDataToInt(&localAuthPack.pkAuth.nonce, (krb5_int32 *)nonce)) {
	    ourRtn = ASN1_BAD_FORMAT;
	    goto errOut;
	}
    }
    if(checksum) {
	if(pkiDataToInt(&cksum->checksumType, &checksum->checksum_type)) {
	    ourRtn = ASN1_BAD_FORMAT;
	    goto errOut;
	}
	checksum->contents = (krb5_octet *)malloc(cksum->checksum.Length);
	if(checksum->contents == NULL) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
	checksum->length = cksum->checksum.Length;
	memmove(checksum->contents, cksum->checksum.Data, checksum->length);
	checksum->magic = KV5M_CHECKSUM;
    }
    ourRtn = 0;
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

#pragma mark ----- IssuerAndSerialNumber -----

/*
 * Issuer/serial number - specify issuer as ASN_ANY because we can get it from
 * CL in DER-encoded state.
 */
typedef struct {
    CSSM_DATA		    derIssuer;
    CSSM_DATA		    serialNumber;
} KRB5_IssuerAndSerial;

static const SecAsn1Template KRB5_IssuerAndSerialTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_IssuerAndSerial) },
    { SEC_ASN1_ANY, offsetof(KRB5_IssuerAndSerial, derIssuer) },
    { SEC_ASN1_INTEGER, offsetof(KRB5_IssuerAndSerial, serialNumber) },
    { 0 }
};

/*
 * Given DER-encoded issuer and serial number, create an encoded 
 * IssuerAndSerialNumber.
 */
krb5_error_code pkinit_issuer_serial_encode(
    const krb5_data *issuer,		    // DER encoded
    const krb5_data *serial_num,
    krb5_data       *issuer_and_serial)     // content mallocd and RETURNED
{
    KRB5_IssuerAndSerial issuerSerial;
    SecAsn1CoderRef coder;
    CSSM_DATA ber = {0, NULL};
    OSStatus ortn;

    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    PKI_KRB_TO_CSSM_DATA(issuer, &issuerSerial.derIssuer);
    PKI_KRB_TO_CSSM_DATA(serial_num, &issuerSerial.serialNumber);
    ortn = SecAsn1EncodeItem(coder, &issuerSerial, KRB5_IssuerAndSerialTemplate, &ber);
    if(ortn) {
	ortn = ENOMEM;
	goto errOut;
    }
    ortn = pkiCssmDataToKrb5Data(&ber, issuer_and_serial);
errOut:
    SecAsn1CoderRelease(coder);
    return ortn;
}

/*
 * Decode IssuerAndSerialNumber.
 */
krb5_error_code pkinit_issuer_serial_decode(
    const krb5_data *issuer_and_serial,     // DER encoded
    krb5_data       *issuer,		    // DER encoded, RETURNED
    krb5_data       *serial_num)	    // RETURNED
{
    KRB5_IssuerAndSerial issuerSerial;
    SecAsn1CoderRef coder;
    CSSM_DATA der = {issuer_and_serial->length, (uint8 *)issuer_and_serial->data};
    krb5_error_code ourRtn = 0;
    
    /* Decode --> issuerSerial */
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&issuerSerial, 0, sizeof(issuerSerial));
    if(SecAsn1DecodeData(coder, &der, KRB5_IssuerAndSerialTemplate, &issuerSerial)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    
    /* Convert KRB5_IssuerAndSerial to caller's params */
    if(pkiCssmDataToKrb5Data(&issuerSerial.derIssuer, issuer)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&issuerSerial.serialNumber, serial_num)) {
	ourRtn = ENOMEM;
	goto errOut;
    }

errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

#pragma mark ----- TrustedCA -----

/* Exactly one of these following fields is present */
typedef struct {
    CSSM_DATA		    *caName;		// pre-DER encoded
    CSSM_DATA		    *issuerAndSerial;   // pre-DER encoded
} KRB5_TrustedCA;

static const SecAsn1Template KRB5_TrustedCATemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_TrustedCA) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_TrustedCA, caName), 
      kSecAsn1PointerToAnyTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_TrustedCA, issuerAndSerial), 
      kSecAsn1PointerToIntegerTemplate },
    { 0 }
};
   
/*
 * Encode a TrustedCA.
 * Exactly one of {ca_name, issuer_serial} must be present; ca_name is DER-encoded
 * on entry. 
 */
krb5_error_code pkinit_trusted_ca_encode(
    const krb5_data *ca_name,		
    const krb5_data *issuer_serial,
    krb5_data       *trusted_ca)		// mallocd and RETURNED
{
    KRB5_TrustedCA tca;
    SecAsn1CoderRef coder;
    CSSM_DATA ber = {0, NULL};
    OSStatus ortn;
    CSSM_DATA caName;
    CSSM_DATA issuerAndSerial;

    if((ca_name != NULL) && (issuer_serial != NULL)) {
	return KRB5_CRYPTO_INTERNAL;
    }
    if((ca_name == NULL) && (issuer_serial == NULL)) {
	return KRB5_CRYPTO_INTERNAL;
    }    
    assert(trusted_ca != NULL);

    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&tca, 0, sizeof(tca));
    if(ca_name) {
	PKI_KRB_TO_CSSM_DATA(ca_name, &caName);
	tca.caName = &caName;
    }
    if(issuer_serial) {
	PKI_KRB_TO_CSSM_DATA(issuer_serial, &issuerAndSerial);
	tca.issuerAndSerial = &issuerAndSerial;
    }
    ortn = SecAsn1EncodeItem(coder, &tca, KRB5_TrustedCATemplate, &ber);
    if(ortn) {
	ortn = ENOMEM;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&ber, trusted_ca)) {
	ortn = ENOMEM;
    }
errOut:
    SecAsn1CoderRelease(coder);
    return ortn;
}

/*
 * Decode TrustedCA. A properly encoded TrustedCA will contain exactly one of 
 * of {ca_name, issuer_serial}. The ca_name is returned in DER_encoded form.
 */
krb5_error_code pkinit_trusted_ca_decode(
    const krb5_data *trusted_ca,
    krb5_data *ca_name,			// content optionally mallocd and RETURNED
    krb5_data *issuer_serial)		// ditto
{
    KRB5_TrustedCA tca;
    SecAsn1CoderRef coder;
    CSSM_DATA der = {trusted_ca->length, (uint8 *)trusted_ca->data};
    krb5_error_code ourRtn = 0;
    
    /* Decode --> KRB5_TrustedCA */
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&tca, 0, sizeof(tca));
    if(SecAsn1DecodeData(coder, &der, KRB5_TrustedCATemplate, &tca)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    
    /* Convert KRB5_TrustedCA to caller's params */
    if(tca.caName != NULL) {
	if(pkiCssmDataToKrb5Data(tca.caName, ca_name)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    if(tca.issuerAndSerial != NULL) {
	if(pkiCssmDataToKrb5Data(tca.issuerAndSerial, issuer_serial)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

#pragma mark ----- PA-PK-AS-REQ -----

/*
 * Top-level PA-PK-AS-REQ. All fields are ASN_ANY, pre-encoded before we encode
 * this and still DER-encoded after we decode. 
 */
typedef struct {
    CSSM_DATA		    signedAuthPack;	    // ContentInfo, SignedData  
						    // Content is KRB5_AuthPack
    CSSM_DATA		    **trustedCertifiers;    // optional
    CSSM_DATA		    *kdcCert;		    // optional
    CSSM_DATA		    *encryptionCert;	    // optional
} KRB5_PA_PK_AS_REQ;

static const SecAsn1Template KRB5_PA_PK_AS_REQTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_PA_PK_AS_REQ) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_PA_PK_AS_REQ, signedAuthPack), 
      kSecAsn1AnyTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_PA_PK_AS_REQ, trustedCertifiers), 
      kSecAsn1SequenceOfAnyTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 2,
      offsetof(KRB5_PA_PK_AS_REQ, kdcCert), 
      kSecAsn1PointerToAnyTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 3,
      offsetof(KRB5_PA_PK_AS_REQ, encryptionCert), 
      kSecAsn1PointerToAnyTemplate },
    { 0 }
};

/*
 * Top-level encode for PA-PK-AS-REQ.
 */
krb5_error_code pkinit_pa_pk_as_req_encode(
    const krb5_data *signed_auth_pack,      // DER encoded ContentInfo
    unsigned num_trusted_certifiers,	    // sizeof trused_certifiers
    const krb5_data *trusted_certifiers,    // array of DER-encoded TrustedCAs, optional
    const krb5_data *kdc_cert,		    // DER encoded issuer/serial, optional
    const krb5_data *encryption_cert,       // DER encoded issuer/serial, optional
    krb5_data *pa_pk_as_req)		    // mallocd and RETURNED
{
    KRB5_PA_PK_AS_REQ req;
    SecAsn1CoderRef coder;
    CSSM_DATA ber = {0, NULL};
    OSStatus ortn;
    unsigned dex;
    CSSM_DATA kdcCert;
    CSSM_DATA encryptCert;
    
    assert(signed_auth_pack != NULL);
    assert(pa_pk_as_req != NULL);

    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    
    /* krb5_data ==> CSSM format */
    
    memset(&req, 0, sizeof(req));
    PKI_KRB_TO_CSSM_DATA(signed_auth_pack, &req.signedAuthPack);
    if(num_trusted_certifiers) {
	/* 
	 * Set up a NULL-terminated array of CSSM_DATA pointers. 
	 * We malloc the actual CSSM_DATA as a contiguous array; it's in temp
	 * SecAsn1CoderRef memory. 
	 */
	CSSM_DATA *cas = (CSSM_DATA *)SecAsn1Malloc(coder, 
	    num_trusted_certifiers * sizeof(CSSM_DATA));
	req.trustedCertifiers = 
	    (CSSM_DATA **)pkiNssNullArray(num_trusted_certifiers, coder);
	for(dex=0; dex<num_trusted_certifiers; dex++) {
	    req.trustedCertifiers[dex] = &cas[dex];
	    PKI_KRB_TO_CSSM_DATA(&trusted_certifiers[dex], &cas[dex]);
	}
    }
    if(kdc_cert) {
	PKI_KRB_TO_CSSM_DATA(kdc_cert, &kdcCert);
	req.kdcCert = &kdcCert;
    }
    if(encryption_cert) {
	PKI_KRB_TO_CSSM_DATA(encryption_cert, &encryptCert);
	req.encryptionCert = &encryptCert;
    }
    
    /* encode */
    ortn = SecAsn1EncodeItem(coder, &req, KRB5_PA_PK_AS_REQTemplate, &ber);
    if(ortn) {
	ortn = ENOMEM;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&ber, pa_pk_as_req)) {
	ortn = ENOMEM;
    }
errOut:
    SecAsn1CoderRelease(coder);
    return ortn;
}
    
/*
 * Top-level decode for PA-PK-AS-REQ.
 */
krb5_error_code pkinit_pa_pk_as_req_decode(
    const krb5_data *pa_pk_as_req,
    krb5_data *signed_auth_pack,	    // DER encoded ContentInfo, RETURNED
    /* 
     * Remainder are optionally RETURNED (specify NULL for pointers to 
     * items you're not interested in).
     */
    unsigned *num_trusted_certifiers,       // sizeof trused_certifiers
    krb5_data **trusted_certifiers,	    // mallocd array of DER-encoded TrustedCAs
    krb5_data *kdc_cert,		    // DER encoded issuer/serial
    krb5_data *encryption_cert)		    // DER encoded issuer/serial
{
    KRB5_PA_PK_AS_REQ asReq;
    SecAsn1CoderRef coder;
    CSSM_DATA der;
    krb5_error_code ourRtn = 0;
    
    assert(pa_pk_as_req != NULL);
    
    /* Decode --> KRB5_PA_PK_AS_REQ */
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    PKI_KRB_TO_CSSM_DATA(pa_pk_as_req, &der);
    memset(&asReq, 0, sizeof(asReq));
    if(SecAsn1DecodeData(coder, &der, KRB5_PA_PK_AS_REQTemplate, &asReq)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }

    /* Convert decoded results to caller's args; each is optional */
    if(signed_auth_pack != NULL) {
	if(pkiCssmDataToKrb5Data(&asReq.signedAuthPack, signed_auth_pack)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    if(asReq.trustedCertifiers && (trusted_certifiers != NULL)) {
	/* NULL-terminated array of CSSM_DATA ptrs */
	unsigned numCas = pkiNssArraySize((const void **)asReq.trustedCertifiers);
	unsigned dex;
	krb5_data *kdcCas;
	
	kdcCas = (krb5_data *)malloc(sizeof(krb5_data) * numCas);
	if(kdcCas == NULL) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
	for(dex=0; dex<numCas; dex++) {
	    pkiCssmDataToKrb5Data(asReq.trustedCertifiers[dex], &kdcCas[dex]);
	}
	*trusted_certifiers = kdcCas;
	*num_trusted_certifiers = numCas;
    }
    if(asReq.kdcCert && kdc_cert) {
	if(pkiCssmDataToKrb5Data(asReq.kdcCert, kdc_cert)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    if(asReq.encryptionCert && encryption_cert) {
	if(pkiCssmDataToKrb5Data(asReq.encryptionCert, encryption_cert)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    ourRtn = 0;
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;   
}

#pragma mark ----- InitialVerifiedCAs -----

/* 
 * This goes the issued ticket's AuthorizationData.ad-data[1]
 */
typedef struct {
    NSS_Name		    ca;
    CSSM_DATA		    validated;		// BOOLEAN
} KRB5_InitialVerifiedCA;

typedef struct {
    KRB5_InitialVerifiedCA  **verifiedCas;
} KRB5_InitialVerifiedCAs;

#pragma mark ====== begin PA-PK-AS-REP components ======

typedef struct {
    CSSM_DATA       subjectPublicKey;       // BIT STRING
    CSSM_DATA       nonce;		    // from KRB5_PKAuthenticator.nonce
    CSSM_DATA       *expiration;	    // optional UTC time
} KRB5_KDC_DHKeyInfo;

typedef struct {
    CSSM_DATA		keyType;
    CSSM_DATA		keyValue;
} KRB5_EncryptionKey;

static const SecAsn1Template KRB5_EncryptionKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_EncryptionKey) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_EncryptionKey, keyType), 
      kSecAsn1IntegerTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_EncryptionKey, keyValue), 
      kSecAsn1OctetStringTemplate },
    { 0 }
};

typedef struct {
    KRB5_EncryptionKey  encryptionKey;
    CSSM_DATA		nonce;		    // from KRB5_PKAuthenticator.nonce
} KRB5_ReplyKeyPack;

static const SecAsn1Template KRB5_ReplyKeyPackTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_ReplyKeyPack) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_ReplyKeyPack, encryptionKey), 
      KRB5_EncryptionKeyTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_ReplyKeyPack, nonce), 
      kSecAsn1IntegerTemplate },
    { 0 }
};

/* 
 * Encode a ReplyKeyPack. The result is used as the Content of a SignedData.
 */
krb5_error_code pkinit_reply_key_pack_encode(
    const krb5_keyblock *key_block,
    krb5_ui_4		nonce,
    krb5_data		*reply_key_pack)      // mallocd and RETURNED
{
    KRB5_ReplyKeyPack repKeyPack;
    SecAsn1CoderRef coder;
    krb5_error_code ourRtn = 0;
    CSSM_DATA der = {0, NULL};
    OSStatus ortn;
    KRB5_EncryptionKey *encryptKey = &repKeyPack.encryptionKey;
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&repKeyPack, 0, sizeof(repKeyPack));
    
    if(pkiIntToData(key_block->enctype, &encryptKey->keyType, coder)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    encryptKey->keyValue.Length = key_block->length,
    encryptKey->keyValue.Data = (uint8 *)key_block->contents;
    if(pkiIntToData(nonce, &repKeyPack.nonce, coder)) {
	ourRtn = ENOMEM;
	goto errOut;
    }

    ortn = SecAsn1EncodeItem(coder, &repKeyPack, KRB5_ReplyKeyPackTemplate, &der);
    if(ortn) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&der, reply_key_pack)) {
	ourRtn = ENOMEM;
    }
    else {
	ourRtn = 0;
    }
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

/* 
 * Decode a ReplyKeyPack.
 */
krb5_error_code pkinit_reply_key_pack_decode(
    const krb5_data	*reply_key_pack,
    krb5_keyblock       *key_block,     // RETURNED
    krb5_ui_4		*nonce)		// RETURNED
{
    KRB5_ReplyKeyPack repKeyPack;
    SecAsn1CoderRef coder;
    krb5_error_code ourRtn = 0;
    KRB5_EncryptionKey *encryptKey = &repKeyPack.encryptionKey;
    CSSM_DATA der = {reply_key_pack->length, (uint8 *)reply_key_pack->data};
    krb5_data tmpData;
    
    /* Decode --> KRB5_ReplyKeyPack */
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&repKeyPack, 0, sizeof(repKeyPack));
    if(SecAsn1DecodeData(coder, &der, KRB5_ReplyKeyPackTemplate, &repKeyPack)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    
    if(pkiDataToInt(&encryptKey->keyType, (krb5_int32 *)&key_block->enctype)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&encryptKey->keyValue, &tmpData)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    key_block->contents = tmpData.data;
    key_block->length = tmpData.length;
    
    if(pkiDataToInt(&repKeyPack.nonce, (krb5_int32 *)nonce)) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    ourRtn = 0;
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}


#pragma mark ----- KRB5_PA_PK_AS_REP -----
/*
 * Top-level PA-PK-AS-REP. Exactly one of the optional fields must be present.
 */
typedef struct {
    CSSM_DATA	*dhSignedData;      // ContentInfo, SignedData
				    // Content is KRB5_KDC_DHKeyInfo
    CSSM_DATA	*encKeyPack;	    // ContentInfo, SignedData
				    // Content is ReplyKeyPack
} KRB5_PA_PK_AS_REP;
    
static const SecAsn1Template KRB5_PA_PK_AS_REPTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(KRB5_PA_PK_AS_REP) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 0,
      offsetof(KRB5_PA_PK_AS_REP, dhSignedData), 
      kSecAsn1PointerToAnyTemplate },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_EXPLICIT | 1,
      offsetof(KRB5_PA_PK_AS_REP, encKeyPack), 
      kSecAsn1PointerToAnyTemplate },
    { 0 }
};

/* 
 * Encode a KRB5_PA_PK_AS_REP.
 */
krb5_error_code pkinit_pa_pk_as_rep_encode(
    const krb5_data *dh_signed_data, 
    const krb5_data *enc_key_pack, 
    krb5_data       *pa_pk_as_rep)      // mallocd and RETURNED
{
    KRB5_PA_PK_AS_REP asRep;
    SecAsn1CoderRef coder;
    krb5_error_code ourRtn = 0;
    CSSM_DATA der = {0, NULL};
    OSStatus ortn;
    CSSM_DATA   dhSignedData;
    CSSM_DATA   encKeyPack;
    
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&asRep, 0, sizeof(asRep));
    if(dh_signed_data) {
	PKI_KRB_TO_CSSM_DATA(dh_signed_data, &dhSignedData);
	asRep.dhSignedData = &dhSignedData;
    }
    if(enc_key_pack) {
	PKI_KRB_TO_CSSM_DATA(enc_key_pack, &encKeyPack);
	asRep.encKeyPack = &encKeyPack;
    }

    ortn = SecAsn1EncodeItem(coder, &asRep, KRB5_PA_PK_AS_REPTemplate, &der);
    if(ortn) {
	ourRtn = ENOMEM;
	goto errOut;
    }
    if(pkiCssmDataToKrb5Data(&der, pa_pk_as_rep)) {
	ourRtn = ENOMEM;
    }
    else {
	ourRtn = 0;
    }
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

/* 
 * Decode a KRB5_PA_PK_AS_REP.
 */
krb5_error_code pkinit_pa_pk_as_rep_decode(
    const krb5_data *pa_pk_as_rep,
    krb5_data *dh_signed_data, 
    krb5_data *enc_key_pack)
{
    KRB5_PA_PK_AS_REP asRep;
    SecAsn1CoderRef coder;
    CSSM_DATA der = {pa_pk_as_rep->length, (uint8 *)pa_pk_as_rep->data};
    krb5_error_code ourRtn = 0;
    
    /* Decode --> KRB5_PA_PK_AS_REP */
    if(SecAsn1CoderCreate(&coder)) {
	return ENOMEM;
    }
    memset(&asRep, 0, sizeof(asRep));
    if(SecAsn1DecodeData(coder, &der, KRB5_PA_PK_AS_REPTemplate, &asRep)) {
	ourRtn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    
    if(asRep.dhSignedData) {
	if(pkiCssmDataToKrb5Data(asRep.dhSignedData, dh_signed_data)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    if(asRep.encKeyPack) {
	if(pkiCssmDataToKrb5Data(asRep.encKeyPack, enc_key_pack)) {
	    ourRtn = ENOMEM;
	    goto errOut;
	}
    }
    
errOut:
    SecAsn1CoderRelease(coder);
    return ourRtn;
}

