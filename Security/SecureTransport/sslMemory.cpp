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
	File:		sslMemory.c

	Contains:	memory allocator implementation

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslMemory.h"
#include "sslContext.h"
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

OSStatus SSLAllocBuffer(
	SSLBuffer &buf, 
	UInt32 length, 
	const SSLContext *ctx)			// currently unused
{   
	buf.data = (UInt8 *)sslMalloc(length);
	if(buf.data == NULL) {
		buf.length = 0;
		return memFullErr;
	}
    buf.length = length;
    return noErr;
}

OSStatus
SSLFreeBuffer(SSLBuffer &buf, const SSLContext *ctx)
{   
	if(&buf == NULL) {
		sslErrorLog("SSLFreeBuffer: NULL buf!\n");
		return errSSLInternal;
	}
    sslFree(buf.data);
    buf.data = NULL;
    buf.length = 0;
    return noErr;
}

OSStatus
SSLReallocBuffer(SSLBuffer &buf, UInt32 newSize, const SSLContext *ctx)
{   
	buf.data = (UInt8 *)sslRealloc(buf.data, buf.length, newSize);
	if(buf.data == NULL) {
		buf.length = 0;
		return memFullErr;
	}
	buf.length = newSize;
	return noErr;
}

#pragma mark *** Convenience routines ***

UInt8 *sslAllocCopy(
	const UInt8 *src,
	UInt32 len)
{
	UInt8 *dst;
	
	dst = (UInt8 *)sslMalloc(len);
	if(dst == NULL) {
		return NULL;
	} 
	memmove(dst, src, len);
	return dst;
} 

OSStatus SSLAllocCopyBuffer(
	const SSLBuffer &src, 
	SSLBuffer **dst)		// buffer and data mallocd and returned 
{   
	OSStatus serr;
	
	SSLBuffer *rtn = (SSLBuffer *)sslMalloc(sizeof(SSLBuffer));
	if(rtn == NULL) {
		return memFullErr;
	}
	serr = SSLCopyBuffer(src, *rtn);
	if(serr) {
		sslFree(rtn);
	}
	else {
		*dst = rtn;
	}
	return serr;
}

OSStatus SSLCopyBufferFromData(
	const void *src,
	UInt32 len,
	SSLBuffer &dst)		// data mallocd and returned 
{   
	dst.data = sslAllocCopy((const UInt8 *)src, len);
	if(dst.data == NULL) {
		return memFullErr;
	}
    dst.length = len;
    return noErr;
}

OSStatus SSLCopyBuffer(
	const SSLBuffer &src, 
	SSLBuffer &dst)		// data mallocd and returned 
{   
	return SSLCopyBufferFromData(src.data, src.length, dst);
}

