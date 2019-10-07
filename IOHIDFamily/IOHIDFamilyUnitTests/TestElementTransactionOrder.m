//
//  TestElementTransactionOrder.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 4/18/19.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <AssertMacros.h>
#import <os/log.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/IOHIDManager.h>

AppleVendorDisplayFeatureReport01 rawDataReport1;
AppleVendorDisplayFeatureReport02 rawDataReport2;
AppleVendorDisplayFeatureReport03 rawDataReport3;


static uint8_t descriptor [] = {
    HIDDisplayCompositeDescriptor
};

@interface TestElementTransactionOrder : XCTestCase
@property NSString *uniqueID;
@property IOHIDManagerRef  deviceManager;
@property IOHIDUserDeviceRef userDevice;
@property XCTestExpectation  *deviceExpectation;
@property IOHIDDeviceRef device;
@property dispatch_queue_t queue;
@property NSArray *elements;
@end


static IOReturn __getReportCallbackLength(void * _Nullable refcon __unused, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex * pReportLength)
{
    *pReportLength = 5;
    
    NSLog(@"__get report ID %d type %d",reportID, type);
    
    switch (reportID) {
        case 3:
            bcopy(&rawDataReport1, report, sizeof(AppleVendorDisplayFeatureReport01));
            *pReportLength = sizeof(AppleVendorDisplayFeatureReport01);
            break;
        case 4:
            bcopy(&rawDataReport2, report, sizeof(AppleVendorDisplayFeatureReport02));
            *pReportLength = sizeof(AppleVendorDisplayFeatureReport02);
            break;
        case 5:
            bcopy(&rawDataReport3, report, sizeof(AppleVendorDisplayFeatureReport03));
            *pReportLength = sizeof(AppleVendorDisplayFeatureReport03);
            break;
        default:
            break;
    }
    
    return kIOReturnSuccess;
}

static void HIDManagerDeviceAddedCallback(void * _Nullable context, IOReturn  result __unused, void * _Nullable  sender __unused, IOHIDDeviceRef device) {
    
    
    TestElementTransactionOrder *me = (__bridge TestElementTransactionOrder*)context;
    
    me.device = device;
    
    IOHIDDeviceOpen(me.device, 0);
    
    [me.deviceExpectation fulfill];
    
}

@implementation TestElementTransactionOrder
{
}
-(void) checkElements:(NSArray*) elements
{
    
    IOHIDTransactionRef  transaction = IOHIDTransactionCreate(kCFAllocatorDefault, self.device, kIOHIDTransactionDirectionTypeOutput, kIOHIDOptionsTypeNone);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, transaction != NULL);
    
    IOHIDTransactionClear(transaction);
    
    IOHIDTransactionSetDirection(transaction, kIOHIDTransactionDirectionTypeInput);
    
    for (id element in elements) {
        
        IOHIDTransactionAddElement(transaction,(__bridge IOHIDElementRef)element);
    }
    
    IOReturn kr = IOHIDTransactionCommit(transaction);
    NSLog(@"IOHIDTransactionCommit return 0x%x",kr);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, kr == kIOReturnSuccess);
    
    for (id element in elements) {
        IOHIDValueRef value = IOHIDTransactionGetValue(transaction,(__bridge IOHIDElementRef)element, kIOHIDOptionsTypeNone);
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, value != NULL);
        
        uint32_t usage = IOHIDElementGetUsage((__bridge IOHIDElementRef)element);
        NSInteger length = (NSInteger)IOHIDValueGetLength(value);
        
        NSLog(@"Usage : %u Length : %ld", usage, length);
        
        
        switch (usage) {
            case 0xa:
                HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, length == 2);
                break;
            case 0xb:
                HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, length == 64);
                break;
            case 0xc:
                HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, length == 2);
                break;
            case 0x3:
                HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, length == 4);
                break;
            default:
                break;
        }
        
        
    }
    
    IOHIDTransactionClear(transaction);
    
    CFRelease(transaction);
}
- (void) setUp {
    
    
    memset(&rawDataReport1, 0, sizeof(AppleVendorDisplayFeatureReport01));
    memset(&rawDataReport2, 0, sizeof(AppleVendorDisplayFeatureReport02));
    memset(&rawDataReport3, 0, sizeof(AppleVendorDisplayFeatureReport03));
    
    rawDataReport3.reportId = 0x05;
    rawDataReport2.reportId = 0x04;
    rawDataReport1.reportId = 0x03;
    
    self.uniqueID = [[[NSUUID alloc] init] UUIDString];
    self.deviceManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, self.deviceManager != nil);
    
    
    NSDictionary * matching = @{@kIOHIDPhysicalDeviceUniqueIDKey:self.uniqueID};
    IOHIDManagerSetDeviceMatching(self.deviceManager, (CFDictionaryRef)matching);
    IOHIDManagerRegisterDeviceMatchingCallback(self.deviceManager, HIDManagerDeviceAddedCallback, (__bridge void *)(self));
    IOHIDManagerScheduleWithRunLoop(self.deviceManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    
    self.deviceExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: device match"];
    
    NSMutableDictionary* deviceConfig = [[NSMutableDictionary alloc] init];
    
    NSData *descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    deviceConfig [@kIOHIDPhysicalDeviceUniqueIDKey] = self.uniqueID;
    
    
    self.userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)deviceConfig);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, self.userDevice != NULL);
    
    IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback(self.userDevice, __getReportCallbackLength, NULL);
    
    self.queue = dispatch_queue_create("com.apple.user-device-test", NULL);
    
    IOHIDUserDeviceScheduleWithDispatchQueue (self.userDevice, self.queue);
    
}

- (void) tearDown {
    
    
    if (self.device) {
        IOHIDDeviceClose(self.device, 0);
    }
    
    if (self.deviceManager) {
        CFRelease(self.deviceManager);
    }
    
    if (self.userDevice) {
        IOHIDUserDeviceUnscheduleFromDispatchQueue (self.userDevice, _queue);
        CFRelease(self.userDevice);
    }
}

-(id) getElementForUsages:(uint32_t) usage {
    
    id ret = nil;
    for (id element in self.elements) {
        uint32_t elementUsage = IOHIDElementGetUsage((__bridge IOHIDElementRef)element);
        if (elementUsage == usage) {
            ret = element;
            break;
        }
    }
    return ret;
}

- (void) testElementTransactionOrder {
    
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[self.deviceExpectation] timeout:10];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "%@",
                                self.deviceExpectation);
    
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayPreset)};
    
    self.elements = (__bridge_transfer NSArray*)IOHIDDeviceCopyMatchingElements(self.device, (__bridge CFDictionaryRef)matching, 0);
    
    
    id elementA = [self getElementForUsages:0xa];
    id elementB  = [self getElementForUsages:0xb];
    id elementC = [self getElementForUsages:0xc];
    id elementD = [self getElementForUsages:0x3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, elementA != nil && elementB != nil && elementC != nil && elementD != nil);
    
    
    [self checkElements:@[elementA, elementB, elementC, elementD]];
    [self checkElements:@[elementD, elementC, elementB, elementA]];
    [self checkElements:@[elementB, elementD, elementC, elementA]];
    
    
}

@end
