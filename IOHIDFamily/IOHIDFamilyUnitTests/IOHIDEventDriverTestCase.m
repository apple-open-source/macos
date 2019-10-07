//
//  IOHIDEventDriverTestCase.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import "IOHIDEventDriverTestCase.h"
#include <IOKit/hid/IOHIDEventSystemKeys.h>

static IOReturn getReportCallback(void *refcon, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex *reportLength)
{
    NSUInteger length = (NSUInteger)*reportLength;
    IOReturn ret;
  
    IOHIDEventDriverTestCase * self = (__bridge IOHIDEventDriverTestCase *)refcon;
    
    ret = [self userDeviceGetReportHandler:type :reportID :report :&length];
    *reportLength = (CFIndex)length;
    
    return ret;
}

static IOReturn setReportCallback (void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength)
{
    IOReturn ret;
    IOHIDEventDriverTestCase * self = (__bridge IOHIDEventDriverTestCase *)refcon;
    ret = [self userDeviceSetReportHandler:type :reportID :report :reportLength];
    return ret;
}

@implementation IOHIDEventDriverTestCase


- (void)setUp {
    
    [super setUp];
    
    self.events = [[NSMutableArray alloc] init];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.eventSystem = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    if (!self.eventSystem) {
        return;
    }
    NSDictionary *matching = @{
                               @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                               @"Hidden" : @"*"
                               };
    
    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test HID service (uuid:%@)", uniqueID]];

    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);
    
    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
        [self addService:service];
    };
    
    IOHIDEventSystemClientRegisterDeviceMatchingBlock(self.eventSystem , handler, NULL, NULL);
    
    IOHIDEventSystemClientRegisterEventBlock(self.eventSystem, ^(void * _Nullable target __unused, void * _Nullable refcon __unused, void * _Nullable sender __unused, IOHIDEventRef  _Nonnull event) {
        NSLog(@"Event: %@", event);
        [self handleEvent:event fromService:sender];
    }, NULL,  NULL);
    
    
    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystem, dispatch_get_main_queue());
    
    CFTypeRef val = IOHIDEventSystemClientCopyProperty(self.eventSystem, CFSTR (kIOHIDEventSystemClientIsUnresponsive));
    if (val) {
        CFRelease(val);
    }

    self.userDeviceDescription  = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : self.hidDeviceDescriptor,
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   };

 
    NSLog(@"Device description: %@",  self.userDeviceDescription);
    
    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)self.userDeviceDescription);
    if (!self.userDevice) {
        return;
    }

    IOHIDUserDeviceRegisterSetReportCallback(self.userDevice, setReportCallback, (__bridge void * _Nullable)(self));
    IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback(self.userDevice, getReportCallback, (__bridge void * _Nullable)(self));
    IOHIDUserDeviceScheduleWithDispatchQueue(self.userDevice , dispatch_get_main_queue());

}

- (void)tearDown {

    if (self.userDevice) {
        IOHIDUserDeviceUnscheduleFromDispatchQueue(self.userDevice, dispatch_get_main_queue());
        CFRelease(self.userDevice);
    }
    
    if (self.eventSystem) {
        IOHIDEventSystemClientUnscheduleFromDispatchQueue (self.eventSystem, dispatch_get_main_queue());
        CFRelease(self.eventSystem);
    }

    [super tearDown];
}

-(void) addService:(IOHIDServiceClientRef)service
{
    self.eventService = service;
    [self.testServiceExpectation fulfill];
}

-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    [self.events addObject:(__bridge id _Nonnull)(event)];
}

-(IOReturn)userDeviceGetReportHandler: (IOHIDReportType __unused)type :(uint32_t __unused)reportID :(uint8_t * __unused)report :(NSUInteger * __unused) length
{
    return kIOReturnUnsupported;
}

-(IOReturn)userDeviceSetReportHandler: (IOHIDReportType __unused)type :(uint32_t __unused)reportID :(uint8_t * __unused)report :(NSUInteger __unused) length
{
    return kIOReturnUnsupported;
}

@end
