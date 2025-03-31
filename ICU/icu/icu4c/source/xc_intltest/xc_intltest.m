//
//  xc_intltest.m
//  xc_intltest
//
//  Created by Christopher Chapman on 2024-10-14.
//  Copyright © 2024 Apple. All rights reserved.
//

#import <XCTest/XCTest.h>

extern int main(int argc, char* argv[]);

int call_main(char *argv1) {
    char argv0[] = "intltest";
    int argc = 2;
    char* argv[] = {argv0, argv1, NULL};
    int result = main(argc, argv);
    return result;
}
    
@interface xc_intltest : XCTestCase

@end

@implementation xc_intltest

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)test_utility {
    char argv1[] = "utility";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_normalize {
    char argv1[] = "normalize";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_collate {
    char argv1[] = "collate";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_regex {
    char argv1[] = "regex";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_format {
    char argv1[] = "format";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_translit {
    char argv1[] = "translit";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_rbbi {
    char argv1[] = "rbbi";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_rbnf {
    char argv1[] = "rbnf";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_rbnfrt {
    char argv1[] = "rbnfrt";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_icuserv {
    char argv1[] = "icuserv";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_idna {
    char argv1[] = "idna";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_convert {
    char argv1[] = "convert";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_rbnfp {
    char argv1[] = "rbnfp";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_csdet {
    char argv1[] = "csdet";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_spoof {
    char argv1[] = "spoof";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}
- (void)test_bidi {
    char argv1[] = "bidi";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

// - (void)testExample {
//     // This is an example of a functional test case.
//     // Use XCTAssert and related functions to verify your tests produce the correct results.
//     char argv0[] = "intltest";
//     char argv1[] = "LIST";
//     int argc = 2;
//     const char* const argv[] = {argv0, argv1, NULL};
//     int result = main(argc, argv);
//     printf("result: %d\n", result);

//     XCTAssertEqual(result, 0);

// }

//- (void)testPerformanceExample {
//    // This is an example of a performance test case.
//    [self measureBlock:^{
//        // Put the code you want to measure the time of here.
//    }];
//}

@end
