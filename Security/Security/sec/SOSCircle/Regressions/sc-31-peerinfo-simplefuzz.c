/*
 *  sc-31-peerinfo.c
 *
 *  Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 *
 */


#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

#if TARGET_OS_IPHONE
#include <MobileGestalt.h>
#endif

static unsigned long kTestCount = 2;
static unsigned long kTestFuzzerCount = 20000;

static void tests(void)
{
    SecKeyRef signingKey = NULL;
    SOSFullPeerInfoRef fpi = SOSCreateFullPeerInfoFromName(CFSTR("Test Peer"), &signingKey, NULL);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    unsigned long count;

    CFRetainSafe(pi);
    
    ok(NULL != pi, "info creation");
    
    uint8_t buffer[4096];
    
    const uint8_t *buffer_p = SOSPeerInfoEncodeToDER(pi, NULL, buffer, buffer + sizeof(buffer));
    
    ok(buffer_p != NULL, "encode");

    size_t length = (buffer + sizeof(buffer)) - buffer_p;
    
    uint8_t buffer2[length];
    if(buffer_p == NULL) goto errOut;

        for (count = 0; count < kTestFuzzerCount; count++) {
            memcpy(buffer2, buffer_p, length);

            const uint8_t *startp = buffer2;

            buffer2[arc4random_uniform((u_int32_t)length)] = arc4random() & 0xff;

            SOSPeerInfoRef pi2 = SOSPeerInfoCreateFromDER(NULL, NULL, &startp, buffer2 + length);
            CFReleaseNull(pi2);
            ok(1, "fuzz");
        }
    
errOut:
    CFReleaseNull(signingKey);
    CFReleaseNull(pi);
}

int sc_31_peerinfo(int argc, char *const *argv)
{
    plan_tests((int)(kTestCount + kTestFuzzerCount));
	
    tests();
    
	return 0;
}
