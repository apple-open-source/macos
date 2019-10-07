//
//  TestHIDDevice.m
//  IOHIDFamily
//
//  Created by yg on 1/5/17.
//
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDEventSystemTestController.h"
#import "IOHIDUserDeviceTestController.h"
#import "IOHIDDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDLibPrivate.h>


static uint8_t descriptorForKeyboard[] = {
    HIDKeyboardDescriptor
};

static IOReturn UserDeviceHandleReportAsyncCallback (void * _Nullable refcon, IOReturn result);

static uint8_t descriptorForElements[] = {
    HIDVendorDescriptor
};


static void GetReportAsync (
                     void * _Nullable        context,
                     IOReturn                result,
                     void * _Nullable        sender,
                     IOHIDReportType         type,
                     uint32_t                reportID,
                     uint8_t *               report,
                     CFIndex                 reportLength);


static void SetReportAsync (
                     void * _Nullable        context,
                     IOReturn                result,
                     void * _Nullable        sender,
                     IOHIDReportType         type,
                     uint32_t                reportID,
                     uint8_t *               report,
                     CFIndex                 reportLength);

@interface TestHIDDevice : XCTestCase <IOHIDUserDeviceObserver> 

@property dispatch_queue_t                  deviceControllerQueue;
@property dispatch_queue_t                  sourceControllerQueue;
@property dispatch_queue_t                  rootQueue;
@property NSString *                        elementUniqueID;
@property NSString *                        keyboardUniqueID;
@property IOHIDUserDeviceTestController *   elementController;
@property IOHIDUserDeviceTestController *   keyboardController;

@property IOHIDDeviceTestController *       keyboardDeviceController;
@property IOHIDDeviceTestController *       elementDeviceController;

@end

@implementation TestHIDDevice {
    NSInteger                         setReportCount[4];
    NSInteger                         getReportCount[4];
    NSInteger                         getAsyncReportCount[4];
    NSInteger                         setAsyncReportCount[4];
    HIDVendorDescriptorFeatureReport  featureReport;
    CFIndex                           featureReportLength;
    uint64_t                          timestamp;
    CFIndex                           handleAsyncReportCount;
}

- (void)setUp {
    [super setUp];

    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    self.deviceControllerQueue = dispatch_queue_create_with_target ("IOHIDDeviceTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.deviceControllerQueue != nil);

    self.sourceControllerQueue = dispatch_queue_create_with_target ("IOHIDUserDeviceTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.sourceControllerQueue != nil);
    
    self.keyboardUniqueID = [[[NSUUID alloc] init] UUIDString];
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptorForKeyboard length:sizeof(descriptorForKeyboard)];

    self.keyboardController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:self.keyboardUniqueID andQueue:self.sourceControllerQueue];
    HIDXCTAssertAndThrowTrue(self.keyboardController != nil);

    self.keyboardDeviceController = [[IOHIDDeviceTestController alloc] initWithDeviceUniqueID:self.keyboardUniqueID :CFRunLoopGetCurrent()];
    HIDXCTAssertAndThrowTrue(self.keyboardDeviceController != nil);

    self.elementUniqueID = [[[NSUUID alloc] init] UUIDString];
    descriptorData = [[NSData alloc] initWithBytes:descriptorForElements length:sizeof(descriptorForElements)];
    
    self.elementController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:self.elementUniqueID andQueue:self.sourceControllerQueue];
    HIDXCTAssertAndThrowTrue(self.elementController != nil);
    
    self.elementDeviceController = [[IOHIDDeviceTestController alloc] initWithDeviceUniqueID:self.elementUniqueID :CFRunLoopGetCurrent()];
    HIDXCTAssertAndThrowTrue(self.elementDeviceController != nil);
    

}

- (void)tearDown {
    [self.keyboardController invalidate];
    [self.elementController invalidate];
    [self.elementDeviceController invalidate];
    [self.keyboardDeviceController invalidate];
    
    @autoreleasepool {
        self.keyboardController = nil;
        self.elementController = nil;
        self.elementDeviceController = nil;
        self.keyboardDeviceController = nil;
    }
    [super tearDown];
}



- (void)testOpenByEventDriver {
    
    dispatch_queue_t systemControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(systemControllerQueue != nil);

    IOHIDEventSystemTestController * eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.keyboardUniqueID AndQueue:systemControllerQueue];
    HIDXCTAssertAndThrowTrue(eventController != nil);

    CFTypeRef prop = IOHIDDeviceGetProperty(self.keyboardDeviceController.device, CFSTR(kIOHIDDeviceOpenedByEventSystemKey));
    XCTAssertTrue(prop != NULL, "kIOHIDDeviceOpenedByEventSystemKey does not exist");
    
    if (prop) {
        CFRelease(prop);
    }
    
    [eventController invalidate];
    
    @autoreleasepool {
        eventController  = nil;
    }

}

- (void)testIOHIDUserDeviceCopyService {
    
    io_service_t service = IOHIDUserDeviceCopyService (self.keyboardController.userDevice);
    XCTAssertTrue (service != IO_OBJECT_NULL, "IOHIDUserDeviceGetService:%d", service);
    
    CFTypeRef phisicalUUID =  IORegistryEntryCreateCFProperty(service, CFSTR (kIOHIDPhysicalDeviceUniqueIDKey) , kCFAllocatorDefault, 0);
    
    XCTAssertTrue(phisicalUUID && CFEqual(phisicalUUID, (__bridge CFStringRef)self.keyboardController.userDeviceUniqueID), "UUID does not match");
    CFRelease(phisicalUUID);
    
    if (service) {
        IOObjectRelease(service);
    }
}



- (void)testHIDUserDeviceHandleReport {
   
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        IOReturn status = kIOReturnSuccess;
        
        HIDVendorDescriptorInputReport report;

        NSData * reportData;
        
        report.VEN_VendorDefined0081 = 0x11111111;
        reportData = [[NSData alloc] initWithBytes: &report length:sizeof (report)];
        
        status = [self.elementController handleReport: reportData withInterval: 2000];
        XCTAssert (status == kIOReturnSuccess, "handleReport: 0x%x", status);
        
        report.VEN_VendorDefined0081 = 0x22222222;
        reportData = [[NSData alloc] initWithBytes: &report length:sizeof (report)];
        
        status = [self.elementController handleReportAsync: reportData Callback:UserDeviceHandleReportAsyncCallback Context:(__bridge void *)(self)];
        XCTAssert (status == kIOReturnSuccess, "handleReportAsync: 0x%x", status);
    });
    
    
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 2.0, false);

    
    XCTAssert(handleAsyncReportCount == 1);
    
    NSArray *values = [self.elementDeviceController.values copy];

    XCTAssert(values && values.count == 2, "Invalid reports %@", values);
    
}

- (void)testHIDUserDeviceHandleReportWithTimestamp {
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        HIDVendorDescriptorInputReport report;
        
        IOReturn status = kIOReturnSuccess;
        NSData * reprortData;
        
        timestamp = mach_absolute_time ();
        
        report.VEN_VendorDefined0081 = 0x11111111;
        reprortData = [[NSData alloc] initWithBytes: &report length:sizeof (report)];
        
        status = [self.elementController handleReport: reprortData withTimestamp: timestamp];
        XCTAssert (status == kIOReturnSuccess, "handleReport: 0x%x", status);
        
        report.VEN_VendorDefined0081 = 0x22222222;
        reprortData = [[NSData alloc] initWithBytes: &report length:sizeof (report)];
        
        status = [self.elementController handleReportAsync: reprortData withTimestamp: timestamp Callback:UserDeviceHandleReportAsyncCallback Context:(__bridge void *)(self)];
        XCTAssert (status == kIOReturnSuccess, "handleReport: 0x%x", status);
    });
    
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 2.0, false);
    
    XCTAssert(handleAsyncReportCount == 1);
 
    NSArray *values = [self.elementDeviceController.values copy];
    
    XCTAssert(values && values.count == 2, "Invalid reports %@", values);
    
}


- (void)testHIDDevice {
    
    IOReturn status;
    
    NSDictionary * matching;
    NSArray * elements;
    IOHIDElementRef element;
    IOHIDValueRef elementValue;
    
    HIDVendorDescriptorInputReport report;
    
    NSData * reportData;

    report.VEN_VendorDefined0081 = 0x11111111;
    reportData = [[NSData alloc] initWithBytes: &report length:sizeof (report)];
    
    status = [self.elementController handleReport: reportData withInterval: 2000];
    XCTAssert (status == kIOReturnSuccess, "handleReport: 0x%x", status);

    
    XCTAssert(IOHIDDeviceConformsTo (self.elementDeviceController.device, kHIDPage_AppleVendor, 0x80) == true);
    
    CFTypeRef value = IOHIDDeviceGetProperty (self.elementDeviceController.device, CFSTR(kIOHIDPhysicalDeviceUniqueIDKey));
    XCTAssert(value && CFEqual (value, (CFStringRef)self.elementUniqueID) == true);
 
    CFStringRef testPropertyKey = CFSTR("TestDeviceProperty");
    IOHIDDeviceSetProperty (self.elementDeviceController.device, testPropertyKey, testPropertyKey);
    value = IOHIDDeviceGetProperty (self.elementDeviceController.device, testPropertyKey);
    XCTAssert(value && CFEqual (value, testPropertyKey) == true);
    
    matching = @{@kIOHIDElementUsagePageKey:@(kHIDPage_AppleVendor), @kIOHIDElementUsageKey:@(0x81)};
    elements = (NSArray *)CFBridgingRelease(IOHIDDeviceCopyMatchingElements (self.elementDeviceController.device, (CFDictionaryRef)matching, 0));
    XCTAssert(elements && elements.count == 1);
    
    elementValue = NULL;
    element = (__bridge IOHIDElementRef)elements[0];
    status = IOHIDDeviceGetValue (self.elementDeviceController.device, element, &elementValue);
    XCTAssert(status == kIOReturnSuccess && elementValue != NULL, "IOHIDDeviceGetValue: %x", status);
    CFIndex elementValueInteger = IOHIDValueGetIntegerValue (elementValue);
    XCTAssert(elementValueInteger == 0x11111111);
    
    matching = @{@kIOHIDElementUsagePageKey:@(kHIDPage_AppleVendor), @kIOHIDElementUsageKey:@(0x83)};
    elements = (NSArray *)CFBridgingRelease(IOHIDDeviceCopyMatchingElements (self.elementDeviceController.device, (CFDictionaryRef)matching, 0));
    XCTAssert(elements && elements.count == 1);
    
    element = (__bridge IOHIDElementRef)elements[0];
    XCTAssert(IOHIDElementGetTypeID() == CFGetTypeID(element));
    XCTAssert(IOHIDElementGetUsagePage (element) == kHIDPage_AppleVendor);
    XCTAssert(IOHIDElementGetUsage (element) == 0x83);
    XCTAssert(IOHIDElementGetType (element) == kIOHIDElementTypeFeature);
    IOHIDElementRef tempElement = IOHIDElementGetParent (element);
    XCTAssert (tempElement != NULL);
    XCTAssert(IOHIDElementGetChildren(tempElement) != NULL);
    XCTAssert(IOHIDElementGetDevice (element) == self.elementDeviceController.device);
    XCTAssert(IOHIDElementGetCollectionType (tempElement) == kIOHIDElementCollectionTypeApplication);
    
    elementValue = IOHIDValueCreateWithIntegerValue (kCFAllocatorDefault, element, mach_absolute_time(), 0x55555555);
    XCTAssert (elementValue != NULL);
    
    status = IOHIDDeviceSetValue(self.elementDeviceController.device, element, elementValue);
    XCTAssert(status == kIOReturnSuccess || status == kIOReturnUnsupported, "IOHIDDeviceSetValue: %x", status);
    CFRelease(elementValue);
}

- (void)testElementExponent {
    IOHIDElementRef element = NULL;
    IOHIDElementStruct elementStruct = { 0 };
    IOHIDValueRef value = NULL;
    
    CFDataRef dummyData = CFDataCreateMutable(kCFAllocatorDefault, 1);
    XCTAssert(dummyData);
    
    element = _IOHIDElementCreateWithParentAndData(kCFAllocatorDefault, NULL, dummyData, &elementStruct, 0);
    XCTAssert(element);
    
    // size of 1 byte to hold value
    elementStruct.size = 1;
    
    // logical min/max
    elementStruct.min = 0;
    elementStruct.max = 100;
    
    // physical min/max
    elementStruct.scaledMin = 0;
    elementStruct.scaledMax = 100;
    
    // 10^3
    elementStruct.unitExponent = 0x3;
    
    value = IOHIDValueCreateWithIntegerValue(kCFAllocatorDefault, element, 0, 1);
    XCTAssert(value);
    
    double result = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypeExponent);
    XCTAssert(result == 1000);
    
    // 10^-3
    elementStruct.unitExponent = 0xD;
    
    result = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypeExponent);
    XCTAssert(result == .001);
    
    CFRelease(element);
    CFRelease(value);
    CFRelease(dummyData);
}

- (void)testHIDDeviceReport {
    IOReturn status = kIOReturnSuccess;
    
    CFIndex reportLength;

    self.elementController.userDeviceObserver = self;
    
  
    HIDVendorDescriptorOutputReport outputReport;

    for (int index = 0; index < 10; index++) {
        status = IOHIDDeviceSetReport (self.elementDeviceController.device, kIOHIDReportTypeOutput, 0,  (uint8_t*)&outputReport, sizeof(outputReport));
        XCTAssert (status == kIOReturnSuccess, "IOHIDDeviceSetReport : 0x%x", status);
    }

    sleep (1);
    
    XCTAssert (self->setReportCount[kIOHIDReportTypeOutput] == 10);
    
    for (int index = 0; index < 10; index++) {
        status = IOHIDDeviceSetReport (self.elementDeviceController.device, kIOHIDReportTypeFeature, 0, (uint8_t*)&featureReport, sizeof (featureReport));
        XCTAssert (status == kIOReturnSuccess, "IOHIDDeviceSetReport : 0x%x", status);
    }
    
    sleep (1);
    
    XCTAssert (self->setReportCount[kIOHIDReportTypeFeature] == 10);
 
    featureReportLength = sizeof (featureReport);
    for (int index = 0; index < 10; index++) {
        status = IOHIDDeviceGetReport (self.elementDeviceController.device, kIOHIDReportTypeFeature, 0,  (uint8_t*)&featureReport, &featureReportLength);
        XCTAssert (status == kIOReturnSuccess, "IOHIDDeviceGetReport (kIOHIDReportTypeFeature) : 0x%x", status);
    }

    XCTAssert (self->setReportCount[kIOHIDReportTypeFeature] == 10);
    
    HIDVendorDescriptorInputReport inputReport;
    reportLength = sizeof (inputReport);
    
    for (int index = 0; index < 10; index++) {
        status = IOHIDDeviceGetReport (self.elementDeviceController.device, kIOHIDReportTypeInput, 0,  (uint8_t*)&inputReport, &reportLength);
        XCTAssert (status == kIOReturnSuccess, "IOHIDDeviceGetReport (kIOHIDReportTypeInput) : 0x%x", status);
    }
    
    XCTAssert (self->setReportCount[kIOHIDReportTypeFeature] == 10);

    featureReportLength = sizeof (featureReport);
    status = kIOReturnSuccess;
    for (int index = 0; index < 10 && status == kIOReturnSuccess; index++) {
        status = IOHIDDeviceGetReportWithCallback (self.elementDeviceController.device, kIOHIDReportTypeFeature, 0,  (uint8_t*)&featureReport, &featureReportLength, 1.0, GetReportAsync, (__bridge void * _Nonnull)(self));
    }

    sleep (1);

    XCTAssert(status == kIOReturnUnsupported ||  (status == kIOReturnSuccess && getAsyncReportCount[kIOHIDReportTypeFeature] == 10),
              "IOHIDDeviceGetReportWithCallback status:%x, report count:%ld", status, (long)getAsyncReportCount[kIOHIDReportTypeFeature]
              );

    featureReportLength = sizeof (featureReport);
    status = kIOReturnSuccess;
    for (int index = 0; index < 10 && status == kIOReturnSuccess; index++) {
        status = IOHIDDeviceSetReportWithCallback (self.elementDeviceController.device, kIOHIDReportTypeFeature, 0,  (uint8_t*)&featureReport, featureReportLength, 1.0, SetReportAsync, (__bridge void * _Nonnull)(self));
    }
    
    sleep (1);
    
    XCTAssert(status == kIOReturnUnsupported ||  (status == kIOReturnSuccess && getAsyncReportCount[kIOHIDReportTypeFeature] == 10),
              "IOHIDDeviceGetReportWithCallback status:%x, report count:%ld", status, (long)getAsyncReportCount[kIOHIDReportTypeFeature]
              );

}

-(IOReturn) GetReportCallback: (IOHIDReportType) type : (uint32_t __unused) reportID  : (uint8_t * __unused) report : (CFIndex * __unused) reportLength {
    ++self->getReportCount[type];
    return kIOReturnSuccess;
}

-(IOReturn) SetReportCallback: (IOHIDReportType) type : (uint32_t __unused) reportID  : (uint8_t * __unused) report  : (CFIndex __unused) reportLength  {
    XCTAssert (type == kIOHIDReportTypeOutput || type == kIOHIDReportTypeFeature );
    XCTAssert (reportID == 0);
    ++self->setReportCount[type];
    return kIOReturnSuccess;
}

void GetReportAsync (void * _Nullable        context,
                     IOReturn                result,
                     void * _Nullable        sender         __unused,
                     IOHIDReportType         type,
                     uint32_t                reportID       __unused,
                     uint8_t *               report         __unused,
                     CFIndex                 reportLength   __unused
                     ) {
    TestHIDDevice *self = (__bridge TestHIDDevice *)context;
    XCTAssert(result==kIOReturnSuccess);
    ++self->getAsyncReportCount[type];
}

void SetReportAsync (void * _Nullable        context,
                     IOReturn                result,
                     void * _Nullable        sender         __unused,
                     IOHIDReportType         type,
                     uint32_t                reportID       __unused,
                     uint8_t *               report         __unused,
                     CFIndex                 reportLength   __unused
                     ) {
    TestHIDDevice *self = (__bridge TestHIDDevice *)context;
    XCTAssert(result==kIOReturnSuccess);
    ++self->setAsyncReportCount[type];
}

IOReturn UserDeviceHandleReportAsyncCallback (void * _Nullable refcon, IOReturn result) {
    TestHIDDevice *self = (__bridge TestHIDDevice *)refcon;
    XCTAssert(result==kIOReturnSuccess);
    ++self->handleAsyncReportCount;
    return result;
}

@end
