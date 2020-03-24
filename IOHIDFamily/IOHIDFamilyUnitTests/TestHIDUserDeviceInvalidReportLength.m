//
//  TestHIDUserDeviceInvalidReportLength.m
//  IOHIDFamilyUnitTests
//
//  Created by abhishek on 12/4/19.
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import <HID/HID.h>
#import <AssertMacros.h>
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventData.h>
#import <IOKit/hid/IOHIDUserDevice.h>

/** Test Objective : Test panic like 56970180
*/

static XCTestExpectation *_devAddedExp;
static uint32_t returnReportLength = 0;

static uint8_t descriptor[] = {
    HIDCameraDescriptor
};



@interface TestHIDUserDeviceInvalidReportLength : XCTestCase {
    IOHIDUserDeviceRef      _userDevice;
    NSString                *_uuid;
    HIDManager              *_manager;
    HIDDevice               *_device;
}

@end

static IOReturn __getReportCallback(void * _Nullable refcon __unused, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex * pReportLength) {
    NSLog(@"Get Report reportID %u Report Type %d report Length %ld",reportID, (int)type, *pReportLength);
    uint8_t reportData[] = {0x00,0x12};
    memcpy(report, reportData, 2);
    
    // Modify report length
    *pReportLength = returnReportLength;
    
    return kIOReturnSuccess;
}

@implementation TestHIDUserDeviceInvalidReportLength

-(void) createUserDevice {
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (__bridge CFDictionaryRef)properties);
    

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _userDevice != NULL);
    
    IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback(_userDevice, __getReportCallback, NULL);
    IOHIDUserDeviceSetDispatchQueue(_userDevice, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    IOHIDUserDeviceSetCancelHandler(_userDevice, ^{
        
    });
    
    IOHIDUserDeviceActivate(_userDevice);
    
}

-(void) createManager {
    
     __weak TestHIDUserDeviceInvalidReportLength *weakSelf = self;
    
    _manager = [[HIDManager alloc] initWithOptions:0];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _manager != NULL);
    
    [_manager setDeviceMatching:@{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid }];
    [_manager setDispatchQueue:dispatch_get_main_queue()];
    [_manager setCancelHandler:^{
        
    }];
    [_manager setDeviceNotificationHandler:^(HIDDevice * _Nonnull device, BOOL added) {
        
        __strong TestHIDUserDeviceInvalidReportLength *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        if (added) {
            strongSelf->_device = device;
            NSLog(@"%@",device);
            [_devAddedExp fulfill];
        }
        
        
    }];
    
    [_manager activate];
    
}

- (void)setUp {
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    [self createManager];
    [self createUserDevice];
}

- (void)tearDown {
    if (_manager) {
        [_manager cancel];
    }
    
    if (_userDevice) {
        IOHIDUserDeviceCancel(_userDevice);
        CFRelease(_userDevice);
    }
    
    
}

- (void)testHIDUserDeviceInvalidReportLength {
    XCTWaiterResult result;
    NSError *err = nil;
    BOOL ret = NO;
    NSMutableData *reportData = [[NSMutableData alloc] initWithLength:sizeof(HIDCameraDescriptorInputReport)+sizeof(uint8_t)];
    NSInteger reportLength = reportData.length;
    
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
     HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                _device != NULL);
    
    [_device open];
    returnReportLength = 10;
    
    ret = [_device getReport:[reportData mutableBytes] reportLength:&reportLength withIdentifier:0 forType:HIDReportTypeInput error:&err];
    if (ret == NO) NSLog(@"Get Report Error %@",err);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
    ret == NO);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, err && err.code == kIOReturnOverrun);
    
    returnReportLength = sizeof(HIDCameraDescriptorInputReport)+sizeof(uint8_t);
    
    ret = [_device getReport:[reportData mutableBytes] reportLength:&reportLength withIdentifier:0 forType:HIDReportTypeInput error:&err];
    if (ret == NO) NSLog(@"Get Report Error %@",err);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
    ret == YES);
    
    NSLog(@"Data %@ Length %lu",reportData, reportLength);
    
    [_device close];
}


@end
