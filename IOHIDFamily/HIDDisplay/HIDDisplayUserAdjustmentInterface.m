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
    
    __block NSError *err = nil;

    [data enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
        
        HIDElement *element = nil;
        
        if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentDescriptionKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Description)] : nil;
                
            if (element && [obj isKindOfClass:[NSString class]]) {
                NSData *unicodeData = [(NSString*)obj dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
                if (!unicodeData || unicodeData.length == 0) {
                    uint16_t tmp = 0;
                    unicodeData = [[NSData alloc] initWithBytes:&tmp length:sizeof(uint16_t)];
                    os_log(HIDDisplayLog(), "Invalid Description %@ , Converting it to 2 byte null value", obj);
                }
                element.dataValue = unicodeData;
                [transactionElements addObject:element];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayUserAdjustmentInformationKey]) {
            
            element =  _usageElementMap ? [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayUserAdjustment_Information)] : nil;
            
            if (element && [obj isKindOfClass:[NSData class]]) {
                NSData *dataValue = (NSData*)obj;
                if (dataValue.length == 0) {
                    // This shouldn't be 0 length here, Since Application is provider for this data we should return them error here
                    os_log_error(HIDDisplayLog(), "Invalid User Adjsutment Information %@ Cancel Device Transaction", dataValue);
                    err = [[NSError alloc] initWithDomain:@"Invalid User Adjsutment Information" code:kIOReturnBadArgument userInfo:nil];
                    return;

                }
                element.dataValue = dataValue;
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
    
    if (error) {
        *error = err;
    }

    return ret;
    
}

-(NSDictionary*) get:(NSError *__autoreleasing  _Nullable *)error {
    
    NSMutableDictionary *ret = [[NSMutableDictionary alloc] init];
    NSMutableArray *transactionElements = [[NSMutableArray alloc] init];
    
    NSInteger usages[] = {
        kHIDUsage_AppleVendorDisplayUserAdjustment_Description,
        kHIDUsage_AppleVendorDisplayUserAdjustment_Information ,
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
            case kHIDUsage_AppleVendorDisplayUserAdjustment_Description: {
                    NSString *unicodeDescription = getUnicharStringFromData(element.dataValue);
                    if (unicodeDescription) {
                        ret[(__bridge NSString*)kHIDDisplayUserAdjustmentDescriptionKey] = unicodeDescription;
                    } else {
                        os_log(HIDDisplayLog(), "Invalid / Empty user adjustment description %@", element.dataValue);
                        ret[(__bridge NSString*)kHIDDisplayUserAdjustmentDescriptionKey] = @"";
                    }
                    break;
                }
            case kHIDUsage_AppleVendorDisplayUserAdjustment_Information: {
                    if (element.dataValue) {
                        ret[(__bridge NSString*)kHIDDisplayUserAdjustmentInformationKey] = element.dataValue;
                    } else {
                        os_log(HIDDisplayLog(), "Invalid user adjustment information %@", element.dataValue);
                    }
                    break;
                }
            default:
                break;
        }
    }
    
    os_log(HIDDisplayLog(), "%@ get user adjustment returned data %{public}@", self,  ret);
        
    return ret;
}

@end
