/*!
 * HIDManager.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <HID/HIDManager.h>
#import <HID/HIDDevice.h>
#import <HID/HIDElement_Internal.h>
#import <os/assumes.h>
#import <os/lock_private.h>

@implementation HIDManager {
    IOHIDManagerRef             _manager;
    HIDManagerElementHandler    _elementHandler;
    HIDDeviceHandler            _deviceNotificationHandler;
    HIDReportHandler            _inputReportHandler;
    HIDBlock                    _cancelHandler;
    bool                        _activated;
    os_unfair_recursive_lock    _handlerLock;
}

- (instancetype)init
{
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    _manager = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    _handlerLock = OS_UNFAIR_RECURSIVE_LOCK_INIT;

    if (!_manager) {
        return nil;
    }
    
    return self;
}

- (instancetype)initWithOptions:(HIDManagerOptions)options
{
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    _manager = IOHIDManagerCreate(kCFAllocatorDefault, (IOOptionBits)options);
    _handlerLock = OS_UNFAIR_RECURSIVE_LOCK_INIT;

    if (!_manager) {
        return nil;
    }
    
    return self;
}

- (void)dealloc
{
    if (_manager) {
        CFRelease(_manager);
    }
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@", _manager];
}

- (NSArray<HIDDevice *> *)devices
{
    return [(NSSet *)CFBridgingRelease(IOHIDManagerCopyDevices(_manager))
            allObjects];
}

- (id)propertyForKey:(NSString *)key
{
    return (__bridge id)IOHIDManagerGetProperty(_manager,
                                                (__bridge CFStringRef)key);
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDManagerSetProperty(_manager,
                                   (__bridge CFStringRef)key,
                                   (__bridge CFTypeRef)value);
}

- (void)setInputElementMatching:(id)matching
{
    os_assert([matching isKindOfClass:[NSDictionary class]] ||
              [matching isKindOfClass:[NSArray class]],
              "Unknown matching criteria: %@", matching);
    
    if ([matching isKindOfClass:[NSDictionary class]]) {
        CFDictionaryRef matchDict = (__bridge CFDictionaryRef)matching;
        
        if (((NSDictionary *)matching).count) {
            IOHIDManagerSetInputValueMatching(_manager, matchDict);
        } else {
            IOHIDManagerSetInputValueMatching(_manager, nil);
        }
    } else if ([matching isKindOfClass:[NSArray class]]) {
        CFArrayRef matchArray = (__bridge CFArrayRef)matching;
        
        if (((NSArray *)matching).count) {
            IOHIDManagerSetInputValueMatchingMultiple(_manager, matchArray);
        } else {
            IOHIDManagerSetInputValueMatchingMultiple(_manager, nil);
        }
    }
}

static void inputValueCallback(void *context, IOReturn result __unused,
                               void *sender, IOHIDValueRef value)
{
    HIDManager *me = (__bridge HIDManager *)context;
    HIDElement *element = (__bridge HIDElement *)IOHIDValueGetElement(value);
    element.valueRef = value;
    
    (me->_elementHandler)((__bridge HIDDevice *)sender, element);
}

- (void)setInputElementHandler:(HIDManagerElementHandler)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_elementHandler == NULL, "Input element handler already set");
    _elementHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDManagerRegisterInputValueCallback(_manager,
                                           inputValueCallback,
                                           (__bridge void *)self);
}

- (void)setDeviceMatching:(id)matching
{
    os_assert([matching isKindOfClass:[NSDictionary class]] ||
              [matching isKindOfClass:[NSArray class]],
              "Unknown matching criteria: %@", matching);
    
    if ([matching isKindOfClass:[NSDictionary class]]) {
        CFDictionaryRef matchDict = (__bridge CFDictionaryRef)matching;
        
        if (((NSDictionary *)matching).count) {
            IOHIDManagerSetDeviceMatching(_manager, matchDict);
        } else {
            IOHIDManagerSetDeviceMatching(_manager, nil);
        }
    } else if ([matching isKindOfClass:[NSArray class]]) {
        CFArrayRef matchArray = (__bridge CFArrayRef)matching;
        
        if (((NSArray *)matching).count) {
            IOHIDManagerSetDeviceMatchingMultiple(_manager, matchArray);
        } else {
            IOHIDManagerSetDeviceMatchingMultiple(_manager, nil);
        }
    }
}

static void deviceAddedCallback(void *context __unused,
                                IOReturn result __unused,
                                void *sender __unused,
                                IOHIDDeviceRef device)
{
    HIDManager *me = (__bridge HIDManager *)context;
    (me->_deviceNotificationHandler)((__bridge HIDDevice *)device, true);
}

static void deviceRemovedCallback(void *context,
                                  IOReturn result __unused,
                                  void *sender __unused,
                                  IOHIDDeviceRef device)
{
    HIDManager *me = (__bridge HIDManager *)context;
    
    (me->_deviceNotificationHandler)((__bridge HIDDevice *)device, false);
}

- (void)setDeviceNotificationHandler:(HIDDeviceHandler)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_deviceNotificationHandler == NULL, "Device notification handler already set");
    _deviceNotificationHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);
    
    IOHIDManagerRegisterDeviceMatchingCallback(_manager,
                                               deviceAddedCallback,
                                               (__bridge void *)self);
    
    IOHIDManagerRegisterDeviceRemovalCallback(_manager,
                                              deviceRemovedCallback,
                                              (__bridge void *)self);
}

static void inputReportCallback(void *context,
                                IOReturn result __unused,
                                void *sender,
                                IOHIDReportType type,
                                uint32_t reportID,
                                uint8_t *report,
                                CFIndex reportLength,
                                uint64_t timeStamp)
{
    HIDManager *me = (__bridge HIDManager *)context;
    NSData *data = [[NSData alloc] initWithBytesNoCopy:report
                                                length:reportLength
                                          freeWhenDone:NO];
    
    (me->_inputReportHandler)((__bridge HIDDevice *)sender,
                              timeStamp,
                              (HIDReportType)type,
                              reportID,
                              data);
}

- (void)setInputReportHandler:(HIDReportHandler)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_inputReportHandler == NULL, "Input report handler already set");
    _inputReportHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);
    
    IOHIDManagerRegisterInputReportWithTimeStampCallback(_manager,
                                                         inputReportCallback,
                                                         (__bridge void *)self);
}

- (void)setCancelHandler:(HIDBlock)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(!_activated, "Cancel Handler cannot be set after HIDManager is activated");
    _cancelHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    IOHIDManagerSetDispatchQueue(_manager, queue);
}

- (void)open
{
    IOHIDManagerOpen(_manager, 0);
}

- (void)openWithOptions:(HIDManagerDeviceOptions)options
{
    IOHIDManagerOpen(_manager, (IOOptionBits)options);
}

- (void)close
{
    IOHIDManagerClose(_manager, 0);
}

- (void)activate
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    _activated = true;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDManagerSetCancelHandler(_manager, ^{
        // Block captures reference to self while cancellation hasn't completed.
        if (self->_cancelHandler) {
            self->_cancelHandler();
            self->_cancelHandler = nil;
        }
    });

    IOHIDManagerActivate(_manager);
}

- (void)cancel
{
    IOHIDManagerCancel(_manager);
}

@end
