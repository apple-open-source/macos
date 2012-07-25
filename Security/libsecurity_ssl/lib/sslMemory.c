/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * sslMemory.c - Memory allocator implementation
 */

#include "sslMemory.h"
#include "sslContext.h"
#include "sslDebug.h"

#pragma mark -
#pragma mark Basic low-level malloc/free

/*
 * For now, all allocs/frees go thru here.
 */
#include <string.h>			/* memset */
#include <stdlib.h>

void *
sslMalloc(size_t length)
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
sslRealloc(void *oldPtr, size_t oldLen, size_t newLen)
{
	return realloc(oldPtr, newLen);
}

#pragma mark -
#pragma mark SSLBuffer-level alloc/free

OSStatus SSLAllocBuffer(
	SSLBuffer *buf,
	size_t length,
	const SSLContext *ctx)			// currently unused
{
	buf->data = (UInt8 *)sslMalloc(length);
	if(buf->data == NULL) {
		buf->length = 0;
		return memFullErr;
	}
    buf->length = length;
    return noErr;
}

OSStatus
SSLFreeBuffer(SSLBuffer *buf, const SSLContext *ctx)
{
	if(buf == NULL) {
		sslErrorLog("SSLFreeBuffer: NULL buf!\n");
		return errSSLInternal;
	}
    sslFree(buf->data);
    buf->data = NULL;
    buf->length = 0;
    return noErr;
}

OSStatus
SSLReallocBuffer(SSLBuffer *buf, size_t newSize, const SSLContext *ctx)
{
	buf->data = (UInt8 *)sslRealloc(buf->data, buf->length, newSize);
	if(buf->data == NULL) {
		buf->length = 0;
		return memFullErr;
	}
	buf->length = newSize;
	return noErr;
}

#pragma mark -
#pragma mark Convenience routines

UInt8 *sslAllocCopy(
	const UInt8 *src,
	size_t len)
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
	const SSLBuffer *src,
	SSLBuffer **dst)		// buffer and data mallocd and returned
{
	OSStatus serr;

	SSLBuffer *rtn = (SSLBuffer *)sslMalloc(sizeof(SSLBuffer));
	if(rtn == NULL) {
		return memFullErr;
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

OSStatus SSLCopyBufferFromData(
	const void *src,
	size_t len,
	SSLBuffer *dst)		// data mallocd and returned
{
	dst->data = sslAllocCopy((const UInt8 *)src, len);
	if(dst->data == NULL) {
		return memFullErr;
	}
    dst->length = len;
    return noErr;
}

OSStatus SSLCopyBuffer(
	const SSLBuffer *src,
	SSLBuffer *dst)		// data mallocd and returned
{
	return SSLCopyBufferFromData(src->data, src->length, dst);
}

