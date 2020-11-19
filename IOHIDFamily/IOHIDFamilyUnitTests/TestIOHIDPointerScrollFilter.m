//
//  TestIOHIDPointerScrollFilter
//  IOHIDFamilyUnitTests
//
//  Created by AB on 9/4/18.
//

#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDEventData.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <IOKit/hid/IOHIDKeys.h>

 static uint8_t descriptor [] = {
     HIDMouseDescriptor
 };

@interface TestIOHIDPointerScrollFilter : XCTestCase

@property HIDUserDevice            *userDevice;
@property HIDEventSystemClient     *hidEventSystemClient;
@property HIDServiceClient         *hidServiceClient;
@property IOHIDXCTestExpectation   *mouseServiceExpectation;
@property IOHIDXCTestExpectation   *eventExpectation;
@property IOHIDXCTestExpectation   *eventSystemClientCancelExpectation;
@property bool                     gotAcceleratedEvent;
@property bool                     gotNonAcceleratedEvent;
@property bool                     gotNonAcceleratedScrollEvent;
@end

@implementation TestIOHIDPointerScrollFilter

- (void)setUp {
    
    __weak TestIOHIDPointerScrollFilter *weakSelf = self;
    _gotAcceleratedEvent = false;
    _gotNonAcceleratedEvent = false;
    _gotNonAcceleratedScrollEvent = false;
    
    [super setUp];
    
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    self.hidEventSystemClient  = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _hidEventSystemClient != NULL);
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.mouseServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Mouse service expectation"];
    
    self.eventSystemClientCancelExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Event System Client Cancel expectation"];
    
    self.eventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"mouse Event Expectation"];

    [self.eventExpectation setExpectedFulfillmentCount:4];

    [self.hidEventSystemClient setMatching:@{@kIOHIDUniqueIDKey : uniqueID}];
    
    [self.hidEventSystemClient setDispatchQueue:dispatch_get_main_queue()];
    
    //set notification for device just plugin during test
    [self.hidEventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
        __strong TestIOHIDPointerScrollFilter *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        strongSelf.hidServiceClient = service;
        [strongSelf.mouseServiceExpectation fulfill];
    }];
    
    [self.hidEventSystemClient setCancelHandler:^{
        
        __strong TestIOHIDPointerScrollFilter *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        [strongSelf.eventSystemClientCancelExpectation fulfill];
    }];
    
    [self.hidEventSystemClient setEventHandler:^(HIDServiceClient * _Nullable service, HIDEvent * _Nonnull event) {
        
        __strong TestIOHIDPointerScrollFilter *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
       
        
        [strongSelf.eventExpectation fulfill];
        
        [strongSelf checkScrollEvent:event forService:service];
    }];
    
    [self.hidEventSystemClient activate];
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties:@{@kIOHIDReportDescriptorKey : descriptorData,
                                                                  @kIOHIDUniqueIDKey : uniqueID }];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice != NULL);
    
}

- (void)tearDown {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    [self.hidEventSystemClient cancel];
    
    result = [XCTWaiter waitForExpectations:@[self.eventSystemClientCancelExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, result == XCTWaiterResultCompleted, "%@", @[self.eventSystemClientCancelExpectation]);
    
    [super tearDown];
}

-(void) checkScrollEvent:(HIDEvent* _Nonnull) event forService:(HIDServiceClient * _Nullable) service
{
    
    if (!service || ![service conformsToUsagePage:kHIDPage_GenericDesktop usage:kHIDUsage_GD_Mouse]) {
        NSLog(@"Not Valid Service %@",service);
        return;
    }
    
    NSLog(@"%@",event);
    
    NSInteger pointerXValue = [event integerValueForField:kIOHIDEventFieldPointerX];
    NSInteger scrollYValue = [event integerValueForField:kIOHIDEventFieldScrollY];


    UInt32 eventFlag = IOHIDEventGetEventFlags((__bridge IOHIDEventRef)event);
  
    NSArray *childEvents = event.children;
    
    for (NSUInteger i=0; childEvents && i < childEvents.count; i++) {
        HIDEvent *child = [childEvents objectAtIndex:i];
        NSLog(@"%@",child);
        eventFlag = IOHIDEventGetEventFlags((__bridge IOHIDEventRef)child);
        
        if (pointerXValue == 4 && (eventFlag & kIOHIDAccelerated)) {
            self.gotAcceleratedEvent = true;
        }
        
        if (pointerXValue == 1 && !(eventFlag & kIOHIDAccelerated)) {
            self.gotNonAcceleratedEvent = true;
        }

        if (scrollYValue == 12 && !(eventFlag & kIOHIDAccelerated)) {
            self.gotNonAcceleratedScrollEvent = true;
            NSLog(@"Y scroll");
        }
    }
    
    if (!childEvents && !(eventFlag & kIOHIDAccelerated) && pointerXValue == 1) {
        
        self.gotNonAcceleratedEvent = true;
    }
    
}
- (void)testHIDPointerScrollFilter {
    
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    result = [XCTWaiter waitForExpectations:@[self.mouseServiceExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.mouseServiceExpectation]);
    
    // Test : Property on Service
    //Set acceleration type as pointer acceleration
    [_hidServiceClient setProperty:@kIOHIDPointerAccelerationKey forKey:@kIOHIDPointerAccelerationTypeKey];
    
    int accelerationSet = 3;
    int accelerationGet = 0;
    
    // Set Property for IOHIDScrollFilterPlugin
    [_hidServiceClient setProperty:@(accelerationSet) forKey:@kIOHIDPointerAccelerationKey];
    
    accelerationGet = [[_hidServiceClient propertyForKey:@kIOHIDPointerAccelerationKey] unsignedIntValue];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, accelerationSet == accelerationGet, "@kIOHIDPointerAccelerationKey set : %u get : %u", accelerationSet, accelerationGet);
    
    // Test : Setting Report on service
    HIDMouseDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.GD_MousePointerX = 4;
    
    NSData *reportData = [[NSData alloc] initWithBytes:&report length:sizeof(HIDMouseDescriptorInputReport)];
   
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self.userDevice handleReport:reportData error:nil],"Failed to disptach report");
    
    report.GD_MousePointerX = 1;
    
    reportData = [[NSData alloc] initWithBytes:&report length:sizeof(HIDMouseDescriptorInputReport)];
    
    //Set Acceleration value  = 0;
    accelerationSet = -1;
    accelerationGet = 0xff;
    
    // Set Property for IOHIDScrollFilterPlugin
    [_hidServiceClient setProperty:@(accelerationSet) forKey:@kIOHIDPointerAccelerationKey];
    
    accelerationGet = [[_hidServiceClient propertyForKey:@kIOHIDPointerAccelerationKey] unsignedIntValue];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, accelerationSet == accelerationGet, "@kIOHIDPointerAccelerationKey set : %u get : %u", accelerationSet, accelerationGet);
    
   
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self.userDevice handleReport:reportData error:nil],"Failed to disptach report");
    

    result = [XCTWaiter waitForExpectations:@[self.eventExpectation] timeout:5];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, result == XCTWaiterResultCompleted, "%@", @[self.eventExpectation]);
    
    XCTAssert(self.gotAcceleratedEvent == true, "Failed to get accelerated event");
    XCTAssert(self.gotNonAcceleratedEvent == true, "Failed to get non-accelerated event");
}

- (void)testAcclerationProperty
{
    XCTWaiterResult result = XCTWaiterResultCompleted;

    result = [XCTWaiter waitForExpectations:@[self.mouseServiceExpectation] timeout:3];

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.mouseServiceExpectation]);

    // Test : Property on Service
    //Set acceleration type as pointer acceleration key
    [_hidServiceClient setProperty:@(NO) forKey:@kIOHIDPointerAccelerationSupportKey];

    [_hidServiceClient setProperty:@(NO) forKey:@kIOHIDScrollAccelerationSupportKey];

    bool accelerationEnabled = true;

    accelerationEnabled = [[_hidServiceClient propertyForKey:@kIOHIDPointerAccelerationSupportKey] boolValue];

    HIDXCTAssertWithParameters (RETURN_FROM_TEST, !accelerationEnabled, "@kIOHIDPointerAccelerationSupportKey : %u", accelerationEnabled);

    accelerationEnabled = [[_hidServiceClient propertyForKey:@kIOHIDScrollAccelerationSupportKey] boolValue];

    HIDXCTAssertWithParameters (RETURN_FROM_TEST, !accelerationEnabled, "@kIOHIDScrollAccelerationSupportKey : %u", accelerationEnabled);

    // Test : Setting Report on service
    HIDMouseDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));

    report.GD_MousePointerX = 1;

    NSData *reportData = [[NSData alloc] initWithBytes:&report length:sizeof(HIDMouseDescriptorInputReport)];

    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self.userDevice handleReport:reportData error:nil],"Failed to disptach report");

    report.GD_MousePointerWheel = 12;

    reportData = [[NSData alloc] initWithBytes:&report length:sizeof(HIDMouseDescriptorInputReport)];

    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self.userDevice handleReport:reportData error:nil],"Failed to disptach report");


    result = [XCTWaiter waitForExpectations:@[self.eventExpectation] timeout:5];

    HIDXCTAssertWithParameters (RETURN_FROM_TEST, result == XCTWaiterResultCompleted, "%@", @[self.eventExpectation]);

    XCTAssert(self.gotNonAcceleratedEvent == true, "Failed to get non-accelerated event");
    XCTAssert(self.gotNonAcceleratedScrollEvent == true, "Failed to get non-accelerated scroll event");
}

@end
