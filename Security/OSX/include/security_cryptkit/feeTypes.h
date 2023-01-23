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
 * feeTypes.h - general purpose FEE typedefs and constants
 *
 * Revision History
 * ----------------
 *  23 Mar 98 at Apple
 *	Added FR_BadKeyBlob.
 *  20 Jan 98 at Apple
 * 	New PT_GENERAL depth values.
 *  09 Jan 98 at Apple
 *	Removed obsolete FEE_DEPTH_* values.
 *  20 Aug 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEETYPES_H_
#define _CK_FEETYPES_H_

/*
 * Opaque public key object.
 */
typedef void *feePubKey;

/*
 * Standard return codes.
 * Remember to update frtnStrings[] in utilities.c when adding new items.
 */
typedef enum {
	FR_Success = 0,
	FR_BadPubKey,
	FR_BadPubKeyString,
	FR_IncompatibleKey,		/* incompatible key */
	FR_IllegalDepth,
	FR_BadUsageName,		/* bad usageName */
	FR_BadSignatureFormat,	/* signature corrupted */
	FR_InvalidSignature,	/* signature intact, but not valid */
	FR_IllegalArg,			/* illegal argument */
	FR_BadCipherText,		/* malformed ciphertext */
	FR_Unimplemented,		/* unimplemented function */
	FR_BadCipherFile,
	FR_BadEnc64,			/* bad enc64() format */
	FR_WrongSignatureType,	/* ElGamal vs. ECDSA */
	FR_BadKeyBlob,
	FR_IllegalCurve,		/* e.g., ECDSA with Montgomery curve */
	FR_Internal,			/* internal library error */
	FR_Memory,				/* out of memory */
	FR_ShortPrivData		/* insufficient privData for creating
							 *   private key */
	/* etc. */
} feeReturn;

typedef enum {
    FSF_Default,			/* default */
    FSF_DER,                /* DER */
    FSF_RAW,                /* RAW (for ECDSA, first half is r, second half is s */
} feeSigFormat;

/*
 * The feeDepth parameter defines one of 'n' known curves. From a user's
 * perspective, the most interesting parameter indicated by feeDepth is
 * the size (in bits) of the key.
 */
typedef unsigned feeDepth;

/*
 * Prime and curve description parameters.
 */
typedef enum {
	FPT_Default,			/* default per key size */
	FPT_Mersenne,			/* (2 ** q) - 1 */
	FPT_FEE,				/* (2 ** q) - k */
	FPT_General				/* random prime */
} feePrimeType;

typedef enum {
	FCT_Default,			/* default per key size */
	FCT_Montgomery,			/* a==1, b==0 */
	FCT_Weierstrass,		/* c==0. IEEE P1363 compliant. */
	FCT_ANSI,				/* ANSI X9.62/Certicom, also FCT_Weierstrass */
	FCT_General				/* Other */
} feeCurveType;


/*
 * The real curves as of 4/9/2001.
 * Note that ECDSA signatures can only be performed with curve of 
 * curveType FCT_Weierstrass.
 *
 * Default curveType for curves with same prime size is FCT_Weierstrass.
 */
#define FEE_DEPTH_31M		0		/* size=31  FPT_Mersenne FCT_Montgomery */
#define FEE_DEPTH_31W		1		/* size=31  FPT_Mersenne FCT_Weierstrass */
#define FEE_DEPTH_127M		2		/* size=127 FPT_Mersenne FCT_Montgomery */
#define FEE_DEPTH_128W		3		/* size=128 FPT_FEE FCT_Weierstrass */
#define FEE_DEPTH_161W		4		/* size=161 FPT_FEE      FCT_Weierstrass */
#define FEE_DEPTH_161G		5		/* size=161 FPT_General  FCT_Weierstrass */
#define FEE_DEPTH_192G		6		/* size=192 FPT_General  FCT_Weierstrass */

/* ANSI X9.62/Certicom curves */
#define FEE_DEPTH_secp192r1	7		/* size=192 FPT_General  FCT_ANSI */
#define FEE_DEPTH_secp256r1	8		/* size=256 FPT_General  FCT_ANSI */
#define FEE_DEPTH_secp384r1	9		/* size=384 FPT_General  FCT_ANSI */
#define FEE_DEPTH_secp521r1	10		/* size=521 FPT_General  FCT_ANSI */
/*
 * The default depth.
 */
#define FEE_DEPTH_DEFAULT	FEE_DEPTH_161W

/*
 * Last enumerated depth.
 */
#define FEE_DEPTH_MAX		FEE_DEPTH_secp521r1


/*
 * Random number generator callback function.
 */
typedef feeReturn (*feeRandFcn)(
	void *ref,
	unsigned char *bytes,		/* must be alloc'd by caller */
	unsigned numBytes);
	
#endif	/* _CK_FEETYPES_H_ */
