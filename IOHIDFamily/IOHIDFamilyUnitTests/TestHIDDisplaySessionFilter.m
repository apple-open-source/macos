//
//  TestHIDDisplaySessionFilterPlugin.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 1/25/19.
//

#import <XCTest/XCTest.h>
#import <AssertMacros.h>
#import <os/log.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDUserDevice.h>

static NSString * deviceDescription =
@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
"   <plist version=\"1.0\">          "
"   <dict>                           "
"     <key>VendorID</key>            "
"     <integer>855</integer>         "
"     <key>ProductID</key>           "
"     <integer>855</integer>         "
"     <key>ReportInterval</key>      "
"     <integer>10000</integer>       "
"     <key>RequestTimeout</key>      "
"     <integer>5000000</integer>     "
"     <key>UnitTestService</key>     "
"     <true/>                        "
"   </dict>                          "
"   </plist>                         ";

static uint8_t descriptor [] = {
    AppleVendorDisplayPreset
};

#define kTestPresetCount 50
#define kTestDefaultFactoryPresetIndex 3
#define kTestDefaultActivePresetIndex 5

static AppleVendorDisplayFeatureReport03 presetData[kTestPresetCount];
static AppleVendorDisplayFeatureReport02 featureReport2;
static AppleVendorDisplayFeatureReport01 featureReport1;
static NSMutableArray<NSString*> *presetUUIDs;

static void __initPresets()
{
    presetUUIDs = [[NSMutableArray alloc] init];
    
    for (NSInteger i=0; i < kTestPresetCount; i++) {
        
        memset(&presetData[i], 0, sizeof(AppleVendorDisplayFeatureReport03));
        presetData[i].reportId = 0x17;
        presetData[i].AppleVendor_DisplayPresetWritable = (i%3 == 0);
        presetData[i].AppleVendor_DisplayPresetValid =  (i%5 == 0);
        
        NSString *testName = [NSString stringWithFormat:@"Test😀PresetName_%ld",i];
        NSString *testDescription = [NSString stringWithFormat:@"Test😀PresetDescription_%ld",i];
        unichar testNameUnichar[256];
        unichar testDescriptionUnichar[512];
        
        [testName getBytes:testNameUnichar maxLength:256 usedLength:NULL encoding:NSUTF16LittleEndianStringEncoding options:0 range:NSMakeRange(0, 256) remainingRange:NULL];
        
        [testDescription getBytes:testDescriptionUnichar maxLength:512 usedLength:NULL encoding:NSUTF16LittleEndianStringEncoding options:0 range:NSMakeRange(0, 512) remainingRange:NULL];
        
        memcpy(presetData[i].AppleVendor_DisplayPresetUnicodeStringName, testNameUnichar, 256);
        memcpy(presetData[i].AppleVendor_DisplayPresetUnicodeStringDescription, testDescriptionUnichar, 512);
        presetData[i].AppleVendor_DisplayPresetDataBlockOneLength = 64;
        snprintf((char*)presetData[i].AppleVendor_DisplayPresetDataBlockOne, 64, "TestPresetDataBlockOne_%ld",i);
        presetData[i].AppleVendor_DisplayPresetDataBlockTwoLength = 192;
        snprintf((char*)presetData[i].AppleVendor_DisplayPresetDataBlockTwo, 192, "TestPresetDataBlockTwo_%ld",i);
        NSString *uuid = [[[NSUUID alloc] init] UUIDString];
        snprintf((char*)presetData[i].AppleVendor_DisplayPresetUUID, 128, "%s",[uuid UTF8String]);
        [presetUUIDs addObject:uuid];
        
    }
    
    memset(&featureReport1, 0, sizeof(AppleVendorDisplayFeatureReport01));
    memset(&featureReport2, 0, sizeof(AppleVendorDisplayFeatureReport02));
    
    featureReport2.reportId = 0x16;
    featureReport1.reportId = 0x15;
    
    featureReport1.AppleVendor_DisplayFactoryDefaultPresetIndex = kTestDefaultFactoryPresetIndex;
    featureReport1.AppleVendor_DisplayActivePresetIndex = kTestDefaultActivePresetIndex;
    
}


@interface TestHIDDisplaySessionFilterPlugin : XCTestCase

@end

static IOReturn __setReportCallback(void * _Nullable __unused refcon, IOHIDReportType __unused type, uint32_t __unused reportID, uint8_t * report __unused, CFIndex reportLength __unused) {
    
    switch (reportID) {
            
        case 0x15:
            bcopy(report, &featureReport1, sizeof(AppleVendorDisplayFeatureReport01));
            break;
        case 0x16:
            bcopy(report, &featureReport2, sizeof(AppleVendorDisplayFeatureReport02));
            break;
        case 0x17: {
            NSInteger currentIndex = featureReport2.AppleVendor_DisplayCurrentPresetIndex;
            if (currentIndex < kTestPresetCount) {
                bcopy(report, &presetData[currentIndex], sizeof(AppleVendorDisplayFeatureReport03));
            }
            break;
        }
        default:
            break;
    }
    
    return kIOReturnSuccess;
}

static IOReturn __getReportCallback(void * _Nullable __unused refcon __unused, IOHIDReportType type __unused, uint32_t reportID , uint8_t * report , CFIndex reportLength __unused) {
    
    switch (reportID) {
        case 0x15:
            bcopy(&featureReport1, report, reportLength);
            break;
        case 0x16:
            bcopy(&featureReport2, report, reportLength);
            break;
        case 0x17: {
            NSInteger currentIndex = featureReport2.AppleVendor_DisplayCurrentPresetIndex;
            if (currentIndex < kTestPresetCount) {
                bcopy(&presetData[currentIndex], report, reportLength);
            }
            break;
        }
        default:
            break;
    }
    
    return kIOReturnSuccess;
}


@implementation TestHIDDisplaySessionFilterPlugin
{
    IOHIDUserDeviceRef   _userDevice;
    dispatch_queue_t _queue;
}
- (void)setUp {
    
    NSMutableDictionary* deviceConfig = [NSPropertyListSerialization propertyListWithData:[deviceDescription dataUsingEncoding:NSUTF8StringEncoding] options:NSPropertyListMutableContainers format:NULL error:NULL];
    
    NSData *descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    
    _userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)deviceConfig);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _userDevice != NULL);
    
    IOHIDUserDeviceRegisterSetReportCallback(_userDevice, __setReportCallback, NULL);
    IOHIDUserDeviceRegisterGetReportCallback(_userDevice, __getReportCallback, NULL);
    
    _queue = dispatch_queue_create("com.apple.user-device-test", NULL);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _queue != NULL);
    
    IOHIDUserDeviceScheduleWithDispatchQueue (_userDevice, _queue);
    
    __initPresets();
    
}

- (void)tearDown {
    
    if (_userDevice) {
        IOHIDUserDeviceUnscheduleFromDispatchQueue (_userDevice, _queue);
        CFRelease(_userDevice);
    }
}

- (void)testHIDDisplaySessionFilterPlugin {
    
    bool expectation = false;
    IOHIDEventSystemClientRef _eventSystemClient = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _eventSystemClient != NULL);
    
    NSArray *properties = (__bridge_transfer NSArray*)IOHIDEventSystemClientCopyProperty(_eventSystemClient, CFSTR(kIOHIDSessionFilterDebugKey));
    
    for (NSDictionary *property in properties) {
        
        NSString *className = property[@"Class"];
        
        if (className && [className isEqualToString:@"HIDDisplaySessionFilter"]) {
            
            NSArray *status = property[@"Status"];
            expectation = status && status.count != 0;
        }
    }
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, expectation != false);
    
    CFRelease(_eventSystemClient);
    
}

@end
