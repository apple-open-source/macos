//
//  secd-156-timers.m
//  secdRegressions
//

#import <Foundation/Foundation.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <CoreFoundation/CFDictionary.h>
#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include <utilities/SecCFWrappers.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 3;


static void testPeerRateLimiterSendNextMessageWithRetain(CFStringRef peerid, CFStringRef accessGroup)
{
    ok(CFStringCompare(peerid, CFSTR("imretainedpeerid"), 0) == kCFCompareEqualTo);
}

static void testOTRTimer()
{
    dispatch_semaphore_t sema2 = dispatch_semaphore_create(0);
    
    CFStringRef peeridRetained = CFRetainSafe(CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), CFSTR("imretainedpeerid")));
    dispatch_queue_t test_queue2 = dispatch_queue_create("com.apple.security.keychain-otrtimer2", DISPATCH_QUEUE_SERIAL);
    __block dispatch_source_t timer2 = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, test_queue2);
    
    dispatch_source_set_timer(timer2, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_source_set_event_handler(timer2, ^{
        testPeerRateLimiterSendNextMessageWithRetain(peeridRetained, CFSTR("NoAttribute"));
        dispatch_semaphore_signal(sema2);
    });
    dispatch_resume(timer2);
    dispatch_source_set_cancel_handler(timer2, ^{
        CFReleaseSafe(peeridRetained);
    });
    
    CFReleaseNull(peeridRetained);
    dispatch_semaphore_wait(sema2, DISPATCH_TIME_FOREVER);
    
}

static void testOTRTimerWithRetain(CFStringRef peerid)
{
    ok(CFStringCompare(peerid, CFSTR("imretainedpeerid"), 0) == kCFCompareEqualTo);
}

static void testRateLimitingTimer()

{
    dispatch_semaphore_t sema2 = dispatch_semaphore_create(0);

    CFStringRef peeridRetained = CFRetainSafe(CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), CFSTR("imretainedpeerid")));
    
    dispatch_queue_t test_queue2 = dispatch_queue_create("com.apple.security.keychain-ratelimit12", DISPATCH_QUEUE_SERIAL);
    __block dispatch_source_t timer2 = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, test_queue2);
    
    dispatch_source_set_timer(timer2, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_source_set_event_handler(timer2, ^{
        testOTRTimerWithRetain(peeridRetained);
        dispatch_semaphore_signal(sema2);
    });
    dispatch_resume(timer2);
    
    dispatch_source_set_cancel_handler(timer2, ^{
        CFReleaseSafe(peeridRetained);
    });
    
    CFReleaseNull(peeridRetained);
    dispatch_semaphore_wait(sema2, DISPATCH_TIME_FOREVER);
    
}
static void tests(){
    
    testOTRTimer();
    testRateLimitingTimer();
}

int secd_156_timers(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
