/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * BlockCryptor.cpp - common context for block-oriented encryption algorithms
 *
 * Created March 5 2001 by dmitch
 */

#include "BlockCryptor.h"
#include "BinaryKey.h"
#include "AppleCSPSession.h"
#include <security_utilities/alloc.h>
#include <Security/cssmerr.h>
#include <string.h>
#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmdata.h>

#define BlockCryptDebug(args...)	secdebug("blockCrypt", ## args)
#define bprintf(args...)			secdebug("blockCryptBuf", ## args)
#define ioprintf(args...)			secdebug("blockCryptIo", ## args)

BlockCryptor::~BlockCryptor()
{
	if(mInBuf) {
		memset(mInBuf, 0, mInBlockSize);
		session().free(mInBuf);
		mInBuf = NULL;
	}
	if(mChainBuf) {
		memset(mChainBuf, 0, mInBlockSize);
		session().free(mChainBuf);
		mChainBuf = NULL;
	}
	mInBufSize = 0;
}

/* 
 * Reusable setup functions called from subclass's init.
 * This is the general purpose one....
 */
void BlockCryptor::setup(
		size_t			blockSizeIn,	// block size of input 
		size_t			blockSizeOut,	// block size of output 
		bool			pkcsPad,		// this class performs PKCS{5,7} padding
		bool			needsFinal,		// needs final update with valid data
		BC_Mode			mode,			// ECB, CBC
		const CssmData	*iv)			// init vector, required for CBC
										//Ê  must be at least blockSizeIn bytes
{
	if(pkcsPad && needsFinal) {
		BlockCryptDebug("BlockCryptor::setup pkcsPad && needsFinal");
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	mPkcsPadding = pkcsPad;
	mMode = mode;
	mNeedFinalData = needsFinal;
	
	/* set up inBuf, all configurations */
	if(mInBuf != NULL) {
		/* only reuse if same size */
		if(mInBlockSize != blockSizeIn) {
			session().free(mInBuf);
			mInBuf = NULL;
		}
	}
	if(mInBuf == NULL) {
		mInBuf = (uint8 *)session().malloc(blockSizeIn);
	}
	
	/* set up chain buf, decrypt/CBC only; skip if algorithm does its own chaining */
	if((mMode == BCM_CBC) && !encoding() && !mCbcCapable) {
		if(mChainBuf != NULL) {
			/* only reuse if same size */
			if(mInBlockSize != blockSizeIn) {
				session().free(mChainBuf);
				mChainBuf = NULL;
			}
		}
		if(mChainBuf == NULL) {
			mChainBuf = (uint8 *)session().malloc(blockSizeIn);
		}
	}
	
	/* IV iff CBC mode, and ensure IV is big enough */
	switch(mMode) {
		case BCM_ECB:
			if(iv != NULL) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR);
			}
			break;
		case BCM_CBC:
			if(iv == NULL) {
				CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
			}
			if(blockSizeIn != blockSizeOut) {
				/* no can do, must be same block sizes */
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_MODE);
			}
			if(iv->Length < blockSizeIn) {
				/* not enough IV */
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR);
			}
			/* save IV as appropriate */
			if(!mCbcCapable) {
				if(encoding()) {
					memmove(mInBuf, iv->Data, blockSizeIn);
				}
				else {
					assert(mChainBuf != NULL);
					memmove(mChainBuf, iv->Data, blockSizeIn);
				}
			}
			break;
	}

	mInBlockSize = blockSizeIn;
	mInBufSize = 0;
	mOutBlockSize = blockSizeOut;
	mOpStarted = false;
}

/*
 * This one is used by simple, well-behaved algorithms which don't do their own
 * padding and which rely on us to do everything but one-block-at-a-time
 * encrypt and decrypt.
 */
void BlockCryptor::setup(
	size_t			blockSize,		// block size of input and output
	const Context 	&context)
{
	bool 		padEnable 	= false;
	bool 		chainEnable = false;
	bool 		ivEnable 	= false;
	CssmData 	*iv			= NULL;
	
	/* 
	 * Validate context 
	 * IV optional per mode
	 * pad optional per mode 
	 * Currently we ignore extraneous attributes (e.g., it's OK to pass in
	 * an IV if the mode doesn't specify it), mainly for simplifying test routines.
	 */
	CSSM_ENCRYPT_MODE cssmMode = context.getInt(CSSM_ATTRIBUTE_MODE);

    switch (cssmMode) {
		/* no mode attr --> 0 == CSSM_ALGMODE_NONE, not currently supported */
 		case CSSM_ALGMODE_CBCPadIV8:
			padEnable = true;
			ivEnable = true;
			chainEnable = true;
			break;

		case CSSM_ALGMODE_CBC_IV8: 
			ivEnable = true;
			chainEnable = true;
			break;
			
		case CSSM_ALGMODE_ECB:
			break;
			
		case CSSM_ALGMODE_ECBPad:
			padEnable = true;
			break;
			
		default:
			errorLog1("DESContext::init: illegal mode (%d)\n", (int)cssmMode);
            CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_MODE);
	}
	
	if(padEnable) {
		/* validate padding type */
		uint32 padding = context.getInt(CSSM_ATTRIBUTE_PADDING); // 0 ==> PADDING_NONE
		if(blockSize == 8) {
			switch(padding) {
				/* backwards compatibility - used to be PKCS1, should be PKCS5 or 7 */
				case CSSM_PADDING_PKCS7:
				case CSSM_PADDING_PKCS5:
				case CSSM_PADDING_PKCS1:		//Êthis goes away soon
					/* OK */
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
			}
		}
		else {
			switch(padding) {
				case CSSM_PADDING_PKCS5:		// this goes away soon
				case CSSM_PADDING_PKCS7:
					/* OK */
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
			}
		}
	}
	if(ivEnable) {
		/* make sure there's an IV in the context of sufficient length */
		iv = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR);
		if(iv == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
		}
		if(iv->Length < blockSize) {
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR);
		}
	}
	setup(blockSize, 
		blockSize, 
		padEnable, 
		false,				// needsFinal 
		chainEnable ? BCM_CBC : BCM_ECB,
		iv);
}

/*
 * Update always leaves some data in mInBuf if:
 *    mNeedsFinalData is true, or
 *    decrypting and mPkcsPadding true. 
 * Also, we always process all of the input (except on error). 
 */
void BlockCryptor::update(
	void 			*inp, 
	size_t 			&inSize, 			// in/out
	void 			*outp, 
	size_t 			&outSize)			// in/out
{
	uint8 		*uInp = (UInt8 *)inp;
	uint8 		*uOutp = (UInt8 *)outp;
	size_t	 	uInSize = inSize;		// input bytes to go
	size_t 		uOutSize = 0;			// ouput bytes generated
	size_t		uOutLeft = outSize;		// bytes remaining in outp
	size_t 		toMove;
	size_t		actMoved;
	unsigned	i;
	bool		needLeftOver = mNeedFinalData || (!encoding() && mPkcsPadding);
	bool		doCbc = (mMode == BCM_CBC) && !mCbcCapable;
	
	assert(mInBuf != NULL);
	mOpStarted = true;
	
	if(mInBufSize) {
		/* attempt to fill mInBuf from inp */
		toMove = mInBlockSize - mInBufSize;
		if(toMove > uInSize) {
			toMove = uInSize;
		}
		if(encoding() && doCbc) {
			/* xor into last cipherblock or IV */
			for(i=0; i<toMove; i++) {
				mInBuf[mInBufSize + i] ^= *uInp++;
			}
		}
		else {
			/* use incoming data as is */
			memmove(mInBuf+mInBufSize, uInp, toMove);
			uInp += toMove;
		}
		uInSize    -= toMove;
		mInBufSize += toMove;
		/* 
		 * Process inBuf if it's full, but skip if no more data in uInp and
		 * inBuf might be needed (by us for unpadding on decrypt, or by
		 * subclass for anything) for a final call 
		 */
		if((mInBufSize == mInBlockSize) && !((uInSize == 0) && needLeftOver)) {
			actMoved = uOutLeft;
			if(encoding()) {
				encryptBlock(mInBuf, mInBlockSize, uOutp, actMoved, false);
				if(doCbc) {
					/* save ciphertext for chaining next block */
					assert(mInBlockSize == actMoved);
					memmove(mInBuf, uOutp, mInBlockSize);
				}
			}
			else {
				decryptBlock(mInBuf, mInBlockSize, uOutp, actMoved, false);
				if(doCbc) {
					/* xor in last ciphertext */
					assert(mInBlockSize == actMoved);
					for(i=0; i<mInBlockSize; i++) {
						uOutp[i] ^= mChainBuf[i];
					}
					/* save this ciphertext for next chain */
					memmove(mChainBuf, mInBuf, mInBlockSize);
				}
			}
			uOutSize += actMoved;
			uOutp    += actMoved;
			uOutLeft -= actMoved;
			mInBufSize = 0;
			assert(uOutSize <= outSize);
		}
	}	/* processing mInBuf */
	if(uInSize == 0) {
		/* done */
		outSize = uOutSize;
		ioprintf("=== BlockCryptor::update encrypt %d   inSize 0x%lx  outSize 0x%lx",
			encoding() ? 1 : 0, inSize, outSize);
		return;
	}
	
	
	/* 
	 * en/decrypt even blocks in (remaining) inp.  
	 */
	size_t leftOver = uInSize % mInBlockSize;
	if((leftOver == 0) && needLeftOver) {
		/* 
		 * Even blocks coming in, but we really need to leave some data
		 * in the buffer (because the subclass asked for it, or we're decrypting
		 * with PKCS padding). Save one block for mInBuf.
		 */
		leftOver = mInBlockSize; 
	}
	toMove = uInSize - leftOver;
	size_t blocks = toMove / mInBlockSize;
	if(mMultiBlockCapable && !doCbc && (blocks != 0)) {
		/* 
		 * Optimization for algorithms that are multi-block capable and that
		 * can do their own CBC (if necessary).
		 */
		size_t thisMove = blocks * mInBlockSize;
		actMoved = uOutLeft;
		if(encoding()) {
			encryptBlock(uInp, thisMove, uOutp, actMoved, false);
		}
		else {
			decryptBlock(uInp, thisMove, uOutp, actMoved, false);
		}
		uOutSize += actMoved;
		uOutp    += actMoved;
		uInp	 += thisMove;
		uOutLeft -= actMoved;
		toMove   -= thisMove; 
		assert(uOutSize <= outSize);
	}
	else if(encoding()) {
		while(toMove) {
			actMoved = uOutLeft;
			if(!doCbc) {
				/* encrypt directly from input to output */
				encryptBlock(uInp, mInBlockSize, uOutp, actMoved, false);
			}
			else {
				/* xor into last ciphertext, encrypt the result */
				for(i=0; i<mInBlockSize; i++) {
					mInBuf[i] ^= uInp[i];
				}
				encryptBlock(mInBuf, mInBlockSize, uOutp, actMoved, false);
				
				/* save new ciphertext for next chain */
				assert(actMoved == mInBlockSize);
				memmove(mInBuf, uOutp, mInBlockSize);
			}
			uOutSize += actMoved;
			uOutp    += actMoved;
			uInp	 += mInBlockSize;
			uOutLeft -= actMoved;
			toMove   -= mInBlockSize; 
			assert(uOutSize <= outSize);
		}	/* main encrypt loop */

	}	
	else {
		/* decrypting */
		while(toMove) {
			actMoved = uOutLeft;
			if(doCbc) {
				/* save this ciphertext for chain; don't assume in != out */
				memmove(mInBuf, uInp, mInBlockSize);
				decryptBlock(uInp, mInBlockSize, uOutp, actMoved, false);
				
				/* chain in previous ciphertext */
				assert(mInBlockSize == actMoved);
				for(i=0; i<mInBlockSize; i++) {
					uOutp[i] ^= mChainBuf[i];
				}
				
				/* save current ciphertext for next block */
				memmove(mChainBuf, mInBuf, mInBlockSize);
			}
			else {
				/* ECB */
				decryptBlock(uInp, mInBlockSize, uOutp, actMoved, false);
			}
			uOutSize += actMoved;
			uOutp    += actMoved;
			uInp	 += mInBlockSize;
			uOutLeft -= actMoved;
			toMove   -= mInBlockSize; 
			assert(uOutSize <= outSize);
		}	/* main decrypt loop */

	}
	
	/* leftover bytes from inp --> mInBuf */
	if(leftOver) {
		if(encoding() && doCbc) {
			/* xor into last cipherblock or IV */
			for(i=0; i<leftOver; i++) {
				mInBuf[i] ^= *uInp++;
			}
		}
		else {
			if(mInBuf && uInp && leftOver) memmove(mInBuf, uInp, leftOver);
		}
	}

	mInBufSize = leftOver;
	outSize = uOutSize;
	ioprintf("=== BlockCryptor::update encrypt %d   inSize 0x%lx  outSize 0x%lx",
		encoding() ? 1 : 0, inSize, outSize);
}
	
void BlockCryptor::final(
	CssmData 		&out)
{
	size_t 		uOutSize = 0;			// ouput bytes generated
	size_t		actMoved;
	size_t		uOutLeft = out.Length;	// bytes remaining in out
	unsigned	i;
	bool		doCbc = (mMode == BCM_CBC) && !mCbcCapable;
	
	assert(mInBuf != NULL);
	mOpStarted = true;
	if((mInBufSize == 0) && mNeedFinalData) {
		/* only way this could happen: no update() called (at least not with 
			* non-zero input data sizes) */
		BlockCryptDebug("BlockCryptor::final with no mInBuf data");
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
	}
	if(encoding()) {
		uint8 *ctext = out.Data;
		
		if(mPkcsPadding) {
			/* 
			 * PKCS5/7 padding: pad byte = size of padding. 
			 * This assertion courtesy of the limitation on the mutual
			 * exclusivity of mPkcsPadding and mNeedFinalData. 
			 */
			assert(mInBufSize < mInBlockSize);
			size_t padSize = mInBlockSize - mInBufSize;
			uint8 *padPtr  = mInBuf + mInBufSize;
			if(!doCbc) {
				for(i=0; i<padSize; i++) {
					*padPtr++ = padSize;
				}
			}
			else {
				for(i=0; i<padSize; i++) {
					*padPtr++ ^= padSize;
				}
			}
			mInBufSize = mInBlockSize;
		}	/* PKCS padding */
		
		/*
		 * Encrypt final mInBuf. If it's not full, the BlockCryptObject better know
		 * how to pad....
		 */
		if(mInBufSize) {
			actMoved = uOutLeft;
			encryptBlock(mInBuf, mInBufSize, ctext, actMoved, true);
			uOutSize += actMoved;
			mInBufSize = 0;
			assert(uOutSize <= out.length());
		}
		out.length(uOutSize);
	}	/* encrypting */
	
	else {
		if(mInBufSize == 0) {
			if(mPkcsPadding) {
				BlockCryptDebug("BlockCryptor::final decrypt/pad with no mInBuf data");
				CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
			}
			else {
				/* simple decrypt op complete */
				ioprintf("=== BlockCryptor::final  encrypt 0   outSize 0");
				out.length(0);
				return;
			}
		}
		
		/*
		 * Decrypt - must have exactly one block of ciphertext.
		 * We trust CSPContext, and our own outputSize(), to have set up
		 * the current output buffer with enough space to handle the 
		 * full size of the decrypt, even though - due to padding - we
		 * might actually pass less than that amount back to caller. 
		 */
		if(mInBufSize != mInBlockSize) {
			BlockCryptDebug("BlockCryptor::final unaligned ciphertext");
			CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
		}
		
		uint8 *ptext = out.Data;
		actMoved = uOutLeft;
		decryptBlock(mInBuf, mInBlockSize, ptext, actMoved, true);
		if(doCbc) {
			/* chain in previous ciphertext one more time */
			assert(mInBlockSize == actMoved);
			for(i=0; i<mInBlockSize; i++) {
				ptext[i] ^= mChainBuf[i];
			}
		}
		if(mPkcsPadding) {
			assert(actMoved == mOutBlockSize);

			/* ensure integrity of padding byte(s) */
			unsigned padSize = ptext[mOutBlockSize - 1];
			if(padSize > mOutBlockSize) {
				BlockCryptDebug("BlockCryptor::final malformed ciphertext (1)");
				CssmError::throwMe(CSSM_ERRCODE_INVALID_DATA);
			}
			uint8 *padPtr = ptext + mOutBlockSize - padSize;
			for(unsigned i=0; i<padSize; i++) {
				if(*padPtr++ != padSize) {
					BlockCryptDebug("BlockCryptor::final malformed ciphertext "
							"(2)");
					CssmError::throwMe(CSSM_ERRCODE_INVALID_DATA);
				}
			}
			actMoved -= padSize;
		}
		assert(actMoved <= out.length());
		out.length(actMoved);
	}	/* decrypting */
	ioprintf("=== BlockCryptor::final  encrypt %d   outSize 0x%lx",
		encoding() ? 1 : 0, out.Length);
}

/* 
 * These three are only valid for algorithms for which encrypting one block 
 * of plaintext always yields exactly one block of ciphertext, and vice versa 
 * for decrypt. The block sizes for plaintext and ciphertext do NOT have to be 
 * the same. Subclasses (e.g. FEED) which do not meet this criterion will have 
 * to override.
 */
 
void BlockCryptor::minimumProgress(
	size_t 			&inSize, 
	size_t 			&outSize)
{
	/* each size = one block (including buffered input) */
    inSize  = mInBlockSize - mInBufSize;
	if(inSize == 0) {
		/* i.e., we're holding a whole buffer */
		inSize++;
	}
	outSize = mOutBlockSize;
	bprintf("--- BlockCryptor::minProgres inSize 0x%lx outSize 0x%lx mInBufSize 0x%lx",
		inSize, outSize, mInBufSize);
}

size_t BlockCryptor::inputSize(
	size_t 			outSize)			// input for given output size
{
	size_t inSize;
	
	if(outSize < mOutBlockSize) {
		/* 
		 * Sometimes CSPFullPluginSession calls us like this....in this
		 * case the legal inSize is just the remainder of the input buffer,
		 * less one byte (in other words, the max we we gobble up without
		 * producing any output). 
		 */
		inSize = mInBlockSize - mInBufSize;
		if(inSize == 0) {
			/* we have a full input buffer! How can this happen!? */
			BlockCryptDebug("BlockCryptor::inputSize: HELP! zero inSize and outSize!\n");
		}
	}
	else {
		/* more-or-less normal case */
		size_t wholeBlocks = outSize / mOutBlockSize;
		assert(wholeBlocks >= 1);
		inSize = (wholeBlocks * mInBlockSize) - mInBufSize;
		if(inSize == 0) {
			/* i.e., we're holding a whole buffer */
			inSize++;
		}
	}
	bprintf("--- BlockCryptor::inputSize  inSize 0x%lx outSize 0x%lx mInBufSize 0x%lx",
		inSize, outSize, mInBufSize);
	return inSize;
}

size_t BlockCryptor::outputSize(
	bool 			final,
	size_t 			inSize /*= 0*/) 		// output for given input size
{
	size_t rawBytes = inSize + mInBufSize;
	// huh?Êdon't round this up!
	//size_t rawBlocks = (rawBytes + mInBlockSize - 1) / mInBlockSize;
	size_t rawBlocks = rawBytes / mInBlockSize;

	/*
	 * encrypting: always get one additional block on final() if we're padding 
	 *             or (we presume) the subclass is padding. Note that we
	 *			   truncated when calculating rawBlocks; to finish out on the 
	 *			   final block, we (or our subclass) will either have to pad
	 *			   out the current partial block, or cook up a full pad block if
	 *			   mInBufSize is currently zero. Subclasses which pad some other
	 *			   way need to override this method. 
	 *
	 * decrypting: outsize always <= insize
	 */
	if(encoding() && final && (mPkcsPadding || mNeedFinalData)) {
		rawBlocks++;
	}
	
	/* FIXME - optimize for needFinalData? (can squeak by with smaller outSize) */
	size_t rtn = rawBlocks * mOutBlockSize;
	bprintf("--- BlockCryptor::outputSize inSize 0x%lx outSize 0x%lx final %d "
		"inBufSize 0x%lx", inSize, rtn, final, mInBufSize);
	return rtn;
}



