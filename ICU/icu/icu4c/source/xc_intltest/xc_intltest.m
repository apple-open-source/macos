//
//  xc_intltest.m
//  xc_intltest
//
//  Created by Christopher Chapman on 2024-10-14.
//  Copyright © 2024 Apple. All rights reserved.
//

#import <XCTest/XCTest.h>
#import "ctest.h"
#import "unicode/uclean.h"

extern int main(int argc, char* argv[]);

int call_main(char *testname) {
    char argv0[] = "intltest";
    char* argv[4];
    int argc = ctest_setup_xctest_argv(argv0, (const char **)argv, testname);
    int result = main(argc, argv);
    return result;
}
    
@interface xc_intltest : XCTestCase

@end

@implementation xc_intltest

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

- (void)test_utility {
    char testname[] = "utility";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_normalize {
    char testname[] = "normalize";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_collate {
    char testname[] = "collate";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_regex {
    char testname[] = "regex";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_format {
    char testname[] = "format";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_translit {
    char testname[] = "translit";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_rbbi {
    char testname[] = "rbbi";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_rbnf {
    char testname[] = "rbnf";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_rbnfrt {
    char testname[] = "rbnfrt";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_icuserv {
    char testname[] = "icuserv";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_idna {
    char testname[] = "idna";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_convert {
    char testname[] = "convert";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_rbnfp {
    char testname[] = "rbnfp";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_csdet {
    char testname[] = "csdet";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_spoof {
    char testname[] = "spoof";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_bidi {
    char testname[] = "bidi";
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
