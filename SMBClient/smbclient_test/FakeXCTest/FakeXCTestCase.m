//
//  FakeXCTest
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import <mach/mach.h>
#import <mach/mach_time.h>
#import "FakeXCTestCase.h"

static uint64_t
relative_nano_time(void)
{
    static uint64_t factor;
    uint64_t now;

    now = mach_absolute_time();

    if (factor == 0) {
        mach_timebase_info_data_t base;
        (void)mach_timebase_info(&base);
        factor = base.numer / base.denom;
    }

    return now * factor;
}

#define MAXTESTRUN 5

@interface XCTestCase () {
    uint64_t _start;
    uint64_t _testRuns[MAXTESTRUN];
    unsigned _testrun;

}
@end

@implementation XCTestCase

+ (NSArray *)defaultPerformanceMetrics {
    return NULL;
}

- (void)setUp {
}
- (void)tearDown {
}

- (void)measureBlock:(void (^)(void))block
{
    [self measureMetrics:[[self class] defaultPerformanceMetrics] automaticallyStartMeasuring:YES forBlock:block];
}


- (void)measureMetrics:(NSArray *)metrics automaticallyStartMeasuring:(BOOL)automaticallyStartMeasuring forBlock:(void (^)(void))block;
{
    unsigned long n;
    uint64_t sum = 0, mean;
    double stddiv = 0.0;

    (void)metrics;

    for (_testrun = 0; _testrun < MAXTESTRUN; _testrun++) {
        _start = 0;
        if (automaticallyStartMeasuring)
            [self startMeasuring];
        block();
        if (automaticallyStartMeasuring)
            [self stopMeasuring];
    }
    printf("TIME %d runs [", MAXTESTRUN);
    for (n = 0; n < MAXTESTRUN; n++) {
        printf("%0.2lf%s", (double)_testRuns[n] / NSEC_PER_SEC,
               n == MAXTESTRUN - 1 ? "" : " ");
        sum += _testRuns[n];
    }
    mean = sum / MAXTESTRUN;

    for (n = 0; n < MAXTESTRUN; n++) {
        int64_t t = (int64_t)(mean - _testRuns[n]);
        stddiv += t * t;
    }
    stddiv = sqrt(stddiv / MAXTESTRUN);
    printf("] seconds average: %0.2lf relative standard deviation: %0.2lf%%\n",
           (double)mean / NSEC_PER_SEC, 100 * stddiv / mean);
}

- (void)startMeasuring
{
    _start = relative_nano_time();
}

- (void)stopMeasuring
{
    uint64_t stop = relative_nano_time();

    if (_start == 0)
        @throw [NSException exceptionWithName:@"startMeasuringNeverCalled"
                                       reason:@"test case failure"
                                     userInfo:nil];
    if (_testrun >= MAXTESTRUN) abort();
    _testRuns[_testrun] = stop - _start;
    _start = 0;
}


@end
