/*
 * giantPort_PPC_Gnu.h - giant definitions, PPC/GNU version.
 */

#ifndef	_CK_NSGIANT_PORT_PPC_GNU_H_
#define _CK_NSGIANT_PORT_PPC_GNU_H_

#include "feeDebug.h"
#include "platform.h"
#include "giantIntegers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* define this true to disable this module and use generic C versions instead */
#define PPC_GIANT_PORT_INLINE	0

#if	PPC_GIANT_PORT_INLINE

#include "giantPort_Generic.h"

#else	// PPC_GIANT_PORT_INLINE

/*
 * Multiple-precision arithmetic routines/macros implemented in 
 * giantPort_PPC_Gnu.s
 */

/*
 * Add two digits, return sum. Carry bit returned as an out parameter.
 */
extern giantDigit giantAddDigits(
	giantDigit dig1,
	giantDigit dig2,
	giantDigit *carry);			/* RETURNED, 0 or 1 */

/*
 * Add a single digit value to a double digit accumulator in place.
 * Carry out of the MSD of the accumulator is not handled.
 */
void giantAddDouble(
	giantDigit *accLow,			/* IN/OUT */
	giantDigit *accHigh,			/* IN/OUT */
	giantDigit val);


/*
 * Subtract a - b, return difference. Borrow bit returned as an out parameter.
 */
giantDigit giantSubDigits(
	giantDigit a,
	giantDigit b,
	giantDigit *borrow);			/* RETURNED, 0 or 1 */


/*
 * Multiply two digits, return two digits.
 */
void giantMulDigits(
	giantDigit	dig1,
	giantDigit	dig2,
 	giantDigit	*lowProduct,		/* RETURNED, low digit */
	giantDigit	*hiProduct);		/* RETURNED, high digit */

/*
 * Multiply a vector of giantDigits, candVector, by a single giantDigit,
 * plierDigit, adding results into prodVector. Returns m.s. digit from
 * final multiply; only candLength digits of *prodVector will be written.
 */
giantDigit VectorMultiply(
	giantDigit plierDigit,
	giantDigit *candVector,
	unsigned candLength,
	giantDigit *prodVector);

#ifdef __cplusplus
}
#endif

#endif	/* !PPC_GIANT_PORT_INLINE */

#endif	/*_CK_NSGIANT_PORT_PPC_GNU_H_*/
