/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * feeECDSA.c - Elliptic Curve Digital Signature Algorithm (per IEEE 1363)
 *
 * Revision History
 * ----------------
 * 11/27/98	dmitch
 *	Added ECDSA_VERIFY_ONLY dependencies.
 * 10/06/98	ap
 *	Changed to compile with C++.
 *  3 Sep 98	Doug Mitchell at Apple
 *  	Rewrote using projective elliptic algebra, per IEEE P1363.
 * 17 Dec 97	Doug Mitchell at Apple
 *	Fixed c==0 bug in feeECDSAVerify()
 * 16 Jul 97	Doug Mitchell at Apple, based on algorithms by Richard Crandall
 *	Created.
 */

/****
 Nomenclature, per IEEE P1363 D1, Dec. 1997

    G = initial public point = (x1Plus, y1Plus) as usual
    x1OrderPlus = IEEE r = (always prime) order of x1Plus
    f = message to be signed, generally a SHA1 message digest
    s = signer's private key
    W = signer's public key
    * : integer multiplication, as in (x * y)
    'o' : elliptic multiply, as in (u 'o' G)

    Signing algorithm:

    1) Obtain random u in [2, x1OrderPlus-2];
    2) Compute x coordinate, call it c, of u 'o' G  (elliptic mul);
    3) Reduce: c := c mod x1OrderPlus;
    4) If c = 0, goto (1);
    5) Compute u^(-1) (mod x1OrderPlus);
    6) Compute signature s as:

	    d = [u^(-1) (f + (s*c))] (mod x1OrderPlus)

    7) If d = 0, goto (1);
    8) Signature is the integer pair (c, d).  Each integer
	in the pair must be in [1, x1OrderPlus-1].

    Note: therefore a component of the signature could be slightly
    larger than base prime.

    Verification algorithm, given signature (c, d):

    1) Compute h = d^(-1) (mod x1OrderPlus);
    2) Compute h1 = digest as giant integer (skips assigning to 'f' as in
       IEEE spec)
    3) Compute h1 = h1 * h (mod x1OrderPlus)   (i.e., = f * h)
    4) Compute h2 = c * h (mod x1OrderPlus);
    5) Compute h2W = h2 'o' W
    6) Compute h1G = h1 'o' G
    7) Compute elliptic sum of h1G + h2W
    8) If elliptic sum is point at infinity, signature is bad; stop.
    9) cPrime = x coordinate of elliptic sum, mod x1OrderPlus
   10) Signature is good iff cPrime == c.

***********/

#include "ckconfig.h"

#if	CRYPTKIT_ECDSA_ENABLE

#include "feeTypes.h"
#include "feePublicKey.h"
#include "feePublicKeyPrivate.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "feeRandom.h"
#include "curveParams.h"
#include "falloc.h"
#include "ckutilities.h"
#include "feeDebug.h"
#include "platform.h"
#include "byteRep.h"
#include <stdlib.h>
#include "feeECDSA.h"
#include "byteRep.h"
#include "feeDigitalSignature.h"
#include "ECDSA_Profile.h"
#include "ellipticProj.h"
#if	CRYPTKIT_DER_ENABLE
#include "CryptKitDER.h"
#endif

#ifndef	ECDSA_VERIFY_ONLY
static void ECDSA_encode(giant c,
	giant d,
	unsigned char **sigData,		// malloc'd and RETURNED
	unsigned *sigDataLen);			// RETURNED
#endif	/* ECDSA_VERIFY_ONLY */

static feeReturn ECDSA_decode(const unsigned char *sigData,
	size_t sigDataLen,
	giant *gs,						// alloc'd & RETURNED
	giant *x0,						// alloc'd & RETURNED
	unsigned *sigVersion);			// RETURNED


#define ECDSA_DEBUG		0
#if	ECDSA_DEBUG
int	ecdsaDebug=1;			/* tweakable at runtime via debugger */
#define sigDbg(x)		\
	if(ecdsaDebug) {	\
		printf x;	\
	}
#define sigLogGiant(s, g)	\
	if(ecdsaDebug) {	\
		printf(s);	\
		printGiant(g) /*printGiantExp(g)*/;	\
	}
#else	// ECDSA_DEBUG
#define sigDbg(x)
#define sigLogGiant(s, g)
#endif	// ECDSA_DEBUG

#if	ECDSA_PROFILE
/*
 * Profiling accumulators.
 */
unsigned signStep1;
unsigned signStep2;
unsigned signStep34;
unsigned signStep5;
unsigned signStep67;
unsigned signStep8;
unsigned vfyStep1;
unsigned vfyStep3;
unsigned vfyStep4;
unsigned vfyStep5;
unsigned vfyStep6;
unsigned vfyStep7;
unsigned vfyStep8;
#endif	// ECDSA_PROFILE

/*
 * Totally incompatible with feeDigitalSignature.c. Caller must be aware of
 * signature format. We will detect an ElGamal signature, however, and
 * return FR_WrongSignatureType from feeECDSAVerify().
 */
#define FEE_ECDSA_VERSION		2
#define FEE_ECDSA_VERSION_MIN	2

/*
 * When true, use ellMulProjSimple rather than elliptic_simple in
 * sign operation. Using ellMulProjSimple is a *big* win.
 */
#define ECDSA_SIGN_USE_PROJ	1

/*
 * Sign specified block of data (most likely a hash result) using
 * specified private key. Result, an enc64-encoded signature block,
 * is returned in *sigData.
 */

#ifndef	ECDSA_VERIFY_ONLY

feeReturn feeECDSASign(feePubKey pubKey,
	const unsigned char *data,   		// data to be signed
	unsigned dataLen,					// in bytes
	feeRandFcn randFcn,					// optional
	void *randRef,						// optional 
	unsigned char **sigData,			// malloc'd and RETURNED
	unsigned *sigDataLen)				// RETURNED
{
	curveParams 		*cp;

	/* giant integers per IEEE P1363 notation */

	giant 			c;		// both 1363 'c' and 'i'
						// i.e., x-coord of u's pub key
	giant 			d;
	giant 			u;		// random private key
	giant			s;		// private key as giant
	giant			f;		// data (message) as giant

	feeReturn 		frtn = FR_Success;
	feeRand 		frand;
	unsigned char 	*randBytes;
	unsigned		randBytesLen;
	giant			privGiant;
	#if	ECDSA_SIGN_USE_PROJ
	pointProjStruct	pt;		// pt->x = c
	giant			pty;		// pt->y
	giant			ptz;		// pt->z
	#endif	// ECDSA_SIGN_USE_PROJ

	if(pubKey == NULL) {
		return FR_BadPubKey;
	}
	cp = feePubKeyCurveParams(pubKey);
	if(cp == NULL) {
		return FR_BadPubKey;
	}
	if(cp->curveType != FCT_Weierstrass) {
		return FR_IllegalCurve;
	}

	CKASSERT(!isZero(cp->x1OrderPlus));

	/*
	 * Private key and message to be signed as giants
	 */
	privGiant = feePubKeyPrivData(pubKey);
	if(privGiant == NULL) {
		dbgLog(("Attempt to Sign without private data\n"));
		return FR_IllegalArg;
	}
	s = borrowGiant(cp->maxDigits);
	gtog(privGiant, s);
	if(dataLen > (cp->maxDigits * GIANT_BYTES_PER_DIGIT)) {
	    f = borrowGiant(BYTES_TO_GIANT_DIGITS(dataLen));
	}
	else {
	    f = borrowGiant(cp->maxDigits);
	}
	deserializeGiant(data, f, dataLen);

	/* 
	 * Certicom SEC1 states that if the digest is larger than the modulus, 
	 * use the left q bits of the digest. 
	 */
	unsigned hashBits = dataLen * 8;
	if(hashBits > cp->q) {
		gshiftright(hashBits - cp->q, f);
	}

	sigDbg(("ECDSA sign:\n"));
	sigLogGiant("  s        : ", s);
	sigLogGiant("  f        : ", f);

	c = borrowGiant(cp->maxDigits);
	d = borrowGiant(cp->maxDigits);
	u = borrowGiant(cp->maxDigits);
	if(randFcn == NULL) {
		frand = feeRandAlloc();
	}
	else {
		frand = NULL;
	}
	
	/*
	 * Random size is just larger than base prime
	 */
	randBytesLen = (feePubKeyBitsize(pubKey) / 8) + 1;
	randBytes = (unsigned char*) fmalloc(randBytesLen);

	#if	ECDSA_SIGN_USE_PROJ
	/* quick temp pointProj */
	pty = borrowGiant(cp->maxDigits);
	ptz = borrowGiant(cp->maxDigits);
	pt.x = c;
	pt.y = pty;
	pt.z = ptz;
	#endif	// ECDSA_SIGN_USE_PROJ

	while(1) {
		/* Repeat this loop until we have a non-zero c and d */

		/*
		 * 1) Obtain random u in [2, x1OrderPlus-2]
		 */
		SIGPROF_START;
		if(randFcn) {
			randFcn(randRef, randBytes, randBytesLen);
		}
		else {
			feeRandBytes(frand, randBytes, randBytesLen);
		}
		deserializeGiant(randBytes, u, randBytesLen);
		x1OrderPlusJustify(u, cp);
		SIGPROF_END(signStep1);
		sigLogGiant("  u        : ", u);

    		/*
		 * note 'o' indicates elliptic multiply, * is integer mult.
		 *
    		 * 2) Compute x coordinate, call it c, of u 'o' G
		 * 3) Reduce: c := c mod x1OrderPlus;
   		 * 4) If c == 0, goto (1);
		 */
		SIGPROF_START;
		gtog(cp->x1Plus, c);

		#if	ECDSA_SIGN_USE_PROJ

		/* projective coordinates */
		gtog(cp->y1Plus, pty);
		int_to_giant(1, ptz);
		ellMulProjSimple(&pt, u, cp);

		#else	/* ECDSA_SIGN_USE_PROJ */

		/* the FEE way */
		elliptic_simple(c, u, cp);

		#endif	/* ECDSA_SIGN_USE_PROJ */

		SIGPROF_END(signStep2);
		SIGPROF_START;
		x1OrderPlusMod(c, cp);
		SIGPROF_END(signStep34);
		if(isZero(c)) {
			dbgLog(("feeECDSASign: zero modulo (1)\n"));
			continue;
		}

		/*
		 * 5) Compute u^(-1) mod x1OrderPlus;
		 */
		SIGPROF_START;
		gtog(u, d);
		binvg_x1OrderPlus(cp, d);
		SIGPROF_END(signStep5);
		sigLogGiant("  u^(-1)   : ", d);

		/*
		 * 6) Compute signature d as:
	 	 *    d = [u^(-1) (f + s*c)] (mod x1OrderPlus)
		 */
		SIGPROF_START;
		mulg(c, s);	     	// s *= c
		x1OrderPlusMod(s, cp);
		addg(f, s);   		// s := f + (s * c)
		x1OrderPlusMod(s, cp);
		mulg(s, d);	     	// d := u^(-1) (f + (s * c))
		x1OrderPlusMod(d, cp);
		SIGPROF_END(signStep67);

		/*
		 * 7) If d = 0, goto (1);
		 */
		if(isZero(d)) {
			dbgLog(("feeECDSASign: zero modulo (2)\n"));
			continue;
		}
		sigLogGiant("  c        : ", c);
		sigLogGiant("  d        : ", d);
		break;			// normal successful exit
	}

	/*
	 * 8) signature is now the integer pair (c, d).
	 */

	/*
	 * Cook up raw data representing the signature.
	 */
	SIGPROF_START;
	ECDSA_encode(c, d, sigData, sigDataLen);
	SIGPROF_END(signStep8);

	if(frand != NULL) {
		feeRandFree(frand);
	}
	ffree(randBytes);
	returnGiant(u);
	returnGiant(d);
	returnGiant(c);
	returnGiant(f);
	returnGiant(s);
	#if	ECDSA_SIGN_USE_PROJ
	returnGiant(pty);
	returnGiant(ptz);
	#endif	/* ECDSA_SIGN_USE_PROJ */
	return frtn;
}

#endif	/* ECDSA_VERIFY_ONLY */

/*
 * Verify signature for specified data (most likely a hash result) and
 * feePubKey. Returns FR_Success or FR_InvalidSignature.
 */

#define LOG_BAD_SIG	0

feeReturn feeECDSAVerify(const unsigned char *sigData,
	size_t sigDataLen,
	const unsigned char *data,
	unsigned dataLen,
	feePubKey pubKey)
{
	/* giant integers per IEEE P1363 notation */
	giant 		h;			// s^(-1)
	giant		h1;			// f h
	giant		h2;			// c times h
	giant		littleC;		// newGiant from ECDSA_decode
	giant 		littleD;		// ditto
	giant		c;			// borrowed, full size
	giant		d;			// ditto
	giant		cPrime = NULL;		// i mod r
	pointProj	h1G = NULL;		// h1 'o' G
	pointProj	h2W = NULL;		// h2 'o' W
	key		W;			// i.e., their public key

	unsigned	version;
	feeReturn	frtn;
	curveParams	*cp = feePubKeyCurveParams(pubKey);
	int		result;

	if(cp == NULL) {
		return FR_BadPubKey;
	}

	/*
	 * First decode the byteRep string.
	 */
	frtn = ECDSA_decode(sigData,
		sigDataLen,
		&littleC,
		&littleD,
		&version);
	if(frtn) {
		return frtn;
	}

	/*
	 * littleC and littleD have capacity = abs(sign), probably
	 * not big enough....
	 */
	c = borrowGiant(cp->maxDigits);
	d = borrowGiant(cp->maxDigits);
	gtog(littleC, c);
	gtog(littleD, d);
	freeGiant(littleC);
	freeGiant(littleD);

	sigDbg(("ECDSA verify:\n"));

	/*
	 * W = signer's public key
	 */
	W = feePubKeyPlusCurve(pubKey);

	/*
	 * 1) Compute h = d^(-1) (mod x1OrderPlus);
	 */
	SIGPROF_START;
	h = borrowGiant(cp->maxDigits);
	gtog(d, h);
	binvg_x1OrderPlus(cp, h);
	SIGPROF_END(vfyStep1);

	/*
	 * 2) h1 = digest as giant (skips assigning to 'f' in P1363)
	 */
	if(dataLen > (cp->maxDigits * GIANT_BYTES_PER_DIGIT)) {
	    h1 = borrowGiant(BYTES_TO_GIANT_DIGITS(dataLen));
	}
	else {
	    h1 = borrowGiant(cp->maxDigits);
	}
	deserializeGiant(data, h1, dataLen);

	/* 
	 * Certicom SEC1 states that if the digest is larger than the modulus, 
	 * use the left q bits of the digest. 
	 */
	unsigned hashBits = dataLen * 8;
	if(hashBits > cp->q) {
		gshiftright(hashBits - cp->q, h1);
	}
	
	sigLogGiant("  Wx       : ", W->x);
	sigLogGiant("  f        : ", h1);
	sigLogGiant("  c        : ", c);
	sigLogGiant("  d        : ", d);
	sigLogGiant("  s^(-1)   : ", h);

	/*
	 * 3) Compute h1 = f * h mod x1OrderPlus;
	 */
	SIGPROF_START;
	mulg(h, h1);					// h1 := f * h
	x1OrderPlusMod(h1, cp);
	SIGPROF_END(vfyStep3);

	/*
	 * 4) Compute h2 = c * h (mod x1OrderPlus);
	 */
	SIGPROF_START;
	h2 = borrowGiant(cp->maxDigits);
	gtog(c, h2);
	mulg(h, h2);					// h2 := c * h
	x1OrderPlusMod(h2, cp);
	SIGPROF_END(vfyStep4);

     	/*
	 * 5) Compute h2W = h2 'o' W  (W = theirPub)
	 */
	CKASSERT((W->y != NULL) && !isZero(W->y));
	h2W = newPointProj(cp->maxDigits);
	gtog(W->x, h2W->x);
	gtog(W->y, h2W->y);
	int_to_giant(1, h2W->z);
	ellMulProjSimple(h2W, h2, cp);

	/*
	 * 6) Compute h1G = h1 'o' G   (G = {x1Plus, y1Plus, 1} )
	 */
	CKASSERT((cp->y1Plus != NULL) && !isZero(cp->y1Plus));
	h1G = newPointProj(cp->maxDigits);
	gtog(cp->x1Plus, h1G->x);
	gtog(cp->y1Plus, h1G->y);
	int_to_giant(1,  h1G->z);
	ellMulProjSimple(h1G, h1, cp);

	/*
	 * 7) h1G := (h1 'o' G) + (h2  'o' W)
	 */
	ellAddProj(h1G, h2W, cp);

	/*
	 * 8) If elliptic sum is point at infinity, signature is bad; stop.
	 */
	if(isZero(h1G->z)) {
		dbgLog(("feeECDSAVerify: h1 * G = point at infinity\n"));
		result = 1;
		goto vfyDone;
	}
	normalizeProj(h1G, cp);

	/*
	 * 9) cPrime = x coordinate of elliptic sum, mod x1OrderPlus
	 */
	cPrime = borrowGiant(cp->maxDigits);
	gtog(h1G->x, cPrime);
	x1OrderPlusMod(cPrime, cp);

	/*
	 * 10) Good sig iff cPrime == c
	 */
	result = gcompg(c, cPrime);

vfyDone:
	if(result) {
		frtn = FR_InvalidSignature;
		#if	LOG_BAD_SIG
		printf("***yup, bad sig***\n");
		#endif	// LOG_BAD_SIG
	}
	else {
		frtn = FR_Success;
	}

	returnGiant(c);
	returnGiant(d);
	returnGiant(h);
	returnGiant(h1);
	returnGiant(h2);
	if(h1G != NULL) {
		freePointProj(h1G);
	}
	if(h2W != NULL) {
		freePointProj(h2W);
	}
	if(cPrime != NULL) {
		returnGiant(cPrime);
	}
	return frtn;
}

#ifndef	ECDSA_VERIFY_ONLY

/*
 * Encode to/from byteRep.
 */
static void ECDSA_encode(giant c,
	giant d,
	unsigned char **sigData,		// malloc'd and RETURNED
	unsigned *sigDataLen)			// RETURNED
{
	#if	CRYPTKIT_DER_ENABLE
	feeDEREncodeECDSASignature(c, d, sigData, sigDataLen);
	#else
	*sigDataLen = lengthOfByteRepSig(c, d);
	*sigData = (unsigned char*) fmalloc(*sigDataLen);
	sigToByteRep(FEE_ECDSA_MAGIC,
		FEE_ECDSA_VERSION,
		FEE_ECDSA_VERSION_MIN,
		c,
		d,
		*sigData);
	#endif
}

#endif	/* ECDSA_VERIFY_ONLY */

static feeReturn ECDSA_decode(const unsigned char *sigData,
	size_t sigDataLen,
	giant *c,						// alloc'd  & RETURNED
	giant *d,						// alloc'd  & RETURNED
	unsigned *sigVersion)			// RETURNED
{
	#if	CRYPTKIT_DER_ENABLE 
	feeReturn frtn = feeDERDecodeECDSASignature(sigData, sigDataLen, c, d);
	if(frtn == FR_Success) {
		*sigVersion = FEE_ECDSA_VERSION;
	}
	return frtn;
	#else
	int		magic;
	int		minVersion;
	int		rtn;

	rtn = byteRepToSig(sigData,
		sigDataLen,
		FEE_ECDSA_VERSION,
		&magic,
		(int *)sigVersion,
		&minVersion,
		c,
		d);
	if(rtn == 0) {
		return FR_BadSignatureFormat;
	}
	switch(magic) {
	    case FEE_ECDSA_MAGIC:
		return FR_Success;
	    case FEE_SIG_MAGIC:		// ElGamal sig!
	    	return FR_WrongSignatureType;
	    default:
	    	return FR_BadSignatureFormat;
	}
	#endif
}

/*
 * For given key, calculate maximum signature size. 
 */
feeReturn feeECDSASigSize(
	feePubKey pubKey,
	unsigned *maxSigLen)
{
	/* For now, assume that c and d in the signature are 
	 * same size as the key's associated curveParams->basePrime.
	 * We might have to pad this a bit....
	 */
	curveParams	*cp = feePubKeyCurveParams(pubKey);

	if(cp == NULL) {
		return FR_BadPubKey;
	}
	#if	CRYPTKIT_DER_ENABLE
	*maxSigLen = feeSizeOfDERSig(cp->basePrime, cp->basePrime);
	#else
	*maxSigLen = (unsigned)lengthOfByteRepSig(cp->basePrime, cp->basePrime);
	#endif
	return FR_Success;
}

#endif	/* CRYPTKIT_ECDSA_ENABLE */

