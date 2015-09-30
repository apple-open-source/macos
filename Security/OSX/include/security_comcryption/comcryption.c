/*
 * Copyright (c) 1997,2011-2012,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "comcryption.h"
#include "comDebug.h"
#include "comcryptPriv.h"

#if		COM_PROFILE

unsigned comProfEnable;
comprof_t cmcTotal;
comprof_t cmcQueSearch;
comprof_t cmcQueMatchMove;
comprof_t cmcQueMissMove;
comprof_t cmcLevel2;
comprof_t cmcPerWordOhead;

#endif	/*COM_PROFILE*/

void comMallocRegister(comMallocExternFcn *mallocExtern,
	comFreeExternFcn *freeExtern)
{
	comMallocExt = mallocExtern;
	comFreeExt = freeExtern;
}

/*
 * Call once at startup. The resulting comcryptObj can be reused multiple
 * times.
 */
comcryptObj comcryptAlloc(void)
{
	comcryptPriv	*cpriv = (comcryptPriv *) ascMalloc(sizeof(comcryptPriv));

	if(cpriv == NULL) {
		return NULL;
	}
	memset(cpriv, 0, sizeof(comcryptPriv));

#if		COMCRYPT_EXPORT_ONLY
	cpriv->key = (unsigned char *)ascMalloc(EXPORT_KEY_SIZE);
#else	/*COMCRYPT_EXPORT_ONLY*/
	cpriv->key = (unsigned char *)ascMalloc(COMCRYPT_MAX_KEYLENGTH);
#endif	/*COMCRYPT_EXPORT_ONLY*/

	if(cpriv->key == NULL) {
		return NULL;
	}
	cpriv->map 			  = (unsigned char *)ascMalloc(256);
	cpriv->invmap 		  = (unsigned char *)ascMalloc(256);
	if((cpriv->map == NULL) || (cpriv->invmap == NULL)) {
		return NULL;
	}
	mallocCodeBufs(&cpriv->cbuf);
	if((cpriv->cbuf.codeBuf == NULL) ||
	   (cpriv->cbuf.level2Buf == NULL)) {
	 	return NULL;
	}
	#if		QUEUE_LOOKAHEAD
	if(cpriv->cbuf.lookAhead == NULL) {
	 	return NULL;
	}
	#endif

	/*
	 * Hard coded limit of two levels of comcryption
	 */
	cpriv->cbuf.nextBuf = (comcryptBuf *)ascMalloc(sizeof(comcryptBuf));
	if(cpriv->cbuf.nextBuf == NULL) {
		return NULL;
	}
	mallocCodeBufs(cpriv->cbuf.nextBuf);
	if((cpriv->cbuf.nextBuf->codeBuf == NULL) ||
	   (cpriv->cbuf.nextBuf->level2Buf == NULL)) {
	 	return NULL;
	}
	#if		QUEUE_LOOKAHEAD
	if(cpriv->cbuf.nextBuf->lookAhead == NULL) {
	 	return NULL;
	}
	#endif

	cpriv->cbuf.nextBuf->nextBuf = NULL;
	return cpriv;
}

/*
 * Call this before starting every stream process
 */
comcryptReturn comcryptInit(
	comcryptObj 		cobj,
    const unsigned char *key,
    unsigned            keyLen,
    comcryptOptimize    optimize)			// CCO_SIZE, etc.
{
	comcryptPriv	*cpriv = (comcryptPriv *)cobj;
	unsigned		maxKeySize;

#if		COMCRYPT_EXPORT_ONLY
	/*
	 * FIXME - NSA might not be satisfied with this, may have to enforce
	 * elsewhere
	 */
	maxKeySize = EXPORT_KEY_SIZE;
#else	/*COMCRYPT_EXPORT_ONLY*/
	maxKeySize = COMCRYPT_MAX_KEYLENGTH;
#endif	/*COMCRYPT_EXPORT_ONLY*/

	if(keyLen > maxKeySize) {
		keyLen = maxKeySize;
	}
	memmove(cpriv->key, key, keyLen);
	cpriv->keybytes = keyLen;
	cpriv->cbuf.codeBufLength = 0;
	cpriv->cbuf.nextBuf->codeBufLength = 0;
	cpriv->version = 0;
	cpriv->versionBytes = 0;
	cpriv->spareBytes = 0;
	cpriv->optimize = optimize;

	/*
	 * Derive feature enable bits from optimize arg. This is highly likely
	 * to change....
	 */
	cpriv->level2enable = 1;
	cpriv->sigSeqEnable = 1;
	switch(optimize) {
	    case CCO_TIME:
			cpriv->level2enable = 0;
			break;
		case CCO_TIME_SIZE:
			cpriv->sigSeqEnable = 0;
			break;
		default:
			break;
	}
#if		QUEUE_LOOKAHEAD
	cpriv->laEnable = 1;
#else	/* QUEUE_LOOKAHEAD */
	cpriv->laEnable = 0;
#endif	/* QUEUE_LOOKAHEAD */

	/*
	 * init queue and maps
	 */
	initCodeBufs(&cpriv->cbuf, key, keyLen, cpriv->laEnable,
		cpriv->sigSeqEnable);
	initCodeBufs(cpriv->cbuf.nextBuf, key, keyLen, cpriv->laEnable,
		cpriv->sigSeqEnable);
	key_perm(key, keyLen, cpriv->map, cpriv->invmap);
	return CCR_SUCCESS;
}

/*
 * Free a comcryptObj object obtained via comcryptAlloc()
 */
void comcryptObjFree(comcryptObj cobj)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	if(cpriv->key != NULL) {
		ascFree(cpriv->key);
	}
	if(cpriv->map != NULL) {
		ascFree(cpriv->map);
	}
	if(cpriv->invmap != NULL) {
		ascFree(cpriv->invmap);
	}
	freeCodeBufs(&cpriv->cbuf);
	ascFree(cpriv);
}

/*
 * Return the maximum input buffer size allowed for for specified
 * output buffer size. Note that for both comcrypt and decomcrypt,
 * to cover the worst case, the output buffer always has to be
 * larger than the input buffer.
 */
unsigned comcryptMaxInBufSize(comcryptObj cobj,
    unsigned outBufSize,
    comcryptOp op)
{
	unsigned fullBlocks;
	unsigned minCblockSize;
	unsigned resid;
	unsigned rtn;
	unsigned tokenBytes;
	comcryptPriv *cpriv = (comcryptPriv *)cobj;
	unsigned ptextFromCodeBuf;
	
	switch(op) {
	    case CCOP_COMCRYPT:
			/*
			 * Worst case: no compression. Also, establish a minimum
			 * ciphertext size to accomodate header and one block.
			 */
			minCblockSize = MIN_CBLOCK_SIZE;
			if(cpriv->versionBytes == 0) {
				minCblockSize += CTEXT_HDR_SIZE;
			}
			if(outBufSize < (minCblockSize)) {
				return 0;
			}
			if(cpriv->versionBytes == 0) {
				outBufSize -= CTEXT_HDR_SIZE;
			}
			fullBlocks = outBufSize / MAX_CBLOCK_SIZE;
			rtn = (fullBlocks * CC_BLOCK_SIZE);		// bytes of ptext

			/*
			 * code must be even aligned, then chop off one for odd ptext
			 */
			rtn &= 0xfffffffe;
			rtn--;
			if(rtn <= 0) {
				return 0;
			}
			resid = outBufSize % MAX_CBLOCK_SIZE;
			if(resid) {
				rtn += resid;

				/*
				 * Account for resid block overhead
				 */
				if(rtn < MIN_CBLOCK_SIZE) {
					return 0;
				}
				rtn -= MIN_CBLOCK_SIZE;

				tokenBytes = TOKEN_BYTES_FROM_PTEXT(resid);
				if(rtn <= tokenBytes) {
					return 0;
				}
				rtn -= tokenBytes;
			}
			if(rtn > INBUF_TRUNC_THRESH) {
				/*
				 * Truncate to even block size to minimize partial cipherblocks
				 */
				rtn &= ~(CC_BLOCK_SIZE - 1);
			}
			return rtn;

		case CCOP_DECOMCRYPT:
			/*
			 * Worst case - 4:1 compression and an almost full block in
			 * codeBuf. Note 4:1 is a super-conservative, easy arithmetic
			 * version of (9/16) squared...
			 */
			ptextFromCodeBuf = cpriv->cbuf.codeBufLength * 4;
			if(outBufSize < ptextFromCodeBuf) {
				/* decrypting codeBuf might overflow output (plaintext)
				 * buffer - won't be able to move anything */
				rtn = 0;
			}
			else {
				/* can decrypt (this much plainText - ptextFromCodeBuf) / 4 */
				rtn = (outBufSize - ptextFromCodeBuf) / 4;
			}
			
			/* may be able to handle a bit extra for initial decrypt... */
			if(cpriv->versionBytes < VERSION_BYTES) {
				rtn += (VERSION_BYTES - cpriv->versionBytes);
			}
			if(cpriv->spareBytes < SPARE_BYTES) {
				rtn += (SPARE_BYTES - cpriv->spareBytes);
			}
			return rtn;
			
		default:
			ddprintf(("bogus op (%d) in comcryptMaxInBufSize()\n", op));
			return 0;
	}
}

/*
 * Return the maximum output buffer size for specified input buffer size.
 * Output buffer size will always be larger than input buffer size.
 */
unsigned comcryptMaxOutBufSize(comcryptObj cobj,
    unsigned inBufSize,
    comcryptOp op,
	char final)
{
	unsigned fullBlocks;
	unsigned resid;
	unsigned rtn;
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	switch(op) {
	    case CCOP_COMCRYPT:
			fullBlocks = inBufSize / CC_BLOCK_SIZE;
			rtn = fullBlocks * MAX_CBLOCK_SIZE;
			resid = inBufSize % CC_BLOCK_SIZE;
			if(resid != 0) {
				/*
				 * partial block
				 */
				unsigned tokenBytes = TOKEN_BYTES_FROM_PTEXT(resid);

				rtn += MIN_CBLOCK_SIZE;
				rtn += tokenBytes;
				rtn += resid;			// no compression
				if(resid & 1) {
					rtn++;				// oddByte uses extra
				}
			}
			if((cpriv == NULL) || 		// i.e., we're being called from mallocCodeBufs
			   (cpriv->versionBytes == 0)) {
				rtn += CTEXT_HDR_SIZE;	// first of a stream
			}
			return rtn;

		case CCOP_DECOMCRYPT:
			/*
			 * Here assume max compression, including resid block in codeBuf
			 */
			inBufSize += cpriv->cbuf.codeBufLength;
			if(inBufSize) {
				/* may be able to handle a bit extra for initial decrypt... */
				unsigned delta;
				if(cpriv->versionBytes < VERSION_BYTES) {
					delta = VERSION_BYTES - cpriv->versionBytes;
					if(inBufSize > delta) {
						inBufSize -= delta;
					}
					else {
						inBufSize = 0;
					}
				}
				if(cpriv->spareBytes < SPARE_BYTES) {
					delta = SPARE_BYTES - cpriv->spareBytes;
					if(inBufSize > delta) {
						inBufSize -= delta;
					}
					else {
						inBufSize = 0;
					}
				}
			}
			rtn = 4 * inBufSize;
			return rtn;
			
		default:
			ddprintf(("bogus op (%d) in comcryptMaxOutBufSize()\n", op));
			return 0;
	}
}

/*
 * Threshold for using memmove() rather than hard-coded loop for
 * moving queue segment. This was derived empirically on a Pentium;
 * we should do similar measurements on PPC.
 */
#define	QUEUE_MEMMOVE_THRESH	3

/*
 * peek at queue[0] before search. This appears to only be a win for
 * constant plaintext, i.e., the codeword is almost always at queue[0].
 */
#define QUEUE_PEEK		0

/*
 * Comcrypt one block.
 */
static comcryptReturn comcryptBlock(
	comcryptPriv 		*cpriv,
	comcryptBuf			*cbuf,				// not necessarily cpriv->cbuf
	const unsigned char	*plainText,
	unsigned			plainTextLen,
	unsigned char		*cipherText,
	unsigned			*cipherTextLen,		// IN/OUT
	unsigned			recursLevel)
{
	unsigned char 	*byteCodePtr;
	unsigned char	*destByteCodePtr;
	unsigned char 	*longCodePtr;
	unsigned char	*startLongCodePtr;
	unsigned char 	*tokenPtr;
	unsigned char	*startTokenPtr;
	unsigned char	*startCtextPtr = cipherText;
	unsigned 		numTokenBytes;		// in bytes, constant
	unsigned short 	codeWord;
	unsigned		oddByte = 0;
	unsigned		match;
	unsigned		jmatch=0;
	unsigned		tokenDex = 0;		// index into array of token bits
	unsigned		j;
	unsigned		numLongCodes = 0;
	unsigned		numByteCodes = 0;
	unsigned		totalCipherTextLen;
	unsigned		above;
	unsigned		jmatchTotal = 0;
	unsigned		jmatchAvg;
	comcryptReturn	crtn;
	unsigned char 	blockDesc = CBD_MAGIC;
	unsigned		fullBlock = 0;
	int				len;
	queueElt		*src;
	queueElt		*dst;
	queueElt 		*cbufq = &cbuf->queue[0];

	/*
	 * 'nibble' is added to 'above' in the call to nextSigWord() for
	 * additional security.
	 *
	 * Normal case : nibble = keynybble()
	 * last word on odd byte : nibble = nibbleDex
	 * hit on queue q : nibble = nibbleDex (optimize to avoid keynybble()
	 *     call)
	 */
	unsigned char	nibble;

	COMPROF_LOCALS;

	#if		COM_LA_DEBUG
	if(testLookAhead(cbuf, 0, 0)) {
		return CCR_INTERNAL;
	}
	#endif

	laprintf(("comcryptBlock recurs level %d\n", recursLevel));

	/*
	 * Set up ptrs for the three arrays we'll be writing
	 */
	tokenPtr = cipherText + CTBO_NUM_TOKENS + 1;
	if(plainTextLen >= (CC_BLOCK_SIZE - 1)) {
		/*
		 * Optimized for full block - no token count in block. Note
		 * that plainTextLen == (CC_BLOCK_SIZE - 1) is also a full block
		 * in that it uses up a full block's worth of tokens!
		 */
		numTokenBytes = CC_BLOCK_SIZE >> 4;
		tokenPtr--;
		blockDesc |= CBD_FULL_BLOCK;
		fullBlock = 1;
	}
	else {
		numTokenBytes = (plainTextLen + 15) >> 4;
	}
	longCodePtr  	  = tokenPtr + numTokenBytes;
	startLongCodePtr  = longCodePtr;
	byteCodePtr	   	  = cbuf->codeBuf;
	startTokenPtr 	  = tokenPtr;

	if((unsigned)(longCodePtr - cipherText) > *cipherTextLen) {
		ddprintf(("comcryptBlock: short block (1)\n"));
		return CCR_OUTBUFFER_TOO_SMALL;
	}
	memset(tokenPtr, 0, numTokenBytes);

	/*
	 * Entering time-critical region. This loop executes once for every
	 * 2 bytes of plaintext. Make every attempt to streamline the code
	 * here; avoid function calls in favor of macros; etc.
	 */
	while(plainTextLen != 0) {

		/*
		 * assemble a 16-bit word from two bytes if possible
		 */
		if(plainTextLen == 1) {
			/*
			 * Odd byte case
			 */
			codeWord = ((unsigned short)(cpriv->map[*plainText]) << 8) |
						 cpriv->map[0];	// a bit of obfuscation - mapped zero
			oddByte = 1;
			blockDesc |= CBD_ODD;
			plainTextLen--;
		}
		else {
			codeWord = ((unsigned short)(cpriv->map[*plainText]) << 8) |
			            (unsigned short)(cpriv->map[plainText[1]]);
			plainText += 2;
			plainTextLen -= 2;
		}

		/*
		 * Calibrate how much profiling is costing us.
		 */
		COMPROF_START;
		COMPROF_END(cmcPerWordOhead);

		/*
		 * See if this word is in queue[]. Skip if oddByte; we'll force
		 * a 16-bit word in that case. Also skip the search if we know
		 * via lookahead that a search would be fruitless.
		 */
		COMPROF_START;		/* cmcQueSearch */
		match = 0;
		do {				/* while 0 - for easy breaks w/o goto */

			/*
			 * First handle some optimizations and special cases
			 */
			if(oddByte) {
				break;			// force longcode
			}

#if		QUEUE_PEEK
			if(cbufq[0] == codeWord) {
				match = 1;
				jmatch = 0;
				break;

			}
#endif	/*QUEUE_PEEK*/

			if(cpriv->laEnable && !inQueue(cbuf, codeWord)) {
				break;
			}

			/*
			 * OK, do the gruntwork search
			 */
			for(j=0; j < QLEN; j++) {
				if(cbufq[j] == codeWord) {
					match = 1;
					jmatch = j;
					break;
				}
			}

#if		COM_LA_DEBUG
			if(cpriv->laEnable && !match) {
				printf("inQueue, not found in queue!\n");
				return CCR_INTERNAL;
			}

			/*
			 * Search for duplicates.
			 */
			if(match) {
				for(j=jmatch+1; j<QLEN; j++) {
					if(cbufq[j] == codeWord) {
						printf("***Huh! Dup queue entry codeWord 0x%x jmatch "
							"0x%x  2nd j 0x%x\n",
							codeWord, jmatch, j);
						return CCR_INTERNAL;
					}
				}
			}
#endif	/*COM_LA_DEBUG*/
		} while(0);

		COMPROF_END(cmcQueSearch);

		/*
		 * Note we measure the overhead on a per-codeword basis. Here,
		 * we ensure that there is exactly one pair of start/end
		 * timestamps per queue move per code word.
		 *
		 * New 17 Dec 1997 - always calculate keynibble for use in signature
		 * sequence update
		 */
#if		!SKIP_NIBBLE_ON_QUEUE_0
		nibble = keynybble(cpriv->key, cpriv->keybytes,
						(cbuf->nybbleDex)++);
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/

		COMPROF_START;
		if(match) {
			/*
			 * 16-bit symbol is in queue. 8 bits of ciphertext, token bit is 0.
			 */
			if(jmatch == 0) {
				/*
				 * Optimization: jmatch = 0. Keep state machine in sync,
				 * but skip queue update.
				 */
				above = 0;
				laprintf(("...queue hit at queue[0]\n"));
#if		SKIP_NIBBLE_ON_QUEUE_0
				nibble = (cbuf->nybbleDex)++;
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/
			}
			else {
#if		SKIP_NIBBLE_ON_QUEUE_0
				nibble = keynybble(cpriv->key, cpriv->keybytes,
								(cbuf->nybbleDex)++);
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/

				above = (cbuf->f1 * jmatch * (16 + nibble)) >> 9;

				/*
				 * queue[above..(jmatch-1)] move one element towards end
				 * queue[above] = this codeWord
				 */
				laprintf(("...queue hit, moving 0x%x from 0x%x to 0x%x\n",
					codeWord, jmatch, above));

				len = (int)jmatch - (int)above;
				if(len > QUEUE_MEMMOVE_THRESH) {
					src = &cbufq[above];
					dst = src + 1;
					len *= sizeof(queueElt);
					memmove(dst, src, len);
				}
				else {
					for(j = jmatch; j>above; j--) {
						cbufq[j] = cbufq[j-1];
					}
				}

				cbufq[above] = codeWord;
#if		COM_LA_DEBUG
				if(testLookAhead(cbuf, above, jmatch)) {
					return CCR_INTERNAL;
				}
#endif	/*COM_LA_DEBUG*/
			}
			COMPROF_END(cmcQueMatchMove);

			codeWord = jmatch;
			incr1byteFrags(recursLevel);
			jmatchTotal += jmatch;
		}
		else if(oddByte == 0) {
			/*
			 * 16-bit symbol is not in queue. 16 bits of ciphertext.
			 * Token bit is 1.
			 *
			 * queue[above...QLEN-1] move one element toward end
			 * queue[QLEN-1] discarded
			 * queue[above] = new codeword
			 *
			 * Note we skip this queue manipulation in the oddbyte case, since
			 * we don't really know (or care) if the current code word is in
			 * the queue or not.
			 */
#if		SKIP_NIBBLE_ON_QUEUE_0
			nibble = keynybble(cpriv->key, cpriv->keybytes,
							(cbuf->nybbleDex)++);
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/

			above = ABOVE(cbuf->f2) + nibble;

#if		COM_DEBUG
			if(above > QLEN) {
				printf("Hey Doug! above %d  QLEN %d\n", above, QLEN);
				return CCR_INTERNAL;
			}
#endif

			laprintf(("...queue miss, adding 0x%x at 0x%x, deleting 0x%x\n",
				codeWord, above, cbufq[QLEN-1]));

			if(cpriv->laEnable) {
				markInQueue(cbuf, codeWord, 1);			// new entry
				markInQueue(cbuf, cbufq[QLEN-1], 0);	// bumped out
			}

			len = QLEN - 1 - (int)above;
			if(len > QUEUE_MEMMOVE_THRESH) {
				src = &cbufq[above];
				dst = src + 1;
				len *= sizeof(queueElt);
				memmove(dst, src, len);
			}
			else {
				for(j=QLEN-1; j > above; j--) {
					cbufq[j] = cbufq[j-1];
				}
			}

			cbufq[above] = codeWord;

#if		COM_LA_DEBUG
			if(testLookAhead(cbuf, above, 0)) {
				return CCR_INTERNAL;
			}
#endif	/*COM_LA_DEBUG*/

			COMPROF_END(cmcQueMissMove);
			incr2byteFrags(recursLevel);
		}
		else {
			/*
			 * Odd byte case, at least gather stats.
			 */
			incr2byteFrags(recursLevel);

			/*
			 * ...and keep this in sync for signature sequence
			 */
			above = 0;
#if		SKIP_NIBBLE_ON_QUEUE_0
			nibble = (cbuf->nybbleDex)++;
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/
		}

		updateToken(tokenPtr, tokenDex, !match);
		tokenDex++;

		if(match) {
			*byteCodePtr++ = codeWord & 0xff;
			numByteCodes++;
		}
		else {
			serializeShort(codeWord, longCodePtr);
			longCodePtr += 2;
			numLongCodes++;
		}
		if(cpriv->sigSeqEnable) {
			nextSigWord(cbuf, tokenDex, match, (above + nibble));
		}
	}

#if		COM_DEBUG
	if(numTokenBytes != ((tokenDex + 7) >> 3)) {
		ddprintf(("comcryptBlock: numTokenBytes (%d), tokenDex (%d)\n",
			numTokenBytes, tokenDex));
	}
#endif	/*COM_DEBUG*/

	/*
	 * We already wrote tokens and longcode to cipherText; verify we
	 * didn't overrun
	 */
	totalCipherTextLen = (unsigned)(longCodePtr - startCtextPtr);
	if(*cipherTextLen < totalCipherTextLen) {
		ddprintf(("comcryptBlock: short block (2)\n"));
		return CCR_OUTBUFFER_TOO_SMALL;
	}
	if(!fullBlock) {
		cipherText[CTBO_NUM_TOKENS] = tokenDex;
	}
	cipherText[CTBO_NUM_LONG_CODES] = numLongCodes;

#if		COM_DEBUG
	if(tokenDex > MAX_TOKENS) {
		ddprintf(("comcryptBlock: counter overflow!\n"));
		return CCR_INTERNAL;
	}
	if((numByteCodes + numLongCodes) != tokenDex) {
		ddprintf(("comcryptBlock: counter mismatch!\n"));
		return CCR_INTERNAL;
	}
#endif	/*COM_DEBUG*/

	/*
	 * See if doing a second level comcryption makes sense.
	 */
	destByteCodePtr = startLongCodePtr + (numLongCodes * 2);
	if(numByteCodes > 0) {
		jmatchAvg = jmatchTotal / numByteCodes;
	}
	else {
		jmatchAvg = cbuf->jmatchThresh + 1;
	}
	if((recursLevel == 0) &&					// hard coded recursion limit
	   (cpriv->level2enable) &&					// enabled by caller
	   (numByteCodes >= cbuf->minByteCode) &&	// meaningful # of bytecodes
	   (jmatchAvg <= cbuf->jmatchThresh)) {		// reasonable compression
	   											//   already achieved

		unsigned thisCtext = cbuf->level2BufSize;

		COMPROF_START;
		crtn = comcryptBlock(cpriv,
				cbuf->nextBuf,
				cbuf->codeBuf,
				numByteCodes,
				cbuf->level2Buf,
				&thisCtext,
				recursLevel + 1);
		if(crtn) {
			return crtn;
		}

		/*
		 * Write level2Buf to cipherText (as byteCodeArray).
		 * Size of 2nd level comcrypted byte code follows longcode array,
		 * then the bytecode itself.
		 * First bump totalCipherTextLen by the size of the comcrypted array
		 * plus one (for the size byte itself), and verify no overflow
		 */
		totalCipherTextLen += (thisCtext + 1);
		if(*cipherTextLen < totalCipherTextLen) {
			ddprintf(("comcryptBlock: short block (3)\n"));
			return CCR_OUTBUFFER_TOO_SMALL;
		}
		*destByteCodePtr++ = thisCtext;
		COMPROF_END(cmcLevel2);
		memmove(destByteCodePtr, cbuf->level2Buf, thisCtext);
		blockDesc |= CBD_DOUBLE;

		l2printf(("***2nd-level comcrypt: numByteCodes %d encrypted "
			"size %d\n", numByteCodes, thisCtext));
		incrComStat(level2byteCode, numByteCodes);
		incrComStat(level2cipherText, thisCtext);
		incrComStat(level2jmatch, jmatchTotal);
		incrComStat(level2blocks, 1);
	}
	else {
		/*
		 * Normal one-level comcryption. Write byteCodes to ciphertext.
		 * numByteCodes is inferred.
		 */
		totalCipherTextLen += numByteCodes;
		if(*cipherTextLen < totalCipherTextLen) {
			ddprintf(("comcryptBlock: short block (3)\n"));
			return CCR_OUTBUFFER_TOO_SMALL;
		}
		memmove(destByteCodePtr, cbuf->codeBuf, numByteCodes);
		blockDesc |= CBD_SINGLE;
		if(recursLevel == 0) {
			incrComStat(level1blocks, 1);
		}
		/* else this is a 2nd-level, our caller will count */

		/*
		 * obfuscate via sigArray (only when we're NOT doing 2nd level
		 *  comcrypt)
		 */
		if(cpriv->sigSeqEnable) {
			sigMunge(cbuf, startTokenPtr, tokenDex,
				destByteCodePtr, startLongCodePtr);

			/*
			 * Prime sigArray state machine for next block. Note in the case
			 * of 2nd level, we skip this step, so the next block starts from
			 * the same state as this one did.
			 */
			cbuf->sigArray[0] = cbuf->sigArray[tokenDex];
		}
	}
	cipherText[CTBO_BLOCK_DESC] = blockDesc;
	*cipherTextLen = totalCipherTextLen;
	return CCR_SUCCESS;
}

/*
 * Main public encrypt function.
 */
comcryptReturn comcryptData(
	comcryptObj 			cobj,
	unsigned char 			*plainText,
	unsigned 				plainTextLen,
	unsigned char 			*cipherText,		// malloc'd by caller
	unsigned 				*cipherTextLen,		// IN/OUT
	comcryptEos 			endOfStream) 		// CCE_END_OF_STREAM, etc.
{
	comcryptPriv	*cpriv = (comcryptPriv *)cobj;
	unsigned		ctextLen = *cipherTextLen;
	comcryptReturn	crtn;
	unsigned		thisPtext;
	unsigned		thisCtext;
	COMPROF_LOCALS;

	COMPROF_START;
	incrComStat(plaintextBytes, plainTextLen);
	if(cpriv->versionBytes == 0) {
		/*
		 * First, put header (version, spare) into head of ciphertext.
		 */
		if(ctextLen < CTEXT_HDR_SIZE) {
			ddprintf(("comcryptData: overflow (0)\n"));
			return CCR_OUTBUFFER_TOO_SMALL;
		}
		serializeInt(VERSION_3_Dec_97, cipherText);
		cipherText += VERSION_BYTES;
		cpriv->versionBytes = VERSION_BYTES;
		serializeInt(0, cipherText);				// spares
		cipherText += SPARE_BYTES;
		ctextLen   -= CTEXT_HDR_SIZE;
	}

	/*
	 * OK, grind it out, one block at a time.
	 */
	while (plainTextLen != 0) {
		thisPtext = CC_BLOCK_SIZE;
		if(thisPtext > plainTextLen) {
			thisPtext = plainTextLen;
		}
		thisCtext = ctextLen;
		crtn = comcryptBlock(cpriv,
			&cpriv->cbuf,
			plainText,
			thisPtext,
			cipherText,
			&thisCtext,
			0);			// recurs level
		if(crtn) {
			return crtn;
		}
		plainText    += thisPtext;
		plainTextLen -= thisPtext;
		if(thisCtext > ctextLen) {
			ddprintf(("comcryptData: undetected ciphertext overlow\n"));
			return CCR_OUTBUFFER_TOO_SMALL;
		}
		cipherText += thisCtext;
		ctextLen   -= thisCtext;
	}
	*cipherTextLen = *cipherTextLen - ctextLen;
	incrComStat(ciphertextBytes, *cipherTextLen);
	COMPROF_END(cmcTotal);
	return CCR_SUCCESS;
}

/*
 * Return values from deComcryptBlock().
 */
typedef enum {
	DCB_SUCCESS,			// OK
	DCB_SHORT,				// incomplete block, try again with more ciphertext
	DCB_PARSE_ERROR,		// bad block
	DCB_OUTBUFFER_TOO_SMALL
} dcbReturn;

/*
 * Assumes exactly one block of ciphertext, error otherwise.
 */
static dcbReturn deComcryptBlock(
	comcryptPriv 			*cpriv,
	comcryptBuf				*cbuf,				// not necessarily cpriv->cbuf
	unsigned char 			*cipherText,
	unsigned 				cipherTextLen,
	unsigned char 			*plainText,
	unsigned	 			*plainTextLen,		// IN/OUT
	comcryptEos 			endOfStream,		// CCE_END_OF_STREAM, etc.
	unsigned				*blockSize)			// RETURNED on DCB_SUCCESS
{
	unsigned char		*tokenPtr;
	unsigned			numTokenBits;			// constant, from ciphertext
	unsigned			numTokenBytes;
	unsigned char		*longCodePtr;
	unsigned			numLongCodes;
	unsigned char		*byteCodePtr;
	unsigned			numByteCodes;
	unsigned			tokenDex;
	unsigned			oddByte = 0;
	unsigned short		codeWord;
	unsigned char		codeByte;
	unsigned			ptextLen = *plainTextLen;	// bytes REMAINING
	unsigned			above;
	unsigned			j;
	unsigned char		blockDesc;
	dcbReturn			drtn;
	int					len;
	queueElt			*src;
	queueElt			*dst;
	int					lastWord = 0;
	queueElt 			*cbufq = &cbuf->queue[0];
	int					level2 = 0;				// 2nd level comcrypted block
	unsigned			match;
	unsigned char		sigSeq;					// signature sequence enable
	unsigned char		nibble;

	blockDesc = cipherText[CTBO_BLOCK_DESC];
	if((blockDesc & CBD_MAGIC_MASK) != CBD_MAGIC) {
		ddprintf(("deComcryptBlock: bad CBD_MAGIC\n"));
		return DCB_PARSE_ERROR;
	}

	/*
	 * Min block size - blockDesc, numLongCodes, numTokens, one token byte,
	 * one bytecode
	 */
	if(cipherTextLen < 5) {
		return DCB_SHORT;
	}
	if((blockDesc & CBD_FULL_BLOCK_MASK) == CBD_FULL_BLOCK) {
		/*
		 * # of token bits implied for full block
		 */
		numTokenBits  = TOKEN_BITS_FROM_PTEXT(CC_BLOCK_SIZE);
		numTokenBytes = TOKEN_BYTES_FROM_PTEXT(CC_BLOCK_SIZE);
		tokenPtr      = cipherText + CTBO_NUM_TOKENS;
	}
	else {
		numTokenBits  = cipherText[CTBO_NUM_TOKENS];
		numTokenBytes = TOKEN_BYTES_FROM_TOKEN_BITS(numTokenBits);
		tokenPtr      = cipherText + CTBO_NUM_TOKENS + 1;
	}
	longCodePtr = tokenPtr + numTokenBytes;
	numLongCodes = cipherText[CTBO_NUM_LONG_CODES];

	byteCodePtr  = longCodePtr + (numLongCodes * 2);	// may increment...
	if((blockDesc & CBD_BLOCK_TYPE_MASK) == CBD_SINGLE) {
		/*
		 * # of bytecodes implied from numTokenBits and numLongCodes
		 */
		numByteCodes = numTokenBits - numLongCodes;
	}
	else {
		/*
		 * size of 2nd level comcrypted bytecode specified after longCode
		 * array (and before the bytecode itself).
		 * Careful, verify that we can read numByteCodes first...
		 */
		if((unsigned)(byteCodePtr - cipherText) > cipherTextLen) {
			return DCB_SHORT;
		}
		numByteCodes = *byteCodePtr++;
		level2 = 1;
	}
	*blockSize = (unsigned)(byteCodePtr - cipherText) + numByteCodes;
	if(*blockSize > cipherTextLen) {
		return DCB_SHORT;
	}

	/*
	 * We now know that we have a complete cipherblock. Go for it.
	 */
	if(level2) {
		/*
		 * this block's bytecode array contains 2nd level comcrypted bytecodes.
		 */
		unsigned thisPtext = cbuf->level2BufSize;
		unsigned level1CodeSize;

		if(cbuf->nextBuf == NULL) {
			ddprintf(("2-level comcypt, no nextBuf available!\n"));
			return DCB_PARSE_ERROR;
		}
		drtn = deComcryptBlock(cpriv,
			cbuf->nextBuf,
			byteCodePtr,
			numByteCodes,
			cbuf->level2Buf,
			&thisPtext,
			CCE_END_OF_STREAM,
			&level1CodeSize);
		switch(drtn) {
			case DCB_SHORT:
				ddprintf(("CBT_DOUBLE block, incomplete cipherblock in "
					"2nd level code\n"));
				return DCB_PARSE_ERROR;

			case DCB_OUTBUFFER_TOO_SMALL:	// not our fault!
			case DCB_PARSE_ERROR:
			default:
				ddprintf(("2nd-level decomcrypt error (%d)\n", drtn));
					return drtn;

			case DCB_SUCCESS:
				/*
				 * Supposedly we passed in exactly one cipherblock...
				 */
				if(numByteCodes != level1CodeSize) {
					ddprintf(("2nd-level decomcrypt: "
						"numByteCodes != level1CodeSize\n"));
					return DCB_PARSE_ERROR;
				}
				l2printf(("2nd-level decomcrypt: ciphertext %d "
					"numByteCodes %d\n", numByteCodes, thisPtext));
				break;
		}
		byteCodePtr = cbuf->level2Buf;
		numByteCodes = thisPtext;
	}

	if((blockDesc & CBD_ODD_MASK) == CBD_ODD) {
		oddByte = 1;
	}

	/*
	 * Skip signature sequence if this was a 2nd level comcrypted block
	 */
	sigSeq = cpriv->sigSeqEnable && !level2;

	for(tokenDex=0; tokenDex<numTokenBits; tokenDex++) {
		match = !getToken(tokenPtr, tokenDex);

		/*
		 * 17 Dec 1997 - Always calculate this regardless of match
		 */
		nibble = keynybble(cpriv->key, cpriv->keybytes,
						  	 (cbuf->nybbleDex)++);

		if(match) {
			codeByte = *byteCodePtr++;

			if(sigSeq) {
				codeByte ^= (unsigned char)(cbuf->sigArray[tokenDex]);
			}

			/*
			 * dynamically process the queue for match - 8 bits
			 * of ciphercode, 16 bits of plaintext
			 */
			codeWord = cbufq[codeByte];
			above = (cbuf->f1 * codeByte * (16 + nibble)) >> 9;

#if		SKIP_NIBBLE_ON_QUEUE_0
			if(codeByte == 0) {
				/*
				 * Special case for top of queue optimization during
				 * comcrypt
				 */
				nibble = cbuf->nybbleDex - 1;
			}
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/

			/*
			 * queue[above..codeByte] move one element towards end
			 * queue[above] = this codeWord
			 */
			len = (int)codeByte - (int)above;
			if(len > QUEUE_MEMMOVE_THRESH) {
				src = &cbufq[above];
				dst = src + 1;
				len *= sizeof(queueElt);
				memmove(dst, src, len);
			}
			else {
				for(j = codeByte; j > above; j--) {
					cbufq[j] = cbufq[j-1];
				}
			}
			cbufq[above] = codeWord;
		}
		else {
			/*
			 * !match, 16 bits of code
			 */
			deserializeShort(codeWord, longCodePtr);
			if(sigSeq) {
				codeWord ^= cbuf->sigArray[tokenDex];
			}

			if(oddByte && (tokenDex == (numTokenBits - 1))) {
				lastWord = 1;
				above = 0;
#if		SKIP_NIBBLE_ON_QUEUE_0
				nibble = cbuf->nybbleDex - 1;
#endif	/*SKIP_NIBBLE_ON_QUEUE_0*/
			}
			else {
				longCodePtr += 2;

				/*
				 * dynamically process the queue for unmatch; skip if this
				 * is an oddByte codeword.
				 * queue[above...QLEN-1] move one element toward end
				 * queue[above] = new codeWord
				 */
				above = ABOVE(cbuf->f2) + nibble;
				len = QLEN - 1 - (int)above;
				if(len > QUEUE_MEMMOVE_THRESH) {
					src = &cbufq[above];
					dst = src + 1;
					len *= sizeof(queueElt);
					memmove(dst, src, len);
				}
				else {
					for(j=QLEN-1; j > above; j--) {
						cbufq[j] = cbufq[j-1];
					}
				}
				cbufq[above] = codeWord;
			}
		}

		if(sigSeq) {
			/*
			 * Advance signature sequence state machine.
			 */
			nextSigWord(cbuf, tokenDex+1, match, (above + nibble));
		}

		/*
		 * cook up a byte or two of plainText from code word and invmap[]
		 */
		if(ptextLen < 1) {
			ddprintf(("decryptBlock: ptext overflow (1)\n"));
			return DCB_OUTBUFFER_TOO_SMALL;
		}
		*plainText++ = cpriv->invmap[(codeWord >> 8) & 0xff];
		ptextLen--;
		if(lastWord) {
			/*
			 * end of oddByte block.
			 */
			tokenDex++;	// for sigArray maintenance
			break;		// out of main loop
		}
		else {
			if(ptextLen < 1) {
				ddprintf(("decryptBlock: ptext overflow (2)\n"));
				return DCB_OUTBUFFER_TOO_SMALL;
			}
			*plainText++ = cpriv->invmap[(codeWord) & 0xff];
			ptextLen--;
		}
	}

	/*
	 * Prime sigArray state machine for next block.
	 */
	if(sigSeq) {
		cbuf->sigArray[0] = cbuf->sigArray[tokenDex];
	}
	*plainTextLen = *plainTextLen - ptextLen;
	return DCB_SUCCESS;
}

comcryptReturn deComcryptData(
	comcryptObj 			cobj,
	unsigned char 			*cipherText,
	unsigned 				cipherTextLen,
	unsigned char 			*plainText,
	unsigned	 			*plainTextLen,	// IN/OUT
	comcryptEos 			endOfStream) 	// CCE_END_OF_STREAM, etc.

{
	comcryptPriv	*cpriv = (comcryptPriv *)cobj;
    unsigned char 	*outorigin = plainText;
	unsigned		ptextLen = *plainTextLen;
	unsigned		thisPtext;				// per block
	unsigned		blockSize;
	dcbReturn		drtn;
	unsigned 		ctextUsed;

	/*
	 * Snag version from ciphertext, or as much as we can get
	 */
	while((cpriv->versionBytes < VERSION_BYTES) && cipherTextLen) {
		cpriv->version <<= 8;
		cpriv->version |= *cipherText;
		cpriv->versionBytes++;
		cipherText++;
		cipherTextLen--;
	}

	/*
	 * Then skip over the remainder of the header (currently spares)
	 */
	if((cpriv->spareBytes < SPARE_BYTES) && cipherTextLen) {
		unsigned toSkip = SPARE_BYTES - cpriv->spareBytes;

		if(toSkip > cipherTextLen) {
			toSkip = cipherTextLen;
		}
		cpriv->spareBytes += toSkip;
		cipherText += toSkip;
		cipherTextLen -= toSkip;
	}

	if(cipherTextLen == 0) {
		*plainTextLen = 0;
		return CCR_SUCCESS;
	}

    if(cpriv->version != VERSION_3_Dec_97) {
    	ddprintf(("Incompatible version.\n"));
		return CCR_BAD_CIPHERTEXT;
    }

	while(cipherTextLen != 0) {

		/*
		 * Main loop. First deal with possible existing partial block.
		 */
		if(cpriv->cbuf.codeBufLength != 0) {
			unsigned toCopy =
				cpriv->cbuf.codeBufSize - cpriv->cbuf.codeBufLength;
			unsigned origBufSize = cpriv->cbuf.codeBufLength;

			if(toCopy > cipherTextLen) {
				toCopy = cipherTextLen;
			}
			memmove(cpriv->cbuf.codeBuf + cpriv->cbuf.codeBufLength,
				cipherText, toCopy);
			cpriv->cbuf.codeBufLength += toCopy;

			thisPtext = ptextLen;
			drtn = deComcryptBlock(cpriv,
				&cpriv->cbuf,
				cpriv->cbuf.codeBuf,
				cpriv->cbuf.codeBufLength,
				plainText,
				&thisPtext,
				endOfStream,
				&blockSize);
			switch(drtn) {
				case DCB_SHORT:
					/*
					 * Incomplete block in codeBuf
					 */
					if(endOfStream == CCE_END_OF_STREAM) {
						/*
						 * Caller thinks this is the end, but we need more
						 */
						ddprintf(("deComcryptData(): CCE_END_OF_STREAM, "
							"not end of block\n"));
						return CCR_BAD_CIPHERTEXT;
					}
					cipherTextLen -= toCopy;
					if(cipherTextLen != 0) {
						/*
						 * i.e., codeBuf overflow - could be s/w error? Do
						 * we need a bigger buffer?
						 */
						ddprintf(("deComcryptData: full codeBuf, incomplete "
							"block\n"));
						return CCR_BAD_CIPHERTEXT;
					}
					else {
						/*
						 * OK, stash it and try again
						 */
						scprintf(("====incomplete codeBuf, codeBufLength %d, "
							"cipherTextLen %d\n",
							cpriv->cbuf.codeBufLength, toCopy));
						break;		// out of main loop (after this switch)
					}

				case DCB_OUTBUFFER_TOO_SMALL:
					ddprintf(("codeBuf decomcrypt error short buf\n"));
					return CCR_OUTBUFFER_TOO_SMALL;
					
				case DCB_PARSE_ERROR:
				default:
					ddprintf(("codeBuf decomcrypt error (%d)\n", drtn));
					return CCR_BAD_CIPHERTEXT;

				case DCB_SUCCESS:
					/*
					 * ctextUsed is how much of caller's ciphertext we used
					 * in this buffered block
					 */
					ctextUsed = blockSize - origBufSize;
					scprintf(("====decrypted block in codeBuf, blockSize %d, "
						"ctextUsed %d, thisPtext %d\n",
						blockSize, ctextUsed, thisPtext));
					cipherText    += ctextUsed;
					cipherTextLen -= ctextUsed;
					plainText     += thisPtext;
					ptextLen      -= thisPtext;
					cpriv->cbuf.codeBufLength = 0;
					break;
			}

			/*
			 * We might have used up all of caller's cipherText processing
			 * codeBuf...
			 */
			if(cipherTextLen == 0) {
				break;				// out of main loop
			}

		}	/* buffered ciphertext in codeBuf */

		/*
		 * Snarf ciphertext, one block at a time.
		 */

		thisPtext = ptextLen;
		drtn = deComcryptBlock(cpriv,
			&cpriv->cbuf,
			cipherText,
			cipherTextLen,
			plainText,
			&thisPtext,
			endOfStream,
			&blockSize);
		switch(drtn) {
			case DCB_SHORT:
				/*
				 * Incomplete block
				 */
				if(endOfStream == CCE_END_OF_STREAM) {
					ddprintf(("deComcryptData(): CCE_END_OF_STREAM, not end of "
						"block (2)\n"));
					return CCR_BAD_CIPHERTEXT;
				}
				if(cipherTextLen >
				       (cpriv->cbuf.codeBufSize - cpriv->cbuf.codeBufLength)) {
					ddprintf(("deComcryptData(): codeBuf overflow!\n"));
					return CCR_BAD_CIPHERTEXT;
				}
				memmove(cpriv->cbuf.codeBuf + cpriv->cbuf.codeBufLength,
					cipherText, cipherTextLen);
				cpriv->cbuf.codeBufLength += cipherTextLen;
				cipherTextLen = 0;
				scprintf(("====Incomplete block, cipherTextLen %d "
					"codeBufLength %d\n", cipherTextLen,
					cpriv->cbuf.codeBufLength));
				break;		// actually out of main loop

		    case DCB_PARSE_ERROR:
			case DCB_OUTBUFFER_TOO_SMALL:
			default:
				return CCR_BAD_CIPHERTEXT;

			case DCB_SUCCESS:
				if(ptextLen < thisPtext) {
					/*
					 * Software error
					 */
					ddprintf(("deComcryptData: undetected ptext "
						"overflow (2)\n"));
					return CCR_BAD_CIPHERTEXT;
				}
				plainText     += thisPtext;
				ptextLen      -= thisPtext;
				cipherText    += blockSize;
				cipherTextLen -= blockSize;
				scprintf(("====decrypted one block, blockSize %d "
					"thisPtext %d\n", blockSize, thisPtext));
				break;
		}
	}	/* main loop */

	*plainTextLen = (unsigned)(plainText - outorigin);
	return CCR_SUCCESS;
}
