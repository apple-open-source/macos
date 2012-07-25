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
 *  vng_neon_sha1.h
 *  MacTomCrypt
 *
 *  InfoSec Standard Configuration
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include <stdint.h>
#include <arm/arch.h>

#if defined(_ARM_ARCH_7)

#ifndef _VNG_NEON_SHA1_H_
#define _VNG_NEON_SHA1_H_

#define VNG_NEON_SHA1_HASHSIZE	20
#define VNG_NEON_SHA1_BLOCKSIZE	64

typedef struct vng_neon_sha1_state {
    uint32_t state[5];
    uint64_t length;
    unsigned char buf[VNG_NEON_SHA1_BLOCKSIZE];
    int curlen;
} vng_neon_sha1_ctx;

int vng_neon_sha1_init(vng_neon_sha1_ctx *ctx);
int vng_neon_sha1_process(vng_neon_sha1_ctx *ctx, const unsigned char *in,
    unsigned long inlen);
int vng_neon_sha1_done(vng_neon_sha1_ctx *ctx, unsigned char *hash);
void sha1_vng_armv7neon_compress(uint32_t state[], unsigned long nblocks, unsigned char *);
#endif /* _ARM_ARCH_7 */
#endif /* _VNG_NEON_SHA1_H_ */

