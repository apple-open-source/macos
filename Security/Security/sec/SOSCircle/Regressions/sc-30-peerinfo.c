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

static int kTestTestCount = 11;
static void tests(void)
{
    SecKeyRef signingKey = NULL;
    SOSFullPeerInfoRef fpi = SOSCreateFullPeerInfoFromName(CFSTR("Test Peer"), &signingKey, NULL);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFRetainSafe(pi);
    
    ok(NULL != pi, "info creation");
    
    uint8_t buffer[4096];
    
    const uint8_t *buffer_p = SOSPeerInfoEncodeToDER(pi, NULL, buffer, buffer + sizeof(buffer));
    
    ok(buffer_p != NULL, "encode");
    
    SOSPeerInfoRef pi2 = SOSPeerInfoCreateFromDER(NULL, NULL, &buffer_p, buffer + sizeof(buffer));
    
    SKIP:
    {
        skip("Decode failed", 1, ok(NULL != pi2, "Decode"));
        ok(CFEqual(pi, pi2), "Decode matches");
    }
    
    buffer_p = SOSFullPeerInfoEncodeToDER(fpi, NULL, buffer, buffer + sizeof(buffer));

    ok(buffer_p != NULL, "Full peer encode");

    SOSFullPeerInfoRef  fpi2 = SOSFullPeerInfoCreateFromDER(kCFAllocatorDefault, NULL, &buffer_p, buffer + sizeof(buffer));

    SKIP:
    {
        skip("Full Peer Decode failed", 1, ok(fpi2, "Full peer inflated"));

        ok(CFEqual(fpi, fpi2), "Full peer inflate matches");
    }


// Application ticket time.
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFErrorRef error = NULL;

    CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
    ok(parameters, "No parameters!");
    ok(error == NULL, "Error: (%@)", error);
    CFReleaseNull(error);

    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, &error);
    CFReleaseSafe(cfpassword);
    CFReleaseNull(parameters);
    SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);

    ok(SOSFullPeerInfoPromoteToApplication(fpi, user_privkey, &error), "Promote to Application");
    ok(SOSPeerInfoApplicationVerify(SOSFullPeerInfoGetPeerInfo(fpi), user_pubkey, &error), "Promote to Application");


    CFReleaseNull(user_privkey);
    CFReleaseNull(user_pubkey);

    CFReleaseNull(signingKey);
    CFReleaseNull(pi);
    CFReleaseNull(pi2);
    CFReleaseNull(fpi);
    CFReleaseNull(fpi2);
}

int sc_30_peerinfo(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();

	return 0;
}
