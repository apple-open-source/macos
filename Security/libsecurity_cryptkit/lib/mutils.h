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
 * mutils.h - general private ObjC routine declarations
 *
 * Revision History
 * ----------------
 *  2 Aug 96	Doug Mitchell at NeXT
 *	Broke out from Blaine Garst's original NSCryptors.m
 */

#ifndef	_CK_MUTILS_H_
#define _CK_MUTILS_H_

#include <Foundation/Foundation.h>
#include "giantIntegers.h"

#ifdef __cplusplus
extern "C" {
#endif

extern NSMutableData *data_with_giant(giant u);
extern void canonicalize_data(NSMutableData *data);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_MUTILS_H_*/
