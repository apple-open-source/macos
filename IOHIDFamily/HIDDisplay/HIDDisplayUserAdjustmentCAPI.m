//
//  HIDDisplayUserAdjustmentCAPI.m
//  HIDDisplay
//
//  Created by abhishek on 1/15/20.
//

#import "HIDDisplayUserAdjustmentCAPI.h"
#import "HIDDisplayUserAdjustmentInterface.h"


CFStringRef kHIDDisplayUserAdjustmentDescriptionKey = CFSTR("UserAdjustmentDescription");
CFStringRef kHIDDisplayUserAdjustmentInformationKey = CFSTR("UserAdjustmentInformation");

HIDDisplayUserAdjustmentInterfaceRef __nullable HIDDisplayCreateUserAdjustmentInterfaceWithContainerID(CFStringRef containerID) {
    
    HIDDisplayUserAdjustmentInterface *device = [[HIDDisplayUserAdjustmentInterface alloc] initWithContainerID:(__bridge NSString*)containerID];

    if (!device) {
       return NULL;
    }

    return (__bridge_retained HIDDisplayUserAdjustmentInterfaceRef)device;
}

HIDDisplayUserAdjustmentInterfaceRef __nullable HIDDisplayCreateUserAdjustmentInterfaceWithService(io_service_t service) {
    
    HIDDisplayUserAdjustmentInterface *device = [[HIDDisplayUserAdjustmentInterface alloc] initWithService:service];
    
    if (!device) {
       return NULL;
    }

    return (__bridge_retained HIDDisplayUserAdjustmentInterfaceRef)device;
}

bool HIDDisplayUserAdjustmentSetData(HIDDisplayUserAdjustmentInterfaceRef interface, CFDictionaryRef data, CFErrorRef* error) {
    
    HIDDisplayUserAdjustmentInterface *device = (__bridge HIDDisplayUserAdjustmentInterface*)interface;

    NSError *err = nil;
    bool ret = false;
    
    ret = [device set:(__bridge NSDictionary*)data error:&err];
    if (ret == false && error) {
       *error = (__bridge_retained CFErrorRef)err;
    }
    
    return ret;

}

CFDictionaryRef _Nullable HIDDisplayUserAdjustmentCopyData(HIDDisplayUserAdjustmentInterfaceRef interface, CFErrorRef* error) {
    
    HIDDisplayUserAdjustmentInterface *device = (__bridge HIDDisplayUserAdjustmentInterface*)interface;
    NSDictionary *data = nil;
    NSError *err = nil;
    
    data = [device get:&err];
    if (!data && error) {
        *error = (__bridge_retained CFErrorRef)err;
    }
    return (__bridge_retained CFDictionaryRef)data;
}

bool HIDDisplayUserAdjustmentIsValid(HIDDisplayUserAdjustmentInterfaceRef interface) {
    
    HIDDisplayUserAdjustmentInterface *device = (__bridge HIDDisplayUserAdjustmentInterface*)interface;
    
    return [device valid];
}

bool HIDDisplayUserAdjustmentInvalidate(HIDDisplayUserAdjustmentInterfaceRef interface, CFErrorRef* error) {
    
    HIDDisplayUserAdjustmentInterface *device = (__bridge HIDDisplayUserAdjustmentInterface*)interface;
    NSError *err = nil;
    bool ret = false;
    ret = [device invalidate:&err];
    if (ret == false && error) {
       *error = (__bridge_retained CFErrorRef)err;
    }
    
    return ret;
}
