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
    int rounds;
    CCDigestAlgorithm alg;
    int dklen;
    char *expectedstr;
    int expected_failure;
} KDFVector;

static KDFVector kdfv[] = {
    // Test Case PBKDF2 - HMACSHA1 http://tools.ietf.org/html/draft-josefsson-pbkdf2-test-vectors-00
    { "password", "salt", 1, kCCDigestSHA1, 20, "0c60c80f961f0e71f3a9b524af6012062fe037a6", 0 },
    { "password", "salt", 2, kCCDigestSHA1, 20, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", 0 },
    { "password", "salt", 4096, kCCDigestSHA1, 20, "4b007901b765489abead49d926f721d065a429c1", 0 },
    { "password", "salt", 1, 0, 20, NULL, -1} // This crashed
};

static size_t kdfvLen = sizeof(kdfv) / sizeof(KDFVector);


static char * testString(char *format, CCDigestAlgorithm alg) {
    static char thestring[80];
    sprintf(thestring, format, digestName(alg));
    return thestring;
}


static int testOriginalKDF(char *password, char *salt, int rounds, CCDigestAlgorithm alg, int dklen, byteBuffer expected, int expected_failure) {
    CCPseudoRandomAlgorithm prf = digestID2PRF(alg);
    byteBuffer derivedKey = mallocByteBuffer(dklen);
    int status = 0;
    
    int retval = CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), (uint8_t *) salt, strlen(salt), prf, rounds, derivedKey->bytes, derivedKey->len);
    if(retval == -1 && expected_failure) {
        return 1;
    }
    ok(status = expectedEqualsComputed(testString("Original PBKDF2-HMac-%s", alg), expected, derivedKey), "Derived key is as expected");
    free(derivedKey);
    return 1;
}

static int testNewKDF(char *password, char *salt, int rounds, CCDigestAlgorithm alg, int dklen, byteBuffer expected, int expected_failure) {
    byteBuffer derivedKey = mallocByteBuffer(dklen);
    int status = 0;
    
    CCStatus retval = CCKeyDerivationHMac(kCCKDFAlgorithmPBKDF2_HMAC, alg, rounds, password, strlen(password), NULL, 0, NULL, 0, NULL, 0, salt, strlen(salt), derivedKey->bytes, derivedKey->len);
    if(retval != kCCSuccess && expected_failure) {
        return 1;
    }
    ok(status = expectedEqualsComputed(testString("New PBKDF2-HMac-%s", alg), expected, derivedKey), "Derived key is as expected");
    free(derivedKey);
    return 1;
}


static int
PBKDF2Test(KDFVector *kdfvec)
{
    byteBuffer expectedBytes = hexStringToBytesIfNotNULL(kdfvec->expectedstr);
    int status = 0;
    
    ok(status = testOriginalKDF(kdfvec->password, kdfvec->salt, kdfvec->rounds, kdfvec->alg, kdfvec->dklen, expectedBytes, kdfvec->expected_failure), "Test Original version of PBKDF2");
    ok(status &= testNewKDF(kdfvec->password, kdfvec->salt, kdfvec->rounds, kdfvec->alg, kdfvec->dklen, expectedBytes, kdfvec->expected_failure), "Test Original Discreet version of PBKDF2");

    free(expectedBytes);
    return status;
}

static int testsPerVector = 2;

int CommonKeyDerivation(int argc, char *const *argv) {
	plan_tests((int) (kdfvLen*testsPerVector));
    
    for(size_t testcase = 0; testcase < kdfvLen; testcase++) {
        diag("Test %lu\n", testcase + 1);
        ok(PBKDF2Test(&kdfv[testcase]), "Successful full test of KDF Vector");
    }
    return 0;
}
