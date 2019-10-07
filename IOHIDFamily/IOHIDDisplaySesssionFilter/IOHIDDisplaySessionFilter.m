//
//  IOHIDDisplaySessionFilter.m
//  IOHIDDisplaySesssionFilter
//
//  Created by AB on 2/6/19.
//

#import "IOHIDDisplaySessionFilter.h"
#import <os/log.h>
#import <HIDDisplay/HIDDisplay.h>
#import <HIDDisplay/HIDDisplayPresetInterfacePrivate.h>
#import <HIDDisplay/HIDDisplayPresetInterface.h>
#import <AssertMacros.h>
#import <HID/HID.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>

static os_log_t HIDDisplaySessionFilterLog (void);

os_log_t HIDDisplaySessionFilterLog (void)
{
    static os_log_t slog = NULL;
    static dispatch_once_t sonceToken;
    dispatch_once(&sonceToken, ^{
        slog = os_log_create("com.apple.iohid", "HIDDisplaySessionFilter");
    });
    
    return slog;
}


@implementation IOHIDDisplaySessionFilter
{
    HIDManager *_manager;
    NSDictionary *_matching;
    NSMutableArray *_debugInfo;
    dispatch_queue_t _queue;
}
- (nullable instancetype)initWithSession:(HIDSession * __unused)session
{
    self = [super init];
    if (!self) return self;
    
    _manager = [[HIDManager alloc] init];
    require_action(_manager, exit,os_log_error(HIDDisplaySessionFilterLog(), "Failed to create HID Manager"));
    
    _matching = @{@kIOHIDDeviceUsagePairsKey : @[@{
                                                     @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                     @kIOHIDDeviceUsageKey: @(kHIDUsage_AppleVendor_Display)
                                                     }]};
    
     _debugInfo = [[NSMutableArray alloc] init];
    
    [_manager setDeviceMatching:_matching];
    
    _queue = dispatch_queue_create("com.apple.IOHIDDisplaySessionFilter-Manager", NULL);
    
exit:
    return self;
}

-(void) dealloc
{
    if (_manager) {
        [_manager cancel];
    }
}

- (nullable id)propertyForKey:(NSString *)key
{
    
    if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        return [self serialize];
    }
          
    return nil;
}

- (void)activate
{
    __weak IOHIDDisplaySessionFilter *weakSelf = self;
    
    if (!_manager) return;
    
    [_manager setDeviceNotificationHandler:^(HIDDevice *device, BOOL added){
        
         __strong IOHIDDisplaySessionFilter *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
 
        os_log(HIDDisplaySessionFilterLog(), "Display device %s: %{public}@", added ? "added" : "removed", device);
        
        NSNumber *uniqueID = nil;
        uniqueID = [device propertyForKey:@(kIOHIDUniqueIDKey)];
        
        if (added) {
            HIDDisplayPresetInterface *displayDevice = [[HIDDisplayPresetInterface alloc] initWithMatching:strongSelf->_matching];
            
            // shouldn't expect failure here, but just for case, deviceMatchCallback is called and the device disappear and is not detected by framework
            if (!displayDevice) {
                os_log_error(HIDDisplaySessionFilterLog(), "No valid HID device for matching %{public}@",strongSelf->_matching);
                return;
            }
            
            [strongSelf setPresetIndexForDevice:displayDevice uniqueID:uniqueID];
            
        } else {
            @synchronized (strongSelf) {
                [strongSelf->_debugInfo removeObject:uniqueID];
            }
        }
    }];
    
    [_manager setDispatchQueue:_queue];
    [_manager activate];

    return;
}

-(NSDictionary*) serialize
{
    NSMutableDictionary *ret = [[NSMutableDictionary alloc] init];
    
    @synchronized (self) {
        
        ret[@"Class"] = @"HIDDisplaySessionFilter";
        ret[@"Status"] = self->_debugInfo;
    }
    
    return ret;
}
-(BOOL) setPresetIndexForDevice:(HIDDisplayPresetInterface*) device uniqueID:(NSNumber*) uniqueID
{
    NSError *err = nil;
    NSInteger factoryDefaultPresetIndex = -1;
    NSInteger activePresetIndex = -1;
    BOOL ret = NO;
    
    factoryDefaultPresetIndex = [device getFactoryDefaultPresetIndex:&err];
    require_action(factoryDefaultPresetIndex != -1, exit, os_log_error(HIDDisplaySessionFilterLog(), "Failed to get factory default preset index with %@",err));
    
    activePresetIndex = [device getActivePresetIndex:&err];
    require_action(activePresetIndex != -1, exit, os_log_error(HIDDisplaySessionFilterLog(), "Failed to get active preset index with %@",err));
    
    if (factoryDefaultPresetIndex != activePresetIndex) {
        
        os_log(HIDDisplaySessionFilterLog(),"Setting active preset index (%ld) to factory default preset index (%ld)", activePresetIndex, factoryDefaultPresetIndex);
        
        require_action([device setActivePresetIndex:factoryDefaultPresetIndex error:&err], exit, os_log_error(HIDDisplaySessionFilterLog(), "Failed to set active preset to factory default preset with error %@",err));
    }
    
    ret = YES;
    
exit:
    
    @synchronized (self) {
        
        if (ret) {
            [_debugInfo addObject:uniqueID];
        }
    }
    
    return ret;
}

- (BOOL)setProperty:(nullable id __unused)value
             forKey:(NSString * __unused)key
{
    return NO;
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService * __unused)service
{
    return event;
}
@end
