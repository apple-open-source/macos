//
//  IOHIDEventDriverTestCase.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import "IOHIDEventDriverTestCase.h"


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

}

- (void)tearDown {

    if (self.userDevice) {
        CFRelease(self.userDevice);
    }
    
    if (self.eventSystem) {
        CFRelease(self.eventSystem);
    }

    [super tearDown];
}

-(void) addService:(IOHIDServiceClientRef)service
{
    self.eventService = service;
    [self.testServiceExpectation fulfill];
}

-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service;
{
    [self.events addObject:(__bridge id _Nonnull)(event)];
}


@end
