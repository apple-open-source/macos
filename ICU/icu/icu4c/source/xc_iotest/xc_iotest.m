//
//  xc_iotest.m
//  xc_iotest
//
//  Created by Christopher Chapman on 2024-10-30.
//  Copyright © 2024 Apple. All rights reserved.
//

#import <XCTest/XCTest.h>

extern int main(int argc, char* argv[]);

int call_main(char *argv1) {
    char argv0[] = "iotest";
    int argc = 2;
    char* argv[] = {argv0, argv1, NULL};
    int result = main(argc, argv);
    return result;
}

@interface xc_iotest : XCTestCase

@end

@implementation xc_iotest

//- (void)setUp {
//    // Put setup code here. This method is called before the invocation of each test method in the class.
//}

//- (void)tearDown {
//    // Put teardown code here. This method is called after the invocation of each test method in the class.
//}

//- (void)testExample {
//    char argv1[] = "-h";
//    int result = call_main(argv1);
//    XCTAssertEqual(result, 0);
//}

- (void)test_datadriv {
    char argv1[] = "/datadriv";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_file {
    char argv1[] = "/file";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_stream {
    char argv1[] = "/stream";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_string {
    char argv1[] = "/string";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_translit {
    char argv1[] = "/translit";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

- (void)test_ScanfMultipleIntegers {
    char argv1[] = "/ScanfMultipleIntegers";
    int result = call_main(argv1);
    XCTAssertEqual(result, 0);
}

//- (void)testPerformanceExample {
//    // This is an example of a performance test case.
//    [self measureBlock:^{
//        // Put the code you want to measure the time of here.
//    }];
//}

@end
