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

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
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
   
    CFErrorRef error = NULL;

    SOSCircleRef circle = SOSCircleCreate(NULL, CFSTR("Test Circle"), &error);
    
    CFStringRef circle_key = SOSCircleKeyCreateWithCircle(circle, NULL);

    CFStringRef circle_name = NULL;
    ok(circle_key, "Circle key created");
    is(SOSKVSKeyGetKeyType(circle_key), kCircleKey, "Is circle key");
    is(SOSKVSKeyGetKeyTypeAndParse(circle_key, &circle_name, NULL, NULL, NULL, NULL, NULL), kCircleKey, "Is circle key, extract name");
    ok(circle_name, "Circle name extracted");
    ok(CFEqualSafe(circle_name, SOSCircleGetName(circle)), "Circle name matches '%@' '%@'", circle_name, SOSCircleGetName(circle));
    CFReleaseNull(circle_name);
    CFReleaseNull(circle_key);
    
    SOSPeerInfoRef pi = SOSCreatePeerInfoFromName(CFSTR("Test Peer"), &publicKey, &error);
    
    CFStringRef other_peer_id = CFSTR("OTHER PEER");
    
    CFStringRef messageKey = SOSMessageKeyCreateWithCircleAndPeerNames(circle, SOSPeerInfoGetPeerID(pi), other_peer_id);
    
    ok(messageKey, "Getting message key '%@'", messageKey);
    
    CFStringRef message_circle_name = NULL;
    CFStringRef message_from_peer_id = NULL;
    CFStringRef message_to_peer_id = NULL;
    CFStringRef message_ring = NULL;
    CFStringRef message_peer_info = NULL;
    CFStringRef message_backup = NULL;
    
    is(SOSKVSKeyGetKeyType(messageKey), kMessageKey, "Is message key");
    is(SOSKVSKeyGetKeyTypeAndParse(messageKey,
                                   &message_circle_name,
                                   &message_peer_info,
                                   &message_ring,
                                   &message_backup,
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
                                   NULL,
                                   NULL,
                                   NULL,
                                   &retirement_peer_id,
                                   NULL), kRetirementKey, "Is retirement key, extract parts");
    CFReleaseSafe(retirementKey);
    ok(CFEqualSafe(SOSCircleGetName(circle), retirement_circle_name), "circle key matches in retirement (%@ v %@)",
       SOSCircleGetName(circle), retirement_circle_name);
    ok(CFEqualSafe(SOSPeerInfoGetPeerID(pi), retirement_peer_id), "retirement peer set correctly (%@ v %@)",
       SOSPeerInfoGetPeerID(pi), retirement_peer_id);
    
    CFReleaseNull(publicKey);
    CFReleaseNull(circle);
    CFReleaseNull(error);
    CFReleaseNull(pi);
    CFReleaseNull(messageKey);
    CFReleaseNull(message_circle_name);
    CFReleaseNull(message_from_peer_id);
    CFReleaseNull(message_to_peer_id);
    CFReleaseNull(retirement_circle_name);
    CFReleaseNull(retirement_peer_id);

}

int sc_20_keynames(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
