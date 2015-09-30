//
//  FakeXCTest
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import "FakeXCTest.h"
#import <objc/runtime.h>
#import <stdarg.h>
#import <fnmatch.h>

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
    unsigned long ranTests = 0, failedTests = 0;
    Class xctestClass = [XCTestCase class];
    int numClasses;
    int superResult = 0;


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

                        if (testCaseResult) {
                            superResult++;
                            failedTests++;
                        }
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
    
    return superResult;
}

@end

