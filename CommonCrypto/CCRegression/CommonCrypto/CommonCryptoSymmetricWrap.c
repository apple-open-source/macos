//
//  CommonCryptoSymmetricWrap.c
//  CCRegressions
//
//  Created by Richard Murphy on 1/13/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"

#if (CCSYMWRAP == 0)
entryPoint(CommonSymmetricWrap,"Symmetric Wrap")
#else

#include <CommonCrypto/CommonSymmetricKeywrap.h>


static int
wrapTest(char *kekstr, char *keystr, char *wrapped_keystr)
{
    byteBuffer kek, key, wrapped_key, bb;    
        
    kek = hexStringToBytes(kekstr);
    key = hexStringToBytes(keystr);
    if(wrapped_keystr) wrapped_key = hexStringToBytes(wrapped_keystr);
    else wrapped_key = hexStringToBytes("0x00");
	const uint8_t *iv =  CCrfc3394_iv;
	const size_t ivLen = CCrfc3394_ivLen;
	size_t wrapped_size = CCSymmetricWrappedSize(kCCWRAPAES, key->len);
	uint8_t wrapped[wrapped_size];
    
    // printf("Wrapped Size %lu\n", wrapped_size);

    ok(CCSymmetricKeyWrap(kCCWRAPAES, iv , ivLen, kek->bytes, kek->len, key->bytes, key->len, wrapped, &wrapped_size) == 0, "function is successful");
    if(wrapped_keystr) {
        bb = bytesToBytes(wrapped, wrapped_size);
        if(!strcmp(wrapped_keystr, "")) printByteBuffer(bb, "Result: ");
        ok(bytesAreEqual(bb, wrapped_key), "Equal to expected wrapping");
        // printByteBuffer(bb, "Result: ");
        // printByteBuffer(wrapped_key, "Expected: ");
        free(bb);
    }
        
	size_t unwrapped_size = CCSymmetricUnwrappedSize(kCCWRAPAES, wrapped_size);
	uint8_t unwrapped[unwrapped_size];
    
    ok(CCSymmetricKeyUnwrap(kCCWRAPAES, iv, ivLen, kek->bytes, kek->len, wrapped, wrapped_size, unwrapped, &unwrapped_size) == 0, "function is successful");
    bb = bytesToBytes(unwrapped, unwrapped_size);
    ok(bytesAreEqual(bb, key), "Equal to original key");
    free(bb);
    free(kek);
    free(key);
    free(wrapped_key);

    return 0;
}





static int kTestTestCount = 35;

int
CommonSymmetricWrap(int argc, char *const *argv)
{
    char *kek, *key, *wrapped_key;
    int accum = 0;
	plan_tests(kTestTestCount);
    
    diag("Test 1");
    kek = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    key = "00112233445566778899aabbccddeeff000102030405060708090a0b0c0d0e0f";
    wrapped_key = "28C9F404C4B810F4CBCCB35CFB87F8263F5786E2D80ED326CBC7F0E71A99F43BFB988B9B7A02DD21";
    accum |= wrapTest(kek, key, wrapped_key);
    
    diag("Test 2");
    kek = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    key = "00112233445566778899aabbccddeeff00010203040506070";
    wrapped_key = "a8f9bc1612c68b3ff6e6f4fbe30e71e4769c8b80a32cb8958cd5d17d6b254da1";
    accum |= wrapTest(kek, key, wrapped_key);

    diag("Test 3");
    byteBuffer keybuf = mallocByteBuffer(2048);
    for(int i=0; i<2048; i++) keybuf->bytes[i] = i%256;
    key = bytesToHexString(keybuf);
    accum |= wrapTest(kek, key, NULL);
    
    diag("Test Vectors from RFC 3394");
    diag("4.1 Wrap 128 bits of Key Data with a 128-bit KEK");
    kek = "000102030405060708090A0B0C0D0E0F";
    key = "00112233445566778899AABBCCDDEEFF";
    wrapped_key = "1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5";
    accum |= wrapTest(kek, key, wrapped_key);

    diag("4.2 Wrap 128 bits of Key Data with a 192-bit KEK");
    kek = "000102030405060708090A0B0C0D0E0F1011121314151617";
    key = "00112233445566778899AABBCCDDEEFF";
    wrapped_key = "96778B25AE6CA435F92B5B97C050AED2468AB8A17AD84E5D";
    accum |= wrapTest(kek, key, wrapped_key);

    diag("4.3 Wrap 128 bits of Key Data with a 256-bit KEK");
    kek = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
    key = "00112233445566778899AABBCCDDEEFF";
    wrapped_key = "64E8C3F9CE0F5BA263E9777905818A2A93C8191E7D6E8AE7";
    accum |= wrapTest(kek, key, wrapped_key);

    diag("4.4 Wrap 192 bits of Key Data with a 192-bit KEK");
    kek = "000102030405060708090A0B0C0D0E0F1011121314151617";
    key = "00112233445566778899AABBCCDDEEFF0001020304050607";
    wrapped_key = "031D33264E15D33268F24EC260743EDCE1C6C7DDEE725A936BA814915C6762D2";
    accum |= wrapTest(kek, key, wrapped_key);

    diag("4.5 Wrap 192 bits of Key Data with a 256-bit KEK");
    kek = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
    key = "00112233445566778899AABBCCDDEEFF0001020304050607";
    wrapped_key = "A8F9BC1612C68B3FF6E6F4FBE30E71E4769C8B80A32CB8958CD5D17D6B254DA1";
    accum |= wrapTest(kek, key, wrapped_key);

    diag("4.6 Wrap 256 bits of Key Data with a 256-bit KEK");
    kek = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
    key = "00112233445566778899AABBCCDDEEFF000102030405060708090A0B0C0D0E0F";
    wrapped_key = "28C9F404C4B810F4CBCCB35CFB87F8263F5786E2D80ED326CBC7F0E71A99F43BFB988B9B7A02DD21";
    accum |= wrapTest(kek, key, wrapped_key);
    
    return 0;
}
#endif
