//
//  TestHarness.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#import "TestHarness.h"

static TestHarness *tests;
static NSMutableDictionary *failedTests;

@interface TestHarnessSenObserver : SenTestObserver
@end

@implementation TestHarnessSenObserver

+ (void) testSuiteDidStart:(NSNotification *) aNotification {
}
+ (void) testSuiteDidStop:(NSNotification *) aNotification {
}

+ (void) testCaseDidStart:(NSNotification *) aNotification {
    printf("[BEGIN] %s\n", [aNotification.test.name UTF8String]);
    [tests.delegate THPTestStart:aNotification.test.name];
}
+ (void) testCaseDidStop:(NSNotification *) aNotification {
    NSString *status = [failedTests objectForKey:aNotification.test.name];
    printf("[%s] %s\n", (status == NULL) ? "PASS" : "FAIL", [aNotification.test.name UTF8String]);
    [tests.delegate THPTestComplete:aNotification.test.name status:(status == NULL) duration:aNotification.run.testDuration];
}
+ (void) testCaseDidFail:(NSNotification *) aNotification {
    [failedTests setObject:[[aNotification exception] description] forKey:aNotification.test.name];
}

+ (void) testCaseOutput:(NSString *)output {
    printf("%s\n", [output UTF8String]);
    [tests.delegate THPTestOutput:output];
}


@end

@implementation TestHarness

+ (void)TestHarnessOutput:(NSString *)output {
    [TestHarnessSenObserver testCaseOutput:output];
}

- (bool)runTests {
    bool hasSucceeded;
    
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        failedTests = [NSMutableDictionary dictionary];
        [[NSUserDefaults standardUserDefaults] setValue:@"TestHarnessSenObserver" forKey:@"SenTestObserverClass"];
    });
    
    [failedTests removeAllObjects];
    tests = self;
    
    @autoreleasepool {
        SenTestSuite *tests = [SenTestSuite defaultTestSuite];
        hasSucceeded = [[tests run] hasSucceeded];
	[self.delegate THPSuiteComplete:hasSucceeded];
    }
    return hasSucceeded;
}

@end
