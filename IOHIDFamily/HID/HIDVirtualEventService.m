/*!
 * HIDVirtualEventService.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <HID/HIDVirtualEventService.h>
#import <HID/HIDVirtualEventService_Internal.h>
#import <HID/HIDEventSystemClient_Internal.h>
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <HID/HIDServiceClient.h>
#import <HID/NSError+IOReturn.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <stdatomic.h>
#import <os/assumes.h>

#import "IOHIDPrivateKeys.h"

@implementation HIDVirtualEventService


- (instancetype)init
{
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    self.client = [[HIDEventSystemClient alloc] initWithType: HIDEventSystemClientTypeSimple];
    
    if (!self.client) {
        return nil;
    }

    // set a matching criteria that won't match any services to minimize messages on the mach port
    // used to handle the virtual service callbacks.
    [self.client setMatching:@{
        @(kIOHIDPrimaryUsagePageKey) : @(UINT32_MAX),
        @(kIOHIDPrimaryUsageKey) : @(UINT32_MAX),
    }];

    return self;
}


- (NSString *)description
{
    return [NSString stringWithFormat:@"<HIDVirtualEventService serviceID:0x%llx>", self.serviceID];
}

- (void)setCancelHandler:(HIDBlock)handler
{
    [self.client setCancelHandler:handler];
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    
    [self.client setDispatchQueue:queue];
    self.queue = queue;
}


static void  __HIDVirtualServiceNotifyCallback ( void * __unused target, void *  __unused context,  IOHIDServiceClientRef __unused service, uint32_t type, CFDictionaryRef property) {
    
    HIDVirtualEventService * self = (__bridge HIDVirtualEventService *)target;
    __strong id <HIDVirtualEventServiceDelegate> delegate  = self.delegate;
    
    if (!delegate) {
        return;
    }
    
    HIDVirtualServiceNotificationType notificationType;
    switch (type) {
        case kIOHIDVirtualServiceOpenedByEventSystem:
            notificationType = HIDVirtualServiceNotificationTypeEnumerated;
            break;
        case kIOHIDVirtualServiceUnScheduledFromDispatchQueue:
        case kIOHIDVirtualServiceReset:
            notificationType = HIDVirtualServiceNotificationTypeTerminated;
            break;
        default:
            return;
            break;
    }
    
    [delegate notification:notificationType withProperty:(__bridge NSDictionary * _Nullable)(property) forService:self];
}

static bool  __HIDVirtualServiceSetPropertyCallback (void * target, void * __unused context, IOHIDServiceClientRef __unused service, CFStringRef key, CFTypeRef value) {
    HIDVirtualEventService * self = (__bridge HIDVirtualEventService *)target;
    __strong id <HIDVirtualEventServiceDelegate> delegate  = self.delegate;
    
    if (!delegate) {
        return false;
    }

    return [delegate setProperty:(__bridge id)value forKey:(__bridge NSString *)key forService:self];
}

static CFTypeRef __HIDVirtualServiceCopyPropertyCallback (void * target, void * __unused context, IOHIDServiceClientRef __unused service,  CFStringRef key) {
    HIDVirtualEventService * self = (__bridge HIDVirtualEventService *)target;
    __strong id <HIDVirtualEventServiceDelegate> delegate  = self.delegate;
    
    if (!delegate) {
        return false;
    }

    return CFBridgingRetain([delegate propertyForKey:(__bridge NSString * _Nonnull)(key) forService:self]);
}

static IOHIDEventRef  __HIDVirtualServiceCopyEvent (void * target, void * __unused context, IOHIDServiceClientRef __unused service, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options) {
    HIDVirtualEventService * self = (__bridge HIDVirtualEventService *)target;
    __strong id <HIDVirtualEventServiceDelegate> delegate  = self.delegate;
    
    if (!delegate) {
        return false;
    }

    NSMutableDictionary * matchingDict = [[NSMutableDictionary alloc] init];
    matchingDict[@(kIOHIDMatchingEventTypeKey)]    = @(type);
    matchingDict[@(kIOHIDMatchingEventKey)]        = (__bridge id)matching;
    matchingDict[@(kIOHIDMatchingEventOptionsKey)] = @(options);

    return (IOHIDEventRef) CFBridgingRetain([delegate copyEventMatching:matchingDict forService:self]);
}

static IOReturn  __HIDVirtualServiceSetOutputEvent (void * target, void * __unused context, IOHIDServiceClientRef __unused service, IOHIDEventRef  event) {
    HIDVirtualEventService * self = (__bridge HIDVirtualEventService *)target;
    
    __strong id <HIDVirtualEventServiceDelegate> delegate  = self.delegate;
    
    if (!delegate) {
        return false;
    }

    return [delegate setOutputEvent:(__bridge HIDEvent *)event forService:self] ? kIOReturnSuccess : kIOReturnError;
}

static IOHIDEventRef _Nullable  __HIDVirtualServiceClientCopyMatchingEventCallback (void *  target, void * __unused context, IOHIDServiceClientRef __unused service, CFDictionaryRef matching)
{
    HIDVirtualEventService * self = (__bridge HIDVirtualEventService *)target;
    
    __strong id <HIDVirtualEventServiceDelegate> delegate  = self.delegate;
    
    if (!delegate) {
        return false;
    }

    return (IOHIDEventRef) CFBridgingRetain([delegate copyEventMatching:(__bridge NSDictionary *)matching forService:self]);
}



- (void)activate
{

    typeof (self->_state) state =  atomic_fetch_or(&self->_state, kIOHIDDispatchStateActive);
    os_assert (state == kIOHIDDispatchStateInactive,  "Invalid dispatch state: %d", state);

    [self.client activate];
    
    IOHIDVirtualServiceClientCallbacksV2 callbacks = {
        kIOHIDVirtualServiceClientCallbacksV2,
        {
            __HIDVirtualServiceNotifyCallback,
            __HIDVirtualServiceSetPropertyCallback,
            __HIDVirtualServiceCopyPropertyCallback,
            __HIDVirtualServiceCopyEvent,
            __HIDVirtualServiceSetOutputEvent
        },
        __HIDVirtualServiceClientCopyMatchingEventCallback
    };
    
    self.serviceClient = (HIDServiceClient *) CFBridgingRelease(IOHIDVirtualServiceClientCreateWithCallbacks (self.client.client, NULL, (IOHIDVirtualServiceClientCallbacks *)&callbacks,  (__bridge void * _Nullable)(self), NULL));
    
    __weak HIDVirtualEventService * weakSelf = self;
    HIDBlock thandler = ^{
        __strong HIDVirtualEventService * self_ = weakSelf;
        if (self_) {
            [self_.delegate notification:HIDVirtualServiceNotificationTypeTerminated withProperty:nil forService:self_];
        }
    };

    if (!self.serviceClient) {
        dispatch_async(self.queue, thandler);
    }
}

- (void)cancel
{
    typeof (self->_state) state =  atomic_fetch_or(&self->_state, kIOHIDDispatchStateCancelled);
    os_assert (state == kIOHIDDispatchStateActive,  "Invalid dispatch state: %d", state);

    [self.client cancel];
}

- (void)dealloc
{
    typeof (self->_state) state = atomic_load(&self->_state);
    os_assert (state != kIOHIDDispatchStateActive,  "Invalid dispatch state: %d", state);
}

- (BOOL) dispatchEvent: (HIDEvent *) event
{
    if (!self.serviceClient) {
        return false;
    }
    return IOHIDVirtualServiceClientDispatchEvent ((__bridge IOHIDServiceClientRef) self.serviceClient, (IOHIDEventRef) event);
}

- (uint64_t) serviceID
{
    if (!self.serviceClient) {
        return 0;
    }

    NSNumber * senderID = (NSNumber *)IOHIDServiceClientGetRegistryID ((__bridge IOHIDServiceClientRef) self.serviceClient);
    return [senderID unsignedLongLongValue];
}


@end

