//
//  TestDigitizerFingerCollection.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 11/4/19.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import <HID/HID.h>
#import <AssertMacros.h>
#import "IOHIDKeyboardWithTouchpadDescriptor.h"
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventData.h>

/** Test Objective : Once we have touch for one finger , we should keep on dispatching it as part of collection
 even if some of it's values are not updated as long as touch usage is set
 */

#define kTouchPadReportCount 6

static XCTestExpectation *_devAddedExp;
static XCTestExpectation *_eventCallback;

static uint8_t descriptor[] = {
    HIDTouchPadKeyboard
};


static NSArray<NSData*> *createReportsTouchOnly(NSUInteger count, uint16_t X, uint16_t Y) {
    
    // Reports
    // First finger should always be touch
    // Second & Third finger should be conbination of touch only and constant touch / untouch
    NSMutableArray *tmp = [[NSMutableArray alloc] init];
    
    for (NSUInteger i=0; i < count; i++) {
        
        NSMutableData *report = [[NSMutableData alloc] initWithLength:sizeof(HIDTouchPadKeyboardInputReport03)];
        
        HIDTouchPadKeyboardInputReport03 *touchPadReports = (HIDTouchPadKeyboardInputReport03*)report.mutableBytes;
        if (!touchPadReports) continue;
                
        memset(touchPadReports, 0, sizeof(HIDTouchPadKeyboardInputReport03));
        
        touchPadReports->reportId = 3;
        
        // First Finger
        // Fixed Touch
        touchPadReports->DIG_TouchPadFingerTransducerIndex = 3;
        touchPadReports->GD_TouchPadFingerX = X;
        touchPadReports->GD_TouchPadFingerY = Y;
        touchPadReports->DIG_TouchPadFingerTipSwitch = 1;
        touchPadReports->DIG_TouchPadFingerConfidence = 1;
        
        
        // Second Finger
        // Constant touch moving
        touchPadReports->DIG_TouchPadFingerTransducerIndex_1 = 4;
        touchPadReports->GD_TouchPadFingerX_1 = X + 10 + (i+1)*3;
        touchPadReports->GD_TouchPadFingerY_1 = Y + 10 + (i+1)*4;
        touchPadReports->DIG_TouchPadFingerTipSwitch_1 = 1;
        touchPadReports->DIG_TouchPadFingerConfidence_1 = 1;
        
        //Third Finger
        //Variable touch moving
        touchPadReports->DIG_TouchPadFingerTransducerIndex_2 = 5;
        touchPadReports->GD_TouchPadFingerX_2 = X + 10 + (i+1)*2;
        touchPadReports->GD_TouchPadFingerY_2 = Y + 10 + (i+1)*6;
        touchPadReports->DIG_TouchPadFingerTipSwitch_2 = (i%2 == 0) ? 1 : 0;
        touchPadReports->DIG_TouchPadFingerConfidence_2 = touchPadReports->DIG_TouchPadFingerTipSwitch_2;
        
        
        [tmp addObject:report];
    }
    return tmp;
}

@interface TestDigitizerFingerCollection : XCTestCase {
    HIDUserDevice           *_userDevice;
    HIDEventSystemClient    *_eventSystemClient;
    HIDServiceClient        *_serviceClient;
    NSString                *_uuid;
    NSMutableArray<HIDEvent*> *_events;
}

@end


@implementation TestDigitizerFingerCollection

-(void) createUserDevice {
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _userDevice != NULL);
    
}

-(void) createHIDEventSystemClient {
    
    
    __weak TestDigitizerFingerCollection *weakSelf = self;
    
    
    _eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _eventSystemClient != NULL);
    
    
    [_eventSystemClient setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid}];
    
    
    [_eventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
         __strong TestDigitizerFingerCollection *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        NSLog(@"Added : %@",service);
        
        strongSelf->_serviceClient = service;
        [_devAddedExp fulfill];
        
        
    }];
    
    
    [_eventSystemClient setEventHandler:^(HIDServiceClient * _Nullable service , HIDEvent * _Nonnull event) {
        
        __strong TestDigitizerFingerCollection *strongSelf = weakSelf;
        if (!strongSelf) return;

        NSString *uuid = [service propertyForKey:@kIOHIDPhysicalDeviceUniqueIDKey];
        if (![uuid isEqual:strongSelf->_uuid] && event.type != kIOHIDEventTypeDigitizer) {
            return;
        }
        NSLog(@"Event Value: %@",event);
        [strongSelf->_events addObject:event];
        
        if (strongSelf->_events.count == kTouchPadReportCount) {
            [_eventCallback fulfill];
        }
        
    }];
    
    [_eventSystemClient setDispatchQueue:dispatch_queue_create("com.apple.HID.Test", nil)];
    [_eventSystemClient activate];
    
}

-(void) setUp {
    
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _eventCallback = [[XCTestExpectation alloc] initWithDescription:@"event callback"];
    _events = [[NSMutableArray alloc] init];
    
    [self createHIDEventSystemClient];
    [self createUserDevice];
}

-(void) tearDown {
    [_eventSystemClient cancel];
    [super tearDown];
}

-(void) testDigitizerFingerCollection {
    
    XCTWaiterResult result;
    NSError *err = nil;
    BOOL ret = NO;
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    NSArray<NSData*> *reports = createReportsTouchOnly(kTouchPadReportCount, 10, 15);
    
    for (NSUInteger i =0; i < kTouchPadReportCount; i++) {
        ret = [_userDevice handleReport:reports[i] error:&err];
        NSLog(@"%@ -> Ret  = %s (%@)",reports[i], ret ? "Success" : "Failure",err);
        HIDXCTAssertWithLogs(ret,"%@",err);
    }
    result = [XCTWaiter waitForExpectations:@[_eventCallback] timeout:10];
       HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted);
    
    [self validateEvents:_events];
    
}

-(void) validateEvents:(NSArray<HIDEvent*>*) events {
    
    
    // Each event should dispatch three transducers
    // For Finger 1 and 2 we will always dispatch touch , so expect 6*2 touch
    // For Finger 3 half should be touch and half should be untouch
    // Total expectation for us would be 15 touch and 3 untouch
    __block NSUInteger touches = 0;
    __block NSUInteger untouches = 0;
    
    
    [events enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull event, NSUInteger idx __unused, BOOL * _Nonnull stop __unused) {
        
        if (event.type != kIOHIDEventTypeDigitizer) return;
        NSArray<HIDEvent*> *children =  event.children;
        
        // We dispatch 3 transducer finger, so we should expect all
        __block NSMutableArray *transducerIndexes = [[NSMutableArray alloc] initWithCapacity:3];
        
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, children && children.count > 0);
        
        // We should have 3 events for each collection
        
        [children enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull childEvent, NSUInteger idx1 __unused, BOOL * _Nonnull stop1 __unused) {
            
            if (childEvent.type != kIOHIDEventTypeDigitizer) return;
            
            NSInteger touch     = [childEvent integerValueForField:kIOHIDEventFieldDigitizerTouch];
            NSInteger transducerIndex = [childEvent integerValueForField:kIOHIDEventFieldDigitizerIndex];
            NSInteger inRange = [childEvent integerValueForField:kIOHIDEventFieldDigitizerRange];
            
            NSLog(@"Touch : %ld Transducer Index : %ld inRange %ld", touch, transducerIndex, inRange);
            
            //InRange and touch should be same
            HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, (touch ^ inRange) == 0);
            
            if (transducerIndex >= 3 && transducerIndex < 6) {
                transducerIndexes[transducerIndex%3] = @(YES);
                touches += touch;
                untouches +=  (touch == 0 ? 1 : 0);
            }
        }];
        
        for (NSInteger i=0; i < 3; i++) {
            NSLog(@"Transducer Index %ld : %s",i+3, transducerIndexes[i] ? "Yes" : "No");
            HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ((NSNumber*)transducerIndexes[i]).boolValue);
        }
        
    }];
    NSLog(@"Touches : %ld UnTouches : %ld", touches, untouches);
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, touches == 15 && untouches == 3);
    
}

@end
