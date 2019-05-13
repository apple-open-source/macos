//
//  TestHIDDisplayFramework.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 1/11/19.
//

#import <XCTest/XCTest.h>
#import <HIDDisplay/HIDDisplay.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDUserDevice.h>
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
        
        NSString *testName = [NSString stringWithFormat:@"Test😊PresetName_%ld",i];
        NSString *testDescription = [NSString stringWithFormat:@"Test😊PresetDescription_%ld",i];
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

@interface TestHIDDisplayFramework : XCTestCase
{
    IOHIDUserDeviceRef   _userDevice;
    HIDDisplayDeviceRef  _hidDisplayDevice;
    dispatch_queue_t     _queue;
    NSString             *_containerID;
    XCTMemoryChecker     *_memoryChecker;
    NSUInteger            _presetCount;
}
@property XCTestExpectation  *testSetReportExpectation;
@property XCTestExpectation  *testGetReportExpectation;
@property NSInteger           getReportCount;
@property NSInteger           setReportCount;

@end

static IOReturn __setReportCallback(void * _Nullable refcon, IOHIDReportType __unused type, uint32_t __unused reportID, uint8_t * report __unused, CFIndex reportLength __unused) {
    
    TestHIDDisplayFramework *selfRef = (__bridge TestHIDDisplayFramework*)refcon;
    selfRef.setReportCount++;
    if (selfRef.setReportCount == 50) {
        [selfRef.testSetReportExpectation fulfill];
    }
    
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

static IOReturn __getReportCallback(void * _Nullable refcon __unused, IOHIDReportType type __unused, uint32_t reportID , uint8_t * report , CFIndex reportLength __unused) {
    
    
    TestHIDDisplayFramework *selfRef = (__bridge TestHIDDisplayFramework*)refcon;
    selfRef.getReportCount++;
    if (selfRef.getReportCount == 52) {
        [selfRef.testGetReportExpectation fulfill];
    }
    
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

@implementation TestHIDDisplayFramework

- (void)setUp {
    
    [super setUp];
    
    _containerID = [[[NSUUID alloc] init] UUIDString];
    
    _memoryChecker = [[XCTMemoryChecker alloc]initWithDelegate:self];
    
    NSMutableDictionary* deviceConfig = [NSPropertyListSerialization propertyListWithData:[deviceDescription dataUsingEncoding:NSUTF8StringEncoding] options:NSPropertyListMutableContainers format:NULL error:NULL];
    
    NSData *descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    
#if !TARGET_OS_IPHONE || TARGET_OS_IOSMAC
    deviceConfig [@kUSBDeviceContainerID] = _containerID;
#else
    deviceConfig [@kUSBContainerID] = _containerID;
#endif
    
    _userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)deviceConfig);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _userDevice != NULL);
    
    IOHIDUserDeviceRegisterSetReportCallback(_userDevice, __setReportCallback, (__bridge void*)self);
    IOHIDUserDeviceRegisterGetReportCallback(_userDevice, __getReportCallback, (__bridge void*)self);
    
    _queue = dispatch_queue_create("com.apple.user-device-test", NULL);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _queue != NULL);
    
    IOHIDUserDeviceScheduleWithDispatchQueue (_userDevice, _queue);
    
    __initPresets();
    
    // Expectation count for set report
    // 50 ( 50 for preset select)
    // Expection count for get report
    // 52 (50 for copy preset + 2 for factory and active preset)
    
    _testSetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: set reports"];
    _testGetReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: get reports"];
    
}

- (void)tearDown {
    
    IOHIDUserDeviceUnscheduleFromDispatchQueue (_userDevice, _queue);
    CFRelease(_userDevice);
    
}

// test initial info on presets
-(void) TestPresetDefaultInfo
{
    _presetCount = (NSUInteger)HIDDisplayGetPresetCount(_hidDisplayDevice);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST , _presetCount == kTestPresetCount);
    
    NSInteger factoryDefaultIndex  = HIDDisplayGetFactoryDefaultPresetIndex(_hidDisplayDevice, NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST , factoryDefaultIndex == kTestDefaultFactoryPresetIndex);
    
    NSInteger activePresetIndex = HIDDisplayGetActivePresetIndex(_hidDisplayDevice, NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST , activePresetIndex == kTestDefaultActivePresetIndex || activePresetIndex == kTestDefaultFactoryPresetIndex);
    
    
    for (NSUInteger i=0; i < _presetCount; i++) {
        
        NSDictionary *info = (__bridge_transfer NSDictionary*)HIDDisplayCopyPreset(_hidDisplayDevice,i, nil);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , info != nil);
        
        NSString* name = info[(__bridge NSString*)kHIDDisplayPresetFieldNameKey];
        NSString* desc = info[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey];
        NSString* dataBlockOne = [NSString stringWithUTF8String:[info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey] bytes]];
        NSNumber *dataBlockOneLength = info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey];
        NSString* dataBlockTwo = [NSString stringWithUTF8String:[info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey] bytes]];
        NSNumber *dataBlockTwoLength = info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey];
        NSString* uuid = info[(__bridge NSString*)kHIDDisplayPresetUniqueIDKey];
        
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , dataBlockOneLength.integerValue == 64);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , dataBlockTwoLength.integerValue == 192);
        
        NSString *expectedName = [NSString stringWithFormat:@"Test😊PresetName_%ld",i];
        NSString *expectedDesc = [NSString stringWithFormat:@"Test😊PresetDescription_%ld",i];
        NSString *expectedDataBlockOne = [NSString stringWithFormat:@"TestPresetDataBlockOne_%ld",i];
        NSString *expectedDataBlockTwo = [NSString stringWithFormat:@"TestPresetDataBlockTwo_%ld",i];
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , name && [name hasPrefix:expectedName]);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , desc && [desc hasPrefix:expectedDesc]);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , dataBlockOne && [expectedDataBlockOne isEqualToString:dataBlockOne]);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , dataBlockTwo && [expectedDataBlockTwo isEqualToString:dataBlockTwo]);
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST , uuid && [uuid isEqualToString:presetUUIDs[i]]);
        
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
    for (NSInteger i=0; i < _presetCount; i++) {
        
        bool isValid = HIDDisplayIsPresetValid(_hidDisplayDevice, i);
        //valid preset (check __initPreset for description)
        if (i%5 == 0) {
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , isValid == true);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , HIDDisplaySetActivePresetIndex(_hidDisplayDevice, i, NULL) == true);
            
        } else {
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , isValid == false);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , HIDDisplaySetActivePresetIndex(_hidDisplayDevice, i, NULL) == false);
        }
        
        //writable preset, valid preset
        if (i%3 == 0 && i%5 == 0) {
            
            //invalidate preset
            NSDictionary *info = @{(__bridge NSString*)kHIDDisplayPresetFieldValidKey : @(0)};
            bool ret = HIDDisplaySetPreset(_hidDisplayDevice, i, (__bridge CFDictionaryRef)info, nil);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , ret == true);
            isValid = HIDDisplayIsPresetValid(_hidDisplayDevice, i);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , isValid == false);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , HIDDisplaySetActivePresetIndex(_hidDisplayDevice, i, NULL) == false);
            
            
            NSString *expectedName = [NSString stringWithFormat:@"ModifiedTestPreset😊Name_%ld",i];
            NSString *expectedDesc = [NSString stringWithFormat:@"ModifiedTestPreset😊Description_%ld",i];

            NSString *expectedDataBlockOne = [NSString stringWithFormat:@"ModifiedTestPresetDataBlockOne_%ld",i];
            NSString *expectedDataBlockTwo = [NSString stringWithFormat:@"ModifiedTestPresetDataBlockTwo_%ld",i];
            NSString *expectedUUID = [[[NSUUID alloc] init] UUIDString];
            
            
            info = @{
                     
                     (__bridge NSString*)kHIDDisplayPresetFieldNameKey : expectedName,
                     (__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey : expectedDesc,
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey : [expectedDataBlockOne dataUsingEncoding:NSUTF8StringEncoding],
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey : [expectedDataBlockTwo dataUsingEncoding:NSUTF8StringEncoding],
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey : @(12+i),
                     (__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey : @(16+i),
                     (__bridge NSString*)kHIDDisplayPresetUniqueIDKey : expectedUUID,
                     
                     };
            
            ret = HIDDisplaySetPreset(_hidDisplayDevice, i, (__bridge CFDictionaryRef)info, nil);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , ret == true);
            
            info = (__bridge_transfer NSDictionary*)HIDDisplayCopyPreset(_hidDisplayDevice,i, nil);
            
            
            NSString* name = info[(__bridge NSString*)kHIDDisplayPresetFieldNameKey];
            NSString* desc = info[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey];
            
            NSString* dataBlockOne = [NSString stringWithUTF8String:[info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey] bytes]];
            
            NSString* dataBlockTwo = [NSString stringWithUTF8String:[info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey] bytes]];
            
            NSNumber *dataBlockOneLength = info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey];
            
            NSNumber *dataBlockTwoLength = info[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey];
            
            NSString *uniqueID = info[(__bridge NSString*)kHIDDisplayPresetUniqueIDKey];
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , name && [name containsString:expectedName]);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , desc && [desc containsString:expectedDesc]);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , dataBlockOne && [expectedDataBlockOne isEqualToString:dataBlockOne]);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , dataBlockTwo && [expectedDataBlockTwo isEqualToString:dataBlockTwo]);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , (unsigned long)dataBlockOneLength.integerValue == 12+i);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , (unsigned long)dataBlockTwoLength.integerValue == 16+i);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , uniqueID && [expectedUUID isEqualToString:uniqueID]);
            
        }
        
        //non writable preset
        if (i%3 !=0) {
            NSDictionary *info = @{(__bridge NSString*)kHIDDisplayPresetFieldValidKey : @(0)};
            bool ret = HIDDisplaySetPreset(_hidDisplayDevice, i, (__bridge CFDictionaryRef)info, nil);
            HIDXCTAssertWithParameters (RETURN_FROM_TEST , ret == false);
        }
        
    }
}
- (void)testHIDDisplayFramework {
    
    
    [_memoryChecker assertObjectsOfTypes:@[@"HIDDisplayDevice", @"HIDDisplayDevicePreset",@"IOHIDDevice",@"HIDElement",@"IOHIDTransaction", @"IOHIDManager"] invalidAfterScope:^{
        
        @autoreleasepool {
            
            _hidDisplayDevice = HIDDisplayCreateDeviceWithContainerID((__bridge CFStringRef)_containerID);
            
            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _hidDisplayDevice != NULL);
            
            [self TestPresetDefaultInfo];
            
            [self TestSetPresetInfo];
            
            CFRelease(_hidDisplayDevice);
        }
    }];
    
}

@end
