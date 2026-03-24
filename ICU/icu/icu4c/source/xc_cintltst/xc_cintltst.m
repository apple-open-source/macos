//
//  xc_cintltst.m
//  xc_cintltst
//
//  Created by Christopher Chapman on 2024-09-25.
//  Copyright © 2024 Apple. All rights reserved.
//

#import <XCTest/XCTest.h>
#import "ctest.h"
#import "unicode/uclean.h"

extern int main(int argc, const char* const argv[]);

int call_main(const char *testname) {
    resetRepeat();
    char argv0[] = "cintltst";
    const char* argv[4];
    int argc = ctest_setup_xctest_argv(argv0, argv, testname);
    int result = main(argc, argv);
    return result;
}


@interface xc_cintltst : XCTestCase

@end

@implementation xc_cintltst

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

// TODO: implement dynamic tests: https://docs.apple.com/access/general/documentation/xctestinternal/060-dynamic-tests


- (void)test_complex {
    char testname[] = "/complex";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_custrtrn {
    char testname[] = "/custrtrn";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_hpmufn {
    char testname[] = "/hpmufn";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_idna {
    char testname[] = "/idna";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_putiltst {
    char testname[] = "/putiltst";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_regex {
    char testname[] = "/regex";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_spreptst {
    char testname[] = "/spreptst";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tscoll {
    char testname[] = "/tscoll";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tsconv {
    char testname[] = "/tsconv";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tsformat {
    char testname[] = "/tsformat";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tsnorm {
    char testname[] = "/tsnorm";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tstformat {
    char testname[] = "/tstformat";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tstxtbd {
    char testname[] = "/tstxtbd";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_tsutil {
    char testname[] = "/tsutil";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_ucsdetst {
    char testname[] = "/ucsdetst";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_udatatst {
    char testname[] = "/udatatst";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_uset {
    char testname[] = "/uset";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_uspoof {
    char testname[] = "/uspoof";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_utf16tst {
    char testname[] = "/utf16tst";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_utf8tst {
    char testname[] = "/utf8tst";
    int result = call_main(testname);
    XCTAssertEqual(result, 0);
}

- (void)test_utrans {
    char testname[] = "/utrans";
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
