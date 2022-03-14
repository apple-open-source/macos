/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2018-2022 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "HIDVirtualEventService.h"
#import "HIDEventSystemClient.h"
#import "HIDEventSystemClientPrivate.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import "HIDServiceClient.h"
#import "NSError+IOReturn.h"
#import "IOHIDPrivateKeys.h"
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <stdatomic.h>
#include <os/assumes.h>

@interface HIDVirtualEventService () {
    _Atomic int _state;
}

@property  HIDEventSystemClient *   client;
@property  HIDServiceClient *       serviceClient;
@property  dispatch_queue_t         queue;

@end

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
        case kIOHIDVirtualServiceScheduledWithDispatchQueue:
            notificationType = HIDVirtualServiceNotificationTypeEnumerated;
            break;
        case kIOHIDVirtualServiceUnScheduledFromDispatchQueue:
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
    
    if (self.serviceClient) {
        [self.serviceClient setRemovalHandler: thandler];
    } else {
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

