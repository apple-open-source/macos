//
//  HIDDisplaySessionFilter.m
//  HIDDisplaySessionFilter
//
//  Created by AB on 1/25/19.
//


#import "IOHIDDisplaySessionFilter.h"
#import <os/log.h>
#import <HIDDisplay/HIDDisplay.h>
#import <HIDDisplay/HIDDisplayDevicePrivate.h>
#import <AssertMacros.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDDevice.h>

static os_log_t HIDDisplaySessionFilterLog (void);

@interface HIDDisplaySessionFilter (HIDDisplaySessionFilterPrivate)

-(void) handleDeviceMatchCallback:(IOHIDDeviceRef) device;
-(void) handleDeviceRemovalCallback:(IOHIDDeviceRef) device;

@end

os_log_t HIDDisplaySessionFilterLog (void)
{
    static os_log_t slog = NULL;
    static dispatch_once_t sonceToken;
    dispatch_once(&sonceToken, ^{
        slog = os_log_create("com.apple.HIDDisplaySessionFilter", "default");
    });
    
    return slog;
}


static void __deviceMatchingCallback(void * _Nullable context, IOReturn  result, void * _Nullable __unused  sender, IOHIDDeviceRef device)
{
    HIDDisplaySessionFilter *selfRef = (__bridge HIDDisplaySessionFilter*)context;
    
    if (!selfRef || result != kIOReturnSuccess) {
        return;
    }
    
    [selfRef handleDeviceMatchCallback:device];
}

static void __deviceRemovalCallback(void * _Nullable context, IOReturn  result, void * _Nullable __unused  sender, IOHIDDeviceRef device)
{
    HIDDisplaySessionFilter *selfRef = (__bridge HIDDisplaySessionFilter*)context;
    
    if (!selfRef || result != kIOReturnSuccess) {
        return;
    }
    
    [selfRef handleDeviceRemovalCallback:device];
}

@implementation HIDDisplaySessionFilter
{
    IOHIDManagerRef _manager;
    NSDictionary *_matching;
    NSMutableArray *_debugInfo;
    NSThread  *_thread;
    CFRunLoopRef _runLoop;
    
}
-(nullable instancetype) init
{
    self = [super init];
    require(self, exit);
    
    _manager = IOHIDManagerCreate (kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    require(_manager, exit);
    
    _matching = @{@kIOHIDPrimaryUsagePageKey : @(kHIDPage_AppleVendor), @kIOHIDPrimaryUsageKey : @(kHIDUsage_AppleVendor_Display)};
    
    _debugInfo = [[NSMutableArray alloc] init];
    
    IOHIDManagerSetDeviceMatching(_manager, (__bridge CFDictionaryRef)_matching);
    
    IOHIDManagerRegisterDeviceMatchingCallback(_manager, __deviceMatchingCallback, (__bridge void*)self);
    
    IOHIDManagerRegisterDeviceRemovalCallback(_manager, __deviceRemovalCallback, (__bridge void*)self);
    
exit:
    return self; // nil can cause unwanted condition in plugin
}

-(BOOL) open
{
    __weak HIDDisplaySessionFilter *weakSelf = self;
    
    if (!_manager) {
        return NO;
    }
    
    [NSThread detachNewThreadWithBlock:^{
        
        __strong HIDDisplaySessionFilter *strongSelf = weakSelf;
        
        if (!strongSelf) {
            return;
        }
        
        strongSelf->_runLoop = CFRunLoopGetCurrent();
        
        IOHIDManagerScheduleWithRunLoop(strongSelf->_manager, strongSelf->_runLoop, kCFRunLoopDefaultMode);
        
        IOHIDManagerOpen(strongSelf->_manager, kIOHIDOptionsTypeNone);
        
        CFRunLoopRun();
        
    }];
    
    return YES;
}

-(void) handleDeviceMatchCallback:(IOHIDDeviceRef) device
{
    NSNumber *uniqueID = nil;
    
    HIDDisplayDevice *displayDevice = [[HIDDisplayDevice alloc] initWithMatching:_matching];
    
    uniqueID = (__bridge NSNumber*)IOHIDDeviceGetProperty( device, CFSTR(kIOHIDUniqueIDKey) );
    
    // shouldn't expect failure here, but just for case, deviceMatchCallback is called and the device disappear and is not detected by framework
    require_action(displayDevice, exit, os_log_error(HIDDisplaySessionFilterLog(), "No valid HID device for matching %{public}@",_matching));
    
    
    [self setPresetIndexForDevice:displayDevice uniqueID:uniqueID];
    
exit:
    return;
}

-(void) handleDeviceRemovalCallback:(IOHIDDeviceRef) device
{
    NSNumber *uniqueID = (__bridge NSNumber*)IOHIDDeviceGetProperty( device, CFSTR(kIOHIDUniqueIDKey) );
    
    @synchronized (self) {
         [_debugInfo removeObject:uniqueID];
    }
}

-(BOOL) setPresetIndexForDevice:(HIDDisplayDevice*) device uniqueID:(NSNumber*) uniqueID
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

-(void) close
{
    
    if (!_runLoop) return;
    
    CFRunLoopPerformBlock(_runLoop, kCFRunLoopDefaultMode, ^{
        
        if (_manager) {
            
            IOHIDManagerUnscheduleFromRunLoop(_manager, _runLoop, kCFRunLoopDefaultMode);
            IOHIDManagerClose(_manager, kIOHIDOptionsTypeNone);
        }
        CFRunLoopStop(_runLoop);
        
    });
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
-(CFDictionaryRef) getProperty
{
    return (__bridge CFDictionaryRef)[self serialize];
}

-(void) dealloc
{
    if (_manager) {
        CFRelease(_manager);
    }
}
@end
