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
	File:		sslalloc.h

	Contains:	memory allocator declarations

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslalloc.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslalloc.h   Allocation shell routines

    These routines wrap the user-supplied callbacks to provide allocation
    functionality.

    ****************************************************************** */

#ifndef _SSLALLOC_H_
#define _SSLALLOC_H_ 1

#include "sslctx.h"
#include "sslerrs.h"
#include "sslPriv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * General purpose allocators
 */
void *sslMalloc(UInt32 length);
void sslFree(void *p);
void *sslRealloc(void *oldPtr, UInt32 oldLen, UInt32 newLen);

/*
 * SSLBuffer-oriented allocators
 */
SSLErr SSLAllocBuffer(SSLBuffer *buf, UInt32 length, const SystemContext *ctx);
SSLErr SSLFreeBuffer(SSLBuffer *buf, const SystemContext *ctx);
SSLErr SSLReallocBuffer(SSLBuffer *buf, UInt32 newSize, const SystemContext *ctx);

/*
 * Set up/tear down CF allocators.
 */
OSStatus cfSetUpAllocators(SSLContext *ctx);
void cfTearDownAllocators(SSLContext *ctx);

/*
 * Convenience routines.
 */
UInt8 *sslAllocCopy(const UInt8 *src, UInt32 len);

#ifdef __cplusplus
}
#endif

#endif
