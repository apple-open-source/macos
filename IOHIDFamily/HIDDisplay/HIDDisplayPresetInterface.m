//
//  HIDDisplayPresetInterface.m
//  HIDDisplay
//
//  Created by AB on 3/26/19.
//

#import "HIDDisplayPresetInterface.h"
#import "HIDDisplayInterfacePrivate.h"
#import "HIDDisplayInterface.h"
#import "HIDDisplayPresetInterfacePrivate.h"
#import "HIDDisplayPresetData.h"
#import "HIDDisplayPresetDataPrivate.h"
#import "HIDDisplayPrivate.h"

#import <AssertMacros.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>

@implementation HIDDisplayPresetInterface
{
    NSDictionary<NSNumber*,HIDElement*>* _usageElementMap;
}
-(nullable instancetype) initWithMatching:(NSDictionary*) matching
{
    self = [super initWithMatching:matching];
    if (!self) {
        return nil;
    }
    
    if (![self setupPresets]) {
        return nil;
    }
    
    return self;
}

-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    self = [super initWithContainerID:containerID];
    if (!self) {
        return nil;
    }
    
    if (![self setupPresets]) {
        return nil;
    }
    
    return self;
}

-(BOOL) setupPresets
{
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayPreset)};
    
    NSDictionary<NSNumber*,HIDElement*>* usageElementMap = [self getDeviceElements:matching];
    
    if (!usageElementMap || usageElementMap.count == 0) {
        return NO;
    }
    
    _usageElementMap = usageElementMap;
    
    [self createPresets];
    
    return YES;
}

-(NSArray*) getHIDDevices
{
   NSDictionary *matching = @{@kIOHIDDeviceUsagePairsKey : @[@{
                                                    @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                    @kIOHIDDeviceUsageKey: @(kHIDUsage_AppleVendor_Display)
                                                    }]};
    return [self getHIDDevicesForMatching:matching];
    
}

-(HIDElement*) getHIDElementForUsage:(NSInteger) usage
{
    if (!_usageElementMap) {
        return nil;
    }
    
    return [_usageElementMap objectForKey:@(usage)];
}

-(void) createPresets
{
    NSMutableArray<HIDDisplayPresetData*> *presets = [[NSMutableArray alloc] init];
    NSInteger presetCount = 0;
    
    HIDElement *presetCountElement = [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayCurrentPresetIndex)];
    
    if (presetCountElement) {
        presetCount = presetCountElement.logicalMax;
    }
    
    for (NSInteger i=0; i < presetCount; i++) {
        
        HIDDisplayPresetData *preset = [[HIDDisplayPresetData alloc] initWithDisplayDevice:self index:i];
        if (preset) {
            [presets addObject:preset];
        }
    }
    
    _presets = presets;
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
    
    HIDDisplayPresetData *preset = _presets[index];
    
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
