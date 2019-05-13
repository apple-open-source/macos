//
//  HIDDisplayDevicePreset.m
//  HIDDisplay
//
//  Created by AB on 1/10/19.
//

#import "HIDDisplayDevicePreset.h"
#import "HIDElement.h"
#import "HIDDisplayDevicePrivate.h"
#import "HIDDisplayDevicePresetPrivate.h"
#import "HIDDisplayPrivate.h"
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/IOReturn.h>

@implementation HIDDisplayDevicePreset
{
    NSInteger _index; // will expose if it has use case
    __weak HIDDisplayDevice *_deviceRef;
}

-(nullable instancetype) initWithDisplayDevice:(HIDDisplayDevice*) hidDisplay index:(NSInteger) index
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    self.hidDisplay = hidDisplay;
    _index = index;
    
    return self;
}

-(HIDDisplayDevice*) hidDisplay
{
    return _deviceRef;
}

-(void) setHidDisplay:(HIDDisplayDevice*) device
{
    _deviceRef = device;
}

-(BOOL) valid
{
    __strong HIDDisplayDevice *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        return NO;
    }
    
    NSError *err = nil;
    
    if (![_deviceRef setCurrentPresetIndex:_index error:&err]) {
        
        os_log_error(HIDDisplayLog(),"[containerID:%@] Failed set preset index %ld",_deviceRef.containerID, _index);
        
        return NO;
    }
    
    
    HIDElement *presetValidElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetValid];
    
    if (!presetValidElement) {
        return NO;
    }
    
    if ([hidDisplay extract:@[presetValidElement] error:nil]) {
        return presetValidElement.integerValue == 1 ? YES : NO;
    }
    
    return NO;
}

-(BOOL) writable
{
    __strong HIDDisplayDevice *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        return NO;
    }
    
    NSError *err = nil;
    
    if (![_deviceRef setCurrentPresetIndex:_index error:&err]) {
        
        os_log_error(HIDDisplayLog(),"[containerID:%@] Failed set preset index %ld",_deviceRef.containerID, _index);
        return NO;
    }
    
    HIDElement *presetWritableElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetWritable];
    
    if (!presetWritableElement) {
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
    
    __strong HIDDisplayDevice *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        return nil;
    }
    
    
    if (![_deviceRef setCurrentPresetIndex:_index error:error]) {
        os_log_error(HIDDisplayLog(),"[containerID:%@] Failed set preset index %ld",_deviceRef.containerID, _index);
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
    
    for (NSInteger i=0; i < sizeof(usages)/sizeof(usages[0]); i++) {
        
        NSInteger usage = usages[i];
        HIDElement *element = [hidDisplay getHIDElementForUsage:usage];
        if (!element) continue;
        [transactionElements addObject:element];
    }
    
    if (![hidDisplay extract:transactionElements error:error] ) {
        return nil;
    }
    
    for (NSInteger i=0; i < sizeof(usages)/sizeof(usages[0]); i++) {
        NSInteger usage = usages[i];
        HIDElement *element = [hidDisplay getHIDElementForUsage:usage];
        
        if (!element) continue;

    
        switch (usage) {
            case kHIDUsage_AppleVendorDisplayPresetUnicodeStringName:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldNameKey] = [NSString stringWithCharacters:(unichar*)[element.dataValue bytes] length:element.dataValue.length];
                break;
            case kHIDUsage_AppleVendorDisplayPresetUnicodeStringDescription:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey] = [NSString stringWithCharacters:(unichar*)[element.dataValue bytes] length:element.dataValue.length];
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
                ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockOneKey] = element.dataValue;
                break;
            case kHIDUsage_AppleVendorDisplayPresetDataBlockTwoLength:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoLengthKey] = @(element.integerValue);
                break;
            case kHIDUsage_AppleVendorDisplayPresetDataBlockTwo:
                ret[(__bridge NSString*)kHIDDisplayPresetFieldDataBlockTwoKey] = element.dataValue;
                break;
            case kHIDUsage_AppleVendorDisplayPresetUniqueID:
                ret[(__bridge NSString*)kHIDDisplayPresetUniqueIDKey] = [NSString stringWithUTF8String:[element.dataValue bytes]];
                break;
            default:
                break;
        }
        
    }
    
    return ret;
}

-(BOOL) set:(NSDictionary*) info error:(NSError**) error
{
    
    __block NSMutableArray *transactionElements = [[NSMutableArray alloc] init];
    __block BOOL ret = YES;
    
    __strong HIDDisplayDevice *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        if (error) {
            *error = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnInvalid userInfo:nil];
        }
        return NO;
    }
    
    if (![_deviceRef setCurrentPresetIndex:_index error:error]) {
        os_log_error(HIDDisplayLog(),"[containerID:%@] Failed set preset index %ld",_deviceRef.containerID, _index);
        return NO;
    }
    
    __block NSError *err = nil;
    
    if (self.writable == 0) {
        err = [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:kIOReturnUnsupported userInfo:nil];
        os_log_error(HIDDisplayLog(),"[containerID:%@]  preset index %ld not writable",_deviceRef.containerID, _index);
        return NO;
    }
    
    [info enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull __unused  stop) {
        
        if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldNameKey]) {
            
            HIDElement *presetNameElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUnicodeStringName];
            
            if (presetNameElement && [obj isKindOfClass:[NSString class]]) {
                presetNameElement.dataValue = [(NSString*)obj dataUsingEncoding:NSUnicodeStringEncoding];
                [transactionElements addObject:presetNameElement];
            }
            
        } else if ([key isEqualToString:(__bridge NSString*)kHIDDisplayPresetFieldDescriptionKey]) {
            
            HIDElement *presetDescriptionElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUnicodeStringDescription];
            
            if (presetDescriptionElement && [obj isKindOfClass:[NSString class]]) {
                presetDescriptionElement.dataValue = [(NSString*)obj dataUsingEncoding:NSUnicodeStringEncoding];
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
                presetDataBlockOneElement.dataValue = obj;
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
                presetDataBlockTwoElement.dataValue = obj;
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
            
            if (presetUniqueIDElement && [obj isKindOfClass:[NSString class]]) {
                presetUniqueIDElement.dataValue = [(NSString*)obj dataUsingEncoding:NSUTF8StringEncoding];
                [transactionElements addObject:presetUniqueIDElement];
            }
            
        }
        
        
    }];
    
    if (ret) {
        ret = [hidDisplay commit:transactionElements error:&err];
    }
    
    if (error) {
        *error = err;
    }
    
    return ret;
}

-(nullable NSString*) uniqueID
{
    __strong HIDDisplayDevice *hidDisplay = self.hidDisplay;
    if (!hidDisplay) {
        return nil;
    }
    
    NSError *err = nil;
    
    if (![_deviceRef setCurrentPresetIndex:_index error:&err]) {
        
        os_log_error(HIDDisplayLog(),"[containerID:%@] Failed set preset index %ld",_deviceRef.containerID, _index);
        
        return nil;
    }
    
    HIDElement *presetUniqueIDElement = [hidDisplay getHIDElementForUsage:kHIDUsage_AppleVendorDisplayPresetUniqueID];
    
    if (!presetUniqueIDElement) {
        return nil;
    }
    
    if ([hidDisplay extract:@[presetUniqueIDElement] error:nil]) {
        return [NSString stringWithUTF8String:[presetUniqueIDElement.dataValue bytes]];
    }
    
    return nil;
}
@end
