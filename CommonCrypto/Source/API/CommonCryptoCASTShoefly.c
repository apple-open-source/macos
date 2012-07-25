/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

// #define COMMON_CASTSHOEFLY_FUNCTIONS

#include <stdio.h>
#include "lionCompat.h"
#include "CommonCryptorSPI.h"
#define DIAGNOSTIC

#include "ccdebug.h"

void CAST_set_key(CAST_KEY *key, int len, const unsigned char *data)
{
    CCCryptorRef *encCryptorRef;
    size_t dataUsed;
    
    encCryptorRef = key->cref;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    (void) CCCryptorCreateWithMode(kCCBoth, kCCModeECB, kCCAlgorithmCAST, ccNoPadding, NULL, data, len, NULL, 0, 0, 0, encCryptorRef);
}

void CAST_ecb_encrypt(const unsigned char *in,unsigned char *out, CAST_KEY *key, int enc)
{
    size_t moved;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Op: %d\n", enc);
    if(enc)
        CCCryptorEncryptDataBlock(key->cref, NULL, in, kCCBlockSizeCAST, out);
    else
        CCCryptorDecryptDataBlock(key->cref, NULL, in, kCCBlockSizeCAST, out);
    
}

