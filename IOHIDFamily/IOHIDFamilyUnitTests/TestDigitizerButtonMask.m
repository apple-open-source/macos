//
//  TestDigitizerButtonMask.m
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

/** Test Objective : Button mask to set on collection element
 */

#define kTouchPadReportCount 6

static XCTestExpectation *_devAddedExp;
static XCTestExpectation *_eventCallback;

static uint8_t descriptor[] = {
    HIDTouchPadKeyboard
};


static NSArray<NSData*> *createReportsButtonOnly(NSUInteger count, uint16_t X, uint16_t Y) {
    

    NSMutableArray *tmp = [[NSMutableArray alloc] init];
    
    for (NSUInteger i=0; i < count; i++) {
        
        NSMutableData *report = [[NSMutableData alloc] initWithLength:sizeof(HIDTouchPadKeyboardInputReport03)];
        
        HIDTouchPadKeyboardInputReport03 *touchPadReports = (HIDTouchPadKeyboardInputReport03*)report.mutableBytes;
        if (!touchPadReports) continue;
                
        memset(touchPadReports, 0, sizeof(HIDTouchPadKeyboardInputReport03));
        
        touchPadReports->reportId = 3;
        
        // First Finger
        // Button only , it should also have touch , but just to verify button logic
        touchPadReports->BTN_TouchPadButton1 = 1;
        
        touchPadReports->DIG_TouchPadFingerTransducerIndex = 3;
        touchPadReports->GD_TouchPadFingerX = X;
        touchPadReports->GD_TouchPadFingerY = Y;
        
        
        // Second Finger
        // Constant touch
        touchPadReports->DIG_TouchPadFingerTransducerIndex_1 = 4;
        touchPadReports->GD_TouchPadFingerX_1 = X;
        touchPadReports->GD_TouchPadFingerY_1 = Y;
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

@interface TestDigitizerButtonMask : XCTestCase {
    HIDUserDevice           *_userDevice;
    HIDEventSystemClient    *_eventSystemClient;
    HIDServiceClient        *_serviceClient;
    NSString                *_uuid;
    NSMutableArray<HIDEvent*> *_events;
}

@end


@implementation TestDigitizerButtonMask

-(void) createUserDevice {
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _userDevice != NULL);
    
}

-(void) createHIDEventSystemClient {
    
    
    __weak TestDigitizerButtonMask *weakSelf = self;
    
    
    _eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _eventSystemClient != NULL);
    
    
    [_eventSystemClient setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid}];
    
    
    [_eventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
         __strong TestDigitizerButtonMask *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        NSLog(@"Added : %@",service);
        
        strongSelf->_serviceClient = service;
        [_devAddedExp fulfill];
        
        
    }];
    
    
    [_eventSystemClient setEventHandler:^(HIDServiceClient * _Nullable service , HIDEvent * _Nonnull event) {
        
        __strong TestDigitizerButtonMask *strongSelf = weakSelf;
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

-(void) testDigitizerButtonMask {
    
    XCTWaiterResult result;
    NSError *err = nil;
    BOOL ret = NO;
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    NSArray<NSData*> *reports = createReportsButtonOnly(kTouchPadReportCount, 10, 15);
    
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
    
    // Button mask should only be published on parent collection
    [events enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull event, NSUInteger idx __unused, BOOL * _Nonnull stop __unused) {
        
        if (event.type != kIOHIDEventTypeDigitizer) return;
        NSArray<HIDEvent*> *children =  event.children;
        
        // We dispatch 3 transducer finger, so we should expect all
        __block uint8_t *transducerIndexes = (uint8_t*)malloc(sizeof(uint8_t)*3);
        __block NSInteger buttonMask = [event integerValueForField:kIOHIDEventFieldDigitizerButtonMask];
        
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, children && children.count > 0);
        
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, buttonMask);
        
        // We should have 3 events for each collection
        // Button mask should be set on one child event and no other event should have it
        
        [children enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull childEvent, NSUInteger idx1 __unused, BOOL * _Nonnull stop1 __unused) {
            
            if (childEvent.type != kIOHIDEventTypeDigitizer) return;
            
            NSInteger transducerIndex = [childEvent integerValueForField:kIOHIDEventFieldDigitizerIndex];
            buttonMask =  [childEvent integerValueForField:kIOHIDEventFieldDigitizerButtonMask];
            
            NSLog(@"Transducer Index : %ld Button Mask : %ld", transducerIndex, buttonMask);
            if (transducerIndex >= 3 && transducerIndex < 6) {
                transducerIndexes[transducerIndex%3] = YES;
            }

        }];
        
        for (NSInteger i=0; i < 3; i++) {
            NSLog(@"Transducer Index %ld : %s",i+3, transducerIndexes[i] ? "Yes" : "No");
            HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, transducerIndexes[i]);
        }
        
        if (transducerIndexes) {
            free(transducerIndexes);
        }
        NSLog(@"========");
        
    }];
    
}

@end
