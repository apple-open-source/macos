/*
 * osKeyTemplate.h -  ASN1 templates for openssl asymmetric keys
 */

#include "osKeyTemplates.h"

/**** 
 **** DSA support 
 ****/

/* X509 style DSA algorithm parameters */
const SEC_ASN1Template NSS_DSAAlgParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgParams) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParams,p) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParams,q) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParams,g) },
	{ 0, }
};

/* BSAFE style DSA algorithm parameters */
const SEC_ASN1Template NSS_DSAAlgParamsBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgParamsBSAFE) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,keySizeInBits) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,p) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,q) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,g) },
	{ 0, }
};

/* DSA X509-style AlgorithmID */
const SEC_ASN1Template NSS_DSAAlgorithmIdX509Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgorithmIdX509) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DSAAlgorithmIdX509, algorithm) },
	/* per CMS, this is optional */
    { SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_DSAAlgorithmIdX509,params),
	  NSS_DSAAlgParamsTemplate },
	{ 0, }
};

/* DSA BSAFE-style AlgorithmID */
const SEC_ASN1Template NSS_DSAAlgorithmIdBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgorithmIdBSAFE) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DSAAlgorithmIdBSAFE, algorithm) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAAlgorithmIdBSAFE,params),
	  NSS_DSAAlgParamsBSAFETemplate },
	{ 0, }
};

/**** 
 **** DSA public keys 
 ****/

/* DSA public key, openssl/X509 format */
const SEC_ASN1Template NSS_DSAPublicKeyX509Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPublicKeyX509) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPublicKeyX509, dsaAlg),
	  NSS_DSAAlgorithmIdX509Template },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_DSAPublicKeyX509, publicKey), },
	{ 0, }
};

/* DSA public key, BSAFE/FIPS186 format */
const SEC_ASN1Template NSS_DSAPublicKeyBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPublicKeyBSAFE) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPublicKeyBSAFE, dsaAlg),
	  NSS_DSAAlgorithmIdBSAFETemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_DSAPublicKeyBSAFE, publicKey), },
	{ 0, }
};

/**** 
 **** DSA private keys 
 ****/
 
/* DSA Private key, openssl custom format */
const SEC_ASN1Template NSS_DSAPrivateKeyOpensslTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyOpenssl) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,version) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,p) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,q) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,g) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,pub) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,priv) },
	{ 0, }
};

/*
 * DSA private key, BSAFE/FIPS186 style.
 * This is basically a DSA-specific NSS_PrivateKeyInfo.
 *
 * NSS_DSAPrivateKeyBSAFE.privateKey is an octet string containing
 * the DER encoding of this.
 */
const SEC_ASN1Template NSS_DSAPrivateKeyOctsTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyOcts) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOcts,privateKey) },
	{ 0, }
};

const SEC_ASN1Template NSS_DSAPrivateKeyBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyBSAFE) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyBSAFE,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPrivateKeyBSAFE, dsaAlg),
	  NSS_DSAAlgorithmIdBSAFETemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DSAPrivateKeyBSAFE,privateKey) },
	{ 0, }
};

/*
 * DSA Private Key, PKCS8/SMIME style.
 */
const SEC_ASN1Template NSS_DSAPrivateKeyPKCS8Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyPKCS8) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyPKCS8,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPrivateKeyPKCS8, dsaAlg),
	  NSS_DSAAlgorithmIdX509Template },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DSAPrivateKeyPKCS8,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(NSS_DSAPrivateKeyPKCS8,attributes),
        NSS_SetOfAttributeTemplate },
	{ 0, }
};

const SEC_ASN1Template NSS_DSASignatureTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSASignature) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSASignature,r) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSASignature,s) },
	{ 0, }
};


