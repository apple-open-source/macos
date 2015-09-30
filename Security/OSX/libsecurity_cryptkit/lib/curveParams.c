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
 * curveParams.c - FEE curve parameter static data and functions
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 *  9 Sep 98 at NeXT
 * 	Added y1Plus for IEEE P1363 compliance.
 *    	Added curveParamsInferFields().
 *  08 Apr 98 at Apple
 *	Mods for giantDigit.
 *  20 Jan 98 at Apple
 *	Added primeType, m, basePrimeRecip; added a few PT_GENERAL curves.
 *  19 Jan 1998 at Apple
 *	New curve: q=160, k=57
 *  09 Jan 1998 at Apple
 *	Removed obsolete (i.e., incomplete) curves parameters.
 *  11 Jun 1997 at Apple
 *	Added x1OrderPlusRecip and lesserX1OrderRecip fields
 *	Added curveParamsInitGiants()
 *  9 Jan 1997 at NeXT
 *	Major mods for IEEE-style parameters.
 *  7 Aug 1996 at NeXT
 *	Created.
 */

#include "curveParams.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "ellipticProj.h"
#include "platform.h"
#include "falloc.h"
#include "feeDebug.h"
#include <stdlib.h>

typedef unsigned short arrayDigit;

static giant arrayToGiant(const arrayDigit *array);

/*
 * Can't declare giants statically; we declare them here via static arrayDigit
 * arrays which contain the 'digits' in base 65536 of a giant
 * used as a curve parameter. First element is sign; next element is
 * l.s. digit; size of each array is abs(sign) + 1. These arrays are
 * converted to a giant via arrayToGiant().
 *
 * Static q and k values, as well as pointers to the arrayDigit arrays
 * associated with the various giants for a given curve, are kept in an
 * array of curveParamsStatic structs; a feeDepth is an index into this
 * array. A curveParamsStatic struct is converted to a curveParams struct in
 * curveParamsForDepth().
 */
typedef struct {
	feePrimeType		primeType;
	feeCurveType		curveType;
	unsigned			q;
	int         		k;
	const arrayDigit	*basePrime;		// FPT_General only
	arrayDigit			m;				// must be 1 for current release
	const arrayDigit	*a;
	const arrayDigit	*b;
	const arrayDigit	*c;
	const arrayDigit	*x1Plus;
	const arrayDigit	*y1Plus;		// optional, currently only used for ECDSA curves
	const arrayDigit	*x1Minus;		// optional, not used for ECDSA curves
	const arrayDigit	*cOrderPlus;
	const arrayDigit	*cOrderMinus;	// optional, not used for ECDSA curves
	const arrayDigit	*x1OrderPlus;
	const arrayDigit	*x1OrderMinus;	// optional, not used for ECDSA curves
	const arrayDigit	*x1OrderPlusRecip;

	/*
	 * A null lesserX1OrderRecip when x1OrderPlusRecip is non-null
	 * means that the two values are identical; in this case, only
	 * one giant is alloc'd in the actual curveParams struct.
	 */
	const arrayDigit 	*lesserX1OrderRecip;
} curveParamsStatic;

/*
 * First some common giant-arrays used in lots of curveGiants.
 */
static const arrayDigit ga_666[]  = {1, 666 };	// a common value for 'c'
static const arrayDigit ga_zero[] = {1, 0   };	// (giant)0
static const arrayDigit ga_one[]  = {1, 1   };	// (giant)1

/*
 * Here are the actual static arrays, one for each giant we know about.
 * Since they're variable size, we have to allocate and name each one
 * individually....
 */

#if		FEE_PROTOTYPE_CURVES
#include "curveParamDataOld.h"
#else
#include "curveParamData.h"
#endif

/*
 * Now the curveParamsStatic structs, which provide templates for creating the
 * fields in a specific curveParams struct.
 *
 * All giants in a curveParamsStatic struct except for basePrime are
 * guaranteed valid.
 *
 * Note these are stored as an array, an index into which is a feeDepth
 * parameter.
 */
#if		FEE_PROTOTYPE_CURVES
static curveParamsStatic curveParamsArray[] = {
    {	// depth=0
	FPT_Mersenne,
	FCT_Weierstrass,
	31, 1,			// q=31, k=1
	NULL,			// basePrime only used for FPT_General
	1,				// m = 1
    ga_w31_1_a,		// a = 7
	ga_one,			// b = 1
	ga_zero,		// c = 0
	ga_w31_1_x1Plus,
	NULL,			// y1Plus
	ga_w31_1_x1Minus,
	ga_w31_1_plusOrder,
	ga_w31_1_minusOrder,
	ga_w31_1_x1OrderPlus,
	ga_w31_1_x1OrderMinus,
	ga_w31_1_x1OrderPlusRecip,
	ga_w31_1_lesserX1OrderRecip
    },
    {	// depth=1
     	FPT_Mersenne,
	FCT_Montgomery,
   	31, 1,			// q=31, k=1
	NULL,
	1,				// m = 1
   	ga_one,			// a = 1
	ga_zero,		// b = 0
	ga_666,			// c = 666
	ga_m31_1_x1Plus,
	NULL,			// y1Plus
	ga_m31_1_x1Minus,
	ga_m31_1_plusOrder,
	ga_m31_1_minusOrder,
	ga_m31_1_x1OrderPlus,
	ga_m31_1_x1OrderMinus,
	ga_m31_1_x1OrderPlusRecip,
	ga_m31_1_lesserX1OrderRecip

   },
    {	// depth=2
    	FPT_Mersenne,
	FCT_Weierstrass,
   	31, 1,				// q=31, k=1, prime curve orders
	NULL,
	1,					// m = 1
    ga_31_1P_a,			// a = 5824692
	ga_31_1P_b,			// b = 2067311435
	ga_zero,			// c = 0
	ga_31_1P_x1Plus,
	NULL,			// y1Plus
	ga_31_1P_x1Minus,
	ga_31_1P_plusOrder,
	ga_31_1P_minusOrder,
	ga_31_1P_x1OrderPlus,
	ga_31_1P_x1OrderMinus,
	ga_31_1P_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser

   },
    {	// depth=3
    	FPT_FEE,
	FCT_Weierstrass,
   	40, 213,			// q=40, k=213, prime curve orders
	NULL,
 	1,					// m = 1
   	ga_40_213_a,		// a = 1627500953
	ga_40_213_b,		// b = 523907505
	ga_zero,			// c = 0
	ga_40_213_x1Plus,
	NULL,			// y1Plus
	ga_40_213_x1Minus,
	ga_40_213_plusOrder,
	ga_40_213_minusOrder,
	ga_40_213_x1OrderPlus,
	ga_40_213_x1OrderMinus,
	ga_40_213_x1OrderPlusRecip,
	ga_40_213_lesserX1OrderRecip

   },
   {	// depth=4
     	FPT_Mersenne,
	FCT_Montgomery,
   	127, 1,
	NULL,
	1,				// m = 1
   	ga_one,			// a = 1
	ga_zero,		// b = 0
	ga_666,			// c = 666
	ga_127_1_x1Plus,
	NULL,			// y1Plus
	ga_127_1_x1Minus,
	ga_127_1_plusOrder,
	ga_127_1_minusOrder,
	ga_127_1_x1OrderPlus,
	ga_127_1_x1OrderMinus,
	ga_127_1_x1OrderPlusRecip,
	ga_127_1_lesserX1OrderRecip

    },
    {	// depth=5
     	FPT_Mersenne,
	FCT_Weierstrass,
   	127, 1, 		// q=127, k=1 Weierstrass
	NULL,
	1,				// m = 1
    ga_666,			// a = 666
	ga_one,			// b = 1
	ga_zero,		// c = 0
	ga_127_1W_x1Plus,
	NULL,			// y1Plus
	ga_127_1W_x1Minus,
	ga_127_1W_plusOrder,
	ga_127_1W_minusOrder,
	ga_127_1W_x1OrderPlus,
	ga_127_1W_x1OrderMinus,
	ga_127_1W_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser

    },
    {	// depth=6
    FPT_FEE,
	FCT_Weierstrass,	// also Atkin3
    160, 57,
	NULL,
	1,					// m = 1
	ga_zero,			// a = 0
	ga_160_57_b,		// b = 3
	ga_zero,			// c = 0
	ga_160_57_x1Plus,
	NULL,			// y1Plus
	ga_160_57_x1Minus,
	ga_160_57_plusOrder,
	ga_160_57_minusOrder,
	ga_160_57_x1OrderPlus,
	ga_160_57_x1OrderMinus,
	ga_160_57_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser
    },
    {	// depth=7
    FPT_FEE,
	FCT_Weierstrass,	// also Atkin3
     192, 1425,
	NULL,
	1,					// m = 1
    ga_zero,			// a = 0
	ga_192_1425_b,		// b = -11
	ga_zero,			// c = 0
	ga_192_1425_x1Plus,
	NULL,			// y1Plus
	ga_192_1425_x1Minus,
	ga_192_1425_plusOrder,
	ga_192_1425_minusOrder,
	ga_192_1425_x1OrderPlus,
	ga_192_1425_x1OrderMinus,
	ga_192_1425_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser

    },
    {	// depth=8
    FPT_FEE,
	FCT_Weierstrass,
    192, -529891,
	NULL,
	1,						// m = 1
    ga_192_M529891_a,		// a = -152
	ga_192_M529891_b,		// b = 722
	ga_zero,				// c = 0
	ga_192_M529891_x1Plus,
	NULL,			// y1Plus
	ga_192_M529891_x1Minus,
	ga_192_M529891_plusOrder,
	ga_192_M529891_minusOrder,
	ga_192_M529891_x1OrderPlus,
	ga_192_M529891_x1OrderMinus,
	ga_192_M529891_x1OrderPlusRecip,
	ga_192_M529891_lesserX1OrderRecip

    },
    /*
     * FPT_General curves, currently just copies of known FPT_FEE or FPT_Mersenne
     * curves with primeType set to FPT_General. These are just for
     * verification the general curve are handled properly.
	 * We include the q parameter here for use by feeKeyBitsToDepth().
     */
    {	// depth=9
    FPT_General,
	FCT_General,
   	127, 0,
	ga_127_1_bp,	// explicit basePrime
	1,				// m = 1
   	ga_one,			// a = 1
	ga_zero,		// b = 0
	ga_666,			// c = 666
	ga_127_1_x1Plus,
	NULL,			// y1Plus
	ga_127_1_x1Minus,
	ga_127_1_plusOrder,
	ga_127_1_minusOrder,
	ga_127_1_x1OrderPlus,
	ga_127_1_x1OrderMinus,
	ga_127_1_x1OrderPlusRecip,
	ga_127_1_lesserX1OrderRecip

    },

    {	// depth=10, FPT_General version of q=160
	FPT_General,
	FCT_Weierstrass,
	160, 0,				// we don't use these...
	ga_160_57_bp,		// explicit basePrime
	1,					// m = 1
	ga_zero,			// a = 0
	ga_160_57_b,		// b = 3
	ga_zero,
	ga_160_57_x1Plus,
	NULL,			// y1Plus
	ga_160_57_x1Minus,
	ga_160_57_plusOrder,
	ga_160_57_minusOrder,
	ga_160_57_x1OrderPlus,
	ga_160_57_x1OrderMinus,
	ga_160_57_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser
    },

    {	// depth=11, FPT_General, 161 bits
	FPT_General,
	FCT_Weierstrass,
	//161, 0,
    161, 0,				// for verifying we don't use these...
	ga_161_gen_bp,		// explicit basePrime
	1,					// m = 1
	ga_161_gen_a,		// a = -152
	ga_161_gen_b,		// b = 722
	ga_zero,			// c = 0
	ga_161_gen_x1Plus,
	NULL,			// y1Plus
	ga_161_gen_x1Minus,
	ga_161_gen_plusOrder,
	ga_161_gen_minusOrder,
	ga_161_gen_x1OrderPlus,
	ga_161_gen_x1OrderMinus,
	ga_161_gen_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser
    },

};

#else	/* FEE_PROTOTYPE_CURVES */

static const curveParamsStatic curveParamsArray[] = {
{	
	/*
	 * depth = 0
	 * FEE CURVE: USE FOR FEE SIG. & FEED ONLY.
	 * primeType->Mersenne
	 * curveType->Montgomery
	 * q = 31;   k = 1;  p = 2^q - k;
	 * a = 1;   b = 0;   c = 666;
	 * Both orders composite.
	 */
	FPT_Mersenne,
	FCT_Montgomery,
	31, 1,			// q=31, k=1
	NULL,			// basePrime only used for FPT_General
	1,				// m = 1
    ga_one,			// a = 1
	ga_zero,		// b = 0
	ga_666,			// c = 666
	ga_31m_x1Plus,
	NULL,			// y1Plus
	ga_31m_x1Minus,
	ga_31m_plusOrder,
	ga_31m_minusOrder,
	ga_31m_x1OrderPlus,
	ga_31m_x1OrderMinus,
	ga_31m_x1OrderPlusRecip,
	ga_31m_lesserX1OrderRecip
},
{
	/* 
	 * depth = 1
	 * IEEE P1363 COMPATIBLE.
	 * primeType->Mersenne
	 * curveType->Weierstrass
	 * q = 31;   k = 1; p = 2^q-k;  
	 * a = 5824692    b = 2067311435   c = 0
	 * Both orders prime.
	 */
	FPT_Mersenne,
	FCT_Weierstrass,
	31, 1,			// q=31, k=1
	NULL,			// basePrime only used for FPT_General
	1,				// m = 1
    ga_31w_a,		
	ga_31w_b,		
	ga_zero,		// c = 0
	ga_31w_x1Plus,
	NULL,			// y1Plus
	ga_31w_x1Minus,
	ga_31w_plusOrder,
	ga_31w_minusOrder,
	ga_31w_x1OrderPlus,
	ga_31w_x1OrderMinus,
	ga_31w_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser
},
{
	/* 
	 * depth = 2
	 * FEE CURVE: USE FOR FEE SIG. & FEED ONLY.
	 * primeType->Mersenne
	 * curveType->Montgomery
	 * q = 127;   k = 1;  p = 2^q - k;
	 * a = 1;   b = 0;   c = 666;
	 * Both orders composite.
	 */
	FPT_Mersenne,
	FCT_Montgomery,
	127, 1,			// q = 127; k = 1
	NULL,			// basePrime only used for FPT_General
	1,				// m = 1
    ga_one,		
	ga_zero,		
	ga_666,
	ga_127m_x1Plus,
	NULL,			// y1Plus
	ga_127m_x1Minus,
	ga_127m_plusOrder,
	ga_127m_minusOrder,
	ga_127m_x1OrderPlus,
	ga_127m_x1OrderMinus,
	ga_127m_x1OrderPlusRecip,
	ga_127m_lesserX1OrderRecip
},
{
	/*
	 * depth = 3
 	 * IEEE P1363 COMPATIBLE.
	 * primeType->feemod
	 * curveType->Weierstrass
	 * q = 127;  k = -57675; p = 2^q - k;
	 * a = 170141183460469025572049133804586627403;   
	 * b = 170105154311605172483148226534443139403;    c = 0;
	 * Both orders prime.
	 */
	FPT_FEE,
	FCT_Weierstrass,
	127, -57675,	// q = 127;  k = -57675
	NULL,			// basePrime only used for FPT_General
	1,				// m = 1
    ga_128w_a,		
	ga_128w_b,		
	ga_zero,
	ga_128w_x1Plus,
	NULL,			// y1Plus
	ga_128w_x1Minus,
	ga_128w_plusOrder,
	ga_128w_minusOrder,
	ga_128w_x1OrderPlus,
	ga_128w_x1OrderMinus,
	ga_128w_x1OrderPlusRecip,
	/* REC said NULL, dmitch says: */
	ga_128w_lesserX1OrderRecip			// x1PlusOrder is lesser
},
{
	/*
	 * depth = 4
 	 * IEEE P1363 COMPATIBLE.
	 * primeType->feemod
	 * curveType->Weierstrass
	 * q = 160;  k = -5875; p = 2^q - k;
	 * a = 1461501637330902918203684832716283019448563798259;   
	 * b = 36382017816364032;    c = 0;
	 * Both orders prime.:
	 */
	FPT_FEE,
	FCT_Weierstrass,
	160, -5875,		// q = 160;  k = -5875
	NULL,			// basePrime only used for FPT_General
	1,				// m = 1
    ga_161w_a,		
	ga_161w_b,		
	ga_zero,
	ga_161w_x1Plus,
	NULL,			// y1Plus
	ga_161w_x1Minus,
	ga_161w_plusOrder,
	ga_161w_minusOrder,
	ga_161w_x1OrderPlus,
	ga_161w_x1OrderMinus,
	ga_161w_x1OrderPlusRecip,
	/* dmitch addenda - REC said NULL */ 
	ga_161w_lesserX1OrderRecip
},
{
	/*
	 * depth = 5
 	 * IEEE P1363 COMPATIBLE.
	 * primeType->General
	 * curveType->Weierstrass
	 * p is a 161-bit random prime (below, ga_161_gen_bp[]);
	 * a = -152;   b = 722;    c = 0;
	 * Both orders composite.
	 */
	FPT_General,
	FCT_Weierstrass,
	161, 0,			// not used
	ga_161_gen_bp,	// basePrime 
	1,				// m = 1
    ga_161_gen_a,		
	ga_161_gen_b,		
	ga_zero,
	ga_161_gen_x1Plus,
	NULL,			// y1Plus
	ga_161_gen_x1Minus,
	ga_161_gen_plusOrder,
	ga_161_gen_minusOrder,
	ga_161_gen_x1OrderPlus,
	ga_161_gen_x1OrderMinus,
	ga_161_gen_x1OrderPlusRecip,
	NULL			// x1PlusOrder is lesser
},
{
	/*
	 * depth = 6
 	 * IEEE P1363 COMPATIBLE.
	 * (NIST-P-192 RECOMMENDED PRIME)
	 * primeType->General
	 * curveType->Weierstrass
	 * p is a 192-bit random prime (below, ga_161_gen_bp[]);
	 * a = -3;   
	 * b = 2455155546008943817740293915197451784769108058161191238065;    
	 * c = 0;
	 * Plus-order is prime, minus-order is composite.
	 */
	FPT_General,
	FCT_Weierstrass,
	192, 0,			// only used for initGiantStacks(giantMaxDigits())
	ga_192_gen_bp,	// basePrime 
	1,				// m = 1
    ga_192_gen_a,		
	ga_192_gen_b,		
	ga_zero,
	ga_192_gen_x1Plus,
	NULL,			// y1Plus
	ga_192_gen_x1Minus,
	ga_192_gen_plusOrder,
	ga_192_gen_minusOrder,
	ga_192_gen_x1OrderPlus,
	ga_192_gen_x1OrderMinus,
	ga_192_gen_x1OrderPlusRecip,
	ga_192_gen_lesserX1OrderRecip  
},

/* ANSI X9.62/Certicom curves */
{
	/*
	 * depth = 7
 	 * ANSI X9.62/Certicom secp192r1
	 */
	FPT_General,
	FCT_Weierstrass,
	192, 0,			// only used for initGiantStacks(giantMaxDigits())
	ga_192_secp_bp,	// basePrime 
	1,				// m = 1
    ga_192_secp_a,		
	ga_192_secp_b,		
	ga_zero,
	ga_192_secp_x1Plus,
	ga_192_secp_y1Plus,
	NULL,			// x1Minus
	ga_192_secp_plusOrder,
	NULL,			// minusOrder,
	ga_192_secp_x1OrderPlus,
	NULL,			// x1OrderMinus,
	ga_192_secp_x1OrderPlusRecip,
},
{
	/*
	 * depth = 8
 	 * ANSI X9.62/Certicom secp256r1
	 */
	FPT_General,
	FCT_Weierstrass,
	256, 0,			// only used for initGiantStacks(giantMaxDigits())
	ga_256_secp_bp,	// basePrime 
	1,				// m = 1
    ga_256_secp_a,		
	ga_256_secp_b,		
	ga_zero,
	ga_256_secp_x1Plus,
	ga_256_secp_y1Plus,
	NULL,
	ga_256_secp_plusOrder,
	NULL,
	ga_256_secp_x1OrderPlus,
	NULL,
	ga_256_secp_x1OrderPlusRecip,
	NULL  
},
{
	/*
	 * depth = 9
 	 * ANSI X9.62/Certicom secp384r1
	 */
	FPT_General,
	FCT_Weierstrass,
	384, 0,			// only used for initGiantStacks(giantMaxDigits())
	ga_384_secp_bp,	// basePrime 
	1,				// m = 1
    ga_384_secp_a,		
	ga_384_secp_b,		
	ga_zero,
	ga_384_secp_x1Plus,
	ga_384_secp_y1Plus,
	NULL,
	ga_384_secp_plusOrder,
	NULL,
	ga_384_secp_x1OrderPlus,
	NULL,
	ga_384_secp_x1OrderPlusRecip,
	NULL  
},
{
	/*
	 * depth = 10
 	 * ANSI X9.62/Certicom secp521r1
	 */
	FPT_General,
	FCT_Weierstrass,
	521, 0,		
	ga_521_secp_bp,	// basePrime 
	1,				// m = 1
    ga_521_secp_a,		
	ga_521_secp_b,		
	ga_zero,
	ga_521_secp_x1Plus,
	ga_521_secp_y1Plus,
	NULL,
	ga_521_secp_plusOrder,
	NULL,
	ga_521_secp_x1OrderPlus,
	NULL,
	ga_521_secp_x1OrderPlusRecip,
	NULL  
}
};
#endif	/* FEE_PROTOTYPE_CURVES */

/*
 * Convert the static form of a giant - i.e., an array of arrayDigits,
 * the first of which is the sign, the remainder of which are base 65536
 * digits - into a giant. A null pointer on input results in a null return.
 */
static giant arrayToGiant(const arrayDigit *array)
{
	unsigned 	numBytes;	// in result->n[]
	int			numDigits;	// ditto
	giant    	result;
	giantDigit 	digit;
	unsigned char 	byte;
	unsigned 	i;
	unsigned 	digitDex;	// index into result->n[]
	unsigned 	digitByte;	// byte selector in digit
	const arrayDigit 	*ap;		// running ptr into array
	short		sign;

	if(array == NULL) {
		CKRaise("arrayToGiant: NULL array");
	}
	sign = (short)array[0];
	numBytes = abs(sign) * sizeof(unsigned short);
	numDigits = BYTES_TO_GIANT_DIGITS(numBytes);

	/* note giantstruct has one explicit giantDigit */
	result = (giant) fmalloc(sizeof(giantstruct) +
		((numDigits - 1) * GIANT_BYTES_PER_DIGIT));
	result->capacity = numDigits;

	ap = array + 1;
	digit = 0;
	digitDex = 0;

	for(i=0; i<numBytes;) {
	    for(digitByte=0; digitByte<GIANT_BYTES_PER_DIGIT; digitByte++) {
	        /* grab a byte from the array */
	    	if(i & 1) {
		    /* odd byte - snag it and advance to next array digit */
		    byte = (unsigned char)(*ap++ >> 8);
		}
		else {
		    /* even, i.e., l.s. byte */
		    byte = (unsigned char)(*ap);
		}

		/* add byte to current digit */
		digit |= (byte << (8 * digitByte));
		if(++i == numBytes) {
		    /* end of array, perhaps in the midst of a digit */
		    break;
		}
	    }
	    result->n[digitDex++] = digit;
	    digit = 0;
	};

	/* Careful:
	 * -- array elements are unsigned. The first element is
	 *    he number of SHORTS in the array; convert to native
	 *    digits.
	 * -- in the very odd (test only) case of giantDigit = unsigned char,
	 *    we might have fewer valid digits than numDigits (might have
	 *    leading zeros).
	 */
	if(sign < 0) {
	    result->sign = -numDigits;
	}
	else {
	    result->sign = numDigits;
	}
	gtrimSign(result);
	return result;
}

/*
 * Obtain a malloc'd and uninitialized curveParams, to be init'd by caller.
 */
curveParams *newCurveParams(void)
{
	curveParams *params = (curveParams*) fmalloc(sizeof(curveParams));

	bzero(params, sizeof(curveParams));
	return params;
}

/*
 * Alloc and zero reciprocal giants, when maxDigits is known.
 * Currently only called when creating a curveParams from a public key.
 * cp->primeType must be valid on input.
 */
void allocRecipGiants(curveParams *cp)
{
	cp->lesserX1OrderRecip = newGiant(cp->maxDigits);
	cp->x1OrderPlusRecip = newGiant(cp->maxDigits);
	int_to_giant(0, cp->lesserX1OrderRecip);
	int_to_giant(0, cp->x1OrderPlusRecip);
}

/*
 * Obtain a malloc'd curveParams for a specified feeDepth.
 */
curveParams *curveParamsForDepth(feeDepth depth)
{
	curveParams *cp;
	const curveParamsStatic *cps = &curveParamsArray[depth];

	if(depth > FEE_DEPTH_MAX) {
		return NULL;
	}
	#if	GIANTS_VIA_STACK
	curveParamsInitGiants();
	#endif
	cp = newCurveParams();
	cp->primeType = cps->primeType;
	cp->curveType = cps->curveType;
	cp->q = cps->q;
	cp->k = cps->k;
	cp->m = cps->m;
	if(cp->primeType == FPT_General) {
	    cp->basePrime = arrayToGiant(cps->basePrime);
	}
	cp->a = arrayToGiant(cps->a);
	cp->b = arrayToGiant(cps->b);
	cp->c = arrayToGiant(cps->c);
	cp->x1Plus  = arrayToGiant(cps->x1Plus);
	if(cps->y1Plus) {
		cp->y1Plus = arrayToGiant(cps->y1Plus);
	}
	if(cps->x1Minus) {
		cp->x1Minus = arrayToGiant(cps->x1Minus);
	}
	cp->cOrderPlus   = arrayToGiant(cps->cOrderPlus);
	if(cps->cOrderMinus) {
		cp->cOrderMinus  = arrayToGiant(cps->cOrderMinus);
	}
	cp->x1OrderPlus  = arrayToGiant(cps->x1OrderPlus);
	if(cps->x1OrderMinus) {
		cp->x1OrderMinus = arrayToGiant(cps->x1OrderMinus);
	}
	cp->x1OrderPlusRecip = arrayToGiant(cps->x1OrderPlusRecip);

	/*
	 * Special case optimization for equal reciprocals.
	 */
	if(cps->lesserX1OrderRecip == NULL) {
	    cp->lesserX1OrderRecip = cp->x1OrderPlusRecip;
	}
	else {
	    cp->lesserX1OrderRecip = arrayToGiant(cps->lesserX1OrderRecip);
	}

	/* remainder calculated at runtime */
	curveParamsInferFields(cp);
	return cp;
}

/*
 * Alloc a new curveParams struct as a copy of specified instance.
 * This is the only way we can create a curveParams struct which doesn't
 * match any existing known curve params.
 */
curveParams *curveParamsCopy(curveParams *cp)
{
	curveParams *newcp = newCurveParams();

	newcp->primeType = cp->primeType;
	newcp->curveType = cp->curveType;
	newcp->q = cp->q;
	newcp->k = cp->k;
	newcp->m = cp->m;
	newcp->basePrime = copyGiant(cp->basePrime);
	newcp->minBytes = cp->minBytes;
	newcp->maxDigits = cp->maxDigits;

	newcp->a            = copyGiant(cp->a);
	newcp->b            = copyGiant(cp->b);
	newcp->c            = copyGiant(cp->c);
	newcp->x1Plus       = copyGiant(cp->x1Plus);
	if(cp->x1Minus) {
		newcp->x1Minus 	= copyGiant(cp->x1Minus);
	}
	newcp->y1Plus 	    = copyGiant(cp->y1Plus);
	newcp->cOrderPlus   = copyGiant(cp->cOrderPlus);
	if(cp->cOrderMinus) {
		newcp->cOrderMinus  = copyGiant(cp->cOrderMinus);
	}
	newcp->x1OrderPlus  = copyGiant(cp->x1OrderPlus);
	if(cp->x1OrderMinus) {
		newcp->x1OrderMinus = copyGiant(cp->x1OrderMinus);
	}
	
	newcp->x1OrderPlusRecip = copyGiant(cp->x1OrderPlusRecip);
	if(cp->x1OrderPlusRecip == cp->lesserX1OrderRecip) {
		/*
		 * Equal reciprocals; avoid new malloc
		 */
		newcp->lesserX1OrderRecip = newcp->x1OrderPlusRecip;
	}
	else {
		newcp->lesserX1OrderRecip = copyGiant(cp->lesserX1OrderRecip);
	}
	if(cp->primeType == FPT_General) {
		newcp->basePrimeRecip = copyGiant(cp->basePrimeRecip);
	}
	return newcp;
}

/*
 * Free a curveParams struct.
 */
void freeCurveParams(curveParams *cp)
{
	if(cp->basePrime != NULL) {
		freeGiant(cp->basePrime);
	}
	if(cp->a != NULL) {
		freeGiant(cp->a);
	}
	if(cp->b != NULL) {
		freeGiant(cp->b);
	}
	if(cp->c != NULL) {
		freeGiant(cp->c);
	}
	if(cp->x1Plus != NULL) {
		freeGiant(cp->x1Plus);
	}
	if(cp->x1Minus != NULL) {
		freeGiant(cp->x1Minus);
	}
	if(cp->y1Plus != NULL) {
		freeGiant(cp->y1Plus);
	}
	if(cp->cOrderPlus != NULL) {
		freeGiant(cp->cOrderPlus);
	}
	if(cp->cOrderMinus != NULL) {
		freeGiant(cp->cOrderMinus);
	}
	if(cp->x1OrderPlus != NULL) {
		freeGiant(cp->x1OrderPlus);
	}
	if(cp->x1OrderMinus != NULL) {
		freeGiant(cp->x1OrderMinus);
	}
	if(cp->x1OrderPlusRecip != NULL) {
		freeGiant(cp->x1OrderPlusRecip);
	}

	/*
	 * Special case - if these are equal, we only alloc'd one giant
	 */
	if(cp->lesserX1OrderRecip != cp->x1OrderPlusRecip) {
		freeGiant(cp->lesserX1OrderRecip);
	}
	if(cp->basePrimeRecip != NULL) {
		freeGiant(cp->basePrimeRecip);
	}
	ffree(cp);
}

/*
 * Returns 1 if two sets of curve parameters are equivalent, else returns 0.
 */
int curveParamsEquivalent(curveParams *cp1, curveParams *cp2)
{
	if(cp1 == cp2) {
		/*
		 * Trivial case, actually common for signature verify
		 */
		return 1;
	}
	if(cp1->primeType != cp2->primeType) {
		return 0;
	}
	if(cp1->curveType != cp2->curveType) {
		return 0;
	}
	if(cp1->k != cp2->k) {
		return 0;
	}
	if(cp1->q != cp2->q) {
		return 0;
	}
	if(cp1->m != cp2->m) {
		return 0;
	}
	if(gcompg(cp1->basePrime, cp2->basePrime)) {
		return 0;
	}
	if(gcompg(cp1->a, cp2->a)) {
		return 0;
	}
	if(gcompg(cp1->b, cp2->b)) {
		return 0;
	}
	if(gcompg(cp1->c, cp2->c)) {
		return 0;
	}
	if(gcompg(cp1->x1Plus, cp2->x1Plus)) {
		return 0;
	}
	if((cp1->x1Minus != NULL) && (cp2->x1Minus != NULL)) {
		if(gcompg(cp1->x1Minus, cp2->x1Minus)) {
			return 0;
		}
	}
	if(gcompg(cp1->cOrderPlus, cp2->cOrderPlus)) {
		return 0;
	}
	if((cp1->cOrderMinus != NULL) && (cp2->cOrderMinus != NULL)) {
		if(gcompg(cp1->cOrderMinus, cp2->cOrderMinus)) {
			return 0;
		}
	}
	if(gcompg(cp1->x1OrderPlus, cp2->x1OrderPlus)) {
		return 0;
	}
	if((cp1->x1OrderMinus != NULL) && (cp2->x1OrderMinus != NULL)) {
		if(gcompg(cp1->x1OrderMinus, cp2->x1OrderMinus)) {
			return 0;
		}
	}
	/*
	 * If we got this far, reciprocals can't possibly be different
	 */
	return 1;
}

/*
 * Obtain the lesser of {x1OrderPlus, x1OrderMinus}. Returned value is not
 * malloc'd; it's a pointer to one of the orders in *cp.
 */
giant lesserX1Order(curveParams *cp)
{
	CKASSERT(!isZero(cp->x1OrderPlus));

	if(cp->x1OrderMinus == NULL) {
	    return(cp->x1OrderPlus);
	}
	else if(gcompg(cp->x1OrderPlus, cp->x1OrderMinus) >= 0) {
	    return(cp->x1OrderMinus);
	}
	else {
	    return(cp->x1OrderPlus);
	}
}

#if		GIANTS_VIA_STACK

/*
 * Prime the curveParams and giants modules for quick allocs of giants.
 */
static int giantsInitd = 0;

void curveParamsInitGiants(void)
{
	const curveParamsStatic *cps = &curveParamsArray[FEE_DEPTH_MAX];

	if(giantsInitd) {
		return;
	}

	/*
	 * Figure the max giant size of the largest depth we know about...
	 */
	initGiantStacks(giantMaxDigits(giantMinBytes(cps->q, cps->k)));
	giantsInitd = 1;
}

#endif	// GIANTS_VIA_STACK

/*
 * Infer the following fields from a partially constructed curveParams:
 *
 *   basePrimeRecip if primeType == FPT_General
 *   basePrime if primeType != FPT_General
 *   y1Plus if curveType == FCT_Weierstrass and not pre-calculated
 *   minBytes
 *   maxDigits
 *
 * Assumes the following valid on entry:
 *   curveType
 *   primeType
 *   basePrime if primeType == FPT_General
 *   q, k if primeType != FPT_General
 */
void curveParamsInferFields(curveParams *cp)
{
	/* calc maxDigits, minBytes */
	calcGiantSizes(cp);

	/* basePrime or its reciprocal */
	if(cp->primeType == FPT_General) {
		/* FIXME this should be declared statically! */
	    cp->basePrimeRecip = newGiant(cp->maxDigits);
	    make_recip(cp->basePrime, cp->basePrimeRecip);
	}
	else {
	    /*
	     * FPT_FEE, FPT_Mersenne
	     */
	    cp->basePrime = newGiant(cp->maxDigits);
	    make_base_prim(cp);
	}

	/* y1Plus */
	#if CRYPTKIT_ELL_PROJ_ENABLE
	if(cp->curveType == FCT_Weierstrass) {
		if(cp->y1Plus == NULL) {
			/* ECDSA Curves already have this */
			pointProj pt = newPointProj(cp->maxDigits);
			findPointProj(pt, cp->x1Plus, cp);

			/* initial point is supposed to be on curve! */
			if(gcompg(pt->x, cp->x1Plus)) {
				CKRaise("curveParamsInferFields failure");
			}
			cp->y1Plus = copyGiant(pt->y);
			freePointProj(pt);
		}
	}
	else {
		cp->y1Plus = newGiant(1);
	}
	#else	/* CRYPTKIT_ELL_PROJ_ENABLE */
	cp->y1Plus = newGiant(1);
	#endif
	
	if((cp->x1OrderPlusRecip == NULL) || isZero(cp->x1OrderPlusRecip)) {
		/*
		 * an easy way of figuring this one out, this should not 
		 * normally run. 
		 */
		cp->x1OrderPlusRecip = newGiant(cp->maxDigits);
		make_recip(cp->x1OrderPlus, cp->x1OrderPlusRecip);
		if(cp->lesserX1OrderRecip != NULL) {
			freeGiant(cp->lesserX1OrderRecip);
		}
		cp->lesserX1OrderRecip = cp->x1OrderPlusRecip;
	}
}

/*
 * Given key size in bits, obtain the asssociated depth.
 * Returns FR_IllegalDepth if specify key size not found
 * in current curve tables. 
 */
#define LOG_DEPTH	0

#if	FEE_PROTOTYPE_CURVES
feeReturn feeKeyBitsToDepth(unsigned keySize,
	feePrimeType primeType,		/* FPT_Fefault means "best one" */
	feeCurveType curveType,		/* FCT_Default means "best one" */
	feeDepth *depth)
{
	feeReturn frtn = FR_Success;
	switch(keySize) {
	    case 31:
			switch(curveType) {
				case FCT_Montgomery:
				default:
					*depth = FEE_DEPTH_31_1_M;
					break;
				case FCT_Weierstrass:
					*depth = FEE_DEPTH_31_1_P;
					break;
			}
			break;
		case 40:
			switch(curveType) {
				case FCT_Weierstrass:
				default:
					*depth = FEE_DEPTH_40_213;
					break;
				case FCT_Montgomery:
					return FR_IllegalDepth;
			}
			break;
		case 127:
			switch(curveType) {
				case FCT_Montgomery:
					if(primeType == FPT_General) {
						*depth = FEE_DEPTH_127_GEN;
					}
					else{
						*depth = FEE_DEPTH_127_1;
					}
					break;
				case FCT_Weierstrass:
				default:
					*depth = FEE_DEPTH_127_1W;
					break;
			}
			break;
		case 160:
			switch(curveType) {
				case FCT_Montgomery:
					return FR_IllegalDepth;
				case FCT_Weierstrass:
				default:
					if(primeType == FPT_General) {
						*depth = FEE_DEPTH_160_GEN;
					}
					else {
						*depth = FEE_DEPTH_160_57;
					}
					break;
			}
			break;
		case 192:
			switch(curveType) {
				case FCT_Montgomery:
					*depth = FEE_DEPTH_192_M529891;
				case FCT_Weierstrass:
				default:
					*depth = FEE_DEPTH_192_1425;
					break;
			}
			break;
		default:
			frtn = FR_IllegalDepth;
			break;
	}
	#if LOG_DEPTH
	printf("feeKeyBitsToDepth: depth %d\n", *depth);
	#endif
	return frtn;
}

#else	/* FEE_PROTOTYPE_CURVES */

feeReturn feeKeyBitsToDepth(unsigned keySize,
	feePrimeType primeType,		/* FPT_Fefault means "best one" */
	feeCurveType curveType,		/* FCT_Default means "best one" */
	feeDepth *depth)
{
	feeReturn frtn = FR_Success;
	switch(keySize) {
	    case 31:
			if(primeType == FPT_General) {
				return FR_IllegalDepth; 
			}
			/* note we cut a request for FPT_FEE some slack...this is actually
			 * FPT_Mersenne, but that is technically a subset of FEE. */
			switch(curveType) {
				case FCT_Montgomery:
					*depth = FEE_DEPTH_31M;
					break;
				case FCT_Weierstrass:
				case FCT_Default:
					*depth = FEE_DEPTH_31W;
					break;
				default:
					return FR_IllegalDepth; 
			}
			break;
		case 127:
			/* Montgomery only */
			if(primeType == FPT_General) {
				return FR_IllegalDepth; 
			}
			/* note we cut a request for FPT_FEE some slack...this is actually
			 * FPT_Mersenne, but that is technically a subset of FEE. */
			switch(curveType) {
				case FCT_Montgomery:
				case FCT_Default:
					*depth = FEE_DEPTH_127M;
					break;
				case FCT_Weierstrass:
				default:
					return FR_IllegalDepth; 
			}
			break;
		case 128:
			/* Weierstrass/feemod only */
			switch(primeType) {
				case FPT_General:
				case FPT_Mersenne:
					return FR_IllegalDepth; 
				default:
					/* FPT_Default, FPT_FEE */
					break;
			}
			switch(curveType) {
				case FCT_Weierstrass:
				case FCT_Default:
					*depth = FEE_DEPTH_128W;
					break;
				default:
					return FR_IllegalDepth; 
			}
			break;
		case 161:
			switch(curveType) {
				case FCT_Weierstrass:
				case FCT_Default:
					switch(primeType) {
						case FPT_General:
							*depth = FEE_DEPTH_161G;
							break;
						case FPT_FEE:
						case FPT_Default:
							*depth = FEE_DEPTH_161W;
							break;
						default:
							/* i.e., FPT_Mersenne */
							return FR_IllegalDepth;
					}
					break;
				default:
					return FR_IllegalDepth;
			}
			break;
		case 192:
			switch(curveType) {
				case FCT_Montgomery:
				default:
					return FR_IllegalDepth;
				case FCT_Weierstrass:
				case FCT_Default:
					switch(primeType) {
						case FPT_General:
						case FPT_Default:
							*depth = FEE_DEPTH_192G;
							break;
						default:
							/* i.e., FPT_Mersenne, FPT_FEE */
							return FR_IllegalDepth;
					}
					break;
				case FCT_ANSI:
					switch(primeType) {
						case FPT_General:
						case FPT_Default:
							break;
						default:
							return FR_IllegalDepth;
					}
					*depth = FEE_DEPTH_secp192r1;
					break;
			}
			break;
		case 256:
			switch(curveType) {
				case FCT_ANSI:
				case FCT_Default:
					break;
				default:
					return FR_IllegalDepth;
			}
			switch(primeType) {
				case FPT_General:
				case FPT_Default:
					break;
				default:
					return FR_IllegalDepth;
			}
			*depth = FEE_DEPTH_secp256r1;
			break;
		case 384:
			switch(curveType) {
				case FCT_ANSI:
				case FCT_Default:
					break;
				default:
					return FR_IllegalDepth;
			}
			switch(primeType) {
				case FPT_General:
				case FPT_Default:
					break;
				default:
					return FR_IllegalDepth;
			}
			*depth = FEE_DEPTH_secp384r1;
			break;
		case 521:
			switch(curveType) {
				case FCT_ANSI:
				case FCT_Default:
					break;
				default:
					return FR_IllegalDepth;
			}
			switch(primeType) {
				case FPT_General:
				case FPT_Default:
					break;
				default:
					return FR_IllegalDepth;
			}
			*depth = FEE_DEPTH_secp521r1;
			break;
			
		default:
			frtn = FR_IllegalDepth;
			break;
	}
	#if LOG_DEPTH
	printf("feeKeyBitsToDepth: depth %d\n", *depth);
	#endif
	return frtn;
}

#endif	/* FEE_PROTOTYPE_CURVES  */

/* 
 * Obtain depth for specified curveParams
 */
feeReturn curveParamsDepth(
	curveParams *cp,
	feeDepth *depth)
{
	if(cp == NULL) {
		return FR_IllegalArg;
	}
	
	/* We do it this way to allow reconstructing depth from an encoded curveParams */
	feeCurveType curveType = cp->curveType;
	if((curveType == FCT_Weierstrass) && (cp->x1Minus == NULL)) {
		/* actually an ANSI curve */
		curveType = FCT_ANSI;
	}
	return feeKeyBitsToDepth(cp->q, cp->primeType, curveType, depth);
}

	
