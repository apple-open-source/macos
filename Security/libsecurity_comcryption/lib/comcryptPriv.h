/*
	File:		comcryptPriv.h

	Contains:	private typedefs and #defines for comcryption library.

	Written by:	Doug Mitchell

	Copyright:	(c) 1997 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		05/28/98	dpm		Added platform-dependent ascMalloc and ascFree
		12/08/97	dpm		Added Signature Sequence mechanism.
		12/03/97	dpm		Added queue lookahead; various optimizations.
		11/13/97	dpm		Created; broke out from comcryption.c

	To Do:
*/

#ifndef	_COMCRYPT_PRIV_H_
#define	_COMCRYPT_PRIV_H_

#include "comcryption.h"
#include "comDebug.h"

#ifdef __cplusplus
extern "C" {
#endif

extern comMallocExternFcn *comMallocExt;
extern comFreeExternFcn *comFreeExt;

/*
 * type of element in comcryptBuf.queue[]. Making this an unsigned int gives
 * a slight performance improvement on the i486 platform, but it does use up
 * more memory.
 */
typedef unsigned queueElt;

/*
 * Enable queue lookahead via comcryptBuf.lookAhead[]. This is currently
 * just the default value for comcryptBuf.laEnable.
 */
#define QUEUE_LOOKAHEAD		1

/*
 * lookahead queue is bit array if 1, else byte array.
 * FIXME - this will most likely be a hard-coded 1 for Mac and
 * dynamically configurable for other platforms.
 */
#define QUEUE_LOOKAHEAD_BIT	1

/*
 * Size of lookAhead buffer in bytes.
 */
#if		QUEUE_LOOKAHEAD_BIT
/*
 * 1 bit per potential queueElt value.
 */
#define LOOKAHEAD_SIZE		(1 << ((2 * 8) - 3))
#else	/* QUEUE_LOOKAHEAD_BIT */
/*
 * One byte per queueElt value; avoids shifts and masks in accessing
 * array elements at the cost of additional memory.
 */
#define LOOKAHEAD_SIZE		(1 << (2 * 8))
#endif	/* QUEUE_LOOKAHEAD_BIT */

/*
 * When true, optimize away the cost of the keynybble() call on a hit
 * on queue[0].
 */
#define SKIP_NIBBLE_ON_QUEUE_0		1

/*
 * pre-malloc'd buffers, one per level of comcryption. This allows each level
 * to maintain its own queue state machine as well as its own comcryption
 * parameters.
 */
typedef struct _comcryptBuf {
	queueElt 					*queue;			// mallocd, QLEN elements
	unsigned					nybbleDex;		// index for keynybble()
	struct _comcryptBuf			*nextBuf;		// for recursion

	/*
	 * Used to temporarily store bytecode fragments during comcryption and
	 * partial blocks during decomcryption.
	 */
	unsigned char				*codeBuf;
	unsigned					codeBufSize;	// malloc'd size of codeBuf
	unsigned					codeBufLength;	// valid bytes in codeBuf

	/*
	 * Buffer for two-level comcryption. During comcryption, 2nd level
	 * comcrypted bytecode is placed here. During decomcryption, the result
	 * of decomcrytping the 2nd level bytecode is placed here.
	 */
	unsigned char				*level2Buf;
	unsigned					level2BufSize;	// malloc'd size of level2Buf

	/*
	 * comcryption parameters, may (eventually) be different for different
	 * levels. Tweakable, for now, only via private API in comDebug.h.
	 */
	unsigned					f1;
	unsigned					f2;
	unsigned					jmatchThresh;	// max avg jmatch for 2 level
	unsigned					minByteCode;	// min numByteCodes for 2 level

	/*
	 * Bit map, one bit per potential value in queue[]; 1 means "this value
	 * is somewhere in queue[]"
	 */
	unsigned char				*lookAhead;

	/*
	 * Signature Sequence array - to be Xord with ciphertext
	 * size = MAX_TOKENS
	 */
	unsigned					*sigArray;
} comcryptBuf;


/*
 * Private struct associated with client's comcryptObj.
 */
typedef struct {
	unsigned char 			*key;
	unsigned 				keybytes;			// valid bytes in *key
	comcryptOptimize 		optimize;			// CCO_SIZE, etc.
	unsigned char 			*map;
	unsigned char 			*invmap;
	unsigned				version;			// from ciphertext
	unsigned				versionBytes;		// valid bytes in version;
												//   also nonzero on comcrypt
												//   means version has been
												//   written
	unsigned				spareBytes;			// # ciphertext header spare
												//   bytes skipped
	comcryptBuf				cbuf;

	/*
	 * To save a tiny bit of memory, these could/should be bits, but
	 * we examine some of them on every code word, so we'll expand them into
	 * bytes...
	 */
	unsigned char				laEnable;		// lookahead enable
	unsigned char				sigSeqEnable;	// signature sequence enable
	unsigned char				level2enable;	// 2-level comcryption

} comcryptPriv;


/*
 * Block and buffer sizes. Subject to tweaking...
 */
#define CC_BLOCK_SIZE		256						/* bytes of plaintext */

/*
 * For comcryptMaxInBufSize(CCOP_COMCRYPT), if outBufSize exceeds this
 * threshhold, truncate the max inBufSize so that
 * inBufSize = 0 mod CC_BLOCK_SIZE.
 */
#define INBUF_TRUNC_THRESH	(16 * 1024)

/*
 * Macros to calculate number of token bits and bytes associated with
 * a quantity of plaintext (in bytes)
 */
#define TOKEN_BITS_FROM_PTEXT(pt)			((pt + 1) >> 1)
#define TOKEN_BYTES_FROM_PTEXT(pt)			((pt + 15) >> 4)
#define TOKEN_BYTES_FROM_TOKEN_BITS(tb)		((tb + 7) >> 3)

/*
 * Max number of token bits or code fragments in a block
 */
#define MAX_TOKENS				(CC_BLOCK_SIZE / 2)

/*
 * Size of comcryptBuf.queue[].
 */
#define QLEN 					256

/*
 * FIXME - some info on these constants?
 */
#define F1_DEFAULT				12
#define F2_DEFAULT				12
#define ABOVE(F2) 				((F2 * QLEN) >> 4)

/*
 * Constants for obfuscation via signature sequence.
 */
#define HASH_Q		19
#define HASH_PRIME 	((1<<HASH_Q)-1)  	/* Must be prime less than 2^19. */
#define IN_OFFSET	3  					/* Must be in [1,255]. */
#define OUT_OFFSET 	5 					/* Must be in [1,255]. */

/*
 * Ciphertext structure:
 *
 *   4 bytes of version
 *   4 bytes spare
 *   n blocks, format described below
 */
#define VERSION_3_Dec_97 		0xc0de0003
#define VERSION_BYTES			4
#define SPARE_BYTES				4
#define	CTEXT_HDR_SIZE			(VERSION_BYTES + SPARE_BYTES)

/*
 * Format of CBD_SINGLE block
 *
 * 		block description (see CBD_xxx, below)
 *		number of longCodes
 *		number of tokens - optional, absent if CBD_FULL_BLOCK
 *		token array
 *		longCode array
 *		byteCode array - length implied from number of longCodes, tokens
 *
 * Format of CBD_DOUBLE block
 *
 * 		block description (see CBD_xxx, below)
 *		number of longCodes
 *		number of tokens - optional, absent if CBD_FULL_BLOCK
 *		token array
 *		longCode array
 *		length of 2nd level comcrypted byte code to follow
 *		2nd level comcrypted byte code array
 */

/*
 * Offsets (block-relative) of ciphertext components. All fields are byte-wide.
 * This limits block size to < 512 (the limiting case is a whole block of
 * bytecodes or a whole block of longcodes). Changing the counts to
 * two bytes would add flexibility and is necessary for block sizes of 512
 * or greater, but it would cost up to 3 bytes per block.
 */
#define CTBO_BLOCK_DESC			0x00	/* descriptor bits, see below */
#define CTBO_NUM_LONG_CODES		0x01	/* in 16-bit words */

/*
 * if block[CTBO_BLOCK_DESC] & CBD_FULL_BLOCK, the following byte
 * is deleted (actually, implied) and subsequent fields are moved
 * up one byte. This saves one byte per block for most blocks.
 */
#define CTBO_NUM_TOKENS			0x02

/*
 * Offsets of remaining fields not constant; they depend on CBD_FULL_BLOCK and
 * CBD_SINGLE/CBD_DOUBLE.
 */

/*
 * Min block size - blockDesc, numLongCodes, numTokens, one token byte,
 * one bytecode
 */
#define MIN_CBLOCK_SIZE			5	/* min cipherblock size */

/*
 * Max block size - blockDesc, numLongCodes, full block's tokens, and
 * a full block of longcodes
 */
#define MAX_CBLOCK_SIZE			(2 +				\
		TOKEN_BYTES_FROM_PTEXT(CC_BLOCK_SIZE) +		\
		CC_BLOCK_SIZE)

/*
 * Bits in block[CTBO_BLOCK_DESC]
 */
#define CBD_MAGIC				0xd0	/* high nibble must be 0xd */
#define CBD_MAGIC_MASK			0xf0
#define CBD_BLOCK_TYPE_MASK		0x01
#define	CBD_SINGLE				0x00	/* single-level comcrypt */
#define CBD_DOUBLE				0x01	/* double-level comcrypt */
#define CBD_ODD_MASK			0x02
#define CBD_ODD					0x02	/* last code maps to single */
										/*   (odd) byte */
#define CBD_EVEN				0x00
#define CBD_FULL_BLOCK_MASK		0x04
#define CBD_FULL_BLOCK			0x04	/* expands to CC_BLOCK_SIZE, also  */
										/* implies no CTBO_NUM_TOKENS byte
										 * in block */
/*
 * Defining this non-zero limits effective key size to 40 bits for export
 */
#define COMCRYPT_EXPORT_ONLY	0
#define EXPORT_KEY_SIZE			5		/* in bytes */

/*
 * Threshholds for performing 2-level comcrypt
 */
#define THRESH_2LEVEL_JMATCH_DEF		40		/* max average jmatch */
#define THRESH_2LEVEL_NUMBYTECODES_DEF	30		/* min number of bytecodes */


/*
 * Private routines in comcryptPriv.c
 */
extern void key_perm(
	const unsigned char *key,
	int 				keybytes,
	unsigned char 		*map,
	unsigned char 		*invmap);
extern int keybyte(
	const unsigned char *key,
	int 				keybytes,
	int 				index);
extern int keynybble(
	const unsigned char *key,
	int 				keybytes,
	int 				index);
extern void mallocCodeBufs(comcryptBuf *cbufs);
extern void freeCodeBufs(comcryptBuf *cbufs);
extern void initCodeBufs(
	comcryptBuf *cbuf,
	const unsigned char *key,
	unsigned keyLen,
	unsigned char laEnable,
	unsigned char sigSeqEnable);
#if	0
extern void serializeShort(
	unsigned short s,
	unsigned char *buf);
unsigned short deserializeShort(unsigned char *buf);
#endif	/*0*/
void serializeInt(
	unsigned i,
	unsigned char *buf);
unsigned deserializeInt(unsigned char *buf);
void initSigSequence(comcryptBuf *cbuf,
	const unsigned char *key,
	unsigned keyLen);
void sigMunge(comcryptBuf *cbuf,
	const unsigned char *tokenPtr,
	unsigned numTokens,
	unsigned char *byteCodePtr,
	unsigned char *longCodePtr);
#if		0
void nextSigWord(comcryptBuf *cbuf,
	unsigned sigDex,			// same as tokenDex
	unsigned match,
	unsigned above);
#endif

#if		COM_LA_DEBUG
extern int testLookAhead(comcryptBuf *cbuf, int i1, int i2);
extern int initTestLookAhead(comcryptBuf *cbuf);
#else	/*COM_LA_DEBUG*/
#define testLookAhead(cbuf, i1, i2)
#define initTestLookAhead(cbuf)
#endif	/* COM_LA_DEBUG */

/*
 * Routines written as macros solely for performance reasons
 */

/*
 * try a couple different mersenne mods...
 */
#define MOD_HASH(x) { 							\
	while(x > HASH_PRIME) { 					\
		x = (x >> HASH_Q) + (x & HASH_PRIME); 	\
	} 											\
}

/*
 * Haven't gotten this to work for the Mac yet...
 */
#ifdef	NeXT
#define SIG_WORD_INLINE 1
#else	/*NeXT*/
#define SIG_WORD_INLINE 0
#endif

#if		SIG_WORD_INLINE

static inline void nextSigWord(comcryptBuf *cbuf,
	unsigned sigDex,			// same as tokenDex
	unsigned match,
	unsigned above)				// (jabove, keyabove) + nibbleDex
{
	unsigned offset;
	unsigned *sigArray = cbuf->sigArray;

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
	sigArray[sigDex] = sigArray[sigDex-1] * (above + offset);
	MOD_HASH(sigArray[sigDex]);
}

#else	/*SIG_WORD_INLINE*/

#define nextSigWord(cbuf, sigDex, match, above) {					\
	unsigned offset = (match ? IN_OFFSET : OUT_OFFSET);				\
	unsigned *sigArray = cbuf->sigArray;							\
	unsigned result = (sigArray[sigDex-1] * (above + offset));		\
	MOD_HASH(result);												\
	sigArray[sigDex] = result;										\
}

#endif	/*SIG_WORD_INLINE*/

/*
 * Inline serializeShort(), deserializeShort()
 */
#define serializeShort(s, buf)  		\
	buf[0] = (unsigned char)(s >> 8);	\
	buf[1] = (unsigned char)(s);		\

#define deserializeShort(s, buf)		\
	s = ((unsigned short)buf[0]) << 8;	\
	s |= buf[1];						\


/*
 * General purpose macros for accessing bit arrays. Used for accessing
 * token bits and lookahead array bits if QUEUE_LOOKAHEAD_BIT = 1.
 */
#define MARK_BIT_ARRAY(cp, index, val) {						\
	unsigned char bit = 1 << (index & 7);						\
	unsigned char *bytePtr = &cp[index>>3];						\
	if(val) {													\
		*bytePtr |= bit;										\
	}															\
	else {														\
		*bytePtr &= ~bit;										\
	}															\
}
#define GET_BIT_ARRAY(cp, index) 								\
	(cp[index >> 3] & (1 << (index & 7)))

#define getToken(tokenPtr, tokenDex) 				\
	GET_BIT_ARRAY(tokenPtr, tokenDex)

#define updateToken(tokenPtr, tokenDex, tokenBit) 	\
	MARK_BIT_ARRAY(tokenPtr, tokenDex, tokenBit)

/*
 * Macros for accessing lookahead array elements
 */

#if		QUEUE_LOOKAHEAD_BIT
/*
 * This way saves memory
 */
#define markInQueue(cbuf, codeWord, val) 			\
	MARK_BIT_ARRAY(cbuf->lookAhead, codeWord, val)

#define inQueue(cbuf, codeWord)						\
	GET_BIT_ARRAY(cbuf->lookAhead, codeWord)

#else	/* QUEUE_LOOKAHEAD_BIT */

/*
 * This way saves time
 */
#define markInQueue(cbuf, codeWord, val) {		\
	cbuf->lookAhead[codeWord] = val;			\
}
#define inQueue(cbuf, codeWord)		(cbuf->lookAhead[codeWord])

#endif	/* QUEUE_LOOKAHEAD_BIT */

void *ascMalloc(unsigned size);
void ascFree(void *data);

#ifdef __cplusplus
}
#endif

#endif	/*_COMCRYPT_PRIV_H_*/
