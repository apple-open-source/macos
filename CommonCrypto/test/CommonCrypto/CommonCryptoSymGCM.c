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

/*
 *  CCGCMTest.c
 *  CommonCrypto
 */

#include "capabilities.h"
#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include "CCCryptorTestFuncs.h"
#include "testbyteBuffer.h"
#include "testmore.h"

#if (CCSYMGCM == 0)
entryPoint(CommonCryptoSymGCM,"CommonCrypto Symmetric GCM Testing")
#else




static int kTestTestCount = 16;

int CommonCryptoSymGCM(int argc, char *const *argv) {
	char *keyStr;
	char *iv;
	char *plainText;
	char *cipherText;
    char *adata;
    char *tag;
    CCAlgorithm alg;
	int retval, accum = 0;

    alg		   = kCCAlgorithmAES128;
    
	plan_tests(kTestTestCount);
    
    /* testcase #1 */

    keyStr =     "00000000000000000000000000000000";
    adata =      "";
    iv =         "000000000000000000000000";
    plainText =  "";
    cipherText = "";
    tag =        "58e2fccefa7e3061367f1d57a4e7455a";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    
    ok(retval == 0, "AES-GCM Testcase 1.1");
    accum += retval;
    
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    
    ok(retval == 0, "AES-GCM Testcase 1.2");
    accum += retval;
    
    /* testcase #2 */

    keyStr =     "00000000000000000000000000000000";
    adata =      "";
    iv =         "000000000000000000000000";
    plainText =  "00000000000000000000000000000000";
    cipherText = "0388dace60b6a392f328c2b971b2fe78";
    tag =        "ab6e47d42cec13bdf53a67b21257bddf";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 2.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 2.2");
    accum += retval;

    /* testcase #3 */

    keyStr =     "feffe9928665731c6d6a8f9467308308";
    adata =      "";
    iv =         "cafebabefacedbaddecaf888";
    plainText =  "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255";
    cipherText = "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985";
    tag =        "4d5c2af327cd64a62cf35abd2ba6fab4";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 3.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 3.2");
    accum += retval;

    /* testcase #4 */

    keyStr =     "feffe9928665731c6d6a8f9467308308";
    adata =      "feedfacedeadbeeffeedfacedeadbeefabaddad2";
    iv =         "cafebabefacedbaddecaf888";
    plainText =  "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
    cipherText = "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091";
    tag =        "5bc94fbc3221a5db94fae95ae7121a47";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 4.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 4.2");
    accum += retval;

    /* testcase #5 */

    keyStr =     "feffe9928665731c6d6a8f9467308308";
    adata =      "feedfacedeadbeeffeedfacedeadbeefabaddad2";
    iv =         "cafebabefacedbad";
    plainText =  "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
    cipherText = "61353b4c2806934a777ff51fa22a4755699b2a714fcdc6f83766e5f97b6c742373806900e49f24b22b097544d4896b424989b5e1ebac0f07c23f4598";
    tag =        "3612d2e79e3b0785561be14aaca2fccb";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 5.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 5.2");
    accum += retval;

    /* testcase #6 */

    keyStr =     "feffe9928665731c6d6a8f9467308308";
    adata = "feedfacedeadbeeffeedfacedeadbeefabaddad2";
    iv = "9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b";
    plainText = "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
    cipherText = "8ce24998625615b603a033aca13fb894be9112a5c3a211a8ba262a3cca7e2ca701e4a9a4fba43c90ccdcb281d48c7c6fd62875d2aca417034c34aee5";
    tag = "619cc5aefffe0bfa462af43c1699d050";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 6.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 6.2");
    accum += retval;

    /* testcase #46 from BG (catchestheLTCbugofv1.15) */
    keyStr = "00000000000000000000000000000000";
    adata =  "688e1aa984de926dc7b4c47f44";
    iv =     "b72138b5a05ff5070e8cd94183f761d8";
    plainText =  "a2aab3ad8b17acdda288426cd7c429b7ca86b7aca05809c70ce82db25711cb5302eb2743b036f3d750d6cf0dc0acb92950d546db308f93b4ff244afa9dc72bcd758d2c";
    cipherText = "cbc8d2f15481a4cc7dd1e19aaa83de5678483ec359ae7dec2ab8d534e0906f4b4663faff58a8b2d733b845eef7c9b331e9e10eb2612c995feb1ac15a6286cce8b297a8";
    tag =    "8d2d2a9372626f6bee8580276a6366bf";

    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 7.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 7.2");
    accum += retval;

    /* testcase #8 - #1 with NULL IV and AAD */
    
    keyStr =     "00000000000000000000000000000000";
    adata =      "";
    iv =         "";
    plainText =  "";
    cipherText = "";
    tag =        "66e94bd4ef8a2c3b884cfa59ca342b2e";
    
    retval = CCCryptorGCMTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 8.1");
    accum += retval;
    retval = CCCryptorGCMDiscreetTestCase(keyStr, iv, adata, tag, alg, cipherText, plainText);
    ok(retval == 0, "AES-GCM Testcase 8.2");
    accum += retval;


    return accum != 0;
}
#endif

