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
	File:		sslMemory.h

	Contains:	memory allocator declarations

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef _SSLMEMORY_H_
#define _SSLMEMORY_H_ 1

#include "sslContext.h"
#include "sslPriv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * General purpose allocators
 */
void *sslMalloc(size_t length);
void sslFree(void *p);
void *sslRealloc(void *oldPtr, size_t oldLen, size_t newLen);

/*
 * SSLBuffer-oriented allocators
 */
OSStatus SSLAllocBuffer(SSLBuffer *buf, size_t length, const SSLContext *ctx);
OSStatus SSLFreeBuffer(SSLBuffer *buf, const SSLContext *ctx);
OSStatus SSLReallocBuffer(SSLBuffer *buf, size_t newSize, const SSLContext *ctx);

/*
 * Convenience routines
 */
UInt8 *sslAllocCopy(const UInt8 *src, size_t len);
OSStatus SSLAllocCopyBuffer(
	const SSLBuffer *src, 
	SSLBuffer **dst);		// buffer itself and data mallocd and returned 
OSStatus SSLCopyBufferFromData(
	const void *src,
	size_t len,
	SSLBuffer *dst);		// data mallocd and returned 
OSStatus SSLCopyBuffer(
	const SSLBuffer *src, 
	SSLBuffer *dst);		// data mallocd and returned 

#ifdef __cplusplus
}
#endif

#endif
