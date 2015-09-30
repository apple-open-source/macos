/* Copyright (c) 1998,2011-2012,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * feePublicKey.h
 *
 * Revision History
 * ----------------
 * 23 Mar 98 at Apple
 *	Added blob support.
 * 17 Jul 97 at Apple
 *	Added ECDSA signature routines.
 * 20 Aug 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEEPUBLICKEY_H_
#define _CK_FEEPUBLICKEY_H_

#include "ckconfig.h"

#include <sys/types.h>		/* size_t */

#if	!defined(__MACH__)
#include <feeTypes.h>
#else
#include <security_cryptkit/feeTypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Obatin a newly allocated feePubKey.
 */
feePubKey feePubKeyAlloc(void);

void feePubKeyFree(feePubKey pubKey);

/*
 * Init feePubKey from private "password" data. Incoming password data will
 * be processed with digests before use if hashPrivData is true, otherwise
 * it'll be used as is. In the 'as is' case, the privData must be at least 
 * as large as the key being created. 
 *
 * Currently two versions - one in which the size of the key is specified as
 * a feeDepth; one for key size in bits and optional primeType and curveType. 
 */
feeReturn feePubKeyInitFromPrivDataDepth(feePubKey pubKey,
	const unsigned char *privData,
	unsigned privDataLen,
	feeDepth depth,
	char hashPrivData);
	
feeReturn feePubKeyInitFromPrivDataKeyBits(feePubKey pubKey,
	const unsigned char *privData,
	unsigned privDataLen,
	unsigned keyBits,			/* key size in bits */
	feePrimeType primeType,		/* FPT_Fefault means "best one" */
	feeCurveType curveType,		/* FCT_Default means "best one" */
	char hashPrivData);

/*
 * Init feePubKey from private "password" and from data curve parameters
 * matching existing oldKey. Incoming password data will
 * be processed with digests before use if hashPrivData is true, otherwise
 * it'll be used as is. In the 'as is' case, the privData must be at least 
 * as large as the key being created. 

 */
feeReturn feePubKeyInitFromKey(feePubKey pubKey,
	const unsigned char *privData,
	unsigned privDataLen,
	feePubKey oldKey,
	char hashPrivData);

/***
 *** Exportable key blob support.
 ***
 *** Currently there are three different ways of representing a feePubKey in
 *** an exportable format. 
 ***
 *** Raw blob:  basic native blob format. 
 *** DER blob:  DER-encoded. Currently not available in ANSI C version of 
 ***            CryptKit library without additional porting; the OS X version of 
 ***            Apple implements this functionality via SNACC-generated C++ classes.
 *** KeyString: NULL-terminated ASCII C string, suitable for application such as
 ***            distributing one's public key via email. Only public keys (not
 ***            private) can be exported and imported via KeyStrings. 
 ***/
 
/*
 * Obtain portable public and private key blobs from a key.
 */
feeReturn feePubKeyCreatePubBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED

feeReturn feePubKeyCreatePrivBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED

/*
 * Init an empty feePubKey from a blob, public and private key versions. 
 */
feeReturn feePubKeyInitFromPubBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	unsigned keyBlobLen);
feeReturn feePubKeyInitFromPrivBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	unsigned keyBlobLen);

/*
 * Create a public key in the form of a null-terminated C string.
 */
feeReturn feePubKeyCreateKeyString(feePubKey pubKey,
	char **pubKeyString,		/* fmalloc'd & RETURNED */
	unsigned *pubKeyStringLen);	/* RETURNED */

/*
 * Init feePubKey from a public key string.
 */
feeReturn feePubKeyInitFromKeyString(feePubKey pubKey,
	const char *keyStr,
	unsigned keyStrLen);

#if CRYPTKIT_DER_ENABLE

/* 
 * DER format support. 
 * Obtain portable public and private DER-encoded key blobs from a key.
 */
feeReturn feePubKeyCreateDERPubBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED

feeReturn feePubKeyCreateDERPrivBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED
	
/*
 * Init an empty feePubKey from a DER-encoded blob, public and private key versions. 
 */
feeReturn feePubKeyInitFromDERPubBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	size_t keyBlobLen);
feeReturn feePubKeyInitFromDERPrivBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	size_t keyBlobLen);

/* 
 * X509 (public) and PKCS8 (private) key formatting.
 */
feeReturn feePubKeyCreateX509Blob(
	feePubKey pubKey,			// public key
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED
	
feeReturn feePubKeyCreatePKCS8Blob(
	feePubKey pubKey,			// private key
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED

feeReturn feePubKeyInitFromX509Blob(
	feePubKey pubKey,			// public key 
	unsigned char *keyBlob,
	size_t keyBlobLen);

feeReturn feePubKeyInitFromPKCS8Blob(
	feePubKey pubKey,			// private key 
	unsigned char *keyBlob,
	size_t keyBlobLen);

/*
 * The native OpenSSL ECDSA key format contains both the private and public
 * components in one blob. This throws a bit of a monkey wrench into the API
 * here, as we only have one encoder - which requires a private key - and one
 * decoder, which can result in the decoding of either a public or a private
 * key.
 */
feeReturn feePubKeyCreateOpenSSLBlob(
	feePubKey pubKey,			// private key
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED

feeReturn feePubKeyInitFromOpenSSLBlob(
	feePubKey pubKey,			// private or public key 
	int pubOnly,
	unsigned char *keyBlob,
	size_t keyBlobLen);

#endif	/* CRYPTKIT_DER_ENABLE */

/*
 * ANSI X9.62/Certicom key support.
 * Public key is 04 || x || y
 * Private key is privData per Certicom SEC1 C.4.
 */
feeReturn feeCreateECDSAPubBlob(feePubKey pubKey,
	unsigned char **keyBlob,
	unsigned *keyBlobLen);

feeReturn feeCreateECDSAPrivBlob(feePubKey pubKey,
	unsigned char **keyBlob,
	unsigned *keyBlobLen);

/* Caller determines depth from other sources (e.g. AlgId.Params) */
feeReturn feePubKeyInitFromECDSAPubBlob(feePubKey pubKey,
	const unsigned char *keyBlob,
	unsigned keyBlobLen,
	feeDepth depth);

feeReturn feePubKeyInitFromECDSAPrivBlob(feePubKey pubKey,
	const unsigned char *keyBlob,
	unsigned keyBlobLen,
	feeDepth depth);


/***
 *** Miscellaneous feePubKey functions.
 ***/
 
/* 
 * Given private-capable privKey, initialize pubKey to be its corresponding 
 * public key.
 */
feeReturn feePubKeyInitPubKeyFromPriv(feePubKey privKey,
	feePubKey pubKey);
	
/*
 * Returns non-zero if two keys are equivalent.
 */
int feePubKeyIsEqual(feePubKey key1,
	feePubKey key2);

/*
 * Returns non-zero if key is private-capable (i.e., capable of signing
 * and decrypting).
 */
int feePubKeyIsPrivate(feePubKey key);

#if	CRYPTKIT_KEY_EXCHANGE

/*
 * Generate a pad, for use with symmetric encryption, derived from two keys.
 * 'myKey' must be created with private data (via feePubKeyInitFromPrivData()
 * or feePubKeyInitFromKey().
 */
feeReturn feePubKeyCreatePad(feePubKey myKey,
	feePubKey theirKey,
	unsigned char **padData,	/* fmalloc'd & RETURNED */
	unsigned *padDataLen);		/* RETURNED padData length in bytes */

#endif	/* CRYPTKIT_KEY_EXCHANGE */

#if	CRYPTKIT_HIGH_LEVEL_SIG

/*
 * The following two routines are implemented using primitives in the
 * feeHash and feeDigitalSignature objects.
 *
 * Generate digital signature, ElGamal style.
 */
feeReturn feePubKeyCreateSignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	unsigned char **signature,	/* fmalloc'd and RETURNED */
	unsigned *signatureLen);	/* RETURNED */

/*
 * Verify digital signature, ElGamal style.
 */
feeReturn feePubKeyVerifySignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	const unsigned char *signature,
	unsigned signatureLen);
	
#if CRYPTKIT_ECDSA_ENABLE
    
/*
 * The following two routines are implemented using primitives in the
 * feeHash and feeECDSA objects.
 *
 * Generate digital signature, ECDSA style.
 */
feeReturn feePubKeyCreateECDSASignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	unsigned char **signature,		/* fmalloc'd and RETURNED */
	unsigned *signatureLen);		/* RETURNED */

/*
 * Verify digital signature, ECDSA style.
 */
feeReturn feePubKeyVerifyECDSASignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	const unsigned char *signature,
	unsigned signatureLen);

#endif	/* CRYPTKIT_ECDSA_ENABLE */

#endif	/* CRYPTKIT_HIGH_LEVEL_SIG */

/* 
 * Diffie-Hellman. Public key is specified either as a feePubKey or 
 * a ANSI X9.62 format public key string (0x04 | x | y). In either case
 * the caller must ensure that the two keys are on the same curve. 
 * Output data is falloc'd here; caller must free. Output data is 
 * exactly the size of the curve's modulus in bytes. 
 */
feeReturn feePubKeyECDH(
	feePubKey privKey,
	/* one of the following two is non-NULL */
	feePubKey pubKey,
	const unsigned char *pubKeyStr,
	unsigned pubKeyStrLen,
	/* output fallocd and RETURNED here */
	unsigned char **output,
	unsigned *outputLen);

/*
 * Accessor routines.
 */
const char *feePubKeyAlgorithmName(void);

unsigned feePubKeyBitsize(feePubKey pubKey);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_FEEPUBLICKEY_H_*/
