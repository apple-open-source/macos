#include <Availability.h>
#include "capabilities.h"
#include "testmore.h"
#include "testbyteBuffer.h"

#if (CCDH == 0)
entryPoint(CommonDH,"Diffie-Hellman Key Agreement")
#else

#include "CommonDH.h"
static int kTestTestCount = 8;

int CommonDH(int argc, char *const *argv) {
    CCDHRef dh1, dh2;

    plan_tests(kTestTestCount);
    
    
    dh1 = CCDHCreate(kCCDHRFC2409Group2);
    ok(dh1 == NULL, "kCCDHRFC2409Group2 isn't implemented in corecrypto");

    dh1 = CCDHCreate(kCCDHRFC3526Group5);
    ok(dh1 != NULL, "got a DH ref");
    
    dh2 = CCDHCreate(kCCDHRFC3526Group5);
    ok(dh2 != NULL, "got a DH ref");
    

    uint8_t pubkey1[4096], pubkey2[4096];
    size_t len1 = 4096, len2 = 4096;
    int ret1 = CCDHGenerateKey(dh1, pubkey1, &len1);
    int ret2 = CCDHGenerateKey(dh2, pubkey2, &len2);

    ok(ret1 != -1 && ret2 != -1, "pubkeys generated");
    
    uint8_t sharedkey1[4096], sharedkey2[4096];
    size_t slen1 = 4096, slen2 = 4096;

    int sret1 = CCDHComputeKey(sharedkey1, &slen1, pubkey2, len2, dh1);
    int sret2 = CCDHComputeKey(sharedkey2, &slen2, pubkey1, len1, dh2);
    
    ok(sret1 != -1 && sret2 != -1, "shared keys generated");

    ok(slen1 == slen2, "shared key lengths are equal");
    
    ok(memcmp(sharedkey1, sharedkey2, slen1) == 0, "shared keys are equal");
    
    
    
    CCDHRelease(dh1);
    ok(1, "Didn't crash");


    return 0;
}

#endif
