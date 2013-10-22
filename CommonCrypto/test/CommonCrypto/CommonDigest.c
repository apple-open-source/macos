#include <stdio.h>
#include "testbyteBuffer.h"
#include "capabilities.h"
#include "testmore.h"
#include <string.h>

#define COMMON_DIGEST_FOR_RFC_1321
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>

#ifdef CCDIGEST
#include <CommonCrypto/CommonDigestSPI.h>
#endif

#ifdef CCKEYDERIVATION
#include <CommonCrypto/CommonKeyDerivation.h>
#endif

static char *digestName(CCDigestAlgorithm digestSelector) {
    switch(digestSelector) {
        default: return "None";
        case  kCCDigestMD2: return "MD2";
        case  kCCDigestMD4: return "MD4";
        case  kCCDigestMD5: return "MD5";
        case  kCCDigestRMD128: return "RMD128";
        case  kCCDigestRMD160: return "RMD160";
        case  kCCDigestRMD256: return "RMD256";
        case  kCCDigestRMD320: return "RMD320";
        case  kCCDigestSHA1: return "SHA1";
        case  kCCDigestSHA224: return "SHA224";
        case  kCCDigestSHA256: return "SHA256";
        case  kCCDigestSHA384: return "SHA384";
        case  kCCDigestSHA512: return "SHA512";
        case  kCCDigestSkein128: return "Skein128";
        case  kCCDigestSkein160: return "Skein160";
        case  kCCDigestSkein224: return "Skein224";
        case  kCCDigestSkein256: return "Skein256";
        case  kCCDigestSkein384: return "Skein384";
        case  kCCDigestSkein512: return "Skein512";
    }
}

static size_t nullstrlen(const char *s) {
    if(!s) return 0;
    return strlen(s);
}

#define MAX_DIGEST_SIZE CC_SHA512_DIGEST_LENGTH

#if (CCKEYDERIVATION == 1)
static int
PBKDF2Test(char *password, uint8_t *salt, size_t saltlen, int rounds, CCDigestAlgorithm PRF, int dklen, char *expected)
{
    byteBuffer derivedKey;
    byteBuffer expectedBytes;
    char outbuf[80];
    int retval = 0;
    
    if(expected) expectedBytes = hexStringToBytes(expected);
    derivedKey = mallocByteBuffer(dklen);
    switch(PRF) {
        case 0: CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), (uint8_t *) salt, saltlen, 0, rounds, derivedKey->bytes, derivedKey->len); break;
        case kCCDigestSHA1: CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, saltlen, kCCPRFHmacAlgSHA1, rounds, derivedKey->bytes, derivedKey->len); break;
        case kCCDigestSHA224: CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, saltlen, kCCPRFHmacAlgSHA224, rounds, derivedKey->bytes, derivedKey->len); break;
        case kCCDigestSHA256: CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, saltlen, kCCPRFHmacAlgSHA256, rounds, derivedKey->bytes, derivedKey->len); break;
        case kCCDigestSHA384: CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, saltlen, kCCPRFHmacAlgSHA384, rounds, derivedKey->bytes, derivedKey->len); break;
        case kCCDigestSHA512: CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, saltlen, kCCPRFHmacAlgSHA512, rounds, derivedKey->bytes, derivedKey->len); break;
        default: return 1;
    }
	sprintf(outbuf, "PBKDF2-HMAC-%s test for %s", digestName(PRF), password);
    
    if(expected) {
        ok(bytesAreEqual(derivedKey, expectedBytes), outbuf);
        
        if(!bytesAreEqual(derivedKey, expectedBytes)) {
            diag("KEYDERIVE FAIL: PBKDF2-HMAC-%s(\"%s\")\n expected %s\n      got %s\n", digestName(PRF), password, expected, bytesToHexString(derivedKey));
            retval = 1;
        } else {
            //printf("KEYDERIVE PASS: PBKDF2-HMAC-%s(\"%s\")\n", digestName(PRF), password);
        }
        free(expectedBytes);
    }
    free(derivedKey);
    return retval;
}
#endif

static byteBuffer mallocDigestBuffer(CCDigestAlgorithm digestSelector) {
    size_t len;
    switch(digestSelector) {
        default: len = CCDigestGetOutputSize(digestSelector); break;
        case kCCDigestMD2: len = CC_MD2_DIGEST_LENGTH; break;
        case kCCDigestMD4: len = CC_MD4_DIGEST_LENGTH; break;
        case kCCDigestMD5: len = CC_MD5_DIGEST_LENGTH; break;
        case kCCDigestSHA1: len = CC_SHA1_DIGEST_LENGTH; break;
        case kCCDigestSHA224: len = CC_SHA224_DIGEST_LENGTH; break;
        case kCCDigestSHA256: len = CC_SHA256_DIGEST_LENGTH; break;
        case kCCDigestSHA384: len = CC_SHA384_DIGEST_LENGTH; break;
        case kCCDigestSHA512: len = CC_SHA512_DIGEST_LENGTH; break;
    }
    return mallocByteBuffer(len);
}

static void
OneShotHmac(CCHmacAlgorithm hmacAlg, uint8_t *key, size_t keylen, const char *data, size_t datalen, uint8_t *output)
{
    CCHmacContext ctx;
    
    CCHmacInit(&ctx, hmacAlg, key, keylen);
    CCHmacUpdate(&ctx, data, datalen);
    CCHmacFinal(&ctx, output);    
}

static int
HMACTest(const char *input, char *keystr, CCDigestAlgorithm digestSelector, char *expected)
{
    CCHmacAlgorithm hmacAlg;
    size_t inputLen = nullstrlen(input);
    char outbuf[80];
    int retval = 0;
    
    byteBuffer expectedBytes = hexStringToBytes(expected);
    byteBuffer keyBytes = hexStringToBytes(keystr);
    byteBuffer mdBuf = mallocDigestBuffer(digestSelector);
    switch(digestSelector) {
        case kCCDigestMD5: hmacAlg = kCCHmacAlgMD5; break;
        case kCCDigestSHA1: hmacAlg = kCCHmacAlgSHA1; break;
        case kCCDigestSHA224: hmacAlg = kCCHmacAlgSHA224; break;
        case kCCDigestSHA256: hmacAlg = kCCHmacAlgSHA256; break;
        case kCCDigestSHA384: hmacAlg = kCCHmacAlgSHA384; break;
        case kCCDigestSHA512: hmacAlg = kCCHmacAlgSHA512; break;
        default: return 1;
    }
    CCHmac(hmacAlg, keyBytes->bytes, keyBytes->len, input, inputLen, mdBuf->bytes);
    sprintf(outbuf, "Hmac-%s test for %s", digestName(digestSelector), input);
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("HMAC FAIL: HMAC-%s(\"%s\")\n expected %s\n      got %s\n", digestName(digestSelector), input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // printf("HMAC PASS: HMAC-%s(\"%s\")\n", digestName(digestSelector), input);
    }
    OneShotHmac(hmacAlg, keyBytes->bytes, keyBytes->len, input, inputLen, mdBuf->bytes);
    sprintf(outbuf, "Hmac-%s test for %s", digestName(digestSelector), input);
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("HMAC FAIL: HMAC-%s(\"%s\")\n expected %s\n      got %s\n", digestName(digestSelector), input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        //printf("HMAC PASS: HMAC-%s(\"%s\")\n", digestName(digestSelector), input);
    }
    free(mdBuf);
    free(expectedBytes);
    free(keyBytes);
	return retval;
}

#if (CCDIGEST == 1)

static void OneShotDigest(CCDigestAlgorithm algorithm, const uint8_t *bytesToDigest, size_t numbytes, uint8_t *outbuf)
{
    CCDigestRef d;
    *outbuf = 0;
    if((d = CCDigestCreate(algorithm)) == NULL) return;
    
    size_t fromAlg = CCDigestGetOutputSize(algorithm);
    size_t fromRef = CCDigestGetOutputSizeFromRef(d);
    size_t fromOldRoutine = CCDigestOutputSize(d);
    
    ok(fromAlg == fromRef, "Size is the same from ref or alg");
    ok(fromAlg == fromOldRoutine, "Size is the same from ref or alg");
    if(CCDigestUpdate(d, bytesToDigest, numbytes)) return;
    if(CCDigestFinal(d, outbuf)) return;
    
    uint8_t dupBuf[fromRef];
    CCDigestReset(d);
    if(CCDigestUpdate(d, bytesToDigest, numbytes)) return;
    if(CCDigestFinal(d, dupBuf)) return;
    ok(memcmp(outbuf, dupBuf, fromRef) == 0, "result should be the same from recycled context");

    CCDigestDestroy(d);

}


static int
newHashTest(char *input, CCDigestAlgorithm digestSelector, char *expected)
{
    size_t inputLen = nullstrlen(input);
    char outbuf[4096];
    int retval = 0;
    
    byteBuffer expectedBytes = hexStringToBytes(expected);
    byteBuffer mdBuf = mallocByteBuffer(CCDigestGetOutputSize(digestSelector));

    CCDigest(digestSelector, (const uint8_t *) input, inputLen, mdBuf->bytes);
    sprintf(outbuf, "new interface %s test for %s", digestName(digestSelector), input);
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("Digestor FAIL: %s(\"%s\")\nexpected %s\ngot      %s\n", digestName(digestSelector), input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // printf("Digestor PASS: %s(\"%s\")\n", digestName(digestSelector), input);
    }

    // printf("Digest is %s\n", digestName(digestSelector));
    OneShotDigest(digestSelector, (const uint8_t *) input, inputLen, mdBuf->bytes);
    sprintf(outbuf, "composite interface %s test for %s", digestName(digestSelector), input);
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("Digestor FAIL: %s(\"%s\")\nexpected %s\ngot      %s\n", digestName(digestSelector), input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // printf("Digestor PASS: %s(\"%s\")\n", digestName(digestSelector), input);
    }
 
    free(mdBuf);
    free(expectedBytes);
	return retval;
}

static int
unHashTest(CCDigestAlgorithm digestSelector)
{
    char *buf[128];
    int retval;
    CCDigestRef retref;
    
    retval = CCDigest(digestSelector, (const uint8_t *) buf, 128, (uint8_t *) buf);
    ok(retval == kCCUnimplemented, "Unsupported Digest returns kCCUnimplemented");
    retref = CCDigestCreate(digestSelector);
    ok(retref == NULL, "Unsupported Digest returns NULL");
    return 0;
}
#endif

#define CC_SHA224_CTX CC_SHA256_CTX
#define CC_SHA384_CTX CC_SHA512_CTX
#define OLD_ALL_IN_ONE_HASH(name,input,len,out) \
{ \
    CC_##name##_CTX ctx; \
    ok(CC_##name##_Init(&ctx) == 1, "Old Hash init should result in 1\n"); \
    ok(CC_##name##_Update(&ctx, input, len) == 1, "Old Hash update should result in 1\n"); \
    ok(CC_##name##_Final(out, &ctx) == 1, "Old Hash final should result in 1\n"); \
} \
break


static int
hashTest(char *input, CCDigestAlgorithm digestSelector, char *expected)
{
    CC_LONG inputLen = (CC_LONG) nullstrlen(input);
    char outbuf[4096];
    int retval = 0;
    byteBuffer mdBuf = mallocDigestBuffer(digestSelector);
    byteBuffer expectedBytes = hexStringToBytes(expected);
    
    switch(digestSelector) {
        case kCCDigestMD2:		CC_MD2(input, inputLen, mdBuf->bytes); break;
        case kCCDigestMD4:		CC_MD4(input, inputLen, mdBuf->bytes); break;
        case kCCDigestMD5:		CC_MD5(input, inputLen, mdBuf->bytes); break;
        case kCCDigestSHA1:	CC_SHA1(input, inputLen, mdBuf->bytes); break;
        case kCCDigestSHA224:	CC_SHA224(input, inputLen, mdBuf->bytes); break;
        case kCCDigestSHA256:	CC_SHA256(input, inputLen, mdBuf->bytes); break;
        case kCCDigestSHA384:	CC_SHA384(input, inputLen, mdBuf->bytes); break;
        case kCCDigestSHA512:	CC_SHA512(input, inputLen, mdBuf->bytes); break;
        default: return 1;
    }
    sprintf(outbuf, "Legacy %s test for %s", digestName(digestSelector), input);
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);

    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("Legacy FAIL: %s(\"%s\") expected %s got %s\n", digestName(digestSelector), input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // printf("Legacy PASS: %s(\"%s\")\n", digestName(digestSelector), input);
    }
    
    switch(digestSelector) {
        case kCCDigestMD2:		OLD_ALL_IN_ONE_HASH(MD2, input, inputLen, mdBuf->bytes);
        case kCCDigestMD4:		OLD_ALL_IN_ONE_HASH(MD4, input, inputLen, mdBuf->bytes);
        case kCCDigestMD5:		OLD_ALL_IN_ONE_HASH(MD5, input, inputLen, mdBuf->bytes);
        case kCCDigestSHA1:	OLD_ALL_IN_ONE_HASH(SHA1, input, inputLen, mdBuf->bytes);
        case kCCDigestSHA224:	OLD_ALL_IN_ONE_HASH(SHA224, input, inputLen, mdBuf->bytes);
        case kCCDigestSHA256:	OLD_ALL_IN_ONE_HASH(SHA256, input, inputLen, mdBuf->bytes);
        case kCCDigestSHA384:	OLD_ALL_IN_ONE_HASH(SHA384, input, inputLen, mdBuf->bytes);
        case kCCDigestSHA512:	OLD_ALL_IN_ONE_HASH(SHA512, input, inputLen, mdBuf->bytes);
        default: return 1;
    }
    sprintf(outbuf, "Legacy %s test for %s", digestName(digestSelector), input);
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("Legacy FAIL: %s(\"%s\") expected %s got %s\n", digestName(digestSelector), input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // printf("Legacy PASS: %s(\"%s\")\n", digestName(digestSelector), input);
    }

   free(mdBuf);
   free(expectedBytes);
   return retval;
}

static int
rfc1321Test(char *input, char *expected)
{
    CC_LONG inputLen = (CC_LONG) nullstrlen(input);
    char outbuf[80];
    int retval = 0;
    MD5_CTX ctx;
    byteBuffer expectedBytes = hexStringToBytes(expected);
    byteBuffer mdBuf = mallocByteBuffer(CC_MD5_DIGEST_LENGTH); 
    
    MD5Init(&ctx);
    MD5Update(&ctx, input, inputLen);
    MD5Final(mdBuf->bytes, &ctx);
    
    sprintf(outbuf, "Legacy MD5-1321 test for %s", input);
    
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("Legacy FAIL: MD5-1321(\"%s\") expected %s got %s\n", input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // printf("Legacy PASS: MD5-1321(\"%s\")\n", input);
    }
    free(mdBuf);
    free(expectedBytes);
    return retval;
}


static int kTestTestCount = 316;

int CommonDigest(int argc, char *const *argv) {
	char *strvalue, *keyvalue;
	plan_tests(kTestTestCount);
    int accum = 0;
    
	/* strvalue of NULL and strvalue of "" must end up the same */
    strvalue = NULL;
    accum |= hashTest(strvalue, kCCDigestMD2, "8350e5a3e24c153df2275c9f80692773");
    accum |= hashTest(strvalue, kCCDigestMD4, "31d6cfe0d16ae931b73c59d7e0c089c0");
    accum |= hashTest(strvalue, kCCDigestMD5, "d41d8cd98f00b204e9800998ecf8427e");
    accum |= hashTest(strvalue, kCCDigestSHA1, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    accum |= hashTest(strvalue, kCCDigestSHA224, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
    accum |= hashTest(strvalue, kCCDigestSHA256, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    accum |= hashTest(strvalue, kCCDigestSHA384, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
    accum |= hashTest(strvalue, kCCDigestSHA512, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
#if (CCDIGEST == 1)
    accum |= newHashTest(strvalue, kCCDigestMD2, "8350e5a3e24c153df2275c9f80692773");
    accum |= newHashTest(strvalue, kCCDigestMD4, "31d6cfe0d16ae931b73c59d7e0c089c0");
    accum |= newHashTest(strvalue, kCCDigestMD5, "d41d8cd98f00b204e9800998ecf8427e");
    accum |= newHashTest(strvalue, kCCDigestSHA1, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    accum |= newHashTest(strvalue, kCCDigestSHA224, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
    accum |= newHashTest(strvalue, kCCDigestSHA256, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    accum |= newHashTest(strvalue, kCCDigestSHA384, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
    accum |= newHashTest(strvalue, kCCDigestSHA512, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
    accum |= newHashTest(strvalue, kCCDigestRMD128, "cdf26213a150dc3ecb610f18f6b38b46");
    accum |= newHashTest(strvalue, kCCDigestRMD160, "9c1185a5c5e9fc54612808977ee8f548b2258d31");
    accum |= newHashTest(strvalue, kCCDigestRMD256, "02ba4c4e5f8ecd1877fc52d64d30e37a2d9774fb1e5d026380ae0168e3c5522d");
    accum |= newHashTest(strvalue, kCCDigestRMD320, "22d65d5661536cdc75c1fdf5c6de7b41b9f27325ebc61e8557177d705a0ec880151c3a32a00899b8");
#else
    diag("No Testing of the new Digest Interfaces\n");
#endif

    strvalue = "";
    accum |= hashTest(strvalue, kCCDigestMD2, "8350e5a3e24c153df2275c9f80692773");
    accum |= hashTest(strvalue, kCCDigestMD4, "31d6cfe0d16ae931b73c59d7e0c089c0");
    accum |= hashTest(strvalue, kCCDigestMD5, "d41d8cd98f00b204e9800998ecf8427e");
    accum |= hashTest(strvalue, kCCDigestSHA1, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    accum |= hashTest(strvalue, kCCDigestSHA224, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
    accum |= hashTest(strvalue, kCCDigestSHA256, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    accum |= hashTest(strvalue, kCCDigestSHA384, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
    accum |= hashTest(strvalue, kCCDigestSHA512, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
#if (CCDIGEST == 1)
    accum |= newHashTest(strvalue, kCCDigestMD2, "8350e5a3e24c153df2275c9f80692773");
    accum |= newHashTest(strvalue, kCCDigestMD4, "31d6cfe0d16ae931b73c59d7e0c089c0");
    accum |= newHashTest(strvalue, kCCDigestMD5, "d41d8cd98f00b204e9800998ecf8427e");
    accum |= newHashTest(strvalue, kCCDigestSHA1, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    accum |= newHashTest(strvalue, kCCDigestSHA224, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
    accum |= newHashTest(strvalue, kCCDigestSHA256, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    accum |= newHashTest(strvalue, kCCDigestSHA384, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
    accum |= newHashTest(strvalue, kCCDigestSHA512, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
    accum |= newHashTest(strvalue, kCCDigestRMD128, "cdf26213a150dc3ecb610f18f6b38b46");
    accum |= newHashTest(strvalue, kCCDigestRMD160, "9c1185a5c5e9fc54612808977ee8f548b2258d31");
    accum |= newHashTest(strvalue, kCCDigestRMD256, "02ba4c4e5f8ecd1877fc52d64d30e37a2d9774fb1e5d026380ae0168e3c5522d");
    accum |= newHashTest(strvalue, kCCDigestRMD320, "22d65d5661536cdc75c1fdf5c6de7b41b9f27325ebc61e8557177d705a0ec880151c3a32a00899b8");
#if defined(TESTSKEIN)

    accum |= newHashTest(strvalue, kCCDigestSkein128, "030085a5c5e9fc54612808977ee8f548");
    accum |= newHashTest(strvalue, kCCDigestSkein160, "030085a5c5e9fc54612808977ee8f548b2258d31");
    accum |= newHashTest(strvalue, kCCDigestSkein224, "030085a5c5e9fc54612808977ee8f548b2258d31009b934ca495991b");
    accum |= newHashTest(strvalue, kCCDigestSkein256, "0900d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
    accum |= newHashTest(strvalue, kCCDigestSkein384, "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
    accum |= newHashTest(strvalue, kCCDigestSkein512, "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
#else
    accum |= unHashTest(kCCDigestSkein128);
    accum |= unHashTest(kCCDigestSkein160);
    accum |= unHashTest(kCCDigestSkein224);
    accum |= unHashTest(kCCDigestSkein256);
    accum |= unHashTest(kCCDigestSkein384);
    accum |= unHashTest(kCCDigestSkein512);
#endif
#else
    diag("No Testing of the new Digest Interfaces\n");
#endif

    strvalue = "Test vector from febooti.com";
    accum |= hashTest(strvalue, kCCDigestMD2, "db128d6e0d20a1192a6bd1fade401150");
    accum |= hashTest(strvalue, kCCDigestMD4, "6578f2664bc56e0b5b3f85ed26ecc67b");
    accum |= hashTest(strvalue, kCCDigestMD5, "500ab6613c6db7fbd30c62f5ff573d0f");
    accum |= hashTest(strvalue, kCCDigestSHA1, "a7631795f6d59cd6d14ebd0058a6394a4b93d868");
    accum |= hashTest(strvalue, kCCDigestSHA224, "3628b402254caa96827e3c79c0a559e4558da8ee2b65f1496578137d");
    accum |= hashTest(strvalue, kCCDigestSHA256, "077b18fe29036ada4890bdec192186e10678597a67880290521df70df4bac9ab");
    accum |= hashTest(strvalue, kCCDigestSHA384, "388bb2d487de48740f45fcb44152b0b665428c49def1aaf7c7f09a40c10aff1cd7c3fe3325193c4dd35d4eaa032f49b0");
    accum |= hashTest(strvalue, kCCDigestSHA512, "09fb898bc97319a243a63f6971747f8e102481fb8d5346c55cb44855adc2e0e98f304e552b0db1d4eeba8a5c8779f6a3010f0e1a2beb5b9547a13b6edca11e8a");
    accum |= rfc1321Test(strvalue, "500ab6613c6db7fbd30c62f5ff573d0f");


#if (CCDIGEST == 1)
    accum |= newHashTest(strvalue, kCCDigestMD2, "db128d6e0d20a1192a6bd1fade401150");
    accum |= newHashTest(strvalue, kCCDigestMD4, "6578f2664bc56e0b5b3f85ed26ecc67b");
    accum |= newHashTest(strvalue, kCCDigestMD5, "500ab6613c6db7fbd30c62f5ff573d0f");
    accum |= newHashTest(strvalue, kCCDigestSHA1, "a7631795f6d59cd6d14ebd0058a6394a4b93d868");
    accum |= newHashTest(strvalue, kCCDigestSHA224, "3628b402254caa96827e3c79c0a559e4558da8ee2b65f1496578137d");
    accum |= newHashTest(strvalue, kCCDigestSHA256, "077b18fe29036ada4890bdec192186e10678597a67880290521df70df4bac9ab");
    accum |= newHashTest(strvalue, kCCDigestSHA384, "388bb2d487de48740f45fcb44152b0b665428c49def1aaf7c7f09a40c10aff1cd7c3fe3325193c4dd35d4eaa032f49b0");
    accum |= newHashTest(strvalue, kCCDigestSHA512, "09fb898bc97319a243a63f6971747f8e102481fb8d5346c55cb44855adc2e0e98f304e552b0db1d4eeba8a5c8779f6a3010f0e1a2beb5b9547a13b6edca11e8a");
#if defined(TESTSKEIN)
    accum |= newHashTest(strvalue, kCCDigestSkein128, "03000000000000700000000000000070");
    accum |= newHashTest(strvalue, kCCDigestSkein160, "0300000000000070000000000000007000000000");
    accum |= newHashTest(strvalue, kCCDigestSkein224, "030000000000007000000000000000700000000000000070ca030210");
    accum |= newHashTest(strvalue, kCCDigestSkein256, "0000000000000000000000000000000000000000000000000000000000000000");
    accum |= newHashTest(strvalue, kCCDigestSkein384, "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
    accum |= newHashTest(strvalue, kCCDigestSkein512, "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
#endif

#else
    diag("No Testing of the new Digest Interfaces\n");
#endif

	// Test Case 1 http://www.faqs.org/rfcs/rfc4231.html
    strvalue = "Hi There";
    keyvalue = "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b";
	accum |= HMACTest(strvalue, keyvalue, kCCDigestSHA224, "896fb1128abbdf196832107cd49df33f47b4b1169912ba4f53684b22");   
	accum |= HMACTest(strvalue, keyvalue, kCCDigestSHA256, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");   
	accum |= HMACTest(strvalue, keyvalue, kCCDigestSHA384, "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6");   
	accum |= HMACTest(strvalue, keyvalue, kCCDigestSHA512, "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");
    // Test Vector from http://www.faqs.org/rfcs/rfc2104.html
    keyvalue = "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b";
    accum |= HMACTest(strvalue, keyvalue, kCCDigestMD5, "9294727a3638bb1c13f48ef8158bfc9d");   
    
#if (CCKEYDERIVATION == 1)
    // Test Case PBKDF2 - HMACSHA1 http://tools.ietf.org/html/draft-josefsson-pbkdf2-test-vectors-00
    accum |= PBKDF2Test("password", (uint8_t *) "salt", 4, 1, kCCDigestSHA1, 20, "0c60c80f961f0e71f3a9b524af6012062fe037a6");
    accum |= PBKDF2Test("password", (uint8_t *) "salt", 4, 2, kCCDigestSHA1, 20, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957");
    accum |= PBKDF2Test("password", (uint8_t *) "salt", 4, 4096, kCCDigestSHA1, 20, "4b007901b765489abead49d926f721d065a429c1");
    
    // This crashes
    accum |= PBKDF2Test("password", (uint8_t *) "salt", 4, 1, 0, 20, NULL);
#else
    diag("No Key Derivation Support Testing\n");
#endif
    
    // Test from <rdar://problem/11285435> CC_SHA512_Init(),CC_SHA512_Update(),CC_SHA512_Final() gives wrong digest
    strvalue = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    accum |= hashTest(strvalue, kCCDigestSHA512, "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
    accum |= newHashTest(strvalue, kCCDigestSHA512, "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");

    return accum;

}
