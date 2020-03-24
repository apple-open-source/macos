//
//  HIDDisplayUserAdjustmentInterface.m
//  HIDDisplay
//
//  Created by abhishek on 1/15/20.
//

#import "HIDDisplayUserAdjustmentInterface.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayInterfacePrivate.h"

#import <AssertMacros.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>

@implementation HIDDisplayUserAdjustmentInterface {
    NSDictionary<NSNumber*,HIDElement*>* _usageElementMap;
}

-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    self = [super initWithContainerID:containerID];
    
    require_action(self, exit, os_log_error(HIDDisplayLog(), "Failed to create HIDDisplayUserAdjustmentInterface for %@",containerID));
    
    require_action([self setupInterface], exit, os_log_error(HIDDisplayLog(), "Failed to setup HIDDisplayUserAdjustmentInterface for %@",containerID));
    
    return self;
exit:
    return nil;
}

-(nullable instancetype) initWithService:(io_service_t) service {
    
    self = [super initWithService:service];
    
    require_action(self, exit, os_log_error(HIDDisplayLog(), "Failed to create HIDDisplayUserAdjustmentInterface for %d",(int)service));
    
    require_action([self setupInterface], exit, os_log_error(HIDDisplayLog(), "Failed to setup HIDDisplayUserAdjustmentInterface for %d",(int)service));
    
    return self;
exit:
    
    return nil;
}

-(NSArray*) getHIDDevices
{
    NSDictionary *matching = @{@kIOHIDDeviceUsagePairsKey : @[@{
                                                                  @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                                  @kIOHIDDeviceUsageKey: @(kHIDUsage_AppleVendor_DisplayUserAdjustment)
                                                                  }]};
    return [self getHIDDevicesForMatching:matching];
    
}

-(BOOL) setupInterface {
    
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayUserAdjustment)};
    
    NSDictionary<NSNumber*,HIDElement*>* usageElementMap = [self getDeviceElements:matching];
    
    if (!usageElementMap || usageElementMap.count == 0) {
        return NO;
    }
    
    _usageElementMap = usageElementMap;
    return YES;
}

-(BOOL) valid {
    
    BOOL ret = NO;
    NSError *err = nil;
    HIDElement *validElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Valid)] : NULL;
    
    if (validElement) {
        
        if ([self extract:@[validElement] error:&err]) {
            ret = validElement.integerValue;
        } else {
            os_log_error(HIDDisplayLog(),"%@ failed to extract valid element value with error %@",self, err);
        }
        
    } else {
        os_log_error(HIDDisplayLog(),"%@ valid no associated element",self);
    }
    return ret;
}

-(BOOL) invalidate:(NSError**) error {
   
    HIDElement *validElement = _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Valid)] : NULL;
    BOOL ret = NO;
    os_log(HIDDisplayLog(),"%@ invalidate user adjustement",self);
    
    if (validElement) {
        validElement.integerValue = 0;
        ret = [self commit:@[validElement] error:error];
    }
    return ret;
}

-(BOOL) set:(NSDictionary *)data error:(NSError *__autoreleasing  _Nullable *)error {
    __block NSMutableArray *transactionElements = [[NSMutableArray alloc] init];
    BOOL ret = NO;
    
    os_log_info(HIDDisplayLog(),"%@ set adjustment %@",self, data);
    
    // We should get updated value for all elements
    if (![self get:error]) {
        os_log_error(HIDDisplayLog(),"%@ Failed to update elements",self);
        return NO;
    }
    
    if (!data) {
        
        if (error) {
            *error = [[NSError alloc] initWithDomain:@"Invalid argument" code:kIOReturnBadArgument userInfo:nil];
        }
        return NO;
    }
    
    HIDElement *validElement = [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Valid)];
    
    if (validElement) {
        validElement.integerValue = 1;
        [transactionElements addObject:validElement];
    }
    
    [data enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
        
        HIDElement *element = nil;
        
        if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentDescriptionKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Description)] : nil;
    
            if (element && [obj isKindOfClass:[NSString class]]) {
                element.dataValue = [(NSString*)obj dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentTimestampKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Timestamp)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentTemperatureKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Temperature)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentFrontAmblientWhitePointXKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ALXFront)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentFrontAmblientWhitePointYKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ALYFront)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentFrontAmblientIlluminanceKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ALLuminanceFront)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentRearAmblientWhitePointXKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ALXRear)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentRearAmblientWhitePointYKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ALYRear)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentRearAmblientIlluminanceKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ALLuminanceRear)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentMeasuredWhitePointXKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_MWPX)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentMeasuredWhitePointYKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_MWPY)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentMeasuredLuminanceKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_MLuminance)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentExpectedWhitePointXKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_EWPX)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentExpectedWhitePointYKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_EWPY)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentExpectedLuminanceKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_ELuminance)] : nil;
            
            if (element && [obj isKindOfClass:[NSNumber class]]) {
                element.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:element];
            }
        }
        
    }];
    
    if (transactionElements.count > 0) {
        ret = [self commit:transactionElements error:error];
    } else {
        os_log_error(HIDDisplayLog(),"%@ no matching element to set ",self);
        if (error) {
            *error = [[NSError alloc] initWithDomain:@"Invalid argument" code:kIOReturnBadArgument userInfo:nil];
        }
    }
    
    return ret;
    
}

-(NSDictionary*) get:(NSError *__autoreleasing  _Nullable *)error {
    
    NSMutableDictionary *ret = [[NSMutableDictionary alloc] init];
    NSMutableArray *transactionElements = [[NSMutableArray alloc] init];
    
    NSInteger usages[] = {
        kHIDUsage_AppleVendorDisplayUserAdjustment_Description,
        kHIDUsage_AppleVendorDisplayUserAdjustment_Timestamp ,
        kHIDUsage_AppleVendorDisplayUserAdjustment_Temperature,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ALXFront,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ALYFront,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ALLuminanceFront,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ALXRear,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ALYRear,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ALLuminanceRear,
        kHIDUsage_AppleVendorDisplayUserAdjustment_MWPX,
        kHIDUsage_AppleVendorDisplayUserAdjustment_MWPY,
        kHIDUsage_AppleVendorDisplayUserAdjustment_MLuminance,
        kHIDUsage_AppleVendorDisplayUserAdjustment_EWPX,
        kHIDUsage_AppleVendorDisplayUserAdjustment_EWPY,
        kHIDUsage_AppleVendorDisplayUserAdjustment_ELuminance,
    };
    
    for (NSUInteger i=0; i < sizeof(usages)/sizeof(usages[0]); i++) {
        
        NSInteger usage = usages[i];
        HIDElement *element = _usageElementMap ? [_usageElementMap objectForKey:@(usage)] : nil;
        if (!element) continue;
        [transactionElements addObject:element];
    }
    
    if (![self extract:transactionElements error:error] ) {
        return nil;
    }
    
    for (NSUInteger i=0; i < sizeof(usages)/sizeof(usages[0]); i++) {
        NSInteger usage = usages[i];
        HIDElement *element = _usageElementMap ? [_usageElementMap objectForKey:@(usage)] : nil;
        
        if (!element) continue;
        
        switch (usage) {
            case kHIDUsage_AppleVendorDisplayUserAdjustment_Description:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentDescriptionKey] = getUnicharStringFromData(element.dataValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_Timestamp:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentTimestampKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_Temperature:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentTemperatureKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ALXFront:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentFrontAmblientWhitePointXKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ALYFront:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentFrontAmblientWhitePointYKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ALLuminanceFront:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentFrontAmblientIlluminanceKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ALXRear:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentRearAmblientWhitePointXKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ALYRear:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentRearAmblientWhitePointYKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ALLuminanceRear:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentRearAmblientIlluminanceKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_MWPX:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentMeasuredWhitePointXKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_MWPY:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentMeasuredWhitePointYKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_MLuminance:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentMeasuredLuminanceKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_EWPX:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentExpectedWhitePointXKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_EWPY:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentExpectedWhitePointYKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayUserAdjustment_ELuminance:
                ret[(__bridge NSString*)kHIDDisplayUserAdjustmentExpectedLuminanceKey] = @(element.integerValue);
                break;
        }
    }
    
    os_log(HIDDisplayLog(), "%@ get user adjustment returned data %{public}@", self,  ret);
        
    return ret;
}

@end
