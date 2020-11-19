//
//  TestHIDSameUsageElementConsolidation.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 9/23/19.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import <IOKit/hid/IOHIDElement.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "IOHIDXCTestExpectation.h"

static uint8_t descriptor[] = {
    HIDReportWithSingleUsageAndMultiReportCount
};



@interface TestHIDSameUsageElementConsolidation : XCTestCase {
    
    HIDUserDevice *_userDevice;
    NSUUID        *_uuid;
    HIDManager    *_hidManager;
    HIDDevice     *_device;
}
@end

@implementation TestHIDSameUsageElementConsolidation {

XCTestExpectation *_devAddedExp;
}

-(void) createUserDevice
{
    if (_userDevice) return;
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
       
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                     @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithLogs(_userDevice,"Failed to create user device");
}

-(void) createHIDManager
{
    
    __weak TestHIDSameUsageElementConsolidation *weakSelf = self;
    
    if (_hidManager) return;
    
    _hidManager  = [[HIDManager alloc] init];
    
    HIDXCTAssertWithLogs(_hidManager,"Failed to create hid manager");
    
    [_hidManager setDeviceMatching:@{@kIOHIDPhysicalDeviceUniqueIDKey : _uuid}];
    
    [_hidManager setDispatchQueue:dispatch_queue_create("com.apple.HID.Test", nil)];
    
    [_hidManager setDeviceNotificationHandler:^(HIDDevice * _Nonnull device, BOOL added) {
        
        __strong TestHIDSameUsageElementConsolidation *strongSelf = weakSelf;
        
        if (!strongSelf) {
            return;
        }
        if (added) {
            strongSelf->_device = device;
            [strongSelf->_devAddedExp fulfill];
        }
        
        NSLog(@"%@",device);
    }];
    
    [_hidManager open];
    [_hidManager activate];
}
- (void)setUp
{
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _uuid = [[NSUUID alloc] init];
    
    [self createHIDManager];
    
    [self createUserDevice];
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:10];
       HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
       result == XCTWaiterResultCompleted);
}

- (void)tearDown
{
    [_hidManager cancel];
    [_hidManager close];
}

-(void) testElementConsolidation {
    
    
    NSArray<HIDElement*> *elements = [_device elementsMatching:@{@kIOHIDElementUsagePageKey : @(0xff26)}];
    
    HIDXCTAssertWithLogs(elements,"No valid elements ");
    
    __block NSMutableArray *reportIDOne = [[NSMutableArray alloc] init];
    __block NSMutableArray *reportIDTwo = [[NSMutableArray alloc] init];
    
    [elements enumerateObjectsUsingBlock:^(HIDElement * _Nonnull obj, NSUInteger __unused  idx, BOOL __unused  * _Nonnull stop) {
        
        NSLog(@"%@",obj);
        NSLog(@"Element Count %u Element Size %ld", IOHIDElementGetReportCount((__bridge IOHIDElementRef)obj), (long)obj.reportSize);

        
        HIDElement *element = obj;
        
        switch (element.reportID) {
            case 1:
                [reportIDOne addObject:element];
                break;
            case 2:
                [reportIDTwo addObject:element];
                break;
                
            default:
                break;
        }
        
    }];
    
    NSLog(@"Element Count Report ID 1 %ld",reportIDOne.count);
    NSLog(@"Element Count Report ID 2 %ld",reportIDTwo.count);
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, reportIDOne.count == 1);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, reportIDTwo.count > 1);
    
}
@end
