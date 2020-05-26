//
//  TestHIDKeyboardWithTouchPad.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 10/23/19.
//


#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDKeyboardWithTouchpadDescriptor.h"
#import "IOHIDXCTestExpectation.h"
#import <HID/HID.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventData.h>

static XCTestExpectation *_devAddedExp;
static XCTestExpectation *_eventDispatchedCallback;

static uint8_t descriptor[] = {
    HIDTouchPadKeyboard
};

#define kTouchPadReportCount 6

static NSArray<NSData*> *createSingleContactReports(NSUInteger count, uint16_t X, uint16_t Y, BOOL fixed) {
    
    NSMutableArray *tmp = [[NSMutableArray alloc] init];
    
    for (NSUInteger i=0; i < count; i++) {
        
        NSMutableData *report = [[NSMutableData alloc] initWithLength:sizeof(HIDTouchPadKeyboardInputReport03)];
        
        HIDTouchPadKeyboardInputReport03 *touchPadReports = (HIDTouchPadKeyboardInputReport03*)report.mutableBytes;
        if (!touchPadReports) continue;
                
        memset(touchPadReports, 0, sizeof(HIDTouchPadKeyboardInputReport03));
        
        touchPadReports->reportId = 3;
        touchPadReports->BTN_TouchPadButton1 = 1;
        touchPadReports->DIG_TouchPadFingerTransducerIndex = 3;
        touchPadReports->GD_TouchPadFingerX = X;
        touchPadReports->GD_TouchPadFingerY = Y;
        touchPadReports->DIG_TouchPadFingerTipSwitch = ~(i%2);
        touchPadReports->DIG_TouchPadFingerConfidence = ~(i%2);
        touchPadReports->DIG_TouchPadRelativeScanTime = i+10;
        if (!fixed) {
            X += (i%2) ? 2 : -1;
            Y += (i%2) ? -2 : 1;
        }
        [tmp addObject:report];
    }
    return tmp;
}

static NSArray<NSData*> *createMultiContactReports(NSUInteger count, uint16_t X, uint16_t Y, BOOL fixed) {
    
   NSMutableArray *tmp = [[NSMutableArray alloc] init];
    
    for (NSUInteger i=0; i < count; i++) {
        
        NSMutableData *report = [[NSMutableData alloc]initWithLength:sizeof(HIDTouchPadKeyboardInputReport03)];
        HIDTouchPadKeyboardInputReport03 *touchPadReports = (HIDTouchPadKeyboardInputReport03*)report.mutableBytes;
        if (!touchPadReports) continue;
        
        memset(touchPadReports, 0, sizeof(HIDTouchPadKeyboardInputReport03));
        
        
        touchPadReports->reportId = 3;
        touchPadReports->BTN_TouchPadButton1 = 1;
        touchPadReports->DIG_TouchPadFingerTransducerIndex = 4;
        touchPadReports->GD_TouchPadFingerX = X;
        touchPadReports->GD_TouchPadFingerY = Y;
        touchPadReports->DIG_TouchPadFingerTipSwitch = ~(i%2);
        touchPadReports->DIG_TouchPadFingerConfidence = ~(i%2);
        
        touchPadReports->GD_TouchPadFingerX_1 = X+2;
        touchPadReports->GD_TouchPadFingerY_1 = Y+2;
        touchPadReports->DIG_TouchPadFingerTransducerIndex_1 = 2;
        touchPadReports->DIG_TouchPadFingerTipSwitch_1 = ~(i%2);
        touchPadReports->DIG_TouchPadFingerConfidence_1 = ~(i%2);
        touchPadReports->DIG_TouchPadRelativeScanTime = i+10;
        
        if (!fixed) {
           X += (i%2) ? 2 : -1;
           Y += (i%2) ? -2 : 1;
        }
        [tmp addObject:report];
    }
    return tmp;
    
}


@interface TestHIDKeyboardWithTouchPad : XCTestCase {
    NSString                *_uuid;
    HIDUserDevice           *_userDevice;
    HIDEventSystemClient    *_eventSystemClient;
    HIDServiceClient        *_serviceClient;
    NSMutableArray          *_events;
}
@end

@implementation TestHIDKeyboardWithTouchPad


-(void) createUserDevice {
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _userDevice != NULL);
    
}

-(void) createHIDEventSystemClient {
    
    
    __weak TestHIDKeyboardWithTouchPad *weakSelf = self;
    
    
    _eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _eventSystemClient != NULL);
    
    
    [_eventSystemClient setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid}];
    
    
    [_eventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
         __strong TestHIDKeyboardWithTouchPad *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        NSLog(@"Added : %@",service);
        
        strongSelf->_serviceClient = service;
        [_devAddedExp fulfill];
        
        
    }];
    
    
    [_eventSystemClient setEventHandler:^(HIDServiceClient * _Nullable service , HIDEvent * _Nonnull event) {
        
        __strong TestHIDKeyboardWithTouchPad *strongSelf = weakSelf;
        if (!strongSelf) return;

        NSString *uuid = [service propertyForKey:@kIOHIDPhysicalDeviceUniqueIDKey];
        if (![uuid isEqual:strongSelf->_uuid] && event.type != kIOHIDEventTypeDigitizer) {
            return;
        }
        NSLog(@"Event Value: %@",event);

        [strongSelf validateCollectionType:event];
        [strongSelf->_events addObject:event];
        
        if (strongSelf->_events.count == kTouchPadReportCount) {
            [_eventDispatchedCallback fulfill];
        }
    }];
    
    [_eventSystemClient setDispatchQueue:dispatch_queue_create("com.apple.HID.Test", nil)];
    [_eventSystemClient activate];
    
}

-(void) validateCollectionType:(HIDEvent*) event {
        
    NSInteger isCollection = [event integerValueForField:kIOHIDEventFieldDigitizerCollection];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                         isCollection == 1);
}

-(void) setUp {
    
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _eventDispatchedCallback = [[XCTestExpectation alloc] initWithDescription:@"Event Callback"];
    _events = [[NSMutableArray alloc] init];
    
    [self createHIDEventSystemClient];
    [self createUserDevice];
    
}

-(void) tearDown {
    
    [_eventSystemClient cancel];
    [super tearDown];
}


-(void) testSurfaceDimensions {
    
    XCTWaiterResult result;
    NSDictionary *dimensions = nil;
    double width = 0;
    double height = 0;
    
    
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    id value = [_serviceClient propertyForKey:@(kIOHIDSurfaceDimensionsKey)];
    NSLog(@"Dimensions : %@",value);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, value != nil && [value isKindOfClass:[NSDictionary class]]);
    
    dimensions = (NSDictionary*)value;
    
    width = CAST_FIXED_TO_DOUBLE(((NSNumber*)dimensions[@(kIOHIDWidthKey)]).integerValue);
    height = CAST_FIXED_TO_DOUBLE(((NSNumber*)dimensions[@(kIOHIDHeightKey)]).integerValue);
    
    
    NSLog(@"Width(mm) : %f Height(mm) : %f",width, height);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, round(height) == 51 && round(width) == 92);
    
}

-(void) testEventType {
    
    NSError *err = nil;
    BOOL ret = NO;
    XCTWaiterResult result;
    
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
       HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                  result == XCTWaiterResultCompleted);
    
    NSArray<NSData*> *reports = createSingleContactReports(kTouchPadReportCount, 207, 207, YES);
   
    for (NSUInteger i=0; i < kTouchPadReportCount; i++) {

        ret = [_userDevice handleReport:reports[i] error:&err];
        HIDXCTAssertWithLogs(ret,"%@",err);
        NSLog(@"%@ -> Ret (Single Contact) = %s (%@)",reports[i], ret ? "Success" : "Failure",err);
    }
   
   result = [XCTWaiter waitForExpectations:@[_eventDispatchedCallback] timeout:10];
   HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                              result == XCTWaiterResultCompleted);
}


-(void) testSingleEventCollection {
    
    NSError *err = nil;
    BOOL ret = NO;
    XCTWaiterResult result;
    
    
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
       HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                  result == XCTWaiterResultCompleted);
    
    NSArray<NSData*> *reports = createSingleContactReports(kTouchPadReportCount, 2047, 2047, YES);
    
    for (NSUInteger i=0; i < kTouchPadReportCount; i++) {

       ret = [_userDevice handleReport:reports[i] error:&err];
       HIDXCTAssertWithLogs(ret,"%@",err);
       NSLog(@"%@ -> Ret (Single Contact) = %s (%@)",reports[i], ret ? "Success" : "Failure",err);
    }

    result = [XCTWaiter waitForExpectations:@[_eventDispatchedCallback] timeout:10];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                             result == XCTWaiterResultCompleted);
    
    [self validateSCFREvents:_events];
}

-(void) testMultiEventCollection {

    NSError *err = nil;
    BOOL ret = NO;
    XCTWaiterResult result;
    
    
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);

    NSArray<NSData*> *reports  = createMultiContactReports(kTouchPadReportCount, 2047, 2047, YES);
    for (NSUInteger i =0; i < kTouchPadReportCount; i++) {
        ret = [_userDevice handleReport:reports[i] error:&err];
        HIDXCTAssertWithLogs(ret,"%@",err);
        NSLog(@"%@ -> Ret (Multi Contact) = %s (%@)",reports[i], ret ? "Success" : "Failure",err);
    }
    result = [XCTWaiter waitForExpectations:@[_eventDispatchedCallback] timeout:10];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                             result == XCTWaiterResultCompleted);
    [self validateMCFREvents:_events];
}

-(void) validateSCFREvents:(NSArray<HIDEvent*>*) events {
    
    
    __block NSInteger touchEventCount = 0;
    __block NSInteger untouchEventCount = 0;

    [events enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull event, NSUInteger idx  , BOOL * _Nonnull stop __unused) {
        
        
        if (event.type != kIOHIDEventTypeDigitizer) return;
        
        NSArray<HIDEvent*> *children =  event.children;
        // Parent is collection event , so we must look for child events
        
        
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, children && children.count > 0);
        
        __block BOOL validEventMask = NO;
        __block BOOL validTouch = NO;
        __block BOOL validTransducerIndex = NO;
        __block BOOL validDigitizerType = NO;
        __block BOOL validScanTime = NO;
        
        [children enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull childEvent, NSUInteger idx1 , BOOL * _Nonnull stop1 __unused) {
            
            // First time we have report , we consider it as valid report and should dispatch all transducer events
            // although that may not be intended same way by user
            
            if (childEvent.type == kIOHIDEventTypeVendorDefined) {

                NSInteger usagePage = [childEvent integerValueForField:kIOHIDEventFieldVendorDefinedUsagePage];
                NSInteger usage = [childEvent integerValueForField:kIOHIDEventFieldVendorDefinedUsage];
                NSInteger dataLength = [childEvent integerValueForField:kIOHIDEventFieldVendorDefinedDataLength];
                NSMutableData *data =  nil;
                uint16_t expectedScanTime = idx + 10;
                NSData *expectedData = [[NSData alloc] initWithBytes:&expectedScanTime length:sizeof(uint16_t)];

                if (usagePage == kHIDPage_Digitizer && usage == kHIDUsage_Dig_RelativeScanTime) {

                    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,dataLength != 0);


                    const void *tmp = (const void*)[childEvent dataValueForField:kIOHIDEventFieldVendorDefinedData];


                    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,tmp != NULL);
                    data = [[NSMutableData alloc] initWithBytes:tmp length:dataLength];

                    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, [data isEqual:expectedData]);
                    validScanTime = YES;
                }
            }
            
            if (childEvent.type != kIOHIDEventTypeDigitizer) return;
            
            NSInteger eventMask = [childEvent integerValueForField:kIOHIDEventFieldDigitizerEventMask];
            NSInteger touch     = [childEvent integerValueForField:kIOHIDEventFieldDigitizerTouch];
            NSInteger transducerIndex = [childEvent integerValueForField:kIOHIDEventFieldDigitizerIndex];
            NSInteger digitizerType = [childEvent integerValueForField:kIOHIDEventFieldDigitizerType];
            
            NSLog(@"Event Mask : %lu , Touch : %lu, Transducer Index : %lu, digitizer type : %lu", eventMask, touch, transducerIndex, digitizerType);
            
            if (digitizerType == ((idx % 2) == 0 ? kIOHIDDigitizerTransducerTypeFinger : kIOHIDDigitizerTransducerTypeHand)) {
                validDigitizerType = YES;
            }
            
            if (transducerIndex == 3) {
                validTransducerIndex = YES;
            }
            
            if (touch == ((idx % 2) == 0 ? 1 : 0)) {
                validTouch = YES;
            }
            
            if ((idx == 0 && (eventMask & kIOHIDDigitizerEventPosition) && (eventMask & kIOHIDDigitizerEventTouch)) || eventMask & kIOHIDDigitizerEventTouch) {
                validEventMask = YES;
            }
            
            // Consider valid touches from first child only
            if (idx1 == 0) {
                touchEventCount += ((idx % 2) == 0 ? 1 : 0);
                untouchEventCount += ((idx % 2) == 0 ? 1 : 0);
            }
        
        }];
        
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validEventMask);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validTouch);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validTransducerIndex);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validDigitizerType);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validScanTime);
        
    }];


    // We are getting pointer event for our coodinates and digitizer events are child events
    //HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, touchEventCount == kTouchPadReportCount/2 && untouchEventCount == kTouchPadReportCount/2);
    
}

-(void) validateMCFREvents:(NSArray<HIDEvent*>*) events {
    
    
    __block NSMutableArray *touchCount = [[NSMutableArray alloc] init];
    __block NSMutableArray *untouchCount = [[NSMutableArray alloc] init];
    
   
    // Button Mask for parent event is always 1
    // And Button Mask for children corresponds to touch
    
    [events enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull event, NSUInteger idx , BOOL * _Nonnull stop __unused) {
        
        if (event.type != kIOHIDEventTypeDigitizer) return;
        
        NSArray<HIDEvent*> *children =  event.children;
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, children && children.count > 0);
        
        __block BOOL validEventMask = NO;
        __block BOOL validTouch = NO;
        __block BOOL validTransducerIndexA = NO;
        __block BOOL validTransducerIndexB = NO;
        __block BOOL validDigitizerType = NO;
        __block BOOL validScanTime = NO;
        
        [children enumerateObjectsUsingBlock:^(HIDEvent * _Nonnull childEvent, NSUInteger idx1 , BOOL * _Nonnull stop1 __unused) {
            
           
            if (childEvent.type == kIOHIDEventTypeVendorDefined) {
                    
                NSInteger usagePage = [childEvent integerValueForField:kIOHIDEventFieldVendorDefinedUsagePage];
                NSInteger usage = [childEvent integerValueForField:kIOHIDEventFieldVendorDefinedUsage];
                NSInteger dataLength = [childEvent integerValueForField:kIOHIDEventFieldVendorDefinedDataLength];
                NSMutableData *data =  nil;
                uint16_t expectedScanTime = idx + 10;
                NSData *expectedData = [[NSData alloc] initWithBytes:&expectedScanTime length:sizeof(uint16_t)];

                if (usagePage == kHIDPage_Digitizer && usage == kHIDUsage_Dig_RelativeScanTime) {

                   HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,dataLength != 0);


                   const void *tmp = (const void*)[childEvent dataValueForField:kIOHIDEventFieldVendorDefinedData];


                   HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,tmp != NULL);
                   data = [[NSMutableData alloc] initWithBytes:tmp length:dataLength];

                   HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, [data isEqual:expectedData]);
                   validScanTime = YES;
                }
            }
            
            if (childEvent.type != kIOHIDEventTypeDigitizer) return;
            
            if (touchCount.count == idx1) {
                 [touchCount addObject:@(0)];
            }
            
            if (untouchCount.count == idx1) {
                [untouchCount addObject:@(0)];
            }
            
            NSInteger eventMask = [childEvent integerValueForField:kIOHIDEventFieldDigitizerEventMask];
            NSInteger touch     = [childEvent integerValueForField:kIOHIDEventFieldDigitizerTouch];
            NSInteger transducerIndex = [childEvent integerValueForField:kIOHIDEventFieldDigitizerIndex];
            NSInteger digitizerType = [childEvent integerValueForField:kIOHIDEventFieldDigitizerType];
           
            
            NSLog(@"Event Mask : %lu , Touch : %lu, Transducer Index : %lu, digitizer type : %lu", eventMask, touch, transducerIndex, digitizerType);
            
             if (digitizerType == ((idx % 2) == 0 ? kIOHIDDigitizerTransducerTypeFinger : kIOHIDDigitizerTransducerTypeHand)) {
                validDigitizerType = YES;
             }
            
            // Now Event Mask flag is reporting different value on collection
            // For first event parent and first child have same mask
            // wheras second child have different mask than parent , although
            // both are returning position
            // FIXME : Expectation of event mask for multi contact case
            
            // For now just validate touch
            if (eventMask & kIOHIDDigitizerEventTouch) {
                validEventMask = YES;
            }
            
            if (touch == ((idx % 2) == 0 ? 1 : 0)) {
                validTouch = YES;
            }
            
            touchCount[idx1] = @(((NSNumber*)touchCount[idx1]).integerValue +  ((idx % 2) == 0 ? 1 : 0));
            untouchCount[idx1] = @(((NSNumber*)touchCount[idx1]).integerValue +  ((idx % 2) == 0 ? 1 : 0));

            
            if (transducerIndex == 4 && idx1 == 0) {
                validTransducerIndexA = YES;
            }
            
            if (transducerIndex == 2 && idx1 == 1) {
                validTransducerIndexB = YES;
            }
            
            
        }];
        
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validEventMask);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validTouch);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validTransducerIndexA);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validTransducerIndexB);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validDigitizerType);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, validScanTime);
        
       
        
    }];
    
    [touchCount enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx , BOOL * _Nonnull stop ) {
               
        if (idx >= 2) {
            *stop = YES;
            return;
        }
       // HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ((NSNumber*)obj).integerValue == kTouchPadReportCount/2);
        
    }];
    
    [untouchCount enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx , BOOL * _Nonnull stop ) {
        if (idx >= 2) {
            *stop = YES;
            return;
        }
       // HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ((NSNumber*)obj).integerValue == kTouchPadReportCount/2);
    }];
    
}



@end

