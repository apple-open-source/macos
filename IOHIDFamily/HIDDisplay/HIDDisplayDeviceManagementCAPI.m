//
//  HIDDisplayDeviceManagementCAPI.m
//  HIDDisplay
//
//  Created by abhishek on 1/6/20.
//

#import "HIDDisplayDeviceManagementCAPI.h"
#import "HIDDisplayDeviceManagementInterface.h"

HIDDisplayDeviceManagementInterfaceRef __nullable HIDDisplayCreateDeviceManagementInterfaceWithContainerID(CFStringRef containerID) {
    
    HIDDisplayDeviceManagementInterface *device = [[HIDDisplayDeviceManagementInterface alloc] initWithContainerID:(__bridge NSString*)containerID];

    if (!device) {
       return NULL;
    }

    return (__bridge_retained HIDDisplayDeviceManagementInterfaceRef)device;
}

HIDDisplayDeviceManagementInterfaceRef __nullable HIDDisplayCreateDeviceManagementInterfaceWithService(io_service_t service) {
    
    HIDDisplayDeviceManagementInterface *device = [[HIDDisplayDeviceManagementInterface alloc] initWithService:service];
    
    if (!device) {
       return NULL;
    }

    return (__bridge_retained HIDDisplayDeviceManagementInterfaceRef)device;
}

bool HIDDisplayDeviceManagementFactoryReset(HIDDisplayDeviceManagementInterfaceRef interface, HIDDisplayFactoryResetType type, uint64_t securityToken, CFErrorRef *error) {
    
    HIDDisplayDeviceManagementInterface *device = (__bridge HIDDisplayDeviceManagementInterface*)interface;

    NSError *err = nil;

    bool ret = [device factoryReset:(uint8_t)type securityToken:securityToken error:&err];

    if (ret == false && error) {
       *error = (__bridge_retained CFErrorRef)err;
    }
    return ret;
}

bool HIDDisplayDeviceManagementSetFactoryResetData(HIDDisplayDeviceManagementInterfaceRef interface, uint8_t data, uint64_t securityToken, CFErrorRef *error) {
    
    HIDDisplayDeviceManagementInterface *device = (__bridge HIDDisplayDeviceManagementInterface*)interface;

    NSError *err = nil;

    bool ret = [device factoryReset:data securityToken:securityToken error:&err];

    if (ret == false && error) {
       *error = (__bridge_retained CFErrorRef)err;
    }
    return ret;
}

bool HIDDisplayDeviceManagementGetSecurityToken(HIDDisplayDeviceManagementInterfaceRef interface, uint64_t* securityToken, CFErrorRef* error) {
    
    HIDDisplayDeviceManagementInterface *device = (__bridge HIDDisplayDeviceManagementInterface*)interface;

    NSError *err = nil;
    
    bool ret = [device getSecurityToken:securityToken error:&err];
    if (ret == false && error) {
       *error = (__bridge_retained CFErrorRef)err;
    }
    
    
    return ret;
}
