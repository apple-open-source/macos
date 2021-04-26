// DANGER Objective C++


#import <XCTest/XCTest.h>
#include <security_utilities/threading.h>

struct ThreadTestClass : public Thread {
    XCTestExpectation *expectation;
    
    ThreadTestClass(XCTestExpectation *e): Thread("test"), expectation(e) {  }
    void threadAction() {
        [expectation fulfill];
    }
};

@interface legacyCodeRegressions : XCTestCase
@end

@implementation legacyCodeRegressions

- (void)testThread {
    XCTestExpectation *e = [self expectationWithDescription:@"wait"];
    
    auto t = new ThreadTestClass(e);
    t->threadRun();
    [self waitForExpectations:@[e] timeout:10.0];
    delete t;
}

@end
