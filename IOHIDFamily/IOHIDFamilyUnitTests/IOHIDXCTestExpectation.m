//
//  IOHIDXCTestExpectation.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/12/18.
//

#import "IOHIDXCTestExpectation.h"

@implementation IOHIDXCTestExpectation

- (void)fulfill
{
    [super fulfill];
    
    ++_currentFulfillmentCount;
    
}


- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ (fulfillment:%lu expected:%lu)",
            self.expectationDescription,
            (unsigned long) _currentFulfillmentCount,
            (unsigned long) _expectedFulfillmentCount];
}

-(void)setExpectedFulfillmentCount:(NSUInteger)expectedFulfillmentCount
{
    _expectedFulfillmentCount = expectedFulfillmentCount;
    
    [super setExpectedFulfillmentCount:expectedFulfillmentCount];
}

@end
