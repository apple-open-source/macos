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
	File:		sslalloc.c

	Contains:	memory allocator implementation

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslalloc.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslalloc.c   Utility functions for doing allocation

    These functions call the user-supplied callbacks to
    allocate/free/reallocate memory

    ****************************************************************** */

#include "sslalloc.h"
#include "sslctx.h"
#include "sslDebug.h"

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#pragma mark *** Basic low-level malloc/free ***

/*
 * For now, all allocs/frees go thru here. 
 */
#include <string.h>			/* memset */
#include <stdlib.h>

void *
sslMalloc(UInt32 length)
{   
	return malloc(length);
}

void
sslFree(void *p)
{   
	if(p != nil) {
		free(p);
	}
}

void *
sslRealloc(void *oldPtr, UInt32 oldLen, UInt32 newLen)
{   
	return realloc(oldPtr, newLen);
}

#pragma mark *** SSLBuffer-level alloc/free ***

SSLErr SSLAllocBuffer(
	SSLBuffer *buf, 
	UInt32 length, 
	const SystemContext *ctx)
{   
	buf->data = sslMalloc(length);
	if(buf->data == NULL) {
		buf->length = 0;
		return SSLMemoryErr;
	}
    buf->length = length;
    return SSLNoErr;
}

SSLErr
SSLFreeBuffer(SSLBuffer *buf, const SystemContext *ctx)
{   
	if(buf == NULL) {
		errorLog0("SSLFreeBuffer: NULL buf!\n");
		return SSLInternalError;
	}
    sslFree(buf->data);
    buf->data = NULL;
    buf->length = 0;
    return SSLNoErr;
}

SSLErr
SSLReallocBuffer(SSLBuffer *buf, UInt32 newSize, const SystemContext *ctx)
{   
	buf->data = sslRealloc(buf->data, buf->length, newSize);
	if(buf->data == NULL) {
		buf->length = 0;
		return SSLMemoryErr;
	}
	buf->length = newSize;
	return SSLNoErr;
}

#pragma mark *** Convenience routines ***

UInt8 *sslAllocCopy(
	const UInt8 *src,
	UInt32 len)
{
	UInt8 *dst;
	
	dst = sslMalloc(len);
	if(dst == NULL) {
		return NULL;
	} 
	memmove(dst, src, len);
	return dst;
} 

SSLErr SSLAllocCopyBuffer(
	const SSLBuffer *src, 
	SSLBuffer **dst)		// buffer and data mallocd and returned 
{   
	SSLErr serr;
	
	SSLBuffer *rtn = sslMalloc(sizeof(SSLBuffer));
	if(rtn == NULL) {
		return SSLMemoryErr;
	}
	serr = SSLCopyBuffer(src, rtn);
	if(serr) {
		sslFree(rtn);
	}
	else {
		*dst = rtn;
	}
	return serr;
}

SSLErr SSLCopyBuffer(
	const SSLBuffer *src, 
	SSLBuffer *dst)		// data mallocd and returned 
{   
	dst->data = sslAllocCopy(src->data, src->length);
	if(dst->data == NULL) {
		return SSLMemoryErr;
	}
    dst->length = src->length;
    return SSLNoErr;
}
