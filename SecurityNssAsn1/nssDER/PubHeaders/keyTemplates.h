/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * keyTemplate.h -  ASN1 templates for asymmetric keys and related
 * structs.
 */

#ifndef	_NSS_KEY_TEMPLATES_H_
#define _NSS_KEY_TEMPLATES_H_

#include <SecurityNssAsn1/secasn1.h>
#include <Security/x509defs.h>

/*
 * Arrays of SEC_ASN1Templates are always associated with a specific
 * C struct. We attempt to use C structs which are defined in CDSA
 * if at all possible; these always start with the CSSM_ prefix.
 * Otherwise we define the struct here, with an NSS_ prefix.
 * In either case, the name of the C struct is listed in comments
 * along with the extern declaration of the SEC_ASN1Template array.
 */

SEC_BEGIN_PROTOS

/*
 * ASN class : AlgorithmIdentifier
 * C struct  : CSSM_X509_ALGORITHM_IDENTIFIER
 */
extern const SEC_ASN1Template NSS_AlgorithmIDTemplate[];

/*
 * ASN class : SubjectPublicKeyInfo
 * C struct  : CSSM_X509_SUBJECT_PUBLIC_KEY_INFO
 */
extern const SEC_ASN1Template NSS_SubjectPublicKeyInfoTemplate[];

/*
 * ASN class : Attribute
 * C struct  : NSS_Attribute
 */
typedef struct {
    CSSM_OID 	attrType;	
    CSSM_DATA 	**attrValue;
} NSS_Attribute;

extern const SEC_ASN1Template NSS_AttributeTemplate[];
extern const SEC_ASN1Template NSS_SetOfAttributeTemplate[];

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

extern const SEC_ASN1Template NSS_PrivateKeyInfoTemplate[];

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

extern const SEC_ASN1Template NSS_EncryptedPrivateKeyInfoTemplate[];

/*
 * ASN class : DigestInfo
 * C struct  : NSS_DigestInfo
 */
typedef struct {
	CSSM_X509_ALGORITHM_IDENTIFIER	digestAlgorithm;
	CSSM_DATA						digest;
} NSS_DigestInfo;

extern const SEC_ASN1Template NSS_DigestInfoTemplate[];

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

extern const SEC_ASN1Template NSS_RSAPublicKeyPKCS1Template[];

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

extern const SEC_ASN1Template NSS_RSAPrivateKeyPKCS1Template[];

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

extern const SEC_ASN1Template NSS_DHParameterTemplate[];

/*
 * ASN class : DHParameterBlock
 * C struct  : NSS_DHParameterBlock
 */
typedef struct {
	CSSM_OID		oid;				// CSSMOID_PKCS3
	NSS_DHParameter	params;
} NSS_DHParameterBlock;

extern const SEC_ASN1Template NSS_DHParameterBlockTemplate[];

/*
 * ASN class : DHPrivateKey
 * C struct  : NSS_DHPrivateKey
 */
typedef struct {
	CSSM_OID		dhOid;				// CSSMOID_DH
	NSS_DHParameter	params;
	CSSM_DATA		secretPart;
} NSS_DHPrivateKey;

extern const SEC_ASN1Template NSS_DHPrivateKeyTemplate[];

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

extern const SEC_ASN1Template NSS_DHValidationParamsTemplate[];
extern const SEC_ASN1Template NSS_DHDomainParamsX942Template[];
extern const SEC_ASN1Template NSS_DHAlgorithmIdentifierX942Template[];

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

extern const SEC_ASN1Template NSS_DHPrivateKeyPKCS8Template[];
extern const SEC_ASN1Template NSS_DHPublicKeyX509Template[];
 
SEC_END_PROTOS

#endif	/* _NSS_RSA_KEY_TEMPLATES_H_ */
