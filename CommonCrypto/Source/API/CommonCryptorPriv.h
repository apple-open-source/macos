/* 
 * Copyright (c) 2006-2010 Apple, Inc. All Rights Reserved.
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
 * CommonCryptorPriv.h - interface between CommonCryptor and operation- and
 *           algorithm-specific service providers. 
 */

#ifndef _CC_COMMON_CRYPTOR_PRIV_
#define _CC_COMMON_CRYPTOR_PRIV_

#include "CommonCryptor.h"
#include "CommonCryptorSPI.h"
#include <dispatch/dispatch.h>
#include "corecryptoSymmetricBridge.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    /* Byte-Size Constants */
#define CCMAXBUFFERSIZE 128             /* RC2/RC5 Max blocksize */
#define DEFAULT_CRYPTOR_MALLOC 4096
#define CC_STREAMKEYSCHED  2048
#define CC_MODEKEYSCHED  2048
#define CC_MAXBLOCKSIZE  128
    
typedef struct _CCCryptor {
    uint8_t        buffptr[32];
    uint32_t        bufferPos;
    uint32_t        bytesProcessed;
    uint32_t        cipherBlocksize;

    CCAlgorithm     cipher;
    CCMode          mode;
    CCOperation     op;        /* kCCEncrypt, kCCDecrypt, or kCCBoth */
    
    corecryptoMode  symMode[2];
    cc2CCModeDescriptor *modeDesc;
    modeCtx         ctx[2];
    cc2CCPaddingDescriptor *padptr;
} CCCryptor;
    

typedef struct _CCCompat {
    uint32_t			weMallocd;
    CCCryptor			*cryptor;
} CCCompatCryptor;

    
#define CCCRYPTOR_SIZE  sizeof(struct _CCCryptor)
#define kCCContextSizeGENERIC (sizeof(CCCompatCryptor))


    corecryptoMode getCipherMode(CCAlgorithm cipher, CCMode mode, CCOperation direction);

#ifdef __cplusplus
}
#endif

#endif  /* _CC_COMMON_CRYPTOR_PRIV_ */
