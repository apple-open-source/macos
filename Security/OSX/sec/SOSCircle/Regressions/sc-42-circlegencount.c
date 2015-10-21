//
//  sc-42-circlegencount.c
//  sec
//
//  Created by Richard Murphy on 9/10/14.
//
//




#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>

#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

static int kTestTestCount = 7;

static void tests(void)
{
    uint64_t beginvalue;
    uint64_t lastvalue;
    uint64_t newvalue;
    
    SOSCircleRef circle = SOSCircleCreate(NULL, CFSTR("TEST DOMAIN"), NULL);
    
    ok(NULL != circle, "Circle creation");
    
    ok(0 == SOSCircleCountPeers(circle), "Zero peers");
    
    ok(0 != (beginvalue = SOSCircleGetGenerationSint(circle))); // New circles should never be 0
    
    SOSCircleGenerationSetValue(circle, 0);
    
    ok(0 == SOSCircleGetGenerationSint(circle)); // Know we're starting out with a zero value (forced)
        
    SOSCircleGenerationIncrement(circle);
    
    ok(beginvalue <= (newvalue = SOSCircleGetGenerationSint(circle))); // incremented value should be greater or equal than where we began quantum is 2 seconds
    lastvalue = newvalue;
    
    SOSCircleGenerationIncrement(circle);
    ok(lastvalue < (newvalue = SOSCircleGetGenerationSint(circle))); // incremented value should be greater than last
    lastvalue = newvalue;

    SOSCircleResetToEmpty(circle, NULL);
    ok(lastvalue < (newvalue = SOSCircleGetGenerationSint(circle))); // incremented value should be greater than last
    
    CFReleaseNull(circle);
}

int sc_42_circlegencount(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
