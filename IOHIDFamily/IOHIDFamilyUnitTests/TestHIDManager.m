//
//  TestHIDManager.m
//  IOHIDFamily
//
//  Created by YG on 04/06/17.
//
//

#import <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDDevice.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDUnitTestDescriptors.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"


static void HIDManagerValueCallback (
                                    void * _Nullable        context,
                                    IOReturn                result,
                                    void * _Nullable        sender,
                                    IOHIDValueRef           value);

static void HIDManagerReportCallback (
                                     void * _Nullable        context,
                                     IOReturn                result,
                                     void * _Nullable        sender,
                                     IOHIDReportType         type,
                                     uint32_t                reportID,
                                     uint8_t *               report,
                                     CFIndex                 reportLength);

void HIDManagerDeviceCallback (void * _Nullable         context,
                               IOReturn                 result,
                               void * _Nullable         sender,
                               IOHIDDeviceRef           device);

static uint8_t descriptorForVendor[] = {
    HIDVendorMessage32BitDescriptor
};

static uint8_t descriptorForKeyboard[] = {
    HIDKeyboardDescriptor
};

static uint8_t descriptorForDigitizer[] = {
    HIDDigitizerWithTouchCancel
};

@interface TestHIDManager : XCTestCase

@property   IOHIDManagerRef          deviceManager;
@property   NSString *               keyboardID;
@property   NSString *               vendorID;
@property   NSString *               digitizerID;
@property   NSMutableDictionary *    deviceDict;
@property   dispatch_queue_t         deviceControllerQueue;
@property   dispatch_queue_t         rootQueue;
@property   CFRunLoopRef             runLoop;
@property   NSInteger                valueCount;
@property   NSInteger                reportCount;
@property   XCTestExpectation *      testDeviceExpectation;

@end

@implementation TestHIDManager

- (void)setUp {
    [super setUp];
    
    TestLog("TestHIDManager setup");
	IOHIDUserDeviceTestController * device;
    NSData * descriptorData;

    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    self.deviceControllerQueue = dispatch_queue_create_with_target ("IOHIDDeviceTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.deviceControllerQueue != nil);
    
    self.deviceDict = [[NSMutableDictionary alloc] init];
    HIDXCTAssertAndThrowTrue ( self.deviceDict != nil);

    self.testDeviceExpectation = [[XCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test devices"]];

    self.deviceManager = IOHIDManagerCreate (kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    self.testDeviceExpectation.expectedFulfillmentCount = 3;

    self.keyboardID = [[[NSUUID alloc] init] UUIDString];
    self.vendorID = [[[NSUUID alloc] init] UUIDString];
    self.digitizerID = [[[NSUUID alloc] init] UUIDString];

    NSArray * matchingMultiple = @[
                                   @{@kIOHIDPhysicalDeviceUniqueIDKey:self.digitizerID} ,
                                   @{@kIOHIDPhysicalDeviceUniqueIDKey:self.vendorID} ,
                                   @{@kIOHIDPhysicalDeviceUniqueIDKey:self.keyboardID}
                                   ];

    IOHIDManagerSetDeviceMatchingMultiple(self.deviceManager, (CFArrayRef)matchingMultiple);

    IOHIDManagerRegisterDeviceMatchingCallback(self.deviceManager, HIDManagerDeviceCallback, (__bridge void * _Nullable) self);
    
    IOHIDManagerRegisterInputValueCallback (self.deviceManager, HIDManagerValueCallback, (__bridge void * _Nullable)(self));
    IOHIDManagerRegisterInputReportCallback (self.deviceManager,  HIDManagerReportCallback, (__bridge void * _Nullable)(self));
    IOHIDManagerScheduleWithRunLoop(self.deviceManager, CFRunLoopGetCurrent() , kCFRunLoopDefaultMode);
    IOHIDManagerOpen(self.deviceManager, kIOHIDOptionsTypeNone);
    
    descriptorData = [[NSData alloc] initWithBytes:descriptorForKeyboard length:sizeof(descriptorForKeyboard)];
    device = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:self.keyboardID andQueue:nil];
    HIDXCTAssertAndThrowTrue(device != nil);
    self.deviceDict [self.keyboardID] = device;
    
    descriptorData = [[NSData alloc] initWithBytes:descriptorForVendor length:sizeof(descriptorForVendor)];
    device = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:self.vendorID andQueue:nil];
    HIDXCTAssertAndThrowTrue(device != nil);
    self.deviceDict [self.vendorID] = device;
    
    descriptorData = [[NSData alloc] initWithBytes:descriptorForDigitizer length:sizeof(descriptorForDigitizer)];
    device = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:self.digitizerID andQueue:nil];
    HIDXCTAssertAndThrowTrue(device != nil);
    self.deviceDict [self.digitizerID] = device;
    
}

- (void)tearDown {
    TestLog("TestHIDManager tearDown");

    for(id key in self.deviceDict) {
        id value = [self.deviceDict objectForKey:key];
        [value invalidate];
    }
    
    self.deviceDict = nil;
    
    IOHIDManagerUnscheduleFromRunLoop (self.deviceManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(self.deviceManager, 0);
    CFRelease(self.deviceManager);

    [super tearDown];
}

- (void)testMatching {
    NSSet * devices;
    NSDictionary *matching;
    
    XCTWaiterResult result;
    result = [XCTWaiter waitForExpectations:@[self.testDeviceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testDeviceExpectation);

    NSMutableArray *matchingMultiple = [[NSMutableArray alloc ] initWithCapacity:3];
    HIDXCTAssertAndThrowTrue (matchingMultiple != nil);
    
    matching = @{@kIOHIDPhysicalDeviceUniqueIDKey:self.keyboardID};
    IOHIDManagerSetDeviceMatching(self.deviceManager, (CFDictionaryRef)matching);
    
    devices = CFBridgingRelease(IOHIDManagerCopyDevices (self.deviceManager));
    XCTAssert(devices != nil && devices.count == 1, "devices: %@", devices);
    
    [matchingMultiple addObject: matching];
    matching = @{@kIOHIDPhysicalDeviceUniqueIDKey:self.vendorID};
    [matchingMultiple addObject: matching];
    
    IOHIDManagerSetDeviceMatchingMultiple(self.deviceManager, (CFArrayRef)matchingMultiple);
    
    devices = CFBridgingRelease(IOHIDManagerCopyDevices (self.deviceManager));
    XCTAssert(devices != nil && devices.count == 2, "devices: %@", devices);
    
    matching = @{@kIOHIDPhysicalDeviceUniqueIDKey:self.digitizerID};
    [matchingMultiple addObject: matching];
    
    IOHIDManagerSetDeviceMatchingMultiple(self.deviceManager, (CFArrayRef)matchingMultiple);
    
    devices = CFBridgingRelease(IOHIDManagerCopyDevices (self.deviceManager));
    XCTAssert(devices != nil && devices.count == 3, "devices: %@", devices);

    matching = @{@kIOHIDPhysicalDeviceUniqueIDKey:self.keyboardID};
    IOHIDManagerSetDeviceMatching(self.deviceManager, (CFDictionaryRef)matching);
 
    devices = CFBridgingRelease(IOHIDManagerCopyDevices (self.deviceManager));
    XCTAssert(devices != nil && devices.count == 1, "devices: %@", devices);
}

- (void)testProperty {
    
    NSSet * devices;

    XCTWaiterResult result;
    result = [XCTWaiter waitForExpectations:@[self.testDeviceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testDeviceExpectation);

    devices = CFBridgingRelease(IOHIDManagerCopyDevices (self.deviceManager));
    
    // 35266200
    HIDXCTAssertWithLogs(devices != nil && devices.count == 3, "Devices: %@", devices);
    
    IOHIDManagerSetProperty(self.deviceManager, CFSTR("TestKey"), CFSTR("TestValue"));
    
    for (id device in devices) {
        CFTypeRef value = IOHIDDeviceGetProperty ((IOHIDDeviceRef)device, CFSTR("TestKey"));
        XCTAssert(IOHIDDeviceGetTypeID() == CFGetTypeID((IOHIDDeviceRef)device));
        XCTAssert (CFEqual (value, CFSTR("TestValue")) == true);
    }

}

- (void)MAC_OS_ONLY_TEST_CASE(testReports) {
    
    __block IOReturn status;

    self.valueCount     = 0;
    self.reportCount    = 0;
    
    XCTWaiterResult result;
    result = [XCTWaiter waitForExpectations:@[self.testDeviceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testDeviceExpectation);

    NSArray * elementMatching = @[
                                    @{
                                        @kIOHIDElementUsagePageKey:@(kHIDPage_AppleVendor),
                                        @kIOHIDElementUsageKey:@(kHIDUsage_AppleVendor_Message) ,
                                    },
                                    @{
                                        @kIOHIDElementUsagePageKey:@(kHIDPage_Digitizer),
                                        @kIOHIDElementUsageKey:@(kHIDUsage_Dig_Touch)
                                    }
                                ];
    
    IOHIDManagerSetInputValueMatchingMultiple (self.deviceManager, (CFArrayRef)elementMatching);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), self.deviceControllerQueue, ^{
        
        HIDDigitizerWithTouchCancelInputReport digitizerReport;
        memset (&digitizerReport, 00, sizeof(digitizerReport));
        
        digitizerReport.DIG_TouchPadFingerTouch = 1;
        status = [self.deviceDict[self.digitizerID] handleReport:(uint8_t*)&digitizerReport Length:sizeof(digitizerReport) andInterval:2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport result:0x%x\n", status);
   
        digitizerReport.DIG_TouchPadFingerTouch = 0;
        status = [self.deviceDict[self.digitizerID] handleReport:(uint8_t*)&digitizerReport Length:sizeof(digitizerReport) andInterval:2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport result:0x%x\n", status);

        digitizerReport.DIG_TouchPadFingerUntouch = 1;
        status = [self.deviceDict[self.digitizerID] handleReport:(uint8_t*)&digitizerReport Length:sizeof(digitizerReport) andInterval:2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport result:0x%x\n", status);

        digitizerReport.DIG_TouchPadFingerUntouch = 0;
        status = [self.deviceDict[self.digitizerID] handleReport:(uint8_t*)&digitizerReport Length:sizeof(digitizerReport) andInterval:2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport result:0x%x\n", status);

        HIDVendorMessage32BitDescriptorInputReport vendorReport;
        vendorReport.VEN_VendorDefined0023 = 1;
        status = [self.deviceDict[self.vendorID] handleReport:(uint8_t*)&vendorReport Length:sizeof(vendorReport) andInterval:2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport result:0x%x\n", status);

        vendorReport.VEN_VendorDefined0023 = 2;
        status = [self.deviceDict[self.vendorID] handleReport:(uint8_t*)&vendorReport Length:sizeof(vendorReport) andInterval:2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport result:0x%x\n", status);
    });

    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 5.0, false);
    
    XCTAssert(self.reportCount == 6 && self.valueCount == 4, "reportCount:%x && valueCount:%x", (int)self.reportCount, (int)self.valueCount);
    
}

-(void) managerValueCallback: (IOHIDDeviceRef) __unused sender :(IOHIDValueRef) __unused value :(IOReturn) result {
    ++self.valueCount;
    XCTAssert (result == kIOReturnSuccess, "managerValueCallback result: %x", result);
}

-(void) managerReportCallback: (IOHIDDeviceRef) __unused sender :(IOHIDReportType) __unused type :(uint32_t) __unused reportID : (uint8_t*) __unused report : (CFIndex) __unused reportLength : (IOReturn) __unused result {
    ++self.reportCount;
}

@end

void HIDManagerValueCallback (void * _Nullable  context, IOReturn result, void * _Nullable  sender __unused, IOHIDValueRef value) {
    TestHIDManager *self = (__bridge TestHIDManager *)context;
    TestLog("HIDManagerValueCallback sender:%@ result:0x%x\n", sender, result);
    [self managerValueCallback: (IOHIDDeviceRef) sender :value :result];
}

void HIDManagerReportCallback (void * _Nullable        context,
                               IOReturn                result,
                               void * _Nullable        sender,
                               IOHIDReportType         type,
                               uint32_t                reportID,
                               uint8_t *               report,
                               CFIndex                 reportLength) {
    TestHIDManager *self = (__bridge TestHIDManager *)context;
    TestLog("HIDManagerReportCallback sender:%@ result:0x%x\n", sender, result);
    [self managerReportCallback: (IOHIDDeviceRef) sender :type :reportID :report :reportLength :result];
}

void HIDManagerDeviceCallback (void * _Nullable         context,
                                IOReturn                result __unused,
                                void * _Nullable        sender __unused,
                                IOHIDDeviceRef          device __unused)
{
    TestHIDManager * self = (__bridge TestHIDManager *)context;
    TestLog("HIDManagerDeviceCallback device:%@ result:0x%x\n", device, result);
    [self.testDeviceExpectation fulfill];
}

    
                                             
