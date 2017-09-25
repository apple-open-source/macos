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
#include "ccMemory.h"
#include "CommonCryptorPriv.h"

#if (CCSYMGCM == 0)
entryPoint(CommonCryptoSymGCM,"CommonCrypto Symmetric GCM Testing")
#else


gcm_text_vector_t gcm_kat_vectors[]= {
    {
        .key =     "00000000000000000000000000000000",
        .adata =      "",
        .iv =         "000000000000000000000000",
        .plainText =  "",
        .cipherText = "",
        .tag =        "58e2fccefa7e3061367f1d57a4e7455a",
    },
    {
        .key =     "00000000000000000000000000000000",
        .adata =      "",
        .iv =         "000000000000000000000000",
        .plainText =  "00000000000000000000000000000000",
        .cipherText = "0388dace60b6a392f328c2b971b2fe78",
        .tag =        "ab6e47d42cec13bdf53a67b21257bddf",
    },{
        .key =     "feffe9928665731c6d6a8f9467308308",
        .adata =      "",
        .iv =         "cafebabefacedbaddecaf888",
        .plainText =  "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
        .cipherText = "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
        .tag =        "4d5c2af327cd64a62cf35abd2ba6fab4",
    },{
        .key =     "feffe9928665731c6d6a8f9467308308",
        .adata =      "feedfacedeadbeeffeedfacedeadbeefabaddad2",
        .iv =         "cafebabefacedbaddecaf888",
        .plainText =  "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
        .cipherText = "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
        .tag =        "5bc94fbc3221a5db94fae95ae7121a47",
    },{
        .key =     "feffe9928665731c6d6a8f9467308308",
        .adata = "feedfacedeadbeeffeedfacedeadbeefabaddad2",
        .iv = "9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
        .plainText = "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
        .cipherText = "8ce24998625615b603a033aca13fb894be9112a5c3a211a8ba262a3cca7e2ca701e4a9a4fba43c90ccdcb281d48c7c6fd62875d2aca417034c34aee5",
        .tag = "619cc5aefffe0bfa462af43c1699d050",
    },{
        .key = "00000000000000000000000000000000",
        .adata =  "688e1aa984de926dc7b4c47f44",
        .iv =     "b72138b5a05ff5070e8cd94183f761d8",
        .plainText =  "a2aab3ad8b17acdda288426cd7c429b7ca86b7aca05809c70ce82db25711cb5302eb2743b036f3d750d6cf0dc0acb92950d546db308f93b4ff244afa9dc72bcd758d2c",
        .cipherText = "cbc8d2f15481a4cc7dd1e19aaa83de5678483ec359ae7dec2ab8d534e0906f4b4663faff58a8b2d733b845eef7c9b331e9e10eb2612c995feb1ac15a6286cce8b297a8",
        .tag =    "8d2d2a9372626f6bee8580276a6366bf",
    },{
        .key=NULL
    }
};

gcm_text_vector_t gcm_old_vectors[]= {
    { //IV too short
      .key =     "feffe9928665731c6d6a8f9467308308",
      .adata =      "feedfacedeadbeeffeedfacedeadbeefabaddad2",
      .iv =         "cafebabefacedbad",
      .plainText =  "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
      .cipherText = "61353b4c2806934a777ff51fa22a4755699b2a714fcdc6f83766e5f97b6c742373806900e49f24b22b097544d4896b424989b5e1ebac0f07c23f4598",
      .tag =        "3612d2e79e3b0785561be14aaca2fccb",
    },{ //no IV
     .key =     "00000000000000000000000000000000", //testcase #8 - #1 with NULL IV and AAD
     .adata =      "",
     .iv =         "",
     .plainText =  "",
     .cipherText = "",
     .tag =        "66e94bd4ef8a2c3b884cfa59ca342b2e",
    },{
        .key=NULL
    }
};

static bool test_new_api = false;
static bool call_api_with_null = false;

#define n_args(...)  (sizeof((void *[]){__VA_ARGS__})/sizeof(void *))
#define free_all(...)  (_free(n_args(__VA_ARGS__), __VA_ARGS__))
static void _free(int n_args, ...)
{
    va_list ap;
    
    va_start(ap, n_args);
    while (n_args--)
        free(va_arg(ap, void *));
    va_end(ap);
    
}
//------------------------------------------------------------------------------
static int
CCCryptorGCMTestCase(gcm_text_vector_t *v)
{
    byteBuffer key, iv, pt, ct, adata, tag;
    int rc=1; //fail
    
    CCCryptorStatus retval;
    size_t tagOutlen;
    size_t  dataLen;
    CCAlgorithm alg = kCCAlgorithmAES;

    key = hexStringToBytes(v->key);
    adata = ccConditionalTextBuffer(v->adata);
    tag = hexStringToBytes(v->tag);
    pt = ccConditionalTextBuffer(v->plainText);
    ct = ccConditionalTextBuffer(v->cipherText);
    iv = ccConditionalTextBuffer(v->iv);
    char cipherDataOut[ct->len];
    char plainDataOut[pt->len];
    char tagOut[tag->len];
    
    dataLen = pt->len;
    
    tagOutlen = tag->len;
    CC_XZEROMEM(tagOut, 16);
    if(test_new_api)
        retval = CCCryptorGCMOneshotEncrypt(alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, pt->bytes, dataLen, cipherDataOut, tagOut, tagOutlen);
    else
        retval =   CCCryptorGCM(kCCEncrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, pt->bytes, dataLen, cipherDataOut, tagOut, &tagOutlen);
    ok_or_goto(retval == kCCSuccess, "CCCryptorGCM encrypt", errOut);
    ok_memcmp(ct->bytes, cipherDataOut, ct->len, "CCCryptorGCM encrypt");
    
    
    // Decrypt correctly
    tagOutlen = tag->len;
    CC_XZEROMEM(plainDataOut, pt->len);
    if(test_new_api){
        //use the output tag of encrypt as the tag input
        retval = CCCryptorGCMOneshotDecrypt(alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagOut, tagOutlen);
    }else{
        CC_XZEROMEM(tagOut, 16);
        retval =   CCCryptorGCM(kCCDecrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagOut, &tagOutlen);
        rc=timingsafe_bcmp(tagOut, tag->bytes, tag->len);
        ok_or_goto(rc==0, "tag comparison", errOut);
    }
    ok_or_goto(retval == kCCSuccess, "CCCryptorGCM decryption", errOut);
    ok_memcmp(pt->bytes, plainDataOut, pt->len, "CCCryptorGCM decryption data out");
    
    // Negative test, modify IV
    tagOutlen = tag->len;
    CC_XZEROMEM(plainDataOut, pt->len);
    if(iv->len>0)
        iv->bytes[0]^=1;
    else
        key->bytes[0]^=1;
    
    if(test_new_api){
        retval =     CCCryptorGCMOneshotDecrypt(alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagOut, tagOutlen);
        ok_or_goto(retval!= kCCSuccess, "CCCryptorGCM decryption, negative test", errOut);

    }else{
        CC_XZEROMEM(tagOut, 16);
        retval=CCCryptorGCM(kCCDecrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagOut, &tagOutlen);
        ok_or_goto(retval == kCCSuccess, "CCCryptorGCM decryption, negative test", errOut);
        
        rc=timingsafe_bcmp(tagOut, tag->bytes, tag->len);
        ok(rc!=0, "tag comparison, negative test");
    }
    
    rc = memcmp(pt->bytes, plainDataOut, pt->len) || pt->len==0;
    ok(rc!=0, "CCCryptorGCM decryption, negative test");
    
    
    rc = 0;
    
errOut:
    free_all(pt, ct, key, iv, adata, tag);
    return rc;
}

//------------------------------------------------------------------------------
static CCCryptorStatus
CallGCMFuncs(CCOperation op,
             CCAlgorithm alg,
             const void  *key,
             size_t 	 keyLength,
             const void  *iv,
             size_t 	 ivLen,
             const void  *aData,
             size_t 	 aDataLen,
             const void  *dataIn,
             size_t 	 dataInLength,
             void 		 *dataOut,
             void 		 *tag,
             size_t 	 tagLength)
{
    CCCryptorStatus retval;
    CCCryptorRef    cref;
    
    retval = CCCryptorCreateWithMode(op, kCCModeGCM, alg, ccNoPadding, NULL, key, keyLength, NULL, 0, 0, 0, &cref);
    if(retval != kCCSuccess) return retval;
    
    if(call_api_with_null || ivLen!=0){
        if(test_new_api){
            retval = CCCryptorGCMSetIV(cref, NULL, ivLen);
            ok_or_goto(retval == kCCParamError, "add NULL IV", out);
            
            retval = CCCryptorGCMSetIV(cref, iv, AESGCM_MIN_IV_LEN-1);
            ok_or_goto(retval == kCCParamError, "add small IV", out);

            retval = CCCryptorGCMSetIV(cref, iv, ivLen);
        }
        else
            retval = CCCryptorGCMAddIV(cref, iv, ivLen);
        ok_or_goto(retval == kCCSuccess, "add IV", out);
    }
    
    if(call_api_with_null || aDataLen!=0){
        retval = CCCryptorGCMaddAAD(cref, aData, aDataLen);
        ok_or_goto(retval == kCCSuccess, "add AAD", out);
    }
    
    
    if(call_api_with_null || dataInLength!=0){
        if(kCCEncrypt == op) {
            retval = CCCryptorGCMEncrypt(cref, dataIn, dataInLength, dataOut);
            ok_or_goto(retval == kCCSuccess, "Encrypt", out);
        } else {
            retval = CCCryptorGCMDecrypt(cref, dataIn, dataInLength, dataOut);
            ok_or_goto(retval == kCCSuccess, "Decrypt", out);
        }
    }
    
    if(op==kCCDecrypt){
        if(test_new_api){
            retval = CCCryptorGCMFinalize(cref, tag, tagLength);
        }else{
            char tagOut[tagLength];
            retval = CCCryptorGCMFinal(cref, tagOut, &tagLength);
            if (timingsafe_bcmp(tagOut, tag, tagLength)) {
                diag("FAIL Tag on ciphertext is wrong\n");
                retval=1;
                goto out;
            }
        }
        ok_or_goto(retval == kCCSuccess, "Finalize", out);
    }else{
        char tagOut[tagLength];
        if(test_new_api)
            retval = CCCryptorGCMFinalize(cref, tagOut, tagLength);
        else
            retval = CCCryptorGCMFinal(cref, tagOut, &tagLength);
        
        ok_or_goto(retval == kCCSuccess, "Finalize", out);
        ok_memcmp(tagOut, tag, tagLength, "tag comparison");
    }
        
    retval = CCCryptorGCMReset(cref);
    ok_or_goto(retval == kCCSuccess, "Failed to Reset", out);
    
out:
    
    CCCryptorRelease(cref);
    return retval;
}

static int
GCMDiscreteTestCase(CCOperation op, gcm_text_vector_t *v)
{
    byteBuffer key, iv, pt, ct, adata, tag;
    
    CCCryptorStatus retval;
    CCAlgorithm alg = kCCAlgorithmAES;
    
    key = hexStringToBytes(v->key);
    adata = ccConditionalTextBuffer(v->adata);
    tag = hexStringToBytes(v->tag);
    pt = ccConditionalTextBuffer(v->plainText);
    ct = ccConditionalTextBuffer(v->cipherText);
    iv = ccConditionalTextBuffer(v->iv);
    char dataOut[pt->len];
    
    const void 		*dataIn;
    size_t 			dataInLength;
    if(op == kCCEncrypt){
        dataInLength = pt->len;
        dataIn = pt->bytes;
    } else{
        dataInLength = ct->len;
        dataIn = ct->bytes;
    }
    
    //tagOutlen = tag->len;
    retval = CallGCMFuncs(op, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, dataIn, dataInLength, dataOut, tag->bytes, tag->len);
    ok_or_goto(retval== kCCSuccess, "Encrypt Failed", out);
    
    if(op == kCCEncrypt){
        ok_memcmp(dataOut, ct->bytes, dataInLength, "GCM discrete encrypt");

    } else{
        ok_memcmp(dataOut, pt->bytes, dataInLength, "GCM discrete decrypt");
    }
    
out:
    free_all(tag, pt, ct, key, iv, adata);
    return retval;
}

static int
CCCryptorGCMDiscreetTestCase(gcm_text_vector_t *v)
{
    CCCryptorStatus rc;
    
    call_api_with_null=false;
    rc = GCMDiscreteTestCase(kCCEncrypt, v);
    ok(rc==0, "GCM Encrypt\n");
    rc = GCMDiscreteTestCase(kCCDecrypt, v);
    ok(rc==0, "GCM Decrypt\n");
    
    
    call_api_with_null=true;
    rc = GCMDiscreteTestCase(kCCEncrypt, v);
    ok(rc==0, "GCM Encrypt (bypassing IV and AAD)\n");
    rc = GCMDiscreteTestCase(kCCDecrypt, v);
    ok(rc==0, "GCM decrypt (bypassing IV and AAD)\n");
    
    return 0;
}

static int
AESGCMTests()
{
    int retval, accum = 0;
    
    gcm_text_vector_t *v = gcm_kat_vectors;
    for(int i=0; v->key!=NULL; i++, v++){
        retval = CCCryptorGCMTestCase(v);
        ok(retval == 0, "AES-GCM Testcase %d", i+1);
        accum += retval;
        
        retval = CCCryptorGCMDiscreetTestCase(v);
        ok(retval == 0, "AES-GCM Discreet Testcase %d", i+1);
        accum += retval;
    }
    
    return accum;
}

static int kTestTestCount = 548;

int
CommonCryptoSymGCM(int __unused argc, char *const * __unused argv)
{
    int accum = 0;
    
    plan_tests(kTestTestCount);
    test_new_api = false;
    accum = AESGCMTests();
    test_new_api = true;
    accum += AESGCMTests();
    
    return accum != 0;
}
#endif

