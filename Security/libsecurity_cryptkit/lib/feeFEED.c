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
 * FeeFEED.c - generic, portable FEED encryption object, expanionless version
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 20 Jan 1998	Doug Mitchell at Apple
 * 	Mods for primeType == PT_GENERAL case.
 * 12 Jun 1997	Doug Mitchell at Apple
 *	Was curveOrderJustify(), is lesserX1OrderJustify()
 * 31 Mar 1997	Doug Mitchell at Apple
 *	Fixed initialRS leak
 *  3 Mar 1997	Doug Mitchell at Apple
 *	Trimmed plainBlockSize by one byte if q mod 8 = 0
 * 30 Jan 1997	Doug Mitchell at NeXT
 *	Created.
 */

/*
 * FIXME - a reusable init function would be nice (i.e., free up
 * session-dependent state and re-init it)...
 */
#include "ckconfig.h"

#if	CRYPTKIT_ASYMMETRIC_ENABLE

#include "feeTypes.h"
#include "feeFEED.h"
#include "feeFEEDExp.h"
#include "feePublicKey.h"
#include "feePublicKeyPrivate.h"
#include "elliptic.h"
#include "falloc.h"
#include "feeRandom.h"
#include "ckutilities.h"
#include "feeFunctions.h"
#include "platform.h"
#include "curveParams.h"
#include "feeDebug.h"
#include <stdlib.h>
#include <stdio.h>

#define FEED_DEBUG	0
#define BUFFER_DEBUG	0
#if		BUFFER_DEBUG
#define bprintf(s)	printf s
#else
#define bprintf(s)
#endif

/*
 * Minimum combined size of random r and s, in bytes. For small q sizes,
 * r and s may be even smaller, but we never truncate them to smaller than
 * this.
 * This must be kept in sync with constant of same name in FEED.java.
 */
#define RS_MIN_SIZE	16

/*
 * Private data.
 */
typedef struct {
	curveParams		*cp;

	/*
	 * the clues are initially (r * ourPriv * theirPub(+/-)).
	 */
	giant			cluePlus;
	giant			clueMinus;

	/*
	 * sPlus and sMinus are based on the random s generated at encrypt
	 * time. Values are s * x1{Plus,Minus}.
	 */
	giant			sPlus;
	giant			sMinus;
	giant			r;					/* random, generated at encrypt time */
	unsigned		plainBlockSize;		/* plaintext block size */
	unsigned		cipherBlockSize;	/* ciphertext block size */
	unsigned char 	*initialRS;			/* initial random R,S as bytes */
	unsigned		initialRSSize;		/* in bytes */
	feeFEEDExp		feedExp;			/* for encr/decr r+s params */

	/*
	 * The first few blocks of ciphertext in a stream are the 2:1-FEED
	 * encrypted r and s parameters. While decrypting, we stash incoming
	 * ciphertext in rsCtext until we get enough ciphertext to decrypt
	 * initialRS. RsBlockCount keeps a running count of the
	 * cipherBlocks received. When rsBlockCount == rsSizeCipherBlocks, we
	 * FEEDExp-decrypt rsCtext to get r and s (actually, to get
	 * initialRS; r and s are extraced later in initFromRS()).
	 *
	 * During encrypt, if rsBlockCount is zero, the first thing we send as
	 * ciphertext is the FEED-encrypted initialRS.
	 */
	unsigned char	*rsCtext;			/* buffer for encrypted initialRS */
	unsigned		rsBlockCount;		/* running total of incoming rs
										 *   cipherblocks */

	int 			forEncrypt;			/* added for feeFEED*TextSize() */
	
	/*
 	 * These are calculated at init time - for encrypt and
	 * decrypt - as an optimization.
	 */
	unsigned 		rsCtextSize;		/* number of meaningful bytes in
										 *   rsCtext */
	unsigned		rsSizeCipherBlocks;	/* # of our cipherblocks holding
										 *   rsCtext */
	
	/*
	 * temporary variables used for encrypt/decrypt. The values in these
	 * are not needed to be kept from block to block; we just
	 * alloc them once per lifetime of a feeFEED object as an optimization.
	 */
	giant 			xp;					/* plaintext */
	giant			xm;					/* ciphertext */
	giant			tmp1;				/* scratch */
	giant			tmp2;				/* scratch */
} feedInst;

/*
 * "zero residue" indicator.
 */
#define RESID_ZERO	0xff

/*
 * cons up:
 * 	cluePlus(0)
 *	clueMinus(0)
 *	sPlus
 *	sMinus
 *	r
 * Assumes:
 *	cluePlus = clueMinus = ourPriv * theirPub
 *	initialRS
 * 	initialRSSize
 *	cp
 *
 * Called at feeFEEDNewWithPubKey while encrypting, or upon decrypting
 * first block of data.
 */
static feeReturn initFromRS(feedInst *finst)
{
	giant s;
	unsigned rSize = finst->initialRSSize / 2;

	#if	FEED_DEBUG
	if((finst->initialRS == NULL) ||
	   (finst->cp == NULL) ||
	   (finst->cluePlus == NULL) ||
	   (finst->clueMinus == NULL) ||
	   (finst->initialRSSize == 0)) {
	    	dbgLog(("initFromRS: resource shortage\n"));
	    	return FR_Internal;
	}
	#endif	// FEED_DEBUG

	finst->r = giant_with_data(finst->initialRS, rSize);
	s = giant_with_data(finst->initialRS+rSize, rSize);

	#if	FEED_DEBUG
	if(isZero(finst->r)) {
		printf("initFromRS: r = 0! initialRSSize = %d; encr = %s\n",
			finst->initialRSSize,
			(finst->rsCtext == NULL) ? "TRUE" : "FALSE");
	}
	if(isZero(s)) {
		printf("initFromRS: s = 0! initialRSSize = %d; encr = %s\n",
			finst->initialRSSize,
			(finst->rsCtext == NULL) ? "TRUE" : "FALSE");
	}
	#endif	// FEE_DEBUG
	/*
	 * Justify r and s to be in [2, minimumX1Order].
	 */
	lesserX1OrderJustify(finst->r, finst->cp);
	lesserX1OrderJustify(s, finst->cp);

	/*
	 * sPlus  = s * x1Plus
	 * sMinus = s * x1Minus
	 */
	finst->sPlus = newGiant(finst->cp->maxDigits);
	finst->sMinus = newGiant(finst->cp->maxDigits);
	gtog(finst->cp->x1Plus, finst->sPlus);
	elliptic_simple(finst->sPlus, s, finst->cp);
	gtog(finst->cp->x1Minus, finst->sMinus);
	elliptic_simple(finst->sMinus, s, finst->cp);

	/*
	 * And finally, the initial clues. They are currently set to
	 * ourPriv * theirPub.
	 */
	#if	FEED_DEBUG
	printf("cluePlus : "); printGiant(finst->cluePlus);
	printf("clueMinus: "); printGiant(finst->clueMinus);
	#endif	// FEED_EEBUG

	elliptic_simple(finst->cluePlus, finst->r, finst->cp);
	elliptic_simple(finst->clueMinus, finst->r, finst->cp);

	#if	FEED_DEBUG
	printf("r        : "); printGiant(finst->r);
	printf("s        : "); printGiant(s);
	printf("sPlus    : "); printGiant(finst->sPlus);
	printf("sMinus   : "); printGiant(finst->sMinus);
	printf("cluePlus : "); printGiant(finst->cluePlus);
	printf("clueMinus: "); printGiant(finst->clueMinus);
	#endif	// FEED_DEBUG

	freeGiant(s);
	return FR_Success;
}

/*
 * Alloc and init a feeFEED object associated with specified public and
 * private keys.
 */
feeFEED feeFEEDNewWithPubKey(feePubKey myPrivKey,
	feePubKey theirPubKey,
	int forEncrypt,			// 0 ==> decrypt   1 ==> encrypt
	feeRandFcn randFcn,		// optional 
	void *randRef)
{
	feedInst 		*finst;
	giant			privGiant;
	key				k;
	unsigned 		expPlainSize;
	unsigned 		expCipherSize;
	unsigned 		expBlocks;

	if(!curveParamsEquivalent(feePubKeyCurveParams(theirPubKey),
		    feePubKeyCurveParams(myPrivKey))) {
		dbgLog(("feeFEEDNewWithPubKey: Incompatible Keys\n"));
		return NULL;
	}
	finst = (feedInst*) fmalloc(sizeof(feedInst));
	bzero(finst, sizeof(feedInst));
	finst->forEncrypt = forEncrypt;
	finst->cp = curveParamsCopy(feePubKeyCurveParams(theirPubKey));
	finst->rsBlockCount = 0;
	finst->xp = newGiant(finst->cp->maxDigits);
	finst->xm = newGiant(finst->cp->maxDigits);
	finst->tmp1 = newGiant(finst->cp->maxDigits);
	if(forEncrypt) {
	    finst->tmp2 = newGiant(finst->cp->maxDigits);
	}

	/*
	 * cluePlus  = ourPriv * theirPub+
	 * clueMinus = ourPriv * theirPub-
	 */
	finst->cluePlus  = newGiant(finst->cp->maxDigits);
	finst->clueMinus = newGiant(finst->cp->maxDigits);
	privGiant = feePubKeyPrivData(myPrivKey);
	if(privGiant == NULL) {
		dbgLog(("feeFEEDNewWithPubKey: no private key\n"));
		goto abort;
	}
	k = feePubKeyPlusCurve(theirPubKey);
	gtog(k->x, finst->cluePlus);			// cluePlus = theirPub+
	elliptic_simple(finst->cluePlus, privGiant, finst->cp);
	k = feePubKeyMinusCurve(theirPubKey);
	gtog(k->x, finst->clueMinus);			// theirPub-
	elliptic_simple(finst->clueMinus, privGiant, finst->cp);

	/*
	 * Set up block sizes.
	 */
	if(finst->cp->primeType == FPT_General) {
	    unsigned blen = bitlen(finst->cp->basePrime);

	    finst->plainBlockSize = blen / 8;
	    if((blen & 0x7) == 0) {
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
		 * Special case, with q mod 8 == 0. Here we have to
		 * trim back the plainBlockSize by one byte.
		 */
		finst->plainBlockSize--;
	    }
	}
	finst->cipherBlockSize = finst->cp->minBytes + 1;

	/*
	 * the size of initialRS is subject to tweaking - if we make it
	 * not a multiple of plainBlockSize, we save one FEEDExp cipherBlock
	 * in our ciphertext.
	 */
	finst->initialRSSize = finst->plainBlockSize * 2;
	if(finst->initialRSSize > RS_MIN_SIZE) {
	    unsigned minPlainBlocks;
	    unsigned maxSize;

	    /*
	     * How many plainblocks to hold RS_MIN_SIZE?
	     */
	    minPlainBlocks = (RS_MIN_SIZE + finst->plainBlockSize - 1) /
	    	finst->plainBlockSize;

	    /*
	     * Max size = that many plainblocks, less 2 bytes (to avoid
	     * extra residue block).
	     */
	    maxSize = minPlainBlocks * finst->plainBlockSize - 2;

	    /*
	     * But don't bother with more than 2 plainblocks worth
	     */
	    if(finst->initialRSSize > maxSize) {
	        finst->initialRSSize = maxSize;
	    }
	}
	/* else leave it alone, that's small enough */

	if(forEncrypt) {
		feeRand frand = NULL;

		/*
		 * Encrypt-capable FEEDExp object
		 */
		finst->feedExp = feeFEEDExpNewWithPubKey(theirPubKey,
			randFcn,
			randRef);
		if(finst->feedExp == NULL) {
			goto abort;
		}

		/*
		 * Generate initial r and s data.
		 */
		finst->initialRS = (unsigned char*) fmalloc(finst->initialRSSize);
		if(randFcn != NULL) {
			randFcn(randRef, finst->initialRS, finst->initialRSSize);
		}
		else {
			frand = feeRandAlloc();
			feeRandBytes(frand, finst->initialRS, finst->initialRSSize);
			feeRandFree(frand);
		}
		if(initFromRS(finst)) {
			goto abort;
		}
	}
	else {
		/*
		 * Decrypt-capable FEEDExp object
		 */
		finst->feedExp = feeFEEDExpNewWithPubKey(myPrivKey,
			randFcn,
			randRef);
		if(finst->feedExp == NULL) {
			goto abort;
		}

	}

	/*
	 * Figure out how many of our cipherblocks it takes to hold
	 * a FEEDExp-encrypted initialRS. If initialRSSize is an exact
	 * multiple of expPlainSize, we get an additional feedExp
	 * residue block.
	 */
	expPlainSize = feeFEEDExpPlainBlockSize(finst->feedExp);
	expCipherSize = feeFEEDExpCipherBlockSize(finst->feedExp);
	expBlocks = (finst->initialRSSize + expPlainSize - 1) /
		expPlainSize;
	if((finst->initialRSSize % expPlainSize) == 0) {
		expBlocks++;
	}

	/*
	 * Total meaningful bytes of encrypted initialRS
	 */
	finst->rsCtextSize = expBlocks * expCipherSize;

	/*
	 * Number of our cipherblocks it takes to hold rsCtextSize
	 */
	finst->rsSizeCipherBlocks = (finst->rsCtextSize +
		finst->cipherBlockSize - 1) / finst->cipherBlockSize;
	if(!forEncrypt) {
	    finst->rsCtext = (unsigned char*) fmalloc(finst->rsSizeCipherBlocks *
		finst->cipherBlockSize);
	}

	/*
	 * Sanity check...
	 */
	#if	FEED_DEBUG
	{
	    unsigned fexpBlockSize = feeFEEDExpCipherBlockSize(finst->feedExp);

	    /*
	     * FEEDExp has one more giant in ciphertext, plaintext is
	     * same size
	     */
	    if((finst->cipherBlockSize + finst->cp->minBytes) !=
			fexpBlockSize) {
		dbgLog(("feeFEEDNewWithPubKey: FEEDExp CBlock Size "
			"screwup\n"));
		goto abort;
	    }
	    fexpBlockSize = feeFEEDExpPlainBlockSize(finst->feedExp);
	    if(fexpBlockSize != finst->plainBlockSize) {
		dbgLog(("feeFEEDNewWithPubKey: FEEDExp PBlock Size "
			"screwup\n"));
		goto abort;
	    }
	}
	#endif	// FEED_DEBUG

	return finst;

abort:
	feeFEEDFree(finst);
	return NULL;
}

void feeFEEDFree(feeFEED feed)
{
	feedInst *finst = (feedInst*) feed;

	if(finst->cp) {
		freeCurveParams(finst->cp);
	}
	if(finst->initialRS) {
		ffree(finst->initialRS);
	}
	if(finst->cluePlus) {
		freeGiant(finst->cluePlus);
	}
	if(finst->clueMinus) {
		freeGiant(finst->clueMinus);
	}
	if(finst->sPlus) {
		freeGiant(finst->sPlus);
	}
	if(finst->sMinus) {
		freeGiant(finst->sMinus);
	}
	if(finst->r) {
		freeGiant(finst->r);
	}
	if(finst->feedExp) {
		feeFEEDExpFree(finst->feedExp);
	}
	if(finst->rsCtext) {
		ffree(finst->rsCtext);
	}
	if(finst->xp) {
		freeGiant(finst->xp);
	}
	if(finst->xm) {
		freeGiant(finst->xm);
	}
	if(finst->tmp1) {
		freeGiant(finst->tmp1);
	}
	if(finst->tmp2) {
		freeGiant(finst->tmp2);
	}
	ffree(finst);
}

unsigned feeFEEDPlainBlockSize(feeFEED feed)
{
	feedInst *finst = (feedInst *) feed;

	return finst->plainBlockSize;
}

unsigned feeFEEDCipherBlockSize(feeFEED feed)
{
	feedInst *finst = (feedInst *) feed;

	return finst->cipherBlockSize;
}

/*
 * Calculate size of buffer currently needed to encrypt one block of
 * plaintext. Also used to calculate required input during decrypt
 * to get any output.
 */
unsigned feeFEEDCipherBufSize(feeFEED feed,
	 int finalBlock)
{
	feedInst *finst = (feedInst *) feed;
	unsigned blocks = 1;			// always at least one block of ciphertext
	
	if(finst->rsBlockCount == 0) {
		/* haven't sent/seen encrypted RS yet */
		blocks += finst->rsSizeCipherBlocks;
	}
	
	if(finalBlock) {
		/* only needed if ptext is aligned, but tell caller to malloc */
		blocks++;
	}
	bprintf(("$$$ feeFEEDCipherBufSize( %s, %s): rtn 0x%x\n",
		finst->forEncrypt ? "encrypt" : "decrypt",
		finalBlock ? " final" : "!final", 
		blocks * finst->cipherBlockSize));
	return blocks * finst->cipherBlockSize;
}

/*
 * Return the size of ciphertext currently needed to encrypt specified 
 * size of plaintext. Also can be used to calculate size of ciphertext 
 * which can be decrypted into specified size of plaintext. 
 */
unsigned feeFEEDCipherTextSize(feeFEED feed, 
	unsigned 	plainTextSize,
	int 		finalBlock)
{
	feedInst *finst = (feedInst *) feed;
	
	/* how many blocks of plaintext? */
	unsigned blocks = (plainTextSize + finst->plainBlockSize - 1) /
		finst->plainBlockSize;

	if(finst->forEncrypt) {
		/* have we generated RS? */
		if(finst->rsBlockCount == 0) {
			/* haven't sent encrypted RS yet */
			blocks += finst->rsSizeCipherBlocks;
		}
	
		/* final? residue? */
		if(finalBlock) {
			if((plainTextSize % finst->plainBlockSize) == 0) {
				blocks++;
			}
		}
	}	/* encrypting */
	else {
		/* 
		 * Decrypting - how much ciphertext can we decrypt safely into
		 * specified plaintext? Add in RS if we haven't seen it all
		 * yet.
		 */
		#if BUFFER_DEBUG
		if(finst->rsBlockCount > finst->rsSizeCipherBlocks) {
			printf("******HEY! rsBlockCount overflow! (blockCount %d rsSize %d)\n",
				finst->rsBlockCount, finst->rsSizeCipherBlocks);
		}
		#endif
		blocks += (finst->rsSizeCipherBlocks - finst->rsBlockCount);
	}
	bprintf(("$$$ feeFEEDCipherTextSize(%s, %s, 0x%x): rtn 0x%x\n",
		finst->forEncrypt ? "encrypt" : "decrypt",
		finalBlock ? " final" : "!final", 
		plainTextSize, blocks * finst->cipherBlockSize));
	return blocks * finst->cipherBlockSize;
}

/*
 * Return the size of plaintext currently needed to decrypt specified size 
 * of ciphertext. Also can be used to calculate size of plaintext 
 * which can be encrypted into specified size of ciphertext.
 */
unsigned feeFEEDPlainTextSize(feeFEED feed, 
	unsigned 	cipherTextSize,
	int 		finalBlock)			// ignored if !forEncrypt
{
	feedInst *finst = (feedInst *) feed;
	
	/* start with basic cipher block count */
	unsigned cipherBlocks = (cipherTextSize + finst->cipherBlockSize - 1) /
		finst->cipherBlockSize;
		
	/* where are we in the RS stream? */
	unsigned rsBlocksToGo = finst->rsSizeCipherBlocks - finst->rsBlockCount;
	if(finst->forEncrypt) {
		/* 
		 * Encrypting, seeking plaintext size we can encrypt given
		 * a specified size of ciphertext.
		 */
		if(rsBlocksToGo >= cipherBlocks) {
			/* no room! next encrypt would overflow ctext buffer! */
			return 0;
		}
		cipherBlocks -= rsBlocksToGo;
		
		/* another constraint - residue */
		if(finalBlock) {
			if(cipherBlocks) {
				/* skip if already zero... */
				cipherBlocks--;
			}
		}
	}	/* encrypting */
	else {
		/* decrypting */
		if(rsBlocksToGo >= cipherBlocks) {
			/* still processing RS, no plaintext will be generated. Play it real
			 * safe and just tell caller one block. */
			cipherBlocks = 1;
		}
		else {
			/* diminish by size of RS to be gobbled with no output */
			cipherBlocks -= rsBlocksToGo;
		}
	}
	bprintf(("$$$ feeFEEDPlainTextSize( %s, %s, 0x%x): rtn 0x%x\n",
		finst->forEncrypt ? "encrypt" : "decrypt",
		finalBlock ? " final" : "!final", 
		cipherTextSize, cipherBlocks * finst->plainBlockSize));
	return cipherBlocks * finst->plainBlockSize;
}

/*
 * Bits in last byte of cipherblock
 */
#define CLUE_BIT		0x01	/* 1 ==> plus curve */
#define CLUE_PLUS		0x01
#define CLUE_MINUS		0x00
#define PARITY_BIT		0x02	/* 1 ==> plus 's' arg to elliptic_add() */
#define PARITY_PLUS		0x02
#define PARITY_MINUS	0x00

/*
 * Encrypt a block or less of data. Caller malloc's cipherText.
 * Generates up to feeFEEDCipherBufSize() bytes of ciphertext.
 */
feeReturn feeFEEDEncryptBlock(feeFEED feed,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char *cipherText,
	unsigned *cipherTextLen,		// RETURNED
	int finalBlock)
{
	feedInst 		*finst = (feedInst *) feed;
	unsigned		ctextLen = 0;
	feeReturn		frtn = FR_Success;
	int				whichCurve;
	giant			thisClue;		// not alloc'd or freed
	giant			thisS;			// ditto
	unsigned char	clueByte;

	if(plainTextLen > finst->plainBlockSize) {
		return FR_IllegalArg;
	}
	if((plainTextLen < finst->plainBlockSize) && !finalBlock) {
		return FR_IllegalArg;
	}
	if(finst->initialRS == NULL) {
		/*
		 * Init'd for decrypt?
		 */
		return FR_IllegalArg;
	}

	/*
	 * First block - encrypt initialRS via FEEDExp
	 */
	if(finst->rsBlockCount == 0) {
	    unsigned char *thisCtext;	// malloc's by FEEDExp
	    unsigned padLen;

	    if(finst->initialRS == NULL) {
		/*
		 * init'd for decrypt or reused
		 */
		dbgLog(("feeFEEDEncryptBlock: NULL initialRS!\n"));
		return FR_IllegalArg;
	    }

	    frtn = feeFEEDExpEncrypt(finst->feedExp,
		    finst->initialRS,
		    finst->initialRSSize,
		    &thisCtext,
		    &ctextLen);
	    if(frtn) {
		    /*
		     * Should never happen...
		     */
		    dbgLog(("feeFEEDEncryptBlock: error writing encrypted"
			    " initialRS (%s)\n", feeReturnString(frtn)));
		    return FR_Internal;
	    }
	    bcopy(thisCtext, cipherText, ctextLen);
	    cipherText += ctextLen;
	    ffree(thisCtext);

	    finst->rsBlockCount = finst->rsSizeCipherBlocks;
	    padLen = finst->cipherBlockSize -
	    	(ctextLen % finst->cipherBlockSize);	// zeros to write

	    #if		0	/* FEED_DEBUG */

	    /*
	     * Hard-coded assumptions and tests about initRSSize...
	     * Currently we assume that initRSSize % expBlockSize = 0
	     */
	    if((ctextLen / finst->cipherBlockSize) != 5) {
		dbgLog(("feeFEEDEncryptBlock: cipherblock size screwup (1)\n"));
		return FR_Internal;
	    }
	    if(padLen != 3) {
		dbgLog(("feeFEEDEncryptBlock: cipherblock size screwup (2)\n"));
		return FR_Internal;
	    }
	    #endif	// FEED_DEBUG

	    /*
	     * pad to multiple of (our) cipherblock size.
	     */
	    while(padLen) {
		*cipherText++ = 0;
		ctextLen++;
		padLen--;
	    }
	}

	/*
	 * plaintext to giant xp
	 */
	if(finalBlock) {
		unsigned char *ptext = (unsigned char*) fmalloc(finst->plainBlockSize);
		bzero(ptext, finst->plainBlockSize);
		if(plainTextLen) {
			/*
			 * skip for empty block with resid length 0
			 */
			bcopy(plainText, ptext, plainTextLen);
		}
		if(plainTextLen < finst->plainBlockSize) {
		    if(plainTextLen == 0) {
		    	/*
			 * Special case - resid block with no actual plaintext.
			 * Can't actually write zero here; it screws up
			 * deserializing the giant during decrypt
			 */
		        ptext[finst->plainBlockSize - 1] = RESID_ZERO;
				bprintf(("=== FEED encrypt: RESID_ZERO\n"));
		    }
		    else {
				ptext[finst->plainBlockSize - 1] = plainTextLen;
				bprintf(("=== FEED encrypt: resid len 0x%x\n", plainTextLen));
		    }
		}
		/*
		 * else handle evenly aligned case (i.e., finalBlock true
		 * and (plainTextLen ==  plainBlockSize)) below...
		 */
		deserializeGiant(ptext, finst->xp, finst->plainBlockSize);
		ffree(ptext);
	}
	else {
		deserializeGiant(plainText, finst->xp, plainTextLen);
	}

	/*
	 * encrypt xp
	 *     xm = xp + clue(+/-)
	 * determine parity needed to restore xp
	 *     parity = ((xm + clue(+/-) == xp) ? 1 : -1
	 * and adjust clue
	 *     clue[n+1] = r * clue[n] + (s * P1)
	 */
	whichCurve = which_curve(finst->xp, finst->cp);
	if(whichCurve == CURVE_PLUS) {
		thisClue = finst->cluePlus;
		thisS    = finst->sPlus;
		clueByte = CLUE_PLUS;
	}
	else {
		thisClue = finst->clueMinus;
		thisS    = finst->sMinus;
		clueByte = CLUE_MINUS;
	}
	// calculate xm
	elliptic_add(thisClue, finst->xp, finst->xm, finst->cp, SIGN_PLUS);
	// save xm + clue in tmp1
	elliptic_add(finst->xm, thisClue, finst->tmp1, finst->cp, SIGN_PLUS);
	// Adjust clue
	elliptic_simple(thisClue, finst->r, finst->cp);
	gtog(thisClue, finst->tmp2);
	elliptic_add(finst->tmp2, thisS, thisClue, finst->cp, SIGN_PLUS);

	/*
	 * Calculate parity
	 */
	if(gcompg(finst->tmp1, finst->xp) == 0) {
		clueByte |= PARITY_PLUS;
	}

	/*
	 * Ciphertext = (xm, clueByte)
	 */
	serializeGiant(finst->xm, cipherText, finst->cp->minBytes);
	cipherText += finst->cp->minBytes;
	ctextLen += finst->cp->minBytes;
	*cipherText++ = clueByte;
	ctextLen++;

	#if	FEED_DEBUG
	printf("encrypt  clue %d\n", clueByte);
	printf("  xp : "); printGiant(finst->xp);
	printf("  xm : "); printGiant(finst->xm);
	printf("  cluePlus  :"); printGiant(finst->cluePlus);
	printf("  clueMinus :"); printGiant(finst->clueMinus);
	#endif	// FEED_DEBUG

	if(finalBlock && (plainTextLen == finst->plainBlockSize)) {
	       /*
		* Special case: finalBlock true, plainTextLen == blockSize.
		* In this case we generate one more block of ciphertext,
		* with a resid length of zero.
		*/
		unsigned moreCipher;			// additional cipherLen

		frtn = feeFEEDEncryptBlock(feed,
			NULL,				// plainText not used
			0,				// resid
			cipherText,			// append...
			&moreCipher,
			1);
		if(frtn == FR_Success) {
			ctextLen += moreCipher;
		}
	}
	bprintf(("=== FEED encryptBlock ptextLen 0x%x  ctextLen 0x%x\n",
		plainTextLen, ctextLen));
		
	*cipherTextLen = ctextLen;
	return frtn;
}

/*
 * Decrypt (exactly) a block of data. Caller malloc's plainText. Always
 * generates feeFEEDPlainBlockSize of plaintext, unless finalBlock is
 * non-zero (in which case feeFEEDPlainBlockSize or less bytes of plainText are
 * generated).
 */
feeReturn feeFEEDDecryptBlock(feeFEED feed,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char *plainText,
	unsigned *plainTextLen,			// RETURNED
	int finalBlock)
{
	feedInst 	*finst = (feedInst *) feed;
	feeReturn	frtn = FR_Success;
	unsigned char	clueByte;
	giant		thisClue;		// not alloc'd
	giant		thisS;			// ditto
	int 		parity;

	if(finst->rsCtext == NULL) {
		/*
		 * Init'd for encrypt?
		 */
		return FR_IllegalArg;
	}
	if(cipherTextLen != finst->cipherBlockSize) {
	 	dbgLog(("feeFEEDDecryptBlock: bad cipherTextLen\n"));
		return FR_IllegalArg;
	}
	if(finst->rsBlockCount < finst->rsSizeCipherBlocks) {
		/*
		 * Processing initialRS, FEEDExp-encrypted
		 */
		unsigned char *rsPtr = finst->rsCtext +
			(finst->rsBlockCount * finst->cipherBlockSize);
		unsigned feedExpCipherSize;

		if(finalBlock) {
		    dbgLog(("feeFEEDDecryptBlock: incomplete initialRS\n"));
		    return FR_BadCipherText;
		}
		bcopy(cipherText, rsPtr, finst->cipherBlockSize);
		finst->rsBlockCount++;
		if(finst->rsBlockCount < finst->rsSizeCipherBlocks) {
		    /*
		     * Not done with this yet...
		     */
			bprintf(("=== FEED Decrypt: gobbled 0x%x bytes ctext, no ptext (1)\n", 
				cipherTextLen));
		    *plainTextLen = 0;
		    return FR_Success;
		}

		#if	FEED_DEBUG
		if((finst->rsBlockCount * finst->cipherBlockSize) <
				finst->rsCtextSize) {
		    dbgLog(("feeFEEDDecryptBlock: rsCtextSize underflow!\n"));
		    return FR_Internal;
		}
		#endif	// FEED_DEBUG

		/*
		 * OK, we should have the FEEDExp ciphertext for initialRS
		 * in rsCtext. Note the last few bytes are extra; we don't
		 * pass them to FEEDExp.
		 */
		feedExpCipherSize = feeFEEDCipherBlockSize(finst->feedExp);
		frtn = feeFEEDExpDecrypt(finst->feedExp,
			finst->rsCtext,
			finst->rsCtextSize,
			&finst->initialRS,
			&finst->initialRSSize);
		if(frtn) {
   		    dbgLog(("feeFEEDDecryptBlock: error decrypting "
		    	"initialRS (%s)\n", feeReturnString(frtn)));
		    return FR_BadCipherText;
		}

		/*
		 * we already know how long this should be...
		 */
		if(finst->initialRSSize != finst->initialRSSize) {
   		    dbgLog(("feeFEEDDecryptBlock: initialRS sync error\n"));
		    return FR_BadCipherText;
		}

		/*
		 * Set up clues
		 */
		if(initFromRS(finst)) {
   		    dbgLog(("feeFEEDDecryptBlock: bad initialRS\n"));
		    return FR_BadCipherText;
		}
		else {
		    /*
		     * Normal completion of last cipherblock containing
		     * initialRS.
		     */
			bprintf(("=== FEED Decrypt: gobbled 0x%x bytes ctext, no ptext (2)\n", 
				cipherTextLen));
		    *plainTextLen = 0;
		    return FR_Success;
		}
	}

	/*
	 * grab xm and clueByte from cipherText
	 */
	deserializeGiant(cipherText, finst->xm, finst->cp->minBytes);
	cipherText += finst->cp->minBytes;
	clueByte = *cipherText;

	if((clueByte & CLUE_BIT) == CLUE_PLUS) {
		thisClue = finst->cluePlus;
		thisS = finst->sPlus;
	}
	else {
		thisClue = finst->clueMinus;
		thisS = finst->sMinus;
	}
	if((clueByte & PARITY_BIT) == PARITY_PLUS) {
		parity = SIGN_PLUS;
	}
	else {
		parity = SIGN_MINUS;
	}

	/*
	 * recover xp
	 *     xp = xm + clue(+/-) w/parity
	 * adjust clue
	 *     clue[n+1] = r * clue[n] + (s * P1)
	 */
	elliptic_add(thisClue, finst->xm, finst->xp, finst->cp, parity);

	elliptic_simple(thisClue, finst->r, finst->cp);
	gtog(thisClue, finst->tmp1);
	elliptic_add(finst->tmp1, thisS, thisClue, finst->cp, SIGN_PLUS);

	/*
	 * plaintext in xp
	 */
	#if	FEED_DEBUG
	printf("decrypt  clue %d\n", clueByte);
	printf("  xp : "); printGiant(finst->xp);
	printf("  xm : "); printGiant(finst->xm);
	printf("  cluePlus  :"); printGiant(finst->cluePlus);
	printf("  clueMinus :"); printGiant(finst->clueMinus);
	#endif	// FEED_DEBUG

	if(finalBlock) {
		/*
		 * Snag data from xp in order to find out how much to move to
		 * *plainText
		 */
		unsigned char *ptext = (unsigned char*) fmalloc(finst->plainBlockSize);

		serializeGiant(finst->xp, ptext, finst->plainBlockSize);
		*plainTextLen = ptext[finst->plainBlockSize - 1];
		if(*plainTextLen == RESID_ZERO) {
			bprintf(("=== FEED Decrypt: RESID_ZERO\n"));
			*plainTextLen = 0;
		}
		else if(*plainTextLen > (finst->plainBlockSize - 1)) {
			dbgLog(("feeFEEDDecryptBlock: ptext overflow!\n"));
			bprintf(("feeFEEDDecryptBlock: ptext overflow!\n"));
			frtn = FR_BadCipherText;
		}
		else {
			bprintf(("=== FEED Decrypt: resid len 0x%x\n", *plainTextLen));
			bcopy(ptext, plainText, *plainTextLen);
		}
		ffree(ptext);
	}
	else {
		*plainTextLen = finst->plainBlockSize;
		serializeGiant(finst->xp, plainText, *plainTextLen);
	}
	bprintf(("=== FEED decryptBlock ptextLen 0x%x  ctextLen 0x%x\n",
		*plainTextLen, cipherTextLen));

	return frtn;
}

/*
 * Convenience routines to encrypt & decrypt multi-block data.
 */
feeReturn feeFEEDEncrypt(feeFEED feed,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char **cipherText,		// malloc'd and RETURNED
	unsigned *cipherTextLen)		// RETURNED
{
	const unsigned char	*ptext;			// per block
	unsigned			ptextLen;		// total to go
	unsigned			thisPtextLen;		// per block
	unsigned char		*ctext;			// per block
	unsigned			ctextLen;		// per block
	unsigned char		*ctextResult;		// to return
	unsigned			ctextResultLen;		// size of ctextResult
	unsigned char		*ctextPtr;
	unsigned 			ctextLenTotal;		// running total
	feeReturn			frtn;
	int					finalBlock;
	unsigned			numBlocks;
	unsigned			plainBlockSize;
	#if	FEE_DEBUG
	unsigned		expectedCtextSize;

	expectedCtextSize = feeFEEDCipherTextSize(feed, plainTextLen, 1);
	#endif
	
	if(plainTextLen == 0) {
		dbgLog(("feeFEEDDecrypt: NULL plainText\n"));
		return FR_IllegalArg;
	}

	ptext = plainText;
	ptextLen = plainTextLen;
	ctext = (unsigned char*) fmalloc(feeFEEDCipherBufSize(feed, 1));
	plainBlockSize = feeFEEDPlainBlockSize(feed);
	numBlocks = (plainTextLen + plainBlockSize - 1)/plainBlockSize;

	/*
	 * Calculate the worst-case size needed to hold all of the ciphertext
	 */
	ctextResultLen = feeFEEDCipherTextSize(feed, plainTextLen, 1);
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
		frtn = feeFEEDEncryptBlock(feed,
			ptext,
			thisPtextLen,
			ctext,
			&ctextLen,
			finalBlock);
		if(frtn) {
			dbgLog(("feeFEEDEncrypt: encrypt error: %s\n",
				feeReturnString(frtn)));
			break;
		}
		if(ctextLen == 0) {
			dbgLog(("feeFEEDEncrypt: null ciphertext\n"));
			frtn = FR_Internal;
			break;
		}
		bcopy(ctext, ctextPtr, ctextLen);
		ctextLenTotal += ctextLen;
		if(ctextLenTotal > ctextResultLen) {
			dbgLog(("feeFEEDEncrypt: ciphertext overflow\n"));
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
		if(expectedCtextSize != ctextLenTotal) {
		    printf("feeFEEDEncrypt: feeFEEDCipherTextSize error!\n");
		    printf("ptext %d  exp ctext %d  actual ctext %d\n",
		    	plainTextLen,
			expectedCtextSize,
			ctextLenTotal);
		}
		#endif	// FEE_DEBUG
	}
	return frtn;

}

feeReturn feeFEEDDecrypt(feeFEED feed,
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
	unsigned		plainBlockSize = feeFEEDPlainBlockSize(feed);
	unsigned		cipherBlockSize = feeFEEDCipherBlockSize(feed);

	if(cipherTextLen % cipherBlockSize) {
		dbgLog(("feeFEEDDecrypt: unaligned cipherText\n"));
		return FR_BadCipherText;
	}
	if(cipherTextLen == 0) {
		dbgLog(("feeFEEDDecrypt: NULL cipherText\n"));
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
		frtn = feeFEEDDecryptBlock(feed,
			ctext,
			cipherBlockSize,
			ptext,
			&ptextLen,
			finalBlock);
		if(frtn) {
			dbgLog(("feeFEEDDecryptBlock: %s\n",
				feeReturnString(frtn)));
			break;
		}
		if(ptextLen) {
			if(ptextLen > plainBlockSize) {
			    dbgLog(("feeFEEDDecrypt: ptext overflow!\n"));
			    frtn = FR_Internal;
			    break;
			}
			bcopy(ptext, ptextPtr, ptextLen);
			ptextPtr += ptextLen;
			ptextLenTotal += ptextLen;
		}
		/*
		 * note ptextLen == 0 is normal termination case for
		 * plainTextLen % plainBlockSize == 0.
		 * Also expected for first 4 blocks of ciphertext;
		 * proceed (we break when ctextLen is exhausted).
		 */
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
