//
//  HIDDisplayInterface.m
//  HIDDisplay
//
//  Created by AB on 1/10/19.
//

#import "HIDDisplayInterface.h"
#import "HIDDisplayPresetData.h"
#import "HIDElement.h"
#import "HIDElementPrivate.h"
#import "HIDDisplayCAPI.h"
#import "HIDDisplayPresetDataPrivate.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayInterfacePrivate.h"

#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <IOKit/usb/USBSpec.h>
#import <AssertMacros.h>
#import <IOKit/hid/IOHIDLib.h>
#import <TargetConditionals.h>
#import <IOKit/IOKitKeys.h>

@implementation HIDDisplayInterface
{
    IOHIDManagerRef _manager;
    IOHIDDeviceRef _deviceRef;
}
-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    NSArray *hidDevices = [self getHIDDevices];
    
    if (!hidDevices || hidDevices.count == 0) {
        os_log_error(HIDDisplayLog(), "Failed to get valid hid device for %@",containerID);
        return nil;
    }
    
    BOOL matchingDevice = NO;
    for (NSUInteger i = 0; i < hidDevices.count; i++) {
        matchingDevice = [self hasMatchingContainerID:(__bridge IOHIDDeviceRef)[hidDevices objectAtIndex:i] containerID:containerID];
        if (matchingDevice == YES) {
            self.device = (__bridge_retained IOHIDDeviceRef)[hidDevices objectAtIndex:i];
            break;
        }
    }
    
    if (matchingDevice == NO) {
        return nil;
    }
    
    IOReturn kr = IOHIDDeviceOpen(self.device, 0);
    
    if (kr) {
        os_log(OS_LOG_DEFAULT, "Device open status 0x%x",kr);
        return nil;
    }
    
    _containerID = containerID;
    
    return self;
}

-(void) dealloc
{
    if (_manager) {
        
        CFRelease(_manager);
        _manager = nil;
    }
    
    if (self.device) {
        IOHIDDeviceClose(self.device, 0);
        CFRelease(self.device);
        self.device = nil;
    }
}

-(BOOL) hasMatchingContainerID:(IOHIDDeviceRef) device containerID:(NSString*) containerID
{
    io_service_t service = IOHIDDeviceGetService(device);
    
    uint64_t registryID =0;
    
    BOOL ret = NO;
    id containerIdValue  = nil;
    NSString *containerIDKey = nil;
    require(service != IO_OBJECT_NULL, exit);
    
    IORegistryEntryGetRegistryEntryID(service, &registryID);
    
    os_log(HIDDisplayLog(), "Registry ID 0x%llx containerID %@",registryID, containerID);
    
#if !TARGET_OS_IPHONE || TARGET_OS_IOSMAC
    containerIDKey = @kUSBDeviceContainerID;
#else
    containerIDKey = @kUSBContainerID;
#endif
    
    containerIdValue =  (__bridge_transfer id)IORegistryEntrySearchCFProperty(service, kIOServicePlane, (__bridge CFStringRef)containerIDKey, kCFAllocatorDefault, kIORegistryIterateParents | kIORegistryIterateRecursively);
    
    require(containerIdValue, exit);
    
    if ([containerIdValue isKindOfClass:[NSString class]]) {
         
        os_log(HIDDisplayLog(), "IORegistryEntrySearchCFProperty (NSString) result %@ length %ld",containerIdValue, containerIdValue ? ((NSString*)containerIdValue).length : 0);
        
        //convert both to lower case to avoid any confusion
        NSString *ctrIDOne = containerID.lowercaseString;
        NSString *ctrIDTwo = ((NSString*)containerIdValue).lowercaseString;
        
        if ([ctrIDOne containsString:ctrIDTwo] || [ctrIDTwo containsString:ctrIDOne]) {
            ret = YES;
            os_log(HIDDisplayLog(), "Container ID Match for %@",containerID);
        }
    }
    
exit:
    return ret;
}

-(NSArray*) getHIDDevicesForMatching:(NSDictionary*) matching
{
    NSSet *devices = nil;
    NSArray *tmp = nil;
    
    if (!_manager) {
        _manager = IOHIDManagerCreate (kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    }
    
    require(_manager, exit);
    
    IOHIDManagerSetDeviceMatching(_manager, (__bridge CFDictionaryRef)matching);
    
    devices =  (__bridge_transfer NSSet*)IOHIDManagerCopyDevices(_manager);
    
    require(devices, exit);
    
    tmp = [devices allObjects];
exit:
    return tmp;
}

-(NSArray*) getHIDDevices
{
    return nil;
}

-(nullable instancetype) initWithMatching:(NSDictionary*) matching
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    NSArray *hidDevices = [self getHIDDevicesForMatching:matching];
    
    if (!hidDevices || hidDevices.count == 0) {
        return nil;
    }
    
    self.device = (__bridge_retained IOHIDDeviceRef)[hidDevices objectAtIndex:0];
    
    IOReturn kr = IOHIDDeviceOpen(self.device, 0);
    
    if (kr) {
        os_log(HIDDisplayLog(), "Device open status 0x%x",kr);
        return nil;
    }
    
    return self;
}

-(NSDictionary<NSNumber*,HIDElement*>*) getDeviceElements:(NSDictionary*) matching
{
    NSArray *elements = (__bridge_transfer NSArray*)IOHIDDeviceCopyMatchingElements(self.device, (__bridge CFDictionaryRef)matching, 0);
    BOOL ret = NO;
    NSMutableDictionary<NSNumber*,HIDElement*> *usageElementMap = [[NSMutableDictionary alloc] init];
    
    require(elements, exit);
    
    usageElementMap = [[NSMutableDictionary alloc] init];
    
    for (NSUInteger i = 0; i < elements.count; i++ ) {
        
        IOHIDElementRef element = (__bridge IOHIDElementRef)[elements objectAtIndex:i];
        uint32_t usage = IOHIDElementGetUsage(element);
        HIDElement *displayElement = [[HIDElement alloc] initWithObject:element];
        
        if (displayElement) {
            usageElementMap[@(usage)] = displayElement;
            ret = YES;
        }
    }
exit:
    return ret == YES ? usageElementMap : nil;
}
-(IOHIDDeviceRef) device
{
    return _deviceRef;
}

-(void) setDevice:(IOHIDDeviceRef) device
{
    _deviceRef = device;
}

-(BOOL) commit:(NSArray<HIDElement*>*) elements error:(NSError**) error
{
    BOOL ret = NO;
    IOReturn kr = kIOReturnSuccess;
    IOHIDTransactionRef transaction = NULL;
    
    transaction = IOHIDTransactionCreate(kCFAllocatorDefault, self.device, kIOHIDTransactionDirectionTypeOutput, kIOHIDOptionsTypeNone);
    
    require_action(transaction, exit, kr = kIOReturnError; os_log_error(HIDDisplayLog(),"[containerID:%@] Unable to create transaction",self.containerID));
    
    for (HIDElement *element in elements) {
        
        IOHIDTransactionAddElement(transaction,element.element);
        IOHIDTransactionSetValue(transaction,element.element, element.valueRef, kIOHIDOptionsTypeNone);
        
    }
    
    kr = IOHIDTransactionCommit(transaction);
    
    require_action(kr == kIOReturnSuccess, exit, os_log_error(HIDDisplayLog(),"[containerID:%@] HIDTransactionCommit error : 0x%x",self.containerID, kr));
    
    ret = YES;
    
exit:
    
    if (ret == NO && error) {
        *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kr userInfo:nil];
    }
    
    if (transaction) {
        CFRelease(transaction);
    }
    
    return ret;
}

-(BOOL) extract:(NSArray<HIDElement*>*) elements error:(NSError**) error
{
    IOReturn kr = kIOReturnSuccess;
    BOOL ret = NO;
    IOHIDTransactionRef transaction = NULL;
    
    transaction = IOHIDTransactionCreate(kCFAllocatorDefault, self.device, kIOHIDTransactionDirectionTypeInput, kIOHIDOptionsTypeNone);
    
    require_action(transaction, exit, kr = kIOReturnError; os_log_error(HIDDisplayLog(),"[containerID:%@] Unable to create transaction",self.containerID));
    
    for (HIDElement *element in elements) {
        IOHIDTransactionAddElement(transaction,element.element);
    }
    
    kr = IOHIDTransactionCommit(transaction);
    require_action(kr == kIOReturnSuccess, exit, os_log_error(HIDDisplayLog(),"[containerID:%@] HIDTransactionExtract error : 0x%x",self.containerID, kr));
    
    
    for (HIDElement *element in elements) {
        element.valueRef = IOHIDTransactionGetValue(transaction,element.element,kIOHIDOptionsTypeNone);
    }
    
    ret = YES;
exit:
    
    if (ret == NO && error) {
        *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kr userInfo:nil];
    }
    
    if (transaction) {
        CFRelease(transaction);
    }
    
    return ret;
}

-(nullable NSArray<NSString*>*) capabilities
{
    return nil;
}

@end

