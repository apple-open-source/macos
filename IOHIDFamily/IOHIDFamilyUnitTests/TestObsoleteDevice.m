//
//  TestObsoleteDevice.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 12/18/17.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "HID.h"
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/hid/IOHIDLibObsolete.h>

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

static int _setCapsLockCount;
static int _getReportCount;
static int _queueCallbackCount;
static int _removalCallbackCount;
static int _inputReportCount;
static HIDKeyboardDescriptorInputReport _inputReportBuffer;

@interface TestObsoleteDevice : XCTestCase

@property dispatch_queue_t      deviceQueue;
@property dispatch_queue_t      rootQueue;
@property __block HIDUserDevice *userDevice;
@property dispatch_block_t      userDeviceCancelHandler;

@end;

@implementation TestObsoleteDevice

- (void)setUp
{
    [super setUp];
    
    _setCapsLockCount = 0;
    _getReportCount = 0;
    _queueCallbackCount = 0;
    _removalCallbackCount = 0;
    _inputReportCount = 0;
    bzero(&_inputReportBuffer, sizeof(_inputReportBuffer));
    
    NSMutableDictionary *deviceConfig = [[NSMutableDictionary alloc] init];
    NSData *desc = [[NSData alloc] initWithBytes:descriptor
                                          length:sizeof(descriptor)];
    
    _rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    _deviceQueue = dispatch_queue_create_with_target("HIDUserDevice",
                                                     DISPATCH_QUEUE_SERIAL,
                                                     _rootQueue);
    HIDXCTAssertAndThrowTrue(_deviceQueue != nil);
    
    deviceConfig[@kIOHIDReportDescriptorKey] = desc;
    deviceConfig[@kIOHIDVendorIDKey] = @(1452);
    deviceConfig[@kIOHIDProductIDKey] = @(123);
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:deviceConfig];
    HIDXCTAssertAndThrowTrue(_userDevice);
    
    [_userDevice setDispatchQueue:_deviceQueue];
    
    _userDeviceCancelHandler = dispatch_block_create(0, ^{
        ;
    });
    HIDXCTAssertAndThrowTrue(_userDeviceCancelHandler);
    
    [_userDevice setCancelHandler:_userDeviceCancelHandler];
    
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType type __unused,
                                               NSInteger reportID __unused,
                                               void *report,
                                               NSInteger *reportLength __unused)
    {
        ((uint8_t *)report)[0] = kHIDUsage_LED_CapsLock;
        
        NSLog(@"Get Report Count type : %d  ID : %ld count %d",(int)type, reportID, _getReportCount);
        _getReportCount++;
        return kIOReturnSuccess;
    }];
    
    [_userDevice setSetReportHandler:^IOReturn(HIDReportType type __unused,
                                               NSInteger reportID __unused,
                                               const void *report __unused,
                                               NSInteger reportLength __unused)
    {
        
        NSData *reportData = [[NSData alloc] initWithBytes:report length:reportLength];
        const HIDKeyboardDescriptorOutputReport *kbReport = [reportData bytes];
        
        NSLog(@"Set Report Count type : %d  ID : %ld report : %@ count %d",(int)type, reportID, reportData, _setCapsLockCount);
        if (kbReport->LED_KeyboardCapsLock == 1) {
            _setCapsLockCount++;
        }
        return kIOReturnSuccess;
    }];
    
    [_userDevice activate];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    NSLog(@"setUp");
}

- (void)tearDown
{
    NSLog(@"tearDown");
    [super tearDown];
}

static void _queueCallback(void *target __unused,
                           IOReturn result __unused,
                           void *refcon __unused,
                           void *sender)
{
    IOHIDQueueInterface **queue = (IOHIDQueueInterface **)sender;
    IOHIDEventStruct eventStruct;
    AbsoluteTime time = { 0, 0 };
    IOReturn ret;
    
    do {
        ret = (*queue)->getNextEvent(queue, &eventStruct, time, 0);
        if (ret == kIOReturnSuccess && eventStruct.longValue) {
            free(eventStruct.longValue);
        }
    } while (ret == kIOReturnSuccess);
    
    _queueCallbackCount++;
}

static void _removalCallback(void *target __unused,
                             IOReturn result __unused,
                             void *refcon __unused,
                             void *sender __unused)
{
    _removalCallbackCount++;
}

static void _inputReportCallback(void *target __unused,
                                 IOReturn result __unused,
                                 void *refcon __unused,
                                 void *sender __unused,
                                 uint32_t bufferSize __unused)
{
    _inputReportCount++;
}

- (void)testObsoleteDevice {
    IOCFPlugInInterface **plugin = nil;
    IOHIDDeviceInterface122 **device = nil;
    SInt32 score = 0;
    IOReturn ret = kIOReturnError;
    HRESULT result = E_NOINTERFACE;
    IOHIDElementCookie cookie;
    IOHIDEventStruct eventStruct = { 0 };
    NSDictionary *matching;
    CFArrayRef matchingElements;
    IOHIDQueueInterface **queue = nil;
    HIDKeyboardDescriptorOutputReport kbdReport = { 0 };
    CFRunLoopSourceRef deviceSource = NULL;
    mach_port_t asyncPort = MACH_PORT_NULL;
    IOHIDOutputTransactionInterface **transaction = nil;
    
    ret = IOCreatePlugInInterfaceForService(_userDevice.service,
                                            kIOHIDDeviceUserClientTypeID,
                                            kIOCFPlugInInterfaceID,
                                            &plugin, &score);
    XCTAssert(ret == kIOReturnSuccess && plugin);
    
    result = (*plugin)->QueryInterface(plugin,
                                    CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
                                    (LPVOID)&device);
    XCTAssert(result == S_OK && device);
    
    ret = (*device)->createAsyncEventSource(device, &deviceSource);
    XCTAssert(ret == kIOReturnSuccess && deviceSource);
    
    CFRelease(deviceSource);
    deviceSource = NULL;
    
    deviceSource = (*device)->getAsyncEventSource(device);
    XCTAssert(deviceSource);
    
    CFRunLoopAddSource(CFRunLoopGetMain(), deviceSource, kCFRunLoopDefaultMode);
    
    ret = (*device)->createAsyncPort(device, &asyncPort);
    XCTAssert(ret == kIOReturnSuccess && asyncPort);
    
    asyncPort = MACH_PORT_NULL;
    
    asyncPort = (*device)->getAsyncPort(device);
    XCTAssert(asyncPort);
    
    ret = (*device)->open(device, 0);
    XCTAssert(ret == kIOReturnSuccess);
    
    ret = (*device)->setRemovalCallback(device, _removalCallback, NULL, NULL);
    XCTAssert(ret == kIOReturnSuccess);
    
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_LEDs),
                   @kIOHIDElementUsageKey : @(kHIDUsage_LED_CapsLock) };
    ret = (*device)->copyMatchingElements(device,
                                          (__bridge CFDictionaryRef)matching,
                                          &matchingElements);
    XCTAssert(ret == kIOReturnSuccess);
    
    NSArray *matchingElement = (NSArray *)CFBridgingRelease(matchingElements);
    XCTAssert(matchingElement.count == 1);
    
    NSDictionary *element = [matchingElement objectAtIndex:0];
    cookie = (IOHIDElementCookie)[element[@kIOHIDElementCookieKey] unsignedIntValue];
    XCTAssert(cookie);
    
    kbdReport.LED_KeyboardCapsLock = 1;
    IOHIDReportType reportType = kIOHIDReportTypeOutput;
    uint8_t reportID = 0;
    uint32_t reportLength = sizeof(kbdReport);
    
    ret = (*device)->setReport(device,
                               reportType,
                               reportID,
                               &kbdReport,
                               reportLength,
                               0,
                               NULL,
                               NULL,
                               NULL);

    NSLog(@"set report ret %x count %d",ret, _setCapsLockCount);
    XCTAssert(ret == kIOReturnSuccess && _setCapsLockCount == 1);

    ret = (*device)->getElementValue(device, cookie, &eventStruct);
    XCTAssert(ret == kIOReturnSuccess && eventStruct.value == 1);
    

    ret = (*device)->setElementValue(device, cookie, &eventStruct, 0, 0, 0, 0);
    NSLog(@"set element ret %x count %d",ret, _setCapsLockCount);
    XCTAssert(ret == kIOReturnSuccess && _setCapsLockCount == 2);
    
    kbdReport.LED_KeyboardCapsLock = 0;
    ret = (*device)->getReport(device,
                               reportType,
                               reportID,
                               &kbdReport,
                               &reportLength,
                               0,
                               NULL,
                               NULL,
                               NULL);
    
    NSLog(@"get report ret %x count %d",ret, _getReportCount);
    XCTAssert(ret == kIOReturnSuccess &&
              _getReportCount &&
              kbdReport.LED_KeyboardCapsLock == 1);

    queue = (*device)->allocQueue(device);
    XCTAssert(queue);
    
    ret = (*queue)->create(queue, 0, 1);
    XCTAssert(ret == kIOReturnSuccess);
    
    CFRunLoopSourceRef source = (*queue)->getAsyncEventSource(queue);
    XCTAssert(source);
    
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
    
    ret = (*device)->copyMatchingElements(device,
                                          nil,
                                          &matchingElements);
    matchingElement = (NSArray *)CFBridgingRelease(matchingElements);
    
    for (element in matchingElement) {
        IOHIDElementType type;
        
        type = [element[@kIOHIDElementTypeKey] unsignedIntValue];
        cookie = (IOHIDElementCookie)[element[@kIOHIDElementCookieKey] unsignedIntValue];
        
        if (type <= kIOHIDElementTypeInput_ScanCodes) {
            ret = (*queue)->addElement(queue, cookie, 0);
            XCTAssert(ret == kIOReturnSuccess);
        }
    }
    
    ret = (*queue)->setEventCallout(queue, _queueCallback, NULL, NULL);
    XCTAssert(ret == kIOReturnSuccess);
    
    ret = (*queue)->start(queue);
    XCTAssert(ret == kIOReturnSuccess);
    
    ret = (*device)->setInterruptReportHandlerCallback(device,
                                                       (void *)&_inputReportBuffer,
                                                       sizeof(_inputReportBuffer),
                                                       _inputReportCallback,
                                                       0,
                                                       0);
    XCTAssert(ret == kIOReturnSuccess);
    
    HIDKeyboardDescriptorInputReport inReport = { 0 };
    inReport.KB_Keyboard[0] = kHIDUsage_KeyboardCapsLock;
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&inReport
                                                      length:sizeof(inReport)
                                                freeWhenDone:NO];
    
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    inReport.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    NSLog(@"_queueCallbackCount  %d, _inputReportCount %d",_queueCallbackCount, _inputReportCount);
    XCTAssert(_queueCallbackCount == 2);
    XCTAssert(_inputReportCount == 2);
    XCTAssert(_setCapsLockCount == 3);
    
    ret = (*queue)->stop(queue);
    XCTAssert(ret == kIOReturnSuccess);
    
    (*queue)->Release(queue);
    
    transaction = (*device)->allocOutputTransaction(device);
    XCTAssert(transaction);
    
    CFRunLoopSourceRef transactionSource = NULL;
    ret = (*transaction)->createAsyncEventSource(transaction, &transactionSource);
    XCTAssert(ret == kIOReturnSuccess && transactionSource);
    
    CFRelease(transactionSource);
    transactionSource = NULL;
    
    transactionSource = (*transaction)->getAsyncEventSource(transaction);
    XCTAssert(deviceSource);
    
    CFRunLoopAddSource(CFRunLoopGetMain(),
                       transactionSource,
                       kCFRunLoopDefaultMode);
    
    asyncPort = MACH_PORT_NULL;
    
    ret = (*transaction)->createAsyncPort(transaction, &asyncPort);
    XCTAssert(ret == kIOReturnSuccess && asyncPort);
    
    asyncPort = MACH_PORT_NULL;
    
    asyncPort = (*transaction)->getAsyncPort(transaction);
    XCTAssert(asyncPort);
    
    ret = (*transaction)->create(transaction);
    XCTAssert(ret == kIOReturnSuccess);
    
    NSDictionary *ledElement = nil;
    
    for (element in matchingElement) {
        IOHIDElementType type;
        uint32_t usagePage = 0;
        uint32_t usage = 0;
        
        type = [element[@kIOHIDElementTypeKey] unsignedIntValue];
        cookie = (IOHIDElementCookie)[element[@kIOHIDElementCookieKey] unsignedIntValue];
        usagePage = [element[@kIOHIDElementUsagePageKey] unsignedIntValue];
        usage = [element[@kIOHIDElementUsageKey] unsignedIntValue];
        
        if (type == kIOHIDElementTypeOutput &&
            usagePage == kHIDPage_LEDs &&
            usage == kHIDUsage_LED_CapsLock) {
            
            ret = (*transaction)->addElement(transaction, cookie);
            XCTAssert(ret == kIOReturnSuccess);
            
            bool has = (*transaction)->hasElement(transaction, cookie);
            XCTAssert(has);
            
            ledElement = element;
            break;
        }
    }
    
    XCTAssert(ledElement);
    bzero(&eventStruct, sizeof(eventStruct));
    
    eventStruct.type = kIOHIDElementTypeOutput;
    eventStruct.elementCookie = cookie;
    eventStruct.value = 3;
    
    ret = (*transaction)->setElementDefault(transaction, cookie, &eventStruct);
    XCTAssert(ret == kIOReturnSuccess);
    
    bzero(&eventStruct, sizeof(eventStruct));
    
    ret = (*transaction)->getElementDefault(transaction, cookie, &eventStruct);
    XCTAssert(ret == kIOReturnSuccess);
    
    XCTAssert(eventStruct.type == kIOHIDElementTypeOutput &&
              eventStruct.elementCookie == cookie &&
              eventStruct.value == 3);
    
    eventStruct.value = 1;
    ret = (*transaction)->setElementValue(transaction, cookie, &eventStruct);
    XCTAssert(ret == kIOReturnSuccess);
    
    ret = (*transaction)->commit(transaction, 0, 0, 0, 0);
    NSLog(@"commit  ret %x count %d",ret, _setCapsLockCount);
    XCTAssert(ret == kIOReturnSuccess && _setCapsLockCount == 4);
    
    ret = (*transaction)->getElementValue(transaction, cookie, &eventStruct);
    XCTAssert(ret == kIOReturnSuccess && eventStruct.value ==1 );
    
    ret = (*transaction)->dispose(transaction);
    XCTAssert(ret == kIOReturnSuccess);
    
    (*transaction)->Release(transaction);
    
    ret = (*device)->close(device);
    XCTAssert(ret == kIOReturnSuccess);

    [_userDevice cancel];
    dispatch_wait(_userDeviceCancelHandler, DISPATCH_TIME_FOREVER);
    _userDevice = nil;

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    XCTAssert(_removalCallbackCount == 1);
    
    if (plugin) {
        (*plugin)->Release(plugin);
    }
    
    if (device) {
        (*device)->Release(device);
    }
}

@end
