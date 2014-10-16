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
 * FeeFEEDExp.c - generic FEED encryption object, 2:1 expansion
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 20 Jan 1998 at Apple
 * 	Mods for primeType == PT_GENERAL case.
 * 12 Jun 1997 at Apple
 *	Was curveOrderJustify(), is lesserX1OrderJustify()
 * 03 Mar 1997 at Apple
 *	Trimmed plainBlockSize by one byte if q mod 8 = 0
 * 03 Feb 97 at NeXT
 *	Renamed to feeFEEDExp.c
 *	Justified random xaux to [2, minimumX1Order]
 *	Added feeFEEDExpCipherTextSize()
 * 15 Jan 97 at NeXT
 *	Cleaned up which_curve/index code to use CURVE_MINUS/CURVE_PLUS
 * 28 Aug 96 at NeXT
 *	Created from Blaine Garst's NSFEECryptor.m.
 */

#include "ckconfig.h"

#if	CRYPTKIT_ASYMMETRIC_ENABLE

#include "feeTypes.h"
#include "feeFEEDExp.h"
#include "feePublicKey.h"
#include "feePublicKeyPrivate.h"
#include "elliptic.h"
#include "falloc.h"
#include "feeRandom.h"
#include "ckutilities.h"
#include "feeFunctions.h"
#include "platform.h"
#include "feeDebug.h"
#include <stdlib.h>

#define	FEED_DEBUG				0

#define PRINT_GIANT(g)	printGiant(g)

/*
 * Format of clue byte. Currently just one bit. 
 */
#define CLUE_ELL_ADD_SIGN		0x01
#define CLUE_ELL_ADD_SIGN_PLUS	0x01
#define CLUE_ELL_ADD_SIGN_MINUS	0x00

/*
 * Private data.
 */
typedef struct {
	key			plus;
	key			minus;
	unsigned	plainBlockSize;	/* plaintext block size */
	unsigned	cipherBlockSize;/* ciphertext block size */
	curveParams	*cp;
	giant		gPriv;		/* private data, only for decrypt */
	/* one of the follow two is valid for encrypt */
	feeRand		rand;		/* only created for encrypt */
	feeRandFcn	randFcn;
	void		*randRef;
	
	/*
	 * temporary variables used for encrypt/decrypt. The values in these
	 * is not needed to be kept from block to block; we just
	 * alloc them once per lifetime of a feeFEED object as an optimization.
	 */
	giant 		xp;		/* plaintext */
	giant		xc;		/* clue = r(P1?) */
	giant		xq;		/* r(pubB?) or priB?(xc) */
	giant		xm;		/* ciphertext */
	giant		xaux;		/* scratch */
	unsigned char	*randData;	/* only created for encrypt */
} feedInst;

/*
 * "zero residue" indicator.
 */
#define RESID_ZERO	0xff

/*
 * Alloc and init a feeFEEDExp object associated with specified feePubKey.
 */
feeFEEDExp feeFEEDExpNewWithPubKey(
	feePubKey pubKey,
	feeRandFcn randFcn,		// optional 
	void *randRef)
{
	feedInst 		*finst = (feedInst *) fmalloc(sizeof(feedInst));
	giant 			privGiant;

	finst->cp = curveParamsCopy(feePubKeyCurveParams(pubKey));
	finst->plus = new_public_with_key(feePubKeyPlusCurve(pubKey),
		finst->cp);
	finst->minus = new_public_with_key(feePubKeyMinusCurve(pubKey),
		finst->cp);

	/*
	 * These might yield NULL data; we can only encrypt in that case.
	 */
	privGiant = feePubKeyPrivData(pubKey);
	if(privGiant) {
		finst->gPriv = newGiant(finst->cp->maxDigits);
		gtog(privGiant, finst->gPriv);
	}
	else {
		finst->gPriv = NULL;
	}

	/*
	 * Conservative, rounding down, on plaintext blocks since we don't
	 * want to split bytes.
	 */
	if(finst->cp->primeType == FPT_General) {
	    unsigned blen = bitlen(finst->cp->basePrime);

	    finst->plainBlockSize = blen / 8;
	    if((blen % 8) == 0) {
	    	/*
		 * round down some more...
		 */
		finst->plainBlockSize--;
	    }
	}
	else {
	    finst->plainBlockSize = finst->cp->q / 8;
	    if(((finst->cp->q & 0x7) == 0) && (finst->cp->k > 0)) {
		/*
		 * Special case, with q mod 8 == 0. Here we have to trim back
		 * the plainBlockSize by one byte.
		 */
		finst->plainBlockSize--;
	    }
	}

	/*
	 * One block of ciphertext - two giants (with implied sign) and a
	 * parity byte
	 */
	finst->cipherBlockSize = (2 * finst->cp->minBytes) + 1;

	finst->xp = newGiant(finst->cp->maxDigits);
	finst->xc = newGiant(finst->cp->maxDigits);
	finst->xq = newGiant(finst->cp->maxDigits);
	finst->xm = newGiant(finst->cp->maxDigits);
	finst->xaux = newGiant(finst->cp->maxDigits);
	finst->rand = NULL;
	finst->randData = NULL;
	finst->randFcn = randFcn;
	finst->randRef = randRef;
	return finst;
}

void feeFEEDExpFree(feeFEEDExp feed)
{
	feedInst *finst = (feedInst *) feed;

	free_key(finst->plus);
	free_key(finst->minus);
	freeGiant(finst->xc);
	clearGiant(finst->xp); freeGiant(finst->xp);
	clearGiant(finst->xq); freeGiant(finst->xq);
	freeGiant(finst->xm);
	clearGiant(finst->xaux); freeGiant(finst->xaux);
	if(finst->gPriv) {
		clearGiant(finst->gPriv);
		freeGiant(finst->gPriv);
	}
	if(finst->rand) {
		feeRandFree(finst->rand);
	}
	if(finst->randData) {
		ffree(finst->randData);
	}
	if(finst->cp) {
		freeCurveParams(finst->cp);
	}
	ffree(finst);
}

unsigned feeFEEDExpPlainBlockSize(feeFEEDExp feed)
{
	feedInst *finst = (feedInst *) feed;

	return finst->plainBlockSize;
}

unsigned feeFEEDExpCipherBlockSize(feeFEEDExp feed)
{
	feedInst *finst = (feedInst *) feed;

	return finst->cipherBlockSize;
}

unsigned feeFEEDExpCipherBufSize(feeFEEDExp feed)
{
	feedInst *finst = (feedInst *) feed;

	return 2 * finst->cipherBlockSize;
}

/*
 * Return the size of ciphertext to hold specified size of plaintext.
 */
unsigned feeFEEDExpCipherTextSize(feeFEEDExp feed, unsigned plainTextSize)
{
	/*
	 * Normal case is one block of ciphertext for each block of
	 * plaintext. Add one cipherBlock if
	 * plainTextSize % plainBlockSize == 0.
	 */
	feedInst *finst = (feedInst *) feed;
	unsigned blocks = (plainTextSize + finst->plainBlockSize - 1) /
		finst->plainBlockSize;

	if((plainTextSize % finst->plainBlockSize) == 0) {
		blocks++;
	}
	return blocks * finst->cipherBlockSize;
}

/*
 * Return the size of plaintext to hold specified size of decrypted ciphertext.
 */
unsigned feeFEEDExpPlainTextSize(feeFEEDExp feed, unsigned cipherTextSize)
{
	feedInst *finst = (feedInst *) feed;
	unsigned blocks = (cipherTextSize + finst->cipherBlockSize - 1) /
		finst->cipherBlockSize;

	return blocks * finst->plainBlockSize;
}

/*
 * Encrypt a block or less of data. Caller malloc's cipherText.
 */
feeReturn feeFEEDExpEncryptBlock(feeFEEDExp feed,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char *cipherText,
	unsigned *cipherTextLen,		// RETURNED
	int finalBlock)
{
	feedInst 		*finst = (feedInst *) feed;
	int 			index;				/* which curve (+/- 1) */
	char 			g = 0;				/* parity, which_curve bits in ciphertext */
	key 			B;
	unsigned char	*ptext;				/* for final block */
	unsigned		ctextLen;
	feeReturn		frtn = FR_Success;
	giant			x1;
	unsigned		randLen;
	curveParams		*cp = finst->cp;
	
	if(plainTextLen > finst->plainBlockSize) {
		return FR_IllegalArg;
	}
	else if ((plainTextLen < finst->plainBlockSize) && !finalBlock) {
		return FR_IllegalArg;
	}

	/*
	 * Init only on first encrypt
	 */
	if((finst->randFcn == NULL) && (finst->rand == NULL)) {
		finst->rand = feeRandAlloc();
	}
	if(finst->randData == NULL) {
		finst->randData = (unsigned char*) fmalloc(finst->cp->minBytes);
	}
	
	/*
	 * plaintext as giant xp
	 */
	if(finalBlock) {
		ptext = (unsigned char*) fmalloc(finst->plainBlockSize);
		bzero(ptext, finst->plainBlockSize);
		if(plainTextLen) {
			/*
			 * 0 for empty block with resid length 0
			 */
			bcopy(plainText, ptext, plainTextLen);
		}
		if(plainTextLen < finst->plainBlockSize) {
		    if(plainTextLen == 0) {
		    	/*
				 * Special case - can't actually write zero here;
				 * it screws up deserializing the giant during
				 * decrypt
				 */
		        ptext[finst->plainBlockSize - 1] = RESID_ZERO;
		    }
		    else {
				ptext[finst->plainBlockSize - 1] = plainTextLen;
		    }
			#if FEED_DEBUG
			printf("encrypt: resid 0x%x\n", ptext[finst->plainBlockSize - 1]);
			#endif
		}
		/*
		 * else handle evenly aligned case below...
		 */
		deserializeGiant(ptext, finst->xp, finst->plainBlockSize);
		ffree(ptext);
	}
	else {
		deserializeGiant(plainText, finst->xp, plainTextLen);
	}
	#if	FEED_DEBUG
	printf("encrypt:\n");
	printf("  xp : "); PRINT_GIANT(finst->xp);
	#endif	// FEED_DEBUG

	/*
	 * pick curve B? that data lies upon
	 */
	index = which_curve(finst->xp, finst->cp);
	if(index == CURVE_PLUS) {
		B = finst->plus;
		x1 = finst->cp->x1Plus;
	}
	else {
		B = finst->minus;
		x1 = finst->cp->x1Minus;
	}
	#if	FEED_DEBUG
	printf("  which_curve: %s\n", 
		(index == CURVE_PLUS) ? "CURVE_PLUS" : "CURVE_MINUS");
	#endif

	/*
	 * random number as giant xaux
	 */
	randLen = cp->minBytes;
	if(finst->randFcn != NULL) {
		finst->randFcn(finst->randRef, finst->randData, randLen);
	}
	else {
		feeRandBytes(finst->rand, finst->randData, randLen);
	}
	deserializeGiant(finst->randData, finst->xaux, randLen);
	
	#if	FEE_DEBUG
	if(isZero(finst->xaux)) {
		printf("feeFEEDExpEncryptBlock: random xaux = 0!\n");
	}
	#endif	// FEE_DEBUG
	/*
	 * Justify random # to be in [2, minimumX1Order].
	 */
	lesserX1OrderJustify(finst->xaux, cp);
	#if		FEED_DEBUG
	printf(" xaux: "); PRINT_GIANT(finst->xaux);
	#endif	// FEED_DEBUG

	gtog(B->x, finst->xq);				// xq = pubB?
    elliptic_simple(finst->xq, finst->xaux, cp);
										// xq = r(pubB?)
	#if		FEED_DEBUG
	printf(" r(pubB?): "); PRINT_GIANT(finst->xq);
	#endif
	elliptic_add(finst->xp, finst->xq, finst->xm, cp, SIGN_PLUS);
										// xm = data + r(pubB?)
	gtog(x1, finst->xc);
    elliptic_simple(finst->xc, finst->xaux, cp);
										// xc = r(P1?)
	elliptic_add(finst->xm, finst->xq, finst->xaux, cp, SIGN_PLUS);
										// xaux = xm + xq (for curve +1)
										//      = (data + r(pubB?)) + r(pubB?)
	if(gcompg(finst->xaux, finst->xp) == 0) {
		g |= CLUE_ELL_ADD_SIGN_PLUS;
	}
	else {
		g |= CLUE_ELL_ADD_SIGN_MINUS;
		#if	FEED_DEBUG
		/* this better be true.... */
		elliptic_add(finst->xm, finst->xq, finst->xaux, cp, SIGN_MINUS);
		if(gcompg(finst->xaux, finst->xp)) {
			printf("*******elliptic_add(xm, xq, -1) != xp! *************\n");
			printf("  xq : "); PRINT_GIANT(finst->xq);
			printf("  ell_add(xm, xq, -1) : "); PRINT_GIANT(finst->xaux);
		}
		#endif
	}									// g = (xaux == data) ? add : subtract

	/*
	 * Ciphertext = (xm, xc, g)
	 */
	serializeGiant(finst->xm, cipherText, cp->minBytes);
	cipherText += cp->minBytes;
	serializeGiant(finst->xc, cipherText, cp->minBytes);
	cipherText += cp->minBytes;
	*cipherText++ = g;
	ctextLen = finst->cipherBlockSize;
	#if	FEED_DEBUG
	printf("  xm : "); PRINT_GIANT(finst->xm);
	printf("  xc : "); PRINT_GIANT(finst->xc);
	printf("   g : %d\n", g);
	#endif	// FEED_DEBUG
	if(finalBlock && (plainTextLen == finst->plainBlockSize)) {
		/*
		 * Special case: finalBlock true, plainTextLen == blockSize.
		 * In this case we generate one more block of ciphertext,
		 * with a resid length of zero.
		 */
		unsigned moreCipher;			// additional cipherLen

		#if FEED_DEBUG
		printf("encrypt: one more empty block\n");
		#endif
		frtn = feeFEEDExpEncryptBlock(feed,
			NULL,				// plainText not used
			0,				// resid
			cipherText,			// append...
			&moreCipher,
			1);
		if(frtn == FR_Success) {
			ctextLen += moreCipher;
		}
	}

	*cipherTextLen = ctextLen;
	return frtn;
}

/*
 * Decrypt (exactly) a block of data. Caller malloc's plainText. Always
 * generates feeFEEDExpPlainBlockSize of plaintext, unless finalBlock is
 * non-zero (in which case feeFEEDExpPlainBlockSize or less bytes of
 * plainText are generated).
 */
feeReturn feeFEEDExpDecryptBlock(feeFEEDExp feed,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char *plainText,
	unsigned *plainTextLen,			// RETURNED
	int finalBlock)
{
	feedInst 	*finst = (feedInst *) feed;
	char 		g;
	int 		s;
	feeReturn	frtn = FR_Success;
	curveParams	*cp = finst->cp;
	
	if(finst->gPriv == NULL) {
		/*
		 * Can't decrypt without private data
		 */
		return FR_BadPubKey;
	}

	/*
	 * grab xm, xc, and g from cipherText
	 */
	deserializeGiant(cipherText, finst->xm, finst->cp->minBytes);
	cipherText += finst->cp->minBytes;
	deserializeGiant(cipherText, finst->xc, finst->cp->minBytes);
	cipherText += finst->cp->minBytes;
	g = *cipherText;
	#if	FEED_DEBUG
	printf("decrypt g=%d\n", g);
	printf("  privKey : "); PRINT_GIANT(finst->gPriv);
	printf("  xm : "); PRINT_GIANT(finst->xm);
	printf("  xc : "); PRINT_GIANT(finst->xc);
	#endif	// FEED_DEBUG

	if((g & CLUE_ELL_ADD_SIGN) == CLUE_ELL_ADD_SIGN_PLUS) {
		s = SIGN_PLUS;
	}
	else {
		s = SIGN_MINUS;
	}

	/*
	 * xc = r(P1?)
	 * xc := r(P1?)(pri) = xq
	 * xp = data + r(priB+) +/- pri(rB?)
	 */
	elliptic_simple(finst->xc, finst->gPriv, cp);
	#if	FEED_DEBUG
	printf(" xc1 : "); PRINT_GIANT(finst->xc);
	#endif
	elliptic_add(finst->xm, finst->xc, finst->xp, cp, s);

	/*
	 * plaintext in xp
	 */
	#if	FEED_DEBUG
	printf("  xp : "); PRINT_GIANT(finst->xp);
	#endif	// FEED_DEBUG

	if(finalBlock) {
		/*
		 * Snag data from xp in order to find out how much to move to
		 * *plainText
		 */
		unsigned char *ptext = (unsigned char*) fmalloc(finst->plainBlockSize);

		serializeGiant(finst->xp, ptext, finst->plainBlockSize);
		*plainTextLen = ptext[finst->plainBlockSize - 1];
		#if FEED_DEBUG
		printf("decrypt: resid 0x%x\n", *plainTextLen);
		#endif
		if(*plainTextLen == RESID_ZERO) {
			*plainTextLen = 0;
		}
		else if(*plainTextLen > (finst->plainBlockSize - 1)) {
		    dbgLog(("feeFEEDExpDecryptBlock: ptext overflow!\n"));
		    frtn = FR_BadCipherText;
		}
		else {
			bcopy(ptext, plainText, *plainTextLen);
		}
		ffree(ptext);
	}
	else {
		*plainTextLen = finst->plainBlockSize;
		serializeGiant(finst->xp, plainText, *plainTextLen);
	}
	return frtn;
}

/*
 * Convenience routines to encrypt & decrypt multi-block data.
 */
feeReturn feeFEEDExpEncrypt(feeFEEDExp feed,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char **cipherText,		// malloc'd and RETURNED
	unsigned *cipherTextLen)		// RETURNED
{
	const unsigned char	*ptext;			// per block
	unsigned		ptextLen;		// total to go
	unsigned		thisPtextLen;		// per block
	unsigned char		*ctext;			// per block
	unsigned		ctextLen;		// per block
	unsigned char		*ctextResult;		// to return
	unsigned		ctextResultLen;
	unsigned char		*ctextPtr;
	unsigned 		ctextLenTotal;		// running total
	feeReturn		frtn;
	int			finalBlock;
	unsigned		numBlocks;
	unsigned		plainBlockSize;

	if(plainTextLen == 0) {
		dbgLog(("feeFEEDExpDecrypt: NULL plainText\n"));
		return FR_IllegalArg;
	}

	ptext = plainText;
	ptextLen = plainTextLen;
	ctext = (unsigned char*) fmalloc(feeFEEDExpCipherBufSize(feed));
	plainBlockSize = feeFEEDExpPlainBlockSize(feed);
	numBlocks = (plainTextLen + plainBlockSize - 1)/plainBlockSize;
	ctextResultLen = (numBlocks + 1) * feeFEEDExpCipherBlockSize(feed);
	ctextResult = (unsigned char*) fmalloc(ctextResultLen);
	ctextPtr = ctextResult;
	ctextLenTotal = 0;

	while(1) {
		if(ptextLen <= plainBlockSize) {
			finalBlock = 1;
			thisPtextLen = ptextLen;
		}
		else {
			finalBlock = 0;
			thisPtextLen = plainBlockSize;
		}
		frtn = feeFEEDExpEncryptBlock(feed,
			ptext,
			thisPtextLen,
			ctext,
			&ctextLen,
			finalBlock);
		if(frtn) {
			dbgLog(("feeFEEDExpEncrypt: encrypt error: %s\n",
				feeReturnString(frtn)));
			break;
		}
		if(ctextLen == 0) {
			dbgLog(("feeFEEDExpEncrypt: null ciphertext\n"));
			frtn = FR_Internal;
			break;
		}
		bcopy(ctext, ctextPtr, ctextLen);
		ctextLenTotal += ctextLen;
		if(ctextLenTotal > ctextResultLen) {
			dbgLog(("feeFEEDExpEncrypt: ciphertext overflow\n"));
			frtn = FR_Internal;
			break;
		}
		if(finalBlock) {
			break;
		}
		ctextPtr += ctextLen;
		ptext += thisPtextLen;
		ptextLen -= thisPtextLen;
	}

	ffree(ctext);
	if(frtn) {
		ffree(ctextResult);
		*cipherText = NULL;
		*cipherTextLen = 0;
	}
	else {
		*cipherText = ctextResult;
		*cipherTextLen = ctextLenTotal;
		#if	FEE_DEBUG
		if(feeFEEDExpCipherTextSize(feed, plainTextLen) !=
			    ctextLenTotal) {
		    printf("feeFEEDExpEncrypt: feeFEEDCipherTextSize "
		    	"error!\n");
		    printf("ptext %d  exp ctext %d  actual ctext %d\n",
		    	plainTextLen,
			feeFEEDExpCipherTextSize(feed, plainTextLen),
			ctextLenTotal);
		}
		#endif	// FEE_DEBUG
	}
	return frtn;

}

feeReturn feeFEEDExpDecrypt(feeFEEDExp feed,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char **plainText,		// malloc'd and RETURNED
	unsigned *plainTextLen)			// RETURNED
{
	const unsigned char	*ctext;
	unsigned		ctextLen;		// total to go
	unsigned char		*ptext;			// per block
	unsigned		ptextLen;		// per block
	unsigned char		*ptextResult;		// to return
	unsigned char		*ptextPtr;
	unsigned 		ptextLenTotal;		// running total
	feeReturn		frtn = FR_Success;
	int			finalBlock;
	unsigned		numBlocks;
	unsigned		plainBlockSize =
					feeFEEDExpPlainBlockSize(feed);
	unsigned		cipherBlockSize =
					feeFEEDExpCipherBlockSize(feed);

	if(cipherTextLen % cipherBlockSize) {
		dbgLog(("feeFEEDExpDecrypt: unaligned cipherText\n"));
		return FR_BadCipherText;
	}
	if(cipherTextLen == 0) {
		dbgLog(("feeFEEDExpDecrypt: NULL cipherText\n"));
		return FR_BadCipherText;
	}

	ptext = (unsigned char*) fmalloc(plainBlockSize);
	ctext = cipherText;
	ctextLen = cipherTextLen;
	numBlocks = cipherTextLen / cipherBlockSize;
	ptextResult = (unsigned char*) fmalloc(plainBlockSize * numBlocks);
	ptextPtr = ptextResult;
	ptextLenTotal = 0;

	while(ctextLen) {
		if(ctextLen == cipherBlockSize) {
		    finalBlock = 1;
		}
		else {
		    finalBlock = 0;
		}
		frtn = feeFEEDExpDecryptBlock(feed,
			ctext,
			cipherBlockSize,
			ptext,
			&ptextLen,
			finalBlock);
		if(frtn) {
			dbgLog(("feeFEEDExpDecryptBlock: %s\n",
				feeReturnString(frtn)));
			break;
		}
		if(ptextLen == 0) {
			/*
			 * Normal termination case for
			 * plainTextLen % plainBlockSize == 0
			 */
			if(!finalBlock) {
			    dbgLog(("feeFEEDExpDecrypt: decrypt sync"
			    	" error!\n"));
			    frtn = FR_BadCipherText;
			}
			break;
		}
		else if(ptextLen > plainBlockSize) {
			dbgLog(("feeFEEDExpDecrypt: ptext overflow!\n"));
			frtn = FR_Internal;
			break;
		}
		else {
			bcopy(ptext, ptextPtr, ptextLen);
			ptextPtr += ptextLen;
			ptextLenTotal += ptextLen;
		}
		ctext += cipherBlockSize;
		ctextLen -= cipherBlockSize;
	}

	ffree(ptext);
	if(frtn) {
		ffree(ptextResult);
		*plainText = NULL;
		*plainTextLen = 0;
	}
	else {
		*plainText = ptextResult;
		*plainTextLen = ptextLenTotal;
	}
	return frtn;

}

#endif	/* CRYPTKIT_ASYMMETRIC_ENABLE */
