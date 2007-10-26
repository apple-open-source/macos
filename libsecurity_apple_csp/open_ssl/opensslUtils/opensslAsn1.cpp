/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * opensslAsn1.h - ANS1 encode/decode of openssl object, libssnasn1 version
 */
#include "opensslAsn1.h"
#include "BinaryKey.h"
#include "AppleCSPUtils.h"
#include "opensshCoding.h"
#include <Security/osKeyTemplates.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>

#include <security_asn1/SecNssCoder.h>
#include <security_asn1/secerr.h>
#include <Security/keyTemplates.h>
#include <security_utilities/debugging.h>
#include <Security/oidsalg.h>
#include <Security/SecAsn1Templates.h>

#include <assert.h>

#define sslAsn1Debug(args...)	secdebug("sslAsn1", ##args)

#ifndef	NDEBUG
/* set to 1 to see all ASN related errors */
#define LOG_ASN_ERRORS  0
#else
#define LOG_ASN_ERRORS  0
#endif

#if		LOG_ASN_ERRORS
#include <stdio.h>
#include <security_asn1/secerr.h>

static void logAsnErr(
	const char *op,
	PRErrorCode perr)
{
	printf("Error on %s: %s\n", op, SECErrorString(perr));
}
#else
#define logAsnErr(op, perr)
#endif  /* LOG_ASN_ERRORS */

/* CSSM_DATA --> BIGNUM */
BIGNUM *cssmDataToBn(
	const CSSM_DATA &cdata)
{
	BIGNUM *bn = BN_new();
	BIGNUM *rtn;

	rtn = BN_bin2bn(cdata.Data, cdata.Length, bn);
	if(rtn == NULL) {
		BN_free(bn);
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	return bn;
}

/* BIGNUM --> CSSM_DATA, mallocing from a SecNssCoder's PL_ArenaPool */
void bnToCssmData(
	const BIGNUM *bn,
	CSSM_DATA &cdata,
	SecNssCoder &coder)
{
	assert(bn != NULL);
	unsigned numBytes = BN_num_bytes(bn);
	cdata.Data = (uint8 *)coder.malloc(numBytes);
	if(cdata.Data == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	cdata.Length = numBytes;
	BN_bn2bin(bn, cdata.Data);
}

/*
 * CSSM_DATA --> unsigned int
 */
unsigned cssmDataToInt(
	const CSSM_DATA &cdata)
{
	if((cdata.Length == 0) || (cdata.Data == NULL)) {
		return 0;
	}
	unsigned len = (unsigned)cdata.Length;
	if(len > sizeof(int)) {
		logAsnErr("cssmDataToInt: Length error (%u)", len);
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
	}
	
	unsigned rtn = 0;
	uint8 *cp = cdata.Data;
	for(unsigned i=0; i<len; i++) {
		rtn = (rtn << 8) | *cp++;
	}
	return rtn;
}

/*
 * unsigned int --> CSSM_DATA, mallocing from an SecNssCoder 
 */
void intToCssmData(
	unsigned num,
	CSSM_DATA &cdata,
	SecNssCoder &coder)
{
	unsigned len = 0;
	
	if(num < 0x100) {
		len = 1;
	}
	else if(num < 0x10000) {
		len = 2;
	}
	else if(num < 0x1000000) {
		len = 3;
	}
	else {
		len = 4;
	}
	cdata.Data = (uint8 *)coder.malloc(len);
	cdata.Length = len;
	uint8 *cp = &cdata.Data[len - 1];
	for(unsigned i=0; i<len; i++) {
		*cp-- = num & 0xff;
		num >>= 8;
	}
}

/* 
 * Set up a encoded NULL for AlgorithmIdentifier.parameters, 
 * required for RSA 
 */
static void nullAlgParams(
	CSSM_X509_ALGORITHM_IDENTIFIER	&algId)
{
	static const uint8 encNull[2] = { SEC_ASN1_NULL, 0 };
	CSSM_DATA encNullData;
	encNullData.Data = (uint8 *)encNull;
	encNullData.Length = 2;

	algId.parameters = encNullData;
}

#pragma mark -
#pragma mark *** RSA key encode/decode ***

/*
 * DER encode/decode RSA keys in various formats. 
 *
 * Public key, CSSM_KEYBLOB_RAW_FORMAT_PKCS1 
 *   -- compatible with BSAFE
 *   -- used for CSSM_KEYBLOB_RAW_FORMAT_DIGEST on both keys
 */
static CSSM_RETURN RSAPublicKeyDecodePKCS1(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	NSS_RSAPublicKeyPKCS1 nssPubKey;
	
	memset(&nssPubKey, 0, sizeof(nssPubKey));
	PRErrorCode perr = coder.decode(p, length, 
		kSecAsn1RSAPublicKeyPKCS1Template, &nssPubKey);
	if(perr) {
		logAsnErr("decode(RSAPublicKeyPKCS1)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	try {
		openKey->n = cssmDataToBn(nssPubKey.modulus);
		openKey->e = cssmDataToBn(nssPubKey.publicExponent);
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

static CSSM_RETURN RSAPublicKeyEncodePKCS1(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	/* convert to NSS_RSAPublicKeyPKCS1 */
	NSS_RSAPublicKeyPKCS1 nssPubKey;
	
	try {
		bnToCssmData(openKey->n, nssPubKey.modulus, coder);
		bnToCssmData(openKey->e, nssPubKey.publicExponent, coder);
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	PRErrorCode prtn;
	prtn = SecNssEncodeItemOdata(&nssPubKey, 
		kSecAsn1RSAPublicKeyPKCS1Template, encodedKey);
	if(prtn) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

/* 
 * SubjectPublicKeyInfo, as used by openssl. 
 * The subjectPublicKey component is a PKCS1-style RSAPublicKey. 
 */
static CSSM_RETURN RSAPublicKeyDecodeX509(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	void 			*p, 
	CSSM_SIZE		length,
	/* mallocd/returned encoded alg params for OAEP key */
	uint8			**algParams,
	CSSM_SIZE		*algParamLen)
{
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO nssPubKeyInfo;
	PRErrorCode perr;
	
	memset(&nssPubKeyInfo, 0, sizeof(nssPubKeyInfo));
	perr = coder.decode(p, length, kSecAsn1SubjectPublicKeyInfoTemplate, 
		&nssPubKeyInfo);
	if(perr) {
		logAsnErr("decode(RSA SubjectPublicKeyInfo)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	/* verify alg identifier */
	const CSSM_OID *oid = &nssPubKeyInfo.algorithm.algorithm;
	if(!cspCompareCssmData(oid, &CSSMOID_RSA)) {
		if(!cspCompareCssmData(oid, &CSSMOID_RSAWithOAEP)) {
			sslAsn1Debug("RSAPublicKeyDecodeX509: bad OID");
			return CSSMERR_CSP_INVALID_KEY;
		}
		if(nssPubKeyInfo.algorithm.parameters.Data != NULL) {
			uint32 len = nssPubKeyInfo.algorithm.parameters.Length;
			*algParams = (uint8 *)malloc(len);
			memmove(*algParams, nssPubKeyInfo.algorithm.parameters.Data, len);
			*algParamLen = len;
		}
	}
	
	/* decode the raw bits */
	CSSM_DATA *pubKey = &nssPubKeyInfo.subjectPublicKey;
	/* decoded length was in bits */
	pubKey->Length = (pubKey->Length + 7) / 8;	
	return RSAPublicKeyDecodePKCS1(coder, openKey, pubKey->Data, 
		pubKey->Length);
}

static CSSM_RETURN RSAPublicKeyEncodeX509(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey,
	/* encoded alg params for OAEP key */
	uint8			*algParams,
	uint32			algParamsLen)
{
	CssmAutoData aData(Allocator::standard());
	CSSM_RETURN crtn;
	
	/* First get an encoded PKCS1-style RSAPublicKey */
	crtn = RSAPublicKeyEncodePKCS1(coder, openKey, aData);
	if(crtn) {
		return crtn;
	}
	
	/* 
	 * That's the AsnBits subjectPublicKey component of a
	 * SubjectPublicKeyInfo 
	 */
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO nssPubKeyInfo;
	memset(&nssPubKeyInfo, 0, sizeof(nssPubKeyInfo));
	nssPubKeyInfo.subjectPublicKey.Data = (uint8 *)aData.data();
	nssPubKeyInfo.subjectPublicKey.Length = aData.length() * 8;
	
	CSSM_X509_ALGORITHM_IDENTIFIER &algId = nssPubKeyInfo.algorithm;
	algId.algorithm = CSSMOID_RSA;

	if(algParams) {
		algId.parameters.Data = (uint8 *)algParams;
		algId.parameters.Length = algParamsLen;
	}
	else {
		/* NULL algorithm parameters */
		nullAlgParams(algId);
	}
	
	/* DER encode */
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&nssPubKeyInfo, 
		kSecAsn1SubjectPublicKeyInfoTemplate, encodedKey);

	if(perr) {
		logAsnErr("encode(RSA SubjectPublicKeyInfo)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

/* 
 * RSA private key, PKCS1 format, used by openssl.
 */
static CSSM_RETURN RSAPrivateKeyDecodePKCS1(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	NSS_RSAPrivateKeyPKCS1 nssPrivKey;
	PRErrorCode perr;
	
	memset(&nssPrivKey, 0, sizeof(nssPrivKey));
	perr = coder.decode(p, length, kSecAsn1RSAPrivateKeyPKCS1Template, &nssPrivKey);
	if(perr) {
		logAsnErr("decode(RSAPrivateKeyPKCS)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	/* convert nssPrivKey fields to RSA key fields */
	try {
		openKey->version = cssmDataToInt(nssPrivKey.version);
		openKey->n	  = cssmDataToBn(nssPrivKey.modulus);
		openKey->e	  = cssmDataToBn(nssPrivKey.publicExponent);
		openKey->d 	  = cssmDataToBn(nssPrivKey.privateExponent);
		openKey->p 	  = cssmDataToBn(nssPrivKey.prime1);
		openKey->q 	  = cssmDataToBn(nssPrivKey.prime2);
		openKey->dmp1 = cssmDataToBn(nssPrivKey.exponent1);
		openKey->dmq1 = cssmDataToBn(nssPrivKey.exponent2);
		openKey->iqmp = cssmDataToBn(nssPrivKey.coefficient);
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

static CSSM_RETURN RSAPrivateKeyEncodePKCS1(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	NSS_RSAPrivateKeyPKCS1 nssPrivKey;
	PRErrorCode perr;
	
	/* convert to NSS_RSAPrivateKeyPKCS1 */
	try {
		intToCssmData(openKey->version, nssPrivKey.version, coder);
		bnToCssmData(openKey->n, 	nssPrivKey.modulus, coder);
		bnToCssmData(openKey->e, 	nssPrivKey.publicExponent, coder);
		bnToCssmData(openKey->d, 	nssPrivKey.privateExponent, coder);
		bnToCssmData(openKey->p, 	nssPrivKey.prime1, coder);
		bnToCssmData(openKey->q, 	nssPrivKey.prime2, coder);
		bnToCssmData(openKey->dmp1, nssPrivKey.exponent1, coder);
		bnToCssmData(openKey->dmq1, nssPrivKey.exponent2, coder);
		bnToCssmData(openKey->iqmp, nssPrivKey.coefficient, coder);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* DER encode */
	perr = SecNssEncodeItemOdata(&nssPrivKey, kSecAsn1RSAPrivateKeyPKCS1Template,
		encodedKey);
	if(perr) {
		logAsnErr("encode(RSAPrivateKeyPKCS1)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

/* 
 * RSA private key, PKCS8, compatible with BSAFE.
 */
static CSSM_RETURN RSAPrivateKeyDecodePKCS8(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	void 			*p, 
	CSSM_SIZE		length,
	/* mallocd/returned encoded alg params for OAEP key */
	uint8			**algParams,
	CSSM_SIZE		*algParamLen)
{
	NSS_PrivateKeyInfo nssPrivKeyInfo;
	PRErrorCode perr;
	
	memset(&nssPrivKeyInfo, 0, sizeof(nssPrivKeyInfo));
	perr = coder.decode(p, length, kSecAsn1PrivateKeyInfoTemplate, &nssPrivKeyInfo);
	if(perr) {
		logAsnErr("decode(PrivateKeyInfo)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* verify alg identifier */
	const CSSM_OID *oid = &nssPrivKeyInfo.algorithm.algorithm;
	if(!cspCompareCssmData(oid, &CSSMOID_RSA)) {
		if(!cspCompareCssmData(oid, &CSSMOID_RSAWithOAEP)) {
			sslAsn1Debug("RSAPrivateKeyDecodePKCS8: bad OID");
			return CSSMERR_CSP_INVALID_KEY;
		}
		if(nssPrivKeyInfo.algorithm.parameters.Data != NULL) {
			uint32 len = nssPrivKeyInfo.algorithm.parameters.Length;
			*algParams = (uint8 *)malloc(len);
			memmove(*algParams, nssPrivKeyInfo.algorithm.parameters.Data, len);
			*algParamLen = len;
		}
	}
	
	/* 
	 * nssPrivKeyInfo.privateKey is an octet string which needs 
	 * subsequent decoding 
	 */
	CSSM_DATA *privKey = &nssPrivKeyInfo.privateKey;
	return RSAPrivateKeyDecodePKCS1(coder, openKey, 
		privKey->Data, privKey->Length);
}

static CSSM_RETURN RSAPrivateKeyEncodePKCS8(
	SecNssCoder 	&coder,
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey,
	/* encoded alg params for OAEP key */
	uint8			*algParams,
	uint32			algParamsLen)
{

	/* First get PKCS1-style encoding */
	CssmAutoData aData(Allocator::standard());
	CSSM_RETURN crtn = RSAPrivateKeyEncodePKCS1(coder, openKey, aData);
	if(crtn) {
		return crtn;
	}

	/* that encoding is the privateKey field of a NSS_PrivateKeyInfo */
	NSS_PrivateKeyInfo nssPrivKeyInfo;
	memset(&nssPrivKeyInfo, 0, sizeof(nssPrivKeyInfo));
	nssPrivKeyInfo.privateKey.Data = (uint8 *)aData.data();
	nssPrivKeyInfo.privateKey.Length = aData.length();
	
	CSSM_X509_ALGORITHM_IDENTIFIER &algId = nssPrivKeyInfo.algorithm;
	algId.algorithm = CSSMOID_RSA;

	if(algParams) {
		algId.parameters.Data = (uint8 *)algParams;
		algId.parameters.Length = algParamsLen;
	}
	else {
		/* NULL algorithm parameters */
		nullAlgParams(algId);
	}

	/* FIXME : attributes? */
	
	uint8 vers = 0;
	nssPrivKeyInfo.version.Data = &vers;
	nssPrivKeyInfo.version.Length = 1;
	
	/* DER encode */
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&nssPrivKeyInfo, 
		kSecAsn1PrivateKeyInfoTemplate, encodedKey);

	if(perr) {
		logAsnErr("encode(RSA PrivateKeyInfo)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

CSSM_RETURN RSAPublicKeyDecode(
	RSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	void 				*p, 
	size_t				length)
{
	SecNssCoder coder;
	
	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
			return RSAPublicKeyDecodePKCS1(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_X509:
			return RSAPublicKeyDecodeX509(coder, openKey, p, length, NULL, NULL);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
			return RSAPublicKeyDecodeOpenSSH1(openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
			return RSAPublicKeyDecodeOpenSSH2(openKey, p, length);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	RSAPublicKeyEncode(
	RSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	const CssmData		&descData,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
			return RSAPublicKeyEncodePKCS1(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_X509:
			return RSAPublicKeyEncodeX509(coder, openKey, encodedKey, NULL, 0);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
			return RSAPublicKeyEncodeOpenSSH1(openKey, descData, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
			return RSAPublicKeyEncodeOpenSSH2(openKey, descData, encodedKey);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN RSAPrivateKeyDecode(
	RSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	void 				*p, 
	size_t				length)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
			return RSAPrivateKeyDecodePKCS1(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
			return RSAPrivateKeyDecodePKCS8(coder, openKey, p, length, NULL, NULL);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
			return RSAPrivateKeyDecodeOpenSSH1(openKey, p, length);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	RSAPrivateKeyEncode(
	RSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	const CssmData		&descData,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
			return RSAPrivateKeyEncodePKCS1(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
			return RSAPrivateKeyEncodePKCS8(coder, openKey, encodedKey, NULL, 0);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
			return RSAPrivateKeyEncodeOpenSSH1(openKey, descData, encodedKey);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	RSAOAEPPrivateKeyEncode(
	RSA 				*openKey, 
	const CSSM_DATA		*label,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;
	CSSM_DATA encodedParams = {0, NULL};
	/* TBD encode the label into a RSAES-OAEP-params */

	return RSAPrivateKeyEncodePKCS8(coder, openKey, encodedKey, encodedParams.Data, encodedParams.Length);
}

CSSM_RETURN	RSAOAEPPublicKeyEncode(
	RSA 				*openKey, 
	const CSSM_DATA		*label,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;
	CSSM_DATA encodedParams = {0, NULL};
	/* TBD encode the label into a RSAES-OAEP-params */

	return RSAPublicKeyEncodeX509(coder, openKey, encodedKey, encodedParams.Data, encodedParams.Length);
}

CSSM_RETURN RSAOAEPPublicKeyDecode(
	RSA 				*openKey, 
	void 				*p, 
	size_t				length,
	/* mallocd and returned label */
	CSSM_DATA			*label)
{
	SecNssCoder coder;
	CSSM_RETURN crtn;
	CSSM_DATA encodedParams = {0, NULL};
	
	crtn = RSAPublicKeyDecodeX509(coder, openKey, p, length, &encodedParams.Data, 
		&encodedParams.Length);
	if(crtn) {
		return crtn;
	}
	
	/* TBD - decode label from encoded alg params */
	label->Data = NULL;
	label->Length = 0;
	return CSSM_OK;
}

CSSM_RETURN RSAOAEPPrivateKeyDecode(
	RSA 				*openKey, 
	void 				*p, 
	size_t				length,
	/* mallocd and returned label */
	CSSM_DATA			*label)
{
	SecNssCoder coder;
	CSSM_RETURN crtn;
	CSSM_DATA encodedParams = {0, NULL};
	
	crtn = RSAPrivateKeyDecodePKCS8(coder, openKey, p, length, &encodedParams.Data, 
		&encodedParams.Length);
	if(crtn) {
		return crtn;
	}
	
	/* TBD - decode label from encoded alg params */
	label->Data = NULL;
	label->Length = 0;
	return CSSM_OK;
}

#pragma mark -
#pragma mark *** DSA key encode/decode ***

/***
 *** DSA
 ***/

/* NSS_DSAAlgorithmIdBSAFE <--> DSA->{p,g,q} */
static void dsaToNssAlgIdBSAFE(
	const DSA *openKey,
	NSS_DSAAlgorithmIdBSAFE &algId,
	SecNssCoder &coder)
{
	/* non-standard, BSAFE-specific OID */
	algId.algorithm = CSSMOID_DSA;	// not mallocd
	unsigned numBits = BN_num_bits(openKey->p);
	intToCssmData(numBits, algId.params.keySizeInBits, coder);
	bnToCssmData(openKey->p, algId.params.p, coder);
	bnToCssmData(openKey->q, algId.params.q, coder);
	bnToCssmData(openKey->g, algId.params.g, coder);
}

static CSSM_RETURN nssAlgIdToDsaBSAFE(
	NSS_DSAAlgorithmIdBSAFE &algId,
	DSA *openKey)
{
	/* non-standard, BSAFE-specific OID */
	if(!cspCompareCssmData(&algId.algorithm, &CSSMOID_DSA)) {
		sslAsn1Debug("nssAlgIdToDsaBSAFE: bad OID");
		return CSSMERR_CSP_INVALID_KEY;
	}
	openKey->p = cssmDataToBn(algId.params.p);
	openKey->q = cssmDataToBn(algId.params.q);
	openKey->g = cssmDataToBn(algId.params.g);
	return CSSM_OK;
}

/* NSS_DSAAlgorithmIdX509 <--> DSA->{p,g,q} */
static void dsaToNssAlgIdX509(
	const DSA *openKey,
	NSS_DSAAlgorithmIdX509 &algId,
	SecNssCoder &coder)
{
	algId.algorithm = CSSMOID_DSA_CMS;	// not mallocd
	bnToCssmData(openKey->p, algId.params->p, coder);
	bnToCssmData(openKey->q, algId.params->q, coder);
	bnToCssmData(openKey->g, algId.params->g, coder);
}

static CSSM_RETURN nssAlgIdToDsaX509(
	NSS_DSAAlgorithmIdX509 &algId,
	DSA *openKey)
{
	if(!cspCompareCssmData(&algId.algorithm, &CSSMOID_DSA_CMS) &&
	   !cspCompareCssmData(&algId.algorithm, &CSSMOID_DSA_JDK)) {
		sslAsn1Debug("nssAlgIdToDsaX509: bad OID");
		return CSSMERR_CSP_INVALID_KEY;
	}
	/* these might be absent per CMS */
	if(algId.params == NULL) {
		return CSSM_OK;
	}
	openKey->p = cssmDataToBn(algId.params->p);
	openKey->q = cssmDataToBn(algId.params->q);
	openKey->g = cssmDataToBn(algId.params->g);
	return CSSM_OK;
}

/* 
 * DSA public keys, FIPS186 format.
 * Compatible with BSAFE.
 */
CSSM_RETURN DSAPublicKeyDecodeFIPS186(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	void 			*p, 
	unsigned		length)
{
	NSS_DSAPublicKeyBSAFE nssPubKey;
	PRErrorCode perr;
	CSSM_RETURN crtn;
	
	memset(&nssPubKey, 0, sizeof(nssPubKey));
	perr = coder.decode(p, length, kSecAsn1DSAPublicKeyBSAFETemplate, 
		&nssPubKey);
	if(perr) {
		logAsnErr("decode(DSAPublicKeyBSAFE)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* BSAFE style DSA-specific alg params */
	NSS_DSAAlgorithmIdBSAFE &algId = nssPubKey.dsaAlg;
	crtn = nssAlgIdToDsaBSAFE(algId, openKey);
	if(crtn) {
		return crtn;
	}
	
	/* inside of nssPubKey.publicKey is the DER-encoding of a 
	 * ASN Integer; decoded length was in bits */
	nssPubKey.publicKey.Length = (nssPubKey.publicKey.Length + 7) / 8;	
	CSSM_DATA pubKeyBytes;
	perr = coder.decodeItem(nssPubKey.publicKey, 
		kSecAsn1UnsignedIntegerTemplate, 
		&pubKeyBytes);
	if(perr) {
		logAsnErr("decode(NSS_DSAPublicKeyBSAFE.publicKey)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	openKey->pub_key = cssmDataToBn(pubKeyBytes);

	if(openKey->pub_key == NULL) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	return 0;
}

CSSM_RETURN	DSAPublicKeyEncodeFIPS186(
	SecNssCoder		&coder,
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* convert to NSS_DSAPublicKeyBSAFE */
		NSS_DSAPublicKeyBSAFE nssPubKey;
		memset(&nssPubKey, 0, sizeof(nssPubKey));
		dsaToNssAlgIdBSAFE(openKey, nssPubKey.dsaAlg, coder);
		
		/* 
		 * publicKey is the DER-encoding of a ASN INTEGER wrapped in 
		 * an AsnBits
		 */
		CSSM_DATA pubKeyRaw;
		PRErrorCode perr;
		bnToCssmData(openKey->pub_key, pubKeyRaw, coder);
		perr = coder.encodeItem(&pubKeyRaw,	kSecAsn1UnsignedIntegerTemplate, 
			nssPubKey.publicKey);
		if(perr) {
			logAsnErr("encodeItem(DSAPublicKeyBSAFE.publicKey)", perr);
			return CSSMERR_CSP_MEMORY_ERROR;
		}
		nssPubKey.publicKey.Length *= 8;

		/* DER encode */
		SecNssEncodeItemOdata(&nssPubKey, kSecAsn1DSAPublicKeyBSAFETemplate, 
			encodedKey);
		return CSSM_OK;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

/* 
 * DSA private keys, FIPS186 format.
 * Compatible with BSAFE.
 */
CSSM_RETURN DSAPrivateKeyDecodeFIPS186(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	void 			*p, 
	unsigned		length)
{
	NSS_DSAPrivateKeyBSAFE nssPrivKeyInfo;
	PRErrorCode perr;
	
	memset(&nssPrivKeyInfo, 0, sizeof(nssPrivKeyInfo));
	perr = coder.decode(p, length, kSecAsn1DSAPrivateKeyBSAFETemplate, 
		&nssPrivKeyInfo);
	if(perr) {
		logAsnErr("decode(DSA PrivateKeyInfo)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	CSSM_RETURN crtn = nssAlgIdToDsaBSAFE(nssPrivKeyInfo.dsaAlg, openKey);
	if(crtn) {
		return crtn;
	}

	/* nssPrivKeyInfo.privateKey is the DER-encoding of a 
	 * DSAPrivateKeyOcts... */
	 try {
		PRErrorCode perr;
		NSS_DSAPrivateKeyOcts keyOcts;
		
		perr = coder.decodeItem(nssPrivKeyInfo.privateKey, 
			kSecAsn1DSAPrivateKeyOctsTemplate, &keyOcts);
		if(perr) {
			logAsnErr("decode(DSA PrivateKeyInfoOcts)", perr);
			return CSSMERR_CSP_INVALID_KEY;
		}
	
		openKey->priv_key = cssmDataToBn(keyOcts.privateKey);
		if(openKey->priv_key == NULL) {
			return CSSMERR_CSP_INVALID_KEY;
		}
		return 0;
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
}

CSSM_RETURN	DSAPrivateKeyEncodeFIPS186(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* First convert into a NSS_DSAPrivateKeyBSAFE */
		NSS_DSAPrivateKeyBSAFE nssPrivKey;
		intToCssmData(openKey->version, nssPrivKey.version, coder);
		dsaToNssAlgIdBSAFE(openKey, nssPrivKey.dsaAlg, coder);
		
		/* nssPrivKey.privateKey is the DER-encoding of one of these... */
		NSS_DSAPrivateKeyOcts privKeyOcts;
		bnToCssmData(openKey->priv_key, privKeyOcts.privateKey, coder);

		/* DER encode the privateKey portion into arena pool memory
		 * into NSS_DSAPrivateKeyPKCS8.privateKey */
		coder.encodeItem(&privKeyOcts, kSecAsn1DSAPrivateKeyOctsTemplate,
			nssPrivKey.privateKey);

		/* DER encode the whole thing */
		PRErrorCode perr;
		perr = SecNssEncodeItemOdata(&nssPrivKey,
			kSecAsn1DSAPrivateKeyBSAFETemplate, encodedKey);
		return 0;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

/* 
 * DSA private keys, PKCS8/SMIME format.
 */
CSSM_RETURN DSAPrivateKeyDecodePKCS8(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	void 			*p, 
	unsigned		length)
{
	NSS_DSAPrivateKeyPKCS8 nssPrivKeyInfo;
	PRErrorCode perr;
	
	memset(&nssPrivKeyInfo, 0, sizeof(nssPrivKeyInfo));
	perr = coder.decode(p, length, kSecAsn1DSAPrivateKeyPKCS8Template, 
		&nssPrivKeyInfo);
	if(perr) {
		logAsnErr("decode(DSA NSS_DSAPrivateKeyPKCS8)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	CSSM_RETURN crtn = nssAlgIdToDsaX509(nssPrivKeyInfo.dsaAlg, openKey);
	if(crtn) {
		return crtn;
	}

	/* 
	 * Post-decode, nssPrivKeyInfo.privateKey is the DER-encoding of a 
	 * an ASN integer.  
	 */
	 try {
		PRErrorCode perr;
		CSSM_DATA privKeyInt = {0, NULL};
		
		perr = coder.decodeItem(nssPrivKeyInfo.privateKey, 
			kSecAsn1UnsignedIntegerTemplate, &privKeyInt);
		if(perr) {
			logAsnErr("decode(DSA nssPrivKeyInfo.privateKey)", perr);
			return CSSMERR_CSP_INVALID_KEY;
		}
	
		openKey->priv_key = cssmDataToBn(privKeyInt);
		if(openKey->priv_key == NULL) {
			return CSSMERR_CSP_INVALID_KEY;
		}
		return 0;
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
}

CSSM_RETURN	DSAPrivateKeyEncodePKCS8(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* First convert into a NSS_DSAPrivateKeyPKCS8 */
		NSS_DSAPrivateKeyPKCS8 nssPrivKey;
		NSS_DSAAlgParams algParams;
		memset(&nssPrivKey, 0, sizeof(nssPrivKey));
		memset(&algParams, 0, sizeof(algParams));
		nssPrivKey.dsaAlg.params = &algParams;
		intToCssmData(openKey->version, nssPrivKey.version, coder);
		dsaToNssAlgIdX509(openKey, nssPrivKey.dsaAlg, coder);
		
		/* pre-encode, nssPrivKey.privateKey is the DER-encoding of 
		 * an ASN integer... */
		CSSM_DATA privKeyInt;
		bnToCssmData(openKey->priv_key, privKeyInt, coder);

		/* DER encode the privateKey portion into arena pool memory
		 * into NSS_DSAPrivateKeyPKCS8.privateKey */
		coder.encodeItem(&privKeyInt, kSecAsn1UnsignedIntegerTemplate,
			nssPrivKey.privateKey);

		/* DER encode the whole thing */
		PRErrorCode perr;
		perr = SecNssEncodeItemOdata(&nssPrivKey,
			kSecAsn1DSAPrivateKeyPKCS8Template, encodedKey);
		return 0;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

/* 
 * DSA public key, X509/openssl format.
 */
static CSSM_RETURN DSAPublicKeyDecodeX509(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	NSS_DSAPublicKeyX509 nssPubKey;
	PRErrorCode perr;
	CSSM_RETURN crtn;
	
	memset(&nssPubKey, 0, sizeof(nssPubKey));
	perr = coder.decode(p, length, kSecAsn1DSAPublicKeyX509Template, 
		&nssPubKey);
	if(perr) {
		logAsnErr("decode(DSAPublicKeyX509)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* X509 style DSA-specific alg params */
	NSS_DSAAlgorithmIdX509 &algId = nssPubKey.dsaAlg;
	crtn = nssAlgIdToDsaX509(algId, openKey);
	if(crtn) {
		return crtn;
	}
	
	/* inside of nssPubKey.publicKey is the DER-encoding of a 
	 * ASN Integer; decoded length was in bits */
	nssPubKey.publicKey.Length = (nssPubKey.publicKey.Length + 7) / 8;	
	CSSM_DATA pubKeyBytes = {0, NULL};
	perr = coder.decodeItem(nssPubKey.publicKey,
		kSecAsn1UnsignedIntegerTemplate, 
		&pubKeyBytes);
	if(perr) {
		logAsnErr("decode(NSS_DSAPublicKeyX509.publicKey)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}
	openKey->pub_key = cssmDataToBn(pubKeyBytes);

	if(openKey->pub_key == NULL) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	return 0;
}

static CSSM_RETURN DSAPublicKeyEncodeX509(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* convert to NSS_DSAPublicKeyX509 */
		NSS_DSAPublicKeyX509 nssPubKey;
		NSS_DSAAlgParams algParams;
		memset(&nssPubKey, 0, sizeof(nssPubKey));
		memset(&algParams, 0, sizeof(algParams));
		nssPubKey.dsaAlg.params = &algParams;
		dsaToNssAlgIdX509(openKey, nssPubKey.dsaAlg, coder);
		
		/* 
		 * publicKey is the DER-encoding of a ASN INTEGER wrapped in 
		 * an AsnBits
		 */
		CSSM_DATA pubKeyRaw;
		PRErrorCode perr;
		bnToCssmData(openKey->pub_key, pubKeyRaw, coder);
		perr = coder.encodeItem(&pubKeyRaw,	kSecAsn1UnsignedIntegerTemplate, 
			nssPubKey.publicKey);
		if(perr) {
			logAsnErr("encodeItem(DSAPublicKeyX509.publicKey)", perr);
			return CSSMERR_CSP_MEMORY_ERROR;
		}
		nssPubKey.publicKey.Length *= 8;
		
		/* DER encode */
		SecNssEncodeItemOdata(&nssPubKey, kSecAsn1DSAPublicKeyX509Template, 
			encodedKey);
		return CSSM_OK;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

/* 
 * Encode public key portion only for calculating key digest.
 * Note this works just fine on a partial DSA public key, i.e.,
 * A DSA public key's digest-capable blob is the same whether or
 * not the DSA key has its DSA parameters p, q, and g.
 */
static CSSM_RETURN DSAPublicKeyEncodeHashable(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* 
		 * publicKey is the DER-encoding of an ASN integer
		 */
		CSSM_DATA pubKey;
		bnToCssmData(openKey->pub_key, pubKey, coder);
		PRErrorCode perr;
		
		perr = SecNssEncodeItemOdata(&pubKey, kSecAsn1UnsignedIntegerTemplate,
			encodedKey);
		if(perr) {
			logAsnErr("encode(DSAPubHashable)", perr);
			return CSSMERR_CSP_MEMORY_ERROR;
		}
		return CSSM_OK;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

/*
 * DSA private key, custom openssl format. 
 */
static CSSM_RETURN DSAPrivateKeyDecodeOpenssl(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	NSS_DSAPrivateKeyOpenssl nssPrivKey;
	PRErrorCode perr;
	
	memset(&nssPrivKey, 0, sizeof(nssPrivKey));
	perr = coder.decode(p, length, kSecAsn1DSAPrivateKeyOpensslTemplate, 
		&nssPrivKey);
	if(perr) {
		logAsnErr("decode(DSAPrivateKeyOpenssl)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	/* convert nssPrivKey fields to RSA key fields */
	try {
		openKey->version = cssmDataToInt(nssPrivKey.version);
		openKey->p	 	  = cssmDataToBn(nssPrivKey.p);
		openKey->q	 	  = cssmDataToBn(nssPrivKey.q);
		openKey->g 	 	  = cssmDataToBn(nssPrivKey.g);
		openKey->pub_key  = cssmDataToBn(nssPrivKey.pub);
		openKey->priv_key = cssmDataToBn(nssPrivKey.priv);
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

static CSSM_RETURN DSAPrivateKeyEncodeOpenssl(
	SecNssCoder 	&coder,
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	NSS_DSAPrivateKeyOpenssl nssPrivKey;
	PRErrorCode perr;
	
	/* convert to NSS_DSAPrivateKeyOpenssl */
	try {
		intToCssmData(openKey->version, nssPrivKey.version, coder);
		bnToCssmData(openKey->p, 		nssPrivKey.p, coder);
		bnToCssmData(openKey->q, 		nssPrivKey.q, coder);
		bnToCssmData(openKey->g, 		nssPrivKey.g, coder);
		bnToCssmData(openKey->pub_key, 	nssPrivKey.pub, coder);
		bnToCssmData(openKey->priv_key,	nssPrivKey.priv, coder);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* DER encode */
	perr = SecNssEncodeItemOdata(&nssPrivKey, kSecAsn1DSAPrivateKeyOpensslTemplate,
		encodedKey);
	if(perr) {
		logAsnErr("encode(DSAPrivateKeyOpenssl)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

CSSM_RETURN DSAPublicKeyDecode(
	DSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	void 				*p, 
	size_t				length)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
			return DSAPublicKeyDecodeFIPS186(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_X509:
			return DSAPublicKeyDecodeX509(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
			return DSAPublicKeyDecodeOpenSSH2(openKey, p, length);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	DSAPublicKeyEncode(
	DSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	const CssmData		&descData,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
			return DSAPublicKeyEncodeFIPS186(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_X509:
			return DSAPublicKeyEncodeX509(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
			return DSAPublicKeyEncodeHashable(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
			return DSAPublicKeyEncodeOpenSSH2(openKey, descData, encodedKey);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN DSAPrivateKeyDecode(
	DSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	void 				*p, 
	size_t				length)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
			return DSAPrivateKeyDecodeFIPS186(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
			return DSAPrivateKeyDecodeOpenssl(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
			return DSAPrivateKeyDecodePKCS8(coder, openKey, p, length);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	DSAPrivateKeyEncode(
	DSA 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	const CssmData		&descData,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
			return DSAPrivateKeyEncodeFIPS186(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
			return DSAPrivateKeyEncodeOpenssl(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
			return DSAPrivateKeyEncodePKCS8(coder, openKey, encodedKey);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

#pragma mark -
#pragma mark *** DSA Signature encode/decode ***

CSSM_RETURN DSASigEncode(
	DSA_SIG			*openSig,
	CssmOwnedData	&encodedSig)
{
	/* temp allocs from this pool */
	SecNssCoder coder;
	/* convert to NSS_DSASignature */
	NSS_DSASignature nssSig;
	
	try {
		bnToCssmData(openSig->r, nssSig.r, coder);
		bnToCssmData(openSig->s, nssSig.s, coder);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	PRErrorCode prtn = SecNssEncodeItemOdata(&nssSig, 
		kSecAsn1DSASignatureTemplate, encodedSig);
	if(prtn) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

CSSM_RETURN DSASigDecode(
	DSA_SIG 		*openSig, 
	const void 		*p, 
	unsigned		length)
{
	NSS_DSASignature nssSig;
	SecNssCoder coder;
	
	memset(&nssSig, 0, sizeof(nssSig));
	PRErrorCode perr = coder.decode(p, length, 
		kSecAsn1DSASignatureTemplate, &nssSig);
	if(perr) {
		logAsnErr("decode(DSASigDecode)", perr);
		return CSSMERR_CSP_INVALID_SIGNATURE;
	}
	
	try {
		openSig->r = cssmDataToBn(nssSig.r);
		openSig->s = cssmDataToBn(nssSig.s);
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

#pragma mark -
#pragma mark *** DSA Algorithm Parameters encode/decode ***

CSSM_RETURN DSAEncodeAlgParams(
	NSS_DSAAlgParams	&algParams,
	CssmOwnedData		&encodedParams)
{
	PRErrorCode prtn = SecNssEncodeItemOdata(&algParams, 
		kSecAsn1DSAAlgParamsTemplate, encodedParams);
	if(prtn) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

CSSM_RETURN DSADecodeAlgParams(
	NSS_DSAAlgParams	&algParams,
	const void			*p,
	unsigned			len,
	SecNssCoder 		&coder)
{
	
	memset(&algParams, 0, sizeof(algParams));
	PRErrorCode perr = coder.decode(p, len, 
		kSecAsn1DSAAlgParamsTemplate, &algParams);
	if(perr) {
		logAsnErr("decode(DSAAlgParams)", perr);
		return CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS;
	}
	return CSSM_OK;
}

#pragma mark -
#pragma mark *** Diffie-Hellman key encode/decode ***

CSSM_RETURN DHPrivateKeyDecodePKCS3(
	SecNssCoder		&coder,
	DH	 			*openKey, 
	unsigned char 	*p, 
	unsigned 		length)
{
	NSS_DHPrivateKey nssPrivKey;
	PRErrorCode perr;
	
	memset(&nssPrivKey, 0, sizeof(nssPrivKey));
	perr = coder.decode(p, length, kSecAsn1DHPrivateKeyTemplate, &nssPrivKey);
	if(perr) {
		logAsnErr("decode(DHPrivateKey)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	/* verify alg identifier */
	const CSSM_OID *oid = &nssPrivKey.dhOid;
	if(!cspCompareCssmData(oid, &CSSMOID_DH)) {
		sslAsn1Debug("DHPrivateKeyDecode: bad OID");
		return CSSMERR_CSP_ALGID_MISMATCH;
	}

	NSS_DHParameter	&params = nssPrivKey.params;

	try {
		openKey->priv_key = cssmDataToBn(nssPrivKey.secretPart);
		openKey->p	      = cssmDataToBn(params.prime);
		openKey->g 	      = cssmDataToBn(params.base);
		/* TBD - ignore privateValueLength for now */
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN	DHPrivateKeyEncodePKCS3(
	SecNssCoder		&coder,
	DH	 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	/* convert into a NSS_DHPrivateKey */
	NSS_DHPrivateKey nssPrivKey;
	NSS_DHParameter &params = nssPrivKey.params;
	memset(&nssPrivKey, 0, sizeof(nssPrivKey));
	nssPrivKey.dhOid = CSSMOID_DH;
	
	
	try {
		bnToCssmData(openKey->priv_key, nssPrivKey.secretPart, coder);
		bnToCssmData(openKey->p, params.prime, coder);
		bnToCssmData(openKey->g, params.base, coder);
		if(openKey->length) {
			/* actually currently not supported in openssl... */
			intToCssmData(openKey->length, params.privateValueLength, coder);
		}
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* DER encode */
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&nssPrivKey, kSecAsn1DHPrivateKeyTemplate,
		encodedKey);
	if(perr) {
		logAsnErr("encode(DHPrivateKey)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

/* 
 * NSS_DHAlgorithmIdentifierX942 <--> DH 
 * NOTE this is incomplete. It's functional on decode, but we throw
 * away everything except p and g. On encode, we put zeroes in 
 * all the fields we don't deal with. Thus the encode side will NOT be 
 * interoperable with other implementations.
 */
static void dhToNssAlgIdX942(
	const DH *openKey,
	NSS_DHAlgorithmIdentifierX942 &algId,
	SecNssCoder &coder)
{
	/*
	 * When trying to encode a public key in X509 form, we may in 
	 * fact have nothing here - public keys created and exported in 
	 * PKCS3 have the pub_key value, and that's it.
	 */
	 
	memset(&algId, 0, sizeof(algId));
	algId.oid = CSSMOID_ANSI_DH_PUB_NUMBER;	// not mallocd
	NSS_DHDomainParamsX942 &params = algId.params;
	uint8 zero = 0;
	CSSM_DATA czero = {1, &zero};
	if(openKey->p != NULL) {
		bnToCssmData(openKey->p, params.p, coder);
	}
	else {
		coder.allocCopyItem(czero, params.p);
	}
	if(openKey->g != NULL) {
		bnToCssmData(openKey->g, params.g, coder);
	}
	else {
		coder.allocCopyItem(czero, params.g);
	}
	/* and we never have a vali0d q */
	coder.allocCopyItem(czero, params.q);
	
}

static CSSM_RETURN nssAlgIdToDhX942(
	NSS_DHAlgorithmIdentifierX942 &algId,
	DH *openKey)
{
	if(!cspCompareCssmData(&algId.oid, &CSSMOID_ANSI_DH_PUB_NUMBER)) {
		sslAsn1Debug("nssAlgIdToDhX942: bad OID");
		return CSSMERR_CSP_INVALID_KEY;
	}
	openKey->p = cssmDataToBn(algId.params.p);
	openKey->g = cssmDataToBn(algId.params.g);
	return CSSM_OK;
}

CSSM_RETURN DHPrivateKeyDecodePKCS8(
	SecNssCoder		&coder,
	DH	 			*openKey, 
	unsigned char 	*p, 
	unsigned 		length)
{
	NSS_DHPrivateKeyPKCS8 nssPrivKey;
	PRErrorCode perr;
	
	memset(&nssPrivKey, 0, sizeof(nssPrivKey));
	perr = coder.decode(p, length, kSecAsn1DHPrivateKeyPKCS8Template,
		&nssPrivKey);
	if(perr) {
		logAsnErr("decode(DHPrivateKeyPKCS8)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	try {
		CSSM_RETURN crtn = nssAlgIdToDhX942(nssPrivKey.algorithm, openKey);
		if(crtn) {
			return crtn;
		}
		
		/* post-decode private key is a DER encoded integer */
		CSSM_DATA privKeyInt = {0, NULL};
		if(coder.decodeItem(nssPrivKey.privateKey,
				kSecAsn1UnsignedIntegerTemplate,
				&privKeyInt)) {
			logAsnErr("decode(DHPrivateKeyPKCS8 privKey int)", perr);
			return CSSMERR_CSP_INVALID_KEY;
		}

		openKey->priv_key = cssmDataToBn(privKeyInt);
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN	DHPrivateKeyEncodePKCS8(
	SecNssCoder		&coder,
	DH	 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	/* convert into a NSS_DHPrivateKeyPKCS8 */
	NSS_DHPrivateKeyPKCS8 nssPrivKey;
	memset(&nssPrivKey, 0, sizeof(nssPrivKey));
	uint8 vers = 0;
	nssPrivKey.version.Length = 1;
	nssPrivKey.version.Data = &vers;
	NSS_DHAlgorithmIdentifierX942 &alg = nssPrivKey.algorithm;

	try {
		
		dhToNssAlgIdX942(openKey, alg, coder);
		/* pre-encode, nssPrivKey.privateKey is the DER-encoding of 
		 * an ASN integer... */
		CSSM_DATA privKeyInt;
		bnToCssmData(openKey->priv_key, privKeyInt, coder);

		/* DER encode the privateKey portion into arena pool memory
		 * into nssPrivKey.privateKey */
		coder.encodeItem(&privKeyInt, kSecAsn1UnsignedIntegerTemplate,
			nssPrivKey.privateKey);
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* DER encode */
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&nssPrivKey, kSecAsn1DHPrivateKeyPKCS8Template,
		encodedKey);
	if(perr) {
		logAsnErr("encode(DHPrivateKey)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

/*
 * In the PKCS3 form, the public blob is simply the literal
 * public key value, not DER encoded.
 */
static CSSM_RETURN DHPublicKeyDecodePKCS3(
	DH	 			*openKey, 
	SecNssCoder		&coder,
	unsigned char 	*p, 
	unsigned 		length)
{
	try {
		CSSM_DATA pubKey = {(uint32)length, (uint8 *)p};
		openKey->pub_key = cssmDataToBn(pubKey);
		return CSSM_OK;
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

static CSSM_RETURN DHPublicKeyEncodePKCS3(
	DH	 			*openKey, 
	SecNssCoder		&coder,
	CssmOwnedData	&encodedKey)
{
	try {
		CSSM_DATA pubKey;
		bnToCssmData(openKey->pub_key, pubKey, coder);
		encodedKey.copy(CssmData::overlay(pubKey));
		return CSSM_OK;
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

static CSSM_RETURN DHPublicKeyDecodeX509(
	DH	 			*openKey, 
	SecNssCoder		&coder,
	unsigned char 	*p, 
	unsigned 		length)
{
	NSS_DHPublicKeyX509 nssPubKey;
	PRErrorCode perr;
	
	memset(&nssPubKey, 0, sizeof(nssPubKey));
	perr = coder.decode(p, length, kSecAsn1DHPublicKeyX509Template,
		&nssPubKey);
	if(perr) {
		logAsnErr("decode(DHPublicKeyX509)", perr);
		return CSSMERR_CSP_INVALID_KEY;
	}

	try {
		CSSM_RETURN crtn = nssAlgIdToDhX942(nssPubKey.algorithm, openKey);
		if(crtn) {
			return crtn;
		}
		
		/* 
		 * Post-decode public key length in bits 
		 * Contents are pub_key as DER-encoded INTEGER
		 */
		CSSM_DATA &pubKey = nssPubKey.publicKey;
		pubKey.Length = (pubKey.Length + 7) / 8;
		CSSM_DATA pubKeyInt = {0, NULL};
		if(coder.decodeItem(pubKey, 
				kSecAsn1UnsignedIntegerTemplate, &pubKeyInt)) {
			logAsnErr("decode(DHPublicKeyX509 pub key int)", perr);
			return CSSMERR_CSP_INVALID_KEY;
		}
		openKey->pub_key = cssmDataToBn(pubKeyInt);
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

static CSSM_RETURN DHPublicKeyEncodeX509(
	DH	 			*openKey, 
	SecNssCoder		&coder,
	CssmOwnedData	&encodedKey)
{
	/* convert into a NSS_DHPublicKeyX509 */
	NSS_DHPublicKeyX509 nssPubKey;
	memset(&nssPubKey, 0, sizeof(nssPubKey));
	NSS_DHAlgorithmIdentifierX942 &alg = nssPubKey.algorithm;

	try {
		dhToNssAlgIdX942(openKey, alg, coder);
		
		/* encode pub_key as integer */
		CSSM_DATA pubKeyInt = {0, NULL};
		bnToCssmData(openKey->pub_key, pubKeyInt, coder);
		coder.encodeItem(&pubKeyInt, kSecAsn1UnsignedIntegerTemplate,
			nssPubKey.publicKey);
		/* specify length in bits */
		nssPubKey.publicKey.Length *= 8;
	}
	catch(...) {
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* DER encode */
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&nssPubKey, kSecAsn1DHPublicKeyX509Template,
		encodedKey);
	if(perr) {
		logAsnErr("encode(DHPublicKeyX509)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

CSSM_RETURN DHPrivateKeyDecode(
	DH	 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	unsigned char 		*p, 
	unsigned 			length)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS3:
			return DHPrivateKeyDecodePKCS3(coder, openKey, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
			return DHPrivateKeyDecodePKCS8(coder, openKey, p, length);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	DHPrivateKeyEncode(
	DH	 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS3:
			return DHPrivateKeyEncodePKCS3(coder, openKey, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
			return DHPrivateKeyEncodePKCS8(coder, openKey, encodedKey);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN DHPublicKeyDecode(
	DH	 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	unsigned char 		*p, 
	unsigned 			length)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS3:
			return DHPublicKeyDecodePKCS3(openKey, coder, p, length);
		case CSSM_KEYBLOB_RAW_FORMAT_X509:
			return DHPublicKeyDecodeX509(openKey, coder, p, length);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

CSSM_RETURN	DHPublicKeyEncode(
	DH	 				*openKey, 
	CSSM_KEYBLOB_FORMAT	format,
	CssmOwnedData		&encodedKey)
{
	SecNssCoder coder;

	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS3:
			return DHPublicKeyEncodePKCS3(openKey, coder, encodedKey);
		case CSSM_KEYBLOB_RAW_FORMAT_X509:
			return DHPublicKeyEncodeX509(openKey, coder, encodedKey);
		default:
			assert(0);
			return CSSMERR_CSP_INTERNAL_ERROR;
	}
}

/*
 * Encode/decode a NSS_DHParameterBlock.
 */
CSSM_RETURN DHParamBlockDecode(
	const CSSM_DATA &encParam,
	NSS_DHParameterBlock &paramBlock,
	SecNssCoder &coder)
{
	PRErrorCode perr;
	
	memset(&paramBlock, 0, sizeof(paramBlock));
	perr = coder.decodeItem(encParam, kSecAsn1DHParameterBlockTemplate, 
		&paramBlock);
	if(perr == 0) {
		return CSSM_OK;
	}
	
	/*
	 * CDSA Extension: the CDSA Algorithm Guide says that the D-H
	 * parameter block is supposed to be wrapped with its accompanying
	 * OID. However Openssl does not do this; it just exports 
	 * an encoded DHParameter rather than a DHParameterBlock.
	 * For compatibility we'll try decoding the parameters as one
	 * of these. 
	 */
	memset(&paramBlock, 0, sizeof(paramBlock));
	perr = coder.decodeItem(encParam, kSecAsn1DHParameterTemplate, 
		&paramBlock.params);
	if(perr == 0) {
		return CSSM_OK;
	}
	return CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS;
}

#pragma mark -
#pragma mark *** Message Digest ***

/*
 * Given a message digest and associated algorithm, cook up a PKCS1-style
 * DigestInfo and return its DER encoding. This is a necessary step for 
 * RSA signature (both generating and verifying) - the output of this 
 * routine is what gets encrypted during signing, and what is expected when
 * verifying (i.e., decrypting the signature).
 *
 * A good guess for the length of the output digestInfo is the size of the
 * key being used to sign/verify. The digest can never be larger than that. 
 */
CSSM_RETURN generateDigestInfo(
	const void		*msgDigest,
	size_t			digestLen,
	CSSM_ALGORITHMS	digestAlg,		// CSSM_ALGID_SHA1, etc.
	CssmOwnedData	&encodedInfo,
	size_t			maxEncodedSize)
{
	if(digestAlg == CSSM_ALGID_NONE) {
		/* special case, no encode, just copy */
		encodedInfo.copy(msgDigest, digestLen);
		return 0;
	}
	
	NSS_DigestInfo	digestInfo;
	CSSM_X509_ALGORITHM_IDENTIFIER &algId = digestInfo.digestAlgorithm;
	
	memset(&digestInfo, 0, sizeof(digestInfo));
	switch(digestAlg) {
		case CSSM_ALGID_MD5:
			algId.algorithm = CSSMOID_MD5;
			break;
		case CSSM_ALGID_MD2:
			algId.algorithm = CSSMOID_MD2;
			break;
		case CSSM_ALGID_SHA1:
			algId.algorithm = CSSMOID_SHA1;
			break;
		case CSSM_ALGID_SHA224:
			algId.algorithm = CSSMOID_SHA224;
			break;
		case CSSM_ALGID_SHA256:
			algId.algorithm = CSSMOID_SHA256;
			break;
		case CSSM_ALGID_SHA384:
			algId.algorithm = CSSMOID_SHA384;
			break;
		case CSSM_ALGID_SHA512:
			algId.algorithm = CSSMOID_SHA512;
			break;
		default:
			return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	nullAlgParams(algId);
	digestInfo.digest.Data = (uint8 *)msgDigest;
	digestInfo.digest.Length = digestLen;

	/* DER encode */
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&digestInfo, kSecAsn1DigestInfoTemplate,
		encodedInfo);
	if(perr) {
		logAsnErr("encode(digestInfo)", perr);
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return CSSM_OK;
}

