//
//  HIDDisplayDevice.m
//  HIDDisplay
//
//  Created by AB on 1/10/19.
//

#import "HIDDisplayDevice.h"
#import "HIDDisplayDevicePreset.h"
#import "HIDElement.h"
#import "HIDElementPrivate.h"
#import "HIDDisplayCAPI.h"
#import "HIDDisplayDevicePresetPrivate.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayDevicePrivate.h"

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

@implementation HIDDisplayDevice
{
    IOHIDManagerRef _manager;
    IOHIDDeviceRef _deviceRef;
    NSMutableDictionary<NSNumber*,HIDElement*> *_usageElementMap;
    IOHIDTransactionRef _transaction;
}
-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _containerID = containerID;
    
#if !TARGET_OS_IPHONE || TARGET_OS_IOSMAC
    NSDictionary *matching =  @{@kIOPropertyMatchKey : @{@kUSBDeviceContainerID : _containerID}};
#else
    NSDictionary *matching =  @{@kIOPropertyMatchKey : @{@kUSBContainerID : _containerID}};
#endif
    
    if (![self createDeviceWithMatching:matching]) {
        return nil;
    }
    
    return self;
}

-(nullable instancetype) initWithMatching:(NSDictionary*) matching
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    if (![self createDeviceWithMatching:matching]) {
        return nil;
    }
    
    return self;
}

-(BOOL) createDeviceWithMatching:(NSDictionary*) matching
{
    if (![self discoverHIDDevice:matching]) {
        os_log_error(HIDDisplayLog(),"Failed to discover HID Display device for matching %{public}@",matching);
        return NO;
    }
    
    if (![self discoverDeviceElements]) {
        
        os_log_error(HIDDisplayLog(),"Failed to discover HID Elements for  device %@ for matching %{public}@",_deviceRef,matching);
        
        return NO;
    }
    
    _transaction = IOHIDTransactionCreate(kCFAllocatorDefault, self.device, kIOHIDTransactionDirectionTypeOutput, kIOHIDOptionsTypeNone);
    
    if (!_transaction) {
        return NO;
    }
    
    [self createPresets];
    
    return YES;
}

-(void) createPresets
{
    NSMutableArray<HIDDisplayDevicePreset*> *presets = [[NSMutableArray alloc] init];
    NSInteger presetCount = 0;
    
    HIDElement *presetCountElement = [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayCurrentPresetIndex)];
    
    if (presetCountElement) {
        presetCount = presetCountElement.logicalMax;
    }
    
    for (NSInteger i=0; i < presetCount; i++) {
        
        HIDDisplayDevicePreset *preset = [[HIDDisplayDevicePreset alloc] initWithDisplayDevice:self index:i];
        if (preset) {
            [presets addObject:preset];
        }
    }
    
    _presets = presets;
}

-(void) dealloc
{
    if (_manager) {
        
        IOHIDManagerClose(_manager, 0);
        CFRelease(_manager);
        _manager = nil;
    }
    
    if (_transaction) {
        CFRelease(_transaction);
        _transaction = nil;
    }
    
    if (self.device) {
        CFRelease(self.device);
        self.device = nil;
    }
}

-(BOOL) discoverHIDDevice:(NSDictionary*) matching
{
    BOOL ret = NO;
    NSSet *devices = nil;
    NSArray *tmp = nil;
    _manager = IOHIDManagerCreate (kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    require(_manager, exit);
    
    IOHIDManagerSetDeviceMatching(_manager, (__bridge CFDictionaryRef)matching);
    
    IOHIDManagerOpen(_manager, kIOHIDOptionsTypeNone);
    
    devices =  (__bridge_transfer NSSet*)IOHIDManagerCopyDevices(_manager);
    
    require(devices, exit);
    
    tmp = [devices allObjects];
    
    if (tmp.count == 1) {
        self.device = (__bridge_retained IOHIDDeviceRef)[tmp objectAtIndex:0];
    }
    
    require(self.device, exit);
    
    ret = YES;
exit:
    return ret;
}

-(BOOL) discoverDeviceElements
{
    
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayPreset)};
    
    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(self.device, (__bridge CFDictionaryRef)matching, 0);
    BOOL ret = NO;
    
    require(elements, exit);
    
    _usageElementMap = [[NSMutableDictionary alloc] init];
    
    for (CFIndex i = 0; i < CFArrayGetCount(elements); i++ ) {
        
        IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        uint32_t usage = IOHIDElementGetUsage(element);
        
        HIDElement *displayElement = [[HIDElement alloc] initWithObject:element];
        
        if (displayElement) {
            _usageElementMap[@(usage)] = displayElement;
            ret = YES;
        }
    }
    
    
exit:
    if (elements) {
        CFRelease(elements);
    }
    
    return ret;
}
-(IOHIDDeviceRef) device
{
    return _deviceRef;
}

-(void) setDevice:(IOHIDDeviceRef) device
{
    _deviceRef = device;
}

-(HIDElement*) getHIDElementForUsage:(NSInteger) usage
{
    if (!_usageElementMap) {
        return nil;
    }
    
    return [_usageElementMap objectForKey:@(usage)];
}

-(NSInteger) getFactoryDefaultPresetIndex:(NSError**) error
{
    NSInteger index = -1;
    
    HIDElement *factoryDefaultIndexElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayFactoryDefaultPresetIndex)] : NULL;
    
    if (factoryDefaultIndexElement) {
        if ([self extract:@[factoryDefaultIndexElement] error:error]) {
            index = factoryDefaultIndexElement.integerValue;
        }
    }
    
    return index;
}

-(NSInteger) getActivePresetIndex:(NSError**) error
{
    NSInteger index = -1;
    
    HIDElement *activePresetIndexElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayActivePresetIndex)] : NULL;
    
    if (activePresetIndexElement) {
        if ([self extract:@[activePresetIndexElement] error:error]) {
            index = activePresetIndexElement.integerValue;
        }
    }

    return index;
}

-(BOOL) setActivePresetIndex:(NSInteger) index error:(NSError**) error
{
    BOOL ret = YES;
    
    if (index < 0 || (NSUInteger)index >= _presets.count) {
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        return NO;
    }
    
    HIDDisplayDevicePreset *preset = _presets[index];
    
    // check if preset can be set as active preset
    if (preset.valid == 0) {
        
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        os_log_error(HIDDisplayLog(),"[containerID:%@] setActivePresetIndex on invalid  preset index : %ld",self.containerID, index);
        return NO;
    }
    
    HIDElement *activePresetIndexElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayActivePresetIndex)] : NULL;
    
    if (activePresetIndexElement) {
        activePresetIndexElement.integerValue = index;
        ret = [self commit:@[activePresetIndexElement] error:error];
    } else {
        os_log_error(HIDDisplayLog(),"[containerID:%@] setActivePresetIndex no associated element",self.containerID);
    }
    
    return ret;
}

-(BOOL) setCurrentPresetIndex:(NSInteger) index error:(NSError**) error
{
    BOOL ret = YES;
    
    HIDElement *currentPresetIndexElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayCurrentPresetIndex)] : NULL;
    
    if (currentPresetIndexElement) {
        currentPresetIndexElement.integerValue = index;
        ret = [self commit:@[currentPresetIndexElement] error:error];
    }
    return ret;
}

-(NSInteger) getCurrentPresetIndex:(NSError**) error
{
    NSInteger index = -1;
    
    HIDElement *currentPresetIndexElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayCurrentPresetIndex)] : NULL;
    
    if (currentPresetIndexElement) {
        if ([self extract:@[currentPresetIndexElement] error:error]) {
            index = currentPresetIndexElement.integerValue;
        }
    }
    
    return index;
}

-(BOOL) commit:(NSArray<HIDElement*>*) elements error:(NSError**) error
{
    BOOL ret = YES;
    IOReturn kr = kIOReturnSuccess;
    
    IOHIDTransactionClear(_transaction);
    
    IOHIDTransactionSetDirection(_transaction, kIOHIDTransactionDirectionTypeOutput);
    
    for (HIDElement *element in elements) {
        
        IOHIDTransactionAddElement(_transaction,element.element);
        IOHIDTransactionSetValue(_transaction,element.element, element.valueRef, kIOHIDOptionsTypeNone);
        
    }
    
    kr = IOHIDTransactionCommit(_transaction);
    
    if (kr) {
        ret = NO;
        
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kr userInfo:nil];
        }
        os_log_error(HIDDisplayLog(),"[containerID:%@] HIDTransactionCommit error : 0x%x",self.containerID, kr);
        
        return ret;
    }
    
    IOHIDTransactionClear(_transaction);
    
    return ret;
}

-(BOOL) extract:(NSArray<HIDElement*>*) elements error:(NSError**) error
{
    IOReturn kr = kIOReturnSuccess;
    BOOL ret = YES;
    
    IOHIDTransactionClear(_transaction);
    
    IOHIDTransactionSetDirection(_transaction, kIOHIDTransactionDirectionTypeInput);
    
    for (HIDElement *element in elements) {
        IOHIDTransactionAddElement(_transaction,element.element);
    }
    
    kr = IOHIDTransactionCommit(_transaction);
    if (kr) {
        ret = NO;
        
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kr userInfo:nil];
        }
        os_log_error(HIDDisplayLog(),"[containerID:%@] HIDTransactionExtract error : 0x%x",self.containerID, kr);
        
        return ret;
    }
    
    for (HIDElement *element in elements) {
        element.valueRef = IOHIDTransactionGetValue(_transaction,element.element,kIOHIDOptionsTypeNone);
    }
    
    IOHIDTransactionClear(_transaction);
    
    return ret;
}

-(nullable NSArray<NSString*>*) capabilities
{
    // check for container id for specific display
    
    return @[(__bridge NSString*)kHIDDisplayPresetFieldWritableKey,
             (__bridge NSString*)kHIDDisplayPresetFieldValidKey,
             (__bridge NSString*)kHIDDisplayPresetFieldNameKey,
             (__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey,
             (__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey,
             (__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey,
             (__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey,
             (__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey,
             (__bridge NSString*)kHIDDisplayPresetUniqueIDKey,
             ];
}

@end

