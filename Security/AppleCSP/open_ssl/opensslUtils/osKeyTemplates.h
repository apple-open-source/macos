/*
 * osKeyTemplate.h -  ASN1 templates for openssl asymmetric keys
 */

#ifndef	_OS_KEY_TEMPLATES_H_
#define _OS_KEY_TEMPLATES_H_

#include <SecurityNssAsn1/secasn1.h>
#include <SecurityNssAsn1/keyTemplates.h>

/*
 * Arrays of SEC_ASN1Templates are always associated with a specific
 * C struct. We attempt to use C structs which are defined in CDSA
 * if at all possible; these always start with the CSSM_ prefix.
 * Otherwise we define the struct here, with an NSS_ prefix.
 * In either case, the name of the C struct is listed in comments
 * along with the extern declaration of the SEC_ASN1Template array.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*** 
 *** Note: RSA and Diffie-Hellman keys and structs are in 
 *** SecurityNssAsn1/keyTemplates.h.
 ***/
 
#pragma mark *** DSA ***

/* 
 * Note that most of the DSA structs are hand rolled and are not
 * expressed in ASN1 in any doc that I'm aware of.
 */
 
/**** 
 **** DSA support 
 ****/

/* 
 * DSA algorithm parameters. Used in CDSA key generation context as 
 * well as the parameters in an X509-formatted DSA public key.
 */
typedef struct {
	CSSM_DATA	p;
	CSSM_DATA	q;
	CSSM_DATA	g;
} NSS_DSAAlgParams;

extern const SEC_ASN1Template NSS_DSAAlgParamsTemplate[];

/*
 * DSA algorithm parameters, BSAFE style. Only used in FIPS186 format
 * public and private keys.
 */
typedef struct {
	CSSM_DATA	keySizeInBits;
	CSSM_DATA	p;
	CSSM_DATA	q;
	CSSM_DATA	g;
} NSS_DSAAlgParamsBSAFE;

extern const SEC_ASN1Template NSS_DSAAlgParamsBSAFETemplate[];

/*
 * DSA X509-style AlgorithmID. Avoids ASN_ANY processing via direct 
 * insertion of the appropriate parameters.
 */
typedef struct {
	CSSM_OID			algorithm;
	NSS_DSAAlgParams	*params;		// optional
} NSS_DSAAlgorithmIdX509;

extern const SEC_ASN1Template NSS_DSAAlgorithmIdX509Template[];

/*
 * DSA AlgorithmID, BSAFE style. Avoids ASN_ANY 
 * processing via direct insertion of the appropriate parameters.
 */
typedef struct {
	CSSM_OID				algorithm;
	NSS_DSAAlgParamsBSAFE	params;
} NSS_DSAAlgorithmIdBSAFE;

extern const SEC_ASN1Template NSS_DSAAlgorithmIdBSAFETemplate[];

/**** 
 **** DSA public keys 
 ****/

/*
 * DSA public key, openssl/X509 format.
 *
 * The publicKey is actually the DER encoding of an ASN 
 * integer, wrapped in a BIT STRING. 
 */
typedef struct {
	NSS_DSAAlgorithmIdX509	dsaAlg;
	CSSM_DATA				publicKey;		// BIT string - Length in bits
} NSS_DSAPublicKeyX509;

extern const SEC_ASN1Template NSS_DSAPublicKeyX509Template[];

/*
 * DSA public key, BSAFE/FIPS186 format.
 * The public key is the DER encoding of an ASN integer, wrapped
 * in a bit string.
 */
typedef struct {
	NSS_DSAAlgorithmIdBSAFE		dsaAlg;
	CSSM_DATA					publicKey;	// BIT string - Length in bits
} NSS_DSAPublicKeyBSAFE;

extern const SEC_ASN1Template NSS_DSAPublicKeyBSAFETemplate[];

/**** 
 **** DSA private keys 
 ****/
 
/*
 * DSA Private key, openssl custom format.
 */
typedef struct {
	CSSM_DATA	version;
	CSSM_DATA	p;
	CSSM_DATA	q;
	CSSM_DATA	g;
	CSSM_DATA	pub;
	CSSM_DATA	priv;
} NSS_DSAPrivateKeyOpenssl;

extern const SEC_ASN1Template NSS_DSAPrivateKeyOpensslTemplate[];

/*
 * DSA private key, BSAFE/FIPS186 style.
 * This is basically a DSA-specific NSS_PrivateKeyInfo.
 *
 * NSS_DSAPrivateKeyBSAFE.privateKey is an octet string containing
 * the DER encoding of this.
 */
typedef struct {
	CSSM_DATA				privateKey;
} NSS_DSAPrivateKeyOcts;

extern const SEC_ASN1Template NSS_DSAPrivateKeyOctsTemplate[];

typedef struct {
	CSSM_DATA				version;
	NSS_DSAAlgorithmIdBSAFE	dsaAlg;
	/* octet string containing a DER-encoded NSS_DSAPrivateKeyOcts */
	CSSM_DATA				privateKey;
} NSS_DSAPrivateKeyBSAFE;

extern const SEC_ASN1Template NSS_DSAPrivateKeyBSAFETemplate[];

/*
 * DSA Private Key, PKCS8/SMIME style. Doesn't have keySizeInBits
 * in the alg params; has version in the top-level struct; the 
 * private key itself is a DER-encoded integer wrapped in an
 * octet string.
 */
typedef struct {
	CSSM_DATA				version;
	NSS_DSAAlgorithmIdX509	dsaAlg;
	/* octet string containing DER-encoded integer */
	CSSM_DATA				privateKey;
    NSS_Attribute 			**attributes;		// optional
} NSS_DSAPrivateKeyPKCS8;

extern const SEC_ASN1Template NSS_DSAPrivateKeyPKCS8Template[];

/* 
 * DSA Signature.
 */
typedef struct {
	CSSM_DATA	r;
	CSSM_DATA	s;
} NSS_DSASignature;

extern const SEC_ASN1Template NSS_DSASignatureTemplate[];

#ifdef	__cplusplus
}
#endif


#endif	/* _OS_KEY_TEMPLATES_H_ */
