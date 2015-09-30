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
 * sslMemory.h - tls_buffer and Memory allocator declarations
 */

/* This header should be kernel safe */

#ifndef _SSLMEMORY_H_
#define _SSLMEMORY_H_ 1

#include "tls_types.h"

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
 * tls_buffer-oriented allocators
 */
int SSLAllocBuffer(tls_buffer *buf, size_t length);
int SSLFreeBuffer(tls_buffer *buf);
int SSLReallocBuffer(tls_buffer *buf, size_t newSize);

int tls_free_buffer_list(tls_buffer_list_t *list);
int tls_copy_buffer_list(const tls_buffer_list_t *src, tls_buffer_list_t **dst);

/*
 * Convenience routines
 */
uint8_t *sslAllocCopy(const uint8_t *src, size_t len);
int SSLAllocCopyBuffer(
	const tls_buffer *src, 
	tls_buffer **dst);		// buffer itself and data mallocd and returned 
int SSLCopyBufferFromData(
	const void *src,
	size_t len,
	tls_buffer *dst);		// data mallocd and returned 
int SSLCopyBuffer(
	const tls_buffer *src, 
	tls_buffer *dst);		// data mallocd and returned
int SSLCopyBufferTerm(
    const void *src,
    size_t len,
    tls_buffer *dst);		// data mallocd and returned

#ifdef __cplusplus
}
#endif

#define SET_SSL_BUFFER(buf, d, l)   do { (buf).data = (d); (buf).length = (l); } while (0)

#endif
