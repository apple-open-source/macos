//
//  CCCMACtests.c
//  CCRegressions
//

#include <stdio.h>
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"

#if (CCCMAC == 0)
entryPoint(CommonCMac,"Common CMac")
#else

#include <CommonCrypto/CommonCMACSPI.h>

static int
CMACTest(char *input, char *keystr, char *expected)
{
    byteBuffer mdBuf;
    byteBuffer inputBytes, expectedBytes, keyBytes;
    char *digestName;
    char outbuf[160];
    int retval = 0;
    
    inputBytes = hexStringToBytes(input);
    expectedBytes = hexStringToBytes(expected);
    keyBytes = hexStringToBytes(keystr);
    mdBuf = mallocByteBuffer(CC_CMACAES_DIGEST_LENGTH); digestName = "CMAC-AES"; 
    CCAESCmac(keyBytes->bytes, inputBytes->bytes, inputBytes->len, mdBuf->bytes);
    
	sprintf(outbuf, "Hmac-%s test for %s", digestName, input);
    
    ok(bytesAreEqual(mdBuf, expectedBytes), outbuf);
    
    if(!bytesAreEqual(mdBuf, expectedBytes)) {
        diag("HMAC FAIL: HMAC-%s(\"%s\")\n expected %s\n      got %s\n", digestName, input, expected, bytesToHexString(mdBuf));
        retval = 1;
    } else {
        // diag("HMAC PASS: HMAC-%s(\"%s\")\n", digestName, input);
    }
    
    free(mdBuf);
    free(expectedBytes);
    free(keyBytes);
    free(inputBytes);
    return retval;
}

static int kTestTestCount = 4;

int CommonCMac (int argc, char *const *argv) {
	char *strvalue, *keyvalue;
	plan_tests(kTestTestCount);
    int accum = 0;
    
    strvalue = "";
    keyvalue = "2b7e151628aed2a6abf7158809cf4f3c";
	accum |= CMACTest(strvalue, keyvalue, "bb1d6929e95937287fa37d129b756746");   
    strvalue = "6bc1bee22e409f96e93d7e117393172a";
	accum |= CMACTest(strvalue, keyvalue, "070a16b46b4d4144f79bdd9dd04a287c");   
    strvalue = "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411";
	accum |= CMACTest(strvalue, keyvalue, "dfa66747de9ae63030ca32611497c827");   
    strvalue = "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710";
	accum |= CMACTest(strvalue, keyvalue, "51f0bebf7e3b9d92fc49741779363cfe");  
    
    return accum;
}
#endif


