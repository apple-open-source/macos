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
 * byteRep.c -  FEE portable byte representation support
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 18 Apr 98 at Apple
 *	Mods for variable size giantDigit.
 * 20 Jan 98 at Apple
 *	Added curve param fields for CURVE_PARAM_VERSION 2.
 * 17 Jul 97 at Apple
 *	Added signature routines.
 *  9 Jan 97 at NeXT
 *	Split off from utilities.c
 */

#include "byteRep.h"
#include "feeTypes.h"
#include "curveParams.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "falloc.h"
#include "ckutilities.h"
#include "feeDebug.h"
#include <stdlib.h>

#ifndef	NULL
#define NULL	((void *)0)
#endif	/* NULL */

/*
 * Support for portable bytestream representation of keys and signatures.
 * Platform and endianness independent; format shared with JavaFEE
 * implementation.
 */

/*
 * Some handy macros.
 */
#define ENC_BYTE(n, b, bytes)			\
	*b++ = n;				\
	bytes++;

#define ENC_INT(n, b, bytes, i)			\
	i = intToByteRep(n, b);			\
	bytes += i;				\
	b += i;

#define ENC_GIANT(g, b, bytes, i)		\
	i = giantToByteRep(g, b);		\
	bytes += i;				\
	b += i;

#define DEC_BYTE(n, b, blen, bytes)		\
	n = *b++;				\
	bytes++;				\
	blen--;

#define DEC_INT(n, b, blen, bytes)		\
	n = byteRepToInt(b);			\
	b += sizeof(int);			\
	bytes += sizeof(int);			\
	blen -= gLen;

#define DEC_GIANT(g, b, blen, glen, bytes, out)	\
	g = byteRepToGiant(b, blen, &glen);	\
	if(g == NULL) {				\
		goto out;			\
	}					\
	b += glen;				\
	bytes += glen;				\
	blen -= gLen;




/*
 * The routines which convert various types to byte reps return the number
 * of bytes written to the output stream.
 */
int intToByteRep(int i, unsigned char *buf)
{
	*buf++ = (unsigned char)((i >> 24) & 0xff);
	*buf++ = (unsigned char)((i >> 16) & 0xff);
	*buf++ = (unsigned char)((i >> 8)  & 0xff);
	*buf   = (unsigned char)(i & 0xff);
	return 4;
}

int shortToByteRep(short s, unsigned char *buf)
{
	*buf++ = (unsigned char)((s >> 8)  & 0xff);
	*buf   = (unsigned char)(s & 0xff);
	return 2;
}

/*
 * 7 Apr 1998 : leading int is now the number of bytes in the giant's
 * giantDigits array. This value is signed.
 */
int giantToByteRep(giant g, unsigned char *buf)
{
	int numBytes = g->sign * GIANT_BYTES_PER_DIGIT;
	unsigned aNumBytes = abs(numBytes);

	CKASSERT(g != NULL);
	intToByteRep(numBytes, buf);
	buf += sizeof(int);
	serializeGiant(g, buf, aNumBytes);
	return (sizeof(int) + aNumBytes);
}

int keyToByteRep(key k, unsigned char *buf)
{
	int numBytes = 0;
	int i;

	CKASSERT(k != NULL);
	ENC_GIANT(k->x, buf, numBytes, i);

	/* only write y for plus curve */
	if(k->twist == CURVE_PLUS) {
		CKASSERT(k->y != NULL);
		ENC_GIANT(k->y, buf, numBytes, i);
	}
	return numBytes;
}

#define CURVE_PARAM_VERSION	3
#define CURVE_PARAM_VERSION_MIN	3

int curveParamsToByteRep(curveParams *cp, unsigned char *buf)
{
	int numBytes = 0;
	int i;

	CKASSERT(cp != NULL);
	ENC_INT(CURVE_PARAM_VERSION, buf, numBytes, i);
	ENC_INT(CURVE_PARAM_VERSION_MIN, buf, numBytes, i);
	ENC_BYTE(cp->primeType, buf, numBytes);
	ENC_BYTE(cp->curveType, buf, numBytes);
	ENC_INT(cp->q, buf, numBytes, i);
	ENC_INT(cp->k, buf, numBytes, i);
	ENC_INT(cp->m, buf, numBytes, i);
	ENC_INT(0, buf, numBytes, i);		// spare

	ENC_GIANT(cp->a, buf, numBytes, i);
	ENC_GIANT(cp->b, buf, numBytes, i);
	ENC_GIANT(cp->c, buf, numBytes, i);
	ENC_GIANT(cp->x1Plus, buf, numBytes, i);
	ENC_GIANT(cp->x1Minus, buf, numBytes, i);
	ENC_GIANT(cp->cOrderPlus, buf, numBytes, i);
	ENC_GIANT(cp->cOrderMinus, buf, numBytes, i);
	ENC_GIANT(cp->x1OrderPlus, buf, numBytes, i);
	ENC_GIANT(cp->x1OrderMinus, buf, numBytes, i);
	if(cp->primeType == FPT_General) {
		ENC_GIANT(cp->basePrime, buf, numBytes, i);
	}
	return numBytes;
}

int sigToByteRep(int magic,
	int version,
	int minVersion,
	giant g0,
	giant g1,
	unsigned char *buf)
{
	int numBytes = 0;
	int i;

	ENC_INT(magic, buf, numBytes, i);
	ENC_INT(version, buf, numBytes, i);
	ENC_INT(minVersion, buf, numBytes, i);
	ENC_INT(0, buf, numBytes, i);		// spare
	ENC_GIANT(g0, buf, numBytes, i);
	ENC_GIANT(g1, buf, numBytes, i);

	return numBytes;
}


/*
 * return the size of various data types' byte representations.
 */
int lengthOfByteRepGiant(giant g)
{
	CKASSERT(g != NULL);
    	return sizeof(int) + (GIANT_BYTES_PER_DIGIT * abs(g->sign));
}

int lengthOfByteRepKey(key k)
{
	int len = lengthOfByteRepGiant(k->x);

	CKASSERT(k != NULL);
	if(k->twist == CURVE_PLUS) {
		CKASSERT(k->y != NULL);
		len += lengthOfByteRepGiant(k->y);
	}
	return len;
}

int lengthOfByteRepCurveParams(curveParams *cp)
{
	int length;

	CKASSERT(cp != NULL);
	length = (6 * sizeof(int)) +		// ver, minVers, q, k, m, spare
	        2 + 				// primeType + curveType
		lengthOfByteRepGiant(cp->a) +
		lengthOfByteRepGiant(cp->b) +
		lengthOfByteRepGiant(cp->c) +
		lengthOfByteRepGiant(cp->x1Plus) +
		lengthOfByteRepGiant(cp->x1Minus) +
		lengthOfByteRepGiant(cp->cOrderPlus) +
		lengthOfByteRepGiant(cp->cOrderMinus) +
		lengthOfByteRepGiant(cp->x1OrderPlus) +
		lengthOfByteRepGiant(cp->x1OrderMinus);
	if(cp->primeType == FPT_General) {
		length += lengthOfByteRepGiant(cp->basePrime);
	}
	return length;
}

int lengthOfByteRepSig(giant g0,
	giant g1)
{
	int length = (4 * sizeof(int)) +	// magic, version, minVersion,
						// spare
	    lengthOfByteRepGiant(g0) +
	    lengthOfByteRepGiant(g1);
	return length;
}

/*
 * Routine to cons up various types from a byte rep stream.
 */
int byteRepToInt(const unsigned char *buf) {
    	int result;

    	result = (((int)buf[0] << 24) & 0xff000000) |
		 (((int)buf[1] << 16) & 0x00ff0000) |
		 (((int)buf[2] << 8) & 0xff00) |
		 (((int)buf[3]) & 0xff);
    	return result;
}

unsigned short byteRepToShort(const unsigned char *buf) {
    	unsigned short result;

    	result = (((unsigned short)buf[0] << 8) & 0xff00) |
		 (((unsigned short)buf[1]) & 0xff);
    	return result;
}

/*
 * Probably need byteRepToShortArray...
 */

/*
 * byte rep stream to giant. Returns NULL on error; returns number of bytes
 * of *buf snarfed in *giantLen if successful.
 *
 * 7 Apr 1998 : leading int is now the number of bytes in the giant's
 * giantDigits array. This value is signed.
 */
giant byteRepToGiant(const unsigned char *buf,
	unsigned bufLen,
	unsigned *giantLen)
{
	giant g;
	int numDigits;
	int numBytes;			// signed!
	unsigned aNumBytes;

   	if(bufLen < sizeof(int)) {
		return (giant)NULL;
	}
    	numBytes = byteRepToInt(buf);
	aNumBytes = abs(numBytes);
	numDigits = BYTES_TO_GIANT_DIGITS(aNumBytes);
	buf += sizeof(int);
	bufLen -= sizeof(int);
	if(numDigits > MAX_DIGITS) {
		return (giant)NULL;
	}

    	if(bufLen < aNumBytes) {
		return (giant)NULL;
	}

	/* 9 Apr 1998 - sign = 0 means no following n[] bytes in the
	 * byteRep. We do need to alloc one digit, in this case, though...
	 * Note that the giantstruct has one implicit digit in n[].
	 */
	if(aNumBytes == 0) {
	    g = (giant)fmalloc(sizeof(giantstruct));
	    g->capacity = 1;
	}
	else {
	    g = (giant)fmalloc(sizeof(giantstruct) +
	    	aNumBytes - GIANT_BYTES_PER_DIGIT);
	    g->capacity = numDigits;
	}
	deserializeGiant(buf, g, aNumBytes);

	/* deserializeGiant always cooks up positive giant; sign is
	 * properly trimmed to handle trailing (M.S.) zeroes. */
	if(numBytes < 0) {
	 	g->sign = -g->sign;
	}
	*giantLen = sizeof(int) + aNumBytes;
	return g;

}

/*
 * Convert a byte stream (and some other parameters) into a
 * keystruct.
 * Returns NULL on error; returns number of bytes of *buf snarfed in
 * *keyLen if successful.
 */
key byteRepToKey(const unsigned char *buf,
	unsigned bufLen,
	int twist,
	curveParams *cp,
	unsigned *keyLen)	// returned
{
	key k;
	giant x;
	giant y;
	unsigned gLen;
	unsigned totalLen;

	x = byteRepToGiant(buf, bufLen, &gLen);
	if(x == NULL) {
		return NULL;
	}
	bufLen  -= gLen;
	buf     += gLen;
	totalLen = gLen;
	if(twist == CURVE_PLUS) {
		/* this also contains y */
		y = byteRepToGiant(buf, bufLen, &gLen);
		if(y == NULL) {
			freeGiant(x);
			return NULL;
		}
		totalLen += gLen;
	}
	else {
		/* minus curve, y is not used */
		y = newGiant(1);
		int_to_giant(0, y);
	}
	k = (key)fmalloc(sizeof(keystruct));
	k->twist = twist;
	k->cp = cp;
	k->x = x;
	k->y = y;
	*keyLen = totalLen;
	return k;
}

curveParams *byteRepToCurveParams(const unsigned char *buf,
	unsigned bufLen,
	unsigned *cpLen)
{
	curveParams *cp;
	unsigned gLen = 0;
	int version;
	int minVersion;
	int spare;
	int bytes = 0;

	if(bufLen < (5 * sizeof(int))) {	// ver, minVers, q, k, spare
		return NULL;
	}
	cp = newCurveParams();

	DEC_INT(version, buf, bufLen, bytes);
	DEC_INT(minVersion, buf, bufLen, bytes);
	if(minVersion > CURVE_PARAM_VERSION) {
		/*
		 * Can't parse this; things have changed too much between
		 * this version of the code and the time this curveParams
		 * was written.
		 */
		goto abort;
	}

	DEC_BYTE(cp->primeType, buf, bufLen, bytes);
	DEC_BYTE(cp->curveType, buf, bufLen, bytes);
	DEC_INT(cp->q, buf, bufLen, bytes);
	DEC_INT(cp->k, buf, bufLen, bytes);
	DEC_INT(cp->m, buf, bufLen, bytes);
	DEC_INT(spare, buf, bufLen, bytes);

	DEC_GIANT(cp->a, 		buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->b, 		buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->c, 		buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->x1Plus, 		buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->x1Minus, 		buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->cOrderPlus, 	buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->cOrderMinus, 	buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->x1OrderPlus, 	buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(cp->x1OrderMinus, 	buf, bufLen, gLen, bytes, abort);

	/*
	 * basePrime only present in byte rep for PT_GENERAL
	 */
	if(cp->primeType == FPT_General) {
	    DEC_GIANT(cp->basePrime, buf, bufLen, gLen, bytes, abort);
	}

	/* remaining fields inferred */
	curveParamsInferFields(cp);
	allocRecipGiants(cp);

	*cpLen = bytes;
	return cp;

abort:
	freeCurveParams(cp);
	return NULL;
}

/*
 * Returns 0 if bad format, e.g., if minVersion of sig is > than codeVersion.
 */
int byteRepToSig(const unsigned char *buf,
	unsigned bufLen,
	int codeVersion,
	int *sigMagic,				// RETURNED
	int *sigVersion,			// RETURNED
	int *sigMinVersion,			// RETURNED
	giant *g0,					// alloc'd  & RETURNED
	giant *g1)					// alloc'd  & RETURNED
{
	unsigned gLen = 0;
	int spare;
	int bytes = 0;

	if(bufLen < (4 * sizeof(int))) {	// magic, version, minVersion,
						// spare
		return 0;
	}
	DEC_INT(*sigMagic, buf, bufLen, bytes);
	DEC_INT(*sigVersion, buf, bufLen, bytes);
	DEC_INT(*sigMinVersion, buf, bufLen, bytes);
	if(*sigMinVersion > codeVersion) {
		return 0;
	}
	DEC_INT(spare, buf, bufLen, bytes);
	// deleted 2/20/01 DEC_INT(*signerLen, buf, bufLen, bytes);
	// deleted 2/20/01 *signer = byteRepToUnichars(buf, *signerLen);
	// deleted 2/20/01 buf += (2 * *signerLen);
	// deleted 2/20/01 bufLen -= (2 * *signerLen);
	DEC_GIANT(*g0, buf, bufLen, gLen, bytes, abort);
	DEC_GIANT(*g1, buf, bufLen, gLen, bytes, abort);

	return 1;
abort:
	return 0;
}
