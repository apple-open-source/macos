//
//  CKKSLaunchSequenceTests.m
//

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import "keychain/analytics/CKKSLaunchSequence.h"
#import <utilities/SecCoreAnalytics.h>


@interface CKKSLaunchSequenceTests : XCTestCase
@property (strong) XCTestExpectation *launchExpection;
@end

@implementation CKKSLaunchSequenceTests

- (void)mockSendEvent:(NSDictionary *)name event:(NSDictionary *)event {
    NSLog(@"CAEvent: %@", event);
    [self.launchExpection fulfill];
}

- (void)testLaunch {

    id credentialStoreMock = OCMClassMock([SecCoreAnalytics class]);
    OCMStub([credentialStoreMock sendEvent:[OCMArg any] event:[OCMArg any]]).andCall(self, @selector(mockSendEvent:event:));

    CKKSLaunchSequence *launch = [[CKKSLaunchSequence alloc] initWithRocketName:@"rocket"];
    [launch addEvent:@"keyword1"];
    usleep(1000);
    [launch addEvent:@"keyword2"];
    [launch addAttribute:@"attribute" value:@"value"];

    self.launchExpection = [self expectationWithDescription:@"launch"];

    [launch launch];

    [self waitForExpectations:@[self.launchExpection] timeout:0.2];

    self.launchExpection = [self expectationWithDescription:@"launch"];
    self.launchExpection.inverted = true;

    [launch launch];

    [self waitForExpectations:@[self.launchExpection] timeout:0.2];
    self.launchExpection = nil;

    NSArray *res = [launch eventsByTime];
    XCTAssert(res.count > 0, "should have event");
    bool found = false;
    for (NSString *event in res) {
        found |= [event hasPrefix:@"attr:"];
    }
    XCTAssert(found, "should have an attribute");



}

@end
