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
 * keyTemplate.cpp -  ASN1 templates for asymmetric keys and related
 * structs.
 */

#include <secasn1.h>
#include "keyTemplates.h"

/* AlgorithmIdentifier : CSSM_X509_ALGORITHM_IDENTIFIER */
const SEC_ASN1Template NSS_AlgorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CSSM_X509_ALGORITHM_IDENTIFIER) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CSSM_X509_ALGORITHM_IDENTIFIER,algorithm), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(CSSM_X509_ALGORITHM_IDENTIFIER,parameters), },
    { 0, }
};

/* SubjectPublicKeyInfo : CSSM_X509_SUBJECT_PUBLIC_KEY_INFO */
const SEC_ASN1Template NSS_SubjectPublicKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO) },
    { SEC_ASN1_INLINE,
	  offsetof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO,algorithm),
	  NSS_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CSSM_X509_SUBJECT_PUBLIC_KEY_INFO,subjectPublicKey), },
    { 0, }
};

/* Attribute : NSS_Attribute */
const SEC_ASN1Template NSS_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(NSS_Attribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(NSS_Attribute, attrType) },
    { SEC_ASN1_SET_OF, offsetof(NSS_Attribute, attrValue),
        SEC_AnyTemplate },
    { 0 }
};

const SEC_ASN1Template NSS_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, NSS_AttributeTemplate },
};

/* PKCS8 PrivateKeyInfo : NSS_PrivateKeyInfo */
const SEC_ASN1Template NSS_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_PrivateKeyInfo) },
    { SEC_ASN1_INTEGER, offsetof(NSS_PrivateKeyInfo,version) },
    { SEC_ASN1_INLINE, offsetof(NSS_PrivateKeyInfo,algorithm),
        NSS_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_PrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(NSS_PrivateKeyInfo,attributes),
        NSS_SetOfAttributeTemplate },
    { 0 }
};

/* NSS_EncryptedPrivateKeyInfo */
const SEC_ASN1Template NSS_EncryptedPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_EncryptedPrivateKeyInfo) },
    { SEC_ASN1_INLINE, 
	  offsetof(NSS_EncryptedPrivateKeyInfo,algorithm),
	  NSS_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, 
	  offsetof(NSS_EncryptedPrivateKeyInfo,encryptedData) },
    { 0 }
};

/* DigestInfo: NSS_DigestInfo */
const SEC_ASN1Template NSS_DigestInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DigestInfo) },
    { SEC_ASN1_INLINE, offsetof(NSS_DigestInfo,digestAlgorithm),
        NSS_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DigestInfo,digest) },
    { 0 }
};

#pragma mark -
#pragma mark *** RSA ***

/*** RSA public key, PKCS1 format : NSS_RSAPublicKeyPKCS1 ***/
const SEC_ASN1Template NSS_RSAPublicKeyPKCS1Template[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_RSAPublicKeyPKCS1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPublicKeyPKCS1,modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPublicKeyPKCS1,publicExponent) },
    { 0, }
};

/*** RSA private key key, PKCS1 format : NSS_RSAPrivateKeyPKCS1 ***/
const SEC_ASN1Template NSS_RSAPrivateKeyPKCS1Template[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_RSAPrivateKeyPKCS1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,version) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,prime1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,prime2) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,exponent1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,exponent2) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,coefficient) },
    { 0, }
};

#pragma mark -
#pragma mark *** Diffie-Hellman ***

/****
 **** Diffie-Hellman, from PKCS3.
 ****/
const SEC_ASN1Template NSS_DHParameterTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHParameter) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHParameter,prime) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHParameter,base) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof(NSS_DHParameter,privateValueLength) },
	{ 0, }
};

const SEC_ASN1Template NSS_DHParameterBlockTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHParameterBlock) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DHParameterBlock, oid) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DHParameterBlock, params),
	  NSS_DHParameterTemplate },
	{ 0, }
};

const SEC_ASN1Template NSS_DHPrivateKeyTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHPrivateKey) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DHPrivateKey, dhOid) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DHPrivateKey, params),
	  NSS_DHParameterTemplate },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHPrivateKey,secretPart) },
	{ 0, }
};

/*
 * Diffie-Hellman, X9.42 style.
 */
const SEC_ASN1Template NSS_DHValidationParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHValidationParams) },
	{ SEC_ASN1_BIT_STRING, offsetof(NSS_DHValidationParams, seed) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHValidationParams, pGenCounter) },
	{ 0, }
};

const SEC_ASN1Template NSS_DHDomainParamsX942Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHDomainParamsX942) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHDomainParamsX942, p) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHDomainParamsX942, g) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHDomainParamsX942, q) },
	{ SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, 
	  offsetof(NSS_DHDomainParamsX942, j) },
	{ SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL, 
	  offsetof(NSS_DHDomainParamsX942, valParams),
	  NSS_DHValidationParamsTemplate },
	{ 0, }
};

const SEC_ASN1Template NSS_DHAlgorithmIdentifierX942Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHAlgorithmIdentifierX942) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DHAlgorithmIdentifierX942, oid) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DHAlgorithmIdentifierX942, params),
	  NSS_DHDomainParamsX942Template },
	{ 0, }
};

const SEC_ASN1Template NSS_DHPrivateKeyPKCS8Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHPrivateKeyPKCS8) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHPrivateKeyPKCS8,version) },
    { SEC_ASN1_INLINE, offsetof(NSS_DHPrivateKeyPKCS8,algorithm),
        NSS_DHAlgorithmIdentifierX942Template },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DHPrivateKeyPKCS8,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(NSS_DHPrivateKeyPKCS8,attributes),
        NSS_SetOfAttributeTemplate },
    { 0 }
};

const SEC_ASN1Template NSS_DHPublicKeyX509Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHPublicKeyX509) },
    { SEC_ASN1_INLINE, offsetof(NSS_DHPublicKeyX509,algorithm),
        NSS_DHAlgorithmIdentifierX942Template },
	{ SEC_ASN1_BIT_STRING, offsetof(NSS_DHPublicKeyX509, publicKey) },
    { 0 }
};

