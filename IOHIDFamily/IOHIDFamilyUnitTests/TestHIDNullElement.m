//
//  TestHIDNullElement.m
//  IOHIDFamilyUnitTests
//
//  Created by jkergan on 9/16/20.
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "HID.h"
#import "IOHIDHIDDeviceElementsDescriptor.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hid/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/IOHIDTransaction.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDServiceKeys.h>


static uint8_t descriptor[] = {
    HIDDeviceElements
};

@interface TestHIDNullElement : XCTestCase

@property IOHIDUserDeviceRef            userDevice;
@property IOHIDDeviceRef                device;
@property XCTestExpectation           * testDeviceCancelExpectation;
@property NSInteger                     setReportCount;
@property NSInteger                     getReportCount;

@end

@implementation TestHIDNullElement

static IOReturn handleSetReport(void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * __unused report, CFIndex __unused reportLength) {
	IOReturn status = kIOReturnSuccess;
    TestHIDNullElement * self = (__bridge TestHIDNullElement*)refcon;
    NSLog(@"setSetReportHandler: type:%d reportID:%d",(int)type, (int)reportID);
    self.setReportCount++;
    return  status;
}

static IOReturn handleGetReport(void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * __unused report, CFIndex __unused reportLength) {
	IOReturn status = kIOReturnSuccess;
    TestHIDNullElement * self = (__bridge TestHIDNullElement*)refcon;
    NSLog(@"setGetReportHandler: type:%d reportID:%d",(int)type, (int)reportID);
    self.getReportCount++;
    return  status;
}


- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    [super setUp];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   };

    
    self.testDeviceCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: device cancel"];
    self.testDeviceCancelExpectation.expectedFulfillmentCount = 1;

    __weak TestHIDNullElement * self_ = self;
    
    self.userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (__bridge CFDictionaryRef)description);
    if (!self.userDevice) {
        return;
    }
    
    IOHIDUserDeviceRegisterSetReportCallback(self.userDevice, &handleSetReport, (__bridge void * _Nullable)(self));

    IOHIDUserDeviceRegisterGetReportCallback(self.userDevice, &handleGetReport, (__bridge void * _Nullable)(self));

    IOHIDUserDeviceScheduleWithDispatchQueue(self.userDevice, dispatch_get_main_queue());
    
    mach_timespec_t waitTime = {30, 0};
    io_service_t userDeviceService = IOHIDUserDeviceCopyService(self.userDevice);
    kern_return_t kr = IOServiceWaitQuiet (userDeviceService, &waitTime);
    if (kr) {
        NSLog(@"IOServiceWaitQuiet:0x%x", kr);
        IOObjectRelease(userDeviceService);
        return;
    }

    self.device = IOHIDDeviceCreate(kCFAllocatorDefault, userDeviceService);
    if (!self.device) {
        return;
    }
 

     IOHIDDeviceSetCancelHandler(self.device, ^{
        [self_.testDeviceCancelExpectation fulfill];
        CFRelease(self_.device);
    });

    IOHIDDeviceSetDispatchQueue(self.device, dispatch_get_main_queue());

    IOHIDDeviceActivate(self.device);
    IOHIDDeviceOpen(self.device, 0);
    IOObjectRelease(userDeviceService);
}

- (void)tearDown {
    IOHIDUserDeviceUnscheduleFromDispatchQueue(self.userDevice, dispatch_get_main_queue());

    IOHIDDeviceClose(self.device, 0);
    IOHIDDeviceCancel(self.device);
        
    [super tearDown];
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

- (void) testNullElementTransaction {
    NSDictionary *matching = @{@kIOHIDElementReportIDKey: @(1)};

    CFArrayRef arrayElements  =  IOHIDDeviceCopyMatchingElements(self.device, (__bridge CFDictionaryRef)matching, 0);

    IOHIDTransactionRef transaction = IOHIDTransactionCreate(kCFAllocatorDefault, self.device, kIOHIDTransactionDirectionTypeOutput, kIOHIDTransactionOptionsNone);

    for (id elementVar in (__bridge NSArray*)arrayElements) {
        IOHIDElementRef element = (__bridge IOHIDElementRef)elementVar;
        IOHIDValueRef value = NULL;
        IOHIDTransactionAddElement(transaction, element);
        IOHIDTransactionSetValue(transaction, element, value, 0);
    }
    // Test is for crash. Return code likely gives error.
    IOReturn kret = IOHIDTransactionCommit(transaction);
    NSLog(@"IOHIDTransactionCommit: %#x", kret);

    IOHIDTransactionClear(transaction);
    CFRelease(arrayElements);
    CFRelease(transaction);
}

#pragma clang diagnostic pop
@end
