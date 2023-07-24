//
//  SecLaunchSequenceTests.m
//

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <Security/SecLaunchSequence.h>


@interface SecLaunchSequenceTests : XCTestCase
@end

@implementation SecLaunchSequenceTests

- (void)testLaunchAttributes {
    SecLaunchSequence *launch = [[SecLaunchSequence alloc] initWithRocketName:@"rocket"];
    [launch addEvent:@"keyword1"];
    usleep(1000);
    [launch addEvent:@"keyword2"];
    [launch addAttribute:@"attribute" value:@"value"];

    [launch launch];

    NSArray *res = [launch eventsByTime];
    XCTAssert(res.count > 0, "should have event");
    bool found = false;
    for (NSString *event in res) {
        found |= [event hasPrefix:@"attr:"];
    }
    XCTAssert(found, "should have an attribute");
}

@end
