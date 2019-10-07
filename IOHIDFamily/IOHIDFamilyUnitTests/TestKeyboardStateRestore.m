//
//  TestKeyboardStateRestore.m
//  IOHIDFamilyUnitTests
//
//  Created by Matty on 3/30/18.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import "HID.h"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

static NSInteger _setReportCount;

static HIDUserDevice *setupDevice()
{
    HIDUserDevice *device = nil;
    NSMutableDictionary *deviceConfig = [[NSMutableDictionary alloc] init];
    NSData *desc = [[NSData alloc] initWithBytes:descriptor
                                          length:sizeof(descriptor)];
    
    deviceConfig[@kIOHIDReportDescriptorKey] = desc;
    
    // required keys to restore kbd state
    deviceConfig[@kIOHIDKeyboardRestoreStateKey] = @YES;
    deviceConfig[@kIOHIDLocationIDKey] = @12345678;
    
    device = [[HIDUserDevice alloc] initWithProperties:deviceConfig];
    assert(device);
    
    [device setSetReportHandler:^IOReturn(HIDReportType type,
                                          NSInteger reportID,
                                          const void *report,
                                          NSInteger reportLength __unused) {
        HIDKeyboardDescriptorOutputReport *outReport;
        
        assert(type == HIDReportTypeOutput);
        
        outReport = (HIDKeyboardDescriptorOutputReport *)report;
        assert(outReport->LED_KeyboardCapsLock == 1);
        
        NSLog(@"HIDUserDevice setReport type: %lu reportID: %ld",
              (unsigned long)type, (long)reportID);
        
        _setReportCount++;
        
        return kIOReturnSuccess;
    }];
    
    [device setDispatchQueue:dispatch_get_main_queue()];
    
    [device activate];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    return device;
}

@interface TestKeyboardStateRestore : XCTestCase
@property HIDUserDevice *userDevice;

@end

@implementation TestKeyboardStateRestore

- (void)setUp
{
    [super setUp];
    
    _setReportCount = 0;
    
    _userDevice = setupDevice();
    HIDXCTAssertAndThrowTrue(_userDevice);
}

- (void)tearDown
{
    [_userDevice cancel];
    [super tearDown];
}

- (void)MAC_OS_ONLY_TEST_CASE(testKeyboardStateRestore)
{
    HIDKeyboardDescriptorInputReport kbdReport = { 0 };
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&kbdReport
                                                      length:sizeof(kbdReport)
                                                freeWhenDone:NO];
    
    // set capslock on
    kbdReport.KB_Keyboard[0] = kHIDUsage_KeyboardCapsLock;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    kbdReport.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    XCTAssert(_setReportCount == 1, "_setReportCount:%ld", _setReportCount);
    
    // terminate
    [_userDevice cancel];
    _userDevice = nil;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    // setup new device
    _userDevice = setupDevice();
    HIDXCTAssertAndThrowTrue(_userDevice);
    
    // verify caps lock on
    XCTAssert(_setReportCount == 2, "_setReportCount:%ld", _setReportCount);
}

@end
