#include <Availability.h>
#include "capabilities.h"
#include "testmore.h"
#include "testbyteBuffer.h"

#if (CCEC == 0)
entryPoint(CommonEC,"Elliptic Curve Cryptography")
#else

#include <CommonCrypto/CommonECCryptor.h>

static int kTestTestCount = 18;

int CommonEC(int argc, char *const *argv) {
	CCCryptorStatus retval;
    size_t keysize;
    CCECCryptorRef publicKey, privateKey;
    CCECCryptorRef publicKey2;
    // byteBuffer keydata, dekeydata;
    byteBuffer hash;
    char encryptedKey[8192];
    size_t encryptedKeyLen = 8192;
    // char decryptedKey[8192];
    // size_t decryptedKeyLen = 8192;
    char signature[8192];
    size_t signatureLen = 8192;
    char importexport[8192];
    size_t importexportLen = 8192;
    uint32_t valid;
    int accum = 0;
    int debug = 0;
    
	plan_tests(kTestTestCount);
    
    keysize = 256;
    
    retval = CCECCryptorGeneratePair(keysize, &publicKey, &privateKey);
    if(debug) printf("Keys Generated\n");
    ok(retval == 0, "Generate an EC Key Pair");
	accum |= retval;

#ifdef ECDH
    keydata = hexStringToBytes("000102030405060708090a0b0c0d0e0f");
    
    retval = CCECCryptorWrapKey(publicKey, keydata->bytes, keydata->len, encryptedKey, &encryptedKeyLen, kCCDigestSHA1);
    
    ok(retval == 0, "Wrap Key Data with EC Encryption - ccPKCS1Padding");
    accum |= retval;
    
    retval = CCECCryptorUnwrapKey(privateKey, encryptedKey, encryptedKeyLen,
                        decryptedKey, &decryptedKeyLen);
    
    ok(retval == 0, "Unwrap Key Data with EC Encryption - ccPKCS1Padding");
    accum |= retval;

	dekeydata = bytesToBytes(decryptedKey, decryptedKeyLen);
    
	ok(bytesAreEqual(dekeydata, keydata), "Round Trip CCECCryptorWrapKey/CCECCryptorUnwrapKey");
    accum |= retval;
#endif

    
    hash = hexStringToBytes("000102030405060708090a0b0c0d0e0f");

    retval = CCECCryptorSignHash(privateKey, 
                     hash->bytes, hash->len,
                     signature, &signatureLen);
                     
    ok(retval == 0, "EC Signing");
    valid = 0;
    accum |= retval;
    if(debug) printf("Signing Complete\n");
    
    retval = CCECCryptorVerifyHash(publicKey,
                       hash->bytes, hash->len, 
                       signature, signatureLen, &valid);
    ok(retval == 0, "EC Verifying");
    accum |= retval;
	ok(valid, "EC Validity");
    accum |= retval;
    if(debug) printf("Verify Complete\n");
   
    // Mess with the sig - see what happens
    signature[signatureLen-3] += 3;
    retval = CCECCryptorVerifyHash(publicKey,
                                   hash->bytes, hash->len, 
                                   signature, signatureLen, &valid);
    ok(retval == 0, "EC Verifying");
    accum |= retval;
	ok(!valid, "EC Invalid Signature");
    accum |= retval;
    
    if(debug) printf("Verify2 Complete\n");
    
    encryptedKeyLen = 8192;
	retval = CCECCryptorExportPublicKey(publicKey, importexport, &importexportLen);
    
    ok(retval == 0, "EC Export Public Key");
    accum |= retval;

    retval = CCECCryptorImportPublicKey(importexport, importexportLen, &publicKey2);
    
    ok(retval == 0, "EC Import Public Key");
    accum |= retval;
                          
	encryptedKeyLen = 8192;
    retval = CCECCryptorComputeSharedSecret(privateKey, publicKey, encryptedKey, &encryptedKeyLen);

    ok(retval == 0, "EC Shared Secret");
    accum |= retval;

    return accum;
}
#endif /* CCEC */
