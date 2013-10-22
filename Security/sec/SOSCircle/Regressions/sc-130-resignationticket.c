//
//  sc-130-resignationticket.c
//  sec
//
//  Created by Richard Murphy on 5/1/13.
//
//

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>

#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSInternal.h>

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

static inline bool retire_me(piStuff *pi, size_t seconds) {
    return SOSPeerInfoRetireRetirementTicket(seconds, pi->resignation_ticket);
}

// Copied from SOSPeerInfo.c
static CFStringRef sFlatticket = CFSTR("flatticket");
static CFStringRef sSignature = CFSTR("RetirementPsig");
static CFStringRef sPeerid = CFSTR("peerid");


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
    
    CFDateRef retdate = NULL;
    ok((retdate = SOSPeerInfoGetRetirementDate(iPhone->resignation_ticket)) != NULL, "got retirement date %@", retdate);
    CFReleaseSafe(retdate);
    
#if 0
    CFDateRef appdate = NULL;
    ok((appdate = SOSPeerInfoGetApplicationDate(iPhone->resignation_ticket)) != NULL, "got application date %@", appdate);
    CFReleaseSafe(appdate);
#endif
}

static int kTestTestCount = 20;

int sc_130_resignationticket(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
	return 0;
}
