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
 * falloc.c - FEE malloc routines
 *
 * Revision History
 * ----------------
 *  28 May 98	Doug Mitchell at Apple
 *	Added Mac-specific allocators from temp memory
 *  20 Aug 96	Doug Mitchell at NeXT
 *	Created.
 */

#include "platform.h"
#include "falloc.h"
#include <stdlib.h>

/* watchpoint emulator */
#define FALLOC_WATCH	0
#if		FALLOC_WATCH
#include <stdio.h>
/* set these with debugger */
void *mallocWatchAddrs;
void *freeWatchAddrs;
#endif

/* if NULL, use our own */
static mallocExternFcn *mallocExt = NULL;
static freeExternFcn *freeExt = NULL;
static reallocExternFcn *reallocExt = NULL;

void fallocRegister(mallocExternFcn *mallocExtern,
	freeExternFcn *freeExtern,
	reallocExternFcn *reallocExtern)
{
	mallocExt = mallocExtern;
	freeExt = freeExtern;
	reallocExt = reallocExtern;
}

/*
 * All this can be optimized and tailored to specific platforms, of course...
 */

void *fmalloc(unsigned size)
{
	void *rtn;
	if(mallocExt != NULL) {
		rtn = (mallocExt)(size);
	}
	else {
		rtn = malloc(size);
	}
	#if		FALLOC_WATCH
	if(rtn == mallocWatchAddrs) {
		printf("====fmalloc watchpoint (0x%x) hit\n",
			(unsigned)mallocWatchAddrs);
	}
	#endif
	return rtn;
}

void *fmallocWithData(const void *origData,
	unsigned origDataLen)
{
	void *rtn = fmalloc(origDataLen);

	bcopy(origData, rtn, origDataLen);
	return rtn;
}

void ffree(void *data)
{
	#if		FALLOC_WATCH
	if(data == freeWatchAddrs) {
		printf("====ffree watchpoint (0x%x) hit\n",
			(unsigned)freeWatchAddrs);
	}
	#endif
	if(freeExt != NULL) {
		(freeExt)(data);
	}
	else {
		free(data);
	}
}

void *frealloc(void *oldPtr, unsigned newSize)
{
	#if		FALLOC_WATCH
	if(oldPtr == freeWatchAddrs) {
		printf("====frealloc watchpoint (0x%x) hit\n",
			(unsigned)freeWatchAddrs);
	}
	#endif
	if(reallocExt != NULL) {
		return (reallocExt)(oldPtr, newSize);
	}
	else {
		return realloc(oldPtr, newSize);
	}
}
