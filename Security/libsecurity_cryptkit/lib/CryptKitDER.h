/*
 * CryptKitDER.h - snacc-based routines to create and parse DER-encoded FEE 
 *				   keys and signatures
 *
 * Created 3/12/2001 by dmitch.
 */

#ifndef	_CRYPTKIT_DER_H_
#define _CRYPTKIT_DER_H_

#include <security_cryptkit/ckconfig.h>

#if	CRYPTKIT_DER_ENABLE

#include <security_cryptkit/feeTypes.h>
#include <security_cryptkit/feePublicKey.h>
#include <security_cryptkit/giantIntegers.h>
#include <security_cryptkit/falloc.h>
#include <security_cryptkit/curveParams.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Encode/decode the two FEE signature types. We malloc returned data via
 * falloc(); caller must free via ffree().
 */
feeReturn feeDEREncodeElGamalSignature(
	giant			u,
	giant			PmX,
	unsigned char	**encodedSig,		// fallocd and RETURNED
	unsigned		*encodedSigLen);	// RETURNED
	
feeReturn feeDEREncodeECDSASignature(
	giant			c,
	giant			d,
	unsigned char	**encodedSig,		// fallocd and RETURNED
	unsigned		*encodedSigLen);	// RETURNED

feeReturn feeDERDecodeElGamalSignature(
	const unsigned char	*encodedSig,
	size_t			encodedSigLen,
	giant			*u,					// newGiant'd and RETURNED
	giant			*PmX);				// newGiant'd and RETURNED
	
feeReturn feeDERDecodeECDSASignature(
	const unsigned char	*encodedSig,
	size_t			encodedSigLen,
	giant			*c,					// newGiant'd and RETURNED
	giant			*d);				// newGiant'd and RETURNED

/*
 * Encode/decode the FEE private and public keys. We malloc returned data via
 * falloc(); caller must free via ffree().
 * These use a DER format which is custom to this module.
 */
feeReturn feeDEREncodePublicKey(
	int			version,
	const curveParams	*cp,
	giant			plusX,
	giant			minusX,
	giant			plusY,			// may be NULL
	unsigned char	**keyBlob,		// fmallocd and RETURNED
	unsigned		*keyBlobLen);		// RETURNED
	
feeReturn feeDEREncodePrivateKey(
	int				version,
	const curveParams	*cp,
	const giant		privData,
	unsigned char	**keyBlob,		// fmallocd and RETURNED
	unsigned		*keyBlobLen);	// RETURNED

feeReturn feeDERDecodePublicKey(
	const unsigned char	*keyBlob,
	unsigned		keyBlobLen,
	int				*version,		// this and remainder RETURNED
	curveParams		**cp,
	giant			*plusX,
	giant			*minusX,
	giant			*plusY);		// always valid, may be (giant)0
	
feeReturn feeDERDecodePrivateKey(
	const unsigned char	*keyBlob,
	unsigned		keyBlobLen,
	int				*version,		// this and remainder RETURNED
	curveParams		**cp,
	giant			*privData);		// RETURNED

/* obtain the max size of a DER-encoded signature (either ElGamal or ECDSA) */
unsigned feeSizeOfDERSig(
	giant g1,
	giant g2);

/* 
 * Encode/decode public key in X.509 format.
 */
feeReturn feeDEREncodeX509PublicKey(
	const unsigned char	*pubBlob,		/* x and y octet string */
	unsigned			pubBlobLen,
	curveParams			*cp,
	unsigned char		**x509Blob,		/* fmallocd and RETURNED */
	unsigned			*x509BlobLen);	/* RETURNED */
	
feeReturn feeDERDecodeX509PublicKey(
	const unsigned char	*x509Blob,
	unsigned			x509BlobLen,
	feeDepth			*depth,			/* RETURNED */
	unsigned char		**pubBlob,		/* x and y octet string RETURNED */
	unsigned			*pubBlobLen);	/* RETURNED */

/* 
 * Encode private, and decode private or public key, in unencrypted OpenSSL format.
 */
feeReturn feeDEREncodeOpenSSLPrivateKey(
	const unsigned char	*privBlob,		/* private data octet string */
	unsigned			privBlobLen,
	const unsigned char *pubBlob,		/* public key, optional */
	unsigned			pubBlobLen,
	curveParams			*cp,
	unsigned char		**openBlob,		/* fmallocd and RETURNED */
	unsigned			*openBlobLen);	/* RETURNED */
	
feeReturn feeDERDecodeOpenSSLKey(
	const unsigned char	*osBlob,
	unsigned			osBlobLen,
	feeDepth			*depth,			/* RETURNED */
	unsigned char		**privBlob,		/* private data octet string RETURNED */
	unsigned			*privBlobLen,	/* RETURNED */
	unsigned char		**pubBlob,		/* public data octet string optionally RETURNED */
	unsigned			*pubBlobLen);
	
/* 
 * Encode/decode private key in unencrypted PKCS8 format.
 */
feeReturn feeDEREncodePKCS8PrivateKey(
	const unsigned char	*privBlob,		/* private data octet string */
	unsigned			privBlobLen,
	const unsigned char	*pubBlob,		/* public blob, optional */
	unsigned			pubBlobLen,
	curveParams			*cp,
	unsigned char		**pkcs8Blob,	/* fmallocd and RETURNED */
	unsigned			*pkcs8BlobLen);	/* RETURNED */
	
feeReturn feeDERDecodePKCS8PrivateKey(
	const unsigned char	*pkcs8Blob,
	unsigned			pkcs8BlobLen,
	feeDepth			*depth,			/* RETURNED */
	unsigned char		**privBlob,		/* private data octet string RETURNED */
	unsigned			*privBlobLen,	/* RETURNED */
	unsigned char		**pubBlob,		/* optionally returned, if it's there */
	unsigned			*pubBlobLen);
	

#ifdef	__cplusplus
}
#endif

#endif	/* CRYPTKIT_DER_ENABLE */
#endif	/* _CRYPTKIT_DER_H_ */


