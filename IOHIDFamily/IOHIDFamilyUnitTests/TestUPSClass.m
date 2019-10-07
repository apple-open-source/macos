//
//  TestUPSClass.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 11/28/17.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <IOKit/ps/IOUPSPlugIn.h>
#import <AssertMacros.h>
#import <IOKit/ps/IOPSKeysPrivate.h>
#import <IOKit/ps/IOPSKeys.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import "IOHIDUnitTestUtility.h"
#import "HID.h"

// This is a report descriptor that resembles the apple battery case
static uint8_t appleBatteryCaseDescriptor[] = {
    0x05, 0x85,                               // Usage Page (Power Class reserved)
    0x09, 0x2E,                               // Usage 1 (0x1)
    0xA1, 0x01,                               // Collection (Application)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x84,                               //     Usage Page (Power Device)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x09, 0x30,                               //     Usage 48 (0x30)
    0x67, 0x21, 0xD1, 0xF0, 0x00,             //     Unit.................... (15782177)
    0x55, 0x04,                               //     Unit Exponent........... (4)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x27, 0x70, 0x17, 0x00, 0x00,             //     Logical Maximum......... (6000)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x31,                               //     Usage 49 (0x31)
    0x67, 0x01, 0x00, 0x10, 0x00,             //     Unit.................... (1048577)
    0x55, 0x0D,                               //     Unit Exponent........... (13)
    0x16, 0x00, 0x80,                         //     Logical Minimum......... (-32768)
    0x26, 0xFF, 0x7F,                         //     Logical Maximum......... (32767)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x36,                               //     Usage 54 (0x36)
    0x67, 0x01, 0x00, 0x01, 0x00,             //     Unit.................... (65537)
    0x55, 0x0F,                               //     Unit Exponent........... (15)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,             //     Logical Maximum......... (65535)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x40,                               //     Usage 64 (0x40)
    0x67, 0x21, 0xD1, 0xF0, 0x00,             //     Unit.................... (15782177)
    0x55, 0x0D,                               //     Unit Exponent........... (13)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x40,                               //     Usage 64 (0x40)
    0x91, 0xA2,                               //     Output..................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x41,                               //     Usage 65 (0x41)
    0x67, 0x01, 0x00, 0x10, 0x00,             //     Unit.................... (1048577)
    0x55, 0x0D,                               //     Unit Exponent........... (13)
    0x81, 0xA2,                               //     Input...................(Data, Variable, Absolute, No Preferred)
    0x09, 0x41,                               //     Usage 65 (0x41)
    0x91, 0xA2,                               //     Output..................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x6D,                               //     Usage 109 (0x6d)
    0x75, 0x08,                               //     Report Size............. (8)
    0x65, 0x00,                               //     Unit.................... (0)
    0x55, 0x00,                               //     Unit Exponent........... (0)
    0x26, 0xFF, 0x00,                         //     Logical Maximum......... (255)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x6D,                               //     Usage 109 (0x6d)
    0x91, 0xA2,                               //     Output..................(Data, Variable, Absolute, No Preferred, Volatile)
    0x05, 0x85,                               //     Usage Page (Battery System)
    0x09, 0x44,                               //     Usage 68 (0x44)
    0x75, 0x01,                               //     Report Size............. (1)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x81, 0xA2,                               //     Input...................(Data, Variable, Absolute, No Preferred)
    0x09, 0x44,                               //     Usage 68 (0x44)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x75, 0x07,                               //     Report Size............. (7)
    0x81, 0x01,                               //     Input...................(Constant)
    0x09, 0x44,                               //     Usage 68 (0x44)
    0xB1, 0x01,                               //     Feature.................(Constant)
    0x09, 0x66,                               //     Usage 102 (0x66)
    0x67, 0x01, 0x10, 0x10, 0x00,             //     Unit.................... (1052673)
    0x27, 0xFF, 0xFF, 0x00, 0x00,             //     Logical Maximum......... (65535)
    0x75, 0x10,                               //     Report Size............. (16)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x66,                               //     Usage 102 (0x66)
    0x91, 0xA2,                               //     Output..................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x67,                               //     Usage 103 (0x67)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x68,                               //     Usage 104 (0x68)
    0x66, 0x01, 0x10,                         //     Unit.................... (4097)
    0x75, 0x20,                               //     Report Size............. (32)
    0x17, 0x00, 0x00, 0x00, 0x80,             //     Logical Minimum......... (-2147483648)
    0x27, 0xFF, 0xFF, 0xFF, 0x7F,             //     Logical Maximum......... (2147483647)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x6B,                               //     Usage 107 (0x6b)
    0x65, 0x00,                               //     Unit.................... (0)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,             //     Logical Maximum......... (65535)
    0x75, 0x10,                               //     Report Size............. (16)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0xD0,                               //     Usage 208 (0xd0)
    0x75, 0x01,                               //     Report Size............. (1)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x81, 0xA2,                               //     Input...................(Data, Variable, Absolute, No Preferred)
    0x09, 0xD0,                               //     Usage 208 (0xd0)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0xD0,                               //     Usage 208 (0xd0)
    0x75, 0x07,                               //     Report Size............. (7)
    0x81, 0x01,                               //     Input...................(Constant)
    0x09, 0xD0,                               //     Usage 208 (0xd0)
    0xB1, 0x01,                               //     Feature.................(Constant)
    0x06, 0x0D, 0xFF,                         //     Usage Page (65293)
    0x09, 0x01,                               //     Usage 1 (0x1)
    0x67, 0x01, 0x10, 0x10, 0x00,             //     Unit.................... (1052673)
    0x27, 0xFF, 0xFF, 0x00, 0x00,             //     Logical Maximum......... (65535)
    0x75, 0x10,                               //     Report Size............. (16)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x02,                               //     Usage 2 (0x2)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0x09, 0x03,                               //     Usage 3 (0x3)
    0x17, 0x00, 0x00, 0x00, 0x80,             //     Logical Minimum......... (-2147483648)
    0x27, 0xFF, 0xFF, 0xFF, 0x7F,             //     Logical Maximum......... (2147483647)
    0x75, 0x20,                               //     Report Size............. (32)
    0xB1, 0xA2,                               //     Feature.................(Data, Variable, Absolute, No Preferred, Volatile)
    0xC0,                                     //   End Collection
    0xC0,                                     // End Collection
};

typedef struct __attribute__((packed)) {
    // kHIDPage_PowerDevice
    uint16_t    voltage;                // kHIDUsage_PD_Voltage (0x30)
    uint16_t    current;                // kHIDUsage_PD_Current (0x31)
    uint16_t    temperature;            // kHIDUsage_PD_Temperature (0x36)
    uint16_t    configVoltage;          // kHIDUsage_PD_ConfigVoltage (0x40)
    uint8_t     used;                   // kHIDUsage_PD_Used (0x6D)
    
    // kHIDPage_BatterySystem
    uint8_t     charging : 1;           // kHIDUsage_BS_Charging (0x44)
    uint8_t:7;                          // pad
    uint16_t    remainingCapacity;      // kHIDUsage_BS_RemainingCapacity (0x66)
    uint16_t    fullChargeCapacity;     // kHIDUsage_BS_FullChargeCapacity (0x67)
    uint32_t    runTimeToEmpty;         // kHIDUsage_BS_RunTimeToEmpty (0x68)
    uint16_t    cycleCount;             // kHIDUsage_BS_CycleCount (0x6B)
    
    uint8_t     acPresent : 1;          // kHIDUsage_BS_ACPresent (0xD0)
    uint8_t:7;                          // pad
    
    // kHIDPage_AppleVendorBattery
    uint16_t    rawCapacity;            // kHIDUsage_AppleVendorBattery_RawCapacity (0x01)
    uint16_t    nominalChargeCapacity;  // kHIDUsage_AppleVendorBattery_NominalChargeCapacity (0x02)
    uint32_t    cumulativeCurrent;      // kHIDUsage_AppleVendorBattery_CumulativeCurrent (0x03)
} AppleBatteryCaseFeatureReport;

NSInteger _featureReportLength = sizeof(AppleBatteryCaseFeatureReport); // 29 bytes
AppleBatteryCaseFeatureReport _featureReport;

typedef struct __attribute__((packed)) {
    // kHIDPage_PowerDevice
    uint16_t    configCurrent;  // kHIDUsage_PD_ConfigCurrent (0x41)
    
    // kHIDPage_BatterySystem
    uint8_t     charging : 1;   // kHIDUsage_BS_Charging (0x44)
    uint8_t:7;                  // pad
    uint8_t     acPresent : 1;  // kHIDUsage_BS_ACPresent (0xD0)
    uint8_t:7;                  // pad
} AppleBatteryCaseInputReport;

NSInteger _inputReportLength = sizeof(AppleBatteryCaseInputReport); // 4 bytes
AppleBatteryCaseInputReport _inputReport;

typedef struct __attribute__((packed)) {
    // kHIDPage_PowerDevice
    uint16_t    configVoltage;      // kHIDUsage_PD_ConfigVoltage (0x40)
    uint16_t    configCurrent;      // kHIDUsage_PD_ConfigCurrent (0x41)
    uint8_t     used;               // kHIDUsage_PD_Used (0x6D)
    
    // kHIDPage_BatterySystem
    uint16_t    remainingCapacity;  //kHIDUsage_BS_RemainingCapacity (0x66)
} AppleBatteryCaseOutputReport;

NSInteger _outputReportLength = sizeof(AppleBatteryCaseOutputReport); // 7 bytes
AppleBatteryCaseOutputReport _outputReport;

int _callbackCount;

@interface TestUPSClass : XCTestCase

@property dispatch_queue_t      deviceQueue;
@property dispatch_queue_t      rootQueue;
@property __block HIDUserDevice *userDevice;
@property dispatch_block_t      userDeviceCancelHandler;

@end;

@implementation TestUPSClass

- (void)setUp
{
    [super setUp];
    NSLog(@"setUp");
}

- (void)tearDown
{
    NSLog(@"tearDown");
    [_userDevice cancel];
    dispatch_wait(_userDeviceCancelHandler, DISPATCH_TIME_FOREVER);
    _userDevice = nil;
    [super tearDown];
}

static void upsCallback(void *target __unused,
                        IOReturn result __unused,
                        void *refcon __unused,
                        void *sender __unused,
                        CFDictionaryRef event)
{
    _callbackCount++;
    NSLog(@"upsCallback %@", (__bridge NSDictionary *)event);
}

- (void)MAC_OS_ONLY_TEST_CASE(testUPSClass) {
    IOCFPlugInInterface **plugin = nil;
    IOUPSPlugInInterface_v140 **ups = nil;
    SInt32 score = 0;
    IOReturn ret = kIOReturnError;
    HRESULT result = E_NOINTERFACE;
    NSDictionary *props = nil;
    NSDictionary *event = nil;
    NSSet *caps = nil;
    NSMutableDictionary *command = [[NSMutableDictionary alloc] init];
    NSMutableDictionary *deviceConfig = [[NSMutableDictionary alloc] init];
    NSData *desc = [[NSData alloc] initWithBytes:appleBatteryCaseDescriptor
                                          length:sizeof(appleBatteryCaseDescriptor)];
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&_inputReport
                                                      length:_inputReportLength
                                                freeWhenDone:NO];
    
    bzero(&_featureReport, _featureReportLength);
    bzero(&_outputReport, _outputReportLength);
    bzero(&_inputReport, _inputReportLength);
    _callbackCount = 0;
    
    // some dummy values taken from an apple battery case
    _inputReport.configCurrent = 123;
    _inputReport.charging = 1;
    _inputReport.acPresent = 1;
    
    _featureReport.voltage = 3968;
    _featureReport.current = 1234;
    _featureReport.temperature = 2971;
    _featureReport.configVoltage = 3951;
    _featureReport.used = 1;
    _featureReport.charging = 1;
    _featureReport.remainingCapacity = 5814;
    _featureReport.fullChargeCapacity = 8168;
    _featureReport.runTimeToEmpty = 21300;
    _featureReport.cycleCount = 60;
    _featureReport.acPresent = 1;
    _featureReport.rawCapacity = 5554;
    _featureReport.nominalChargeCapacity = 8431;
    _featureReport.cumulativeCurrent = 6;
    
    _rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    _deviceQueue = dispatch_queue_create_with_target("HIDUserDevice",
                                                     DISPATCH_QUEUE_SERIAL,
                                                     _rootQueue);
    HIDXCTAssertAndThrowTrue(_deviceQueue != nil);
    
    deviceConfig[@kIOHIDReportDescriptorKey] = desc;
    deviceConfig[@kIOHIDProductKey] = @"Apple Battery Case";
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:deviceConfig];
    HIDXCTAssertAndThrowTrue(_userDevice);
    
    [_userDevice setDispatchQueue:_deviceQueue];
    
    _userDeviceCancelHandler = dispatch_block_create(0, ^{
        ;
    });
    HIDXCTAssertAndThrowTrue(_userDeviceCancelHandler);
    
    [_userDevice setCancelHandler:_userDeviceCancelHandler];
    
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType type,
                                               NSInteger reportID __unused,
                                               void *report,
                                               NSInteger *reportLength)
    {
        NSInteger length;
        
        if (type == kIOHIDReportTypeInput) {
            length = MIN((unsigned long)*reportLength, sizeof(_inputReport));
            
            bcopy((const void *)&_inputReport,
                  report,
                  length);
            
            *reportLength = length;
        } else if (type == kIOHIDReportTypeFeature) {
            length = MIN((unsigned long)*reportLength, sizeof(_featureReport));
            
            bcopy((const void *)&_featureReport,
                  report,
                  length);
            
            *reportLength = length;
        }
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice setSetReportHandler:^IOReturn(HIDReportType type,
                                               NSInteger reportID __unused,
                                               const void *report,
                                               NSInteger reportLength)
    {
        if (type == kIOHIDReportTypeOutput) {
            bcopy(report,
                  (void *)&_outputReport,
                  MIN((unsigned long)reportLength, sizeof(_outputReport)));
        } else if (type == kIOHIDReportTypeFeature) {
            bcopy(report,
                  (void *)&_featureReport,
                  MIN((unsigned long)reportLength, sizeof(_featureReport)));
        }
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice activate];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    ret = IOCreatePlugInInterfaceForService(_userDevice.service,
                                            kIOUPSPlugInTypeID,
                                            kIOCFPlugInInterfaceID,
                                            &plugin, &score);
    XCTAssert(ret == kIOReturnSuccess && plugin);
    
    result = (*plugin)->QueryInterface(plugin,
                                       CFUUIDGetUUIDBytes(kIOUPSPlugInInterfaceID_v140),
                                       (LPVOID)&ups);
    XCTAssert(result == S_OK);
    
    NSArray *eventSources = nil;
    ret = (*ups)->createAsyncEventSource(ups, (void *)&eventSources);
    XCTAssert(ret == kIOReturnSuccess && eventSources);
    
    for (id source in eventSources) {
        CFTypeRef sourceRef = (__bridge CFTypeRef)source;
        
        if (CFGetTypeID(sourceRef) == CFRunLoopTimerGetTypeID()) {
            CFRunLoopAddTimer(CFRunLoopGetCurrent(),
                              (CFRunLoopTimerRef)sourceRef,
                              kCFRunLoopDefaultMode);
        } else if (CFGetTypeID(sourceRef) == CFRunLoopSourceGetTypeID()) {
            CFRunLoopAddSource(CFRunLoopGetCurrent(),
                               (CFRunLoopSourceRef)sourceRef,
                               kCFRunLoopDefaultMode);
        }
    }
    
    ret = (*ups)->getProperties(ups, (void *)&props);
    XCTAssert(ret == kIOReturnSuccess && props);
    XCTAssert([props[@kIOPSNameKey] isEqualToString:@"Apple Battery Case"]);
    
    ret = (*ups)->getCapabilities(ups, (void *)&caps);
    XCTAssert(ret == kIOReturnSuccess && caps && caps.count);
    
    ret = (*ups)->getEvent(ups, (void *)&event);
    XCTAssert(ret == kIOReturnSuccess && event && event.count);
    
    NSLog(@"getEvent %@", event);
    
    [event enumerateKeysAndObjectsUsingBlock:^(NSString *key,
                                               id val,
                                               BOOL *stop __unused)
     {
         // make sure our keys are part of the cabilities set
         XCTAssert([caps containsObject:key]);
         
         // make sure values are valid
         if ([val isKindOfClass:[NSNumber class]]) {
             XCTAssert([val integerValue]);
         }
     }];
    
    // verify some calculations
    NSInteger temperature = [event[@(kIOPSTemperatureKey)] integerValue];
    XCTAssert(temperature == (NSInteger)(2971/10 - 273.15));
    
    NSInteger rawCap = [event[@(kAppleRawCurrentCapacityKey)] integerValue];
    XCTAssert(rawCap == (NSInteger)(5554/3.6));
    
    ret = (*ups)->setEventCallback(ups, upsCallback, NULL, NULL);
    XCTAssert(ret == kIOReturnSuccess);
    
    // should update configVoltage in output and feature report
    command[@kIOPSCommandSetRequiredVoltageKey] = @123;
    
    // should update configCurrent in output report
    command[@kIOPSCommandSetCurrentLimitKey] = @456;
    
    // should update used in output and feature report
    command[@kIOPSAppleBatteryCaseCommandEnableChargingKey] = @1;
    
    // should update remainingCapacity in output report
    command[@kIOPSCommandSendCurrentStateOfCharge] = @789;
    
    NSLog(@"sendCommand: %@", command);
    
    ret = (*ups)->sendCommand(ups, (__bridge CFDictionaryRef)command);
    XCTAssert(ret == kIOReturnSuccess);
    
    XCTAssert(_outputReport.configVoltage == 123 &&
              _outputReport.configCurrent == 456 &&
              _outputReport.used == 1 &&
              _outputReport.remainingCapacity == 789);
    
    XCTAssert(_featureReport.configVoltage == 123 &&
              _featureReport.used == 1);
    
    _inputReport.configCurrent = 456;
    
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    XCTAssert(_callbackCount);
    
    NSInteger current = [event[@(kIOPSAppleBatteryCaseAvailableCurrentKey)] integerValue];
    XCTAssert(current == 456);

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    if (plugin) {
        (*plugin)->Release(plugin);
    }
    
    if (ups) {
        (*ups)->Release(ups);
    }
}

@end;
