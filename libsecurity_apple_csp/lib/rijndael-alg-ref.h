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


/* rijndael-alg-ref.h   v2.0   August '99
 * Reference ANSI C code
 * authors: Paulo Barreto
 *          Vincent Rijmen
 */
#ifndef __RIJNDAEL_ALG_H
#define __RIJNDAEL_ALG_H

#include "aesCommon.h"

#define MAXBC				(MAX_AES_BLOCK_BITS/32)
#define MAXKC				(MAX_AES_KEY_BITS/32)
#define MAXROUNDS			14

#ifdef	__cplusplus
extern "C" {
#endif

typedef unsigned char		word8;	
typedef unsigned short		word16;	
typedef unsigned int		word32;


int rijndaelKeySched (word8 k[4][MAXKC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC]);
int rijndaelEncrypt (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC]);
#ifndef	__APPLE__
int rijndaelEncryptRound (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC], int rounds);
#endif
int rijndaelDecrypt (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC]);
#ifndef	__APPLE__
int rijndaelDecryptRound (word8 a[4][MAXBC], int keyBits, int blockBits, 
		word8 rk[MAXROUNDS+1][4][MAXBC], int rounds);
#endif

#if		!GLADMAN_AES_128_ENABLE

/*
 * Optimized routines for 128-bit block and key.
 */
#define ROUNDS_128_OPT	10
#define BC_128_OPT		4
#define KC_128_OPT		4

/*
 * These require 32-bit word-aligned a, k, and rk arrays 
 */
int rijndaelKeySched128 (word8 k[4][KC_128_OPT], 
	word8 rk[MAXROUNDS+1][4][MAXBC]);
int rijndaelEncrypt128 (word8 a[4][BC_128_OPT], 
	word8 rk[MAXROUNDS+1][4][MAXBC]);
int rijndaelDecrypt128 (word8 a[4][BC_128_OPT], 
	word8 rk[MAXROUNDS+1][4][MAXBC]);

#endif		/* !GLADMAN_AES_128_ENABLE */

#ifdef	__cplusplus
}
#endif

#endif /* __RIJNDAEL_ALG_H */
