/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "TestResourceUsage.h"
#import <libproc.h>
#import <mach/mach_time.h>
#import <mach/message.h>
#import <os/assumes.h>
#import <stdio.h>

@interface TestResourceUsage () <XCTestObservation>
@property (assign) rusage_info_current startResource;
@property (assign) uint64_t startMachTime;
@property (assign) uint64_t stopMachTime;
@end


@implementation TestResourceUsage

+ (TestResourceUsage *)sharedManager
{
    static TestResourceUsage *manager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        manager = [TestResourceUsage new];
    });
    return manager;
}

+ (void)monitorTestResourceUsage
{
    static dispatch_once_t onceToken;
    TestResourceUsage *manager = [TestResourceUsage sharedManager];
    dispatch_once(&onceToken, ^{
        [[XCTestObservationCenter sharedTestObservationCenter] addTestObserver:manager];
    });
}

- (void)testCaseWillStart:(XCTestCase *)testCase
{
    proc_pid_rusage(getpid(), RUSAGE_INFO_CURRENT, (rusage_info_t *)&_startResource);
    _startMachTime = mach_absolute_time();
}

- (void)testCaseDidFinish:(XCTestCase *)testCase
{
    _stopMachTime = mach_absolute_time();

    rusage_info_current endResource;
    proc_pid_rusage(getpid(), RUSAGE_INFO_CURRENT, (rusage_info_t *)&endResource);

    uint64_t startUserTime = [self nsecondsFromMachTime:self.startResource.ri_user_time];
    uint64_t endUserTime = [self nsecondsFromMachTime:endResource.ri_user_time];
    uint64_t startSysrTime = [self nsecondsFromMachTime:self.startResource.ri_system_time];
    uint64_t endSysTime = [self nsecondsFromMachTime:endResource.ri_system_time];

    uint64_t diskUsage = endResource.ri_logical_writes - self.startResource.ri_logical_writes;

    [self BATSUnitTestToken:@"DiskUsage" value:@(diskUsage) testcase:testCase];
    [self BATSUnitTestToken:@"CPUUserTime" value:@(endUserTime - startUserTime) testcase:testCase];
    [self BATSUnitTestToken:@"CPUSysTime" value:@(endSysTime - startSysrTime) testcase:testCase];
    [self BATSUnitTestToken:@"WallTime" value:@(_stopMachTime - _startMachTime) testcase:testCase];
}

- (void)BATSUnitTestToken:(NSString *)name value:(id)value testcase:(XCTestCase *)testCase
{
    printf("%s", [[NSString stringWithFormat:@"[RESULT_KEY] TestResourceUsage:%s:%@\n[RESULT_VALUE] %@\n",
                   object_getClassName([testCase class]), name, value] UTF8String]);
}

- (uint64_t)nsecondsFromMachTime:(uint64_t)machTime
{
    static dispatch_once_t once;
    static uint64_t ratio;
    dispatch_once(&once, ^{
        mach_timebase_info_data_t tbi;
        if (os_assumes_zero(mach_timebase_info(&tbi)) == KERN_SUCCESS) {
            ratio = tbi.numer / tbi.denom;
        } else {
            ratio = 1;
        }
    });

    return (machTime * ratio);
}



@end
