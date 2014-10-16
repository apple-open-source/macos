/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * tls_digest.h - HashReference declarations
 */

#ifndef	_TLS_DIGEST_H_
#define _TLS_DIGEST_H_	1

#include "sslTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These numbers show up all over the place...might as well hard code 'em once.
 */
#define SSL_MD5_DIGEST_LEN      16
#define SSL_SHA1_DIGEST_LEN     20
#define SSL_SHA256_DIGEST_LEN	32
#define SSL_SHA384_DIGEST_LEN	48
#define SSL_MAX_DIGEST_LEN      48 /* >= SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN */

#define MAX_MAC_PADDING         48	/* MD5 MAC padding size = 48 bytes */

extern const uint8_t SSLMACPad1[], SSLMACPad2[];

typedef int (*HashInit)(SSLBuffer *digestCtx);
typedef int (*HashUpdate)(SSLBuffer *digestCtx, const SSLBuffer *data);
/* HashFinal also does HashClose */
typedef int (*HashFinal)(SSLBuffer *digestCtx, SSLBuffer *digest);
typedef int (*HashClose)(SSLBuffer *digestCtx);
typedef int (*HashClone)(const SSLBuffer *src, SSLBuffer *dest);

typedef struct
{
    uint32_t    digestSize;
    uint32_t    macPadSize;
    uint32_t    contextSize;
    HashInit    init;
    HashUpdate  update;
    HashFinal   final;
    HashClose	close;
    HashClone   clone;
} HashReference;

extern const HashReference SSLHashNull;
extern const HashReference SSLHashMD5;
extern const HashReference SSLHashSHA1;
extern const HashReference SSLHashSHA256;
extern const HashReference SSLHashSHA384;


#ifdef __cplusplus
}
#endif

#endif	/* _TLS_DIGEST_H_ */
