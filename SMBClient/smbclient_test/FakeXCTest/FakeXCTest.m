//
//  FakeXCTest
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import "FakeXCTest.h"
#import <objc/runtime.h>
#import <stdarg.h>
#import <fnmatch.h>
#import "json_support.h"

extern char g_test_url1[1024];
extern char g_test_url2[1024];
extern char g_test_url3[1024];
extern CFMutableDictionaryRef json_dict;

static int testCaseResult = 0;
static char *testLimitedGlobingRule = NULL;

int (*XFakeXCTestCallback)(const char *fmt, va_list) = vprintf;

static void
FXCTPrintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    XFakeXCTestCallback(fmt, ap);
    va_end(ap);
}

void
FakeXCFailureHandler(XCTestCase * __unused test, BOOL __unused expected, const char * filePath,
                     NSUInteger lineNumber, NSString * __unused condition, NSString * format, ...)
{
    va_list ap;
    va_start(ap, format);
    NSString *errorString = [[NSString alloc] initWithFormat:format arguments:ap];
    va_end(ap);

    FXCTPrintf("FAILED assertion at: %s:%d %s\n", filePath, (int)lineNumber, [errorString UTF8String]);

#if !__has_feature(objc_arc)
    [errorString release];
#endif

    testCaseResult = 1;
}

static bool
checkLimited(const char *testname)
{
    return testLimitedGlobingRule == NULL ||
    fnmatch(testLimitedGlobingRule, testname, 0) == 0;
}

static bool
returnNull(Method method)
{
    bool res = false;
    char *ret = method_copyReturnType(method);
    if (ret && strcmp(ret, "v") == 0)
        res = true;
    free(ret);

    return res;
}

@implementation XCTest

+ (void)setLimit:(const char *)testLimit
{
    if (testLimitedGlobingRule)
        free(testLimitedGlobingRule);
    testLimitedGlobingRule = strdup(testLimit);
}

+ (int) runTests
{
    NSMutableArray *testsClasses = [NSMutableArray array];
    unsigned long ranTests = 0, failedTests = 0, passTests = 0;
    Class xctestClass = [XCTestCase class];
    int numClasses;
    int superResult = 0;

    /* Save some JSON data */
    if (testLimitedGlobingRule == NULL) {
        json_add_inputs_str(json_dict, "limit", "none");
    }
    else {
        json_add_inputs_str(json_dict, "limit", testLimitedGlobingRule);
    }
    json_add_inputs_str(json_dict, "URL1", g_test_url1);
    json_add_inputs_str(json_dict, "URL2", g_test_url2);
    json_add_inputs_str(json_dict, "URL3", g_test_url3);
    json_add_time_stamp(json_dict, "start_time");

    FXCTPrintf("[TEST] %s\n", getprogname());

    numClasses = objc_getClassList(NULL, 0);

    if (numClasses > 0 ) {
        Class *classes = NULL;

        classes = (Class *)malloc(sizeof(Class) * numClasses);
        numClasses = objc_getClassList(classes, numClasses);
        for (int i = 0; i < numClasses; i++) {

            Class s = classes[i];
            while ((s = class_getSuperclass(s)) != NULL) {
                if (s == xctestClass) {
                    [testsClasses addObject:classes[i]];
                    break;
                }
            }
        }
        free(classes);
    }

    for (Class testClass in testsClasses) {
        unsigned int numMethods, n;

        if (!class_getClassMethod(testClass, @selector(respondsToSelector:)))
            continue;

        /* get all methods conforming to void test...*/
        XCTestCase *tc = NULL;;

        @autoreleasepool {
            Method *methods = NULL;

            tc = [[testClass alloc] init];
            if (tc == NULL)
                continue;

            methods = class_copyMethodList(testClass, &numMethods);
            if (methods == NULL) {
#if !__has_feature(objc_arc)
                [tc release];
#endif
                continue;
            }

            for (n = 0; n < numMethods; n++) {

                SEL ms = method_getName(methods[n]);
                const char *mname = sel_getName(ms);
                if (strncmp("test", mname, 4) == 0
                    && method_getNumberOfArguments(methods[n]) == 2
                    && returnNull(methods[n])
                    && checkLimited(mname))
                {
                    /* Save some JSON individual test data */
                    CFMutableDictionaryRef test_results = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                                 &kCFTypeDictionaryKeyCallBacks,
                                                                                 &kCFTypeDictionaryValueCallBacks);
                    json_add_time_stamp(test_results, "start_time");

                    testCaseResult = 0;

                    if ([tc respondsToSelector:@selector(setUp)])
                        [tc setUp];

                    ranTests++;

                    FXCTPrintf("[BEGIN] %s:%s\n", class_getName(testClass), mname);
                    @try {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
                        [tc performSelector:ms];
#pragma clang diagnostic pop
                    }
                    @catch (NSException *exception) {
                        FXCTPrintf("exception: %s: %s",
                                   [[exception name] UTF8String],
                                   [[exception name] UTF8String]);

                        testCaseResult = 1;
                    }
                    @finally {
                        FXCTPrintf("[%s] %s:%s\n",
                                   testCaseResult == 0 ? "PASS" : "FAIL",
                                   class_getName(testClass), mname);

                         json_add_time_stamp(test_results, "end_time");

                        if (testCaseResult) {
                            superResult++;
                            failedTests++;

                            /* Save in JSON test results pass/fail */
                            json_add_results_str(test_results, "result", "Fail");
                        }
                        else {
                            /* Save in JSON test results pass/fail */
                            json_add_results_str(test_results, "result", "Pass");
                        }

                        /* Save this test results into JSON data */
                        json_add_outputs_dict(json_dict, mname, test_results);

                        if ([tc respondsToSelector:@selector(tearDown)])
                            [tc tearDown];
                    }
                }
            }
            free(methods);
#if !__has_feature(objc_arc)
            [tc release];
#endif
        }
    }
    FXCTPrintf("[SUMMARY]\n"
               "ran %ld tests %ld failed\n",
               ranTests, failedTests);
    
    /* Save some JSON test suite data */
    json_add_time_stamp(json_dict, "end_time");
    passTests = ranTests - failedTests;
    json_add_results(json_dict, "total_tests", &ranTests, sizeof(ranTests));
    json_add_results(json_dict, "passed_tests", &passTests, sizeof(passTests));
    json_add_results(json_dict, "failed_tests", &failedTests, sizeof(failedTests));
    if (superResult) {
         /* Save in JSON overall pass/fail */
         json_add_results_str(json_dict, "result", "Fail");
     }
     else {
         /* Save in JSON overall pass/fail */
         json_add_results_str(json_dict, "result", "Pass");
     }

    return superResult;
}

@end

