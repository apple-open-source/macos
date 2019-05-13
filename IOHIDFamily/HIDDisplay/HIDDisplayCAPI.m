//
//  HIDDisplayCAPI.m
//  HIDDisplay
//
//  Created by AB on 1/10/19.
//

#include "HIDDisplayCAPI.h"
#include "HIDDisplayDevice.h"
#include "HIDDisplayDevicePreset.h"
#include "HIDDisplayDevicePrivate.h"
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

os_log_t HIDDisplayLog (void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.HIDDisplay", "default");
    });
    return log;
}


HIDDisplayDeviceRef __nullable HIDDisplayCreateDeviceWithContainerID(CFStringRef containerID)
{
    HIDDisplayDevice *device = [[HIDDisplayDevice alloc] initWithContainerID:(__bridge NSString*)containerID];
    
    if (!device) {
        return NULL;
    }
    
    return (__bridge_retained HIDDisplayDeviceRef)device;
}

CFIndex HIDDisplayGetPresetCount(HIDDisplayDeviceRef hidDisplayDevice)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return -1;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    return _device.presets.count;
}


CFIndex HIDDisplayGetFactoryDefaultPresetIndex(HIDDisplayDeviceRef hidDisplayDevice, CFErrorRef *error)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return -1;
    }
    
    _device = (HIDDisplayDevice*)device;
    NSError *err = nil;
    
    CFIndex index = [_device getFactoryDefaultPresetIndex:&err];
    
    if (index == -1 && error) {
        *error = (__bridge CFErrorRef)err;
    }
    
    return index;
}

CFIndex HIDDisplayGetActivePresetIndex(HIDDisplayDeviceRef hidDisplayDevice, CFErrorRef* error)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return -1;
    }
    
    _device = (HIDDisplayDevice*)device;
    NSError *err = nil;
    
    CFIndex index = (CFIndex)[_device getActivePresetIndex:&err];
    
    if (index == -1 && error) {
        *error = (__bridge CFErrorRef)err;
    }
    
    return index;
}

bool HIDDisplaySetActivePresetIndex(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex, CFErrorRef* error)
{
    id device = (__bridge id)hidDisplayDevice;
    bool ret = true;
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        
        if (error) {
            *error = (__bridge CFErrorRef)[[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        
        return false;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    NSError *err = nil;
    
    ret = [_device setActivePresetIndex:(NSInteger)presetIndex error:&err];
    
    os_log(HIDDisplayLog(),"setActivePresetIndex on device returned 0x%x",ret);
    
    if (ret == false && error) {
        *error = (__bridge CFErrorRef)err;
    }
    
    return ret;
}

CFDictionaryRef __nullable HIDDisplayCopyPreset(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex, CFErrorRef *error)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    NSError *err = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        if (error) {
            *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, kIOReturnBadArgument , NULL);
        }
        return NULL;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"Invalid preset index %ld ",presetIndex);
        if (error) {
            *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, kIOReturnBadArgument , NULL);
        }
        return NULL;
    }
    
    HIDDisplayDevicePreset *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    NSDictionary *presetInfo = nil;
    
    presetInfo =  [preset get:&err];
    
    if (!presetInfo) {
        
        if (error) {
            *error = (__bridge CFErrorRef)err;
        }
        return NULL;
    }
    
    return (__bridge_retained CFDictionaryRef)presetInfo;
}

bool HIDDisplaySetPreset(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex, CFDictionaryRef info, CFErrorRef *error)
{
    bool ret = true;
    NSError *err = nil;
    
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        if (error) {
            *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, kIOReturnBadArgument , NULL);
        }
        return false;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"Invalid preset index %ld ",presetIndex);
        if (error) {
            *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, kIOReturnBadArgument , NULL);
        }
        return false;
    }
    
    HIDDisplayDevicePreset *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    ret = [preset set:(__bridge NSDictionary*)info error:&err];
    
    if (error && ret == false) {
        *error = (__bridge CFErrorRef)err;
    }
    
    return ret;
}

bool HIDDisplayIsPresetValid(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return false;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"Invalid preset index %ld ",presetIndex);
        return false;
    }
    
    HIDDisplayDevicePreset *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    return preset.valid;
    
}

bool HIDDisplayIsPresetWritable(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return false;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        os_log_error(HIDDisplayLog(),"Invalid preset index %ld ",presetIndex);
        return false;
    }
    
    HIDDisplayDevicePreset *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    return preset.writable;
}

CFArrayRef HIDDisplayGetPresetCapabilities(HIDDisplayDeviceRef hidDisplayDevice)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return NULL;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    NSArray *capabilities = _device.capabilities;
    
    if (!capabilities) {
        return NULL;
    }
    
    return (__bridge CFArrayRef)capabilities;
}

CFStringRef __nullable HIDDisplayCopyPresetUniqueID(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex)
{
    id device = (__bridge id)hidDisplayDevice;
    
    HIDDisplayDevice *_device = nil;
    
    if (![device isKindOfClass:[HIDDisplayDevice class]]) {
        os_log_error(HIDDisplayLog(),"Invalid HIDDisplayDeviceRef");
        return NULL;
    }
    
    _device = (HIDDisplayDevice*)device;
    
    if (presetIndex < 0 || (NSUInteger)presetIndex >= _device.presets.count) {
        
        os_log_error(HIDDisplayLog(),"Invalid preset index %ld ",presetIndex);
        return NULL;
    }
    
    HIDDisplayDevicePreset *preset = [_device.presets objectAtIndex:(NSUInteger)presetIndex];
    
    NSString *uniqueID = preset.uniqueID;
    
    if (!uniqueID) {
        return NULL;
    }
    
    return (__bridge_retained CFStringRef)uniqueID;
}
