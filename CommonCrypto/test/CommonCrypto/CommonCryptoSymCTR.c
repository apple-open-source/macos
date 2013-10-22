/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"

#if (CCSYMCTR == 0)
entryPoint(CommonCryptoSymCTR,"CommonCrypto Symmetric CTR Testing")
#else
static int kTestTestCount = 10;

static CCCryptorStatus doCrypt(char *in, char *out, CCCryptorRef cryptor) {
    byteBuffer inbb = hexStringToBytes(in);
    byteBuffer outbb = hexStringToBytes(out);
    byteBuffer buf = mallocByteBuffer(64);
    CCCryptorStatus retval = CCCryptorUpdate(cryptor, inbb->bytes, inbb->len, buf->bytes, buf->len, &buf->len);
    if(!retval) {
        ok(bytesAreEqual(outbb, buf), "crypt results are equal");
    }
    return retval;
}

int CommonCryptoSymCTR(int argc, char *const *argv)
{
    CCCryptorStatus retval;
    CCCryptorRef cryptor;
    
	plan_tests(kTestTestCount);
    
    byteBuffer key = hexStringToBytes("2b7e151628aed2a6abf7158809cf4f3c");
    byteBuffer counter = hexStringToBytes("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    
    retval = CCCryptorCreateWithMode(kCCEncrypt, kCCModeCTR, kCCAlgorithmAES128, 
                                     ccNoPadding, counter->bytes, key->bytes, key->len, 
                                     NULL, 0, 0, kCCModeOptionCTR_LE, &cryptor);

    
    ok(retval == kCCUnimplemented, "CTR Mode Encrypt unavailable for kCCModeOptionCTR_LE");

    retval = CCCryptorCreateWithMode(kCCEncrypt, kCCModeCTR, kCCAlgorithmAES128, 
                                     ccNoPadding, counter->bytes, key->bytes, key->len, 
                                     NULL, 0, 0, kCCModeOptionCTR_BE, &cryptor);
                                     
    ok(retval == kCCSuccess, "CTR Mode Encrypt");

    retval = doCrypt("6bc1bee22e409f96e93d7e117393172a",
                     "874d6191b620e3261bef6864990db6ce",
                     cryptor);
   
    
    ok(retval == kCCSuccess, "CTR Mode Encrypt");

    retval = doCrypt("ae2d8a571e03ac9c9eb76fac45af8e51",
                     "9806f66b7970fdff8617187bb9fffdff",
                     cryptor);
    
    
    ok(retval == kCCSuccess, "CTR Mode Encrypt");

    retval = doCrypt("30c81c46a35ce411e5fbc1191a0a52ef",
                     "5ae4df3edbd5d35e5b4f09020db03eab",
                     cryptor);
    
    ok(retval == kCCSuccess, "CTR Mode Encrypt");

    retval = doCrypt("f69f2445df4f9b17ad2b417be66c3710",
                     "1e031dda2fbe03d1792170a0f3009cee",
                     cryptor);
    
    ok(retval == kCCSuccess, "CTR Mode Encrypt");
    

    return 0;
}
#endif
