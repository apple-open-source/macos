/*!
 * HIDTimeSync.m
 * HID
 *
 * Copyright © 2025 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <HID/HIDTimeSync_Internal.h>
#import <HID/HIDBasicTimeSync.h>
#import <HID/NSError+IOReturn.h>
#import <os/assumes.h>
#import <stdatomic.h>
#import <AssertMacros.h>
#import <HID/HIDDevice.h>
#import <HID/HIDEventService.h>
#import <HID/HIDServiceClient.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <IOKit/IOMessage.h> // kIOMessageServicePropertyChange

__attribute__((visibility("hidden"))) API_AVAILABLE(ios(19.0))
void HIDTimeSyncPropertyHandler(void *refcon, io_service_t service, uint32_t messageType, void *arg);

@implementation HIDTimeSync
{
    /** Bitmask of HIDTimeSyncState */
    _Atomic uint32_t _state;

    /** HID device, client *or* service reference backing the service. The object can be backed by one. */
    HIDDevice *_device;
    HIDEventService *_service;
    HIDServiceClient *_client;

    IONotificationPortRef _propertyPort;
    io_object_t  _propertyNotify;
}

- (instancetype)init
{
    self = [super init];
    
    return self;
}


- (instancetype)initInternal
{
    return [super init];
}

+ (nullable instancetype)timeSyncFromProtocol:(NSUInteger)protocol
{
    HIDTimeSync *me = nil;

    if (kIOHIDTimeSyncProtocolTest == protocol) {
        me = [HIDTimeSync newTestTimeSync];
    }
#if TIMESYNC_BASIC_AVAILABLE
    else if (kIOHIDTimeSyncProtocolTSClock == protocol) {
        me = [[HIDBasicTimeSync alloc] init];
    }
#endif
    if (!me) {
        return nil;
    }

    me->_propertyPort = NULL;
    me->_propertyNotify = IO_OBJECT_NULL;

    return me;
}

+ (nullable instancetype)timeSyncFromHIDDevice:(HIDDevice *)device
{
    HIDTimeSync *me = nil;
    id protocol = [device propertyForKey:@kIOHIDTimeSyncProtocolKey];
    if (![protocol isKindOfClass:[NSNumber class]]) {
        return me;
    }

    NSNumber *protNum = (NSNumber *)protocol;
    me = [HIDTimeSync timeSyncFromProtocol:protNum.unsignedIntegerValue];
    if (!me) {
        return nil;
    }

    me->_device = device;

    return me;
}

+ (nullable instancetype)timeSyncFromHIDEventService:(HIDEventService *)service
{
    HIDTimeSync *me = nil;
    id protocol = [service propertyForKey:@kIOHIDTimeSyncProtocolKey];
    if (![protocol isKindOfClass:[NSNumber class]]) {
        return me;
    }

    NSNumber *protNum = (NSNumber *)protocol;
    me = [HIDTimeSync timeSyncFromProtocol:protNum.unsignedIntegerValue];
    if (!me) {
        return nil;
    }

    me->_service = service;

    return me;
}


+ (nullable instancetype)timeSyncFromHIDServiceClient:(HIDServiceClient *)client
{
    HIDTimeSync *me = nil;
    id protocol = [client propertyForKey:@kIOHIDTimeSyncProtocolKey];
    if (![protocol isKindOfClass:[NSNumber class]]) {
        return me;
    }

    NSNumber *protNum = (NSNumber *)protocol;
    me = [HIDTimeSync timeSyncFromProtocol:protNum.unsignedIntegerValue];

    me->_client = client;

    return me;
}

- (void)setEventHandler:(HIDTimeSyncEventHandler)handler
{
    os_assert(_state == HIDTimeSyncStateInit);
    _eventHandler = handler;
}


- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    os_assert(_state == HIDTimeSyncStateInit);
    _queue = queue;
}


- (void)setCancelHandler:(HIDBlock)handler
{
    os_assert(_state == HIDTimeSyncStateInit);
    _cancelHandler = handler;
}


- (void)activate
{
    io_service_t device_ref = IO_OBJECT_NULL;

    os_assert(_state == HIDTimeSyncStateInit);

    if (atomic_fetch_or(&_state, (uint32_t)HIDTimeSyncStateActivate) & HIDTimeSyncStateActivate) {
        return;
    }

    os_assert(_queue);
    os_assert(_eventHandler);

    device_ref = [self findDevice];

    [self registerPropertyNotification:device_ref];
    IOObjectRelease(device_ref);

    [self handleActivate];

    // call handlePropertyUpdate with initial properties
    dispatch_async(self.queue, ^{
        [self handlePropertyUpdate:[self properties]];
    });
}

- (void)handleActivate
{
    return;
}

- (void)cancel
{
    uint32_t state = atomic_fetch_or(&_state, (uint32_t)HIDTimeSyncStateCancelling);
    if (state & HIDTimeSyncStateCancelling) {
        return;
    }

    os_assert(_state & HIDTimeSyncStateActivate);
    os_assert(_queue);

    [self handleCancel];

    if (_propertyPort) {
        IONotificationPortSetDispatchQueue (_propertyPort, NULL);
        IONotificationPortDestroy(_propertyPort);
    }
    if (_propertyNotify) {
        IOObjectRelease(_propertyNotify);
    }

    if (_cancelHandler) {
        dispatch_async(_queue, ^{
            _cancelHandler();
            atomic_fetch_or(&_state, (uint32_t)HIDTimeSyncStateCancelled);
        });
    }
}

- (void)handleCancel
{
    return;
}

- (void)dealloc
{
    if (_queue) {
        os_assert((_state & (HIDTimeSyncStateActivate | HIDTimeSyncStateCancelling | HIDTimeSyncStateCancelled))
            == (HIDTimeSyncStateActivate | HIDTimeSyncStateCancelling | HIDTimeSyncStateCancelled),
            "Invalid dispatch state: 0x%x", _state);
    }
}

- (uint32_t)state
{
    return _state;
}

- (void)setState:(uint32_t)state
{
    _state = state;
}

- (io_service_t)findDevice
{
    io_service_t device_ref = IO_OBJECT_NULL;

    if (_device) {
        device_ref = _device.service;
        IOObjectRetain(device_ref);
    }
    else if (_client) {
        device_ref = [HIDTimeSync findDeviceForServiceID:_client.serviceID];
    }
    else if (_service) {
        device_ref = [HIDTimeSync findDeviceForServiceID:_service.serviceID];
    }
    return device_ref;
}

+ (io_service_t)findDeviceForServiceID:(uint64_t)serviceID
{
    CFMutableDictionaryRef matching = NULL;
    io_service_t service = IO_OBJECT_NULL;
    io_service_t device = IO_OBJECT_NULL;
    kern_return_t kr = 0;
    io_iterator_t iter = IO_OBJECT_NULL;
    io_object_t obj = IO_OBJECT_NULL;

    // Get IOHIDEventService kernel object reference
    matching = IORegistryEntryIDMatching(serviceID);
    require_action(matching, exit, os_log_error(_IOHIDLog(), "Failed to get matching for ID 0x%llx", serviceID));
    service = IOServiceGetMatchingService(kIOMainPortDefault, matching); // Releases matching
    matching = NULL;
    require_action(service, exit, os_log_error(_IOHIDLog(), "Failed to get service for ID 0x%llx", serviceID));

    kr = IORegistryEntryCreateIterator(service,
        kIOServicePlane,
        kIORegistryIterateRecursively | kIORegistryIterateParents,
        &iter);
    require_noerr_action(kr, exit, os_log_error(_IOHIDLog(), "Failed to create iterator for ID 0x%llx", serviceID));

    while ((obj = IOIteratorNext(iter))) {
        if (IOObjectConformsTo(obj, "IOHIDDevice")) {
            device = obj;
            break;
        }
        IOObjectRelease(obj);
    }
    require_action(device, exit, os_log_error(_IOHIDLog(), "Failed find device for ID 0x%llx", serviceID));

exit:
    if (service) {
        IOObjectRelease(service);
    }
    if (iter) {
        IOObjectRelease(iter);
    }
    if (matching) {
        CFRelease(matching);
    }

    return device;
}

- (void)registerPropertyNotification:(io_service_t)service
{
    kern_return_t status = 0;

    _propertyPort = IONotificationPortCreate(kIOMainPortDefault);
    IONotificationPortSetDispatchQueue(_propertyPort, self.queue);

    status = IOServiceAddInterestNotification (_propertyPort,
                                               service,
                                               kIOGeneralInterest,
                                               HIDTimeSyncPropertyHandler,
                                               (__bridge void *)self,
                                               &_propertyNotify
                                               );
    os_assert(0 == status, "IOServiceAddInterestNotification:%x", status);
}

- (void)handlePropertyUpdate:(NSDictionary *)properties
{
    os_assert(false, "Unimplemented in base class");
}

- (NSDictionary *)properties
{
    NSDictionary *properties = nil;
    CFTypeRef prop = NULL;
    io_service_t service = IO_OBJECT_NULL;

    if (_device) {
        service = _device.service;
        IOObjectRetain(service);
    }
    else if (_service) {
        service = IOServiceGetMatchingService(kIOMainPortDefault, IORegistryEntryIDMatching(_service.serviceID));
    }
    else if (_client) {
        service = IOServiceGetMatchingService(kIOMainPortDefault, IORegistryEntryIDMatching(_client.serviceID));
    }
    require_quiet(service != IO_OBJECT_NULL, exit);

    prop = IORegistryEntrySearchCFProperty(service,
                                           kIOServicePlane,
                                           CFSTR(kIOHIDTimeSyncPropertiesKey),
                                           kCFAllocatorDefault,
                                           kIORegistryIterateRecursively | kIORegistryIterateParents);
    require_quiet(prop, exit);
    require_action_quiet(CFGetTypeID(prop) == CFDictionaryGetTypeID(), exit, CFRelease(prop));

    properties = (__bridge_transfer NSDictionary *)prop;

exit:
    if (service != IO_OBJECT_NULL) {
        IOObjectRelease(service);
    }
    return properties;
}

- (BOOL)setProviderProperty:(nullable id)value forKey:(NSString *)key
{
    io_service_t device_ref = IO_OBJECT_NULL;
    HIDDevice *tmpDevice = nil;
    BOOL ret = NO;

    device_ref = [self findDevice];
    require_quiet(device_ref != IO_OBJECT_NULL, exit);

    tmpDevice = [[HIDDevice alloc] initWithService:device_ref];

    // Open to allow property passthrough
    [tmpDevice open];

    ret = [tmpDevice setProperty:value forKey:key];

    [tmpDevice close];

exit:
    if (device_ref != IO_OBJECT_NULL) {
        IOObjectRelease(device_ref);
    }
    return ret;
}

- (uint64_t)syncedTimeFromData:(NSData *)timeData error:(out NSError * _Nullable * _Nullable)outError
{
    os_assert(false, "Unimplemented in base class");
    return 0;
}


- (NSData * _Nullable)dataFromSyncedTime:(uint64_t)syncedTime error:(out NSError * _Nullable * _Nullable)outError
{
    os_assert(false, "Unimplemented in base class");
    return nil;
}

@end

void HIDTimeSyncPropertyHandler(void *refcon, [[maybe_unused]] io_service_t service, uint32_t messageType, [[maybe_unused]] void *arg)
{
    if (kIOMessageServicePropertyChange != messageType) {
        return;
    }

    HIDTimeSync *me = (__bridge HIDTimeSync *)refcon;
    [me handlePropertyUpdate:[me properties]];
}
