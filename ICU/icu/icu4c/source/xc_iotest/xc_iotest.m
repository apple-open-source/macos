//
//  xc_iotest.m
//  xc_iotest
//
//  Created by Christopher Chapman on 2024-10-30.
//  Copyright © 2024 Apple. All rights reserved.
//

#import <XCTest/XCTest.h>
#import "ctest.h"
#import "unicode/uclean.h"

extern int main(int argc, char* argv[]);

int call_main(char *testname) {
    char argv0[] = "iotest";
    char* argv[4];
    int argc = ctest_setup_xctest_argv(argv0, (const char **)argv, testname);
    int result = main(argc, argv);
    return result;
}

@interface xc_iotest : XCTestCase

@end

@implementation xc_iotest

//+ (void)setUp {
//    // This method is called only once before any of the test methods begin.
//}

+ (void)tearDown {
    //This method is called only once after all of the test methods are done.

    // rdar://163964842
    // Do one last init to ensure we're leaving ICU in a good state
    // because XCTest uses the libicucore.A.dylib we built and it calls
    // udat_format() via CFDateFormatterCreateStringWithAbsoluteTime()
    // after the tests complete, which will get an EXC_BAD_ACCESS
    // if we've already cleaned up the timezone resource data.
    UErrorCode errorCode = U_ZERO_ERROR;
    u_init(&errorCode);
    if (U_FAILURE(errorCode)) {
        fprintf(stderr, "u_init() failed with status: %s.\n",
                u_errorName(errorCode));
    }
}


//- (void)setUp {
    // This method is called before each test method in the class.
//}

//- (void)tearDown {
    // This method is called after each test method in the class.
//}


- (void)test_datadriv {
    char testname[] = "/datadriv";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_file {
    char testname[] = "/file";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_stream {
    char testname[] = "/stream";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_string {
    char testname[] = "/string";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_translit {
    char testname[] = "/translit";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_ScanfMultipleIntegers {
    char testname[] = "/ScanfMultipleIntegers";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

//- (void)testPerformanceExample {
//    // This is an example of a performance test case.
//    [self measureBlock:^{
//        // Put the code you want to measure the time of here.
//    }];
//}

@end
