//
//  KeychainEntitledTestRunner.m
//  KeychainEntitledTestRunner
//
//  Stolen from Mark Pauley / CDEntitledTestRunner who stole it from Drew Terry / MobileContainerManager
//  Copyright 2016-2017 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <unistd.h>
#import <XCTest/XCTest.h>

@interface TestRunner : NSObject <XCTestObservation> {
    NSBundle *_bundle;
    XCTestSuite *_testSuite;
}

- (instancetype)initWithBundlePath:(NSString *)path andTestNames:(NSArray *)names;
- (NSUInteger)runUnitTestSuite;

- (void)testLogWithFormat:(NSString *)format, ... NS_FORMAT_FUNCTION(1,2);
- (void)testLogWithFormat:(NSString *)format arguments:(va_list)arguments NS_FORMAT_FUNCTION(1,0);

@end

@implementation TestRunner
- (instancetype)initWithBundlePath:(NSString *)path andTestNames:(NSArray *)names
{
    if ((self = [super init])) {
        NSError *error = nil;
        
        _bundle = [NSBundle bundleWithPath:path];
        if (!_bundle) {
            [self testLogWithFormat:@"No bundle at location %@ (%s)\n", path, strerror(errno)];
            return nil;
        }
        if (![_bundle loadAndReturnError:&error]) {
            [self testLogWithFormat:@"Test Bundle at %@ didn't load: %@\n", path, error];
            return nil;
        }

        if(names) {
            XCTestSuite* testSuite = [[XCTestSuite alloc] initWithName:[[path lastPathComponent] stringByDeletingPathExtension]];
            XCTestSuite* loadedSuite = [XCTestSuite testSuiteForBundlePath:path];
            // Filter out only the tests that were named.
            [loadedSuite.tests enumerateObjectsUsingBlock:^(__kindof XCTest * _Nonnull test, NSUInteger __unused idx, BOOL * __unused _Nonnull stop) {
                [self testLogWithFormat:@"Checking test %@\n", test.name];
                if([names containsObject:test.name]) {
                    [testSuite addTest:test];
                }
            }];
            _testSuite = testSuite;
        }
        else {
            _testSuite = [XCTestSuite testSuiteForBundlePath:path];
        }
    }
    return self;
}

- (NSUInteger)runUnitTestSuite
{
    [[XCTestObservationCenter sharedTestObservationCenter] addTestObserver:self];
    
    [_testSuite runTest];
    
    XCTestRun *testRun = [_testSuite testRun];
    
    return testRun.totalFailureCount;
}

- (NSFileHandle *)logFileHandle
{
    return [NSFileHandle fileHandleWithStandardOutput];
}

- (void)testLogWithFormat:(NSString *)format, ...
{
    va_list ap;
    va_start(ap, format);
    [self testLogWithFormat:format arguments:ap];
    va_end(ap);
}

- (void)testLogWithFormat:(NSString *)format arguments:(va_list)arguments
{
    NSString *message = [[NSString alloc] initWithFormat:format arguments:arguments];
    [self.logFileHandle writeData:[message dataUsingEncoding:NSUTF8StringEncoding]];
}

- (NSDateFormatter *)dateFormatter
{
    static NSDateFormatter *dateFormatter;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        dateFormatter = [NSDateFormatter new];
        dateFormatter.dateFormat = @"yyyy-MM-dd HH:mm:ss.SSS";
    });
    return dateFormatter;
}

/* -testBundleWillStart:                            // exactly once per test bundle
 *      -testSuiteWillStart:                        // exactly once per test suite
 *          -testCaseWillStart:                     // exactly once per test case
 *          -testCase:didFailWithDescription:...    // zero or more times per test case, any time between test case start and finish
 *          -testCaseDidFinish:                     // exactly once per test case
 *      -testSuite:didFailWithDescription:...       // zero or more times per test suite, any time between test suite start and finish
 *      -testSuiteDidFinish:                        // exactly once per test suite
 * -testBundleDidFinish:                            // exactly once per test bundle
 */

- (void)testSuiteWillStart:(XCTestSuite *)testSuite
{
    [self testLogWithFormat:@"Test Suite '%@' started at %@\n", testSuite.name, [self.dateFormatter stringFromDate:testSuite.testRun.startDate]];
}

- (void)testSuite:(XCTestSuite *)testSuite didRecordIssue:(XCTIssue *)issue {
	[self testLogWithFormat:@"(%@)%@:%lu: error: %@", testSuite.name, issue.sourceCodeContext.location.fileURL,
													   issue.sourceCodeContext.location.lineNumber, issue.compactDescription];
}

- (void)testSuiteDidFinish:(XCTestSuite *)testSuite
{
    XCTestRun *testRun = testSuite.testRun;
    [self testLogWithFormat:@"Test Suite '%@' %s at %@.\n\t Executed %lu test%s, with %lu failure%s (%lu unexpected) in %.3f (%.3f) seconds\n",
     testSuite.name,
     (testRun.hasSucceeded ? "passed" : "failed"),
     [self.dateFormatter stringFromDate:testRun.stopDate],
     ((unsigned long)testRun.executionCount), (testRun.executionCount != 1 ? "s" : ""),
     ((unsigned long)testRun.totalFailureCount), (testRun.totalFailureCount != 1 ? "s" : ""),
     ((unsigned long)testRun.unexpectedExceptionCount),
     testRun.testDuration,
     testRun.totalDuration];
}

- (void)testCaseWillStart:(XCTestCase *)testCase
{
    [self testLogWithFormat:@"Test Case '%@' started.\n", testCase.name];
}

- (void)testCase:(XCTestCase *)testCase didRecordIssue:(XCTIssue *)issue {
	[self testLogWithFormat:@"(%@)%@:%lu error: %@\n%@", testCase.name, issue.sourceCodeContext.location.fileURL, issue.sourceCodeContext.location.lineNumber, issue.detailedDescription, issue.sourceCodeContext.callStack];
}

- (void)testCaseDidFinish:(XCTestCase *)testCase
{
    [self testLogWithFormat:@"Test Case '%@' %s (%.3f seconds).\n", testCase.name, (testCase.testRun.hasSucceeded ? "passed" : "failed"), testCase.testRun.totalDuration];
    
}
@end




static char* gTestBundleDir = "/AppleInternal/XCTests/com.apple.security";
static char* gTestBundleName = "CKKSCloudKitTests";

static NSMutableArray* gTestCaseNames = nil;

static const char* opt_str = "d:t:c:h";

static void usage(char*const binName, bool longUsage) {
    fprintf(stderr, "Usage: %s [-d <test_dir>] -t test_bundle_name [(-c test_case_name)*]\n", binName);
    if (longUsage) {
        fprintf(stderr, "-d: argument = path to directory where test bundles live\n");
        fprintf(stderr, "-t: argument = name of test bundle to be run (without extension)\n");
        fprintf(stderr, "-c: argument = name of test case to be run (multiple)\n");
    }
}

static void getOptions(int argc, char *const *argv) {
    int ch;
    while ( (ch = getopt(argc, argv, opt_str)) != -1 ) {
        switch(ch)
        {
            case 'd':
                gTestBundleDir = optarg;
                break;
            case 't':
                gTestBundleName = optarg;
                break;
            case 'c':
                if(!gTestCaseNames) {
                    gTestCaseNames = [NSMutableArray new];
                }
                [gTestCaseNames addObject:@(optarg)];
                break;
            case 'h':
            case '?':
            default:
                usage(argv[0], true);
                exit(0);
                break;
        }
    }
}

int main (int argc, const char * argv[])
{
    @autoreleasepool {
        getOptions(argc, (char*const*)argv);
        NSString *testBundleDir = [NSString stringWithCString:gTestBundleDir encoding:NSUTF8StringEncoding];
        NSString *testBundleName = [NSString stringWithCString:gTestBundleName encoding:NSUTF8StringEncoding];
        NSString *testBundlePath = [[testBundleDir stringByAppendingPathComponent:testBundleName] stringByAppendingPathExtension:@"xctest"];
        
        printf("Running unit tests %s at: %s\n", gTestCaseNames?gTestCaseNames.description.UTF8String:"[All]", testBundlePath.UTF8String);
        
        TestRunner *unitTest = [[TestRunner alloc] initWithBundlePath:testBundlePath andTestNames:gTestCaseNames];
        if (!unitTest) {
            fprintf(stderr, "Failed to load unit test runner at: %s\n", testBundlePath.UTF8String);
            return 1;
        }
        
        //runUnitTestSuite returns the number of failures. 0 = success, non-zero means failure. This complies with BATS testing standards.
        return (int)[unitTest runUnitTestSuite];
        
        return 0;
    }
}
