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
 *  CBCTest.c
 *  CommonCrypto
 */

#include "CBCTest.h"

static void
doCBCTestCase(int caseNumber, int direction, int dataLenBits, char *ivStr, char *cipherText, char *plainText, char *keyStr)
{
	char keyString[300];
    int ckLen, keyLength;
    byteBuffer key, tweak, iv;
    byteBuffer pt, ct;
	CCCryptorRef encCryptorRef;
	CCCryptorStatus retval;
    char dataOut[4096];
    int passed = 0;
    int dataLen;
    size_t dataOutMoved;
    
    ckLen = strlen(keyStr);
    strncpy(keyString, keyStr, ckLen);
    keyString[ckLen] = 0;
    
    keyLength = ckLen/2;
    
    key = hexStringToBytes(keyString);
    tweak = NULL;
    iv = hexStringToBytes(ivStr);
    
    encCryptorRef = NULL;
    
    if(plainText) pt = hexStringToBytes(plainText);
    else pt=NULL;
    if(cipherText) ct = hexStringToBytes(cipherText);
    else ct=NULL;
    
    
    printf("\n\nKey       %s\n", keyStr);
    printf("IV        %s\n", ivStr);
    printf("Plaintext %s\n", plainText);
    
    if((retval = CCCryptorCreateWithMode(0, kCCModeCBC, kCCAlgorithmAES128, ccDefaultPadding, NULL, key->bytes, key->len, tweak, 0, 0, 0,  &encCryptorRef)) == kCCSuccess) {
        if(direction == ENCRYPT) {
            dataLen = pt->len;
            if((retval = CCCryptorEncryptDataBlock(encCryptorRef, iv->bytes, pt->bytes, dataLen, dataOut)) == kCCSuccess) {
                byteBuffer bb = bytesToBytes(dataOut, dataLen);
                if(!ct) {
                    printf("Output    %s\n", bytesToHexString(bb));
                    passed = 3;
                }
                else if (!bytesAreEqual(ct, bb))
                    printf("Encrypt (%d) Output %s\nEncrypt (%d) Expect %s\n", dataLen, bytesToHexString(bb), dataLen, cipherText);
                else 
                    passed = 1;
            } else  printf("Failed to encrypt %d\n", retval);
        } else {
            dataLen = ct->len;
            if((retval = CCCryptorDecryptDataBlock(encCryptorRef, iv->bytes, ct->bytes, dataLen, dataOut)) == kCCSuccess) {
                byteBuffer bb = bytesToBytes(dataOut, dataLen);
                if(!pt) {
                    printf("Output    %s\n", bytesToHexString(bb));
                    passed = 3;
                }
                else if (!bytesAreEqual(pt, bb)) 
                    printf("Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), plainText);
                else passed = 1;
        	} else  printf("Failed to decrypt %d\n", retval);
    	}
        
        if((retval = CCCryptorFinal(encCryptorRef, dataOut, 0, &dataOutMoved)) != kCCSuccess) printf("Finalize failed\n");
    } else {
        printf("Failed to create Cryptor\n");
    }
    
    
	if(passed != 3) printf("Case %d Direction %s DataLen %d Test %s\n", caseNumber, (direction == ENCRYPT) ? "Encrypt": "Decrypt", dataLen, (passed) ? "Pass": "Fail");
    else printf("\n");
    free(pt);
    free(ct);
    free(key);
    free(tweak);
    free(iv);
    
}

int main (int argc, const char * argv[]) {
    int direction;
	int caseNumber;
	int dataLen;
	char *keyStr;
	char *iv;
	char *plainText;
	char *cipherText;
    
    direction = DECRYPT;
    caseNumber = 500;
	dataLen = 8192;
	keyStr = "badfd2102e1e180a634204249c5a6933";
	iv = "84c06c16c151007ca9ed9bb926e66eec";
	cipherText = "4b52b5e85aecaaaf886bd9e8805390c62e12e13357e4beb3b713e37d217c6f7a9e432a04f87bd8a4dd0ef79eb7bf41b5a2a27e63361d7cb7af7b3c9a8f0b56ae27dc9cfd6c10eb1a79c7be35d31c3965b8e7099775f7644029bd79321f5dd12c55280a30fabd1b95e27c2d4dec6ca4d8716f36e7abe3408f5120560b573e5495ae7aad668fa84d6a8a1156c231a5b6d983ece3e27d199a806dc629c1a60c08ccb0e4807d9fed88f28ce0f59583708f540f97110b2620b1679220abe13e3c4b727186b289794583b20154ce9a07a284df3e63572f462142cae8949d7dd6f2b26fb90d556ec75e93dd33b59d697883312af89e52945b9baedfebe28759cdba4dfbf6e6f201b087478642cf0b34f983593c68947e4ee05bd17716e6cfb7c74c876c0ba650f3979f5eceb72a71d0d46aac4474ae2048d2a9884aa12e292950c77b17de11e8d3e895e60b1c584b1c8d9edd40ba7917e396d1d3bfd1941923aa40213195e8b8f7f4d5ae1057cbecdf89c8959745d1fcece59115819dc661e7b097c132e8f98720a57a83469cb82c374fdebc97badd7cef8d160a7f27d50f35b7e4af6f1b78361828e32a55b25fd56efbc12f8fcb7e2e4f882afa0c7747a455a1fae00a561cbb878e01b32fafb23f397371a8b3441c8da654b902d8489383542188821859a44f0fb2b63a49835f8ba5f0231ff0f8f5fc3d5c812331b11e39bc03394e28";
	plainText  = "fb58510beb65062c525a3de42d934d4b4ec433d600a1467142751886a10e7bf96f236c196d12dcf0698e09efc79a4bea072bc0830da8886674cf6174206cca2d4e9e543f0016ec4dcc602ffd0a417c722879e259497f89aee5ad99a4f65887058242250fbe44f61eab5e668adbb780a4cba97393f6ff152c13c39b57ed727bb94cf19d1b4a55f45cceb22b6c4f26f736d20a48cb6230578591c8d33d72b778d30b304818b20d918ef654cabeae1038f2a0db5170d2b4df38c6efc887bc1f837fba34e97daf8920414b748a909ad5ef56fb47fa53c680aae808f3e6065689339728251e18cd264f5385c969f87104099563a411cfe681d19134e9479e059d09b69f5010912291d0232f733a2688b3042ec4e82ce5163c384ee54a9f10e48a8ab46fd7147351dd8514bda5d8c4ce8babcc3ef82dbf44799fc59e37d8f3c99506d2168c84d8381f4f9a84cbce7bd0bb4bbbcdef0c626356d3ca126c8776e3a291881af518e23dbd067016c5898bed5f64d6e8f8acefba83f92b0c318ec7b905165fb6b81bc60528c0a0e3db38ab1ee6f37e56dbf270c0751674e0ddb1a6076d8f78084ce31f0d3673e638e0110575b16d9d9f151c1b9aca8d15d7a8111c0de5acf5ae3b307e8064c90329e421e3434a1ecd253b153447c21c79c9946666dae444c49a31b1f94da603a8377168dc4f874e98fff5ae89dd35d44e89df5748223b7a24";
    
    
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);
    
    /* taken from the AES_KAT test file CBCVarKey128e.txt */
    printf("\nIV and Plaintext == 00000...\n");
    direction = ENCRYPT;
    caseNumber = 3;
	dataLen = 256;
    keyStr = "f0000000000000000000000000000000";
    iv = "00000000000000000000000000000000";
    plainText = "00000000000000000000000000000000";
    cipherText = "970014d634e2b7650777e8e84d03ccd8";
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);

    
    /* taken from the AES_KAT test file CBCVarKey128e.txt */
    printf("\nIV  == a0a0..\n");
    direction = ENCRYPT;
    caseNumber = 3;
	dataLen = 256;
    keyStr = "f0000000000000000000000000000000";
    iv =        "a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    plainText = "00000000000000000000000000000000";
    cipherText = NULL;
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);

    /* taken from the AES_KAT test file CBCVarKey128e.txt */
    printf("\nplaintext  == a0a0..\n");
    direction = ENCRYPT;
    caseNumber = 3;
	dataLen = 256;
    keyStr = "f0000000000000000000000000000000";
    iv =        "00000000000000000000000000000000";
    plainText = "a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    cipherText = NULL;
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);

    printf("\nIV and Plaintext == 00 key is all F's\n");
    direction = ENCRYPT;
    caseNumber = 3;
	dataLen = 256;
    keyStr =    "ffffffffffffffffffffffffffffffff";
    iv =        "00000000000000000000000000000000";
    plainText = "00000000000000000000000000000000";
    cipherText = NULL;
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);

    direction = ENCRYPT;
    caseNumber = 3;
	dataLen = 512;
    keyStr =    "ffffffffffffffffffffffffffffffff";
    iv =        "00000000000000000000000000000000";
    plainText = "00000000000000000000000000000000a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    cipherText = NULL;
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);

    direction = ENCRYPT;
    caseNumber = 3;
	dataLen = 512;
    keyStr =    "ffffffffffffffffffffffffffffffff";
    iv =        "a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    plainText = "00000000000000000000000000000000a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0";
    cipherText = NULL;
	doCBCTestCase(caseNumber, direction, dataLen, iv, cipherText, plainText, keyStr);
    return 0;
}
