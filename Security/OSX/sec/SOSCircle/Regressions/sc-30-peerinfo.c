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

static CFDataRef CopyTestBackupKey(void) {
    static uint8_t data[] = { 'A', 'b', 'c' };

    return CFDataCreate(kCFAllocatorDefault, data, sizeof(data));
}

static bool PeerInfoRoundTrip(SOSPeerInfoRef pi) {
    bool retval = false;
    size_t size = SOSPeerInfoGetDEREncodedSize(pi, NULL);
    uint8_t buffer[size];
    const uint8_t *buffer_p = SOSPeerInfoEncodeToDER(pi, NULL, buffer, buffer + sizeof(buffer));
    ok(buffer_p != NULL, "encode");
    if(buffer_p == NULL) return false;
    SOSPeerInfoRef pi2 = SOSPeerInfoCreateFromDER(NULL, NULL, &buffer_p, buffer + sizeof(buffer));
    ok(pi2 != NULL, "decode");
    if(!pi2) return false;
    ok(CFEqual(pi, pi2), "Decode matches");
    if(CFEqual(pi, pi2)) retval = true;
    CFReleaseNull(pi2);
    return retval;
}

static bool FullPeerInfoRoundTrip(SOSFullPeerInfoRef fpi) {
    bool retval = false;
    size_t size = SOSFullPeerInfoGetDEREncodedSize(fpi, NULL);
    uint8_t buffer[size];
    const uint8_t *buffer_p = SOSFullPeerInfoEncodeToDER(fpi, NULL, buffer, buffer + sizeof(buffer));
    ok(buffer_p != NULL, "encode");
    if(buffer_p == NULL) return false;
    SOSFullPeerInfoRef fpi2 = SOSFullPeerInfoCreateFromDER(NULL, NULL, &buffer_p, buffer + sizeof(buffer));
    ok(fpi2 != NULL, "decode");
    if(!fpi2) return false;
    ok(CFEqual(fpi, fpi2), "Decode matches");
    if(CFEqual(fpi, fpi2)) retval = true;
    CFReleaseNull(fpi2);
    return retval;
}

static int kTestTestCount = 24;
static void tests(void)
{
    SecKeyRef signingKey = NULL;
    SOSFullPeerInfoRef fpi = SOSCreateFullPeerInfoFromName(CFSTR("Test Peer"), &signingKey, NULL);
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(fpi);

    ok(NULL != pi, "info creation");
    
    ok(PeerInfoRoundTrip(pi), "PeerInfo safely round-trips");
    ok(FullPeerInfoRoundTrip(fpi), "FullPeerInfo safely round-trips");

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
    
    pi = SOSFullPeerInfoGetPeerInfo(fpi);
    ok(PeerInfoRoundTrip(pi), "PeerInfo safely round-trips");

    CFDataRef testBackupKey = CopyTestBackupKey();

    ok(SOSFullPeerInfoUpdateBackupKey(fpi, testBackupKey, &error), "Set Backup (%@)", error);
    CFReleaseNull(error);

    CFReleaseNull(testBackupKey); // Make sure our ref doesn't save them.
    testBackupKey = CopyTestBackupKey();

    pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFDataRef piBackupKey = SOSPeerInfoCopyBackupKey(pi);

    ok(CFEqualSafe(testBackupKey, piBackupKey), "Same Backup Key");

    ok(PeerInfoRoundTrip(pi), "PeerInfo safely round-trips with backup key");

    CFReleaseNull(piBackupKey);
    piBackupKey = SOSPeerInfoCopyBackupKey(pi);
    ok(CFEqualSafe(testBackupKey, piBackupKey), "Same Backup Key after round trip");

    // Don't own the piBackupKey key
    CFReleaseNull(testBackupKey);
    CFReleaseNull(piBackupKey);
    CFReleaseNull(user_privkey);
    CFReleaseNull(user_pubkey);

    CFReleaseNull(signingKey);
    CFReleaseNull(fpi);
}

int sc_30_peerinfo(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();

	return 0;
}
