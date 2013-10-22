//
//  sc-41-cloudcircle.c
//  sec
//
//  Created by Mitch Adler on 12/13/12.
//
//

#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

#include <SecureObjectSync/SOSCloudCircle.h>
#include <utilities/SecCFWrappers.h>

static const int kSOSCCTestCount = 6;   // # of "ok"s in "tests" below
static int kTestTestCount = kSOSCCTestCount;
static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);

    ok(SOSCCSetUserCredentials(CFSTR("foo1"), cfpassword, &error), "Added Creds (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    ok(SOSCCThisDeviceIsInCircle(&error) == kSOSCCCircleAbsent, "Circle Absent (%@)", error);
    CFReleaseNull(error);
    ok(SOSCCResetToOffering(&error), "SOSCCOfferPotentialCircle (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSCCThisDeviceIsInCircle(&error) == kSOSCCInCircle, "Circle Absent (%@)", error);
    CFReleaseNull(error);
    ok(SOSCCRemoveThisDeviceFromCircle(&error), "Leaving (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSCCThisDeviceIsInCircle(&error) == kSOSCCCircleAbsent, "Circle Absent (%@)", error);
    CFReleaseNull(error);
}

int sc_41_cloudcircle(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();
    
	return 0;
}
