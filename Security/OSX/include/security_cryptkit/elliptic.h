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
 * elliptic.h - Fast Elliptic Encryption functions.
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 19 Feb 97 at NeXT
 *	Created.
 */

#ifndef	_CK_NSFEE_H_
#define _CK_NSFEE_H_

#include "giantIntegers.h"
#include "feeTypes.h"
#include "curveParams.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Twist, or "which curve", parameter.
 */
#define CURVE_PLUS	((int)1)
#define CURVE_MINUS	((int)(-1))

typedef struct {
	int 		twist;			// CURVE_PLUS or CURVE_MINUS
	giant 		x;				// x coord of public key

	/*
	 * only valid for (twist == CURVE_PLUS) and curveType CT_WEIERSTRASS.
	 * Otherwise it's a zero-value giant.
	 */
	giant 		y;				// y coord of public key

	/*
	 * Note: this module never allocs or frees a curveParams structs.
	 * This field is always maintained by clients of this module.
	 */
	curveParams 	*cp;		// common curve parameters
} keystruct;

typedef keystruct *key;

/*
 * Select which curve is the default curve for calculating signatures and
 * doing key exchange. This *must* be CURVE_PLUS for key exchange to work
 * with ECDSA keys and curves. 
 */
#define DEFAULT_CURVE	CURVE_PLUS

key new_public(curveParams *cp, int twist);

/*
 * Specify private data for key created by new_public().
 * Generates k->x.
 */
void set_priv_key_giant(key k, giant privGiant);

/*
 * Generate new key with twist and k->x from old_key.
 */
key new_public_with_key(key old_key, curveParams *cp);

/*
 * Returns 1 if all parameters of two keys are equal, else returns 0.
 */
int key_equal(key first, key second);

/*
 * De-allocate an allocated key.
 */
void free_key(key pub);

/*
 * x3 = x1 + x2 on the curve, with sign ambiguity s.
 *
 * Note that int s is not just the twist field, because both s = +-1 must
 * be tested in general.
 */
void elliptic_add(giant x1, giant x2, giant x3, curveParams *par, int s);

/*
 * Values for the 's', or sign, argument to elliptic_add().
 */
#define SIGN_PLUS	1
#define SIGN_MINUS	(-1)


/*
 * Elliptic multiply: x := n * {x, 1}
 */
void elliptic_simple(giant x, giant n, curveParams *par);

/*
 * General elliptic multiply: {xx, zz} := k * {xx, zz}
 */
void elliptic(giant xx, giant zz, giant k, curveParams *par);

/*
 * Returns CURVE_PLUS or CURVE_MINUS, indicating which curve a particular
 * x coordinate resides on.
 */
int which_curve(giant x, curveParams *par);

/*
 * Generate (2**q)-k.
 */
void make_base_prim(curveParams *cp);

/*
 * return a new giant that is the pad from private data and public key
 */
giant make_pad(giant privGiant, key publicKey);

/*
 * Returns non-zero if x(p1) cannot be the x-coordinate of the
 * sum of two points whose respective x-coordinates are x(p2), x(p3).
 */
int signature_compare(giant p0x, giant p1x, giant p2x, curveParams *par);

/*
 * Set g := g mod curveOrder;
 * force g to be between 2 and (curveOrder-2), inclusive.
 */
void curveOrderJustify(giant g, giant curveOrder);

void lesserX1OrderJustify(giant g, curveParams *cp);
void x1OrderPlusJustify(giant g, curveParams *cp);
void x1OrderPlusMod(giant g, curveParams *cp);

void calcX1OrderPlusRecip(curveParams *cp);

/*
 * x := x mod basePrime.
 */
void feemod(curveParams *par, giant x);

/*
 * For a given curveParams, calculate minBytes and maxDigits.
 */
void calcGiantSizes(curveParams *cp);
unsigned giantMinBytes(unsigned q, int k);
unsigned giantMaxDigits(unsigned minBytes);

int binvg_cp(curveParams *cp, giant x);
int binvg_x1OrderPlus(curveParams *cp, giant x);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_NSFEE_H_*/
