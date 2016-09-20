/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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



#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>

#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

static int kTestGenerationCount = 2;
static void test_generation(void)
{
    SOSGenCountRef generation = SOSGenerationCreate();
    SOSGenCountRef newerGeneration = SOSGenerationCreateWithBaseline(generation);
    SOSGenCountRef evenNewerGeneration = SOSGenerationCreateWithBaseline(newerGeneration);

    ok(SOSGenerationIsOlder(generation, newerGeneration), "should be older");
    ok(SOSGenerationIsOlder(newerGeneration, evenNewerGeneration), "should be older");

    CFReleaseNull(generation);
    CFReleaseNull(newerGeneration);
    CFReleaseNull(evenNewerGeneration);
}


static int kTestTestCount = 26;
static void tests(void)
{
    SOSCircleRef circle = SOSCircleCreate(NULL, CFSTR("TEST DOMAIN"), NULL);
    
    ok(NULL != circle, "Circle creation");

    ok(0 == SOSCircleCountPeers(circle), "Zero peers");

    //SecKeyRef publicKey = NULL;
    SecKeyRef dev_a_key = NULL;
    SecKeyRef dev_b_key = NULL;
    SecKeyRef dev_c_key = NULL;
    SecKeyRef dev_d_key = NULL;
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    
    ok(cfpassword, "no password");
    
    CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
    ok(parameters, "No parameters!");
    ok(error == NULL, "Error: (%@)", error);
    CFReleaseNull(error);

    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, &error);
    CFReleaseNull(parameters);

    SOSFullPeerInfoRef peer_a_full_info = SOSCreateFullPeerInfoFromName(CFSTR("Peer A"), &dev_a_key, NULL);
    
    SOSFullPeerInfoRef peer_b_full_info = SOSCreateFullPeerInfoFromName(CFSTR("Peer B"), &dev_b_key, NULL);

    SOSFullPeerInfoRef peer_c_full_info = SOSCreateFullPeerInfoFromName(CFSTR("Peer C"), &dev_c_key, NULL);

    SOSFullPeerInfoRef peer_d_full_info = SOSCreateFullPeerInfoFromName(CFSTR("Peer D"), &dev_d_key, NULL);

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
    CFReleaseNull(inflated);
    
    ok(SOSCircleRemovePeer(circle, user_privkey, peer_a_full_info, SOSFullPeerInfoGetPeerInfo(peer_a_full_info), NULL));
    ok(SOSCircleCountPeers(circle) == 0, "Peer count");

    // Try multiple peer removal:

    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_a_full_info, NULL));
    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_b_full_info, NULL));
    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_c_full_info, NULL));

    ok(SOSCircleAcceptRequests(circle, user_privkey, peer_a_full_info, NULL));

    ok(SOSCircleRequestAdmission(circle, user_privkey, peer_d_full_info, NULL));

    CFArrayRef peer_array = CFArrayCreateForCFTypes(kCFAllocatorDefault,
                                                     SOSFullPeerInfoGetPeerInfo(peer_b_full_info),
                                                     SOSFullPeerInfoGetPeerInfo(peer_c_full_info),
                                                     SOSFullPeerInfoGetPeerInfo(peer_d_full_info),
                                                     NULL);

    CFSetRef peers_to_remove = CFSetCreateMutableForSOSPeerInfosByIDWithArray(kCFAllocatorDefault, peer_array);
    CFReleaseNull(peer_array);

    ok(SOSCircleRemovePeers(circle, user_privkey, peer_a_full_info, peers_to_remove, NULL));
    CFReleaseNull(peers_to_remove);

    ok(SOSCircleCountPeers(circle) == 1);
    ok(SOSCircleCountApplicants(circle) == 0);

    CFReleaseNull(dev_a_key);
    CFReleaseNull(dev_b_key);
    CFReleaseNull(dev_c_key);
    CFReleaseNull(dev_d_key);

    CFReleaseNull(cfpassword);

    CFReleaseNull(peer_a_full_info);
    CFReleaseNull(peer_b_full_info);
    CFReleaseNull(peer_c_full_info);
    CFReleaseNull(peer_d_full_info);

    CFReleaseNull(user_privkey);
    CFReleaseNull(circle);
}

int sc_40_circle(int argc, char *const *argv)
{
    plan_tests(kTestGenerationCount + kTestTestCount);

    test_generation();

    tests();

	return 0;
}
