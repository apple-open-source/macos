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

/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */

#ifndef LTC_HASHCOMMON_H_
#define LTC_HASHCOMMON_H_

/* a simple macro for making hash "process" functions */
#define LTC_HASH_PROCESS(func_name, compress_name, state_var, field, block_size) \
int func_name (state_var *ctx, const unsigned char *in, unsigned long inlen) \
{                                                                             \
unsigned long n;                                                          \
int           err;                                                        \
\
\
LTC_ARGCHK(ctx != NULL);                                                   \
LTC_ARGCHK(in != NULL);                                                   \
\
\
if (ctx->curlen > sizeof(ctx->buf)) {                             	      \
return CRYPT_INVALID_ARG;					      \
}									      \
if ((ctx->length + inlen) < ctx->length) {				      \
return CRYPT_HASH_OVERFLOW;					      \
}									      \
while (inlen > 0) {							      \
if (ctx->curlen == 0 && inlen >= block_size) {			      \
if ((err = compress_name (ctx, in)) != CRYPT_OK) {		      \
return err;						      \
}								      \
ctx->length    += block_size * 8;				      \
in             += block_size;				      \
inlen          -= block_size;				      \
} else {							      \
n = MIN(inlen, (block_size - ctx->curlen));			      \
memcpy(ctx->buf + ctx->curlen, in, (size_t)n);		      \
ctx->curlen += n;						      \
in             += n;						      \
inlen          -= n;						      \
if (ctx->curlen == block_size) {				      \
if ((err = compress_name (ctx, ctx->buf)) != CRYPT_OK) {	      \
return err;						      \
}								      \
ctx->length += 8*block_size;				      \
ctx->curlen = 0;						      \
}								      \
}								      \
} 									      \
return CRYPT_OK;							      \
}

#endif /* LTC_HASHCOMMON_H_ */
