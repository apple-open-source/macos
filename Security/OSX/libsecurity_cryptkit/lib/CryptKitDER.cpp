/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * CryptKitDER.h - snacc-based routines to create and parse DER-encoded FEE 
 *				   keys and signatures
 *
 */

#include "ckconfig.h"

#if	CRYPTKIT_DER_ENABLE

#include <security_cryptkit/CryptKitDER.h>
#include <security_cryptkit/falloc.h>
#include <security_cryptkit/feeDebug.h>
#include <security_cryptkit/feeFunctions.h>
#include <security_cryptkit/ckutilities.h>
#include "CryptKitAsn1.h"
#include <security_asn1/SecNssCoder.h>
#include <security_asn1/nssUtils.h>
#include <Security/keyTemplates.h>
#include <Security/oidsalg.h>
#include <Security/oidsattr.h>

#define PRINT_SIG_GIANTS		0
#define PRINT_CURVE_PARAMS		0
#define PRINT_SIZES				0
#if		PRINT_SIZES
#define szprint(s)				printf s
#else
#define szprint(s)
#endif

/*
 * Trivial exception class associated with a feeReturn.
 */
class feeException
{
protected:
	feeException(feeReturn frtn, const char *op); 	
public:
	~feeException() throw() {}
	feeReturn frtn() const throw() { return mFrtn; }
    static void throwMe(feeReturn frtn, const char *op = NULL) __attribute__((noreturn));
private:
	feeReturn mFrtn;
};

feeException::feeException(
	feeReturn frtn, 
	const char *op)
		: mFrtn(frtn)
{ 
	if(op) {
		dbgLog(("%s: %s\n", op, feeReturnString(frtn)));
	}
}

void feeException::throwMe(feeReturn frtn, const char *op /*= NULL*/) { throw feeException(frtn, op); }

/*
 * ASN1 encoding rules specify that an integer's sign is indicated by the MSB
 * of the first (MS) content byte. For a non-negative number, if the MSB of 
 * the MS byte (of the unencoded number) is one, then the encoding starts with
 * a byte of zeroes to indicate positive sign. For a negative number, the first
 * nine bits can not be all 1 - if they are (in the undecoded number), leading 
 * bytes of 0xff are trimmed off until the first nine bits are something other
 * than one. Also, the first nine bits of the encoded number can not all be 
 * zero. 
 *
 * CryptKit giants express their sign as part of the giantstruct.sign field. 
 * The giantDigit array (giantstruct.n[]) is stored l.s. digit first. 
 *
 * These routines are independent of platform, endianness, and giatn digit size. 
 */

/* routines to guess maximum size of DER-encoded objects */ 
static unsigned feeSizeOfSnaccGiant(
	giant g)
{
	unsigned rtn = abs(g->sign) * GIANT_BYTES_PER_DIGIT;
	szprint(("feeSizeOfSnaccGiant: sign %d size %d\n", g->sign, rtn + 4));
	return rtn + 4;
}

/* PUBLIC... */
unsigned feeSizeOfDERSig(
	giant g1,
	giant g2)
{
	unsigned rtn = feeSizeOfSnaccGiant(g1);
	rtn += feeSizeOfSnaccGiant(g2);
	szprint(("feeSizeOfDERSig: size %d\n", rtn + 4));
	return rtn + 4;
}

/* perform 2's complement of byte array, expressed MS byte first */
static void twosComplement(
	unsigned char *bytePtr,		// points to MS byte
	unsigned numBytes)
{
	unsigned char *outp = bytePtr + numBytes - 1;
	unsigned char carry = 1;	// first time thru, carry = 1 to add one to 1's comp
	for(unsigned byteDex=0; byteDex<numBytes; byteDex++) {
		/* first complement, then add carry */
		*outp = ~*outp + carry;
		if(carry && (*outp == 0)) {
			/* overflow/carry */
			carry = 1;
		}
		else {
			carry = 0;
		}
		outp--;
	}
}

/*
 * CSSM_DATA --> unsigned int
 */
static unsigned cssmDataToInt(
	const CSSM_DATA &cdata)
{
	if((cdata.Length == 0) || (cdata.Data == NULL)) {
		return 0;
	}
	unsigned len = (unsigned)cdata.Length;
	if(len > sizeof(int)) {
		feeException::throwMe(FR_BadKeyBlob, "cssmDataToInt");
	}
	
	unsigned rtn = 0;
	uint8 *cp = cdata.Data;
	for(unsigned i=0; i<len; i++) {
		rtn = (rtn << 8) | *cp++;
	}
	return rtn;
}

/*
 * unsigned int --> CSSM_DATA, mallocing from an SecNssCoder 
 */
static void intToCssmData(
	unsigned num,
	CSSM_DATA &cdata,
	SecNssCoder &coder)
{
	unsigned len = 0;
	
	if(num < 0x100) {
		len = 1;
	}
	else if(num < 0x10000) {
		len = 2;
	}
	else if(num < 0x1000000) {
		len = 3;
	}
	else {
		len = 4;
	}
	cdata.Data = (uint8 *)coder.malloc(len);
	cdata.Length = len;
	uint8 *cp = &cdata.Data[len - 1];
	for(unsigned i=0; i<len; i++) {
		*cp-- = num & 0xff;
		num >>= 8;
	}
}

/*
 * Convert a decoded ASN integer, as a CSSM_DATA, to a (mallocd) giant. 
 * Only known exception is a feeException.
 */
static giant cssmDataToGiant(
	const CSSM_DATA 	&cdata)
{
	char *rawOcts = (char *)cdata.Data;
	unsigned numBytes = (unsigned)cdata.Length;
	unsigned numGiantDigits;
	int sign = 1;
	giant grtn;
	feeReturn frtn = FR_Success;
	unsigned char *inp = NULL;
	unsigned digitDex;			// index into g->giantDigit[]
	
	/* handle degenerate case (value of zero) */
	if((numBytes == 0) || ((numBytes == 1) && rawOcts[0] == 0)) {
		grtn = newGiant(1);
		if(grtn == NULL) {
			feeException::throwMe(FR_Memory, "newGiant(1)");
		}
		int_to_giant(0, grtn);
		return grtn;
	}
	
	/* make a copy of raw octets if we have to do two's complement */
	unsigned char *byteArray = NULL;
	bool didMalloc = false;
	if(rawOcts[0] & 0x80) {
		sign = -1;
		numBytes++;
		byteArray = (unsigned char *)fmalloc(numBytes);
		didMalloc = true;
		byteArray[0] = 0xff;
		memmove(byteArray + 1, rawOcts, numBytes-1);
		twosComplement(byteArray, numBytes);
	}
	else {
		/* no copy */
		char *foo = rawOcts;
		byteArray = (unsigned char *)foo;
	}
	
	/* cook up a new giant */
	numGiantDigits = (numBytes + GIANT_BYTES_PER_DIGIT - 1) /
			GIANT_BYTES_PER_DIGIT;
	grtn = newGiant(numGiantDigits);
	if(grtn == NULL) {
		frtn = FR_Memory;
		goto abort;
	}

	/* 
	 * Convert byteArray to array of giantDigits
	 * inp - raw input bytes, LSB last
	 * grtn->n[] - output array of giantDigits, LSD first
	 * Start at LS byte and LD digit
	 */
	digitDex = 0;					// index into g->giantDigit[]
	giantDigit thisDigit;
	inp = byteArray + numBytes - 1;	
	unsigned dex;					// total byte counter
	unsigned byteDex;				// index into one giantDigit
	unsigned shiftCount;
	for(dex=0; dex<numBytes; ) {	// increment dex inside
		thisDigit = 0;
		shiftCount = 0;
		for(byteDex=0; byteDex<GIANT_BYTES_PER_DIGIT; byteDex++) {
			thisDigit |= ((giantDigit)(*inp--) << shiftCount);
			shiftCount += 8;
			if(++dex == numBytes) {
				/* must be partial giantDigit */
				break;
			}
		}
		CKASSERT(digitDex < numGiantDigits);
		grtn->n[digitDex++] = thisDigit;
	}
	grtn->sign = (int)numGiantDigits * sign;
	
	/* trim leading (MS) zeroes */
	gtrimSign(grtn);
abort:
	if(didMalloc) {
		ffree(byteArray);
	}
	if(frtn) {
		feeException::throwMe(frtn, "bigIntStrToGiant");
	}
	return grtn;
}

/*
 * Convert a giant to an CSSM_DATA, mallocing using specified coder. 
 * Only known exception is a feeException.
 */
 static void giantToCssmData(
	giant 		g,
	CSSM_DATA 	&cdata,
	SecNssCoder	&coder)
{
	unsigned char doPrepend = 0;	
	unsigned numGiantDigits = abs(g->sign);
	unsigned numBytes = numGiantDigits * GIANT_BYTES_PER_DIGIT;
	giantDigit msGiantBit = 0;
	if(isZero(g)) {
		/* special degenerate case */
		intToCssmData(0, cdata, coder);
		return;
	}
	else {
		msGiantBit = g->n[numGiantDigits - 1] >> (GIANT_BITS_PER_DIGIT - 1);
	}
	
	/* prepend a byte of zero if necessary */
	if((g->sign < 0) ||					// negative - to handle 2's complement 
	   ((g->sign > 0) && msGiantBit)) {	// ensure MS byte is zero
			doPrepend = 1;
			numBytes++;
	}
	
	unsigned char *rawBytes = (unsigned char *)fmalloc(numBytes);
	if(rawBytes == NULL) {
		feeException::throwMe(FR_Memory, "giantToCssmData fmalloc(rawBytes)");
	}
	unsigned char *outp = rawBytes;
	if(doPrepend) {
		*outp++ = 0;
	}
	
	/* 
	 * Convert array of giantDigits to bytes. 
	 * outp point to MS output byte.
	 */
	int digitDex;			// index into g->giantDigit[]
	unsigned byteDex;		// byte index into a giantDigit
	for(digitDex=numGiantDigits-1; digitDex>=0; digitDex--) {
		/* one loop per giantDigit, starting at MS end */
		giantDigit thisDigit = g->n[digitDex];
		unsigned char *bp = outp + GIANT_BYTES_PER_DIGIT - 1;
		for(byteDex=0; byteDex<GIANT_BYTES_PER_DIGIT; byteDex++) {
			/* one loop per byte within the digit, starting at LS end */
			*bp-- = (unsigned char)(thisDigit) & 0xff;
			thisDigit >>= 8;
		}
		outp += GIANT_BYTES_PER_DIGIT;
	}
	
	/* do two's complement for negative giants */
	if(g->sign < 0) {
		twosComplement(rawBytes, numBytes);
	}
	
	/* strip off redundant leading bits (nine zeroes or nine ones) */
	outp = rawBytes;
	unsigned char *endp = outp + numBytes - 1;
	while((*outp == 0) &&			// m.s. byte zero
	      (outp < endp) &&			// more bytes exist
		  (!(outp[1] & 0x80))) {	// 9th bit is 0
		outp++;
		numBytes--;
	}
	while((*outp == 0xff) &&		// m.s. byte all ones
	      (outp < endp) &&			// more bytes exist
		  (outp[1] & 0x80)) {		// 9th bit is 1
		outp++;
		numBytes--;
	}
	cdata.Data = (uint8 *)coder.malloc(numBytes);
	memmove(cdata.Data, outp, numBytes);
	cdata.Length = numBytes;
	ffree(rawBytes);
	return;
}

/* curveParams : CryptKit <--> FEECurveParametersASN1 */
/* Only known exception is a feeException */
static void feeCurveParamsToASN1(
	const curveParams *cp,
	FEECurveParametersASN1 &asnCp,
	SecNssCoder &coder)
{
	#if 	PRINT_CURVE_PARAMS
	printf("===encoding curveParams; cp:\n"); printCurveParams(cp);
	#endif
	memset(&asnCp, 0, sizeof(asnCp));
	try {
		intToCssmData(cp->primeType, asnCp.primeType, coder);
		intToCssmData(cp->curveType, asnCp.curveType, coder);
		intToCssmData(cp->q, asnCp.q, coder);
		intToCssmData(cp->k, asnCp.k, coder);
		intToCssmData(cp->m, asnCp.m, coder);
		giantToCssmData(cp->a, asnCp.a, coder);
		giantToCssmData(cp->b, asnCp.b_, coder);
		giantToCssmData(cp->c, asnCp.c, coder);
		giantToCssmData(cp->x1Plus, asnCp.x1Plus, coder);
		giantToCssmData(cp->x1Minus, asnCp.x1Minus, coder);
		giantToCssmData(cp->cOrderPlus, asnCp.cOrderPlus, coder);
		giantToCssmData(cp->cOrderMinus, asnCp.cOrderMinus, coder);
		giantToCssmData(cp->x1OrderPlus, asnCp.x1OrderPlus, coder);
		giantToCssmData(cp->x1OrderMinus, asnCp.x1OrderMinus, coder);
		if(cp->primeType == FPT_General) {
			giantToCssmData(cp->basePrime, asnCp.basePrime, coder);
		}
	}
	catch(const feeException &ferr) {
		throw;
	}
	catch(...) {
		feeException::throwMe(FR_Memory, "feeCurveParamsToSnacc catchall");	// ???
	}
}

static curveParams *feeCurveParamsFromAsn1(
	const FEECurveParametersASN1 &asnCp)
{
	curveParams *cp = newCurveParams();
	if(cp == NULL) {
		feeException::throwMe(FR_Memory, "feeCurveParamsFromSnacc alloc cp");
	}
	cp->primeType = (feePrimeType)cssmDataToInt(asnCp.primeType);
	cp->curveType = (feeCurveType)cssmDataToInt(asnCp.curveType);
	cp->q 			   = cssmDataToInt(asnCp.q);
	cp->k 			   = cssmDataToInt(asnCp.k);
	cp->m 			   = cssmDataToInt(asnCp.m);
	cp->a 			   = cssmDataToGiant(asnCp.a);
	cp->b 			   = cssmDataToGiant(asnCp.b_);
	cp->c              = cssmDataToGiant(asnCp.c);
	cp->x1Plus         = cssmDataToGiant(asnCp.x1Plus);
	cp->x1Minus        = cssmDataToGiant(asnCp.x1Minus);
	cp->cOrderPlus     = cssmDataToGiant(asnCp.cOrderPlus);
	cp->cOrderMinus    = cssmDataToGiant(asnCp.cOrderMinus);
	cp->x1OrderPlus    = cssmDataToGiant(asnCp.x1OrderPlus);
	cp->x1OrderMinus   = cssmDataToGiant(asnCp.x1OrderMinus);
	if(asnCp.basePrime.Data != NULL) {
		cp->basePrime  = cssmDataToGiant(asnCp.basePrime);
	}
	
	/* remaining fields inferred */
	curveParamsInferFields(cp);
	allocRecipGiants(cp);
	#if 	PRINT_CURVE_PARAMS
	printf("===decoding curveParams; cp:\n"); printCurveParams(cp);
	#endif
	return cp;
}

/***
 *** Public routines. These are usable from C code; they never throw.
 ***/
 
/*
 * Encode/decode the two FEE signature types. We malloc returned data via
 * fmalloc(); caller must free via ffree().
 */
feeReturn feeDEREncodeElGamalSignature(
	giant			u,
	giant			PmX,
	unsigned char	**encodedSig,		// fmallocd and RETURNED
	unsigned		*encodedSigLen)		// RETURNED
{
	/* convert to FEEElGamalSignatureASN1 */
	FEEElGamalSignatureASN1 asnSig;
	SecNssCoder coder;
	
	try {
		giantToCssmData(u, asnSig.u, coder);
		giantToCssmData(PmX, asnSig.pmX, coder);
	} 
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	
	/* DER encode */
	PRErrorCode perr;
	CSSM_DATA encBlob;			// mallocd by coder
	perr = coder.encodeItem(&asnSig, FEEElGamalSignatureASN1Template, encBlob);
	if(perr) {
		return FR_Memory;
	}

	/* copy out  to caller */
	*encodedSig = (unsigned char *)fmalloc((unsigned)encBlob.Length);
	*encodedSigLen = (unsigned)encBlob.Length;
	memmove(*encodedSig, encBlob.Data, encBlob.Length); 
	
	#if	PRINT_SIG_GIANTS
	printf("feeEncodeElGamalSignature:\n");
	printf("   u   : "); printGiantHex(u);
	printf("   PmX : "); printGiantHex(PmX);
	#endif
	
	return FR_Success;
}

/*
 * Encode a DER formatted ECDSA signature
 */
feeReturn feeDEREncodeECDSASignature(
	giant			c,
	giant			d,
	unsigned char	**encodedSig,		// fmallocd and RETURNED
	unsigned		*encodedSigLen)		// RETURNED
{
	/* convert to FEEECDSASignatureASN1 */
	FEEECDSASignatureASN1 asnSig;
	SecNssCoder coder;
	
	try {
		giantToCssmData(c, asnSig.c, coder);
		giantToCssmData(d, asnSig.d, coder);
	} 
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	
	/* DER encode */
	PRErrorCode perr;
	CSSM_DATA encBlob;			// mallocd by coder
	perr = coder.encodeItem(&asnSig, FEEECDSASignatureASN1Template, encBlob);
	if(perr) {
		return FR_Memory;
	}

	/* copy out  to caller */
	*encodedSig = (unsigned char *)fmalloc((unsigned)encBlob.Length);
	*encodedSigLen = (unsigned)encBlob.Length;
	memmove(*encodedSig, encBlob.Data, encBlob.Length); 
	
	#if	PRINT_SIG_GIANTS
	printf("feeDEREncodeECDSASignature:\n");
	printf("   c   : "); printGiantHex(c);
	printf("   d   : "); printGiantHex(d);
	#endif
	return FR_Success;

}

#if PRINT_SIG_GIANTS
static void printHex(
              const unsigned char *buf,
              unsigned len,
              unsigned maxLen)
{
    bool doEllipsis = false;
    unsigned dex;
    if(len > maxLen) {
        len = maxLen;
        doEllipsis = true;
    }
    for(dex=0; dex<len; dex++) {
        printf("%02X ", *buf++);
    }
    if(doEllipsis) {
        printf("...etc.");
    }
}
#endif

/*
 * Encode a RAW formatted ECDSA signature
 */
feeReturn feeRAWEncodeECDSASignature(unsigned       groupBytesLen,
                                     giant			c,
                                     giant			d,
                                     unsigned char	**encodedSig,		// fmallocd and RETURNED
                                     unsigned		*encodedSigLen)		// RETURNED
{
    /* copy out  to caller */
    *encodedSig = (unsigned char *)fmalloc(2*groupBytesLen);
    *encodedSigLen = (unsigned)2*groupBytesLen;

    /* convert to FEEECDSASignatureASN1 */
    try {
        serializeGiant(c, *encodedSig, groupBytesLen);
        serializeGiant(d, *encodedSig+groupBytesLen, groupBytesLen);
    }
    catch(const feeException &ferr) {
        return ferr.frtn();
    }

#if	PRINT_SIG_GIANTS
    printf("feeRAWEncodeECDSASignature:\n");
    printf("   c   : "); printGiantHex(c);
    printf("   d   : "); printGiantHex(d);
    printf("   sig : "); printHex(*encodedSig,*encodedSigLen,512);
#endif
    return FR_Success;
    
}

feeReturn feeDERDecodeElGamalSignature(
	const unsigned char	*encodedSig,
	size_t				encodedSigLen,
	giant				*u,				// newGiant'd and RETURNED
	giant				*PmX)			// newGiant'd and RETURNED
{
	FEEElGamalSignatureASN1 asnSig;
	SecNssCoder coder;
	
	memset(&asnSig, 0, sizeof(asnSig));
	PRErrorCode perr = coder.decode(encodedSig, encodedSigLen, 
		FEEElGamalSignatureASN1Template, &asnSig);
	if(perr) {
		return FR_BadSignatureFormat;
	}

	try {
		*u   = cssmDataToGiant(asnSig.u);
		*PmX = cssmDataToGiant(asnSig.pmX);
	}
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_Memory;
	}
	#if	PRINT_SIG_GIANTS
	printf("feeDecodeElGamalSignature:\n");
	printf("   u   : "); printGiantHex(*u);
	printf("   PmX : "); printGiantHex(*PmX);
	#endif
	return FR_Success;
}

/*
 * Decode a DER formatted ECDSA signature
 */
feeReturn feeDERDecodeECDSASignature(
	const unsigned char	*encodedSig,
	size_t				encodedSigLen,
	giant				*c,				// newGiant'd and RETURNED
	giant				*d)				// newGiant'd and RETURNED
{
	FEEECDSASignatureASN1 asnSig;
	SecNssCoder coder;
	
	memset(&asnSig, 0, sizeof(asnSig));
	PRErrorCode perr = coder.decode(encodedSig, encodedSigLen, 
		FEEECDSASignatureASN1Template, &asnSig);
	if(perr) {
		return FR_BadSignatureFormat;
	}

	try {
		*c = cssmDataToGiant(asnSig.c);
		*d = cssmDataToGiant(asnSig.d);
	}
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_Memory;
	}
#if	PRINT_SIG_GIANTS
	printf("feeDERDecodeECDSASignature:\n");
    printf("   c   : "); printGiantHex(*c);
    printf("   d   : "); printGiantHex(*d);
#endif
	return FR_Success;
}

/*
 * Decode a RAW formatted ECDSA signature
 */
feeReturn feeRAWDecodeECDSASignature(unsigned groupBytesLen,
                                     const unsigned char	*encodedSig,
                                     size_t				encodedSigLen,
                                     giant				*c,				// newGiant'd and RETURNED
                                     giant				*d)				// newGiant'd and RETURNED
{

    // Size must be even
    if (((encodedSigLen & 1) == 1) || (groupBytesLen != (encodedSigLen>>1))) {
        return FR_BadSignatureFormat;
    }

    try {
        *c = giant_with_data((uint8_t*)encodedSig,(int)groupBytesLen);
        *d = giant_with_data((uint8_t*)encodedSig+groupBytesLen, (int)groupBytesLen);
    }
    catch(const feeException &ferr) {
        return ferr.frtn();
    }
    catch(...) {
        /* FIXME - bad sig? memory? */
        return FR_Memory;
    }
#if	PRINT_SIG_GIANTS
    printf("feeRAWDecodeECDSASignature:\n");
    printf("   c   : "); printGiantHex(*c);
    printf("   d   : "); printGiantHex(*d);
#endif
    return FR_Success;
}

/*
 * Encode/decode the FEE private and public keys. We malloc returned data via
 * falloc(); caller must free via ffree(). Public C functions which never throw. 
 */
feeReturn feeDEREncodePublicKey(
	int					version,
	const curveParams	*cp,
	giant				plusX,
	giant				minusX,
	giant				plusY,				// may be NULL
	unsigned char		**keyBlob,			// fmallocd and RETURNED
	unsigned			*keyBlobLen)		// RETURNED
{
	FEEPublicKeyASN1 asnKey;
	SecNssCoder coder;
	
	memset(&asnKey, 0, sizeof(asnKey));
	intToCssmData(version, asnKey.version, coder);
	
	try {
		feeCurveParamsToASN1(cp, asnKey.curveParams, coder);
		giantToCssmData(plusX, asnKey.plusX, coder);
		giantToCssmData(minusX, asnKey.minusX, coder);
		if(plusY != NULL) {
			giantToCssmData(plusY, asnKey.plusY, coder);
		}
	}
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	
	/* DER encode */
	PRErrorCode perr;
	CSSM_DATA encBlob;			// mallocd by coder
	perr = coder.encodeItem(&asnKey, FEEPublicKeyASN1Template, encBlob);
	if(perr) {
		return FR_Memory;
	}

	/* copy out */
	*keyBlob = (unsigned char *)fmalloc((unsigned)encBlob.Length);
	*keyBlobLen = (unsigned)encBlob.Length;
	memmove(*keyBlob, encBlob.Data, encBlob.Length); 
	return FR_Success;
}

feeReturn feeDEREncodePrivateKey(
	int					version,
	const curveParams	*cp,
	const giant			privData,
	unsigned char		**keyBlob,			// fmallocd and RETURNED
	unsigned			*keyBlobLen)		// RETURNED
{
	FEEPrivateKeyASN1 asnKey;
	SecNssCoder coder;
	
	memset(&asnKey, 0, sizeof(asnKey));
	intToCssmData(version, asnKey.version, coder);
	
	try {
		feeCurveParamsToASN1(cp, asnKey.curveParams, coder);
		giantToCssmData(privData, asnKey.privData, coder);
	}
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	
	/* DER encode */
	PRErrorCode perr;
	CSSM_DATA encBlob;			// mallocd by coder
	perr = coder.encodeItem(&asnKey, FEEPrivateKeyASN1Template, encBlob);
	if(perr) {
		return FR_Memory;
	}

	/* copy out */
	*keyBlob = (unsigned char *)fmalloc((unsigned)encBlob.Length);
	*keyBlobLen = (unsigned)encBlob.Length;
	memmove(*keyBlob, encBlob.Data, encBlob.Length); 
	return FR_Success;
}

feeReturn feeDERDecodePublicKey(
	const unsigned char	*keyBlob,
	unsigned			keyBlobLen,
	int					*version,			// this and remainder RETURNED
	curveParams			**cp,
	giant				*plusX,
	giant				*minusX,
	giant				*plusY)				// may be NULL
{
	FEEPublicKeyASN1 asnKey;
	SecNssCoder coder;
	
	memset(&asnKey, 0, sizeof(asnKey));
	PRErrorCode perr = coder.decode(keyBlob, keyBlobLen, 
		FEEPublicKeyASN1Template, &asnKey);
	if(perr) {
		return FR_BadKeyBlob;
	}

	try {
		*version = cssmDataToInt(asnKey.version);
		*cp     = feeCurveParamsFromAsn1(asnKey.curveParams);
		*plusX  = cssmDataToGiant(asnKey.plusX);
		*minusX = cssmDataToGiant(asnKey.minusX);
		if(asnKey.plusY.Data != NULL) {
			/* optional */
			*plusY = cssmDataToGiant(asnKey.plusY);
		}
		else {
			*plusY = newGiant(1);
			int_to_giant(0, *plusY);
		}
	}
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_Memory;
	}
	return FR_Success;
}
	
feeReturn feeDERDecodePrivateKey(
	const unsigned char	*keyBlob,
	unsigned			keyBlobLen,
	int					*version,			// this and remainder RETURNED
	curveParams			**cp,
	giant				*privData)			// RETURNED
{
	FEEPrivateKeyASN1 asnKey;
	SecNssCoder coder;
	
	memset(&asnKey, 0, sizeof(asnKey));
	PRErrorCode perr = coder.decode(keyBlob, keyBlobLen, 
		FEEPrivateKeyASN1Template, &asnKey);
	if(perr) {
		return FR_BadKeyBlob;
	}

	try {
		*version = cssmDataToInt(asnKey.version);
		*cp     = feeCurveParamsFromAsn1(asnKey.curveParams);
		*privData  = cssmDataToGiant(asnKey.privData);
	}
	catch(const feeException &ferr) {
		return ferr.frtn();
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_Memory;
	}
	return FR_Success;
}

#pragma mark --- ECDSA support ---

/* convert between feeDepth and curve OIDs */
static const CSSM_OID *depthToOid(
	feeDepth depth)
{
	switch(depth) {
		case FEE_DEPTH_secp192r1:
			return &CSSMOID_secp192r1;
		case FEE_DEPTH_secp256r1:
			return &CSSMOID_secp256r1;
		case FEE_DEPTH_secp384r1:
			return &CSSMOID_secp384r1;
		case FEE_DEPTH_secp521r1:
			return &CSSMOID_secp521r1;
		default:
			dbgLog(("depthToOid needs work\n"));
			return NULL;
	}
}

static feeReturn curveOidToFeeDepth(
	const CSSM_OID *curveOid, 
	feeDepth *depth)			/* RETURNED */
{
	if(nssCompareCssmData(curveOid, &CSSMOID_secp192r1)) {
		*depth = FEE_DEPTH_secp192r1;
	}
	else if(nssCompareCssmData(curveOid, &CSSMOID_secp256r1)) {
		*depth = FEE_DEPTH_secp256r1;
	}
	else if(nssCompareCssmData(curveOid, &CSSMOID_secp384r1)) {
		*depth = FEE_DEPTH_secp384r1;
	}
	else if(nssCompareCssmData(curveOid, &CSSMOID_secp521r1)) {
		*depth = FEE_DEPTH_secp521r1;
	}
	else {
		dbgLog(("curveOidToFeeDepth: unknown curve OID\n"));
		return FR_BadKeyBlob;
	}
	return FR_Success;
}


/* 
 * Validate a decoded CSSM_X509_ALGORITHM_IDENTIFIER and infer
 * depth from its algorith.parameter
 */
static feeReturn feeAlgIdToDepth(
	const CSSM_X509_ALGORITHM_IDENTIFIER *algId,
	feeDepth *depth)
{
	const CSSM_OID *oid = &algId->algorithm;
	/* FIXME what's the value here for a private key!? */
	if(!nssCompareCssmData(oid, &CSSMOID_ecPublicKey)) {
		dbgLog(("feeAlgIdToDepth: bad OID"));
		return FR_BadKeyBlob;
	}
	
	/* 
	 * AlgId.params is curve OID, still encoded since it's an ASN_ANY.
	 * First two bytes of encoded OID are (06, length) 
	 */
	const CSSM_DATA *param = &algId->parameters;
	if((param->Length <= 2) || (param->Data[0] != BER_TAG_OID)) {
		dbgLog(("feeAlgIdToDepth: no curve params\n"));
		return FR_BadKeyBlob;
	}
	
	CSSM_OID decOid = {param->Length-2, algId->parameters.Data+2};
	return curveOidToFeeDepth(&decOid, depth);
}

/*
 * Prepare an CSSM_X509_ALGORITHM_IDENTIFIER for encoding.
 */
static feeReturn feeSetupAlgId(
	feeDepth depth,
	SecNssCoder &coder,
	CSSM_X509_ALGORITHM_IDENTIFIER &algId)
{
	algId.algorithm = CSSMOID_ecPublicKey;
	const CSSM_OID *curveOid = depthToOid(depth);
	if(curveOid == NULL) {
		return FR_IllegalDepth;
	}
	
	/* quick & dirty encode of the parameter OID; it's an ASN_ANY in the template */
	coder.allocItem(algId.parameters, curveOid->Length + 2);
	algId.parameters.Data[0] = BER_TAG_OID;
	algId.parameters.Data[1] = curveOid->Length;
	memmove(algId.parameters.Data+2, curveOid->Data, curveOid->Length);
	return FR_Success;
}

#pragma mark --- ECDSA public key, X.509 format ---

/* 
 * Encode/decode public key in X.509 format.
 */
feeReturn feeDEREncodeX509PublicKey(
	const unsigned char	*pubBlob,		/* x and y octet string */
	unsigned			pubBlobLen,
	curveParams			*cp,
	unsigned char		**x509Blob,		/* fmallocd and RETURNED */
	unsigned			*x509BlobLen)	/* RETURNED */
{
	SecNssCoder coder;
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO nssPubKeyInfo;
	
	memset(&nssPubKeyInfo, 0, sizeof(nssPubKeyInfo));
	
	/* The x/y string, to be encoded in a bit string */
	nssPubKeyInfo.subjectPublicKey.Data = (uint8 *)pubBlob;
	nssPubKeyInfo.subjectPublicKey.Length = pubBlobLen * 8;
	
	feeDepth depth;
	feeReturn frtn = curveParamsDepth(cp, &depth);
	if(frtn) {
		dbgLog(("feeDEREncodePKCS8PrivateKey: curveParamsDepth error\n"));
		return frtn;
	}

	CSSM_X509_ALGORITHM_IDENTIFIER &algId = nssPubKeyInfo.algorithm;
	frtn = feeSetupAlgId(depth, coder, algId);
	if(frtn) {
		return frtn;
	}
	
	/* DER encode */
	CSSM_DATA encBlob;			// mallocd by coder
	PRErrorCode perr = coder.encodeItem(&nssPubKeyInfo, kSecAsn1SubjectPublicKeyInfoTemplate, encBlob);
	if(perr) {
		return FR_Memory;
	}

	/* copy out */
	*x509Blob = (unsigned char *)fmalloc((unsigned)encBlob.Length);
	*x509BlobLen = (unsigned)encBlob.Length;
	memmove(*x509Blob, encBlob.Data, encBlob.Length); 
	return FR_Success;
}

feeReturn feeDERDecodeX509PublicKey(
	const unsigned char	*x509Blob,
	unsigned			x509BlobLen,
	feeDepth			*depth,			/* RETURNED */
	unsigned char		**pubBlob,		/* x and y octet string RETURNED */
	unsigned			*pubBlobLen)	/* RETURNED */
{
	SecNssCoder coder;
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO nssPubKeyInfo;
	PRErrorCode perr;
	
	memset(&nssPubKeyInfo, 0, sizeof(nssPubKeyInfo));
	perr = coder.decode(x509Blob, x509BlobLen, kSecAsn1SubjectPublicKeyInfoTemplate, 
		&nssPubKeyInfo);
	if(perr) {
		dbgLog(("decode(SubjectPublicKeyInfo) error"));
		return FR_BadKeyBlob;
	}

	/* verify alg identifier & depth */
	feeReturn frtn = feeAlgIdToDepth(&nssPubKeyInfo.algorithm, depth);
	if(frtn) {
		return frtn;
	}
	
	/* copy public key string - it's in bits here */
	CSSM_DATA *pubKey = &nssPubKeyInfo.subjectPublicKey;
	unsigned keyLen =(unsigned) (pubKey->Length + 7) / 8;
	*pubBlob = (unsigned char *)fmalloc(keyLen);
	if(*pubBlob == NULL) {
		return FR_Memory;
	}
	memmove(*pubBlob, pubKey->Data, keyLen);
	*pubBlobLen = keyLen;
	return FR_Success;
}

#pragma mark --- ECDSA keys, OpenSSL format ---

/* 
 * Encode private, and decode private or public key, in unencrypted OpenSSL format.
 */
feeReturn feeDEREncodeOpenSSLPrivateKey(
	const unsigned char	*privBlob,		/* private data octet string */
	unsigned			privBlobLen,
	const unsigned char *pubBlob,		/* public key, optional */
	unsigned			pubBlobLen,
	curveParams			*cp,
	unsigned char		**openBlob,		/* fmallocd and RETURNED */
	unsigned			*openBlobLen)	/* RETURNED */
{
	feeDepth depth;
	const CSSM_OID *curveOid;
	SecNssCoder coder;
	
	NSS_ECDSA_PrivateKey ecdsaPrivKey;
	memset(&ecdsaPrivKey, 0, sizeof(ecdsaPrivKey));
	uint8 vers = 1;
	ecdsaPrivKey.version.Data = &vers;
	ecdsaPrivKey.version.Length = 1;
	ecdsaPrivKey.privateKey.Data = (uint8 *)privBlob;
	ecdsaPrivKey.privateKey.Length = privBlobLen;
	
	/* Params - ASN_ANY - actually the curve OID */
	if(curveParamsDepth(cp, &depth)) {
		dbgLog(("feeDEREncodeOpenSSLPrivateKey: bad depth"));
		return FR_BadKeyBlob;
	}
	curveOid = depthToOid(depth);
	if(curveOid == NULL) {
		return FR_BadKeyBlob;
	}
	
	/* quickie DER-encode of the curve OID */
	try {
		coder.allocItem(ecdsaPrivKey.params, curveOid->Length + 2);
	}
	catch(...) {
		return FR_Memory;
	}
	ecdsaPrivKey.params.Data[0] = BER_TAG_OID;
	ecdsaPrivKey.params.Data[1] = curveOid->Length;
	memmove(ecdsaPrivKey.params.Data+2, curveOid->Data, curveOid->Length);
	
	/* public key - optional - bit string, length in bits */
	if(pubBlob) {
		ecdsaPrivKey.pubKey.Data = (uint8 *)pubBlob;
		ecdsaPrivKey.pubKey.Length = pubBlobLen * 8;
	}
	
	CSSM_DATA encPriv = {0, NULL};
	PRErrorCode perr = coder.encodeItem(&ecdsaPrivKey, kSecAsn1ECDSAPrivateKeyInfoTemplate, encPriv);
	if(perr) {
		return FR_Memory;
	}

	/* copy out */
	*openBlob = (unsigned char *)fmalloc((unsigned)encPriv.Length);
	*openBlobLen = (unsigned)encPriv.Length;
	memmove(*openBlob, encPriv.Data, encPriv.Length); 
	return FR_Success;
}

feeReturn feeDERDecodeOpenSSLKey(
	const unsigned char	*osBlob,
	unsigned			osBlobLen,
	feeDepth			*depth,			/* RETURNED */
	unsigned char		**privBlob,		/* private data octet string RETURNED */
	unsigned			*privBlobLen,	/* RETURNED */
	unsigned char		**pubBlob,		/* public data octet string optionally RETURNED */
	unsigned			*pubBlobLen)
{
	SecNssCoder coder;
	NSS_ECDSA_PrivateKey ecdsaPrivKey;
	memset(&ecdsaPrivKey, 0, sizeof(ecdsaPrivKey));
	if(coder.decode(osBlob, osBlobLen,
			kSecAsn1ECDSAPrivateKeyInfoTemplate, &ecdsaPrivKey)) {
		dbgLog(("Error decoding openssl priv key\n"));
		return FR_BadKeyBlob;
	}
	
	unsigned keyLen = (unsigned)ecdsaPrivKey.privateKey.Length;
	if(keyLen == 0) {
		dbgLog(("NULL priv key data in PKCS8\n"));
	}
	*privBlob = (unsigned char *)fmalloc(keyLen);
	if(*privBlob == NULL) {
		return FR_Memory;
	}
	*privBlobLen = keyLen;
	memmove(*privBlob, ecdsaPrivKey.privateKey.Data, keyLen);
	
	/* curve OID --> depth */
	if(ecdsaPrivKey.params.Data != NULL) {
		/* quickie decode */
		const CSSM_DATA *param = &ecdsaPrivKey.params;
		if((param->Data[0] != BER_TAG_OID) || (param->Length <= 2)) {
			dbgLog(("feeDERDecodeOpenSSLKey: bad curve params\n"));
			return FR_BadKeyBlob;
		}
		CSSM_OID decOid = {param->Length-2, param->Data+2};
		if(curveOidToFeeDepth(&decOid, depth)) {
			return FR_BadKeyBlob;
		}
	}

	/* Public key, if it's there and caller wants it */
	if((ecdsaPrivKey.pubKey.Length != 0) && (pubBlob != NULL)) {
		*pubBlobLen = (unsigned)(ecdsaPrivKey.pubKey.Length + 7) / 8;
		*pubBlob = (unsigned char *)fmalloc(*pubBlobLen);
		memmove(*pubBlob, ecdsaPrivKey.pubKey.Data, *pubBlobLen);
	}
	return FR_Success;
}

#pragma mark --- ECDSA public key, PKCS8 format ---

/* 
 * Encode/decode private key in unencrypted PKCS8 format.
 */
feeReturn feeDEREncodePKCS8PrivateKey(
	const unsigned char	*privBlob,		/* private data octet string */
	unsigned			privBlobLen,
	const unsigned char	*pubBlob,		/* public blob, optional */
	unsigned			pubBlobLen,
	curveParams			*cp,
	unsigned char		**pkcs8Blob,	/* fmallocd and RETURNED */
	unsigned			*pkcs8BlobLen)	/* RETURNED */
{
	/* First encode a NSS_ECDSA_PrivateKey */
	unsigned char *encPriv = NULL;
	unsigned encPrivLen = 0;
	feeReturn frtn = feeDEREncodeOpenSSLPrivateKey(privBlob, privBlobLen,
		pubBlob, pubBlobLen, cp, &encPriv, &encPrivLen);
	if(frtn) {
		return frtn;
	}
	
	/* That encoding goes into NSS_PrivateKeyInfo.private key */
	SecNssCoder coder;
	NSS_PrivateKeyInfo nssPrivKeyInfo;
	CSSM_X509_ALGORITHM_IDENTIFIER &algId = nssPrivKeyInfo.algorithm;
	memset(&nssPrivKeyInfo, 0, sizeof(nssPrivKeyInfo));
	nssPrivKeyInfo.privateKey.Data = (uint8 *)encPriv;
	nssPrivKeyInfo.privateKey.Length = encPrivLen;
	uint8 vers = 0;
	
	feeDepth depth;
	frtn = curveParamsDepth(cp, &depth);
	if(frtn) {
		dbgLog(("feeDEREncodePKCS8PrivateKey: curveParamsDepth error\n"));
		goto errOut;
	}
	frtn = feeSetupAlgId(depth, coder, algId);
	if(frtn) {
		goto errOut;
	}
	
	nssPrivKeyInfo.version.Data = &vers;
	nssPrivKeyInfo.version.Length = 1;
	
	/* DER encode */
	CSSM_DATA encPrivInfo;			// mallocd by coder
	if(coder.encodeItem(&nssPrivKeyInfo, kSecAsn1PrivateKeyInfoTemplate, encPrivInfo)) {
		frtn = FR_Memory;
		goto errOut;
	}

	/* copy out */
	*pkcs8Blob = (unsigned char *)fmalloc((unsigned)encPrivInfo.Length);
	*pkcs8BlobLen = (unsigned)encPrivInfo.Length;
	memmove(*pkcs8Blob, encPrivInfo.Data, encPrivInfo.Length); 
errOut:
	if(encPriv) {
		ffree(encPriv);
	}
	return frtn;
}

feeReturn feeDERDecodePKCS8PrivateKey(
	const unsigned char	*pkcs8Blob,
	unsigned			pkcs8BlobLen,
	feeDepth			*depth,			/* RETURNED */
	unsigned char		**privBlob,		/* private data octet string RETURNED */
	unsigned			*privBlobLen,	/* RETURNED */
	unsigned char		**pubBlob,		/* optionally returned, if it's there */
	unsigned			*pubBlobLen)
{
	NSS_PrivateKeyInfo nssPrivKeyInfo;
	PRErrorCode perr;
	SecNssCoder coder;
	
	memset(&nssPrivKeyInfo, 0, sizeof(nssPrivKeyInfo));
	perr = coder.decode(pkcs8Blob, pkcs8BlobLen, kSecAsn1PrivateKeyInfoTemplate, &nssPrivKeyInfo);
	if(perr) {
		dbgLog(("Error decoding top level PKCS8\n"));
		return FR_BadKeyBlob;
	}
	
	/* verify alg identifier & depth */
	feeReturn frtn = feeAlgIdToDepth(&nssPrivKeyInfo.algorithm, depth);
	if(frtn) {
		return frtn;
	}
	
	/* 
	 * nssPrivKeyInfo.privateKey is an octet string containing an encoded 
	 * NSS_ECDSA_PrivateKey. 
	 */
	frtn = feeDERDecodeOpenSSLKey((const unsigned char *)nssPrivKeyInfo.privateKey.Data,
		(unsigned)nssPrivKeyInfo.privateKey.Length, depth, 
		privBlob, privBlobLen,
		pubBlob, pubBlobLen);
		
	return frtn;
}

#endif	/* CRYPTKIT_DER_ENABLE */
