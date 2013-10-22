/*
 * Copyright (c) 2002,2005-2007,2010-2011 Apple Inc. All Rights Reserved.
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

#ifndef	_TLS_HASHHMAC_H_
#define _TLS_HASHHMAC_H_

#include "tls_digest.h"
#include "tls_hmac.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * TLS addenda:
 *	-- new struct HashHmacReference
 *	-- structs which used to use HashReference now use HashHmacReference
 *	-- new union HashHmacContext, used in CipherContext.
 */

typedef struct {
    const HashReference	*hash;
    const HMACReference	*hmac;
} HashHmacReference;

typedef union {
    SSLBuffer			hashCtx;
    HMACContextRef		hmacCtx;
} HashHmacContext;

/* these are declared in tls_hmac.c */
extern const HashHmacReference HashHmacNull;
extern const HashHmacReference HashHmacMD5;
extern const HashHmacReference HashHmacSHA1;
extern const HashHmacReference HashHmacSHA256;
extern const HashHmacReference HashHmacSHA384;

#ifdef	__cplusplus
}
#endif
#endif	/* _TLS_HMAC_H_ */
