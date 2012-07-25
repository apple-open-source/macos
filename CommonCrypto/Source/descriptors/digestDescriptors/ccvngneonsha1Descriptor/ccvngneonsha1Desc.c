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

#if defined(__ARM_ARCH_7A__)
#include "ccvngneonsha1Desc.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "CommonDigest.h"

const ccDescriptor cc_vngneonsha1_desc =
{
    .implementation_info = &cc_sha1_impinfo,
	.dtype.digest.hashsize = CC_SHA1_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA1_BLOCK_BYTES,
    .dtype.digest.digest_info = &cc_vngneonsha1_di,
	.dtype.digest.init = &cc_vngneon_sha1_init,
	.dtype.digest.process = &cc_vngneon_sha1_process,
	.dtype.digest.done = &cc_vngneon_sha1_done,
};


int 
cc_vngneon_sha1_init(cc_sha1_ctx *md_ctx) {
    ccdigest_init(cc_vngneonsha1_di, md_ctx);
    return CRYPT_OK;
}

int 
cc_vngneon_sha1_process(cc_sha1_ctx *md_ctx, const unsigned char *in,
                   unsigned long inlen) {
    ccdigest_update(cc_vngneonsha1_di, md_ctx, inlen, in);
    return CRYPT_OK;
}    

int cc_vngneon_sha1_done(cc_sha1_ctx *md_ctx, unsigned char *hash) {
    ccdigest_final(cc_vngneonsha1_di, md_ctx, hash);
    return CRYPT_OK;
}
#endif

