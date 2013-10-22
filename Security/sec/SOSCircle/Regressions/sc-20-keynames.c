/*
 *  sc-20-keynames.c
 *
 *  Created by Mitch Adler on 1/25/121.
 *  Copyright 2012 Apple Inc. All rights reserved.
 *
 */


#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSInternal.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"


static int kTestTestCount = 15;
static void tests(void)
{
    SecKeyRef publicKey = NULL;
    SecKeyRef signingKey = NULL;
    GenerateECPair(256, &publicKey, &signingKey);
   
    CFErrorRef error = NULL;
    SOSCircleRef circle = SOSCircleCreate(NULL, CFSTR("Test Circle"), &error);
    
    CFStringRef circle_key = SOSCircleKeyCreateWithCircle(circle, NULL);

    CFStringRef circle_name = NULL;
    ok(circle_key, "Circle key created");
    is(SOSKVSKeyGetKeyType(circle_key), kCircleKey, "Is circle key");
    is(SOSKVSKeyGetKeyTypeAndParse(circle_key, &circle_name, NULL, NULL), kCircleKey, "Is circle key, extract name");
    ok(circle_name, "Circle name extracted");
    ok(CFEqualSafe(circle_name, SOSCircleGetName(circle)), "Circle name matches '%@' '%@'", circle_name, SOSCircleGetName(circle));
    
    CFReleaseSafe(circle_key);
    
    SOSPeerInfoRef pi = SOSCreatePeerInfoFromName(CFSTR("Test Peer"), &publicKey, &error);
    
    CFStringRef other_peer_id = CFSTR("OTHER PEER");
    
    CFStringRef messageKey = SOSMessageKeyCreateWithCircleAndPeerNames(circle, SOSPeerInfoGetPeerID(pi), other_peer_id);
    
    ok(messageKey, "Getting message key '%@'", messageKey);
    
    CFStringRef message_circle_name = NULL;
    CFStringRef message_from_peer_id = NULL;
    CFStringRef message_to_peer_id = NULL;

    is(SOSKVSKeyGetKeyType(messageKey), kMessageKey, "Is message key");
    is(SOSKVSKeyGetKeyTypeAndParse(messageKey,
                                   &message_circle_name,
                                   &message_from_peer_id,
                                   &message_to_peer_id), kMessageKey, "Is message key, extract parts");
    
    
    ok(CFEqualSafe(SOSCircleGetName(circle), message_circle_name), "circle key matches in message (%@ v %@)",SOSCircleGetName(circle), message_circle_name);

    
    ok(CFEqualSafe(SOSPeerInfoGetPeerID(pi), message_from_peer_id), "from peer set correctly (%@ v %@)", SOSPeerInfoGetPeerID(pi), message_from_peer_id);
    
    ok(CFEqualSafe(other_peer_id, message_to_peer_id), "to peer set correctly (%@ v %@)", other_peer_id, message_to_peer_id);
    
    CFStringRef retirementKey = SOSRetirementKeyCreateWithCircleAndPeer(circle, SOSPeerInfoGetPeerID(pi));
    CFStringRef retirement_circle_name = NULL;
    CFStringRef retirement_peer_id = NULL;
    
    is(SOSKVSKeyGetKeyType(retirementKey), kRetirementKey, "Is retirement key");
    is(SOSKVSKeyGetKeyTypeAndParse(retirementKey,
                                   &retirement_circle_name,
                                   &retirement_peer_id,
                                   NULL), kRetirementKey, "Is retirement key, extract parts");
    CFReleaseSafe(retirementKey);
    ok(CFEqualSafe(SOSCircleGetName(circle), retirement_circle_name), "circle key matches in retirement (%@ v %@)",
       SOSCircleGetName(circle), retirement_circle_name);
    ok(CFEqualSafe(SOSPeerInfoGetPeerID(pi), retirement_peer_id), "retirement peer set correctly (%@ v %@)",
       SOSPeerInfoGetPeerID(pi), retirement_peer_id);
    
    CFReleaseNull(publicKey);
    CFReleaseNull(signingKey);
    CFReleaseNull(circle);
    CFReleaseNull(error);
    CFReleaseNull(pi);
    CFReleaseNull(messageKey);
    CFReleaseNull(message_circle_name);
    CFReleaseNull(message_from_peer_id);
    CFReleaseNull(message_to_peer_id);
}

int sc_20_keynames(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();

	return 0;
}
