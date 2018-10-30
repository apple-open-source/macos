//
//  IOHIDXCTestExpectation.h
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/12/18.
//

#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDXCTestExpectation : XCTestExpectation {
    NSUInteger _currentFulfillmentCount;
    NSUInteger _expectedFulfillmentCount;
}

@end

NS_ASSUME_NONNULL_END
