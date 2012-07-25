/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * ellipticProj.h - declaration of elliptic projective algebra routines.
 *
 * Revision History
 * ----------------
 * 10/06/98		ap 
 *	Changed to compile with C++.
 * 1 Sep 1998	Doug Mitchell at Apple
 *	Created.
 */

#ifndef	_CRYPTKIT_ELLIPTIC_PROJ_H_
#define _CRYPTKIT_ELLIPTIC_PROJ_H_

#include "ckconfig.h"

#if CRYPTKIT_ELL_PROJ_ENABLE

#include "giantIntegers.h"
#include "curveParams.h"

/*
 * A projective point.
 */
typedef struct {
	giant x;
	giant y;
	giant z;
} pointProjStruct;

typedef pointProjStruct *pointProj;

pointProj  /* Allocates a new projective point. */
newPointProj(unsigned numDigits);

void  /* Frees point. */
freePointProj(pointProj pt);

void  /* Copies point to point; pt2 := pt1. */
ptopProj(pointProj pt1, pointProj pt2);

void /* Point doubling. */
ellDoubleProj(pointProj pt, curveParams *cp);

void /* Point adding; pt0 := pt0 - pt1. */
ellAddProj(pointProj pt0, pointProj pt1, curveParams *cp);

void /* Point negation; pt := -pt. */
ellNegProj(pointProj pt, curveParams *cp);

void /* Point subtraction; pt0 := pt0 - pt1. */
ellSubProj(pointProj pt0, pointProj pt1, curveParams *cp);

void /* pt := pt * k, result normalized */
ellMulProjSimple(pointProj pt0, giant k, curveParams *cp);

void /* General elliptic mul; pt1 := k*pt0. */
ellMulProj(pointProj pt0, pointProj pt1, giant k, curveParams *cp);

void /* Generate normalized point (X, Y, 1) from given (x,y,z). */
normalizeProj(pointProj pt, curveParams *cp);

void /* Find a point (x, y, 1) on the curve. */
findPointProj(pointProj pt, giant seed, curveParams *cp);

#endif	/* CRYPTKIT_ELL_PROJ_ENABLE*/
#endif	/* _CRYPTKIT_ELLIPTIC_PROJ_H_ */
