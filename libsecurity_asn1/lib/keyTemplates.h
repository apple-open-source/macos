/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * keyTemplate.h -  ASN1 templates for asymmetric keys and related
 * structs.
 */

#ifndef	_NSS_KEY_TEMPLATES_H_
#define _NSS_KEY_TEMPLATES_H_

#include <Security/secasn1t.h>
#include <Security/x509defs.h>

/*
 * Arrays of SecAsn1Templates are always associated with a specific
 * C struct. We attempt to use C structs which are defined in CDSA
 * if at all possible; these always start with the CSSM_ prefix.
 * Otherwise we define the struct here, with an NSS_ prefix.
 * In either case, the name of the C struct is listed in comments
 * along with the extern declaration of the SecAsn1Template array.
 */

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * ASN class : AlgorithmIdentifier
 * C struct  : CSSM_X509_ALGORITHM_IDENTIFIER
 */
extern const SecAsn1Template kSecAsn1AlgorithmIDTemplate[];

/*
 * ASN class : SubjectPublicKeyInfo
 * C struct  : CSSM_X509_SUBJECT_PUBLIC_KEY_INFO
 */
extern const SecAsn1Template kSecAsn1SubjectPublicKeyInfoTemplate[];

/*
 * ASN class : Attribute
 * C struct  : NSS_Attribute
 */
typedef struct {
    CSSM_OID 	attrType;	
    CSSM_DATA 	**attrValue;
} NSS_Attribute;

extern const SecAsn1Template kSecAsn1AttributeTemplate[];
extern const SecAsn1Template kSecAsn1SetOfAttributeTemplate[];

/*
 * PKCS8 private key info
 * ASN class : PrivateKeyInfo
 * C struct  : NSS_PrivateKeyInfo
 */
typedef struct {
    CSSM_DATA 						version;
    CSSM_X509_ALGORITHM_IDENTIFIER 	algorithm;
    CSSM_DATA 						privateKey;
    NSS_Attribute 					**attributes;
} NSS_PrivateKeyInfo;

extern const SecAsn1Template kSecAsn1PrivateKeyInfoTemplate[];

/*
 * PKCS8 Encrypted Private Key Info
 * ASN class : EncryptedPrivateKeyInfo
 * C struct  : NSS_EncryptedPrivateKeyInfo
 *
 * The decrypted encryptedData field is a DER-encoded
 * NSS_PrivateKeyInfo.
 */
typedef struct {
	CSSM_X509_ALGORITHM_IDENTIFIER	algorithm;
	CSSM_DATA						encryptedData;
} NSS_EncryptedPrivateKeyInfo;

extern const SecAsn1Template kSecAsn1EncryptedPrivateKeyInfoTemplate[];

/*
 * ASN class : DigestInfo
 * C struct  : NSS_DigestInfo
 */
typedef struct {
	CSSM_X509_ALGORITHM_IDENTIFIER	digestAlgorithm;
	CSSM_DATA						digest;
} NSS_DigestInfo;

extern const SecAsn1Template kSecAsn1DigestInfoTemplate[];

/*
 * Key structs and templates, placed here due to their ubiquitous use.
 */

#pragma mark *** RSA ***

/*
 * RSA public key, PKCS1 format
 * 
 * ASN class : RSAPublicKey
 * C struct  : NSS_RSAPublicKeyPKCS1
 */
typedef struct {
    CSSM_DATA modulus;
    CSSM_DATA publicExponent;
} NSS_RSAPublicKeyPKCS1;

extern const SecAsn1Template kSecAsn1RSAPublicKeyPKCS1Template[];

/*
 * RSA public key, X509 format: NSS_SubjectPublicKeyInfoTemplate
 */

/*
 * RSA private key, PKCS1 format, used by openssl
 *
 * ASN class : RSAPrivateKey
 * C struct  : NSS_RSAPrivateKeyPKCS1
 */
typedef struct {
	CSSM_DATA version;
    CSSM_DATA modulus;
    CSSM_DATA publicExponent;
    CSSM_DATA privateExponent;
    CSSM_DATA prime1;
    CSSM_DATA prime2;
    CSSM_DATA exponent1;
    CSSM_DATA exponent2;
    CSSM_DATA coefficient;
} NSS_RSAPrivateKeyPKCS1;

extern const SecAsn1Template kSecAsn1RSAPrivateKeyPKCS1Template[];

/*
 * RSA private key, PKCS8 format: NSS_PrivateKeyInfo; the privateKey
 * value is a DER-encoded NSS_RSAPrivateKeyPKCS1.
 */

#pragma mark *** Diffie-Hellman ***

/*** from PKCS3 ***/

/*
 * ASN class : DHParameter
 * C struct  : NSS_DHParameter
 */
typedef struct {
	CSSM_DATA		prime;
	CSSM_DATA		base;
	CSSM_DATA		privateValueLength;	// optional
} NSS_DHParameter;

extern const SecAsn1Template kSecAsn1DHParameterTemplate[];

/*
 * ASN class : DHParameterBlock
 * C struct  : NSS_DHParameterBlock
 */
typedef struct {
	CSSM_OID		oid;				// CSSMOID_PKCS3
	NSS_DHParameter	params;
} NSS_DHParameterBlock;

extern const SecAsn1Template kSecAsn1DHParameterBlockTemplate[];

/*
 * ASN class : DHPrivateKey
 * C struct  : NSS_DHPrivateKey
 */
typedef struct {
	CSSM_OID		dhOid;				// CSSMOID_DH
	NSS_DHParameter	params;
	CSSM_DATA		secretPart;
} NSS_DHPrivateKey;

extern const SecAsn1Template kSecAsn1DHPrivateKeyTemplate[];

/* 
 * ANSI X9.42 style Diffie-Hellman keys.
 * 
 * DomainParameters ::= SEQUENCE {  -- Galois field group parameters
 *   p         INTEGER,            -- odd prime, p = jq + 1
 *   g         INTEGER,            -- generator, g ^ q = 1 mod p
 *   q         INTEGER,            -- prime factor of p-1
 *   j         INTEGER  OPTIONAL,  -- cofactor, j >= 2
 *                                 -- required for cofactor method
 *   valParms  ValidationParms  OPTIONAL
 * } 
 *
 * ValidationParms ::= SEQUENCE {
 *   seed           BIT STRING,  -- seed for prime number generation
 *   pGenCounter    INTEGER      -- parameter verification 
 * }
 */
typedef struct {
	CSSM_DATA		seed;			// BIT STRING, length in bits
	CSSM_DATA		pGenCounter;
} NSS_DHValidationParams;

typedef struct {
	CSSM_DATA				p;
	CSSM_DATA				g;
	CSSM_DATA				q;
	CSSM_DATA				j;			// OPTIONAL
	NSS_DHValidationParams	*valParams;	// OPTIONAL
} NSS_DHDomainParamsX942;

/* Custom X9.42 D-H AlgorithmIdentifier */
typedef struct {
	CSSM_OID				oid;		// CSSMOID_ANSI_DH_PUB_NUMBER
	NSS_DHDomainParamsX942	params;
} NSS_DHAlgorithmIdentifierX942;

extern const SecAsn1Template kSecAsn1DHValidationParamsTemplate[];
extern const SecAsn1Template kSecAsn1DHDomainParamsX942Template[];
extern const SecAsn1Template kSecAsn1DHAlgorithmIdentifierX942Template[];

/* PKCS8 form of D-H private key using X9.42 domain parameters */
typedef struct {
    CSSM_DATA 						version;
	NSS_DHAlgorithmIdentifierX942	algorithm;
	/* octet string containing DER-encoded integer */
	CSSM_DATA						privateKey;
    NSS_Attribute 					**attributes;	// OPTIONAL
} NSS_DHPrivateKeyPKCS8;

/* X509 form of D-H public key using X9.42 domain parameters */
typedef struct {
	NSS_DHAlgorithmIdentifierX942	algorithm;
	/* bit string containing DER-encoded integer representing 
	 * raw public key */
	CSSM_DATA						publicKey;		// length in BITS
} NSS_DHPublicKeyX509;

extern const SecAsn1Template kSecAsn1DHPrivateKeyPKCS8Template[];
extern const SecAsn1Template kSecAsn1DHPublicKeyX509Template[];
 
#pragma mark *** ECDSA ***

/* 
 * ECDSA Private key as defined in section C.4 of Certicom SEC1.
 * The DER encoding of this is placed in the privateKey field
 * of a NSS_PrivateKeyInfo.
 */
typedef struct {
    CSSM_DATA 		version;
	CSSM_DATA		privateKey;
	CSSM_DATA		params;		/* optional, ANY */
	CSSM_DATA		pubKey;		/* BITSTRING, optional */
} NSS_ECDSA_PrivateKey;

extern const SecAsn1Template kSecAsn1ECDSAPrivateKeyInfoTemplate[];

#ifdef  __cplusplus
}
#endif

#endif	/* _NSS_RSA_KEY_TEMPLATES_H_ */
