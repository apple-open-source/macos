//
//  TestHIDDisplayFramework.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 1/11/19.
//

#import <XCTest/XCTest.h>
#import <HIDDisplay/HIDDisplay.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <HID/HID.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <XCTest/XCTMemoryChecker.h>
#import <IOKit/usb/USBSpec.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <TargetConditionals.h>

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

#define kTestPresetCount 50
#define kTestDefaultFactoryPresetIndex 3
#define kTestDefaultActivePresetIndex 5

static uint8_t descriptor [] = {
    AppleVendorDisplayPreset
};

typedef AppleVendorDisplayFeatureReport03 AppleVendorDisplayFeatureReport17;
typedef AppleVendorDisplayFeatureReport02 AppleVendorDisplayFeatureReport16;
typedef AppleVendorDisplayFeatureReport01 AppleVendorDisplayFeatureReport15;

static AppleVendorDisplayFeatureReport17 presetData[kTestPresetCount];
static AppleVendorDisplayFeatureReport15 featureReport15;
static AppleVendorDisplayFeatureReport16 featureReport16;

static NSMutableArray<NSString*> *presetUUIDs;

@interface TestHIDDisplayFramework : XCTestCase

@property HIDDisplayDeviceRef   hidDisplayDevice;
@property HIDUserDevice         * userDevice;
@property dispatch_queue_t      queue;
@property XCTestExpectation     * testSetReportExpectation;
@property XCTestExpectation     * testGetReportExpectation;
@property NSInteger             getReportCount;
@property NSInteger             setReportCount;
@property NSUInteger            presetCount;
@property NSString              * containerID;
@property XCTMemoryChecker      * memoryChecker;

@end

@implementation TestHIDDisplayFramework

- (void)setUp {
    
    [super setUp];
    
    // Expectation count for set report
    // 50 ( 50 for preset select)
    // Expection count for get report
    // 52 (50 for copy preset + 2 for factory and active preset)
    
    _testSetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: set reports"];
    _testGetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: get reports"];

    [self initPresets];
    
    _containerID = [[[NSUUID alloc] init] UUIDString];
    
    _memoryChecker = [[XCTMemoryChecker alloc]initWithDelegate:self];
    
    NSMutableDictionary * deviceConfig = [NSPropertyListSerialization propertyListWithData:[deviceDescription dataUsingEncoding:NSUTF8StringEncoding] options:NSPropertyListMutableContainers format:NULL error:NULL];
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    deviceConfig [kHIDUserDevicePropertyCreateInactiveKey] = @YES;

#if !TARGET_OS_IPHONE || TARGET_OS_IOSMAC
     deviceConfig [@kUSBDeviceContainerID] = _containerID;
#else
     deviceConfig [@kUSBContainerID] = _containerID;
#endif
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties: deviceConfig];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE  | COLLECT_HIDUTIL | COLLECT_IOREG, self.userDevice);
    
    __weak TestHIDDisplayFramework * _self  = self;
    
    [self.userDevice setGetReportHandler: ^IOReturn(HIDReportType type,
                                                NSInteger reportID,
                                                void *report,
                                                NSInteger  *reportLength){
        
        NSLog(@"getReportHandler type:%x reportID:%x reportLength:%d", (int)type, (int)reportID, (int)*reportLength);

        _self.getReportCount++;
        if (_self.getReportCount == 52) {
            [_self.testGetReportExpectation fulfill];
        }

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
                                                    NSInteger reportLength) {

        NSLog(@"setReportHandler type:%x reportID:%x report:%@ reportLength:%d", (int)type, (int)reportID, [NSData dataWithBytes:report length:reportLength], (int)reportLength);

        _self.setReportCount++;
        if (_self.setReportCount == 50) {
            [_self.testSetReportExpectation fulfill];
        }

        switch (reportID) {
            case 0x15:
                bcopy(report, &featureReport15, reportLength);
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
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.queue);
    
    [self.userDevice setDispatchQueue:self.queue];
    
    [self.userDevice activate];
}

- (void)tearDown {
    
    [self.userDevice cancel];
    
}

// test initial info on presets
-(void) TestPresetDefaultInfo
{
    _presetCount = (NSUInteger)HIDDisplayGetPresetCount(_hidDisplayDevice);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG , _presetCount == kTestPresetCount,"%lu", _presetCount);
    
    NSUInteger factoryDefaultIndex  = HIDDisplayGetFactoryDefaultPresetIndex(_hidDisplayDevice, NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG , factoryDefaultIndex == kTestDefaultFactoryPresetIndex,"%lu",factoryDefaultIndex);
    
    NSInteger activePresetIndex = HIDDisplayGetActivePresetIndex(_hidDisplayDevice, NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, activePresetIndex == kTestDefaultActivePresetIndex || activePresetIndex == kTestDefaultFactoryPresetIndex, "default index : %d active index : %lu", kTestDefaultActivePresetIndex, activePresetIndex);
    
    
    for (NSUInteger i = 0; i < _presetCount; i++) {
        
        NSDictionary *info = (__bridge_transfer NSDictionary*)HIDDisplayCopyPreset(_hidDisplayDevice,i, nil);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, info != nil);
        
        NSString * name = info[(__bridge NSString*)kHIDDisplayPresetFieldNameKey];
        NSString * desc = info[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey];
        NSString * dataBlockOne = [NSString stringWithUTF8String:[info[(__bridge NSString *)kHIDDisplayPresetFieldDataBlockOneKey] bytes]];
        NSNumber * dataBlockOneLength = info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey];
        NSString * dataBlockTwo = [NSString stringWithUTF8String:[info[(__bridge NSString *)kHIDDisplayPresetFieldDataBlockTwoKey] bytes]];
        NSNumber *dataBlockTwoLength = info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey];

        
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, dataBlockOneLength.integerValue == 64, "%ld", (long)dataBlockOneLength.integerValue);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE |  COLLECT_HIDUTIL | COLLECT_IOREG, dataBlockTwoLength.integerValue == 192, "%ld", (long)dataBlockTwoLength.integerValue);
        
        
        NSString * expectedName = [NSString stringWithFormat:@"TestPresetName😀😀😀_%ld",i];
        NSString * expectedDesc = [NSString stringWithFormat:@"TestPresetDescription😀😀😀😀😀_%ld",i];
        NSString * expectedDataBlockOne = [NSString stringWithFormat:@"TestPresetDataBlockOne_%ld",i];
        NSString * expectedDataBlockTwo = [NSString stringWithFormat:@"TestPresetDataBlockTwo_%ld",i];
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG , name && [name hasPrefix:expectedName], "name:  %@ expected name :%@ length name : %lu expected name length : %ld",name, expectedName, name ? name.length : 0, expectedName ? expectedName.length : 0);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG , desc && [desc hasPrefix:expectedDesc], "desc:  %@ expected desc :%@ length desc : %lu expected desc length : %ld",desc, expectedDesc, desc ? desc.length : 0, expectedDesc ? expectedDesc.length : 0);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG , dataBlockOne && [expectedDataBlockOne isEqualToString:dataBlockOne], "dataBlockOne:  %@ expected dataBlockOne :%@ length dataBlockOne : %lu expected dataBlockOne length : %ld",dataBlockOne, expectedDataBlockOne, dataBlockOne ? dataBlockOne.length : 0, expectedDataBlockOne ? expectedDataBlockOne.length : 0);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG , dataBlockTwo && [expectedDataBlockTwo isEqualToString:dataBlockTwo], "dataBlockTwo:  %@ expected dataBlockTwo :%@ length dataBlockTwo : %lu expected dataBlockTwo length : %ld",dataBlockTwo, expectedDataBlockTwo, dataBlockTwo ? dataBlockTwo.length : 0, expectedDataBlockTwo ? expectedDataBlockTwo.length : 0);
        
        
    }
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_testSetReportExpectation,_testGetReportExpectation] timeout:10];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                result == XCTWaiterResultCompleted,
                                "result:%ld  count get %ld count set %ld %@ %@",
                                (long)result, _getReportCount, _setReportCount,
                                _testSetReportExpectation,_testGetReportExpectation);
}

// modify preset info
-(void) TestSetPresetInfo
{
    for (NSUInteger i = 0; i < _presetCount; i++) {
        
        bool isValid = HIDDisplayIsPresetValid(_hidDisplayDevice, i);
        //valid preset (check __initPreset for description)
        if (i < 10) {
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, isValid == true,"index : %lu",i);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, HIDDisplaySetActivePresetIndex(_hidDisplayDevice, i, NULL) == true, "index : %lu",i);
            
        } else {
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, isValid == false, "index : %lu",i);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, HIDDisplaySetActivePresetIndex(_hidDisplayDevice, i, NULL) == false, "index : %lu",i);
        }
        
        //writable preset, valid preset
        if (i >=10) {
            
            NSDictionary *info = nil;
            bool ret = false;
            NSString *expectedName = [NSString stringWithFormat:@"User : 😀😀😀😀😀😀😀     😎 Let's 😎 do HID 😎😎😎 test $ _%lu",i];
            NSString *expectedDesc = [NSString stringWithFormat:@"ModifiedTestPresetDescription😀😀😀😀_%ld",i];
            NSString *expectedDataBlockOne = [NSString stringWithFormat:@"ModifiedTestPresetDataBlockOne_%ld",i];
            NSString *expectedDataBlockTwo = [NSString stringWithFormat:@"ModifiedTestPresetDataBlockTwo_%ld",i];
            
            
            info = @{
                     
                     (__bridge NSString*)kHIDDisplayPresetFieldNameKey : expectedName,
                     (__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey : expectedDesc,
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey : [expectedDataBlockOne dataUsingEncoding:NSUTF8StringEncoding],
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey : [expectedDataBlockTwo dataUsingEncoding:NSUTF8StringEncoding],
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey : @(12+i),
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey : @(16+i),
                     (__bridge NSString*)kHIDDisplayPresetFieldValidKey : i%3 == 0 ? @YES : @NO,
                     
                     };

            ret = HIDDisplaySetPreset(_hidDisplayDevice, i, (__bridge CFDictionaryRef)info, nil);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, ret == true);
            
            info = (__bridge_transfer NSDictionary*)HIDDisplayCopyPreset(_hidDisplayDevice,i, nil);
            
            
            NSString * name = info[(__bridge NSString*)kHIDDisplayPresetFieldNameKey];
            NSLog(@"%@",name);
            NSString * desc = info[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey];
            NSLog(@"%@",desc);
            
            NSString * dataBlockOne = [NSString stringWithUTF8String:[info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey] bytes]];
            NSLog(@"%@",dataBlockOne);
            NSString * dataBlockTwo = [NSString stringWithUTF8String:[info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey] bytes]];
            NSLog(@"%@",dataBlockTwo);
            NSNumber * dataBlockOneLength = info[(__bridge NSString *)kHIDDisplayPresetFieldDataBlockOneLengthKey];
            
            NSNumber * dataBlockTwoLength = info[(__bridge NSString *)kHIDDisplayPresetFieldDataBlockTwoLengthKey];
            NSNumber *valid = info[(__bridge NSString *)kHIDDisplayPresetFieldValidKey];
            NSNumber *writable = info[(__bridge NSString *)kHIDDisplayPresetFieldWritableKey];
            
            NSLog(@"Valid %ld Writable %ld",valid.integerValue, writable.integerValue);
            if (i%3) {
                HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, valid.integerValue == 0);
            } else {
                HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, valid.integerValue == 1);
            }
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, writable.integerValue == 1);
            
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, name && [name containsString:expectedName], "name:  %@ expected name :%@ length name : %lu expected name length : %ld",name, expectedName, name ? name.length : 0, expectedName ? expectedName.length : 0);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, desc && [desc containsString:expectedDesc],"desc:  %@ expected desc :%@ length desc : %lu expected desc length : %ld",desc, expectedDesc, desc ? desc.length : 0, expectedDesc ? expectedDesc.length : 0 );
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, dataBlockOne && [expectedDataBlockOne isEqualToString:dataBlockOne], "dataBlockOne:  %@ expected dataBlockOne :%@ length dataBlockOne : %lu expected dataBlockOne length : %ld",dataBlockOne, expectedDataBlockOne, dataBlockOne ? dataBlockOne.length : 0, expectedDataBlockOne ? expectedDataBlockOne.length : 0);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, dataBlockTwo && [expectedDataBlockTwo isEqualToString:dataBlockTwo], "dataBlockTwo:  %@ expected dataBlockTwo :%@ length dataBlockTwo : %lu expected dataBlockTwo length : %ld",dataBlockTwo, expectedDataBlockTwo, dataBlockTwo ? dataBlockTwo.length : 0, expectedDataBlockTwo ? expectedDataBlockTwo.length : 0);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, (unsigned long)dataBlockOneLength.integerValue == 12+i, "data block one length : %lu expected  : %lu",(unsigned long)dataBlockOneLength.integerValue, 12+i);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, (unsigned long)dataBlockTwoLength.integerValue == 16+i, "data block two length : %lu expected : %lu",(unsigned long)dataBlockTwoLength.integerValue,16+i);
            
        }
        
    }
}
- (void)testHIDDisplayFramework {
    
    
    [_memoryChecker assertObjectsOfTypes:@[@"HIDDisplayDevice", @"HIDDisplayDevicePreset"] invalidAfterScope:^{
        
        @autoreleasepool {
            
            _hidDisplayDevice = HIDDisplayCreateDeviceWithContainerID((__bridge CFStringRef)_containerID);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST  | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, _hidDisplayDevice != NULL);
            
            [self TestPresetDefaultInfo];
            
            [self TestSetPresetInfo];
            
            CFRelease(_hidDisplayDevice);
        }
    }];
    
}

-(void) dumpBytes:(NSData*) data {
    if (!data) return;
    const uint8_t *bytes = (const uint8_t*)[data bytes];
    for (NSUInteger i=0; i  < data.length; i++) {
        printf("%02x\t",bytes[i]);
    }
    printf("\n");
}

- (void) initPresets {
    presetUUIDs = [[NSMutableArray alloc] init];
    
    // First 10 presets are valid and non writable
    // remaining are invalid and writable
    for (NSInteger i = 0; i < kTestPresetCount; i++) {
        
        memset(&presetData[i], 0, sizeof(AppleVendorDisplayFeatureReport17));
        presetData[i].reportId = 0x17;
        presetData[i].AppleVendor_DisplayPresetWritable = i < 10 ? 0 : 1;
        presetData[i].AppleVendor_DisplayPresetValid =  i < 10 ? 1 : 0;
        
        NSString * testName = [NSString stringWithFormat:@"TestPresetName😀😀😀_%ld",i];
        NSString * testDescription = [NSString stringWithFormat:@"TestPresetDescription😀😀😀😀😀_%ld",i];
        NSData * testNameData = [testName dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
        NSData * testDescriptionData = [testDescription dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
               
        // FW would copy all data , so this should be validated here
        memcpy(presetData[i].AppleVendor_DisplayPresetUnicodeStringName, [testNameData bytes], 256);
        memcpy(presetData[i].AppleVendor_DisplayPresetUnicodeStringDescription, [testDescriptionData  bytes], 512);
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
