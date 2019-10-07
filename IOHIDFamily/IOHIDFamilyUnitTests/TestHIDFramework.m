//
//  TestHIDFramework.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 1/17/18.
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <XCTest/XCTest.h>
#import "HID.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import "IOHIDUnitTestUtility.h"
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <IOKit/hid/IOHIDEvent.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDServiceClient.h>

static uint8_t kbdDescriptor[] = {
    HIDKeyboardDescriptor
};

dispatch_queue_t        _dispatchQueue;
NSString                *_uniqueID;
HIDUserDevice           *_userDevice;
XCTestExpectation       *_userDeviceCancelExp;

HIDDevice               *_device;
XCTestExpectation       *_deviceInputReportExp;
XCTestExpectation       *_deviceInputElementExp;
XCTestExpectation       *_deviceCancelExp;

HIDElement              *_ledElement;

HIDManager              *_manager;
XCTestExpectation       *_managerInputReportExp;
XCTestExpectation       *_managerInputElementExp;
XCTestExpectation       *_managerCancelExp;

HIDEventSystemClient    *_client;

HIDServiceClient        *_serviceClient;

XCTestExpectation       *_clientEventExp;
XCTestExpectation       *_clientResetExp;
XCTestExpectation       *_clientCancelExp;
XCTestExpectation       *_serviceClientAddedExp;
XCTestExpectation       *_serviceClientRemovedExp;

#if TARGET_OS_OSX
#import <IOKit/hidsystem/event_status_driver.h>
#import <IOKit/hidsystem/IOHIDLib.h>

static NXEventHandle openHIDSystem(void)
{
    kern_return_t    kr;
    io_service_t     service = MACH_PORT_NULL;
    NXEventHandle    handle = MACH_PORT_NULL;
    mach_port_t      masterPort;
    
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(kr == KERN_SUCCESS) {
        service = IORegistryEntryFromPath(masterPort, kIOServicePlane ":/IOResources/IOHIDSystem");
        if (service) {
            IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &handle);
            IOObjectRelease(service);
        }
    }
    
    return handle;
}
#endif // TARGET_OS_OSX


static void dispatchKey(UInt32 usage)
{
    HIDKeyboardDescriptorInputReport inReport = { 0 };
    inReport.KB_Keyboard[0] = usage;
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&inReport
                                                      length:sizeof(inReport)
                                                freeWhenDone:NO];
    
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    inReport.KB_Keyboard[0] = 0;
    
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
}

@interface TestHIDFramework : XCTestCase {
    bool _destroyed;
}
@end

@implementation TestHIDFramework

- (void)setUp
{
    [super setUp];
    _dispatchQueue = dispatch_queue_create(0, DISPATCH_QUEUE_SERIAL);
    _uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    _userDeviceCancelExp = [[XCTestExpectation alloc]
                            initWithDescription:@"user device cancel"];
    
    _deviceInputReportExp = [[XCTestExpectation alloc]
                             initWithDescription:@"device input reports"];
    
    _deviceInputElementExp = [[XCTestExpectation alloc]
                              initWithDescription:@"device input elements"];
    
    _deviceCancelExp = [[XCTestExpectation alloc]
                              initWithDescription:@"device cancel"];
    
    _managerInputReportExp = [[XCTestExpectation alloc]
                              initWithDescription:@"manager input reports"];
    
    _managerInputElementExp = [[XCTestExpectation alloc]
                               initWithDescription:@"manager input elements"];
    
    _managerCancelExp = [[XCTestExpectation alloc]
                         initWithDescription:@"manager cancel"];
    
    _clientEventExp = [[XCTestExpectation alloc]
                       initWithDescription:@"HID event expectation"];
    
    _clientResetExp = [[XCTestExpectation alloc]
                       initWithDescription:@"HID event system reset expectation"];
    
    _clientCancelExp = [[XCTestExpectation alloc]
                         initWithDescription:@"manager cancel"];
    
    _serviceClientAddedExp = [[XCTestExpectation alloc]
                              initWithDescription:@"service client added"];
    
    _serviceClientRemovedExp = [[XCTestExpectation alloc]
                                initWithDescription:@"service client removed"];
    
    [self createClient];
    [self createUserDevice];
    [self createDevice];
    [self createManager];
}

- (void)tearDown
{
    [self destroyClient];
    [self destroyManager];
    [self destroyDevice];
    [self destroyUserDevice];
}

- (void)createUserDevice
{
    NSData *descriptor = [[NSData alloc] initWithBytes:kbdDescriptor
                                                length:sizeof(kbdDescriptor)];
    NSMutableDictionary *properties = [[NSMutableDictionary alloc] init];
    
    properties[@kIOHIDReportDescriptorKey] = descriptor;
    properties[@kIOHIDPhysicalDeviceUniqueIDKey] = _uniqueID;
    properties[(__bridge NSString *)kIOHIDServiceHiddenKey] = @YES;
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _userDevice);
    
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType type,
                                               NSInteger reportID,
                                               void *report,
                                               NSInteger *reportLength)
    {
        HIDKeyboardDescriptorOutputReport outReport = { 0 };
        NSInteger length = MIN((unsigned long)*reportLength, sizeof(outReport));
        
        XCTAssert(type == HIDReportTypeOutput);
        
        outReport.LED_KeyboardCapsLock = 1;
        bcopy((const void *)&outReport, report, length);
        
        *reportLength = length;
        
        NSLog(@"HIDUserDevice getReport type: %lu reportID: %ld",
              (unsigned long)type, (long)reportID);
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice setSetReportHandler:^IOReturn(HIDReportType type,
                                               NSInteger reportID,
                                               const void *report __unused,
                                               NSInteger reportLength __unused) {
        XCTAssert(type == HIDReportTypeOutput);
        
        NSLog(@"HIDUserDevice setReport type: %lu reportID: %ld",
              (unsigned long)type, (long)reportID);
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice setCancelHandler:^{
        NSLog(@"HIDUserDevice cancel handler called");
        [_userDeviceCancelExp fulfill];
    }];
    
    [_userDevice setDispatchQueue:_dispatchQueue];
    
    // all handler / matching calls should happene before activate
    [_userDevice activate];
    
    mach_timespec_t waitTime = {30, 0};
    kern_return_t kr = IOServiceWaitQuiet(_userDevice.service, &waitTime);
    if (kr) {
        NSLog(@"IOServiceWaitQuiet:0x%x", kr);
        return;
    }
}

- (void)destroyUserDevice
{
    if (_destroyed) {
        return;
    }
    
    [_userDevice cancel];
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_userDeviceCancelExp]
                                                    timeout:5];
    
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _userDeviceCancelExp);
}

- (void)createDevice
{
    NSDictionary *matching = nil;
    
    _device = [[HIDDevice alloc] initWithService:_userDevice.service];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _device);
    
    // Find caps lock LED element. This will be an output element type
    // so we can call set/getReport against it.
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_LEDs),
                   @kIOHIDElementUsageKey : @(kHIDUsage_LED_CapsLock) };
    
    _ledElement = [[_device elementsMatching:matching] objectAtIndex:0];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, _ledElement);
    
    // match against caps lock key. This will be an input element type
    // so we can use it with the input element handler.
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardCapsLock) };
    
    [_device setInputElementMatching:matching];
    
    [_device setInputElementHandler:^(HIDElement *element)
     {
         NSLog(@"HIDDevice input element: %@", element);
         
         [_deviceInputElementExp fulfill];
         
         XCTAssert(element.usagePage == kHIDPage_KeyboardOrKeypad &&
                   element.usage == kHIDUsage_KeyboardCapsLock);
     }];
    
    [_device setCancelHandler:^{
        NSLog(@"HIDDevice cancel handler called");
        [_deviceCancelExp fulfill];
    }];
    
    [_device setRemovalHandler:^{
        NSLog(@"HIDDevice removal handler called");
    }];
    
    [_device setInputReportHandler:^(HIDDevice *sender,
                                     uint64_t timestamp,
                                     HIDReportType type,
                                     NSInteger reportID,
                                     NSData *report)
     {
         [_deviceInputReportExp fulfill];
         
         NSLog(@"HIDDevice report sender: %@ timestamp: %llu type: %lu reportID: %ld report: %@",
               sender, timestamp, (unsigned long)type, (long)reportID, report);
         
         XCTAssert(report.length == 8);
     }];
    
    [_device setDispatchQueue:dispatch_get_main_queue()];
    
    [_device open];
    
    // all handler / matching calls should happene before activate
    [_device activate];
}

- (void)destroyDevice
{
    [_device close];
    [_device cancel];
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_deviceCancelExp]
                                                    timeout:5];
    
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _deviceCancelExp);
}

- (void)createManager
{
    NSDictionary *matching = nil;
    
    _manager = [[HIDManager alloc] init];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, _manager);
    
    // match against caps lock key. This will be an input element type
    // so we can use it with the input element handler.
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardCapsLock) };
    
    [_manager setInputElementMatching:matching];
    
    [_manager setInputElementHandler:^(HIDDevice *sender, HIDElement *element)
     {
         NSLog(@"HIDManager input element sender: %@ element: %@",
               sender, element);
         
         [_managerInputElementExp fulfill];
         
         XCTAssert(element.usagePage == kHIDPage_KeyboardOrKeypad &&
                   element.usage == kHIDUsage_KeyboardCapsLock);
     }];
    
    matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uniqueID };
    [_manager setDeviceMatching:matching];
    
    [_manager setDeviceNotificationHandler:^(HIDDevice *device,
                                             BOOL added)
     {
         NSString *deviceID = nil;
         
         NSLog(@"HIDManager device notification device: %@ added: %d",
               device, added);
         
         deviceID = [_device propertyForKey:@kIOHIDPhysicalDeviceUniqueIDKey];
         XCTAssert(deviceID && [deviceID isEqualToString:_uniqueID]);
     }];
    
    [_manager setInputReportHandler:^(HIDDevice *sender,
                                      uint64_t timestamp,
                                      HIDReportType type,
                                      NSInteger reportID,
                                      NSData *report)
     {
         [_managerInputReportExp fulfill];
         
         NSLog(@"HIDManager report sender: %@ timestamp: %llu type: %lu reportID: %ld report: %@",
               sender, timestamp, (unsigned long)type, (long)reportID, report);
         
         XCTAssert(report.length == 8);
     }];
    
    [_manager setCancelHandler:^{
        NSLog(@"HIDManager cancel handler called");
        [_managerCancelExp fulfill];
    }];
    
    [_manager setDispatchQueue:dispatch_get_main_queue()];
    
    [_manager open];
    
    // all handler / matching calls should happene before activate
    [_manager activate];
}

- (void)destroyManager
{
    [_manager close];
    [_manager cancel];
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_managerCancelExp]
                                                    timeout:5];
    
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _managerCancelExp);
}

- (void)createClient
{
    _client = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, _client);
    
    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uniqueID,
                                (__bridge NSString *)kIOHIDServiceHiddenKey : @YES
                                };
    [_client setMatching:matching];
    
    [_client setCancelHandler:^{
        NSLog(@"Client cancel handler called");
        [_clientCancelExp fulfill];
    }];
    
    [_client setDispatchQueue:dispatch_get_main_queue()];
    
    [_client setEventHandler:^(HIDServiceClient *service, HIDEvent *event) {
        // 39998907
        if (IOHIDEventGetAttributeDataPtr((__bridge IOHIDEventRef)event)) {
            return;
        }
        
        NSLog(@"Received event: %@ from service: %@", event, service);
        
        [_clientEventExp fulfill];
        
        NSLog(@"type: %d uP: %ld u: %ld down: %ld", event.type,
              (long)[event integerValueForField:kIOHIDEventFieldKeyboardUsage],
              (long)[event integerValueForField:kIOHIDEventFieldKeyboardUsagePage],
              (long)[event integerValueForField:kIOHIDEventFieldKeyboardDown]);
    }];
    
    [_client setResetHandler:^{
        NSLog(@"Client reset handler called");
        [_clientResetExp fulfill];
    }];
    
    [_client setEventFilterHandler:^BOOL(HIDServiceClient *service,
                                         HIDEvent *event) {
        bool filter = false;
        
        if (event.type == kIOHIDEventTypeNULL) {
            filter = true;
        }
        
        NSLog(@"Client filter handler called for event: %@ service: %@",
              event, service);
        
        return filter;
    }];
    
    _serviceClient = nil;
    [_client setServiceNotificationHandler:^(HIDServiceClient *service) {
        NSLog(@"Service added: %@", service);
        _serviceClient = service;
        [_serviceClientAddedExp fulfill];
        
        [_serviceClient setRemovalHandler:^{
            NSLog(@"Service removed: %@", _serviceClient);
            [_serviceClientRemovedExp fulfill];
        }];
    }];
    
    // all handler / matching calls should happene before activate
    [_client activate];
}

- (void)destroyClient
{
    [_client cancel];
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_clientCancelExp]
                                                    timeout:5];
    
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _clientCancelExp);
}

- (void)testDevice {
    bool result = false;
    XCTWaiterResult expResult;
    NSError *err;
    
    NSLog(@"+ Test Device");
    
    XCTAssert([_device conformsToUsagePage:kHIDPage_GenericDesktop
                                     usage:kHIDUsage_GD_Keyboard]);
    
    NSString *deviceID = [_device propertyForKey:@kIOHIDPhysicalDeviceUniqueIDKey];
    XCTAssert(deviceID && [deviceID isEqualToString:_uniqueID]);
    
    // test user device set / get property
    deviceID = [_userDevice propertyForKey:@(kIOHIDPhysicalDeviceUniqueIDKey)];
    XCTAssert(deviceID && [deviceID isEqualToString:_uniqueID]);
    
    XCTAssert([_userDevice setProperty:@123 forKey:@"TestProperty"]);
    
    NSNumber *prop = [_userDevice propertyForKey:@"TestProperty"];
    XCTAssert([prop unsignedIntValue] == 123);
    
    // test set/getReport
    HIDKeyboardDescriptorOutputReport outReport = { 0 };
    outReport.LED_KeyboardCapsLock = 1;
    NSMutableData *reportData = [[NSMutableData alloc] initWithBytes:&outReport
                                                              length:sizeof(outReport)];
    
    BOOL ret = [_device setReport:reportData.bytes
                     reportLength:reportData.length
                   withIdentifier:0
                          forType:HIDReportTypeOutput
                            error:&err];
    XCTAssert(ret, "setReport failed ret: %d err: %@", ret, err);
    
    ((HIDKeyboardDescriptorOutputReport *)reportData.mutableBytes)->LED_KeyboardCapsLock = 0;
    NSInteger reportSize = sizeof(HIDKeyboardDescriptorOutputReport);
    ret = [_device getReport:reportData.mutableBytes
                reportLength:&reportSize
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:nil];
    XCTAssert(ret && ((HIDKeyboardDescriptorOutputReport *)reportData.mutableBytes)->LED_KeyboardCapsLock == 1,
              "getReport failed ret: %d report: %@", ret, reportData);
    
    // test user device suspend / resume
    result = [_userDevice setProperty:@(YES)
                               forKey:@(kIOHIDUserDeviceSuspendKey)];
    XCTAssert(result);
    
    NSError *error = nil;
    ret = [_device setReport:reportData.bytes
                reportLength:reportData.length
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:&error];
    XCTAssert(!ret && error);
    NSLog(@"Set report: %@", error);
    
    error = nil;
    ret = [_device getReport:reportData.mutableBytes
                reportLength:&reportSize
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:&error];
    XCTAssert(!ret && error);
    NSLog(@"Get report: %@", error);
    
    result = [_userDevice setProperty:@(NO)
                               forKey:@(kIOHIDUserDeviceSuspendKey)];
    XCTAssert(result);
    
    ret = [_device setReport:reportData.bytes
                reportLength:reportData.length
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:&err];
    XCTAssert(ret, "setReport failed ret: %d err: %@", ret, err);
    
    ((HIDKeyboardDescriptorOutputReport *)reportData.mutableBytes)->LED_KeyboardCapsLock = 0;
    ret = [_device getReport:reportData.mutableBytes
                reportLength:&reportSize
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:&err];
    XCTAssert(ret && ((HIDKeyboardDescriptorOutputReport *)reportData.mutableBytes)->LED_KeyboardCapsLock == 1,
              "getReport failed ret: %d err: %@", ret, err);
    
    // test set/get element
    _ledElement.integerValue = 1;
    
    // this will call setReport
    ret = [_device commitElements:@[_ledElement]
                        direction:HIDDeviceCommitDirectionOut
                            error:&err];
    XCTAssert(ret, "commitElements (out) failed ret: %d err: %@", ret, err);
    
    _ledElement.integerValue = 0;
    ret = [_device commitElements:@[_ledElement]
                        direction:HIDDeviceCommitDirectionIn
                            error:&err];
    XCTAssert(ret && _ledElement.integerValue == 1,
              "commitElements (in) failed ret: %d err: %@", ret, err);
    
    // test input reports / input elements
    _deviceInputReportExp.expectedFulfillmentCount = 2;
    _deviceInputElementExp.expectedFulfillmentCount = 2;
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    expResult = [XCTWaiter waitForExpectations:@[_deviceInputReportExp,
                                                 _deviceInputElementExp]
                                       timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectations: %@ %@",
                               _deviceInputReportExp,
                               _deviceInputElementExp);
    
    // test device suspend / resume
    [_device setProperty:@(YES) forKey:@(kIOHIDDeviceSuspendKey)];
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    [_device setProperty:@(NO) forKey:@(kIOHIDDeviceSuspendKey)];
    
    _deviceInputReportExp = [[XCTestExpectation alloc]
                              initWithDescription:@"device input reports"];
    _deviceInputReportExp.expectedFulfillmentCount = 2;
    
    _deviceInputElementExp = [[XCTestExpectation alloc]
                               initWithDescription:@"device input elements"];
    _deviceInputElementExp.expectedFulfillmentCount = 2;
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    expResult = [XCTWaiter waitForExpectations:@[_deviceInputReportExp,
                                                 _deviceInputElementExp]
                                       timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectations: %@ %@",
                               _deviceInputReportExp,
                               _deviceInputElementExp);
    
    NSLog(@"- Test Device");
}

- (void)testManager {
    XCTWaiterResult result;
    
    NSLog(@"+ Test Manager");
    
    NSArray *devices = _manager.devices;
    XCTAssert(devices && devices.count == 1);
    
    HIDDevice *device = [devices objectAtIndex:0];
    NSString *deviceID = [device propertyForKey:@kIOHIDPhysicalDeviceUniqueIDKey];
    XCTAssert(deviceID && [deviceID isEqualToString:_uniqueID]);
    
    // test input reports / input elements
    _managerInputReportExp.expectedFulfillmentCount = 2;
    _managerInputElementExp.expectedFulfillmentCount = 2;
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    result = [XCTWaiter waitForExpectations:@[_managerInputReportExp,
                                              _managerInputElementExp]
                                    timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectations: %@ %@",
                               _managerInputReportExp,
                               _managerInputElementExp);
    
#if TARGET_OS_OSX
    IOReturn ret;
    bool state;
    NXEventHandle hidSystem = openHIDSystem();
    ret = IOHIDGetModifierLockState(hidSystem, kIOHIDCapsLockState, &state);
    NSLog(@"IOHIDGetModifierLockState: 0x%x %d", ret, state);
    XCTAssert(ret == kIOReturnSuccess && state == true);
#endif // TARGET_OS_OSX
    
    // test manager suspend / resume
    [_manager setProperty:@(YES) forKey:@(kIOHIDDeviceSuspendKey)];
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
#if TARGET_OS_OSX
    ret = IOHIDGetModifierLockState(hidSystem, kIOHIDCapsLockState, &state);
    NSLog(@"IOHIDGetModifierLockState: 0x%x %d", ret, state);
    XCTAssert(ret == kIOReturnSuccess && state == false);
    IOServiceClose(hidSystem);
#endif // TARGET_OS_OSX
    
    [_manager setProperty:@(NO) forKey:@(kIOHIDDeviceSuspendKey)];
    
    _managerInputReportExp = [[XCTestExpectation alloc]
                       initWithDescription:@"manager input reports"];
    _managerInputReportExp.expectedFulfillmentCount = 2;
    
    _managerInputElementExp = [[XCTestExpectation alloc]
                       initWithDescription:@"manager input elements"];
    _managerInputElementExp.expectedFulfillmentCount = 2;
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    result = [XCTWaiter waitForExpectations:@[_managerInputReportExp,
                                              _managerInputElementExp]
                                    timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectations: %@ %@",
                               _managerInputReportExp,
                               _managerInputElementExp);
    
    NSLog(@"- Test Manager");
}

- (void)testClient {
    XCTWaiterResult result;
    
    NSString *uniqueID;
    NSLog(@"+ Test Client");
    
    result = [XCTWaiter waitForExpectations:@[_serviceClientAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _serviceClientAddedExp);
    
    uniqueID = [_serviceClient propertyForKey:@kIOHIDPhysicalDeviceUniqueIDKey];
    XCTAssert([uniqueID isEqualToString:_uniqueID]);
    
    XCTAssert([_serviceClient setProperty:@123 forKey:@"TestProperty"]);
    
    NSNumber *prop = [_serviceClient propertyForKey:@"TestProperty"];
    XCTAssert([prop unsignedIntValue] == 123);
    
    XCTAssert([_client setProperty:@123 forKey:@"TestProperty"]);
    
    prop = [_client propertyForKey:@"TestProperty"];
    XCTAssert([prop unsignedIntValue] == 123);
    
    NSArray *services = _client.services;
    XCTAssert(services.count == 1);
    
    HIDServiceClient *kbdService = [services objectAtIndex:0];
    XCTAssert([kbdService conformsToUsagePage:kHIDPage_GenericDesktop
                                        usage:kHIDUsage_GD_Keyboard]);
    
    XCTAssert(IOHIDServiceClientGetTypeID() ==
              CFGetTypeID((__bridge IOHIDServiceClientRef)kbdService));
    
    _clientEventExp.expectedFulfillmentCount = 2;
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    result = [XCTWaiter waitForExpectations:@[_clientEventExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _clientEventExp);
    
    // test client suspend / resume
    [_client setProperty:@(YES) forKey:@(kIOHIDClientSuspendKey)];
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    [_client setProperty:@(NO) forKey:@(kIOHIDClientSuspendKey)];
    
    _clientEventExp = [[XCTestExpectation alloc]
                       initWithDescription:@"HID event expectation"];
    _clientEventExp.expectedFulfillmentCount = 2;
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    result = [XCTWaiter waitForExpectations:@[_clientEventExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _clientEventExp);
    
    // Test for 41912875
    // Kill hidd and verify the client is still able to receive events
#if TARGET_OS_OSX
    system("killall hidd");
    
    result = [XCTWaiter waitForExpectations:@[_clientResetExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _clientResetExp);
    
    _clientEventExp = [[XCTestExpectation alloc]
                       initWithDescription:@"HID event expectation"];
    _clientEventExp.expectedFulfillmentCount = 2;
    
    dispatchKey(kHIDUsage_KeyboardCapsLock);
    
    result = [XCTWaiter waitForExpectations:@[_clientEventExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _clientEventExp);
#endif
    
    [self destroyUserDevice];
    _destroyed = true;
    
    result = [XCTWaiter waitForExpectations:@[_serviceClientRemovedExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _serviceClientRemovedExp);
    
    NSLog(@"- Test Client");
}

@end
