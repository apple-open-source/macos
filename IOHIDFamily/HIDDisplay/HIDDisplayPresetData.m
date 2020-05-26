//
//  HIDDisplayPresetData.m
//  HIDDisplay
//
//  Created by AB on 1/10/19.
//

#import "HIDDisplayPresetData.h"
#import <HID/HIDElement.h>
#import "HIDDisplayPresetInterfacePrivate.h"
#import "HIDDisplayPresetDataPrivate.h"
#import <IOKit/hid/AppleHIDUsageTables.h>
#import "HIDDisplayPrivate.h"
#import "HIDDisplayInterfacePrivate.h"
#import <IOKit/IOReturn.h>


@implementation HIDDisplayPresetData
{
    NSInteger _index; // will expose if it has use case
    __weak HIDDisplayPresetInterface *_deviceRef;
}

-(nullable instancetype) initWithDisplayDevice:(HIDDisplayPresetInterface*) hidDisplay index:(NSInteger) index
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    self.hidDisplay = hidDisplay;
    _index = index;
    
    return self;
}
-(HIDDisplayPresetInterface*) hidDisplay
{
    return _deviceRef;
}

-(void) setHidDisplay:(HIDDisplayPresetInterface*) device
{
    _deviceRef = device;
}

-(BOOL) valid
{
    __strong HIDDisplayPresetInterface *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        return NO;
    }
    
    NSError *err = nil;
    
    if (![_deviceRef setCurrentPresetIndex:_index error:&err]) {
        
        os_log_error(HIDDisplayLog(),"%@ Failed set preset index %ld",_deviceRef, _index);
        
        return NO;
    }
    
    HIDElement *presetValidElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetValid];
    
    if (!presetValidElement) {
        os_log_error(HIDDisplayLog(),"%@ Preset data valid no associated element",_deviceRef);
        return NO;
    }
    
    if ([hidDisplay extract:@[presetValidElement] error:nil]) {
        return presetValidElement.integerValue == 1 ? YES : NO;
    }
    
    return NO;
}

-(BOOL) writable
{
    __strong HIDDisplayPresetInterface *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        return NO;
    }
    
    NSError *err = nil;
    
    if (![_deviceRef setCurrentPresetIndex:_index error:&err]) {
        
        os_log_error(HIDDisplayLog(),"%@ Failed set preset index %ld",_deviceRef, _index);
        return NO;
    }
    
    HIDElement *presetWritableElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetWritable];
    
    if (!presetWritableElement) {
        os_log_error(HIDDisplayLog(),"%@ Preset data writable no associated element",_deviceRef);
        return NO;
    }
    
    
    
    if ([hidDisplay extract:@[presetWritableElement] error:nil]) {
        return presetWritableElement.integerValue == 1 ? YES : NO;
    }
    
    return NO;
}

-(nullable NSDictionary*) get:(NSError**) error
{
    NSMutableDictionary *ret = [[NSMutableDictionary alloc] init];
    NSMutableArray *transactionElements = [[NSMutableArray alloc] init];
    NSString *unicharString = nil;
    
    __strong HIDDisplayPresetInterface *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        return nil;
    }
    
    if (![_deviceRef setCurrentPresetIndex:_index error:error]) {
       os_log_error(HIDDisplayLog(),"%@ Failed set preset index %ld",_deviceRef, _index);
        return nil;
    }
    
    NSInteger usages[] = {
        kHIDUsage_AppleVendorDisplayPresetUnicodeStringName,
        kHIDUsage_AppleVendorDisplayPresetUnicodeStringDescription,
        kHIDUsage_AppleVendorDisplayPresetWritable,
        kHIDUsage_AppleVendorDisplayPresetValid,
        kHIDUsage_AppleVendorDisplayPresetDataBlockOneLength,
        kHIDUsage_AppleVendorDisplayPresetDataBlockOne,
        kHIDUsage_AppleVendorDisplayPresetDataBlockTwoLength,
        kHIDUsage_AppleVendorDisplayPresetDataBlockTwo,
        kHIDUsage_AppleVendorDisplayPresetUniqueID
    };
    
    for (NSUInteger i=0; i < sizeof(usages)/sizeof(usages[0]); i++) {
        
        NSInteger usage = usages[i];
        HIDElement *element = [hidDisplay getHIDElementForUsage:usage];
        if (!element) continue;
        [transactionElements addObject:element];
    }
    
    if (![hidDisplay extract:transactionElements error:error] ) {
        return nil;
    }
    
    for (NSUInteger i=0; i < sizeof(usages)/sizeof(usages[0]); i++) {
        NSInteger usage = usages[i];
        HIDElement *element = [hidDisplay getHIDElementForUsage:usage];
        
        if (!element) continue;
        
        
        switch (usage) {
            case kHIDUsage_AppleVendorDisplayPresetUnicodeStringName:
                unicharString = getUnicharStringFromData(element.dataValue);
                if (unicharString) {
                    ret[(__bridge NSString*)kHIDDisplayPresetFieldNameKey] = unicharString;
                } else {
                    os_log(HIDDisplayLog(), "Invalid / Empty Name Data %@", element.dataValue);
                    ret[(__bridge NSString*)kHIDDisplayPresetFieldNameKey] = @"";
                }
                break;
            case kHIDUsage_AppleVendorDisplayPresetUnicodeStringDescription:
                unicharString = getUnicharStringFromData(element.dataValue);
                if (unicharString) {
                    ret[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey] = unicharString;
                } else {
                    os_log(HIDDisplayLog(), "Invalid / Empty Description Data %@", element.dataValue);
                    ret[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey] = @"";
                }
                break;
            case kHIDUsage_AppleVendorDisplayPresetWritable:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldWritableKey] = element.integerValue == 1 ? @YES : @NO;
                break;
            case kHIDUsage_AppleVendorDisplayPresetValid:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldValidKey] = element.integerValue == 1 ? @YES : @NO;
                break;
            case kHIDUsage_AppleVendorDisplayPresetDataBlockOneLength:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayPresetDataBlockOne:
                if (element.dataValue) {
                    ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey] = element.dataValue;
                } else {
                    os_log(HIDDisplayLog(), "Invalid Data Block One Data %@", element.dataValue);
                }
                break;
            case kHIDUsage_AppleVendorDisplayPresetDataBlockTwoLength:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayPresetDataBlockTwo:
                if (element.dataValue) {
                    ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey] = element.dataValue;
                } else {
                    os_log(HIDDisplayLog(), "Invalid Data Block Two Data %@", element.dataValue);
                }
                break;
            case kHIDUsage_AppleVendorDisplayPresetUniqueID:
                if (element.dataValue) {
                    ret[(__bridge NSString*)kHIDDisplayPresetUniqueIDKey] = element.dataValue;
                } else {
                    os_log(HIDDisplayLog(), "Invalid UUID Data %@", element.dataValue);
                }
                break;
            default:
                break;
        }
        
    }
    os_log(HIDDisplayLog(), "%@ get preset for index %ld returned data %{public}@", _deviceRef, _index, ret);
    
    return ret;
}

-(BOOL) set:(NSDictionary*) info error:(NSError**) error
{

    __block NSMutableArray *transactionElements = [[NSMutableArray alloc] init];
    __block BOOL ret = YES;
    
    os_log(HIDDisplayLog(), "%@ set preset for index %ld data %{public}@", _deviceRef, _index, info);
    
    __strong HIDDisplayPresetInterface *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        return NO;
    }
    
    __block NSError *err = nil;
    
    // writable check should be done at device level
    // update elements, Initial call to set may cause
    // setting report with stale value , we should update
    // elements before set
    if (![self get:error]) {
        os_log_error(HIDDisplayLog(),"%@ Failed to update elements for preset index %ld",_deviceRef, _index);
        return NO;
    }
    
    [info enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull  stop) {
        
        os_log_debug(HIDDisplayLog(),"%@ set preset for index %ld key : %@ value : %@",_deviceRef, _index, key, obj);
        
        if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldNameKey]) {
            
            HIDElement *presetNameElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUnicodeStringName];
            
            if (presetNameElement && [obj isKindOfClass:[NSString class]]) {
                NSData *unicodeData = [(NSString*)obj dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
                if (!unicodeData || unicodeData.length == 0) {
                   uint16_t tmp = 0;
                   unicodeData = [[NSData alloc] initWithBytes:&tmp length:sizeof(uint16_t)];
                   os_log(HIDDisplayLog(), "Invalid Name %@ , Converting it to 2 byte null value", obj);
                }
                presetNameElement.dataValue = unicodeData;
                [transactionElements addObject:presetNameElement];
            }
            
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey]) {
            
            HIDElement *presetDescriptionElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUnicodeStringDescription];
            
            if (presetDescriptionElement && [obj isKindOfClass:[NSString class]]) {
                NSData *unicodeData = [(NSString*)obj dataUsingEncoding:NSUTF16LittleEndianStringEncoding];
                
                if (!unicodeData || unicodeData.length == 0) {
                    uint16_t tmp = 0;
                    unicodeData = [[NSData alloc] initWithBytes:&tmp length:sizeof(uint16_t)];
                    os_log(HIDDisplayLog(), "Invalid Description %@ , Converting it to 2 byte null value", obj);
                }
                
                presetDescriptionElement.dataValue = unicodeData;
                [transactionElements addObject:presetDescriptionElement];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneLengthKey]) {
            
            HIDElement *presetDataBlockOneLengthElement =  [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetDataBlockOneLength];
            
            if (presetDataBlockOneLengthElement && [obj isKindOfClass:[NSNumber class]]) {
                presetDataBlockOneLengthElement.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:presetDataBlockOneLengthElement];
            }
            
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey]) {
            
            HIDElement *presetDataBlockOneElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetDataBlockOne];
            
            if (presetDataBlockOneElement && [obj isKindOfClass:[NSData class]]) {
                NSData *data = (NSData*)obj;
                if (data.length == 0) {
                    // This shouldn't be 0 length here, Since Application is provider for this data we should return them error here
                    os_log_error(HIDDisplayLog(), "Invalid Block One Data %@ Cancel Device Transaction", data);
                    err = [[NSError alloc] initWithDomain:@"Invalid Block One Data" code:kIOReturnBadArgument userInfo:nil];
                    ret = NO;
                    *stop = YES;
                    return;
                    
                }
                presetDataBlockOneElement.dataValue = data;
                [transactionElements addObject:presetDataBlockOneElement];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey]) {
            
            HIDElement *presetDataBlockTwoLengthElement =  [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetDataBlockTwoLength];
            
            if (presetDataBlockTwoLengthElement && [obj isKindOfClass:[NSNumber class]]) {
                presetDataBlockTwoLengthElement.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:presetDataBlockTwoLengthElement];
            }
            
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey]) {
            
            HIDElement *presetDataBlockTwoElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetDataBlockTwo];
            
            if (presetDataBlockTwoElement && [obj isKindOfClass:[NSData class]]) {
                
                NSData *data = (NSData*)obj;
                if (data.length == 0) {
                    // This shouldn't be 0 Length here, Since Application is provider for this data we should return them error here
                    os_log_error(HIDDisplayLog(), "Invalid Block One Data %@ Cancel Device Transaction", data);
                    err = [[NSError alloc] initWithDomain:@"Invalid Block Two Data" code:kIOReturnBadArgument userInfo:nil];
                    ret = NO;
                    *stop = YES;
                    return;
                    
                }
                
                presetDataBlockTwoElement.dataValue = data;
                [transactionElements addObject:presetDataBlockTwoElement];
            }
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldValidKey]) {
            
            HIDElement *presetValidElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetValid];
            
            if (presetValidElement && [obj isKindOfClass:[NSNumber class]]) {
                presetValidElement.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:presetValidElement];
            }
            
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetUniqueIDKey]) {
            
            HIDElement *presetUniqueIDElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUniqueID];
            
            if (presetUniqueIDElement && [obj isKindOfClass:[NSData class]]) {
                
                NSData *data = (NSData*)obj;
                if (data.length == 0) {
                   // This shouldn't be 0 Length here, Since Application is provider for this data we should return them error here
                   os_log_error(HIDDisplayLog(), "Invalid Unique ID Data %@ Cancel Device Transaction", data);
                   err = [[NSError alloc] initWithDomain:@"Invalid Unique ID Data" code:kIOReturnBadArgument userInfo:nil];
                   ret = NO;
                   *stop = YES;
                   return;
                }
                presetUniqueIDElement.dataValue = data;
                [transactionElements addObject:presetUniqueIDElement];
            }
            
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldWritableKey]) {
            
            HIDElement *presetWritableElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetWritable];
            
            if (presetWritableElement && [obj isKindOfClass:[NSNumber class]]) {
                presetWritableElement.integerValue = ((NSNumber*)obj).integerValue;
                [transactionElements addObject:presetWritableElement];
            }
        }
        
    }];
    
    if (ret) {
        ret = [hidDisplay commit:transactionElements error:&err];
    } else {
        os_log_error(HIDDisplayLog(), "Skip Device Transaction due to previous issues");
    }
    
    if (error) {
        *error = err;
    }
    
    return ret;
}

-(nullable NSData*) uniqueID
{
    __strong HIDDisplayPresetInterface *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        return nil;
    }
    
    NSError *err = nil;
    
    if (![_deviceRef setCurrentPresetIndex:_index error:&err]) {
        
       os_log_error(HIDDisplayLog(),"%@ Failed set preset index %ld",_deviceRef, _index);
        
        return nil;
    }
    
    HIDElement *presetUniqueIDElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUniqueID];
    
    if (!presetUniqueIDElement) {
        os_log_error(HIDDisplayLog(),"%@ Preset data uniqueID no associated element",_deviceRef);
        return nil;
    }
    
    if ([hidDisplay extract:@[presetUniqueIDElement] error:nil]) {
        return presetUniqueIDElement.dataValue;
    }
    
    return nil;
}

@end
