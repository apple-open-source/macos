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
 * falloc.h - FEE malloc routines
 *
 * Revision History
 * ----------------
 *  20 Aug 96	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_FALLOC_H_
#define _CK_FALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Clients can *optionally* register external memory alloc/free functions here.
 */
typedef void *(mallocExternFcn)(unsigned size);
typedef void (freeExternFcn)(void *data);
typedef void *(reallocExternFcn)(void *oldData, unsigned newSize);
void fallocRegister(mallocExternFcn *mallocExtern,
	freeExternFcn *freeExtern,
	reallocExternFcn *reallocExtern);
	
	
void *fmalloc(unsigned size);		/* general malloc */
void *fmallocWithData(const void *origData,
	unsigned origDataLen);		/* malloc, copy existing data */
void ffree(void *data);			/* general free */
void *frealloc(void *oldPtr, unsigned newSize);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_FALLOC_H_*/
