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
 * CryptKitDER.h - snacc-based routines to create and parse DER-encoded FEE 
 *				   keys and signatures
 *
 * Created 3/12/2001 by dmitch.
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#include <CryptKit/CryptKitDER.h>
#include <CryptKit/falloc.h>
#include <CryptKit/feeDebug.h>
#include <CryptKit/feeFunctions.h>
#include "CryptKitAsn1.h"
#include <SecurityNssAsn1/SecNssCoder.h>


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
	unsigned numBytes = cdata.Length;
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
		feeException::throwMe(FR_Memory, "giantToBigIntStr fmalloc(rawBytes)");
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
	*encodedSig = (unsigned char *)fmalloc(encBlob.Length);
	*encodedSigLen = encBlob.Length;
	memmove(*encodedSig, encBlob.Data, encBlob.Length); 
	
	#if	PRINT_SIG_GIANTS
	printf("feeEncodeElGamalSignature:\n");
	printf("   u   : "); printGiantHex(u);
	printf("   PmX : "); printGiantHex(PmX);
	#endif
	
	return FR_Success;
}

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
	*encodedSig = (unsigned char *)fmalloc(encBlob.Length);
	*encodedSigLen = encBlob.Length;
	memmove(*encodedSig, encBlob.Data, encBlob.Length); 
	
	#if	PRINT_SIG_GIANTS
	printf("feeEncodeECDSASignature:\n");
	printf("   c   : "); printGiantHex(*c);
	printf("   d   : "); printGiantHex(*d);
	#endif
	return FR_Success;

}

feeReturn feeDERDecodeElGamalSignature(
	const unsigned char	*encodedSig,
	unsigned			encodedSigLen,
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

feeReturn feeDERDecodeECDSASignature(
	const unsigned char	*encodedSig,
	unsigned			encodedSigLen,
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
	printf("   u   : "); printGiantHex(*u);
	printf("   PmX : "); printGiantHex(*PmX);
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
	*keyBlob = (unsigned char *)fmalloc(encBlob.Length);
	*keyBlobLen = encBlob.Length;
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
	*keyBlob = (unsigned char *)fmalloc(encBlob.Length);
	*keyBlobLen = encBlob.Length;
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

#endif	/* CRYPTKIT_CSP_ENABLE */
