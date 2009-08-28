/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
	File:		tls_hmac.h

	Contains:	Declarations of HMAC routines used by TLS

	Written by:	Doug Mitchell
*/

#ifndef	_TLS_HMAC_H_
#define _TLS_HMAC_H_

#ifdef	__cplusplus
extern "C" {
#endif	

#include "ssl.h"
#include "sslPriv.h"

/* forward declaration of HMAC object */
struct 						HMACReference;

/* Opaque reference to an HMAC session context */
struct                      HMACContext;
typedef struct HMACContext  *HMACContextRef;

/* The HMAC algorithms we support */
typedef enum {
	HA_Null = 0,		// i.e., uninitialized
	HA_SHA1,
	HA_MD5
} HMAC_Algs;

/* For convenience..the max size of HMAC, in bytes, this module will ever return */
#define TLS_HMAC_MAX_SIZE		20

/* Create an HMAC session */
typedef OSStatus (*HMAC_AllocFcn) (
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	unsigned					keyLen,
	HMACContextRef				*hmacCtx);			// RETURNED
	
/* Free a session */
typedef OSStatus (*HMAC_FreeFcn) (
	HMACContextRef	hmacCtx);	
	
/* Reusable init, using same key */
typedef OSStatus (*HMAC_InitFcn) (
	HMACContextRef	hmacCtx);
	
/* normal crypt ops */
typedef OSStatus (*HMAC_UpdateFcn) (
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen);
	
typedef OSStatus (*HMAC_FinalFcn) (
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen);		// IN/OUT
	
/* one-shot */
typedef OSStatus (*HMAC_HmacFcn) (
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen);		// IN/OUT
	
typedef struct HMACReference {
	UInt32			macSize;
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

#ifdef	__cplusplus
}
#endif	
#endif	/* _TLS_HMAC_H_ */
