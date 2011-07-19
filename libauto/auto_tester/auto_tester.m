/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
//
//  auto_tester.m
//  Copyright (c) 2008-2011 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <objc/objc-auto.h>
#import "TestCase.h"
#import "auto_tester.h"

static BOOL logTestOutput = NO;

void testLoop(NSMutableArray *testCases);

void cleanup(NSMutableArray *testCases)
{
    void (^completionCallback)(void) = ^{ testLoop(testCases); };
    dispatch_queue_t current = dispatch_get_current_queue();
    auto_zone_collect_and_notify(objc_collectableZone(), AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION|AUTO_ZONE_COLLECT_LOCAL_COLLECTION, current, completionCallback);
}

void testLoop(NSMutableArray *testCases)
{
    __block BOOL allTestsComplete = YES;
    __block int successCount = 0;
    __block int failCount = 0;
    __block int skipCount = 0;
    dispatch_queue_t current = dispatch_get_current_queue();    
    void (^completionCallback)(void) = Block_copy(^{ cleanup(testCases); });

    [testCases enumerateObjectsUsingBlock:(void (^)(id obj, NSUInteger idx, BOOL *stop))^(id obj, NSUInteger idx, BOOL *stop){
        TestCase *t = (TestCase *)obj;
        switch ([t result]) {
            case PENDING:
            {
                NSString *skipMessage = [t shouldSkip];
                if (skipMessage) {
                    skipCount++;
                    [t setTestResult:SKIPPED message:skipMessage];
                } else {
                    *stop = YES;
                    allTestsComplete = NO;
                    [t setCompletionCallback:completionCallback];
                    [t startTest];
                }
            }
                break;
            case PASSED:
                successCount++;
                break;
            case FAILED:
                failCount++;
                break;
            case SKIPPED:
                skipCount++;
                break;
            default:
                NSLog(@"unexpected test status in testLoop(): %@ = %d\n", [t className], [t result]);
                exit(-1);
                break;
        }                
    }];
    Block_release(completionCallback);
    
    // report results
    if (allTestsComplete) {
        [testCases enumerateObjectsUsingBlock:(void (^)(id obj, NSUInteger idx, BOOL *stop))^(id obj, NSUInteger idx, BOOL *stop){
            TestCase *t = (TestCase *)obj;
            NSLog(@"%@: %@", [t className], [t resultString]);
            NSString *output = [t testOutput];
            if (logTestOutput && output)
                NSLog(@"Test output:\n%@", output);
        }];
        NSLog(@"%d test cases: %d passed, %d failed, %d skipped", [testCases count], successCount, failCount, skipCount);
        exit(failCount);
    }
}

int main(int argc, char *argv[])
{
    int repeat = 1;
    
    NSMutableArray *testCases = [NSMutableArray array];
    NSMutableArray *testClasses = [NSMutableArray array];
    NSProcessInfo *pi = [NSProcessInfo processInfo];
    NSMutableArray *args = [[pi arguments] mutableCopy];
    [args removeObjectAtIndex:0];
    while ([args count] > 0) {
        int argCount = 0;
        NSString *arg = [args objectAtIndex:0];
        if ([arg isEqual:@"-logTestOutput"]) {
            argCount++;
            logTestOutput = YES;
        }
        if ([arg hasPrefix:@"-repeat="]) {
            argCount++;
            repeat = [[arg substringFromIndex:[@"-repeat=" length]] intValue];
            if (repeat < 1)
                repeat = 1;
        }
        Class c = NSClassFromString(arg);
        if (c) {
            [testClasses addObject:c];
            argCount++;
        }
        [args removeObjectsInRange:NSMakeRange(0, argCount)];
    }
    
    // Simple test driver that just runs all the tests.
    if ([testClasses count] == 0)
        [testClasses addObjectsFromArray:[TestCase testClasses]];
    [testClasses enumerateObjectsUsingBlock:(void (^)(id obj, NSUInteger idx, BOOL *stop))^(id obj, NSUInteger idx, BOOL *stop){
        for (int i=0; i<repeat; i++)
            [testCases addObject:[[obj alloc] init]];
    }];
    
    dispatch_queue_t testQ = dispatch_queue_create("Test Runner", NULL);
    dispatch_async(testQ, ^{testLoop(testCases);});

    [[NSRunLoop mainRunLoop] run];
    dispatch_main();
    return 0;
}
