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
 * FeeDES.c - generic, portable DES encryption object
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 05 Jan 98 at Apple
 *	Avoid a bcopy() on encrypt/decrypt of each block
 * 31 Mar 97 at Apple
 *	New per-instance API for DES.c
 * 26 Aug 96 at NeXT
 *	Created.
 */

#include "ckconfig.h"

#if	CRYPTKIT_SYMMETRIC_ENABLE

#include "feeDES.h"
#include "feeTypes.h"
#include "ckDES.h"
#include "falloc.h"
#include "feeDebug.h"
#include "feeFunctions.h"
#include "platform.h"
#include <stdlib.h>

#ifndef	NULL
#define NULL	((void *)0)
#endif	/* NULL */

typedef struct {
	int		blockMode;			/* default = 0 */
	unsigned char 	lastBlock[DES_BLOCK_SIZE_BYTES];	/* for CBC */
	struct _desInst	dinst;
} fdesInst;

static void feeDESInit(desInst dinst)
{
	desinit(dinst, DES_MODE_STD);		// detects redundant calls
}

/*
 * Alloc and init a feeDES object with specified initial state.
 * State must be at least 8 bytes; only 8 bytes are used, ignoring
 * MSB of each bytes.
 */
feeDES feeDESNewWithState(const unsigned char *state,
	unsigned stateLen)
{
	fdesInst *fdinst;

	if(stateLen < FEE_DES_MIN_STATE_SIZE) {
		return NULL;
	}
	fdinst = (fdesInst*) fmalloc(sizeof(fdesInst));
	bzero(fdinst, sizeof(fdesInst));
	feeDESInit(&fdinst->dinst);
	feeDESSetState((feeDES)fdinst, state, stateLen);
	return fdinst;
}

void feeDESFree(feeDES des)
{
	memset(des, 0, sizeof(fdesInst));
	ffree(des);
}

/*
 * Set new initial state.
 */
feeReturn feeDESSetState(feeDES des,
	const unsigned char *state,
	unsigned stateLen)
{
	fdesInst *fdinst = (fdesInst*) des;
	char Key[DES_KEY_SIZE_BYTES_EXTERNAL];	
					// 'key' causes problems with
					// some weird Unix header
	unsigned byte;

	if(stateLen < (DES_KEY_SIZE_BYTES_EXTERNAL)) {
		return FR_IllegalArg;
	}
	bzero(fdinst->lastBlock, DES_BLOCK_SIZE_BYTES);
	bcopy(state, Key, DES_KEY_SIZE_BYTES_EXTERNAL);

	/*
	 * Set up parity bits
	 */
	for(byte=0; byte<DES_KEY_SIZE_BYTES_EXTERNAL; byte++){
	    int i;
	    unsigned p;

	    p = 0;
	    for(i=0;i<7;i++) {
		if(Key[byte] & (1 << i)) {
		    p++;
		}
	    }
	    if((p & 1) == 0) {
		Key[byte] |= 0x80;
	    }
	    else {
		Key[byte] &= ~0x80;
	    }
	}
	dessetkey(&fdinst->dinst, Key);
	return FR_Success;
}

void feeDESSetBlockMode(feeDES des)
{
	fdesInst *fdinst = (fdesInst*) des;

	fdinst->blockMode = 1;
}

void feeDESSetChainMode(feeDES des)
{
	fdesInst *fdinst = (fdesInst*) des;

	fdinst->blockMode = 0;
}

unsigned feeDESPlainBlockSize(feeDES des)
{
	return DES_BLOCK_SIZE_BYTES;
}

unsigned feeDESCipherBlockSize(feeDES des)
{
	return DES_BLOCK_SIZE_BYTES;
}

unsigned feeDESCipherBufSize(feeDES des)
{
	/*
	 * Normally DES_BLOCK_SIZE, two blocks for finalBlock
	 */
	return 2 * DES_BLOCK_SIZE_BYTES;
}

/*

 * Return the size of ciphertext to hold specified size of plaintext.

 */

unsigned feeDESCipherTextSize(feeDES des, unsigned plainTextSize)

{

	unsigned blocks = (plainTextSize + DES_BLOCK_SIZE_BYTES - 1) /
	    DES_BLOCK_SIZE_BYTES;

	if((plainTextSize % DES_BLOCK_SIZE_BYTES) == 0) {
		/*
		 * One more block for resid count
		 */
		blocks++;
	}

	return blocks * DES_BLOCK_SIZE_BYTES;

}


/*
 * Key size in bits.
 */
unsigned feeDESKeySize(feeDES des)
{
	return DES_KEY_SIZE_BITS;
}

/*
 * Encrypt a block or less of data. Caller malloc's cipherText.
 */
feeReturn feeDESEncryptBlock(feeDES des,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char *cipherText,
	unsigned *cipherTextLen,		// RETURNED
	int finalBlock)
{
	fdesInst *fdinst = (fdesInst*) des;
	feeReturn frtn = FR_Success;
	unsigned cipherLen;

	if(plainTextLen > DES_BLOCK_SIZE_BYTES) {
		return FR_IllegalArg;
	}
	if(plainTextLen) {
		/*
		 * We're called with plainTextLen = 0 and finalBlock
		 * recursively to clean up last block.
		 */
		bcopy(plainText, cipherText, plainTextLen);
	}
	if(plainTextLen < DES_BLOCK_SIZE_BYTES) {
		if(!finalBlock) {
			/*
			 * odd-size block only legal last time thru
			 */
			return FR_IllegalArg;
		}

		/*
		 * Last block, final byte = residual length.
		 */
		cipherText[DES_BLOCK_SIZE_BYTES - 1] = plainTextLen;
	}

        if(!fdinst->blockMode) {
		/*
		 * CBC mode; chain in last cipher word
		 */
		unsigned char *cp = cipherText;
		unsigned char *cp1 = fdinst->lastBlock;
		int i;

		for(i=0; i<DES_BLOCK_SIZE_BYTES; i++) {
		    *cp++ ^= *cp1++;
		}
        }
        endes(&fdinst->dinst, (char *)cipherText);	/* Encrypt block */
        if(!fdinst->blockMode){
		/*
		 * Save outgoing ciphertext for chain
		 */
			bcopy(cipherText, fdinst->lastBlock, DES_BLOCK_SIZE_BYTES);
        }
	cipherLen = DES_BLOCK_SIZE_BYTES;

	if(finalBlock) {
	    if(plainTextLen == DES_BLOCK_SIZE_BYTES) {
	       /*
		* Special case: finalBlock true, plainTextLen == blockSize.
		* In this case we generate one more block of ciphertext,
		* with a resid length of zero.
		*/
		unsigned moreCipher;			// additional cipherLen

		frtn = feeDESEncryptBlock(des,
			NULL,				// plainText not used
			0,				// resid
			cipherText + DES_BLOCK_SIZE_BYTES,	// append...
			&moreCipher,
			1);
		if(frtn == FR_Success) {
			cipherLen += moreCipher;
		}

	    }
	    if(plainTextLen != 0) {
		/*
		 * Reset internal state in prep for next encrypt/decrypt.
		 * Note we avoid this in the recursive call (plainTextLen = 0).
		 */
		bzero(fdinst->lastBlock, DES_BLOCK_SIZE_BYTES);
	    }
	}

	if(frtn == FR_Success) {
		*cipherTextLen = cipherLen;
	}
	return frtn;
}

/*
 * Decrypt a block of data. Caller malloc's plainText. Always
 * generates DES_BLOCK_SIZE_BYTES bytes or less of plainText.
 */
feeReturn feeDESDecryptBlock(feeDES des,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char *plainText,
	unsigned *plainTextLen,		// RETURNED
	int finalBlock)
{
	fdesInst *fdinst = (fdesInst*) des;
	unsigned char work[DES_BLOCK_SIZE_BYTES];
	unsigned char ivtmp[DES_BLOCK_SIZE_BYTES];

	if(cipherTextLen != DES_BLOCK_SIZE_BYTES) {
		/*
		 * We always generate ciphertext in multiples of block size.
		 */
		return FR_IllegalArg;
	}

        bcopy(cipherText, work, DES_BLOCK_SIZE_BYTES);
        if(!fdinst->blockMode && !finalBlock) {
		/*
		 * Save incoming ciphertext for chain
		 */
            	bcopy(cipherText, ivtmp, DES_BLOCK_SIZE_BYTES);
        }
        dedes(&fdinst->dinst, (char *)work);
        if(!fdinst->blockMode){
		/*
		 * Unchain block using previous block's ciphertext;
		 * save current ciphertext for next
		 */
		char *cp = (char *)work;
		char *cp1 = (char*)fdinst->lastBlock;
		int i;

		for(i=0; i<DES_BLOCK_SIZE_BYTES; i++) {
		    *cp++ ^= *cp1++;
		}
		if(!finalBlock) {
		    bcopy(ivtmp, fdinst->lastBlock, DES_BLOCK_SIZE_BYTES);
		}
        }
	if(finalBlock) {
		/*
		 * deal with residual block; its size is in last byte of
		 * work[]
		 */
		unsigned resid = work[DES_BLOCK_SIZE_BYTES-1];

		if(resid > (DES_BLOCK_SIZE_BYTES-1)) {
			return FR_BadCipherText;
		}
		if(resid > 0) {
			bcopy(work, plainText, resid);
		}
		*plainTextLen = resid;

		/*
		 * Reset internal state in prep for next encrypt/decrypt.
		 */
		bzero(fdinst->lastBlock, DES_BLOCK_SIZE_BYTES);
	}
	else {
		bcopy(work, plainText, DES_BLOCK_SIZE_BYTES);
		*plainTextLen = DES_BLOCK_SIZE_BYTES;
	}
	return FR_Success;
}

/*
 * Convenience routines to encrypt & decrypt multi-block data.
 */
feeReturn feeDESEncrypt(feeDES des,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char **cipherText,		// malloc'd and RETURNED
	unsigned *cipherTextLen)		// RETURNED
{
	const unsigned char	*ptext;			// per block
	unsigned		ptextLen;		// total to go
	unsigned		thisPtextLen;		// per block
	unsigned		ctextLen;		// per block
	unsigned char		*ctextResult;		// to return
	unsigned char		*ctextPtr;
	unsigned 		ctextLenTotal;		// running total
	feeReturn		frtn;
	int			finalBlock;
	unsigned		ctextMallocd;

	if(plainTextLen == 0) {
		dbgLog(("feeDESDecrypt: NULL plainText\n"));
		return FR_IllegalArg;
	}

	ptext = plainText;
	ptextLen = plainTextLen;
	ctextMallocd = feeDESCipherTextSize(des, plainTextLen);
	ctextResult = (unsigned char*) fmalloc(ctextMallocd);
	ctextPtr = ctextResult;
	ctextLenTotal = 0;

	while(1) {
		if(ptextLen <= DES_BLOCK_SIZE_BYTES) {
			finalBlock = 1;
			thisPtextLen = ptextLen;
		}
		else {
			finalBlock = 0;
			thisPtextLen = DES_BLOCK_SIZE_BYTES;
		}
		frtn = feeDESEncryptBlock(des,
			ptext,
			thisPtextLen,
			ctextPtr,
			&ctextLen,
			finalBlock);
		if(frtn) {
			dbgLog(("feeDESEncrypt: encrypt error: %s\n",
				feeReturnString(frtn)));
			break;
		}
		if(ctextLen == 0) {
			dbgLog(("feeDESEncrypt: null ciphertext\n"));
			frtn = FR_Internal;
			break;
		}
		ctextLenTotal += ctextLen;
		if(ctextLenTotal > (plainTextLen + DES_BLOCK_SIZE_BYTES)) {
			dbgLog(("feeDESEncrypt: ciphertext overflow\n"));
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
	if(frtn) {
		ffree(ctextResult);
		*cipherText = NULL;
		*cipherTextLen = 0;
	}
	else {
		#if	FEE_DEBUG
		if(ctextLenTotal != ctextMallocd) {
			dbgLog(("feeDESEncrypt: ctextLen error\n"));
		}
		#endif	/* FEE_DEBUG */
		*cipherText = ctextResult;
		*cipherTextLen = ctextLenTotal;
	}
	return frtn;

}

feeReturn feeDESDecrypt(feeDES des,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char **plainText,		// malloc'd and RETURNED
	unsigned *plainTextLen)			// RETURNED
{
	const unsigned char	*ctext;
	unsigned		ctextLen;		// total to go
	unsigned		ptextLen;		// per block
	unsigned char		*ptextResult;		// to return
	unsigned char		*ptextPtr;
	unsigned 		ptextLenTotal;		// running total
	feeReturn		frtn = FR_Success;
	int			finalBlock;

	if(cipherTextLen % DES_BLOCK_SIZE_BYTES) {
		dbgLog(("feeDESDecrypt: unaligned cipherText\n"));
		return FR_BadCipherText;
	}
	if(cipherTextLen == 0) {
		dbgLog(("feeDESDecrypt: NULL cipherText\n"));
		return FR_BadCipherText;
	}

	ctext = cipherText;
	ctextLen = cipherTextLen;

	/*
	 * Plaintext length always <= cipherTextLen
	 */
	ptextResult = (unsigned char*) fmalloc(cipherTextLen);
	ptextPtr = ptextResult;
	ptextLenTotal = 0;

	while(ctextLen) {
		if(ctextLen == DES_BLOCK_SIZE_BYTES) {
		    finalBlock = 1;
		}
		else {
		    finalBlock = 0;
		}
		frtn = feeDESDecryptBlock(des,
			ctext,
			DES_BLOCK_SIZE_BYTES,
			ptextPtr,
			&ptextLen,
			finalBlock);
		if(frtn) {
			dbgLog(("feeDESDecrypt decrypt: %s\n",
				feeReturnString(frtn)));
			break;
		}
		if(ptextLen == 0) {
			/*
			 * Normal termination case for
			 * plainTextLen % DES_BLOCK_SIZE_BYTES == 0
			 */
			if(!finalBlock) {
				dbgLog(("feeDESDecrypt: decrypt sync"
					" error!\n"));
				frtn = FR_BadCipherText;
				break;
			}
			else {
				break;
			}
		}
		else {
			ptextPtr += ptextLen;
			ptextLenTotal += ptextLen;
		}
		ctext += DES_BLOCK_SIZE_BYTES;
		ctextLen -= DES_BLOCK_SIZE_BYTES;
	}

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

#endif	/* CRYPTKIT_SYMMETRIC_ENABLE */
