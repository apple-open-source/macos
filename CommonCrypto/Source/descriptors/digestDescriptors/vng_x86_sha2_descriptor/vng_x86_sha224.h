/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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
 *  vng_x86_sha224.h
 *  MacTomCrypt
 *
 *  InfoSec Standard Configuration
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */


#if defined (__x86_64__) || defined(__i386__)		// x86_64 or i386 architectures

#ifndef _VNG_X86_SHA224_H_
#define _VNG_X86_SHA224_H_

/*
 * Note that vng_x86_sha256 is required for vng_x86_sha224.
 */

#define	VNG_X86_SHA224_HASHSIZE	28
#define	VNG_X86_SHA224_BLOCKSIZE	64

int vng_x86_sha224_init(vng_x86_sha256_ctx *ctx);
#define vng_x86_sha224_process vng_x86_sha256_process
int vng_x86_sha224_done(vng_x86_sha256_ctx *ctx, unsigned char *hash);

#endif /* _VNG_X86_SHA224_H_ */
#endif /* x86 */
