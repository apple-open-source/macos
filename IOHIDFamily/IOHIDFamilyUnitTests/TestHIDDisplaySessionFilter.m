//
//  TestHIDDisplaySessionFilter.m
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
#import <HID/HID.h>

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

typedef AppleVendorDisplayFeatureReport03 AppleVendorDisplayFeatureReport17;
typedef AppleVendorDisplayFeatureReport02 AppleVendorDisplayFeatureReport16;
typedef AppleVendorDisplayFeatureReport01 AppleVendorDisplayFeatureReport15;

static AppleVendorDisplayFeatureReport17 presetData[kTestPresetCount];
static AppleVendorDisplayFeatureReport15 featureReport15;
static AppleVendorDisplayFeatureReport16 featureReport16;
static NSMutableArray<NSString*> *presetUUIDs;

@interface TestHIDDisplaySessionFilter : XCTestCase
    @property HIDManager         * manager;
    @property HIDUserDevice      * userDevice;
    @property dispatch_queue_t   queue;
    @property XCTestExpectation  * testUserDeviceExpectation;
    @property XCTestExpectation  * testActivePresetExpectation;

@end

@implementation TestHIDDisplaySessionFilter

- (void)setUp {
    
    self.testUserDeviceExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: user device"];
    
    self.testActivePresetExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: active preset device"];

    [self initPresets];

    NSMutableDictionary * deviceConfig = [NSPropertyListSerialization propertyListWithData:[deviceDescription dataUsingEncoding:NSUTF8StringEncoding] options:NSPropertyListMutableContainers format:NULL error:NULL];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    deviceConfig [@kIOHIDUniqueIDKey] = uniqueID;
    deviceConfig [kHIDUserDevicePropertyCreateInactiveKey] = @YES;
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties: deviceConfig];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice);
 
    __weak TestHIDDisplaySessionFilter * _self  = self;

    [self.userDevice setGetReportHandler: ^IOReturn(HIDReportType type,
                                                NSInteger reportID,
                                                void *report,
                                                NSInteger  *reportLength){
        
        NSLog(@"getReportHandler type:%x reportID:%x reportLength:%d", (int)type, (int)reportID, (int)*reportLength);

        switch (reportID) {
            case 0x15:
                *reportLength = sizeof(AppleVendorDisplayFeatureReport15);
                bcopy(&featureReport15, report, sizeof(featureReport15));
                break;
            case 0x16:
                *reportLength = sizeof(AppleVendorDisplayFeatureReport16);
                bcopy(&featureReport16, report, sizeof(featureReport16));
                break;
            case 0x17: {
                *reportLength = sizeof(AppleVendorDisplayFeatureReport17);
                NSInteger currentIndex = featureReport16.AppleVendor_DisplayCurrentPresetIndex;
                if (currentIndex < kTestPresetCount) {
                    bcopy(&presetData[currentIndex], report, sizeof (presetData[currentIndex]));
                     NSLog(@"Preset index:%d data:%@", (int)currentIndex, [NSData dataWithBytes:report length:sizeof (&presetData[currentIndex])]) ;
                }
                break;
            }
            default:
                break;
        }
        
        return kIOReturnSuccess;

    }];

    [self.userDevice setSetReportHandler: ^IOReturn(HIDReportType type,
                                                    NSInteger reportID,
                                                    const void *report,
                                                    NSInteger reportLength){

        NSLog(@"setReportHandler type:%x reportID:%x report:%@ reportLength:%d", (int)type, (int)reportID, [NSData dataWithBytes:report length:reportLength], (int)reportLength);
        
        switch (reportID) {
            case 0x15:
                bcopy(report, &featureReport15, reportLength);
                if (featureReport15.AppleVendor_DisplayActivePresetIndex == featureReport15.AppleVendor_DisplayFactoryDefaultPresetIndex ) {
                    [_self.testActivePresetExpectation fulfill];
                }
                break;
            case 0x16:
                bcopy(report, &featureReport16, reportLength);
                break;
            case 0x17: {
                NSInteger currentIndex = featureReport16.AppleVendor_DisplayCurrentPresetIndex;
                if (currentIndex < kTestPresetCount) {
                    bcopy(report, &presetData[currentIndex], reportLength);
                }
                break;
            }
            default:
                break;
        }
        return kIOReturnSuccess;

    }];

    self.queue = dispatch_queue_create("com.apple.user-device-test", NULL);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _queue != NULL);
    
    [self.userDevice setDispatchQueue:self.queue];
    
    [self.userDevice activate];
    
    self.manager = [[HIDManager alloc] init];

    NSDictionary *matching = @{@kIOHIDDeviceUsagePairsKey : @[@{
                                                     @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                     @kIOHIDDeviceUsageKey: @(kHIDUsage_AppleVendor_Display)
    }]};

    [self.manager setDeviceMatching:matching];

    [self.manager setDeviceNotificationHandler:^(HIDDevice * _Nonnull device, BOOL added) {
        if (added) {
            NSLog(@"HID device:%@", device);
            [_self.testUserDeviceExpectation fulfill];
        }
    }];
    
    [self.manager setDispatchQueue: dispatch_get_main_queue()];
    
    [self.manager activate];
}

- (void)tearDown {
    
    [self.userDevice cancel];

    [self.manager cancel];

}

- (void)testHIDDisplaySessionFilter {
    
    bool expectation = false;
    
    HIDEventSystemClient * eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, eventSystemClient);
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[self.testUserDeviceExpectation] timeout:10];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                result == XCTWaiterResultCompleted,
                                "result:%ld  %@",(long)result, self.testUserDeviceExpectation);

    

    result = [XCTWaiter waitForExpectations:@[self.testActivePresetExpectation] timeout:10];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                result == XCTWaiterResultCompleted,
                                "result:%ld  %@",(long)result, self.testActivePresetExpectation);

    
    NSArray * filtersProperties =  (NSArray *) [eventSystemClient propertyForKey:@(kIOHIDSessionFilterDebugKey)];
    
    for (NSDictionary * filterProperties in filtersProperties) {
        
        NSDictionary * pluginProperties = filterProperties[@"plugin"];
        
        if (pluginProperties && pluginProperties[@"Class"] && [pluginProperties[@"Class"] isEqualToString:@"HIDDisplaySessionFilter"]) {
            NSArray *status = pluginProperties[@"Status"];
            expectation = status && status.count != 0;
        }
    }
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_LOGARCHIVE | COLLECT_IOREG, expectation , "%s = %@", kIOHIDSessionFilterDebugKey, filtersProperties);
    

}

- (void) initPresets {
    
    presetUUIDs = [[NSMutableArray alloc] init];
    
    for (NSInteger i = 0; i < kTestPresetCount; i++) {
        
        memset(&presetData[i], 0, sizeof(AppleVendorDisplayFeatureReport17));
        presetData[i].reportId = 0x17;
        presetData[i].AppleVendor_DisplayPresetWritable = 0;
        presetData[i].AppleVendor_DisplayPresetValid    = 1;
        
        NSString * testName = [NSString stringWithFormat:@"Test😀PresetName_%ld",i];
        NSString * testDescription = [NSString stringWithFormat:@"Test😀PresetDescription_%ld",i];
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
    
    memset(&featureReport15, 0, sizeof(AppleVendorDisplayFeatureReport15));
    memset(&featureReport16, 0, sizeof(AppleVendorDisplayFeatureReport16));
    
    featureReport15.reportId = 0x15;
    featureReport16.reportId = 0x16;
    
    featureReport15.AppleVendor_DisplayFactoryDefaultPresetIndex = kTestDefaultFactoryPresetIndex;
    featureReport15.AppleVendor_DisplayActivePresetIndex = kTestDefaultActivePresetIndex;
}

@end
