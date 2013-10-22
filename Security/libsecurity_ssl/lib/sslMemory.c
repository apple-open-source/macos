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

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslMemory.h"
#include "sslDebug.h"

#include <string.h>			/* memset */
#include <AssertMacros.h>

// MARK: -
// MARK: Basic low-level malloc/free

/*
 * For now, all allocs/frees go thru here.
 */

#ifdef KERNEL

/* BSD Malloc */
#include <sys/malloc.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>

/* Define this for debugging sslMalloc and sslFree */
//#define SSL_CANARIS

void *
sslMalloc(size_t length)
{
    void *p;

#ifdef SSL_CANARIS
    length+=8;
#endif
    
    p = _MALLOC(length, M_TEMP, M_WAITOK);
    check(p);
    
    if(p==NULL)
        return p;
    
#ifdef SSL_CANARIS
    *(uint32_t *)p=(uint32_t)length-8;
    printf("sslMalloc @%p of 0x%08lx bytes\n", p, length-8);
    *(uint32_t *)(p+length-4)=0xdeadbeed;
    p+=4;
#endif

    return p;
}

void
sslFree(void *p)
{
	if(p != NULL) {

#ifdef SSL_CANARIS
        p=p-4;
        uint32_t len=*(uint32_t *)p;
        uint32_t marker=*(uint32_t *)(p+4+len);
        printf("sslFree @%p len=0x%08x\n", p, len);
        if(marker!=0xdeadbeef)
            panic("Buffer overflow in SSL!\n");
#endif
        
        _FREE(p, M_TEMP);
	}
}

void *
sslRealloc(void *oldPtr, size_t oldLen, size_t newLen)
{
    /* _REALLOC is in sys/malloc.h but is only exported in debug kernel */
    /* return _REALLOC(oldPtr, newLen, M_TEMP, M_NOWAIT); */

    /* FIXME */
    void *newPtr;
    if(newLen>oldLen) {
        newPtr=sslMalloc(newLen);
        if(newPtr) {
            memcpy(newPtr, oldPtr, oldLen);
            sslFree(oldPtr);
        }
    } else {
        newPtr=oldPtr;
    }
    return newPtr;
}

#else

#include <stdlib.h>

void *
sslMalloc(size_t length)
{
	return malloc(length);
}

void
sslFree(void *p)
{   
	if(p != NULL) {
		free(p);
	}
}

void *
sslRealloc(void *oldPtr, size_t oldLen, size_t newLen)
{
	return realloc(oldPtr, newLen);
}

#endif

// MARK: -
// MARK: SSLBuffer-level alloc/free

int SSLAllocBuffer(
	SSLBuffer *buf,
	size_t length)
{
	buf->data = (uint8_t *)sslMalloc(length);
	if(buf->data == NULL) {
        sslErrorLog("SSLAllocBuffer: NULL buf!\n");
        check(0);
		buf->length = 0;
		return -1;
	}
    buf->length = length;
    return 0;
}

int
SSLFreeBuffer(SSLBuffer *buf)
{   
	if(buf == NULL) {
		sslErrorLog("SSLFreeBuffer: NULL buf!\n");
        check(0);
		return -1;
	}
    sslFree(buf->data);
    buf->data = NULL;
    buf->length = 0;
    return 0;
}

int
SSLReallocBuffer(SSLBuffer *buf, size_t newSize)
{   
	buf->data = (uint8_t *)sslRealloc(buf->data, buf->length, newSize);
	if(buf->data == NULL) {
        sslErrorLog("SSLReallocBuffer: NULL buf!\n");
        check(0);
		buf->length = 0;
		return -1;
	}
	buf->length = newSize;
	return 0;
}

// MARK: -
// MARK: Convenience routines

uint8_t *sslAllocCopy(
	const uint8_t *src,
	size_t len)
{
	uint8_t *dst;
	
	dst = (uint8_t *)sslMalloc(len);
	if(dst == NULL) {
		return NULL;
	}
	memmove(dst, src, len);
	return dst;
}

int SSLAllocCopyBuffer(
	const SSLBuffer *src, 
	SSLBuffer **dst)		// buffer and data mallocd and returned 
{   
	int serr;
	
	SSLBuffer *rtn = (SSLBuffer *)sslMalloc(sizeof(SSLBuffer));
	if(rtn == NULL) {
        sslErrorLog("SSLAllocCopyBuffer: NULL buf!\n");
        check(0);
		return -1;
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

int SSLCopyBufferFromData(
	const void *src,
	size_t len,
	SSLBuffer *dst)		// data mallocd and returned 
{   
	dst->data = sslAllocCopy((const uint8_t *)src, len);
	if(dst->data == NULL) {
        sslErrorLog("SSLCopyBufferFromData: NULL buf!\n");
        check(0);
		return -1;
	}
    dst->length = len;
    return 0;
}

int SSLCopyBuffer(
	const SSLBuffer *src, 
	SSLBuffer *dst)		// data mallocd and returned 
{   
	return SSLCopyBufferFromData(src->data, src->length, dst);
}

