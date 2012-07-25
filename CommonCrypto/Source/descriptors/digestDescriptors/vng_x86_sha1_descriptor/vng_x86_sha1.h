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
 *  vng_x86_sha1.h
 *  MacTomCrypt
 *
 *  InfoSec Standard Configuration
 *  Copyright 2011 Apple Inc. All rights reserved.
 *
 */

#include <stdint.h>

#if defined (__x86_64__) || defined(__i386__)		// x86_64 or i386 architectures

#ifndef _VNG_X86_SHA1_H_
#define _VNG_X86_SHA1_H_

#define VNG_X86_SHA1_HASHSIZE	20
#define VNG_X86_SHA1_BLOCKSIZE	64

// Adjusted to take the same space as CC_SHA1_CTX
// The order is important on a 64 bit system so that
// the length variable is 64 bit aligned.

typedef struct vng_x86_sha1_state {
    uint32_t state[5];                            // 20
    uint32_t curlen;                              //  4
    uint64_t length;                              //  8
    unsigned char buf[VNG_X86_SHA1_BLOCKSIZE];    // 64
} vng_x86_sha1_ctx;

int vng_x86_sha1_init(vng_x86_sha1_ctx *ctx);
int vng_x86_sha1_process(vng_x86_sha1_ctx *ctx, const unsigned char *in,
    unsigned long inlen);
int vng_x86_sha1_done(vng_x86_sha1_ctx *ctx, unsigned char *hash);

// I think the x86 version takes the blocks as the third argument.
void sha1_x86_compress_data_order(uint32_t state[], unsigned char *, unsigned long nblocks);
#endif /* x86 */
#endif /* _VNG_X86_SHA1_H_ */

