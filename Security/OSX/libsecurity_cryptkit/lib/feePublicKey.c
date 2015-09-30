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
 * feePublicKey.c - Portable FEE public key object.
 *
 * Revision History
 * ----------------
 * 11/27/98	dmitch
 *	Added ECDSA_VERIFY_ONLY dependencies.
 * 10/06/98	ap
 *	Changed to compile with C++.
 *  9 Sep 98 at NeXT
 * 	Major changes for IEEE P1363 compliance.
 * 23 Mar 98 at Apple
 *	Added blob support.
 * 21 Jan 98 at Apple
 * 	Fixed feePubKeyBitsize bitlen bug for PT_GENERAL case.
 * 05 Jan 98 at Apple
 *	ECDSA now uses SHA-1 hash. Imcompatible with old ECDSA signatures.
 * 17 Jul 97 at Apple
 *	Added ECDSA signature routines.
 * 12 Jun 97 at Apple
 *	Added feePubKeyInitGiants()
 *	Deleted obsolete code
 *	Changes for lesserX1OrderJustify (was curveOrderJustify)
 * 31 Mar 97 at Apple
 *	Fixed leak in feePubKeyCreateKeyString()
 * 15 Jan 97 at NeXT
 *	PUBLIC_KEY_STRING_VERSION = 3; broke compatibility with all older
 *		versions.
 *	Cleaned up which_curve/index code to use CURVE_MINUS/CURVE_PLUS.
 * 12 Dec 96 at NeXT
 *	Added initFromEnc64KeyStr().
 * 20 Aug 96 at NeXT
 *	Ported to C.
 *  ???? 1994	Blaine Garst at NeXT
 *	Created.
 */

#include "ckconfig.h"
#include "feePublicKey.h"
#include "feePublicKeyPrivate.h"
#include "ckutilities.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "curveParams.h"
#include "falloc.h"
#include "feeTypes.h"
#include "feeDebug.h"
#include "feeHash.h"
#include "ckSHA1.h"
#include "feeDigitalSignature.h"
#include "feeECDSA.h"
#include "platform.h"
#include "enc64.h"
#include "feeDES.h"
#include "byteRep.h"
#if	CRYPTKIT_DER_ENABLE
#include "CryptKitDER.h"
#endif
#include <stdio.h>

/*
 * 11/27/98 dmitch: The ECDSA_VERIFY_ONLY symbol, when #defined, disables all
 * of the code in this module except that which is necessary for ECDSA
 * siggnature verification.
 */
 
#ifndef	NULL
#define NULL ((void *)0)
#endif	// NULL

/*
 * Magic number for a portable key blobs. Must be in sync with static
 * final PUBLIC_KEY_STRING_MAGIC in JavaFee/PublicKey.java.
 */
#define PUBLIC_KEY_BLOB_MAGIC_PUB  		0xfeeddeef
#define PUBLIC_KEY_BLOB_MAGIC_PRIV  	0xfeeddeed
#define PUBLIC_KEY_BLOB_VERSION  		6
#define PUBLIC_KEY_BLOB_MINVERSION		6

#if	CRYPTKIT_DER_ENABLE
#define PUBLIC_DER_KEY_BLOB_VERSION		1
#endif

/*
 * Private data. All "instance" routines are passed a feePubKey (actually
 * a void *) which is actually a pointer to one of these.
 */
typedef struct {
	key			plus;
	key			minus;		// not needed for ECDSA
	curveParams	*cp;		// common params shared by minus, plus
	giant		privGiant;	// private key
} pubKeyInst;

static feeReturn feeGenPrivate(pubKeyInst *pkinst,
	const unsigned char *passwd,
	unsigned passwdLen,
	char hashPasswd);
static pubKeyInst *pubKeyInstAlloc(void);
static void pubKeyInstFree(pubKeyInst *pkinst);
#if		GIANTS_VIA_STACK
static void feePubKeyInitGiants(void);
#endif
static feeReturn createKeyBlob(pubKeyInst *pkinst,
	int isPrivate,			// 0 : public   1 : private
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen);		// RETURNED
static feeReturn feePubKeyInitFromKeyBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	unsigned keyBlobLen);

#pragma mark --- General public API function ---

/*
 * Obatin a newly allocated feePubKey.
 */
feePubKey feePubKeyAlloc(void)
{
	pubKeyInst *pkinst = pubKeyInstAlloc();

	#if		GIANTS_VIA_STACK
	feePubKeyInitGiants();
	#endif
	return pkinst;
}

void feePubKeyFree(feePubKey pubKey)
{
	pubKeyInstFree((pubKeyInst*) pubKey);
}

#ifndef	ECDSA_VERIFY_ONLY
/*
 * Init feePubKey from private key data.
 */
feeReturn feePubKeyInitFromPrivDataKeyBits(feePubKey pubKey,
	const unsigned char *privData,
	unsigned privDataLen,
	unsigned keyBits,			/* key size in bits */
	feePrimeType primeType,		/* FPT_Fefault means "best one" */
	feeCurveType curveType,		/* FCT_Default means "best one" */
	char hashPrivData)
{
	feeReturn frtn;
	feeDepth depth;
	
	frtn = feeKeyBitsToDepth(keyBits, primeType, curveType, &depth);
	if(frtn) {
		return frtn;
	}
	return feePubKeyInitFromPrivDataDepth(pubKey,
		privData, 
		privDataLen,
		depth,
		hashPrivData);
}

feeReturn feePubKeyInitFromPrivDataDepth(feePubKey pubKey,
	const unsigned char *privData,
	unsigned privDataLen,
	feeDepth depth,
	char hashPrivData)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;
	feeReturn  frtn;

	#if	ENGINE_127_BITS
	if(depth != FEE_DEPTH_127_1) {
		dbgLog(("Illegal Depth\n"));
		return FR_IllegalDepth;
	}
	#endif	// ENGINE_127_BITS
	if(depth > FEE_DEPTH_MAX) {
		dbgLog(("Illegal Depth\n"));
		return FR_IllegalDepth;
	}

	pkinst->cp = curveParamsForDepth(depth);
	pkinst->plus  = new_public(pkinst->cp, CURVE_PLUS);
	if(pkinst->cp->x1Minus != NULL) {
		pkinst->minus = new_public(pkinst->cp, CURVE_MINUS);
	}
	/* else only usable for ECDSA */
	
	frtn = feeGenPrivate(pkinst, privData, privDataLen, hashPrivData);
	if(frtn) {
		return frtn;
	}
	set_priv_key_giant(pkinst->plus, pkinst->privGiant);
	if(pkinst->cp->x1Minus != NULL) {
		set_priv_key_giant(pkinst->minus, pkinst->privGiant);
	}
	return FR_Success;
}

#endif	/* ECDSA_VERIFY_ONLY */

/*
 * Init feePubKey from curve parameters matching existing oldKey.
 */
feeReturn feePubKeyInitFromKey(feePubKey pubKey,
	const unsigned char *privData,
	unsigned privDataLen,
	feePubKey oldKey,
	char hashPrivData)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;
	pubKeyInst *oldInst = (pubKeyInst *) oldKey;
	feeReturn  frtn;

	if(oldKey == NULL) {
		dbgLog(("NULL existing key\n"));
		return FR_BadPubKey;
	}

	pkinst->cp = curveParamsCopy(oldInst->cp);
	if(pkinst->cp->x1Minus != NULL) {
		pkinst->minus = new_public(pkinst->cp, CURVE_MINUS);
		if(pkinst->minus == NULL) {
			goto abort;
		}
	}
	/* else this curve only usable for ECDSA */
	
	pkinst->plus = new_public(pkinst->cp, CURVE_PLUS);
	if(pkinst->plus == NULL) {
		goto abort;
	}
	frtn = feeGenPrivate(pkinst, privData, privDataLen, hashPrivData);
	if(frtn) {
		return frtn;
	}
	set_priv_key_giant(pkinst->plus, pkinst->privGiant);
	if(pkinst->cp->x1Minus != NULL) {
		set_priv_key_giant(pkinst->minus, pkinst->privGiant);
	}
	return FR_Success;

abort:
	dbgLog(("Bad Existing Public Key\n"));
	return FR_BadPubKey;
}

/***
 *** Public KeyString support. 
 ***/
/*
 * Init feePubKey from a public key string.
 *
 * See ByteRep.doc for info on the format of the public key string and blobs;
 * PLEASE UPDATE THIS DOCUMENT WHEN YOU MAKE CHANGES TO THE STRING FORMAT.
 */
feeReturn feePubKeyInitFromKeyString(feePubKey pubKey,
	const char *keyStr,
	unsigned keyStrLen)
{
	unsigned char 	*blob = NULL;
	unsigned		blobLen;
	feeReturn		frtn;

	blob = dec64((unsigned char *)keyStr, keyStrLen, &blobLen);
	if(blob == NULL) {
		dbgLog(("Bad Public Key String (not enc64)\n"));
		return FR_BadPubKeyString;
	}
	frtn = feePubKeyInitFromKeyBlob(pubKey, blob, blobLen);
	ffree(blob);
	return frtn;
}

/*
 * Create a public key in the form of a null-terminated C string.
 * This string contains an encoded version of all of our ivars except for
 * privGiant.
 *
 * See ByteRep.doc for info on the format of the public key string and blobs;
 * PLEASE UPDATE THIS DOCUMENT WHEN YOU MAKE CHANGES TO THE STRING FORMAT.
 */
feeReturn feePubKeyCreateKeyString(feePubKey pubKey,
	char **pubKeyString,		/* RETURNED */
	unsigned *pubKeyStringLen)	/* RETURNED */
{
	unsigned char 	*blob;
	unsigned 	blobLen;
	feeReturn 	frtn;
	pubKeyInst 	*pkinst = (pubKeyInst *)pubKey;

	/* get binary pub blob, encode the blob, free the blob */
	frtn = createKeyBlob(pkinst,
		0,		// isPrivate
		&blob,
		&blobLen);
	if(frtn) {
		return frtn;
	}

	*pubKeyString = (char *)enc64(blob, blobLen, pubKeyStringLen);
	ffree(blob);
	return FR_Success;
}

/*** 
 *** Native key blob support.
 ***/
 
#ifndef	ECDSA_VERIFY_ONLY

/*
 * Obtain portable public and private key blobs from a key.
 */
feeReturn feePubKeyCreatePubBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;

	return createKeyBlob(pkinst,
		0,
		keyBlob,
		keyBlobLen);
}

feeReturn feePubKeyCreatePrivBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;

	if(pkinst->privGiant == NULL) {
		return FR_IncompatibleKey;
	}
	return createKeyBlob(pkinst,
		1,
		keyBlob,
		keyBlobLen);
}

/* 
 * Given private-capable privKey, initialize pubKey to be its corresponding 
 * public key.
 */
feeReturn feePubKeyInitPubKeyFromPriv(feePubKey privKey,
	feePubKey pubKey)
{
	pubKeyInst *privInst = (pubKeyInst *)privKey;
	pubKeyInst *pubInst  = (pubKeyInst *)pubKey;

	if((privInst == NULL) || (pubInst == NULL)) {
		return FR_BadPubKey;
	}
	if(privInst->privGiant == NULL) {
		return FR_IncompatibleKey;
	}
	pubInst->cp = curveParamsCopy(privInst->cp);
	if(pubInst == NULL) {
		return FR_Memory;
	}
	pubInst->plus   = new_public_with_key(privInst->plus,  pubInst->cp);
	if(pubInst->plus == NULL) {
		return FR_Memory;
	} 
	if(pubInst->cp->x1Minus != NULL) {
		pubInst->minus  = new_public_with_key(privInst->minus, pubInst->cp);
		if(pubInst->minus == NULL) {
			return FR_Memory;
		} 
	}
	return FR_Success;
}

#endif	/* ECDSA_VERIFY_ONLY */

/*
 * Returns non-zero if two keys are equivalent.
 */
int feePubKeyIsEqual(feePubKey key1, feePubKey key2)
{
	pubKeyInst *pkinst1 = (pubKeyInst *) key1;
	pubKeyInst *pkinst2 = (pubKeyInst *) key2;

	if ((pkinst1 == NULL) || (pkinst2 == NULL)) {
		return 0;
	}
	if((pkinst1->minus != NULL) && (pkinst2->minus != NULL)) {
		if(key_equal(pkinst1->minus, pkinst2->minus) == 0) {
			return 0;
		}
	}
	if(key_equal(pkinst1->plus, pkinst2->plus) == 0) {
		return 0;
	}
	return 1;
}

/*
 * Returns non-zero if key is private-capable (i.e., capable of signing
 * and decrypting).
 */
int feePubKeyIsPrivate(feePubKey key)
{
	pubKeyInst *myPkinst = (pubKeyInst *)key;

	return ((myPkinst->privGiant != NULL) ? 1 : 0);
}

#ifndef	ECDSA_VERIFY_ONLY

#if	CRYPTKIT_KEY_EXCHANGE

feeReturn feePubKeyCreatePad(feePubKey myKey,
	feePubKey theirKey,
	unsigned char **padData,	/* RETURNED */
	unsigned *padDataLen)		/* RETURNED padData length in bytes */
{
	pubKeyInst *myPkinst = (pubKeyInst *) myKey;
	pubKeyInst *theirPkinst = (pubKeyInst *) theirKey;
	giant pad;
	unsigned char *result;
    	unsigned padLen;
	key pkey;

	/*
	 * Do some compatibility checking (myKey, theirKey) here...?
	 */
	if(DEFAULT_CURVE == CURVE_PLUS) {
		pkey = theirPkinst->plus;
	}
	else {
		pkey = theirPkinst->minus;
	}
	pad = make_pad(myPkinst->privGiant, pkey);
	result = mem_from_giant(pad, &padLen);
	freeGiant(pad);

	/*
	 * Ensure we have a the minimum necessary for DES. A bit of a hack,
	 * to be sure.
	 */
	if(padLen >= FEE_DES_MIN_STATE_SIZE) {
		*padData = result;
		*padDataLen = padLen;
	}
	else {
		*padData = (unsigned char*) fmalloc(FEE_DES_MIN_STATE_SIZE);
		*padDataLen = FEE_DES_MIN_STATE_SIZE;
		bzero(*padData, FEE_DES_MIN_STATE_SIZE);
		bcopy(result, *padData, padLen);
		ffree(result);
	}
	return FR_Success;
}

#endif	/* CRYPTKIT_KEY_EXCHANGE */

#if	CRYPTKIT_HIGH_LEVEL_SIG

#warning HLS
/*
 * Generate digital signature, ElGamal style.
 */
feeReturn feePubKeyCreateSignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	unsigned char **signature,	/* fmalloc'd and RETURNED */
	unsigned *signatureLen)		/* RETURNED */
{
	pubKeyInst	*pkinst = (pubKeyInst *) pubKey;
	feeHash 	hash;
	feeSig 		sig;
	unsigned char 	*Pm = NULL;
	unsigned 	PmLen;
	feeReturn	frtn;

	if(pkinst->privGiant == NULL) {
		dbgLog(("feePubKeyCreateSignature: Attempt to Sign without"
			" private data\n"));
		return FR_BadPubKey;
	}
	hash = feeHashAlloc();
	sig = feeSigNewWithKey(pubKey, NULL, NULL);
	if(sig == NULL) {
		/*
		 * Shouldn't happen, but...
		 */
		feeHashFree(hash);
		return FR_BadPubKey;
	}

	/*
	 * Get Pm to salt hash object
	 */
	Pm = feeSigPm(sig, &PmLen);
	feeHashAddData(hash, Pm, PmLen);

	/*
	 * Now hash the data proper, then sign the hash
	 */
	feeHashAddData(hash, data, dataLen);
	frtn = feeSigSign(sig,
		feeHashDigest(hash),
		feeHashDigestLen(),
		pubKey);
	if(frtn == FR_Success) {
		frtn = feeSigData(sig, signature, signatureLen);
	}
	feeHashFree(hash);
	feeSigFree(sig);
	ffree(Pm);
	return frtn;
}

/*
 * Verify digital signature, ElGamal style. If the signature is ECDSA,
 * we'll use that format for compatibility.
 */
feeReturn feePubKeyVerifySignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	const unsigned char *signature,
	unsigned signatureLen)
{
	feeHash 		hash;
	feeSig 			sig;
	unsigned char 	*Pm = NULL;
	unsigned 		PmLen;
	feeReturn		frtn;

	hash = feeHashAlloc();
	frtn = feeSigParse(signature, signatureLen, &sig);
	if(frtn) {
		feeHashFree(hash);
		#if CRYPTKIT_ECDSA_ENABLE
		if(frtn == FR_WrongSignatureType) {
			return feePubKeyVerifyECDSASignature(pubKey,
				data,
				dataLen,
				signature,
				signatureLen);
		}
		#endif	/* CRYPTKIT_ECDSA_ENABLE */
		return frtn;
	}

	/*
	 * Get PM as salt; eat salt, then hash data
	 */
	Pm = feeSigPm(sig, &PmLen);
	feeHashAddData(hash, Pm, PmLen);
	feeHashAddData(hash, data, dataLen);
	frtn = feeSigVerify(sig,
		feeHashDigest(hash),
		feeHashDigestLen(),
		pubKey);

	feeHashFree(hash);
	feeSigFree(sig);
	ffree(Pm);
	return frtn;
}

#pragma mark --- ECDSA signature: high level routines ---

#if	CRYPTKIT_ECDSA_ENABLE
/*
 * Generate digital signature, ECDSA style.
 */
feeReturn feePubKeyCreateECDSASignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	unsigned char **signature,	/* fmalloc'd and RETURNED */
	unsigned *signatureLen)		/* RETURNED */
{
	pubKeyInst	*pkinst = (pubKeyInst *) pubKey;
	sha1Obj 	sha1;
	feeReturn	frtn;

	if(pkinst->privGiant == NULL) {
		dbgLog(("feePubKeyCreateECDSASignature: Attempt to Sign "
			"without private data\n"));
		return FR_BadPubKey;
	}
	sha1 = sha1Alloc();
	sha1AddData(sha1, data, dataLen);
	frtn = feeECDSASign(pubKey,
		sha1Digest(sha1),
		sha1DigestLen(),
		NULL,			// randFcn
		NULL,
		signature,
		signatureLen);
	sha1Free(sha1);
	return frtn;
}
#endif	/* CRYPTKIT_ECDSA_ENABLE */
#endif  /* CRYPTKIT_HIGH_LEVEL_SIG */
#endif	/* ECDSA_VERIFY_ONLY */

#if	CRYPTKIT_HIGH_LEVEL_SIG

#if	CRYPTKIT_ECDSA_ENABLE

/*
 * Verify digital signature, ECDSA style.
 */
feeReturn feePubKeyVerifyECDSASignature(feePubKey pubKey,
	const unsigned char *data,
	unsigned dataLen,
	const unsigned char *signature,
	unsigned signatureLen)
{
	sha1Obj 	sha1;
	feeReturn	frtn;

	sha1 = sha1Alloc();
	sha1AddData(sha1, data, dataLen);
	frtn = feeECDSAVerify(signature,
		signatureLen,
		sha1Digest(sha1),
		sha1DigestLen(),
		pubKey);
	sha1Free(sha1);
	return frtn;
}

#endif	/* CRYPTKIT_ECDSA_ENABLE */

#endif	/* CRYPTKIT_HIGH_LEVEL_SIG */

#pragma mark --- ECDH ---

/* 
 * Diffie-Hellman. Public key is specified either as a feePubKey or 
 * a ANSI X9.62 format public key string (0x04 | x | y). In either case
 * the caller must ensure that the two keys are on the same curve. 
 * Output data is fmalloc'd here; caller must free. Output data is 
 * exactly the size of the curve's modulus in bytes. 
 */
feeReturn feePubKeyECDH(
	feePubKey privKey,
	/* one of the following two is non-NULL */
	feePubKey pubKey,
	const unsigned char *pubKeyStr,
	unsigned pubKeyStrLen,
	/* output fmallocd and RETURNED here */
	unsigned char **output,
	unsigned *outputLen)
{
	feePubKey theirPub = pubKey;
	feeReturn frtn = FR_Success;
	pubKeyInst *privInst = (pubKeyInst *) privKey;
	
	if(privInst->privGiant == NULL) {
		dbgLog(("feePubKeyECDH: privKey not a private key\n"));
		return FR_IncompatibleKey;
	}
	
	if(theirPub == NULL) {
		if(pubKeyStr == NULL) {
			return FR_IllegalArg;
		}
		
		/* Cook up a public key with the same curveParams as the private key */
		feeDepth depth;
		frtn = curveParamsDepth(privInst->cp, &depth);
		if(frtn) {
			return frtn;
		}
		theirPub = feePubKeyAlloc();
		if(theirPub == NULL) {
			return FR_Memory;
		}
		frtn = feePubKeyInitFromECDSAPubBlob(theirPub, pubKeyStr, pubKeyStrLen, depth);
		if(frtn) {
			goto errOut;
		}
	}
	
	pubKeyInst *pubInst = (pubKeyInst *) theirPub;
	
	giant outputGiant = make_pad(privInst->privGiant, pubInst->plus);
	if(outputGiant == NULL) {
		dbgLog(("feePubKeyECDH: make_pad error\n"));
		frtn = FR_Internal;
	}
	else {
		*outputLen = (privInst->cp->q + 7) / 8;
		*output = (unsigned char *)fmalloc(*outputLen);
		if(*output == NULL) {
			frtn = FR_Memory;
			goto errOut;
		}
		serializeGiant(outputGiant, *output, *outputLen);
		freeGiant(outputGiant);
	}
errOut:
	if((pubKey == NULL) && (theirPub != NULL)) {
		feePubKeyFree(theirPub);
	}
	return frtn;
}

#pragma mark --- feePubKey data accessors ---

unsigned feePubKeyBitsize(feePubKey pubKey)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;
	switch(pkinst->cp->primeType) {
		case FPT_General:	/* cp->q is here for just this purpose */
		case FPT_Mersenne:
			return pkinst->cp->q;
		case FPT_FEE:		/* could be larger or smaller than 2^q-1 */
		default:
			return bitlen(pkinst->cp->basePrime);	
	}
	/* NOT REACHED */
	return 0;
}

/*
 * Accessor routines.
 */
/* private only...*/
key feePubKeyPlusCurve(feePubKey pubKey)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;

	return pkinst->plus;
}

key feePubKeyMinusCurve(feePubKey pubKey)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;

	return pkinst->minus;
}

curveParams *feePubKeyCurveParams(feePubKey pubKey)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;

	return pkinst->cp;
}

giant feePubKeyPrivData(feePubKey pubKey)
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;

	return pkinst->privGiant;
}

const char *feePubKeyAlgorithmName(void)
{
	return "Elliptic Curve - FEE by Apple Computer";
}

#pragma mark --- Private functions ---

/*
 * alloc, free pubKeyInst
 */
static pubKeyInst *pubKeyInstAlloc(void)
{
	pubKeyInst *pkinst = (pubKeyInst *) fmalloc(sizeof(pubKeyInst));

	bzero(pkinst, sizeof(pubKeyInst));
	return pkinst;
}

static void pubKeyInstFree(pubKeyInst *pkinst)
{
	if(pkinst->minus) {
		free_key(pkinst->minus);
	}
	if(pkinst->plus) {
		free_key(pkinst->plus);
	}
	if(pkinst->cp) {
		freeCurveParams(pkinst->cp);
	}
	if(pkinst->privGiant) {
		/*
		 * Zero out the private data...
		 */
		clearGiant(pkinst->privGiant);
		freeGiant(pkinst->privGiant);
	}
	ffree(pkinst);
}

#ifndef	ECDSA_VERIFY_ONLY

/*
 * Create a pubKeyInst.privGiant given a password of
 * arbitrary length.
 * Currently, the only error is "private data too short" (FR_IllegalArg).
 */

#define NO_PRIV_MUNGE		0	/* skip this step */

static feeReturn feeGenPrivate(pubKeyInst *pkinst,
	const unsigned char *passwd,
	unsigned passwdLen,
	char hashPasswd)
{
	unsigned 		privLen;			// desired size of pkinst->privData
	feeHash 		*hash = NULL;		// a malloc'd array
	unsigned		digestLen;			// size of MD5 digest
	unsigned 		dataSize;			// min(privLen, passwdLen)
	unsigned		numDigests = 0;
	unsigned		i;
	unsigned char	*cp;
	unsigned		toMove;				// for this digest
	unsigned		moved;				// total digested
	unsigned char	*digest = NULL;
	unsigned char	*privData = NULL;	// temp, before modg(curveOrder)
	giant			corder;				// lesser of two curve orders
	
	/*
	 * generate privData which is just larger than the smaller
	 * curve order.
	 * We'll take the result mod the curve order when we're done.
	 * Note we do *not* have to free corder - it's a pointer to a giant
	 * in pkinst->cp.
	 */
	corder = lesserX1Order(pkinst->cp);
	CKASSERT(!isZero(corder));
	privLen = (bitlen(corder) / 8) + 1;

	if(!hashPasswd) {
		/* 
		 * Caller trusts the incoming entropy. Verify it's big enough and proceed. 
		 */
		if(passwdLen < privLen) {
			return FR_ShortPrivData;
		}
		privLen = passwdLen;
		privData = (unsigned char *)passwd;
		goto finishUp;
	}
	if(passwdLen < 2) {
		return FR_IllegalArg;
	}


	/*
	 * Calculate how many MD5 digests we'll generate.
	 */
	if(privLen > passwdLen) {
		dataSize = passwdLen;
	}
	else {
		dataSize = privLen;
	}
	digestLen = feeHashDigestLen();
	numDigests = (dataSize + digestLen - 1) / digestLen;

	hash = (void**) fmalloc(numDigests * sizeof(feeHash));
	for(i=0; i<numDigests; i++) {
		hash[i] = feeHashAlloc();
	}

	/*
	 * fill digests with passwd data, digestLen (or resid length)
	 * at a time. If (passwdLen > privLen), last digest will hash all
	 * remaining passwd data.
	 */
	cp = (unsigned char *)passwd;
	moved = 0;
	for(i=0; i<numDigests; i++) {
		if(i == (numDigests - 1)) {		    // last digest
		    toMove = passwdLen - moved;
		}
		else {
		    toMove = digestLen;
		}
		feeHashAddData(hash[i], cp, toMove);
		cp += toMove;
		moved += toMove;
	}

	/*
	 * copy digests to privData, up to privLen bytes. Pad with
	 * additional copies of digests if necessary.
	 */
	privData = (unsigned char*) fmalloc(privLen);
	cp = privData;
	moved = 0;
	i = 0;			// digest number
	for(moved=0; moved<privLen; ) {
		if((moved + digestLen) > privLen) {
		   toMove = privLen - moved;
		}
		else {
		   toMove = digestLen;
		}
		digest = feeHashDigest(hash[i++]);
		bcopy(digest, cp, toMove);
		cp += toMove;
		moved += toMove;
		if(i == numDigests) {
		    i = 0;		// wrap to 0, start padding
		}
	}
	
finishUp:
	/*
	 * Convert to giant, justify result to within [2, lesserX1Order]
	 */
	pkinst->privGiant = giant_with_data(privData, privLen);

	#if	FEE_DEBUG
	if(isZero(pkinst->privGiant)) {
		printf("feeGenPrivate: privData = 0!\n");
	}
	#endif	// FEE_DEBUG

	lesserX1OrderJustify(pkinst->privGiant, pkinst->cp);
	if(hashPasswd) {
		memset(privData, 0, privLen);
		ffree(privData);
		for(i=0; i<numDigests; i++) {
			feeHashFree(hash[i]);
		}
		ffree(hash);
	}
	return FR_Success;
}

#endif	/* ECDSA_VERIFY_ONLY */

#if	FEE_DEBUG

void printPubKey(feePubKey pubKey)
{
	pubKeyInst *pkinst = pubKey;

	printf("\ncurveParams:\n");
	printCurveParams(pkinst->cp);
	printf("plus:\n");
	printKey(pkinst->plus);
	printf("minus:\n");
	printKey(pkinst->minus);
	if(pkinst->privGiant != NULL) {
	    printf("privGiant : ");
	    printGiant(pkinst->privGiant);
	}
}

#else	// FEE_DEBUG
void printPubKey(feePubKey pubKey) {}
#endif	// FEE_DEBUG

/*
 * Prime the curveParams and giants modules for quick allocs of giants.
 */
#if		GIANTS_VIA_STACK

static int giantsInitd = 0;

static void feePubKeyInitGiants(void)
{
	if(giantsInitd) {
		return;
	}
	curveParamsInitGiants();
	giantsInitd = 1;
}
#endif

#pragma mark --- Native (custom) key blob formatting ---

/*
 * Exported key blob support. New, 23 Mar 1998.
 *
 * Convert to public or private key blob.
 */

#ifndef	ECDSA_VERIFY_ONLY

/***
 *** Common native blob support 
 ***/
static feeReturn createKeyBlob(pubKeyInst *pkinst,
	int 			isPrivate,		// 0 : public   1 : private
	unsigned char 	**keyBlob,		// mallocd and RETURNED
	unsigned 		*keyBlobLen)	// RETURNED
{
	unsigned char 	*s;		// running ptr into *origS
	unsigned		sLen;
	int				magic;

	/* common blob elements */
	sLen = (4 * sizeof(int)) +		// magic, version, minVersion,
									// spare
	    lengthOfByteRepCurveParams(pkinst->cp);
	if(isPrivate) {
	    /* private only */
	    sLen += lengthOfByteRepGiant(pkinst->privGiant);
	    magic = PUBLIC_KEY_BLOB_MAGIC_PRIV;
	}
	else {
	    /* public only */
	    sLen += (lengthOfByteRepKey(pkinst->plus) +
		     lengthOfByteRepKey(pkinst->minus));
	    magic = PUBLIC_KEY_BLOB_MAGIC_PUB;
	}
	*keyBlob = s = (unsigned char*) fmalloc(sLen);
	s += intToByteRep(magic, s);
	s += intToByteRep(PUBLIC_KEY_BLOB_VERSION, s);
	s += intToByteRep(PUBLIC_KEY_BLOB_MINVERSION, s);
	s += intToByteRep(0, s);			// spare
	s += curveParamsToByteRep(pkinst->cp, s);
	if(isPrivate) {
	    s += giantToByteRep(pkinst->privGiant, s);
	}
	else {
	    /* keyToByteRep writes y for plus curve only */
	    s += keyToByteRep(pkinst->plus, s);
		if(pkinst->minus != NULL) {
			s += keyToByteRep(pkinst->minus, s);
		}
		else {
			/* TBD */
			dbgLog(("work needed here for blobs with no minus key\n"));
		}
	}
	*keyBlobLen = sLen;
	return FR_Success;
}

#endif	/* ECDSA_VERIFY_ONLY */

/*
 * Init an empty feePubKey from a native blob (non-DER format).
 */
static feeReturn feePubKeyInitFromKeyBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	unsigned keyBlobLen)
{
	pubKeyInst		*pkinst = (pubKeyInst *) pubKey;
	unsigned char	*s;		// running pointer
	unsigned		sLen;		// bytes remaining in *s
	int				magic;
	unsigned		len;		// for length of individual components
	int 			minVersion;
	int				version;
	int				isPrivate;

	s = keyBlob;
	sLen = keyBlobLen;
	if(sLen < (4 * sizeof(int))) {	// magic, version, minVersion, spare
		/*
		 * Too short for all the ints we need
		 */
		dbgLog(("feePublicKey: key blob (1)\n"));
		return FR_BadKeyBlob;
	}

	magic = byteRepToInt(s);
	s += sizeof(int);
	sLen -= sizeof(int);
	switch(magic) {
	    case PUBLIC_KEY_BLOB_MAGIC_PUB:
	    	isPrivate = 0;
		break;
	    case PUBLIC_KEY_BLOB_MAGIC_PRIV:
	    	isPrivate = 1;
		break;
	    default:
		dbgLog(("feePublicKey: Bad Public Key Magic Number\n"));
		return FR_BadKeyBlob;
	}

	/*
	 * Switch on this for version-specific cases
	 */
	version = byteRepToInt(s);
	s += sizeof(int);
	sLen -= sizeof(int);

	minVersion = byteRepToInt(s);
	s += sizeof(int);
	sLen -= sizeof(int);
	if(minVersion > PUBLIC_KEY_BLOB_VERSION) {
		/*
		 * old code, newer key blob - can't parse
		 */
		dbgLog(("feePublicKey: Incompatible Public Key (1)\n"));
		return FR_BadKeyBlob;
	}

	s += sizeof(int);			// skip spare
	sLen -= sizeof(int);

	pkinst->cp = byteRepToCurveParams(s, sLen, &len);
	if(pkinst->cp == NULL) {
		dbgLog(("feePublicKey: Bad Key Blob(2)\n"));
		return FR_BadKeyBlob;
	}
	s += len;
	sLen -= len;

	/*
	 * Private key blob: privGiant.
	 * Public Key blob:  plusX, minusX, plusY.
	 */
	if(isPrivate) {
		pkinst->privGiant = byteRepToGiant(s, sLen, &len);
		if(pkinst->privGiant == NULL) {
			dbgLog(("feePublicKey: Bad Key Blob(3)\n"));
			return FR_BadKeyBlob;
		}
		s += len;
		sLen -= len;
	}
	else {
		/* this writes x and y */
		pkinst->plus = byteRepToKey(s,
			sLen,
			CURVE_PLUS,		// twist
			pkinst->cp,
			&len);
		if(pkinst->plus == NULL) {
			dbgLog(("feePublicKey: Bad Key Blob(4)\n"));
			return FR_BadKeyBlob;
		}
		s += len;
		sLen -= len;

		/* this only writes x */
		pkinst->minus = byteRepToKey(s,
			sLen,
			CURVE_MINUS,		// twist
			pkinst->cp,
			&len);
		if(pkinst->minus == NULL) {
			dbgLog(("feePublicKey: Bad Key Blob(5)\n"));
			return FR_BadKeyBlob;
		}
		s += len;
		sLen -= len;
	}

	/*
	 * One more thing: cook up public plusX and minusX for private key
	 * blob case.
	 */
	if(isPrivate) {
		pkinst->plus  = new_public(pkinst->cp, CURVE_PLUS);
		pkinst->minus = new_public(pkinst->cp, CURVE_MINUS);
		set_priv_key_giant(pkinst->plus, pkinst->privGiant);
		set_priv_key_giant(pkinst->minus, pkinst->privGiant);
	}
	return FR_Success;

}

feeReturn feePubKeyInitFromPubBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	unsigned keyBlobLen)
{
	return feePubKeyInitFromKeyBlob(pubKey, keyBlob, keyBlobLen);
}

#ifndef	ECDSA_VERIFY_ONLY

feeReturn feePubKeyInitFromPrivBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	unsigned keyBlobLen)
{
	return feePubKeyInitFromKeyBlob(pubKey, keyBlob, keyBlobLen);	
}

#endif	/* ECDSA_VERIFY_ONLY */

#if	CRYPTKIT_DER_ENABLE
#ifndef	ECDSA_VERIFY_ONLY

/* 
 * DER format support. 
 * Obtain portable public and private DER-encoded key blobs from a key.
 */
feeReturn feePubKeyCreateDERPubBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;

	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	if(pkinst->minus == NULL) {
		/* Only ECDSA key formats supported */
		return FR_IncompatibleKey;
	}
	return feeDEREncodePublicKey(PUBLIC_DER_KEY_BLOB_VERSION,
		pkinst->cp,
		pkinst->plus->x,
		pkinst->minus->x,
		isZero(pkinst->plus->y) ? NULL : pkinst->plus->y, 
		keyBlob,
		keyBlobLen);
}

feeReturn feePubKeyCreateDERPrivBlob(feePubKey pubKey,
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;

	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	if(pkinst->privGiant == NULL) {
		return FR_IncompatibleKey;
	}
	if(pkinst->minus == NULL) {
		/* Only ECDSA key formats supported */
		return FR_IncompatibleKey;
	}
	return feeDEREncodePrivateKey(PUBLIC_DER_KEY_BLOB_VERSION,
		pkinst->cp,
		pkinst->privGiant,
		keyBlob,
		keyBlobLen);
}

#endif	/* ECDSA_VERIFY_ONLY */

/*
 * Init an empty feePubKey from a DER-encoded blob, public and private key versions. 
 */
feeReturn feePubKeyInitFromDERPubBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	size_t keyBlobLen)
{
	pubKeyInst	*pkinst = (pubKeyInst *) pubKey;
	feeReturn	frtn;
	int			version;
	
	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	
	/* kind of messy, maybe we should clean this up. But new_public() does too
	 * much - e.g., it allocates the x and y which we really don't want */
	 memset(pkinst, 0, sizeof(pubKeyInst));
	 pkinst->plus = (key) fmalloc(sizeof(keystruct));
	 pkinst->minus = (key) fmalloc(sizeof(keystruct));
	 if((pkinst->plus == NULL) || (pkinst->minus == NULL)) {
		return FR_Memory;
	 }
	 memset(pkinst->plus, 0, sizeof(keystruct));
	 memset(pkinst->minus, 0, sizeof(keystruct));
	 pkinst->cp = NULL;
	 pkinst->privGiant = NULL;
	 pkinst->plus->twist  = CURVE_PLUS;
	 pkinst->minus->twist = CURVE_MINUS;
	 frtn = feeDERDecodePublicKey(keyBlob, 
		(unsigned)keyBlobLen,
		&version,			// currently unused
		&pkinst->cp,
		&pkinst->plus->x,
		&pkinst->minus->x,
		&pkinst->plus->y);
	if(frtn) {
		return frtn;
	}
	/* minus curve, y is not used */
	pkinst->minus->y = newGiant(1);
	int_to_giant(0, pkinst->minus->y);
	pkinst->plus->cp = pkinst->minus->cp = pkinst->cp;
	return FR_Success;
}

#ifndef	ECDSA_VERIFY_ONLY

feeReturn feePubKeyInitFromDERPrivBlob(feePubKey pubKey,
	unsigned char *keyBlob,
	size_t keyBlobLen)
{
	pubKeyInst	*pkinst = (pubKeyInst *) pubKey;
	int			version;
	feeReturn	frtn;
	
	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	memset(pkinst, 0, sizeof(pubKeyInst));
	frtn = feeDERDecodePrivateKey(keyBlob, 
		(unsigned)keyBlobLen,
		&version,		// currently unused
		&pkinst->cp,
		&pkinst->privGiant);
	if(frtn) {
		return frtn;
	}
	
	/* since this blob only had the private data, infer the remaining fields */
	pkinst->plus  = new_public(pkinst->cp, CURVE_PLUS);
	pkinst->minus = new_public(pkinst->cp, CURVE_MINUS);
	set_priv_key_giant(pkinst->plus, pkinst->privGiant);
	set_priv_key_giant(pkinst->minus, pkinst->privGiant);
	return FR_Success;
}

#endif	/* ECDSA_VERIFY_ONLY */

#pragma mark --- X509 (public) and PKCS8 (private) key formatting ---

feeReturn feePubKeyCreateX509Blob(
	feePubKey pubKey,			// public key
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;
	unsigned char *xyStr = NULL;
	unsigned xyStrLen = 0;
	feeReturn frtn = feeCreateECDSAPubBlob(pubKey, &xyStr, &xyStrLen);
	if(frtn) {
		return frtn;
	}
	frtn = feeDEREncodeX509PublicKey(xyStr, xyStrLen, pkinst->cp, keyBlob, keyBlobLen);
	ffree(xyStr);
	return frtn;
}

feeReturn feePubKeyCreatePKCS8Blob(
	feePubKey pubKey,			// private key
	unsigned char **keyBlob,	// mallocd and RETURNED
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;
	unsigned char *privStr = NULL;
	unsigned privStrLen = 0;
	feeReturn frtn = feeCreateECDSAPrivBlob(pubKey, &privStr, &privStrLen);
	if(frtn) {
		return frtn;
	}
	unsigned char *pubStr = NULL;
	unsigned pubStrLen = 0;
	frtn = feeCreateECDSAPubBlob(pubKey, &pubStr, &pubStrLen);
	if(frtn) {
		goto errOut;
	}
	frtn = feeDEREncodePKCS8PrivateKey(privStr, privStrLen, 
		pubStr, pubStrLen,
		pkinst->cp, keyBlob, keyBlobLen);
errOut:
	if(privStr) {
		ffree(privStr);
	}
	if(pubStr) {
		ffree(pubStr);
	}
	return frtn;
}

feeReturn feePubKeyInitFromX509Blob(
	feePubKey pubKey,			// public key 
	unsigned char *keyBlob,
	size_t keyBlobLen)
{
	feeDepth depth;
	unsigned char *xyStr = NULL;
	unsigned xyStrLen = 0;
	
	/* obtain x/y and depth from X509 encoding */
	feeReturn frtn = feeDERDecodeX509PublicKey(keyBlob, (unsigned)keyBlobLen, &depth,
		&xyStr, &xyStrLen);
	if(frtn) {
		return frtn;
	}
	
	frtn = feePubKeyInitFromECDSAPubBlob(pubKey, xyStr, xyStrLen, depth);
	ffree(xyStr);
	return frtn;
}


feeReturn feePubKeyInitFromPKCS8Blob(
	feePubKey pubKey,			// private key 
	unsigned char *keyBlob,
	size_t keyBlobLen)
{
	feeDepth depth;
	unsigned char *privStr = NULL;
	unsigned privStrLen = 0;
	
	/* obtain x/y and depth from PKCS8 encoding */
	/* For now we ignore the possible public key string */
	feeReturn frtn = feeDERDecodePKCS8PrivateKey(keyBlob, (unsigned)keyBlobLen, &depth,
		&privStr, &privStrLen, NULL, NULL);
	if(frtn) {
		return frtn;
	}
	
	frtn = feePubKeyInitFromECDSAPrivBlob(pubKey, privStr, privStrLen, depth);
	ffree(privStr);
	return frtn;
}

#pragma mark --- OpenSSL key formatting ---

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
	unsigned *keyBlobLen)		// RETURNED
{
	pubKeyInst *pkinst = (pubKeyInst *) pubKey;
	unsigned char *privStr = NULL;
	unsigned privStrLen = 0;
	feeReturn frtn = feeCreateECDSAPrivBlob(pubKey, &privStr, &privStrLen);
	if(frtn) {
		return frtn;
	}
	unsigned char *pubStr = NULL;
	unsigned pubStrLen = 0;
	frtn = feeCreateECDSAPubBlob(pubKey, &pubStr, &pubStrLen);
	if(frtn) {
		goto errOut;
	}
	frtn = feeDEREncodeOpenSSLPrivateKey(privStr, privStrLen, 
		pubStr, pubStrLen,
		pkinst->cp, keyBlob, keyBlobLen);
errOut:
	if(privStr) {
		ffree(privStr);
	}
	if(pubStr) {
		ffree(pubStr);
	}
	return frtn;
}

feeReturn feePubKeyInitFromOpenSSLBlob(
	feePubKey pubKey,			// private or public key 
	int pubOnly,
	unsigned char *keyBlob,
	size_t keyBlobLen)
{
	feeDepth depth;
	unsigned char *privStr = NULL;
	unsigned privStrLen = 0;
	unsigned char *pubStr = NULL;
	unsigned pubStrLen = 0;
	
	/* obtain x/y, public bit string, and depth from PKCS8 encoding */
	feeReturn frtn = feeDERDecodeOpenSSLKey(keyBlob, (unsigned)keyBlobLen, &depth,
		&privStr, &privStrLen, &pubStr, &pubStrLen);
	if(frtn) {
		return frtn;
	}
	
	if(pubOnly) {
		frtn = feePubKeyInitFromECDSAPubBlob(pubKey, pubStr, pubStrLen, depth);
	}
	else {
		frtn = feePubKeyInitFromECDSAPrivBlob(pubKey, privStr, privStrLen, depth);
	}
	if(privStr) {
		ffree(privStr);
	}
	if(pubStr) {
		ffree(pubStr);
	}
	return frtn;
}

#endif	/* CRYPTKIT_DER_ENABLE */

/*
 * ANSI X9.62/Certicom key support.
 * Public key is 04 || x || y
 * Private key is privData per Certicom SEC1 C.4.
 */
feeReturn feeCreateECDSAPubBlob(feePubKey pubKey,
	unsigned char **keyBlob,
	unsigned *keyBlobLen)
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;
	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	
	unsigned giantBytes = (pkinst->cp->q + 7) / 8;
	unsigned blobSize = 1 + (2 * giantBytes);
	unsigned char *blob = fmalloc(blobSize);
	if(blob == NULL) {
		return FR_Memory;
	}
	*blob = 0x04;
	serializeGiant(pkinst->plus->x, blob+1, giantBytes);
	serializeGiant(pkinst->plus->y, blob+1+giantBytes, giantBytes);
	*keyBlob = blob;
	*keyBlobLen = blobSize;
	return FR_Success;
}

feeReturn feeCreateECDSAPrivBlob(feePubKey pubKey,
	unsigned char **keyBlob,
	unsigned *keyBlobLen)
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;
	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	if(pkinst->privGiant == NULL) {
		return FR_IncompatibleKey;
	}

	/* 
	 * Return the raw private key bytes padded with zeroes in
	 * the m.s. end to fill exactly one prime-size byte array.
	 */
	unsigned giantBytes = (pkinst->cp->q + 7) / 8;
	unsigned char *blob = fmalloc(giantBytes);
	if(blob == NULL) {
		return FR_Memory;
	}
	serializeGiant(pkinst->privGiant, blob, giantBytes);
	*keyBlob = blob;
	*keyBlobLen = giantBytes;
	return FR_Success;
}

/* Caller determines depth from other sources (e.g. AlgId.Params) */
feeReturn feePubKeyInitFromECDSAPubBlob(feePubKey pubKey,
	const unsigned char *keyBlob,
	unsigned keyBlobLen,
	feeDepth depth)
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;
	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	curveParams *cp = curveParamsForDepth(depth);
	if(cp == NULL) {
		return FR_IllegalDepth;
	}
	unsigned giantBytes = (cp->q + 7) / 8;
	unsigned blobSize = 1 + (2 * giantBytes);
	if(keyBlobLen != blobSize) {
		dbgLog(("feePubKeyInitFromECDSAPubBlob: bad blobLen\n"));
		return FR_BadKeyBlob;
	}
	if(*keyBlob != 0x04) {
		dbgLog(("feePubKeyInitFromECDSAPubBlob: bad blob leader\n"));
		return FR_BadKeyBlob;
	}
	
	pkinst->cp = cp;
	pkinst->plus = new_public(cp, CURVE_PLUS);
	deserializeGiant(keyBlob+1, pkinst->plus->x, giantBytes);
	deserializeGiant(keyBlob+1+giantBytes, pkinst->plus->y, giantBytes);
	return FR_Success;
}

feeReturn feePubKeyInitFromECDSAPrivBlob(feePubKey pubKey,
	const unsigned char *keyBlob,
	unsigned keyBlobLen,
	feeDepth depth)
{
	pubKeyInst *pkinst = (pubKeyInst *)pubKey;
	if(pkinst == NULL) {
		return FR_BadPubKey;
	}
	curveParams *cp = curveParamsForDepth(depth);
	if(cp == NULL) {
		return FR_IllegalDepth;
	}
	unsigned giantDigits = cp->basePrime->sign;
	unsigned giantBytes = (cp->q + 7) / 8;

	/* 
	 * The specified private key can be one byte smaller than the modulus */
	if((keyBlobLen > giantBytes) || (keyBlobLen < (giantBytes - 1))) {
		dbgLog(("feePubKeyInitFromECDSAPrivBlob: bad blobLen\n"));
		return FR_BadKeyBlob;
	}
	
	pkinst->cp = cp;
	
	/* cook up a new private giant */
	pkinst->privGiant = newGiant(giantDigits);
	if(pkinst->privGiant == NULL) {
		return FR_Memory;
	}
	deserializeGiant(keyBlob, pkinst->privGiant, keyBlobLen);

	/* since this blob only had the private data, infer the remaining fields */
	pkinst->plus  = new_public(pkinst->cp, CURVE_PLUS);
	set_priv_key_giant(pkinst->plus, pkinst->privGiant);
	return FR_Success;
}

