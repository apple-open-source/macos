//
//  CommonKeyDerivation.c
//  CommonCrypto
//
//  Created by Richard Murphy on 1/21/14.
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#include <stdio.h>
#include "testbyteBuffer.h"
#include "testutil.h"
#include "capabilities.h"
#include "testmore.h"
#include <string.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonKeyDerivationSPI.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonKeyDerivation.h>

typedef struct KDFVector_t {
    char *password;
    char *salt;
    int saltLen; //negative means get the salt from the string
    unsigned rounds;
    CCDigestAlgorithm alg;
    int dklen;
    char *expectedstr;
    int expected_failure;
} KDFVector;

static KDFVector kdfv[] = {
    // Test Case PBKDF2 - HMACSHA1 http://tools.ietf.org/html/draft-josefsson-pbkdf2-test-vectors-00
    { "password", "salt", -1, 1   , kCCDigestSHA1, 20, "0c60c80f961f0e71f3a9b524af6012062fe037a6", 0 },
    { "password", "salt", -1, 2   , kCCDigestSHA1, 20, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", 0 },
    { "password", "salt", -1, 4096, kCCDigestSHA1, 20, "4b007901b765489abead49d926f721d065a429c1", 0 },
    { "password", "salt", -1, 1   , 0            , 20, NULL, kCCParamError},
    {.password=NULL},
};

static KDFVector kdfv_for_OriginalKDF[] = {
    // some extra
    { "password", "salt",  0, 4096, kCCDigestSHA1, 20, "546878f250c3baf85d44fbf77435a03828811dfb", 0 },
    { "password", NULL  ,  0, 4096, kCCDigestSHA1, 20, "546878f250c3baf85d44fbf77435a03828811dfb", 0 },
    { "password", NULL  ,999, 4096, kCCDigestSHA1, 20, "", kCCParamError },
    {.password=NULL},
};

static char * testString(char *format, CCDigestAlgorithm alg) {
    static char thestring[80];
    sprintf(thestring, format, digestName(alg));
    return thestring;
}

static int testOriginalKDF(KDFVector *v) {
    CCPseudoRandomAlgorithm prf = digestID2PRF(v->alg);
    byteBuffer derivedKey = mallocByteBuffer(v->dklen);
    byteBuffer expected = hexStringToBytesIfNotNULL(v->expectedstr);
    
    int status = 0;
    size_t saltLen;
    
    if (v->saltLen<0&& v->salt!=NULL)
        saltLen = strlen(v->salt);
    else
        saltLen = v->saltLen;
    
    int retval = CCKeyDerivationPBKDF(kCCPBKDF2, v->password, strlen(v->password), (uint8_t *) v->salt, saltLen, prf, v->rounds, derivedKey->bytes, derivedKey->len);
    if(v->expected_failure) {
        is(retval, v->expected_failure, "CCPBKDF2 should have failed");
    } else {
        status = expectedEqualsComputed(testString("Original PBKDF2-HMac-%s", v->alg), expected, derivedKey);
        ok(status==1, "Derived key failure");
    }
    
    free(derivedKey);
    free(expected);
    return 1;
}

static int testNewKDF(KDFVector *v) {
    byteBuffer derivedKey = mallocByteBuffer(v->dklen);
    byteBuffer expected = hexStringToBytesIfNotNULL(v->expectedstr);

    int status = 0;
    
    CCStatus retval = CCKeyDerivationHMac(kCCKDFAlgorithmPBKDF2_HMAC, v->alg, v->rounds, v->password, strlen(v->password), NULL, 0, NULL, 0, NULL, 0, v->salt, strlen(v->salt), derivedKey->bytes, derivedKey->len);
    if(v->expected_failure) {
        ok(retval != kCCSuccess, "PBKDF2_HMAC Expected failure");
    } else {
        ok(status = expectedEqualsComputed(testString("New PBKDF2-HMac-%s", v->alg), expected, derivedKey), "Derived key is as expected");
    }
    free(derivedKey);
    free(expected);
    return 1;
}

int CommonKeyDerivation(int __unused argc, char *const * __unused argv) {
	plan_tests(11);
    
    int i;
    for(i=0; kdfv[i].password!=NULL; i++) {
        diag("Test %lu", i + 1);
        testOriginalKDF(&kdfv[i]);
        testNewKDF(&kdfv[i]);
    }
    
    for(int k=0; kdfv_for_OriginalKDF[k].password!=NULL; k++, i++) {
        diag("Test %lu", i + 1);
        testOriginalKDF(&kdfv_for_OriginalKDF[k]);
    }


    unsigned iter;
    // Password of length 10byte, salt 16bytes, output 32bytes, 100msec
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 0, kCCPRFHmacAlgSHA1, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA1 no salt:   %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA1, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA1:   %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA224, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA224: %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA256, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA256: %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA384, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA384: %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA512, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA512: %7lu", iter);

    return 0;
}
