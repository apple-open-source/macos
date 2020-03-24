//
//  HIDDisplayPresetCAPI.m
//  HIDDisplay
//
//  Created by AB on 4/22/19.
//

#include "HIDDisplayPresetCAPI.h"
#include "HIDDisplayPresetInterface.h"
#include "HIDDisplayPresetData.h"
#include "HIDDisplayPresetInterfacePrivate.h"
#include "HIDDisplayPrivate.h"
#include <IOKit/IOReturn.h>

CFStringRef kHIDDisplayPresetFieldWritableKey = CFSTR("PresetWritable");
CFStringRef kHIDDisplayPresetFieldValidKey = CFSTR("PresetValid");
CFStringRef kHIDDisplayPresetFieldNameKey = CFSTR("PresetName");
CFStringRef kHIDDisplayPresetFieldDescriptionKey = CFSTR("PresetDescription");
CFStringRef kHIDDisplayPresetFieldDataBlockOneLengthKey =  CFSTR("PresetDataBlockOneLength");
CFStringRef kHIDDisplayPresetFieldDataBlockOneKey =  CFSTR("PresetDataBlockOne");
CFStringRef kHIDDisplayPresetFieldDataBlockTwoLengthKey =  CFSTR("PresetDataBlockTwoLength");
CFStringRef kHIDDisplayPresetFieldDataBlockTwoKey =  CFSTR("PresetDataBlockTwo");
CFStringRef kHIDDisplayPresetUniqueIDKey = CFSTR("PresetUniqueID");

HIDDisplayDeviceRef __nullable HIDDisplayCreateDeviceWithContainerID(CFStringRef containerID)
{
    return (HIDDisplayDeviceRef)HIDDisplayCreatePresetInterfaceWithContainerID(containerID);
}

HIDDisplayPresetInterfaceRef __nullable HIDDisplayCreatePresetInterfaceWithContainerID(CFStringRef containerID)
{
    HIDDisplayPresetInterface *device = [[HIDDisplayPresetInterface alloc] initWithContainerID:(__bridge NSString*)containerID];
    
    if (!device) {
        return NULL;
    }
    
    return (__bridge_retained HIDDisplayPresetInterfaceRef)device;
}

CFIndex HIDDisplayGetPresetCount(HIDDisplayPresetInterfaceRef hidDisplayInterface)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return -1;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    return _device.presets.count;
}


CFIndex HIDDisplayGetFactoryDefaultPresetIndex(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFErrorRef* error)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return -1;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    NSError *err = nil;
    
    CFIndex index = [_device getFactoryDefaultPresetIndex:&err];
    
    if (index == -1) {
        os_log_error(HIDDisplayLog(),"%@ HIDDisplayGetFactoryDefaultPresetIndex error %@ ",_device, err);
    }
    
    if (index == -1 && error) {
        *error = (__bridge_retained CFErrorRef)err;
    }
    
    return index;
}

CFIndex HIDDisplayGetActivePresetIndex(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFErrorRef* error)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return -1;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    NSError *err = nil;
    
    CFIndex index = (CFIndex)[_device getActivePresetIndex:&err];
    
    if (index == -1) {
        os_log_error(HIDDisplayLog(),"%@ HIDDisplayGetActivePresetIndex error %@ ",_device, err);
    }
    
    if (index == -1 && error) {
        *error = (__bridge_retained CFErrorRef)err;
    }
    
    return index;
}

bool HIDDisplaySetActivePresetIndex(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex, CFErrorRef* error)
{
    id device = (__bridge id)hidDisplayInterface;
    bool ret = true;
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        
        if (error) {
            *error = (__bridge_retained CFErrorRef)[[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        return false;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    NSError *err = nil;
    
    ret = [_device setActivePresetIndex:(NSInteger)presetIndex error:&err];
    
    if (ret == false) {
        os_log_error(HIDDisplayLog(),"%@ HIDDisplaySetActivePresetIndex error %@ for  preset index %ld ",_device, err, presetIndex);
    }
    
    if (ret == false && error) {
        
        *error = (__bridge_retained CFErrorRef)err;
    }
    
    return ret;
}

CFDictionaryRef __nullable HIDDisplayCopyPreset(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex, CFErrorRef *error)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    NSError *err = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        if (error) {
            err = [[NSError alloc] initWithDomain:@"Invalid object" code:kIOReturnBadArgument userInfo:nil];
            *error = (__bridge_retained CFErrorRef)err;
        }
        return NULL;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"Invalid preset index %ld ",presetIndex);
        if (error) {
            err = [[NSError alloc] initWithDomain:@"Invalid index" code:kIOReturnBadArgument userInfo:nil];
            *error = (__bridge_retained CFErrorRef)err;
        }
        return NULL;
    }
    
    
    HIDDisplayPresetData *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    NSDictionary *presetInfo = nil;
    
    presetInfo =  [preset get:&err];
    
    if (!presetInfo || presetInfo.count == 0) {
        
        os_log_error(HIDDisplayLog(),"%@ HIDDisplayCopyPreset error %@ for  preset index %ld ",_device, err, presetIndex);
        
        if (error) {
            *error = (__bridge_retained CFErrorRef)err;
        }
        return NULL;
    }
    
    return (__bridge_retained CFDictionaryRef)presetInfo;
}

bool HIDDisplaySetPreset(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex, CFDictionaryRef info, CFErrorRef *error)
{
    bool ret = true;
    NSError *err = nil;
    
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        if (error) {
            err = [[NSError alloc] initWithDomain:@"Invalid object" code:kIOReturnBadArgument userInfo:nil];
            *error = (__bridge_retained CFErrorRef)err;
        }
        return false;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"%@ Invalid preset index %ld ",_device, presetIndex);
        if (error) {
            err = [[NSError alloc] initWithDomain:@"Invalid index" code:kIOReturnBadArgument userInfo:nil];
            *error = (__bridge_retained CFErrorRef)err;
        }
        return false;
    }
    
    HIDDisplayPresetData *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    ret = [preset set:(__bridge NSDictionary*)info error:&err];
    
    if (ret == false) {
        os_log_error(HIDDisplayLog(),"%@ HIDDisplaySetPreset error %@ for  preset index %ld ",_device, err, presetIndex);
    }
    
    
    if (error && ret == false) {
        *error = (__bridge_retained CFErrorRef)err;
    }
    
    return ret;
}

bool HIDDisplayIsPresetValid(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return false;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"%@ Invalid preset index %ld ",_device, presetIndex);
        return false;
    }
    
    
    HIDDisplayPresetData *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    return preset.valid;
    
}

bool HIDDisplayIsPresetWritable(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return false;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"%@ Invalid preset index %ld ",_device, presetIndex);
        return false;
    }
    
    HIDDisplayPresetData *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    return preset.writable;
}

CFArrayRef HIDDisplayGetPresetCapabilities(HIDDisplayPresetInterfaceRef hidDisplayInterface)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return NULL;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    NSArray *capabilities = _device.capabilities;
    
    if (!capabilities) {
        return NULL;
    }
    
    return (__bridge CFArrayRef)capabilities;
}

CFDataRef __nullable HIDDisplayCopyPresetUniqueID(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return NULL;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        
        os_log_error(HIDDisplayLog(),"%@ Invalid preset index %ld ",_device, presetIndex);
        return NULL;
    }
    
    HIDDisplayPresetData *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    NSData *uniqueID = preset.uniqueID;
    
    if (!uniqueID) {
        return NULL;
    }
    
    return (__bridge_retained CFDataRef)uniqueID;
}


HIDDisplayPresetInterfaceRef __nullable HIDDisplayCreatePresetInterfaceWithService(io_service_t service)
{
    HIDDisplayPresetInterface *device = [[HIDDisplayPresetInterface alloc] initWithService:service];
    
    if (!device) {
        return NULL;
    }
    
    return (__bridge_retained HIDDisplayPresetInterfaceRef)device;
}

CFStringRef __nullable HIDDisplayGetContainerID(HIDDisplayPresetInterfaceRef hidDisplayInterface)
{
    id device = (__bridge id)hidDisplayInterface;
    
    HIDDisplayPresetInterface *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayPresetInterface class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayPresetInterfaceRef");
        return NULL;
    }
    
    _device = (HIDDisplayPresetInterface*)device;
    
    return (__bridge CFStringRef)_device.containerID;
}
