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
#include <SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>

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
    
    ok(NULL != pi, "info creation");
    size_t size = SOSPeerInfoGetDEREncodedSize(pi, NULL);

    uint8_t buffer[size+100]; // make the buffer long enough to hold the DER + some room for the fuzzing
    
    const uint8_t *buffer_p = SOSPeerInfoEncodeToDER(pi, NULL, buffer, buffer + sizeof(buffer));
    
    ok(buffer_p != NULL, "encode");

    size_t length = (buffer + sizeof(buffer)) - buffer_p;
    // diag("size %lu length %lu\n", size, length);
    uint8_t buffer2[length];
    if(buffer_p == NULL) goto errOut;

        for (count = 0; count < kTestFuzzerCount; count++) {
            memcpy(buffer2, buffer_p, length);

            const uint8_t *startp = buffer2;
            size_t offset = arc4random_uniform((u_int32_t)length);
            uint8_t value = arc4random() & 0xff;
            // diag("Offset %lu value %d\n", offset, value);
            buffer2[offset] = value;

            SOSPeerInfoRef pi2 = SOSPeerInfoCreateFromDER(NULL, NULL, &startp, buffer2 + length);
            CFReleaseNull(pi2);
            ok(1, "fuzz");
        }
    
errOut:
    CFReleaseNull(signingKey);
    CFReleaseNull(fpi);
}

int sc_31_peerinfo(int argc, char *const *argv)
{
    plan_tests((int)(kTestCount + kTestFuzzerCount));
	
    tests();
    
	return 0;
}
