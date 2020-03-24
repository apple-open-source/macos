//
//  HIDVirtualEventService.m
//  HID
//
//  Created by ygoryachok on 12/12/18.
//

#import "HIDVirtualEventService.h"
#import "HIDEventSystemClient.h"
#import "HIDEventSystemClientPrivate.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import "HIDServiceClient.h"
#import "NSError+IOReturn.h"
#import "IOHIDPrivateKeys.h"

@interface HIDVirtualEventService ()

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
    [self.client cancel];
}

- (BOOL) dispatchEvent: (HIDEvent *) event
{
    return IOHIDVirtualServiceClientDispatchEvent ((__bridge IOHIDServiceClientRef) self.serviceClient, (IOHIDEventRef) event);
}

- (uint64_t) serviceID
{
    NSNumber * senderID = (NSNumber *)IOHIDServiceClientGetRegistryID ((__bridge IOHIDServiceClientRef) self.serviceClient);
    return [senderID unsignedLongLongValue];
}


@end

