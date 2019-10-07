//
//  HIDFrameworkExamples.m
//  
//
//  Created by dekom on 11/11/17.
//  Copyright © 2017 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <HID/HID.h>

static uint8_t kbdDescriptor[] = {
    0x05, 0x01,         // Usage Page (Generic Desktop)
    0x09, 0x06,         // Usage (Keyboard)
    0xA1, 0x01,         // Collection (Application)
    0x05, 0x07,         //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,         //   Usage Minimum........... (224)
    0x29, 0xE7,         //   Usage Maximum........... (231)
    0x15, 0x00,         //   Logical Minimum......... (0)
    0x25, 0x01,         //   Logical Maximum......... (1)
    0x75, 0x01,         //   Report Size............. (1)
    0x95, 0x08,         //   Report Count............ (8)
    0x81, 0x02,         //   Input...................(Data, Variable, Absolute)
    0x95, 0x01,         //   Report Count............ (1)
    0x75, 0x08,         //   Report Size............. (8)
    0x81, 0x01,         //   Input...................(Constant)
    0x05, 0x08,         //   Usage Page (LED)
    0x19, 0x01,         //   Usage Minimum........... (1)
    0x29, 0x05,         //   Usage Maximum........... (5)
    0x95, 0x05,         //   Report Count............ (5)
    0x75, 0x01,         //   Report Size............. (1)
    0x91, 0x02,         //   Output..................(Data, Variable, Absolute)
    0x95, 0x01,         //   Report Count............ (1)
    0x75, 0x03,         //   Report Size............. (3)
    0x91, 0x01,         //   Output..................(Constant)
    0x05, 0x07,         //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,         //   Usage Minimum........... (0)
    0x2A, 0xFF, 0x00,   //   Usage Maximum........... (255)
    0x95, 0x05,         //   Report Count............ (5)
    0x75, 0x08,         //   Report Size............. (8)
    0x15, 0x00,         //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,   //   Logical Maximum......... (255)
    0x81, 0x00,         //   Input...................(Data, Array, Absolute)
    0xC0,               // End Collection
};

typedef struct __attribute__((packed))
{
    uint8_t  leftControl : 1;
    uint8_t  leftShift : 1;
    uint8_t  leftAlt : 1;
    uint8_t  leftGUI : 1;
    uint8_t  rightControl : 1;
    uint8_t  rightShift : 1;
    uint8_t  rightAlt : 1;
    uint8_t  rightGUI : 1;
    uint8_t  : 8;
    uint8_t  keys[5];
} kbdInputReport;

NSInteger _kbdInputReportLength = sizeof(kbdInputReport); // 7 bytes

typedef struct __attribute__((packed))
{
    uint8_t  numLock : 1;
    uint8_t  capsLock : 1;
    uint8_t  scrollLock : 1;
    uint8_t  compose : 1;
    uint8_t  kana : 1;
    uint8_t  : 3;
} kbdOutputReport;

NSInteger _kbdOutReportLength = sizeof(kbdOutputReport); // 1 bytes

dispatch_queue_t        _dispatchQueue;
NSString                *_uniqueID;
HIDUserDevice           *_userDevice;
HIDDevice               *_device;
HIDElement              *_ledElement;
HIDManager              *_manager;
HIDEventSystemClient    *_client;
HIDServiceClient        *_serviceClient;

void createUserDevice() {
    NSData *descriptor = [[NSData alloc] initWithBytes:kbdDescriptor
                                                length:sizeof(kbdDescriptor)];
    NSMutableDictionary *properties = [[NSMutableDictionary alloc] init];
    
    properties[@kIOHIDReportDescriptorKey] = descriptor;
    properties[@kIOHIDPhysicalDeviceUniqueIDKey] = _uniqueID;
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    assert(_userDevice);
    
    [_userDevice setGetReportHandler:^IOReturn(IOHIDReportType type,
                                               uint32_t reportID,
                                               uint8_t *report,
                                               NSUInteger *reportLength) {
        kbdOutputReport outReport = { 0 };
        
        assert(type == kIOHIDReportTypeOutput);
        
        outReport.capsLock = 1;
        bcopy((const void *)&outReport, report, _kbdOutReportLength);
        
        NSData *reportData = [[NSData alloc] initWithBytes:report
                                                    length:*reportLength];
        
        NSLog(@"HIDUserDevice getReport type: %d reportID: %d report: %@",
              type, reportID, reportData);
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice setSetReportHandler:^IOReturn(IOHIDReportType type,
                                               uint32_t reportID,
                                               uint8_t *report,
                                               NSUInteger reportLength) {
        kbdOutputReport *outReport;
        
        assert(type == kIOHIDReportTypeOutput);
        
        NSData *reportData = [[NSData alloc] initWithBytes:report
                                                    length:reportLength];
        
        outReport = (kbdOutputReport *)[reportData bytes];
        assert(outReport->capsLock == 1);
        
        NSLog(@"HIDUserDevice setReport type: %d reportID: %d report: %@",
              type, reportID, reportData);
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice setCancelHandler:^{
        NSLog(@"HIDUserDevice cancel handler called");
        _userDevice = nil;
    }];
    
    [_userDevice setDispatchQueue:_dispatchQueue];
    
    // all handler / matching calls should happene before activate
    [_userDevice activate];
    
    // allow some time for enumeration
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
}

void destroyUserDevice() {
    [_userDevice cancel];
    
    // allow some time for cancellation
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    assert(_userDevice == nil);
}

void createDevice() {
    NSDictionary *matching = nil;
    
    _device = [[HIDDevice alloc] initWithService:_userDevice.service];
    assert(_device);
    
    // Find caps lock LED element. This will be an output element type
    // so we can call set/getReport against it.
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_LEDs),
                   @kIOHIDElementUsageKey : @(kHIDUsage_LED_CapsLock) };
    
    _ledElement = [[_device copyMatchingElements:matching] objectAtIndex:0];
    assert(_ledElement);
    
    // match against caps lock key. This will be an input element type
    // so we can use it with the input element handler.
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardCapsLock) };
    
    [_device setInputElementMatching:matching];
    
    [_device setInputElementHandler:^(HIDDevice *sender, HIDElement *element)
     {
         NSLog(@"HIDDevice input element sender: %@ element: %@",
               sender, element);
         
         assert(element.usagePage == kHIDPage_KeyboardOrKeypad &&
                element.usage == kHIDUsage_KeyboardCapsLock);
     }];
    
    [_device setCancelHandler:^{
        NSLog(@"HIDDevice cancel handler called");
        _device = nil;
    }];
    
    [_device setRemovalHandler:^{
        NSLog(@"HIDDevice removal handler called");
    }];
    
    [_device setInputReportHandler:^(HIDDevice *sender,
                                     IOHIDReportType type,
                                     uint32_t reportID,
                                     uint8_t *report,
                                     NSUInteger reportLength)
     {
         kbdInputReport *inReport;
         NSData *reportData = [[NSData alloc] initWithBytes:report
                                                     length:reportLength];
         inReport = (kbdInputReport *)[reportData bytes];
         
         NSLog(@"HIDDevice report sender: %@ type: %d reportID: %d report: %@",
               sender, type, reportID, reportData);
         
         assert(reportLength == 7 &&
                inReport->keys[0] == kHIDUsage_KeyboardCapsLock);
     }];
    
    [_device setDispatchQueue:_dispatchQueue];
    
    [_device open];
    
    // all handler / matching calls should happene before activate
    [_device activate];
}

void destroyDevice() {
    [_device close];
    [_device cancel];
    
    // allow some time for cancellation
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    assert(_device == nil);
}

void createManager() {
    NSDictionary *matching = nil;
    
    _manager = [[HIDManager alloc] init];
    assert(_manager);
    
    // match against caps lock key. This will be an input element type
    // so we can use it with the input element handler.
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardCapsLock) };
    
    [_manager setInputElementMatching:matching];
    
    [_manager setInputElementHandler:^(HIDDevice *sender, HIDElement *element)
     {
         NSLog(@"HIDManager input element sender: %@ element: %@",
               sender, element);
         
         assert(element.usagePage == kHIDPage_KeyboardOrKeypad &&
                element.usage == kHIDUsage_KeyboardCapsLock);
     }];
    
    matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uniqueID };
    [_manager setDeviceMatching:matching];
    
    [_manager setDeviceNotificationHandler:^(HIDDevice * _Nonnull device,
                                             bool added)
     {
         NSString *deviceID = nil;
         
         NSLog(@"HIDManager device notification device: %@ added: %d",
               device, added);
         
         deviceID = [_device getProperty:@kIOHIDPhysicalDeviceUniqueIDKey];
         assert(deviceID && [deviceID isEqualToString:_uniqueID]);
     }];
    
    [_manager setInputReportHandler:^(HIDDevice *sender,
                                      IOHIDReportType type,
                                      uint32_t reportID,
                                      uint8_t *report,
                                      NSUInteger reportLength)
     {
         kbdInputReport *inReport;
         NSData *reportData = [[NSData alloc] initWithBytes:report
                                                     length:reportLength];
         inReport = (kbdInputReport *)[reportData bytes];
         
         NSLog(@"HIDManager report sender: %@ type: %d reportID: %d report: %@",
               sender, type, reportID, reportData);
         
         assert(reportLength == 7 &&
                inReport->keys[0] == kHIDUsage_KeyboardCapsLock);
     }];
    
    [_manager setCancelHandler:^{
        NSLog(@"HIDManager cancel handler called");
        _manager = nil;
    }];
    
    [_manager setDispatchQueue:_dispatchQueue];
    
    [_manager open];
    
    // all handler / matching calls should happene before activate
    [_manager activate];
}

void destroyManager() {
    [_manager close];
    [_manager cancel];
    
    // allow some time for cancellation
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    assert(_manager == nil);
}

void createClient() {
    _client = [[HIDEventSystemClient alloc] initWithType:kHIDEventSystemClientTypeMonitor];
    assert(_client);
    
    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uniqueID };
    [_client setMatching:matching];
    
    [_client setCancelHandler:^{
        NSLog(@"Client cancel handler called");
        _client = nil;
    }];
    
    [_client setDispatchQueue:_dispatchQueue];
    
    [_client setEventHandler:^(HIDServiceClient *service, HIDEvent *event) {
        NSLog(@"Received event: %@ from service: %@", event, service);
        
        NSLog(@"type: %d uP: %ld u: %ld down: %ld", event.type,
              (long)[event getIntegerField:kIOHIDEventFieldKeyboardUsage],
              (long)[event getIntegerField:kIOHIDEventFieldKeyboardUsagePage],
              (long)[event getIntegerField:kIOHIDEventFieldKeyboardDown]);
    }];
    
    [_client setResetHandler:^{
        NSLog(@"Client reset handler called");
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
        NSLog(@"Received service added notification");
        _serviceClient = service;
        
        [_serviceClient setRemovalHandler:^{
            NSLog(@"Received service removal notification");
            _serviceClient = nil;
        }];
    }];
    
    // all handler / matching calls should happene before activate
    [_client activate];
}

void destroyClient() {
    [_client cancel];
    
    // allow some time for cancellation
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    assert(_client == nil);
}

void testDevice() {
    NSLog(@"+ Test Device");
    createUserDevice();
    createDevice();
    
    assert([_device conformsTo:kHIDPage_GenericDesktop
                         usage:kHIDUsage_GD_Keyboard]);
    
    NSString *deviceID = [_device getProperty:@kIOHIDPhysicalDeviceUniqueIDKey];
    assert(deviceID && [deviceID isEqualToString:_uniqueID]);
    
    // test set/getReport
    kbdOutputReport outReport = { 0 };
    outReport.capsLock = 1;
    
    IOReturn ret = [_device setReport:kIOHIDReportTypeOutput
                             reportID:0
                               report:(uint8_t *)&outReport
                         reportLength:_kbdOutReportLength];
    assert(ret == kIOReturnSuccess);
    
    outReport.capsLock = 0;
    ret = [_device getReport:kIOHIDReportTypeOutput
                    reportID:0
                      report:(uint8_t *)&outReport
                reportLength:&_kbdOutReportLength];
    assert(ret == kIOReturnSuccess && outReport.capsLock == 1);
    
    // test set/get element
    _ledElement.integerValue = 1;
    
    // this will call setReport
    ret = [_device setElement:_ledElement];
    assert(ret == kIOReturnSuccess);
    
    _ledElement.integerValue = 0;
    ret = [_device updateElement:_ledElement options:0];
    assert(ret == kIOReturnSuccess && _ledElement.integerValue == 1);
    
    // test input reports / input elements
    kbdInputReport inReport = { 0 };
    inReport.keys[0] = kHIDUsage_KeyboardCapsLock;
    
    [_userDevice handleReport:(uint8_t *)&inReport
                 reportLength:_kbdInputReportLength];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    destroyUserDevice();
    destroyDevice();
    NSLog(@"- Test Device");
}

void testManager() {
    NSLog(@"+ Test Manager");
    createUserDevice();
    createDevice();
    createManager();
    
    NSArray *devices = [_manager copyDevices];
    assert(devices && devices.count == 1);
    
    HIDDevice *device = [devices objectAtIndex:0];
    NSString *deviceID = [device getProperty:@kIOHIDPhysicalDeviceUniqueIDKey];
    assert(deviceID && [deviceID isEqualToString:_uniqueID]);
    
    // test input reports / input elements
    kbdInputReport inReport = { 0 };
    inReport.keys[0] = kHIDUsage_KeyboardCapsLock;
    
    [_userDevice handleReport:(uint8_t *)&inReport
                 reportLength:_kbdInputReportLength];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    destroyUserDevice();
    destroyDevice();
    destroyManager();
    NSLog(@"- Test Manager");
}

void testTransaction() {
    NSLog(@"+ Test Transaction");
    HIDTransaction *transaction = nil;
    
    createUserDevice();
    createDevice();
    
    transaction = [[HIDTransaction alloc] initWithDevice:_device];
    assert(transaction);
    
    _ledElement.integerValue = 1;
    
    // test output transaction
    transaction.direction = kIOHIDTransactionDirectionTypeOutput;
    
    // this will call setReport
    IOReturn ret = [transaction commit:[NSArray arrayWithObject:_ledElement]];
    assert(ret == kIOReturnSuccess);
    
    // test input transaction
    transaction.direction = kIOHIDTransactionDirectionTypeInput;
    
    _ledElement.integerValue = 0;
    
    // this will call getReport
    ret = [transaction commit:[NSArray arrayWithObject:_ledElement]];
    assert(ret == kIOReturnSuccess && _ledElement.integerValue == 1);
    
    destroyDevice();
    destroyUserDevice();
    NSLog(@"- Test Transaction");
}

void testClient() {
    NSString *uniqueID;
    NSLog(@"+ Test Client");
    
    createClient();
    createUserDevice();
    
    assert(_serviceClient && _serviceClient.serviceID != 0);
    
    uniqueID = [_serviceClient copyProperty:@kIOHIDPhysicalDeviceUniqueIDKey];
    assert([uniqueID isEqualToString:_uniqueID]);
    
    assert([_serviceClient setProperty:@"TestProperty" value:@123]);
    
    NSNumber *prop = [_serviceClient copyProperty:@"TestProperty"];
    assert([prop unsignedIntValue] == 123);
    
    assert([_client setProperty:@"TestProperty" value:@123]);
    
    prop = [_client copyProperty:@"TestProperty"];
    assert([prop unsignedIntValue] == 123);
    
    NSArray *services = [_client copyServices];
    assert(services.count == 1);
    
    HIDServiceClient *kbdService = [services objectAtIndex:0];
    assert([kbdService conformsTo:kHIDPage_GenericDesktop
                            usage:kHIDUsage_GD_Keyboard]);
    
    assert([kbdService isEqualToHIDServiceClient:_serviceClient]);
    
    kbdInputReport inReport = { 0 };
    inReport.keys[0] = kHIDUsage_KeyboardCapsLock;
    
    [_userDevice handleReport:(uint8_t *)&inReport
                 reportLength:_kbdInputReportLength];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    inReport.keys[0] = 0;
    
    [_userDevice handleReport:(uint8_t *)&inReport
                 reportLength:_kbdInputReportLength];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    destroyUserDevice();
    assert(_serviceClient == nil);
    
    destroyClient();
    NSLog(@"- Test Client");
}

int main(int argc, const char * argv[]) {
    _dispatchQueue = dispatch_queue_create(0, DISPATCH_QUEUE_SERIAL);
    _uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    testDevice();
    testManager();
    testTransaction();
    testClient();
    
    return 0;
}
