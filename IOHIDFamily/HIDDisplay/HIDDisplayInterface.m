//
//  HIDDisplayInterface.m
//  HIDDisplay
//
//  Created by AB on 1/10/19.
//

#import "HIDDisplayInterface.h"
#import "HIDElement.h"
#import "HIDDisplayInterfacePrivate.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayCAPI.h"

#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#if !TARGET_OS_SIMULATOR
#import <IOKit/usb/USBSpec.h>
#endif
#import <AssertMacros.h>

#import <HID/HIDManager.h>
#import <HID/HIDTransaction.h>
#import <HID/HIDDevice.h>
#import <HID/HIDElement.h>
#import <TargetConditionals.h>
#import <IOKit/IOKitLib.h>

@implementation HIDDisplayInterface
{
    HIDManager *_manager;
    HIDDevice *_deviceRef;
}

-(NSString*) description
{
    return [NSString stringWithFormat:@"[regID:0x%llx][containerID:%@][instance:%p]",_registryID, _containerID, self];
}
-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    
    self = [super init];
    if (!self) {
        return nil;
    }
    
#if !TARGET_OS_SIMULATOR
    NSArray *hidDevices = [self getHIDDevices];
    
    if (!hidDevices || hidDevices.count == 0) {
        os_log_error(HIDDisplayLog(), "Failed to get valid hid device for %@",containerID);
        return nil;
    }
    
    
    
    BOOL matchingDevice = NO;
    for (NSUInteger i = 0; i < hidDevices.count; i++) {
        matchingDevice = [self hasMatchingContainerID:[hidDevices objectAtIndex:i] containerID:containerID];
        if (matchingDevice == YES) {
            self.device = [hidDevices objectAtIndex:i];
            break;
        }
    }
    
    if (matchingDevice == NO) {
        return nil;
    }
    
    [self.device open];
    
    _containerID = containerID;
    
    IORegistryEntryGetRegistryEntryID(self.device.service, &_registryID);
    
    os_log(HIDDisplayLog(), "%@ Init",self);
    
    return self;
#else
    (void)containerID;
    return nil;
#endif
    
}

-(nullable instancetype) initWithService:(io_service_t) service
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    HIDDevice *device = [[HIDDevice alloc] initWithService:service];
    if (!device) {
        return nil;
    }
    
    self.device = device;
    
    [self.device open];
    
    IORegistryEntryGetRegistryEntryID(service, &_registryID);
    
    _containerID = [self extractContainerIDFromService:service];
    
    os_log(HIDDisplayLog(), "%@ Init",self);
    
    return self;
}

-(nullable instancetype) initWithMatching:(NSDictionary*) matching
{
    os_log_info(HIDDisplayLog(), "%s", __FUNCTION__);
    
    self = [super init];
    if (!self) {
        return nil;
    }
    
    NSArray *hidDevices = [self getHIDDevicesForMatching:matching];
    
    if (!hidDevices || hidDevices.count == 0) {
        return nil;
    }
    
    self.device = [hidDevices objectAtIndex:0];
    
    [self.device open];
    
    IORegistryEntryGetRegistryEntryID(self.device.service, &_registryID);
    
    _containerID = [self extractContainerIDFromService:self.device.service];
    
    os_log(HIDDisplayLog(), "%@ Init", self);
    
    return self;
}

-(void) dealloc
{
    
    os_log(HIDDisplayLog(), "%@ Dealloc",self);
    
    if (self.device){
        [self.device close];
    }
}

// not using containerID property as multiple call's by caller can be made
// so we may want to cache containerID on init
-(NSString*) extractContainerIDFromService:(io_service_t) service
{
    NSString *containerIDKey = nil;
    id containerIdValue  = nil;
    require(service != IO_OBJECT_NULL, exit);
    
#if !TARGET_OS_IPHONE || TARGET_OS_IOSMAC
    containerIDKey = @kUSBDeviceContainerID;
#elif !TARGET_OS_SIMULATOR
    containerIDKey = @kUSBContainerID;
#endif
    
    containerIdValue =  (__bridge_transfer id)IORegistryEntrySearchCFProperty(service, kIOServicePlane, (__bridge CFStringRef)containerIDKey, kCFAllocatorDefault, kIORegistryIterateParents | kIORegistryIterateRecursively);
    
    if (!containerIdValue || ![containerIdValue isKindOfClass:[NSString class]]) return nil;
    
    return (NSString*)containerIdValue;
exit:
    return nil;
}


-(BOOL) hasMatchingContainerID:(HIDDevice*) device containerID:(NSString*) containerID
{
    io_service_t service = device.service;
    BOOL ret = NO;
    NSString* containerIdValue  = nil;
    uint64_t registryID = 0;
    NSString *ctrIDOne  = nil;
    NSString *ctrIDTwo = nil;
    
    IORegistryEntryGetRegistryEntryID(service, &registryID);
    
    containerIdValue = [self extractContainerIDFromService:service];
    
    require(containerIdValue, exit);
    
    
    //convert both to lower case to avoid any confusion
    ctrIDOne = containerID.lowercaseString;
    ctrIDTwo = containerIdValue.lowercaseString;
    
    if ([ctrIDOne containsString:ctrIDTwo] || [ctrIDTwo containsString:ctrIDOne]) {
        ret = YES;
    }
    
    os_log(HIDDisplayLog(), "%@ Container ID Match for %@ returned %s",self, containerIdValue, ret ? "Success" : "Failure");
    
    
exit:
    return ret;
}


-(NSDictionary<NSNumber*,HIDElement*>*) getDeviceElements:(NSDictionary*) matching
{
    NSMutableDictionary<NSNumber*,HIDElement*> *usageElementMap = [[NSMutableDictionary alloc] init];
    
    NSArray<HIDElement*> *elements =  [self.device elementsMatching:matching];
    BOOL ret = NO;
    require(elements, exit);
    
    for (HIDElement *element in elements) {
        
        NSInteger usage = element.usage;
        NSInteger usagePage = element.usagePage;
        os_log_info(HIDDisplayLog(), "%@ Display Device Element UP: 0x%lx , U: 0x%lx ",self, usagePage, usage);
        usageElementMap[@(usage)] = element;
        ret = YES;
    }
    
exit:
    return ret == YES ? usageElementMap : nil;
}

-(NSArray*) getHIDDevicesForMatching:(NSDictionary*) matching
{
    if (!_manager) {
        _manager = [[HIDManager alloc] init];
    }
    
    require(_manager, exit);
    
    [_manager setDeviceMatching:matching];
    
    return _manager.devices;
    
exit:
    return nil;
}

-(NSArray*) getHIDDevices
{
    return nil;
}

-(HIDDevice*) device
{
    return _deviceRef;
}

-(void) setDevice:(HIDDevice*) device
{
    _deviceRef = device;
}

-(BOOL) commit:(NSArray<HIDElement*>*) elements error:(NSError**) error
{
    BOOL ret = YES;
    
    ret = [self.device commitElements:elements direction:HIDDeviceCommitDirectionOut error:error];
    
    if (!ret) {
        os_log_error(HIDDisplayLog(), "%@ Failed Set HID Elements values with error %@",self, error ? *error : @"Undefined");
    }
    
    return ret;
}

-(BOOL) extract:(NSArray<HIDElement*>*) elements error:(NSError**) error
{
    BOOL ret = YES;
    
    ret = [self.device commitElements:elements direction:HIDDeviceCommitDirectionIn error:error];
    
    if (!ret) {
        os_log_error(HIDDisplayLog(), "%@ Failed Get HID Elements values with error %@", self, error ? *error : @"Undefined");
    }
    
    return ret;
}

-(nullable NSArray<NSString*>*) capabilities
{
    return nil;
}

@end

