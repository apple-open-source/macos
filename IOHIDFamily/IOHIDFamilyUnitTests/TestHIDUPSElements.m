//
//  TestHIDUPSElements.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 10/11/19.
//

#import <XCTest/XCTest.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "IOHIDXCTestExpectation.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hidsystem/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <HID/HID.h>
static uint8_t descriptor[] = {
    HIDBatteryCase
};

static XCTestExpectation *_devAddedExp;

@interface TestHIDUPSElements : XCTestCase {
    HIDUserDevice *_userDevice;
    HIDManager    *_manager;
    NSString      *_uuid;
    HIDDevice     *_device;
}

@end


HIDBatteryCaseInputReport03 inputReport3;

@implementation TestHIDUPSElements

-(void) createUserDevice {
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    dispatch_queue_t queue = dispatch_queue_create("com.apple.HIDTest.device", DISPATCH_QUEUE_SERIAL);
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               _userDevice != NULL);
    
    [_userDevice setDispatchQueue:queue];
    
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, void * _Nonnull report, NSInteger * _Nonnull reportLength) {
        
        NSLog(@"Get Report Type %ld ID %ld",type, reportID);
        if (reportID == 3) {
            memset(&inputReport3, 0, sizeof(HIDBatteryCaseInputReport03));
            inputReport3.reportId = 3;
            inputReport3.BAT_PrimaryBatteryCyclecount = 0;
            memcpy(report, &inputReport3, sizeof(HIDBatteryCaseInputReport03));
            *reportLength = sizeof(HIDBatteryCaseInputReport03);
        }
        
        return kIOReturnSuccess;
        
    }];
    
    [_userDevice activate];
    
    
}

-(void) createHIDManager {
    
    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid };
    _manager = [HIDManager new];
    
    [_manager setDeviceMatching:matching];
    [_manager setDispatchQueue:dispatch_get_main_queue()];
    
    __weak TestHIDUPSElements *weakSelf = self;
    
    [_manager setDeviceNotificationHandler:^(HIDDevice *device, BOOL added) {
        NSLog(@"device %s:\n%@", added ? "added" : "removed", device);
         __strong TestHIDUPSElements *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        if (added) {
            strongSelf->_device = device;
            [_devAddedExp fulfill];
        }
    }];
    
    [_manager activate];
    
}
- (void)setUp {
    
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    [self createHIDManager];
    [self createUserDevice];
    
    
}

- (void)tearDown {
    
    if (_manager) {
        [_manager cancel];
    }
    
    if (_userDevice) {
        [_userDevice cancel];
    }
    
    if (_device) {
        [_device close];
    }
    
    [super tearDown];
}

- (void)testHIDUPSElements {
    XCTWaiterResult result;
    NSError *err;
    NSArray<HIDElement*> *elements;
    
    __block HIDElement *cycleCountElement = nil;
   
   result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
   HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                              result == XCTWaiterResultCompleted);
    
    [_device open];
    
    elements = [_device elementsMatching:@{@kIOHIDElementUsagePageKey : @(kHIDPage_BatterySystem)}];
                                               
    [elements enumerateObjectsUsingBlock:^(HIDElement * _Nonnull obj, NSUInteger idx __unused , BOOL * _Nonnull stop) {
        if (obj.usage == kHIDUsage_BS_CycleCount) {
            cycleCountElement = obj;
            *stop = true;
            return;
        }
    }];
    
    [_device commitElements:@[cycleCountElement] direction:HIDDeviceCommitDirectionIn error:&err];
    
    NSLog(@"Element Values UP : %lx U : %lx timestamp : %lld Data : %@",cycleCountElement.usagePage, cycleCountElement.usage, cycleCountElement.timestamp, cycleCountElement.dataValue);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, err == NULL && cycleCountElement.timestamp != 0);
   
}


@end

