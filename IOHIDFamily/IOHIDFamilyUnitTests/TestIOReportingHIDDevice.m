//
//  TestIOReportingHIDDevice.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 4/22/19.
//

#import <XCTest/XCTest.h>
#import <HIDDisplay/HIDDisplay.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDUserDevice.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/usb/USBSpec.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <TargetConditionals.h>


static uint8_t descriptor [] = {
    HIDDisplayCompositeDescriptor
};

static IOReportingInputReport07 inputReportCommand1;
static IOReportingInputReport07 inputReportCommand2;

char *testData1 = "Test Data Command 1";
char *testData2 = "Test Data Command 2";

@interface TestIOReportingHIDDevice : XCTestCase
{
    HIDDisplayIOReportingInterfaceRef  _hidDisplayInterface;
    dispatch_queue_t     _queue;
    NSString             *_containerID;
}
@property XCTestExpectation  *testSetReportExpectation;
@property XCTestExpectation  *testGetReportExpectation;
@property XCTestExpectation  *testReportMatchExpectation;
@property XCTestExpectation  *cancelHandlerExpectation;
@property IOHIDUserDeviceRef userDevice;

@end

static void _initIOReporting(void)
{
    bzero(&inputReportCommand1, sizeof(IOReportingInputReport07));
    bzero(&inputReportCommand2, sizeof(IOReportingInputReport07));
    
    inputReportCommand1.reportId = 0x07;
    inputReportCommand2.reportId = 0x07;
    
    snprintf((char*)inputReportCommand1.VEN_VendorDefined0001, 250, "%s",testData1);
    snprintf((char*)inputReportCommand2.VEN_VendorDefined0001, 250, "%s",testData2);
    
}

static IOReturn __setReportCallback(void * _Nullable refcon, IOHIDReportType __unused type, uint32_t __unused reportID, uint8_t * report __unused, CFIndex reportLength) {
    
    if ((unsigned long)reportLength < sizeof(uint32_t) + 1) {
        return kIOReturnError;
    }
    
    uint32_t testCommand = *((uint32_t*)(report+1));
    
    NSLog(@"Set report Callback testCommand %d",testCommand);
    
    TestIOReportingHIDDevice *me = (__bridge TestIOReportingHIDDevice*)refcon;
    
    switch (testCommand) {
        case 1:
            IOHIDUserDeviceHandleReport(me.userDevice, (uint8_t*)&inputReportCommand1, sizeof(IOReportingInputReport07));
            break;
        case 2:
            IOHIDUserDeviceHandleReport(me.userDevice, (uint8_t*)&inputReportCommand2, sizeof(IOReportingInputReport07));
            break;
        default:
            break;
    }
    
    [me.testSetReportExpectation fulfill];
    
    return kIOReturnSuccess;
}

@implementation TestIOReportingHIDDevice

- (void)setUp {
    
    [super setUp];
    
    _containerID = [[[NSUUID alloc] init] UUIDString];
    
    NSMutableDictionary* deviceConfig = [[NSMutableDictionary alloc] init];
    
    NSData *descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    
#if !TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
    deviceConfig [@kUSBDeviceContainerID] = _containerID;
#else
    deviceConfig [@kUSBContainerID] = _containerID;
#endif
    
    _userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)deviceConfig);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, _userDevice != NULL);
    
    IOHIDUserDeviceRegisterSetReportCallback(_userDevice, __setReportCallback, (__bridge void*)self);
    
    _queue = dispatch_queue_create("com.apple.user-device-test", NULL);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, _queue != NULL);
    
    IOHIDUserDeviceScheduleWithDispatchQueue (_userDevice, _queue);
    
    _testSetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: set reports"];
    _testGetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: get reports"];
    _testReportMatchExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: match reports"];
    _cancelHandlerExpectation = [[XCTestExpectation alloc] initWithDescription:@"Cancel Handler Expectation"];
    
}

- (void)tearDown {
    
    if (_userDevice) {
        IOHIDUserDeviceUnscheduleFromDispatchQueue (_userDevice, _queue);
        CFRelease(_userDevice);
    }
    
}

- (void)testIOReportingHIDDevice {
    
    
    _initIOReporting();
    @autoreleasepool {
        
        _hidDisplayInterface = HIDDisplayCreateIOReportingInterfaceWithContainerID((__bridge CFStringRef)_containerID);
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, _hidDisplayInterface != NULL);
        
        uint32_t testCommand = 1;
        NSData *outputData = [[NSData alloc] initWithBytes:&testCommand length:sizeof(uint32_t)];
        
        
        
        HIDDisplayIOReportingSetInputDataHandler(_hidDisplayInterface, ^(CFDataRef  _Nonnull inputData) {
            
            NSString *dataString = [NSString stringWithUTF8String:((__bridge NSData*)inputData).bytes];
            
            NSLog(@"%@",dataString);
            
            if ([[NSString stringWithFormat:@"%s",testData1] isEqualToString:dataString]) {
                [_testReportMatchExpectation fulfill];
            } else {
                NSLog(@"No Match");
            }
            
            [_testGetReportExpectation fulfill];
            
        });
        
        
        HIDDisplayIOReportingSetDispatchQueue(_hidDisplayInterface, dispatch_queue_create("com.apple.hidtest-inputreport", NULL));
        
        HIDDisplayIOReportingSetCancelHandler(_hidDisplayInterface, ^{
            [_cancelHandlerExpectation fulfill];
        });
        
        HIDDisplayIOReportingActivate(_hidDisplayInterface);
        
        HIDDisplayIOReportingSetOutputData(_hidDisplayInterface, (__bridge CFDataRef)outputData, NULL);
        
        XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_testSetReportExpectation, _testGetReportExpectation, _testReportMatchExpectation] timeout:10];
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG,
                                    result == XCTWaiterResultCompleted,
                                    "result:%ld %@ %@ %@",
                                    (long)result,
                                    _testSetReportExpectation,_testGetReportExpectation, _testReportMatchExpectation);
        
        
        HIDDisplayIOReportingCancel(_hidDisplayInterface);
        
        result = [XCTWaiter waitForExpectations:@[_cancelHandlerExpectation] timeout:10];
        
        if (_hidDisplayInterface) {
            
            CFRelease(_hidDisplayInterface);
        }
        
        
    }
    
}

@end
