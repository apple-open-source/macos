/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * symCipher.h - symmetric cipher module
 */

#ifndef	_SYM_CIPHER_H_
#define _SYM_CIPHER_H_

#include <sys/types.h>
#include <stdint.h>
#include "cipherSpecs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_SECRET_LEN 	48	/* master secret = 3 x MD5 hashes concatenated */

/* SSL V2 - mac secret is the size of symmetric key, not digest */
#define MAX_SYMKEY_SIZE		24

typedef enum
{
    streamCipherType,
    blockCipherType,
    aeadCipherType
} CipherType;

typedef struct  {
    SSL_CipherAlgorithm keyAlg;
    CipherType          cipherType;
    uint8_t           	keySize;            /* Sizes are in bytes */
    uint8_t           	ivSize;
    uint8_t          	blockSize;
} SSLSymmetricCipherParams;


/* All symmetric ciphers go thru these callouts. */
struct SymCipherContext;
typedef struct SymCipherContext *SymCipherContext;

typedef int (*SSLKeyFunc)(
                               const SSLSymmetricCipherParams *params,
                               int encrypting,
                               uint8_t *key,
                               uint8_t *iv,
                               SymCipherContext *cipherCtx);
typedef int (*SSLSetIVFunc)(
                                 const uint8_t *iv,
                                 size_t len,
                                 SymCipherContext cipherCtx);
typedef int (*SSLAddADD)(
                              const uint8_t *src,
                              size_t len,
                              SymCipherContext cipherCtx);
typedef int (*SSLCryptFunc)(
                                 const uint8_t *src,
                                 uint8_t *dest,
                                 size_t len,
                                 SymCipherContext cipherCtx);
typedef int (*SSLFinishFunc)(
                                  SymCipherContext cipherCtx);
typedef int (*SSLAEADDoneFunc)(
                                    uint8_t *mac,
                                    size_t *macLen,
                                    SymCipherContext cipherCtx);

/* Statically defined description of a symmetric cipher. */
typedef struct {
    SSLKeyFunc      	initialize;
    SSLCryptFunc    	encrypt;
    SSLCryptFunc    	decrypt;
} Cipher;

typedef struct {
    SSLKeyFunc      	initialize;
    SSLSetIVFunc        setIV;
    SSLAddADD           update;
    SSLCryptFunc    	encrypt;
    SSLCryptFunc    	decrypt;
    SSLAEADDoneFunc     done;
    uint8_t          	macSize;
} AEADCipher;


typedef struct SSLSymmetricCipher {
    const SSLSymmetricCipherParams *params;
    SSLFinishFunc   	finish;
    union {
        const Cipher    cipher;  /* stream or block cipher type */
        const AEADCipher aead;   /* aeadCipherType */
    } c;
} SSLSymmetricCipher;

extern const SSLSymmetricCipher SSLCipherNull;
extern const SSLSymmetricCipher SSLCipherRC2_40;
extern const SSLSymmetricCipher SSLCipherRC2_128;
extern const SSLSymmetricCipher SSLCipherRC4_40;
extern const SSLSymmetricCipher SSLCipherRC4_128;
extern const SSLSymmetricCipher SSLCipherDES40_CBC;
extern const SSLSymmetricCipher SSLCipherDES_CBC;
extern const SSLSymmetricCipher SSLCipher3DES_CBC;
extern const SSLSymmetricCipher SSLCipherAES_128_CBC;
extern const SSLSymmetricCipher SSLCipherAES_256_CBC;
extern const SSLSymmetricCipher SSLCipherAES_128_GCM;
extern const SSLSymmetricCipher SSLCipherAES_256_GCM;

/* Those are defined in symCipherParams.c */
extern const SSLSymmetricCipherParams SSLCipherNullParams;
extern const SSLSymmetricCipherParams SSLCipherRC2_40Params;
extern const SSLSymmetricCipherParams SSLCipherRC2_128Params;
extern const SSLSymmetricCipherParams SSLCipherRC4_40Params;
extern const SSLSymmetricCipherParams SSLCipherRC4_128Params;
extern const SSLSymmetricCipherParams SSLCipherDES40_CBCParams;
extern const SSLSymmetricCipherParams SSLCipherDES_CBCParams;
extern const SSLSymmetricCipherParams SSLCipher3DES_CBCParams;
extern const SSLSymmetricCipherParams SSLCipherAES_128_CBCParams;
extern const SSLSymmetricCipherParams SSLCipherAES_256_CBCParams;
extern const SSLSymmetricCipherParams SSLCipherAES_128_GCMParams;
extern const SSLSymmetricCipherParams SSLCipherAES_256_GCMParams;

#ifdef __cplusplus
}
#endif

#endif	/* _SYM_CIPHER_H_ */
