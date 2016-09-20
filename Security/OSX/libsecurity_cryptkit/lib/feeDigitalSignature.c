/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * feeDigitalSignature.c
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 *  9 Sep 98 at NeXT
 * 	Major changes to use projective elliptic algebra for
 *		Weierstrass curves.
 * 15 Jan 97 at NeXT
 *	FEE_SIG_VERSION = 3 (removed code for compatibilty with all older
 *		versions).
 *	Was modg(), is curveOrderJustify()
 *	Use plus curve for ellipic algebra per IEEE standards
 * 22 Aug 96 at NeXT
 *	Ported guts of Blaine Garst's NSFEEDigitalSignature.m to C.
 */

#include "ckconfig.h"
#include "feeTypes.h"
#include "feePublicKey.h"
#include "feePublicKeyPrivate.h"
#include "feeDigitalSignature.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "feeRandom.h"
#include "curveParams.h"
#include "falloc.h"
#include "ckutilities.h"
#include "feeDebug.h"
#include "platform.h"
#include "byteRep.h"
#include "feeECDSA.h"
#if	CRYPTKIT_DER_ENABLE
#include "CryptKitDER.h"
#endif

#include <stdlib.h>
#include "ellipticProj.h"

#define SIG_DEBUG		0
#if	SIG_DEBUG
int	sigDebug=1;		// tweakable at runtime via debugger
#endif	// SIG_DEBUG

#define SIG_CURVE 		DEFAULT_CURVE

/*
 * true : justify randGiant to [2, x1OrderPlus-2]
 * false : no truncate or mod of randGiant
 */
#define RAND_JUST_X1_ORDER_PLUS	1

#define FEE_SIG_VERSION		4
#define FEE_SIG_VERSION_MIN	4

#ifndef	max
#define max(a,b) ((a)>(b)? (a) : (b))
#endif	// max

typedef struct {
	giant		PmX;		// m 'o' P1; m = random
	#if	CRYPTKIT_ELL_PROJ_ENABLE
	giant		PmY;		// y-coord of m 'o' P1 if we're
					//  using projective coords
	#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */

	giant		u;
	giant		randGiant;	// random m as giant - only known
					//  when signing
} sigInst;

static sigInst *sinstAlloc()
{
	sigInst *sinst = (sigInst*) fmalloc(sizeof(sigInst));

	bzero(sinst, sizeof(sigInst));
	return sinst;
}

/*
 * Create new feeSig object, including a random large integer 'randGiant' for
 * possible use in salting a feeHash object, and 'PmX', equal to
 * randGiant 'o' P1. Note that this is not called when *verifying* a
 * signature, only when signing.
 */
feeSig feeSigNewWithKey(
	feePubKey 		pubKey,
	feeRandFcn		randFcn,		/* optional */
	void			*randRef)
{
	sigInst 	*sinst = sinstAlloc();
	feeRand 	frand;
	unsigned char 	*randBytes;
	unsigned	randBytesLen;
	curveParams	*cp;

	if(pubKey == NULL) {
		return NULL;
	}
	cp = feePubKeyCurveParams(pubKey);
	if(cp == NULL) {
		return NULL;
	}

	/*
	 * Generate random m, a little larger than key size, save as randGiant
	 */
	randBytesLen = (feePubKeyBitsize(pubKey) / 8) + 1 + 8; // +8bytes (64bits) to reduce the biais when with reduction mod prime. Per FIPS186-4 - "Using Extra Random Bits"
	randBytes = (unsigned char*) fmalloc(randBytesLen);
	if(randFcn) {
		randFcn(randRef, randBytes, randBytesLen);
	}
	else {
		frand = feeRandAlloc();
		feeRandBytes(frand, randBytes, randBytesLen);
		feeRandFree(frand);
	}
	sinst->randGiant = giant_with_data(randBytes, randBytesLen);
	memset(randBytes, 0, randBytesLen);
	ffree(randBytes);

	#if	FEE_DEBUG
	if(isZero(sinst->randGiant)) {
		printf("feeSigNewWithKey: randGiant = 0!\n");
	}
	#endif	// FEE_DEBUG

	/*
	 * Justify randGiant to be in [2, x1OrderPlus]
	 */
	x1OrderPlusJustify(sinst->randGiant, cp);

	/* PmX := randGiant 'o' P1 */
	sinst->PmX = newGiant(cp->maxDigits);

	#if 	CRYPTKIT_ELL_PROJ_ENABLE

	if(cp->curveType == FCT_Weierstrass) {

		pointProjStruct pt0;

		sinst->PmY = newGiant(cp->maxDigits);

		/* cook up pt0 as P1 */
		pt0.x = sinst->PmX;
		pt0.y = sinst->PmY;
		pt0.z = borrowGiant(cp->maxDigits);
		gtog(cp->x1Plus, pt0.x);
		gtog(cp->y1Plus, pt0.y);
		int_to_giant(1, pt0.z);

		/* pt0 := P1 'o' randGiant */
		ellMulProjSimple(&pt0, sinst->randGiant, cp);

		returnGiant(pt0.z);
	}
	else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
		if(SIG_CURVE == CURVE_PLUS) {
			gtog(cp->x1Plus, sinst->PmX);
		}
		else {
			gtog(cp->x1Minus, sinst->PmX);
		}
#pragma clang diagnostic pop
		elliptic_simple(sinst->PmX, sinst->randGiant, cp);
	}
	#else	/* CRYPTKIT_ELL_PROJ_ENABLE */

	if(SIG_CURVE == CURVE_PLUS) {
		gtog(cp->x1Plus, sinst->PmX);
	}
	else {
		gtog(cp->x1Minus, sinst->PmX);
	}
	elliptic_simple(sinst->PmX, sinst->randGiant, cp);

	#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */

	return sinst;
}

void feeSigFree(feeSig sig)
{
	sigInst *sinst = (sigInst*) sig;

	if(sinst->PmX) {
		clearGiant(sinst->PmX);
		freeGiant(sinst->PmX);
	}
	#if 	CRYPTKIT_ELL_PROJ_ENABLE
	if(sinst->PmY) {
		clearGiant(sinst->PmY);
		freeGiant(sinst->PmY);
	}
	#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */
	if(sinst->u) {
		clearGiant(sinst->u);
		freeGiant(sinst->u);
	}
	if(sinst->randGiant) {
		clearGiant(sinst->randGiant);
		freeGiant(sinst->randGiant);
	}
	ffree(sinst);
}

/*
 * Obtain Pm after feeSigNewWithKey() or feeSigParse()
 */
unsigned char *feeSigPm(feeSig sig,
	unsigned *PmLen)
{
	sigInst *sinst = (sigInst*) sig;
	unsigned char *Pm;

	if(sinst->PmX == NULL) {
		dbgLog(("feeSigPm: no PmX!\n"));
		return NULL;
	}
	else {
		Pm = mem_from_giant(sinst->PmX, PmLen);
		#if	SIG_DEBUG
		if(sigDebug)
		{
		    int i;

		    printf("Pm : "); printGiant(sinst->PmX);
		    printf("PmData: ");
		    for(i=0; i<*PmLen; i++) {
		        printf("%x:", Pm[i]);
		    }
		    printf("\n");
		}
		#endif	// SIG_DEBUG
	}
	return Pm;
}

/*
 * Sign specified block of data (most likely a hash result) using
 * specified feePubKey.
 */
feeReturn feeSigSign(feeSig sig,
	const unsigned char *data,   		// data to be signed
	unsigned dataLen,			// in bytes
	feePubKey pubKey)
{
	sigInst 		*sinst = (sigInst*) sig;
	giant 			messageGiant = NULL;
	unsigned 		maxlen;
	giant 			privGiant;
	unsigned		privGiantBytes;
	feeReturn 		frtn = FR_Success;
	unsigned		randBytesLen;
	unsigned		uDigits;	// alloc'd digits in sinst->u
	curveParams		*cp;

	if(pubKey == NULL) {
		return FR_BadPubKey;
	}
	cp = feePubKeyCurveParams(pubKey);
	if(cp == NULL) {
		return FR_BadPubKey;
	}
	
	privGiant = feePubKeyPrivData(pubKey);
	if(privGiant == NULL) {
		dbgLog(("Attempt to Sign without private data\n"));
		frtn = FR_IllegalArg;
		goto abort;
	}
	privGiantBytes = abs(privGiant->sign) * GIANT_BYTES_PER_DIGIT;

	/*
	 * Note PmX = m 'o' P1.
	 * Get message/digest as giant. May be significantly different
	 * in size from pubKey's basePrime.
	 */
	messageGiant = giant_with_data(data, dataLen);	    // M(text)
	randBytesLen = feePubKeyBitsize(pubKey) / 8;
	maxlen = max(randBytesLen, dataLen);

	/* leave plenty of room.... */
	uDigits = (3 * (privGiantBytes + maxlen)) / GIANT_BYTES_PER_DIGIT;
	sinst->u = newGiant(uDigits);
	gtog(privGiant, sinst->u);			    // u := ourPri
	mulg(messageGiant, sinst->u);			    // u *= M(text)
	addg(sinst->randGiant, sinst->u);		    // u += m

	/*
	 * Paranoia: we're using the curveParams from the caller's pubKey;
	 * this cp will have a valid x1OrderPlusRecip if pubKey is the same
	 * as the one passed to feeSigNewWithKey() (since feeSigNewWithKey
	 * called x1OrderPlusJustify()). But the caller could conceivably be
	 * using a different instance of their pubKey, in which case
	 * the key's cp->x1OrderPlusRecip may not be valid.
	 */
	calcX1OrderPlusRecip(cp);

	/* u := u mod x1OrderPlus */
	#if	SIG_DEBUG
	if(sigDebug) {
		printf("sigSign:\n");
		printf("u pre-modg  : ");
		printGiant(sinst->u);
	}
	#endif
	modg_via_recip(cp->x1OrderPlus, cp->x1OrderPlusRecip, sinst->u);

	#if	SIG_DEBUG
	if(sigDebug) {
		printf("privGiant   : ");
		printGiant(privGiant);
		printf("u           : ");
		printGiant(sinst->u);
		printf("messageGiant: ");
		printGiant(messageGiant);
		printf("curveParams :\n");
		printCurveParams(cp);
	}
	#endif	// SIG_DEBUG
abort:
	if(messageGiant) {
		freeGiant(messageGiant);
	}
	return frtn;
}

/*
 * Given a feeSig processed by feeSigSign, obtain a malloc'd byte
 * array representing the signature.
 * See ByteRep.doc for info on the format of the signature string;
 * PLEASE UPDATE THIS DOCUMENT WHEN YOU MAKE CHANGES TO THE STRING FORMAT.
 */
feeReturn feeSigData(feeSig sig,
	unsigned char **sigData,		// IGNORED....malloc'd and RETURNED
	unsigned *sigDataLen)			// RETURNED
{
	sigInst  *sinst = (sigInst*) sig;

	#if		CRYPTKIT_DER_ENABLE
	return feeDEREncodeElGamalSignature(sinst->u, sinst->PmX, sigData, sigDataLen);
	#else
	*sigDataLen = lengthOfByteRepSig(sinst->u, sinst->PmX);
	*sigData = (unsigned char*) fmalloc(*sigDataLen);
	sigToByteRep(FEE_SIG_MAGIC,
		FEE_SIG_VERSION,
		FEE_SIG_VERSION_MIN,
		sinst->u,
		sinst->PmX,
		*sigData);
	return FR_Success;
	#endif
}

/*
 * Obtain a feeSig object by parsing an existing signature block.
 * Note that if Pm is used to salt a hash of the signed data, this must
 * function must be called prior to hashing.
 */
feeReturn feeSigParse(const unsigned char *sigData,
	size_t sigDataLen,
	feeSig *sig)				// RETURNED
{
	sigInst		*sinst = NULL;
	feeReturn	frtn;
	#if	!CRYPTKIT_DER_ENABLE
	int			version;
	int			magic;
	int			minVersion;
	int			rtn;
	#endif
	
	sinst = sinstAlloc();
	#if		CRYPTKIT_DER_ENABLE
	frtn = feeDERDecodeElGamalSignature(sigData, sigDataLen, &sinst->u, &sinst->PmX);
	if(frtn) {
		goto abort;
	}
	#else
	rtn = byteRepToSig(sigData,
		sigDataLen,
		FEE_SIG_VERSION,
		&magic,
		&version,
		&minVersion,
		&sinst->u,
		&sinst->PmX);
	if(rtn == 0) {
		frtn = FR_BadSignatureFormat;
		goto abort;
	}
	switch(magic) {
	    case FEE_ECDSA_MAGIC:
	    	frtn = FR_WrongSignatureType;		// ECDSA!
		goto abort;
	    case FEE_SIG_MAGIC:
	    	break;					// proceed
	    default:
	    	frtn = FR_BadSignatureFormat;
		goto abort;
	}
	#endif		/* CRYPTKIT_DER_ENABLE */
	
	#if	SIG_DEBUG
	if(sigDebug) {
		printf("sigParse: \n");
		printf("u: ");
		printGiant(sinst->u);
	}
	#endif	// SIG_DEBUG

	*sig = sinst;
	return FR_Success;

abort:
	if(sinst) {
		feeSigFree(sinst);
	}
	return frtn;
}

/*
 * Verify signature, obtained via feeSigParse, for specified
 * data (most likely a hash result) and feePubKey. Returns non-zero if
 * signature valid.
 */

#define LOG_BAD_SIG	0

#if	CRYPTKIT_ELL_PROJ_ENABLE

feeReturn feeSigVerifyNoProj(feeSig sig,
	const unsigned char *data,
	unsigned dataLen,
	feePubKey pubKey);

static void borrowPointProj(pointProj pt, unsigned maxDigits)
{
	pt->x = borrowGiant(maxDigits);
	pt->y = borrowGiant(maxDigits);
	pt->z = borrowGiant(maxDigits);
}

static void returnPointProj(pointProj pt)
{
	returnGiant(pt->x);
	returnGiant(pt->y);
	returnGiant(pt->z);
}

feeReturn feeSigVerify(feeSig sig,
	const unsigned char *data,
	unsigned dataLen,
	feePubKey pubKey)
{
	pointProjStruct Q;
	giant 		messageGiant = NULL;
	pointProjStruct	scratch;
	sigInst 	*sinst = (sigInst*) sig;
	feeReturn	frtn;
	curveParams	*cp;
	key		origKey;		// may be plus or minus key

	if(sinst->PmX == NULL) {
		dbgLog(("sigVerify without parse!\n"));
		return FR_IllegalArg;
	}

	cp = feePubKeyCurveParams(pubKey);
	if(cp->curveType != FCT_Weierstrass) {
		return feeSigVerifyNoProj(sig, data, dataLen, pubKey);
	}

	borrowPointProj(&Q, cp->maxDigits);
	borrowPointProj(&scratch, cp->maxDigits);

	/*
	 * Q := P1
	 */
	gtog(cp->x1Plus, Q.x);
	gtog(cp->y1Plus, Q.y);
	int_to_giant(1, Q.z);

	messageGiant = 	giant_with_data(data, dataLen);	// M(ciphertext)

	/* Q := u 'o' P1 */
	ellMulProjSimple(&Q, sinst->u, cp);

	/* scratch := theirPub */
	origKey = feePubKeyPlusCurve(pubKey);
	gtog(origKey->x, scratch.x);
	gtog(origKey->y, scratch.y);
	int_to_giant(1, scratch.z);

	#if	SIG_DEBUG
	if(sigDebug) {
		printf("verify origKey:\n");
		printKey(origKey);
		printf("messageGiant: ");
		printGiant(messageGiant);
		printf("curveParams:\n");
		printCurveParams(cp);
	}
	#endif	// SIG_DEBUG

	/* scratch := M 'o' theirPub */
	ellMulProjSimple(&scratch, messageGiant, cp);

	#if	SIG_DEBUG
	if(sigDebug) {
		printf("signature_compare, with\n");
		printf("p0 = Q:\n");
		printGiant(Q.x);
		printf("p1 = Pm:\n");
		printGiant(sinst->PmX);
		printf("p2 = scratch = R:\n");
		printGiant(scratch.x);
	}
	#endif	// SIG_DEBUG

	if(signature_compare(Q.x, sinst->PmX, scratch.x, cp)) {

		frtn = FR_InvalidSignature;
		#if	LOG_BAD_SIG
		printf("***yup, bad sig***\n");
		#endif	// LOG_BAD_SIG
	}
	else {
		frtn = FR_Success;
	}
	freeGiant(messageGiant);

    	returnPointProj(&Q);
    	returnPointProj(&scratch);
	return frtn;
}

#else	/* CRYPTKIT_ELL_PROJ_ENABLE */

#define feeSigVerifyNoProj(s, d, l, k) feeSigVerify(s, d, l, k)

#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */

/*
 * FEE_SIG_USING_PROJ true  : this is the "no Weierstrass" case
 * feeSigVerifyNoProj false : this is redefined to feeSigVerify
 */
feeReturn feeSigVerifyNoProj(feeSig sig,
	const unsigned char *data,
	unsigned dataLen,
	feePubKey pubKey)
{
	giant 		Q = NULL;
	giant 		messageGiant = NULL;
	giant 		scratch = NULL;
	sigInst 	*sinst = (sigInst*) sig;
	feeReturn	frtn;
	curveParams	*cp;
	key		origKey;		// may be plus or minus key

	if(sinst->PmX == NULL) {
		dbgLog(("sigVerify without parse!\n"));
		frtn = FR_IllegalArg;
		goto out;
	}

	cp = feePubKeyCurveParams(pubKey);
	Q = newGiant(cp->maxDigits);

	/*
	 * pick a key (+/-)
	 * Q := P1
	 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
	if(SIG_CURVE == CURVE_PLUS) {
		origKey = feePubKeyPlusCurve(pubKey);
		gtog(cp->x1Plus, Q);
	}
	else {
		origKey = feePubKeyMinusCurve(pubKey);
		gtog(cp->x1Minus, Q);
	}
#pragma clang diagnostic pop

	messageGiant = 	giant_with_data(data, dataLen);	// M(ciphertext)

	/* Q := u 'o' P1 */
	elliptic_simple(Q, sinst->u, cp);

	/* scratch := theirPub */
	scratch = newGiant(cp->maxDigits);
	gtog(origKey->x, scratch);

	#if	SIG_DEBUG
	if(sigDebug) {
		printf("verify origKey:\n");
		printKey(origKey);
		printf("messageGiant: ");
		printGiant(messageGiant);
		printf("curveParams:\n");
		printCurveParams(cp);
	}
	#endif	// SIG_DEBUG

	/* scratch := M 'o' theirPub */
	elliptic_simple(scratch, messageGiant, cp);

	#if	SIG_DEBUG
	if(sigDebug) {
		printf("signature_compare, with\n");
		printf("p0 = Q:\n");
		printGiant(Q);
		printf("p1 = Pm:\n");
		printGiant(sinst->PmX);
		printf("p2 = scratch = R:\n");
		printGiant(scratch);
	}
	#endif	// SIG_DEBUG

	if(signature_compare(Q, sinst->PmX, scratch, cp)) {

		frtn = FR_InvalidSignature;
		#if	LOG_BAD_SIG
		printf("***yup, bad sig***\n");
		#endif	// LOG_BAD_SIG
	}
	else {
		frtn = FR_Success;
	}
out:
	if(messageGiant != NULL) {
	    freeGiant(messageGiant);
	}
	if(Q != NULL) {
	    freeGiant(Q);
	}
	if(scratch != NULL) {
	    freeGiant(scratch);
	}
	return frtn;
}

/*
 * For given key, calculate maximum signature size. 
 */
feeReturn feeSigSize(
	feePubKey pubKey,
	unsigned *maxSigLen)
{
	/* For now, assume that u and Pm.x in the signature are 
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
