//
//  TestConfigurableNXEventTranslation.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 11/12/18.
//

#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDEventData.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <AssertMacros.h>
#import <IOKit/hid/IOHIDServiceKeys.h>


static uint8_t keyboardDescriptor [] = {
    HIDKeyboardDescriptor
};

static uint8_t pointerDescriptor [] = {
    HIDPointerAbsoluteDescriptor
};

@interface TestConfigurableNXEventTranslation : XCTestCase

@property HIDUserDevice            *keyboardUserDevice;
@property HIDUserDevice            *pointerUserDevice;
@property HIDEventSystemClient     *hidEventSystemClient;
@property NSMutableSet             *hidServiceClients;
@property IOHIDXCTestExpectation   *keyboardServiceExpectation;
@property IOHIDXCTestExpectation   *pointerServiceExpectation;
@property IOHIDXCTestExpectation   *eventsExpectation;
@property IOHIDXCTestExpectation   *eventSystemClientCancelExpectation;
@property BOOL                     _dontExpectTranslatedEvents;
@end

@implementation TestConfigurableNXEventTranslation

- (void)setUp {
    
    [super setUp];
    
    _hidServiceClients = [[NSMutableSet alloc] init];
    
    __weak TestConfigurableNXEventTranslation *weakSelf = self;
    
    NSString *keyboardDeviceUUID = [[[NSUUID alloc] init] UUIDString];
    NSString *pointerDeviceUUID =  [[[NSUUID alloc] init] UUIDString];
    
    self.hidEventSystemClient  = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    self.keyboardServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Keyboard service expectation"];
    self.pointerServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Pointer service expectation"];
    
    self.eventSystemClientCancelExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Event System Client Cancel expectation"];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _hidEventSystemClient != NULL);
    
    [self.hidEventSystemClient setMatching:@{}];
    
    [self.hidEventSystemClient setDispatchQueue:dispatch_get_main_queue()];
    
    //set notification for device just plugin during test
    [self.hidEventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
        __strong TestConfigurableNXEventTranslation *strongSelf = weakSelf;
        
        if (!strongSelf || !service) {
            return;
        }
        
        NSString *uniqueID = [service propertyForKey:@kIOHIDUniqueIDKey];
    
         if ([uniqueID isEqualToString:keyboardDeviceUUID]) {
             
             [strongSelf.keyboardServiceExpectation fulfill];
             [strongSelf.hidServiceClients addObject:service];
             
         }  else if ([uniqueID isEqualToString:pointerDeviceUUID]) {
             
             [strongSelf.pointerServiceExpectation fulfill];
             [strongSelf.hidServiceClients addObject:service];
             
         }
    }];
    
    [self.hidEventSystemClient setCancelHandler:^{
        
        __strong TestConfigurableNXEventTranslation *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        [strongSelf.eventSystemClientCancelExpectation fulfill];
    }];
    
    [self.hidEventSystemClient setEventHandler:^(HIDServiceClient * _Nullable service, HIDEvent * _Nonnull  event) {
        
        __strong TestConfigurableNXEventTranslation *strongSelf = weakSelf;
        if (!strongSelf || !service) {
            return;
        }
        
        if (![strongSelf.hidServiceClients containsObject:service]) {
            return;
        }
        
        [strongSelf.eventsExpectation fulfill];
        
        [strongSelf checkTranslationFlagForEvent:event];
        
    }];
    
    [self.hidEventSystemClient activate];
    
     _keyboardUserDevice =  [[HIDUserDevice alloc] initWithProperties:@{@kIOHIDReportDescriptorKey : [[NSData alloc] initWithBytes:keyboardDescriptor length:sizeof(keyboardDescriptor)], @kIOHIDUniqueIDKey : keyboardDeviceUUID }];
    
     _pointerUserDevice =  [[HIDUserDevice alloc] initWithProperties:@{
                                @kIOHIDReportDescriptorKey : [[NSData alloc] initWithBytes:pointerDescriptor
                                                                                    length:sizeof(pointerDescriptor)],
                                @kIOHIDUniqueIDKey : pointerDeviceUUID,
                                @kIOHIDVendorIDKey : @(1452),
                                @kIOHIDProductIDKey : @(123) }];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, _keyboardUserDevice != NULL && _pointerUserDevice != NULL);
   
}

- (void)tearDown {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    [self.hidEventSystemClient cancel];
    
    result = [XCTWaiter waitForExpectations:@[self.eventSystemClientCancelExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, result == XCTWaiterResultCompleted, "%@", @[self.eventSystemClientCancelExpectation]);
    
    [super tearDown];
}
-(void) checkTranslationFlagForEvent:(HIDEvent*) event
{
    if (!event) {
        return;
    }
    
    // get parent usage
    // redundant but check to make sure
   
    if (__dontExpectTranslatedEvents &&  event.type == kIOHIDEventTypeVendorDefined) {
        
        NSInteger usage =  [event integerValueForField:kIOHIDEventFieldVendorDefinedUsage];
        
        if (usage == kHIDUsage_AppleVendor_NXEvent_Translated) {
           HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, 0);
        }
    }
    
    for (HIDEvent *childEvent in event.children) {
        [self checkTranslationFlagForEvent:childEvent];
    }
}
-(BOOL) dispatchKeyboardEvent
{
    BOOL ret = NO;
    
    HIDKeyboardDescriptorInputReport report;
    HIDKeyboardDescriptorInputReport *ptr = NULL;
    memset (&report, 0 , sizeof(report));
    
    NSMutableData *reportData = [[NSMutableData alloc] initWithBytes:&report length:sizeof(HIDKeyboardDescriptorInputReport)];
    
    ptr = (HIDKeyboardDescriptorInputReport*)[reportData bytes];
    ptr->KB_Keyboard[0] = kHIDUsage_KeyboardD;
    
    require([self.keyboardUserDevice handleReport:reportData error:nil], exit);
    
    ptr->KB_Keyboard[0] = 0;
    
    require([self.keyboardUserDevice handleReport:reportData error:nil], exit);
    
    ret = YES;
exit:
    return ret;
}
-(BOOL) dispatchPointerEvent
{
    BOOL ret = NO;
    
    HIDPointerAbsoluteDescriptorInputReport report;
    HIDPointerAbsoluteDescriptorInputReport *ptr = NULL;
    memset (&report, 0 , sizeof(report));
    
    NSMutableData *reportData = [[NSMutableData alloc] initWithBytes:&report length:sizeof(HIDPointerAbsoluteDescriptorInputReport)];
    
    // translated event is added for pointer event on different coordinate
    // pointer event pattern
    
    ptr = (HIDPointerAbsoluteDescriptorInputReport*)[reportData bytes];
    ptr->GD_MousePointerX = 1;
    ptr->GD_MousePointerY = 1;
    
    require([self.pointerUserDevice handleReport:reportData error:nil], exit);
    
    ptr->GD_MousePointerX = 3;
    ptr->GD_MousePointerY = 2;
    
    require([self.pointerUserDevice handleReport:reportData error:nil], exit);
    
    
    ptr->GD_MousePointerX = 2;
    ptr->GD_MousePointerY = 1;
    
    require([self.pointerUserDevice handleReport:reportData error:nil], exit);
    
    ptr->GD_MousePointerX = 0;
    ptr->GD_MousePointerY = 0;
    
    require([self.pointerUserDevice handleReport:reportData error:nil], exit);
    
    ret = YES;
    
exit:
    return ret;
}

- (void) testConfigurableNXEventTranslation {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    result = [XCTWaiter waitForExpectations:@[self.keyboardServiceExpectation, self.pointerServiceExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.keyboardServiceExpectation, self.pointerServiceExpectation]);
    
    self.eventsExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Events Expectation"];
    [self.eventsExpectation setExpectedFulfillmentCount:12];
    
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self dispatchKeyboardEvent],"Failed to disptach keyboard report");
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self dispatchPointerEvent],"Failed to disptach pointer report");
    
    result = [XCTWaiter waitForExpectations:@[self.eventsExpectation] timeout:5];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.eventsExpectation]);
    
    // wait for all previous events to be dispatched
    
    // Test : Property on Service
    for (HIDServiceClient *client in _hidServiceClients) {
         [client setProperty:@(0) forKey:@kIOHIDNXEventTranslation];
    }
    
    // Set session filter property
    [_hidEventSystemClient setProperty:@(0) forKey:@kIOHIDNXEventTranslation];
    
    
    self.eventsExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Non Translated Events Expectation"];
    [self.eventsExpectation setExpectedFulfillmentCount:12];
    
    __dontExpectTranslatedEvents = true;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self dispatchKeyboardEvent],"Failed to disptach keyboard report");
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,[self dispatchPointerEvent],"Failed to disptach pointer report");
    
    result = [XCTWaiter waitForExpectations:@[self.eventsExpectation] timeout:5];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.eventsExpectation]);
    
}

@end
