/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
 * rijndaelApi.h  -  AES API layer
 *
 * Based on rijndael-api-ref.h v2.0 written by Paulo Barreto
 * and Vincent Rijmen
 */

#ifndef	_RIJNDAEL_API_REF_H_
#define _RIJNDAEL_API_REF_H_

#include <stdio.h>
#include "rijndael-alg-ref.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*  Error Codes  */
#define     BAD_KEY_MAT        -1  /*  Key material not of correct 
									   length */
#define     BAD_KEY_INSTANCE   -2  /*  Key passed is not valid  */

#define     MAX_AES_KEY_SIZE	(MAX_AES_KEY_BITS / 8)
#define 	MAX_AES_BLOCK_SIZE	(MAX_AES_BLOCK_BITS / 8)
#define     MAX_AES_IV_SIZE		MAX_AES_BLOCK_SIZE
	
#define		TRUE		1
#define 	FALSE		0

/*  The structure for key information */
typedef struct {
	word32   		keyLen;		/* Length of the key in bits */
	word32  		blockLen;   /* Length of block in bits */
	word32			columns;	/* optimization, blockLen / 32 */
	word8 			keySched[MAXROUNDS+1][4][MAXBC];	
} keyInstance;

int makeKey(
	keyInstance *key, 
	int keyLen, 		// in BITS
	int blockLen,		// in BITS
	word8 *keyMaterial,
	int enable128Opt);

/*
 * Simplified single-block encrypt/decrypt.
 */
int rijndaelBlockEncrypt(
	keyInstance *key, 
	word8 *input, 
	word8 *outBuffer);
int rijndaelBlockDecrypt(
	keyInstance *key, 
	word8 *input, 
	word8 *outBuffer);
	
#if		!GLADMAN_AES_128_ENABLE
/*
 * Optimized routines for 128 bit block and 128 bit key.
 */
int rijndaelBlockEncrypt128(
	keyInstance 	*key, 
	word8 			*input, 
	word8 			*outBuffer);
int rijndaelBlockDecrypt128(
	keyInstance 	*key, 
	word8 			*input, 
	word8 			*outBuffer);
#endif	/* !GLADMAN_AES_128_ENABLE */

#if defined(__ppc__) && defined(ALTIVEC_ENABLE)
/* 
 * dmitch addenda 4/11/2001: 128-bit only vectorized encrypt/decrypt with no CBC
 */
void vBlockEncrypt128(
	keyInstance *key, 
	word8 *input, 
	word8 *outBuffer);
void vBlockDecrypt128(
	keyInstance *key, 
	word8 *input, 
	word8 *outBuffer);

/* temp switch for runtime enable/disable */
extern int doAES128;

#endif	/* __ppc__ && ALTIVEC_ENABLE */
	
/* ptr to one of several (possibly optimized) encrypt/decrypt functions */
typedef int (*aesCryptFcn)(
	keyInstance *key, 
	word8 *input, 
	word8 *outBuffer);

#ifdef	__cplusplus
}
#endif	// cplusplus

#endif	// RIJNDAEL_API_REF


