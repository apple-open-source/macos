/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
#include <SOSPeerInfoDER.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

typedef struct piStuff_t {
    SecKeyRef signingKey;
    SOSFullPeerInfoRef fpi;
    SOSPeerInfoRef pi;
    SOSPeerInfoRef resignation_ticket;
} piStuff;

static piStuff *makeSimplePeer(char *name) {
    piStuff *pi = malloc(sizeof(piStuff));
    
    if(!pi) return NULL;
    pi->signingKey = NULL;
    CFStringRef cfName = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingMacRoman);
    pi->fpi = SOSCreateFullPeerInfoFromName(cfName, &pi->signingKey, NULL);
    CFReleaseSafe(cfName);
    pi->pi = SOSFullPeerInfoGetPeerInfo(pi->fpi);
    pi->resignation_ticket = SOSPeerInfoCreateRetirementTicket(kCFAllocatorDefault, pi->signingKey, pi->pi, NULL);
    return pi;
}

static void freeSimplePeer(piStuff *pi)
{
    CFReleaseSafe(pi->fpi);
    CFReleaseSafe(pi->signingKey);
    CFReleaseSafe(pi->resignation_ticket);
    free(pi);
}

static inline bool retire_me(piStuff *pi, size_t seconds) {
    return SOSPeerInfoRetireRetirementTicket(seconds, pi->resignation_ticket);
}


static inline bool chkBasicTicket(piStuff *pi) {
    return CFEqual(SOSPeerInfoInspectRetirementTicket(pi->resignation_ticket, NULL), SOSPeerInfoGetPeerID(pi->pi));
}

static bool in_between_time(CFDateRef before, piStuff *pi, CFDateRef after) {
    CFDateRef during = SOSPeerInfoGetRetirementDate(pi->resignation_ticket);
    CFTimeInterval time1 = CFDateGetTimeIntervalSinceDate(before, during);
    CFTimeInterval time2 = CFDateGetTimeIntervalSinceDate(during, after);
    CFReleaseNull(during);
    if(time1 >= 0.0) return false;
    if(time2 >= 0.0) return false;
    return true;
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

static void tests(void)
{
    CFDateRef before_time = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    sleep(1);
    piStuff *iPhone = makeSimplePeer("iPhone");
    piStuff *iPad = makeSimplePeer("iPad");
    piStuff *iMac = makeSimplePeer("iMac");
    piStuff *iDrone = makeSimplePeer("iDrone");
    sleep(1);
    CFDateRef after_time = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    
    ok(in_between_time(before_time, iPhone, after_time), "retirement date recorded correctly");
    CFReleaseSafe(before_time);
    CFReleaseSafe(after_time);
    ok(chkBasicTicket(iPhone), "peer ID's Match");
    ok(chkBasicTicket(iPad), "peer ID's Match");
    ok(chkBasicTicket(iMac), "peer ID's Match");
    ok(chkBasicTicket(iDrone), "peer ID's Match");
    
    // ok(miss_signature(iDrone, iPad), "signature failure detected");
    
    ok(!retire_me(iPhone, 10000), "ticket still valid");
    sleep(2);
    ok(retire_me(iPhone, 1), "ticket not valid");
    
    CFDateRef retdate = SOSPeerInfoGetRetirementDate(iPhone->resignation_ticket);
    ok(retdate != NULL, "got retirement date %@", retdate);
    CFReleaseSafe(retdate);
    
    ok(PeerInfoRoundTrip(iPhone->resignation_ticket), "retirement ticket safely DERs");
#if 0
    CFDateRef appdate = NULL;
    ok((appdate = SOSPeerInfoGetApplicationDate(iPhone->resignation_ticket)) != NULL, "got application date %@", appdate);
    CFReleaseSafe(appdate);
#endif

    freeSimplePeer(iPhone);
    freeSimplePeer(iPad);
    freeSimplePeer(iMac);
    freeSimplePeer(iDrone);
}

static int kTestTestCount = 12;

int sc_130_resignationticket(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
	return 0;
}
