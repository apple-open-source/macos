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

#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>
#include <CryptKit/CryptKitDER.h>
#include <CryptKit/falloc.h>
#include <CryptKit/feeDebug.h>
#include <CryptKit/feeFunctions.h>
#include <Security/cdsaUtils.h>
#include <Security/appleoids.h>

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
	~feeException() 				{ }
	feeReturn frtn() 				{ return mFrtn; }
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

void feeException::throwMe(feeReturn frtn, const char *op = NULL) { throw feeException(frtn, op); }

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

static unsigned feeSizeofSnaccInt()
{
	return 7;
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

static unsigned feeSizeofSnaccCurveParams(const curveParams *cp)
{
	unsigned rtn = 5 * feeSizeofSnaccInt();	// primeType, curveType, q, k, m
	rtn += 10 * feeSizeOfSnaccGiant(cp->basePrime);
	szprint(("feeSizeofSnaccCurveParams: size %d\n", rtn));
	return rtn;
}

static unsigned feeSizeOfSnaccPubKey(const curveParams *cp)
{
	unsigned rtn = 11;						// version plus sequence overhead
	rtn += feeSizeofSnaccCurveParams(cp);
	rtn += (3 * feeSizeOfSnaccGiant(cp->basePrime));
	szprint(("feeSizeOfSnaccPubKey: size %d\n", rtn));
	return rtn;
}

static unsigned feeSizeOfSnaccPrivKey(const curveParams *cp)
{
	unsigned rtn = 11;						// version plus sequence overhead
	rtn += feeSizeofSnaccCurveParams(cp);
	rtn += feeSizeOfSnaccGiant(cp->basePrime);
	szprint(("feeSizeOfSnaccPrivKey: size %d\n", rtn));
	return rtn;
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
 * Convert a BigIntegerStr to a (mallocd) giant. 
 * Only known exception is a feeException.
 */
static giant bigIntStrToGiant(
	BigIntegerStr 	&bigInt)
{
	char *rawOcts = bigInt;
	unsigned numBytes = bigInt.Len();
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
 * Convert a giant to an existing BigIntegerString.
 * Only known exception is a feeException.
 */
static void giantToBigIntStr(
	giant 			g,
	BigIntegerStr 	&bigInt)
{
	unsigned char doPrepend = 0;	
	unsigned numGiantDigits = abs(g->sign);
	unsigned numBytes = numGiantDigits * GIANT_BYTES_PER_DIGIT;
	giantDigit msGiantBit = 0;
	if(isZero(g)) {
		/* special degenerate case */
		bigInt.ReSet("", 1);
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

	/* rawBytes are the ASN-compliant contents */
	bigInt.ReSet(reinterpret_cast<const char *>(outp), numBytes);
	ffree(rawBytes);
}

/* curveParams : CryptKit <--> snacc */
/* Only known exception is a feeException */
static FEECurveParameters *feeCurveParamsToSnacc(
	const curveParams *cp)
{
	#if 	PRINT_CURVE_PARAMS
	printf("===encoding curveParams; cp:\n"); printCurveParams(cp);
	#endif
	FEECurveParameters *snaccCp = NULL;
	try {
		snaccCp = new FEECurveParameters();
		AsnIntType val;
		switch(cp->primeType) {
			case FPT_Mersenne:
				val = FEEPrimeType::pt_mersenne;
				break;
			case FPT_FEE:
				val = FEEPrimeType::pt_fee;
				break;
			case FPT_General:
				val = FEEPrimeType::pt_general;
				break;
			default:
				feeException::throwMe(FR_Internal, "bad cp->primeType");
		}
		snaccCp->primeType.Set(val);
		switch(cp->curveType) {
			case FCT_Montgomery:
				val = FEECurveType::ct_montgomery;
				break;
			case FCT_Weierstrass:
				val = FEECurveType::ct_weierstrass;
				break;
			case FCT_General:
				val = FEECurveType::ct_general;
				break;
			default:
				feeException::throwMe(FR_Internal, "bad cp->curveType");
		}
		snaccCp->curveType.Set(val);
		snaccCp->q.Set(cp->q);
		snaccCp->k.Set(cp->k);
		snaccCp->m.Set(cp->m);
		giantToBigIntStr(cp->a, snaccCp->a);
		giantToBigIntStr(cp->b, snaccCp->bb);
		giantToBigIntStr(cp->c, snaccCp->c);
		giantToBigIntStr(cp->x1Plus, snaccCp->x1Plus);
		giantToBigIntStr(cp->x1Minus, snaccCp->x1Minus);
		giantToBigIntStr(cp->cOrderPlus, snaccCp->cOrderPlus);
		giantToBigIntStr(cp->cOrderMinus, snaccCp->cOrderMinus);
		giantToBigIntStr(cp->x1OrderPlus, snaccCp->x1OrderPlus);
		giantToBigIntStr(cp->x1OrderMinus, snaccCp->x1OrderMinus);
		if(cp->primeType == FPT_General) {
			snaccCp->basePrime = new BigIntegerStr();
			giantToBigIntStr(cp->basePrime, *snaccCp->basePrime);
		}
	}
	catch(feeException ferr) {
		delete snaccCp;
		throw;
	}
	catch(...) {
		delete snaccCp;
		feeException::throwMe(FR_Memory, "feeCurveParamsToSnacc catchall");	// ???
	}
	return snaccCp;
}

static curveParams *feeCurveParamsFromSnacc(
	FEECurveParameters	&snaccCp)
{
	curveParams *cp = newCurveParams();
	if(cp == NULL) {
		feeException::throwMe(FR_Memory, "feeCurveParamsFromSnacc alloc cp");
	}
	AsnIntType val = snaccCp.primeType;
	switch(val) {
		case FEEPrimeType::pt_mersenne:
			cp->primeType = FPT_Mersenne;
			break;
		case FEEPrimeType::pt_fee:
			cp->primeType = FPT_FEE;
			break;
		case FEEPrimeType::pt_general:
			cp->primeType = FPT_General;
			break;
		default:
			feeException::throwMe(FR_BadPubKey, "feeCurveParamsFromSnacc bad primeType");
	}
	val = snaccCp.curveType;
	switch(val) {
		case FEECurveType::ct_montgomery:
			cp->curveType = FCT_Montgomery;
			break;
		case FEECurveType::ct_weierstrass:
			cp->curveType = FCT_Weierstrass;
			break;
		case FEECurveType::ct_general:
			cp->curveType = FCT_General;
			break;
		default:
			feeException::throwMe(FR_BadPubKey, "feeCurveParamsFromSnacc bad curveType");
	}
	cp->q 			   = snaccCp.q;
	cp->k 			   = snaccCp.k;
	cp->m 			   = snaccCp.m;
	cp->a 			   = bigIntStrToGiant(snaccCp.a);
	cp->b 			   = bigIntStrToGiant(snaccCp.bb);
	cp->c              = bigIntStrToGiant(snaccCp.c);
	cp->x1Plus         = bigIntStrToGiant(snaccCp.x1Plus);
	cp->x1Minus        = bigIntStrToGiant(snaccCp.x1Minus);
	cp->cOrderPlus     = bigIntStrToGiant(snaccCp.cOrderPlus);
	cp->cOrderMinus    = bigIntStrToGiant(snaccCp.cOrderMinus);
	cp->x1OrderPlus    = bigIntStrToGiant(snaccCp.x1OrderPlus);
	cp->x1OrderMinus   = bigIntStrToGiant(snaccCp.x1OrderMinus);
	if(snaccCp.basePrime != NULL) {
		cp->basePrime  = bigIntStrToGiant(*snaccCp.basePrime);
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
	FEEElGamalSignature snaccSig;
	CssmAutoData oData(CssmAllocator::standard(CssmAllocator::sensitive));
	
	try {
		giantToBigIntStr(u, snaccSig.u);
		giantToBigIntStr(PmX, snaccSig.pmX);
	} 
	catch(feeException ferr) {
		return ferr.frtn();
	}
	try {
		SC_encodeAsnObj(snaccSig, oData, feeSizeOfDERSig(u, PmX));
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_BadSignatureFormat;
	}
	*encodedSig = (unsigned char *)fmalloc(oData.length());
	*encodedSigLen = oData.length();
	memmove(*encodedSig, oData.get().Data, oData.length()); 
	#if	PRINT_SIG_GIANTS
	printf("feeEncodeElGamalSignature:\n");
	printf("   u   : "); printGiantHex(u);
	printf("   PmX : "); printGiantHex(PmX);
	printf("   u   : "); snaccSig.u.Print(cout); printf("\n");
	printf("   PmX : "); snaccSig.pmX.Print(cout); printf("\n");
	#endif
	return FR_Success;
}

feeReturn feeDEREncodeECDSASignature(
	giant			c,
	giant			d,
	unsigned char	**encodedSig,		// fmallocd and RETURNED
	unsigned		*encodedSigLen)		// RETURNED
{
	FEEECDSASignature snaccSig;
	CssmAutoData oData(CssmAllocator::standard(CssmAllocator::sensitive));
	
	try {
		giantToBigIntStr(c, snaccSig.c);
		giantToBigIntStr(d, snaccSig.d);
	}
	catch(feeException ferr) {
		return ferr.frtn();
	}
	try {
		SC_encodeAsnObj(snaccSig, oData, feeSizeOfDERSig(c, d));
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_BadSignatureFormat;
	}
	*encodedSig = (unsigned char *)fmalloc(oData.length());
	*encodedSigLen = oData.length();
	memmove(*encodedSig, oData.get().Data, oData.length()); 
	#if	PRINT_SIG_GIANTS
	printf("feeEncodeECDSASignature:\n");
	printf("   c   : "); printGiantHex(*c);
	printf("   d   : "); printGiantHex(*d);
	printf("   c   : "); snaccSig.c.Print(cout); printf("\n");
	printf("   d   : "); snaccSig.d.Print(cout); printf("\n");
	#endif
	return FR_Success;
}

feeReturn feeDERDecodeElGamalSignature(
	const unsigned char	*encodedSig,
	unsigned			encodedSigLen,
	giant				*u,				// newGiant'd and RETURNED
	giant				*PmX)			// newGiant'd and RETURNED
{
	FEEElGamalSignature snaccSig;
	CssmData cData((void *)encodedSig, encodedSigLen);
	try {
		SC_decodeAsnObj(cData, snaccSig);
	}
	catch(...) {
		return FR_BadSignatureFormat;
	}
	try {
		*u   = bigIntStrToGiant(snaccSig.u);
		*PmX = bigIntStrToGiant(snaccSig.pmX);
	}
	catch(feeException ferr) {
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
	printf("   u   : "); snaccSig.u.Print(cout); printf("\n");
	printf("   PmX : "); snaccSig.pmX.Print(cout); printf("\n");
	#endif
	return FR_Success;
}

feeReturn feeDERDecodeECDSASignature(
	const unsigned char	*encodedSig,
	unsigned			encodedSigLen,
	giant				*c,				// newGiant'd and RETURNED
	giant				*d)				// newGiant'd and RETURNED
{
	FEEECDSASignature snaccSig;
	CssmData cData((void *)encodedSig, encodedSigLen);
	try {
		SC_decodeAsnObj(cData, snaccSig);
	}
	catch(...) {
		return FR_BadSignatureFormat;
	}
	try {
		*c = bigIntStrToGiant(snaccSig.c);
		*d = bigIntStrToGiant(snaccSig.d);
	}
	catch(feeException ferr) {
		return ferr.frtn();
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_Memory;
	}
	#if	PRINT_SIG_GIANTS
	printf("feeDecodeECDSASignature:\n");
	printf("   c   : "); printGiantHex(*c);
	printf("   d   : "); printGiantHex(*d);
	printf("   c   : "); snaccSig.c.Print(cout); printf("\n");
	printf("   d   : "); snaccSig.d.Print(cout); printf("\n");
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
	FEEPublicKey snaccKey;
	
	/* set up the SNACC object */
	snaccKey.version.Set(version);
	try {
		snaccKey.curveParams = feeCurveParamsToSnacc(cp);
		giantToBigIntStr(plusX, snaccKey.plusX);
		giantToBigIntStr(minusX, snaccKey.minusX);
		if(plusY != NULL) {
			snaccKey.plusY = new BigIntegerStr();
			giantToBigIntStr(plusY, *snaccKey.plusY);
		}
	}
	catch(feeException ferr) {
		return ferr.frtn();
	}
	
	/* encode the SNACC object */
	CssmAutoData oData(CssmAllocator::standard(CssmAllocator::sensitive));
	
	try {
		SC_encodeAsnObj(snaccKey, oData, feeSizeOfSnaccPubKey(cp));
	}
	catch(...) {
		/* FIXME - ???? */
		return FR_Memory;
	}
	*keyBlob = (unsigned char *)fmalloc(oData.length());
	*keyBlobLen = oData.length();
	memmove(*keyBlob, oData.get().Data, oData.length()); 
	return FR_Success;
}

feeReturn feeDEREncodePrivateKey(
	int					version,
	const curveParams	*cp,
	const giant			privData,
	unsigned char		**keyBlob,			// fmallocd and RETURNED
	unsigned			*keyBlobLen)		// RETURNED
{
	FEEPrivateKey snaccKey;
	
	/* set up the SNACC object */
	snaccKey.version.Set(version);
	try {
		snaccKey.curveParams = feeCurveParamsToSnacc(cp);
		giantToBigIntStr(privData, snaccKey.privData);
	}
	catch(feeException ferr) {
		return ferr.frtn();
	}
	
	/* encode the SNACC object */
	CssmAutoData oData(CssmAllocator::standard(CssmAllocator::sensitive));
	
	try {
		SC_encodeAsnObj(snaccKey, oData, feeSizeOfSnaccPrivKey(cp));
	}
	catch(...) {
		/* FIXME - ???? */
		return FR_Memory;
	}
	*keyBlob = (unsigned char *)fmalloc(oData.length());
	*keyBlobLen = oData.length();
	memmove(*keyBlob, oData.get().Data, oData.length()); 
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
	FEEPublicKey snaccKey;
	CssmData cData((unsigned char *)keyBlob, (size_t)keyBlobLen);
	try {
		SC_decodeAsnObj(cData, snaccKey);
	}
	catch(...) {
		return FR_BadPubKey;
	}
	try {
		*version = snaccKey.version;
		*cp     = feeCurveParamsFromSnacc(*snaccKey.curveParams);
		*plusX  = bigIntStrToGiant(snaccKey.plusX);
		*minusX = bigIntStrToGiant(snaccKey.minusX);
		if(snaccKey.plusY != NULL) {
			/* optional */
			*plusY = bigIntStrToGiant(*snaccKey.plusY);
		}
		else {
			*plusY = newGiant(1);
			int_to_giant(0, *plusY);
		}
	}
	catch(feeException ferr) {
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
	FEEPrivateKey snaccKey;
	CssmData cData((unsigned char *)keyBlob, (size_t)keyBlobLen);
	try {
		SC_decodeAsnObj(cData, snaccKey);
	}
	catch(...) {
		return FR_BadPubKey;
	}
	try {
		*version  = snaccKey.version;
		*cp       = feeCurveParamsFromSnacc(*snaccKey.curveParams);
		*privData = bigIntStrToGiant(snaccKey.privData);
	}
	catch(feeException ferr) {
		return ferr.frtn();
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return FR_Memory;
	}
	return FR_Success;
}

#endif	/* CRYPTKIT_CSP_ENABLE */
