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
	File:		symCipher.h

	Contains:	CDSA-based symmetric cipher module

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_SYM_CIPHER_H_
#define _SYM_CIPHER_H_

#include "sslContext.h"
#include "cryptType.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All symmetric cipher logic goes thru these same four routines, on the 
 * way down to CDSA
 */
OSStatus CDSASymmInit(
	uint8 *key, 
	uint8* iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
OSStatus CDSASymmEncrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
OSStatus CDSASymmDecrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx);
OSStatus CDSASymmFinish(
	CipherContext *cipherCtx, 
	SSLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif	/* _SYM_CIPHER_H_ */
