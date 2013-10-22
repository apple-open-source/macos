//
//  sc-103-syncupdate.c
//  sec
//
//  Created by John Hurley on 9/6/12.
//
//

// Run on 2 devices:
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_103_syncupdate -v -- -i alice
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_103_syncupdate -v -- -i bob
// Run on 2 devices:
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_101_accountsync -v -- -i 
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_101_accountsync -v -- -i 

#include <AssertMacros.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xpc/xpc.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

#include <CKBridge/SOSCloudKeychainClient.h>
#include <SecureObjectSync/SOSCloudCircle.h>

#include <notify.h>
#include <dispatch/dispatch.h>
#include <getopt.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <utilities/SecCFRelease.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"
#include "SOSCloudKeychainConstants.h"

#define xsecdebug(format...) secerror(format)

static dispatch_group_t sDispatchGroup = NULL;

static const uint64_t putTestInterval = 10ull * NSEC_PER_SEC;
static const uint64_t leeway = 1ull * NSEC_PER_SEC;
static const uint64_t syncInterval = 60ull * NSEC_PER_SEC;             // seconds
static const uint64_t putDelay = 10ull * NSEC_PER_SEC;   // seconds; should probably be longer than syncedefaultsd latency (6 sec)
static const uint64_t exitDelay = 180ull * NSEC_PER_SEC;   // seconds

static dispatch_queue_t requestqueue;

static dispatch_source_t exitTimerSource;

static uint64_t failCounter = 0;
static uint64_t putAttemptCounter = 0;
static uint64_t itemsChangedCount = 0;


// MARK: ----- Debug Routines -----

static void tearDown(void)
{
    xsecdebug("exit");
    CFRunLoopStop(CFRunLoopGetMain());
}

// MARK: ----- Get Cycle Tester -----

static void initCyclerTests(dispatch_group_t dgroup)
{
    /*
        We set up two timer sources:
        - getRequestsource waits 5 seconds, then gets every 5 seconds
        - exitTimerSource fires once after 300 seconds (5 minutes)
        All are created suspended, so they don't start yet
    */
    
    uint64_t delay = exitDelay;
    dispatch_time_t exitFireTime = dispatch_time(DISPATCH_TIME_NOW, delay);
    exitTimerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, (uintptr_t)NULL, 0, requestqueue);
    dispatch_source_set_timer(exitTimerSource, exitFireTime, 0ull, leeway);
        
    dispatch_source_set_event_handler(exitTimerSource,
    ^{
        xsecdebug("Test Exit: %lld", failCounter);
        printf("Test Exit: fail: %lld, total: %lld, changed: %lld\n", failCounter, putAttemptCounter, itemsChangedCount);
        tearDown();
        exit(0);
    });
}

static void updateSyncingEnabledSwitch()
{
    // Set the visual state of switch based on membership in circle
    CFErrorRef error = NULL;
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
 //   [_syncingEnabled setOn:(BOOL)(ccstatus == kSOSCCInCircle) animated:NO];
    pass("ccstatus: %d, error: %@", ccstatus, error);
}

static void requestToJoinCircle()
{
    // Set the visual state of switch based on membership in circle
    bool bx;
    CFErrorRef error = NULL;
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    pass("ccstatus: %d, error: %@", ccstatus, error);
    if (ccstatus == kSOSCCInCircle)
        return;

    if (ccstatus == kSOSCCNotInCircle)
    {
        pass("Not in circle, requesting admission");
        bx = SOSCCRequestToJoinCircle(&error);
        if (!bx)
            pass("SOSCCRequestToJoinCircle error: %@", error);
    }
    else
    if (ccstatus == kSOSCCRequestPending)
        pass("Not in circle, admission pending");
}

static void handleEnableSyncing(bool turningOn)
{
    dispatch_queue_t workq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    if (turningOn)  // i.e. we are trying to turn on syncing
    {
        pass("Keychain syncing is being turned ON");
        dispatch_async(workq, ^
        {
            CFErrorRef error = NULL;
            bool bx = SOSCCResetToOffering(&error);
            if (bx)
                pass("ResetToOffering OK");
            else
                pass("ResetToOffering fail: %@", error);

            requestToJoinCircle();
            updateSyncingEnabledSwitch();
            dispatch_group_leave(sDispatchGroup);
        });
    }
    else
    {
        pass("Keychain syncing is being turned OFF");
        CFErrorRef error = NULL;
        bool bx = SOSCCRemoveThisDeviceFromCircle(&error);
        updateSyncingEnabledSwitch();
        if (!bx)
            pass("SOSCCRemoveThisDeviceFromCircle fail: %@", error);
    }
}

// MARK: ----- start of all tests -----

static void initialEstablish(void)
{
    dispatch_group_enter(sDispatchGroup);
    dispatch_group_notify(sDispatchGroup, requestqueue, ^
        {
            printf("Exiting via dispatch_group_notify; all work done\n");
            CFRunLoopStop(CFRunLoopGetMain());
        //  exit(0);
        });
handleEnableSyncing(true);

#if 0
    CFErrorRef error = NULL;
    bool bx = false;
    SOSCCStatus sccStatus = SOSCCThisDeviceIsInCircle(&error);
    if (sccStatus == kSOSCCNotInCircle)
        bx = SOSCCResetToOffering(&error);
    else
        bx = SOSCCRequestToJoinCircle(&error);
    if (!bx)
        xsecdebug("circle establish error: %@", error);
    ok(bx, "Circle established");
#endif
}

// MARK: ---------- Main ----------

// define the options table for the command line
static const struct option options[] =
{
	{ "verbose",		optional_argument,	NULL, 'v' },
	{ }
};

static int kTestCount = 22;

int sc_103_syncupdate(int argc, char *const *argv)
{
    extern char *optarg;
    int arg, argSlot;
    
    while (argSlot = -1, (arg = getopt_long(argc, (char * const *)argv, "i:v", options, &argSlot)) != -1)
        switch (arg)
        {
        default:
            secerror("arg: %s", optarg);
            break;
        }
    
    plan_tests(kTestCount);

    SKIP:
    {
        skip("Skipping ckdclient tests because CloudKeychainProxy.xpc is not installed", kTestCount, XPCServiceInstalled());
        sDispatchGroup = dispatch_group_create();
        requestqueue = dispatch_queue_create("sc_103_syncupdate", DISPATCH_QUEUE_CONCURRENT);
        initCyclerTests(sDispatchGroup);
        initialEstablish();
    }
    
    dispatch_group_wait(sDispatchGroup, DISPATCH_TIME_FOREVER);
    pass("Tests are running...");
    printf("Tests are running...\n");
    CFRunLoopRun();                 // Wait for it...
    pass("Exit from run loop");

	return 0;
}



