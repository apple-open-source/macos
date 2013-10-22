/*
	File:		comcryptPriv.c

	Contains:	private routines for comcryption library.

	Written by:	Doug Mitchell

	Copyright:	(c) 1997 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		05/28/98	dpm		Added platform-dependent ascMalloc and ascFree
		12/23/97	dpm		Added keyHash(), used result to initialize
								nybbleDex and queue.
		12/18/97	dpm		Improved queue initialization.
		12/03/97	dpm		Added queue lookahead; various optimizations.
		11/13/97	dpm		Created; broke out from comcryption.c

	To Do:
*/

#include "comcryptPriv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef	macintosh
#include <MacMemory.h>
#endif

/* if NULL, use our own */
comMallocExternFcn *comMallocExt = NULL;
comFreeExternFcn *comFreeExt = NULL;

#if		COM_STATS
comStats _comStats;

void resetComStats()
{
	memset(&_comStats, 0, sizeof(comStats));
}

void getComStats(comStats *stats)
{
	*stats = _comStats;
}

#else	/*COM_STATS*/

#define	incrComStat(stat, num)

#endif	/*COM_STATS*/

/*
 * Generate a symbol permutation from the key.
 */
void key_perm(
	const unsigned char *key,
	int 				keybytes,
	unsigned char 		*map,
	unsigned char 		*invmap)
{
    int i, j, tmp, sum;

    for(sum = 0, j = 0; j < keybytes; j++) {
		sum += key[j];
	}
	for(j=0; j < 256; j++) {
		map[j] = j;
	}
	for(j=0; j < 255; j++) {
		i = (key[j % keybytes] + j*sum) & 0xff;
		tmp = map[i];
		map[i] = map[j];
		map[j] = tmp;
	}
	for(j=0; j<256; j++) {
		invmap[map[j]] = j;
	}
}

int keybyte(
	const unsigned char *key,
	int 				keybytes,
	int 				index)
{
    return((int) key[index % keybytes]);
}

int keynybble(
	const unsigned char *key,
	int 				keybytes,
	int 				index)
{
	int i = index % (2*keybytes);
	int j;

    j = key[i>>1]; 		/* Which byte. */
    if(i & 1) j >>= 4; 	/* Which nybble. */
    return(j & 0xf);
}

/*
 * Hash a key array.
 */

#define HASH_SEED	3
#define HASH_REDUCE	1023

static unsigned keyHash(const unsigned char *key, unsigned keylen)
{
	unsigned x = HASH_SEED;  /* Any seed in [1,p-1].  Like SEED = 3. */
	unsigned ctr;

	for(ctr=0; ctr<keylen; ctr++) {
		x = (x * (key[ctr] + (ctr & HASH_REDUCE) + 1)) % HASH_PRIME;
	}
	return x;
}

void mallocCodeBufs(comcryptBuf *cbuf)
{
	/*
	 * calculate required buffer sizes. 
	 *
	 * Assume max required codeBuf size is the max size of ciphertext needed
	 * to decrypt one block of plaintext.
	 */
	cbuf->codeBufSize = comcryptMaxOutBufSize(NULL,
		CC_BLOCK_SIZE,
		CCOP_COMCRYPT,
		1);
	cbuf->codeBuf = (unsigned char *)ascMalloc(cbuf->codeBufSize);

	/*
	 * max size needed for level2Buf is the MaxOutBufSize of comcrypting
	 * a whole block of byte code. Note we assume that MaxOutBufSize(n) >= n.
	 */
	cbuf->level2BufSize = comcryptMaxOutBufSize(NULL,
		MAX_TOKENS,				// one byte per token
		CCOP_COMCRYPT,
		1);
	cbuf->level2Buf = (unsigned char *)ascMalloc(cbuf->level2BufSize);

	cbuf->queue = (queueElt *)ascMalloc(sizeof(queueElt) * QLEN);

	#if		QUEUE_LOOKAHEAD
	/*
	 * Might want to do this dynamically, though that requires the malloc
	 * of the lookAhead buffer to be done in initCodeBufs(), not here (at
	 * comcryptAlloc() time).
	 *
	 * FIXME : should do the malloc of lookAhead buffer lazily for
	 * non-Mac platforms.
	 */
	cbuf->lookAhead = (unsigned char *)ascMalloc(LOOKAHEAD_SIZE);
	#else	/* QUEUE_LOOKAHEAD */
	cbuf->lookAhead = NULL;
	#endif	/* QUEUE_LOOKAHEAD */

	/*
	 * This maybe should also be done dynamically, lazily...
	 */
	cbuf->sigArray = (unsigned *)ascMalloc((MAX_TOKENS + 1) * sizeof(unsigned));
}

void initCodeBufs(
	comcryptBuf *cbuf,
	const unsigned char *key,
	unsigned keyLen,
	unsigned char laEnable,
	unsigned char sigSeqEnable)
{
	unsigned ct;
	unsigned qval;
	unsigned char khash = (unsigned char)keyHash(key, keyLen);

	cbuf->nybbleDex = khash;

	if(laEnable) {
		memset(cbuf->lookAhead, 0, LOOKAHEAD_SIZE);
	}

	laprintf(("initing queue and lookahead\n"));

	for(ct=0; ct<QLEN; ct++) {
		/*
		 * New queue init 23 Dec - init from khash
		 */
		unsigned short sbyte = ct ^ khash;
		qval = (sbyte << 8) | ct;
		cbuf->queue[ct] = qval;
		if(laEnable) {
			markInQueue(cbuf, qval, 1);
		}
	}
	// note cbuf->nybbleDex = khash on return...

	cbuf->f1 = F1_DEFAULT;
	cbuf->f2 = F2_DEFAULT;
	cbuf->jmatchThresh = THRESH_2LEVEL_JMATCH_DEF;
	cbuf->minByteCode  = THRESH_2LEVEL_NUMBYTECODES_DEF;
	if(sigSeqEnable) {
		initSigSequence(cbuf, key, keyLen);
	}
}

void freeCodeBufs(comcryptBuf *cbuf)
{
	if(cbuf->queue != NULL) {
		ascFree(cbuf->queue);
	}
	if(cbuf->codeBuf != NULL) {
		ascFree(cbuf->codeBuf);
	}
	if(cbuf->level2Buf != NULL) {
		ascFree(cbuf->level2Buf);
	}
	if(cbuf->nextBuf != NULL) {
		freeCodeBufs(cbuf->nextBuf);
		ascFree(cbuf->nextBuf);
		cbuf->nextBuf = NULL;
	}
	if(cbuf->lookAhead != NULL) {
		ascFree(cbuf->lookAhead);
	}
	if(cbuf->sigArray != NULL) {
		ascFree(cbuf->sigArray);
	}
}

void serializeInt(
	unsigned i,
	unsigned char *buf)
{
	buf[0] = (unsigned char)(i >> 24);
	buf[1] = (unsigned char)(i >> 16);
	buf[2] = (unsigned char)(i >> 8);
	buf[3] = (unsigned char)(i & 0xff);
}

unsigned deserializeInt(unsigned char *buf)
{
	unsigned i;

	i  = ((unsigned)buf[0]) << 24;
	i |= ((unsigned)buf[1]) << 16;
	i |= ((unsigned)buf[2]) << 8;
	i |= buf[3];
	return i;
}

#if		COM_PARAM_ENABLE

unsigned getF1(comcryptObj cobj)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	return cpriv->cbuf.f1;
}

void setF1(comcryptObj cobj, unsigned f1)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	cpriv->cbuf.f1 = f1;
	if(cpriv->cbuf.nextBuf != NULL) {
		cpriv->cbuf.nextBuf->f1 = f1;
	}
}

unsigned getF2(comcryptObj cobj)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	return cpriv->cbuf.f2;
}

void setF2(comcryptObj cobj, unsigned f2)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	cpriv->cbuf.f2 = f2;
	if(cpriv->cbuf.nextBuf != NULL) {
		cpriv->cbuf.nextBuf->f2 = f2;
	}
}

unsigned getJmatchThresh(comcryptObj cobj)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	return cpriv->cbuf.jmatchThresh;
}

void setJmatchThresh(comcryptObj cobj, unsigned jmatchThresh)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	cpriv->cbuf.jmatchThresh = jmatchThresh;
	if(cpriv->cbuf.nextBuf != NULL) {
		cpriv->cbuf.nextBuf->jmatchThresh = jmatchThresh;
	}
}

unsigned getMinByteCode(comcryptObj cobj)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	return cpriv->cbuf.minByteCode;
}

void setMinByteCode(comcryptObj cobj, unsigned minByteCode)
{
	comcryptPriv *cpriv = (comcryptPriv *)cobj;

	cpriv->cbuf.minByteCode = minByteCode;
	if(cpriv->cbuf.nextBuf != NULL) {
		cpriv->cbuf.nextBuf->minByteCode = minByteCode;
	}
}

#endif	/*COM_PARAM_ENABLE*/


#if		COM_LA_DEBUG

/*
 * Verify integrity of lookahead w.r.t. queue.
 */
int testLookAhead(comcryptBuf *cbuf, int i1, int i2)
{
	unsigned i;

	if(!cbuf->laEnable) {
		return 0;
	}
	for(i=0; i<QLEN; i++) {
		if(!inQueue(cbuf, cbuf->queue[i])) {
			printf("aaagh, corrupted lookahead - in queue[], !inQueue()\n");
			printf("i=0x%x   i1=0x%x   i2=0x%x\n",
				i, i1, i2);
			printf("\n");
			exit(1);
		}
	}
	//return initTestLookAhead(cbuf);
	return 0;
}

int initTestLookAhead(comcryptBuf *cbuf)
{
	#if		QUEUE_LOOKAHEAD_BIT

	unsigned codeWord = 0;
	unsigned char bit;
	unsigned short byte;
	unsigned char *la = cbuf->lookAhead;

	for(byte=0; byte<LOOKAHEAD_SIZE; byte++) {
		for(bit=1; bit!=0; bit<<=1) {
			if(la[byte] & bit) {
				/*
				 * in lookahead, make sure it's in queue[]
				 */
				int i;
				int found = 0;

				for(i=0; i<QLEN; i++) {
					if(cbuf->queue[i] == codeWord) {
						found = 1;
						break;
					}
				}
				if(!found) {
					printf("***corrupted init lookahead - in l.a., "
						"not in queue[]\n");
					printf("codeWord 0x%x\n", codeWord);
					printf("\n");
					exit(1);
				}
			}
			codeWord++;
		}
	}

	#endif	/* QUEUE_LOOKAHEAD_BIT */
	return 0;
}

#endif	/* COM_LA_DEBUG */

void initSigSequence(comcryptBuf *cbuf,
	const unsigned char *key,
	unsigned keyLen)
{
    unsigned seed = IN_OFFSET;
	unsigned j;

    for(j=0; j<keyLen; j++) {
		seed += key[j];
    }
    seed %= HASH_PRIME;
    if(seed == 0) {
		seed = IN_OFFSET;
	}
	cbuf->sigArray[0] = (unsigned short)seed;
}

#if	0
/*
 * Called once per token bit, after processing the token.
 */
void nextSigWord(comcryptBuf *cbuf,
	unsigned sigDex,			// same as tokenDex
	unsigned match,
	unsigned above)				// jabove, keyabove
{
	unsigned offset;
	unsigned short *sigArray = cbuf->sigArray;

	#if		COM_DEBUG
	if(sigDex == 0) {
		printf("nextSigWord underflow\n");
		exit(1);
	}
	if(sigDex > MAX_TOKENS) {
		printf("nextSigWord overflow\n");
		exit(1);
	}
	#endif

	if(match) {
		offset = IN_OFFSET;
	}
	else {
		offset = OUT_OFFSET;
	}
#if		1
	sigArray[sigDex] = (sigArray[sigDex-1] * (above + offset)) % HASH_PRIME;
#endif
}
#endif

/*
 * Obfuscate a block of ciphertext.
 */
void sigMunge(comcryptBuf *cbuf,
	const unsigned char *tokenPtr,
	unsigned numTokens,
	unsigned char *byteCodePtr,
	unsigned char *longCodePtr)
{
	unsigned char tokenBit = 0x01;
	unsigned token;
	unsigned short sig;

	for(token=0; token<numTokens; token++) {
		sig = cbuf->sigArray[token];
		if(*tokenPtr & tokenBit) {
			/* no match - munge longCode - written MSB first */
			*longCodePtr++ ^= (unsigned char)(sig >> 8);
			*longCodePtr++ ^= (unsigned char)sig;
		}
		else {
			/* match - munge byteCode */
			*byteCodePtr++ ^= (unsigned char)sig;
		}
		tokenBit <<= 1;
		if(tokenBit == 0) {
			tokenBit = 0x01;
			tokenPtr++;
		}
	}
}


/*
 * All this can be optimized and tailored to specific platforms, of course...
 */

void *ascMalloc(unsigned size)
{
	#ifdef	macintosh

	Handle h;
	OSErr err;
	Ptr p;

	#endif	/* mac */
	
	if(comMallocExt != NULL) {
		return (comMallocExt)(size);
	}
	
	#ifdef	macintosh

	h = nil;
	err = errSecSuccess;

	h = NewHandleSys(size);		// system heap is not paged
	do{
		HLockHi(h);			// will move low in system heap
		err = MemError();
		if( err != errSecSuccess ) break;
		p = *h;
	}while(0);
	if( err != errSecSuccess ){
	    return NULL;
	}
	return p;

	#else	/* others...*/
	return malloc(size);
	#endif
}

void ascFree(void *data)
{
	#ifdef macintosh
	Handle h;
	#endif
	
	if(comFreeExt != NULL) {
		(comFreeExt)(data);
		return;
	}

	#ifdef macintosh
	if( data != nil ){
		h = RecoverHandle((Ptr) data);
		DisposeHandle(h);
	}

	#else	/* others */
	free(data);
	#endif
}
