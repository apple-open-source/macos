/*
 * Copyright (c) 2002,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * tls_hmac.h - Declarations of HMAC routines used by TLS
 */

#ifndef	_TLS_HMAC_H_
#define _TLS_HMAC_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include "cipherSpecs.h"

/* forward declaration of HMAC object */
struct 						HMACReference;

/* Opaque reference to an HMAC session context */
struct                      HMACContext;
typedef struct HMACContext  *HMACContextRef;


/* For convenience..the max size of HMAC, in bytes, this module will ever return */
#define TLS_HMAC_MAX_SIZE		48

/* Create an HMAC session */
typedef int (*HMAC_AllocFcn) (
	const struct HMACReference	*hmac,
	const void					*keyPtr,
	size_t                      keyLen,
	HMACContextRef				*hmacCtx);			// RETURNED

/* Free a session */
typedef int (*HMAC_FreeFcn) (
	HMACContextRef	hmacCtx);	
	
/* Reusable init, using same key */
typedef int (*HMAC_InitFcn) (
	HMACContextRef	hmacCtx);

/* normal crypt ops */
typedef int (*HMAC_UpdateFcn) (
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen);
	
typedef int (*HMAC_FinalFcn) (
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen);		// IN/OUT

/* one-shot */
typedef int (*HMAC_HmacFcn) (
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen);		// IN/OUT
	

typedef struct HMACParams {
} HMACParams;

typedef struct HMACReference {
    size_t          macSize;
    HMAC_Algs		alg;
	HMAC_AllocFcn	alloc;
	HMAC_FreeFcn	free;
	HMAC_InitFcn	init;
	HMAC_UpdateFcn	update;
	HMAC_FinalFcn	final;
	HMAC_HmacFcn	hmac;
} HMACReference;

extern const HMACReference TlsHmacNull;
extern const HMACReference TlsHmacSHA1;
extern const HMACReference TlsHmacMD5;
extern const HMACReference TlsHmacSHA256;
extern const HMACReference TlsHmacSHA384;


#ifdef	__cplusplus
}
#endif
#endif	/* _TLS_HMAC_H_ */
