/*
 *  sc-01-create.c
 *
 *  Created by Mitch Adler on 1/25/121.
 *  Copyright 2012 Apple Inc. All rights reserved.
 *
 */


#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>

#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

static int kTestTestCount = 17;
static void tests(void)
{
    SOSCircleRef circle = SOSCircleCreate(NULL, CFSTR("TEST DOMAIN"), NULL);
    
    ok(NULL != circle, "Circle creation");

    ok(0 == SOSCircleCountPeers(circle), "Zero peers");

    //SecKeyRef publicKey = NULL;
    SecKeyRef dev_a_key = NULL;
    SecKeyRef dev_b_key = NULL;
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    
    if(cfpassword == NULL) printf("WTF\n");
    
    CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
    ok(parameters, "No parameters!");
    ok(error == NULL, "Error: (%@)", error);
    CFReleaseNull(error);

    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, &error);
    CFReleaseNull(parameters);

    SOSFullPeerInfoRef peer_a_full_info = SOSCreateFullPeerInfoFromName(CFSTR("Peer A"), &dev_a_key, NULL);
    
    SOSFullPeerInfoRef peer_b_full_info = SOSCreateFullPeerInfoFromName(CFSTR("Peer B"), &dev_b_key, NULL);
    
    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_a_full_info, NULL));
    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_a_full_info, NULL));
    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_a_full_info, NULL));

    ok(SOSCircleAcceptRequest(circle, user_privkey, peer_a_full_info, SOSFullPeerInfoGetPeerInfo(peer_a_full_info), NULL));
    
    ok(!SOSCircleRequestAdmission(circle, user_privkey, peer_a_full_info, NULL));
    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_b_full_info, NULL));
    
    ok(SOSCircleCountPeers(circle) == 1, "Peer count");

    size_t size = SOSCircleGetDEREncodedSize(circle, &error);
    uint8_t buffer[size];
    uint8_t* start = SOSCircleEncodeToDER(circle, &error, buffer, buffer + sizeof(buffer));
    
    ok(start, "successful encoding");
    ok(start == buffer, "Used whole buffer");
    
    const uint8_t *der = buffer;
    SOSCircleRef inflated = SOSCircleCreateFromDER(NULL, &error, &der, buffer + sizeof(buffer));
    
    ok(inflated, "inflated");
    ok(CFEqualSafe(inflated, circle), "Compares");
    
    
    ok(SOSCircleRemovePeer(circle, user_privkey, peer_a_full_info, SOSFullPeerInfoGetPeerInfo(peer_a_full_info), NULL));
    ok(SOSCircleCountPeers(circle) == 0, "Peer count");
    
    CFReleaseNull(dev_a_key);
    CFReleaseNull(cfpassword);
}

int sc_40_circle(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();

	return 0;
}
