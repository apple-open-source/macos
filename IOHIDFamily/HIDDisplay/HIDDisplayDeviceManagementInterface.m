//
//  HIDDisplayDeviceManagementInterface.m
//  HIDDisplay
//
//  Created by abhishek on 1/6/20.
//

#import "HIDDisplayDeviceManagementInterface.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayInterfacePrivate.h"

#import <AssertMacros.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>

@implementation HIDDisplayDeviceManagementInterface {
    NSDictionary<NSNumber*,HIDElement*>* _usageElementMap;
}

-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    self = [super initWithContainerID:containerID];
    
    require_action(self, exit, os_log_error(HIDDisplayLog(), "Failed to create HIDDisplayDeviceManagementInterface for %@",containerID));
    
    require_action([self setupInterface], exit, os_log_error(HIDDisplayLog(), "Failed to setup HIDDisplayDeviceManagementInterface for %@",containerID));
    
    return self;
exit:
    return nil;
}

-(nullable instancetype) initWithService:(io_service_t) service {
    
    self = [super initWithService:service];
    
    require_action(self, exit, os_log_error(HIDDisplayLog(), "Failed to create HIDDisplayDeviceManagementInterface for %d",(int)service));
    
    require_action([self setupInterface], exit, os_log_error(HIDDisplayLog(), "Failed to setup HIDDisplayDeviceManagementInterface for %d",(int)service));
    
    return self;
exit:
    
    return nil;
}

-(NSArray*) getHIDDevices
{
    NSDictionary *matching = @{@kIOHIDDeviceUsagePairsKey : @[@{
                                                                  @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                                  @kIOHIDDeviceUsageKey: @(kHIDUsage_AppleVendor_DeviceManagement)
                                                                  }]};
    return [self getHIDDevicesForMatching:matching];
    
}

-(BOOL) factoryReset:(uint8_t) type securityToken:(uint64_t) securityToken error:(NSError**) error {
    
    BOOL ret = NO;
    
    os_log(HIDDisplayLog(),"%@ factoryReset type %d",self, (int)type);
    
    HIDElement *factoryResetElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDeviceManagement_FactoryReset)] : NULL;
    
    HIDElement *securityTokenElement =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDeviceManagement_SecurityToken)] : NULL;
    
    if (factoryResetElement && securityTokenElement) {
        factoryResetElement.integerValue = (NSInteger)type;
        securityTokenElement.integerValue = (NSInteger)securityToken;
        ret = [self commit:@[factoryResetElement, securityTokenElement] error:error];
    } else {
        os_log_error(HIDDisplayLog(),"%@ factoryResetElement no associated element factoryReset : %@ securityToken : %@",self, factoryResetElement, securityTokenElement);
    }
    
    return ret;
}

-(BOOL) setupInterface {
    
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDeviceManagement)};
    
    NSDictionary<NSNumber*,HIDElement*>* usageElementMap = [self getDeviceElements:matching];
    
    if (!usageElementMap || usageElementMap.count == 0) {
        return NO;
    }
    
    _usageElementMap = usageElementMap;
    return YES;
}

-(BOOL) getSecurityToken:(uint64_t *)securityToken error:(NSError *__autoreleasing  _Nullable *)error {
    
    HIDElement *securityTokenElement =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDeviceManagement_SecurityToken)] : NULL;
    BOOL ret = NO;
    
    if (!securityToken) {
        
        if (error) {
            *error = [NSError errorWithDomain:@"Invalid Argument" code:kIOReturnBadArgument userInfo:nil];
            return NO;
        }
    }
    
    if (securityTokenElement) {
        if ([self extract:@[securityTokenElement] error:error]) {
            *securityToken = securityTokenElement.integerValue;
            ret = YES;
        }
    } else {
        os_log_error(HIDDisplayLog(),"%@ getSecurityToken no associated element",self);
    }
    
    return ret;
    
}

@end
