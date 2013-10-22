#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"

#if (CCSYMOUTPUTLEN == 0)
entryPoint(CommonCryptoOutputLength,"CommonCrypto Output Length Testing")
#else

static int kTestTestCount = 21912;

#define MAXSTART 64
#define MAXOUT 4096

static int
testOutputLength(CCOperation op, CCMode mode, CCAlgorithm alg, size_t keyLength, CCPadding padding, size_t bufferPos, size_t inputLength, bool final, size_t expectedLen)
{
    CCCryptorRef cryptorRef;
    size_t retval;
    CCCryptorStatus status;
    uint8_t iv[16];
    uint8_t key[16];
    uint8_t dataIn[MAXSTART], dataOut[MAXOUT];
    size_t moved = 0;
    
    status = CCCryptorCreateWithMode(op, mode, alg, padding, iv, key, keyLength, NULL, 0, 1, kCCModeOptionCTR_BE, &cryptorRef);
    ok(status == kCCSuccess, "Created Cryptor");
    status = CCCryptorUpdate(cryptorRef, dataIn, bufferPos, dataOut, MAXOUT, &moved);
    ok(status == kCCSuccess, "Setup Initial Internal Length");
    retval = CCCryptorGetOutputLength(cryptorRef, inputLength, final);
    ok(retval == expectedLen, "Got Length Value Expected");
    if(retval != expectedLen) {
        printf("bufferPos = %lu + inputLength = %lu Got %lu expected %lu\n", bufferPos, inputLength, retval, expectedLen);
    }
    status = CCCryptorRelease(cryptorRef);
    ok(status == kCCSuccess, "Released Cryptor");
    return -1;
}

static inline size_t round_down_by_blocksize(size_t len, size_t blocksize) {
    return len / blocksize * blocksize;
}

static inline size_t pkcs7decryptUpdateResultLength_by_blocksize(size_t len, size_t blocksize) {
    if(len <= blocksize) return 0;
    if(!(len % blocksize)) return len - blocksize;
    return round_down_by_blocksize(len, blocksize);
}

int CommonCryptoOutputLength (int argc, char *const *argv)
{
    int verbose = 0;
	plan_tests(kTestTestCount);
    
    /* ENCRYPTING ****************************************************************************************************************************************/
    if(verbose) diag("ENCRYPTING AES-CTR (Streaming Mode) Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            testOutputLength(kCCEncrypt, kCCModeCTR, kCCAlgorithmAES128, kCCKeySizeAES128, ccNoPadding, bufferPos, i, false, i);
        }
    }

    if(verbose) diag("ENCRYPTING AES-ECB-NoPadding Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCEncrypt, kCCModeECB, kCCAlgorithmAES128, kCCKeySizeAES128, ccNoPadding, bufferPos, i, false, total-(total%kCCBlockSizeAES128));
        }
    }
    
    if(verbose) diag("ENCRYPTING 3DES-ECB-NoPadding Update");
    for(size_t i=0; i<3*kCCBlockSize3DES; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSize3DES; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCEncrypt, kCCModeECB, kCCAlgorithm3DES, kCCKeySize3DES, ccNoPadding, bufferPos, i, false, total-(total%kCCBlockSize3DES));
        }
    }
    
    if(verbose) diag("ENCRYPTING AES-CBC-PKCS7Padding Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCEncrypt, kCCModeCBC, kCCAlgorithmAES128, kCCKeySizeAES128, ccPKCS7Padding, bufferPos, i, false, total-(total%kCCBlockSizeAES128));
        }
    }
    
    if(verbose) diag("ENCRYPTING 3DES-CBC-PKCS7Padding Update");
    for(size_t i=0; i<3*kCCBlockSize3DES; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSize3DES; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCEncrypt, kCCModeCBC, kCCAlgorithm3DES, kCCKeySize3DES, ccPKCS7Padding, bufferPos, i, false, total-(total%kCCBlockSize3DES));
        }
    }
    if(verbose) diag("ENCRYPTING AES-CBC-PKCS7Padding-Final");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCEncrypt, kCCModeCBC, kCCAlgorithmAES128, kCCKeySizeAES128, ccPKCS7Padding, bufferPos, i, true, total-(total%kCCBlockSizeAES128)+kCCBlockSizeAES128);
        }
    }
    
    if(verbose) diag("ENCRYPTING 3DES-CBC-PKCS7Padding-Final");
    for(size_t i=0; i<3*kCCBlockSize3DES; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSize3DES; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCEncrypt, kCCModeCBC, kCCAlgorithm3DES, kCCKeySize3DES, ccPKCS7Padding, bufferPos, i, true, total-(total%kCCBlockSize3DES)+kCCBlockSize3DES);
        }
    }

    /* DECRYPTING ****************************************************************************************************************************************/

    if(verbose) diag("DECRYPTING AES-CTR (Streaming Mode) Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i++) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            testOutputLength(kCCDecrypt, kCCModeCTR, kCCAlgorithmAES128, kCCKeySizeAES128, ccNoPadding, bufferPos, i, false, i);
        }
    }
    
    // From here down everything should be in full blocks
    if(verbose) diag("DECRYPTING AES-ECB-NoPadding Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i+=kCCBlockSizeAES128) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCDecrypt, kCCModeECB, kCCAlgorithmAES128, kCCKeySizeAES128, ccNoPadding, bufferPos, i, false, round_down_by_blocksize(total, kCCBlockSizeAES128));
        }
    }
    
    if(verbose) diag("DECRYPTING 3DES-ECB-NoPadding Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i+=kCCBlockSizeAES128) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSizeAES128; bufferPos++) {
            size_t total = i+bufferPos;
            testOutputLength(kCCDecrypt, kCCModeECB, kCCAlgorithm3DES, kCCKeySize3DES, ccNoPadding, bufferPos, i, false, round_down_by_blocksize(total, kCCBlockSizeAES128));
        }
    }
    
    if(verbose) diag("DECRYPTING AES-CBC-PKCS7Padding Update");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i++) {
        for(size_t bufferPos=1; bufferPos <= kCCBlockSizeAES128; bufferPos ++) {
            size_t total = pkcs7decryptUpdateResultLength_by_blocksize(i+bufferPos, kCCBlockSizeAES128);
            testOutputLength(kCCDecrypt, kCCModeCBC, kCCAlgorithmAES128, kCCKeySizeAES128, ccPKCS7Padding, bufferPos, i, false, total);
        }
    }
    
    if(verbose) diag("DECRYPTING 3DES-CBC-PKCS7Padding Update");
    for(size_t i=0; i<3*kCCBlockSize3DES; i++) {
        for(size_t bufferPos=1; bufferPos <= kCCBlockSize3DES; bufferPos ++) {
            size_t total = pkcs7decryptUpdateResultLength_by_blocksize(i+bufferPos, kCCBlockSize3DES);
            testOutputLength(kCCDecrypt, kCCModeCBC, kCCAlgorithm3DES, kCCKeySize3DES, ccPKCS7Padding, bufferPos, i, false, total);
        }

    }
    
    if(verbose) diag("DECRYPTING AES-CBC-PKCS7Padding-Final");
    for(size_t i=0; i<3*kCCBlockSizeAES128; i+=kCCBlockSizeAES128) {
        size_t bufferPos=kCCBlockSizeAES128;
        size_t total = i+bufferPos;
        testOutputLength(kCCDecrypt, kCCModeCBC, kCCAlgorithmAES128, kCCKeySizeAES128, ccPKCS7Padding, bufferPos, i, true, total);
    }

    if(verbose) diag("DECRYPTING 3DES-CBC-PKCS7Padding-Final");
    for(size_t i=0; i<3*kCCBlockSize3DES; i+=kCCBlockSize3DES) {
        for(size_t bufferPos=0; bufferPos<kCCBlockSize3DES; bufferPos+=kCCBlockSize3DES) {
            size_t bufferPos=kCCBlockSize3DES;
            size_t total = i+bufferPos;
            testOutputLength(kCCDecrypt, kCCModeCBC, kCCAlgorithm3DES, kCCKeySize3DES, ccPKCS7Padding, kCCBlockSize3DES, i, true, total);
        }
    }

    return 0;
}
#endif /* CCSYMOUTPUTLEN */

