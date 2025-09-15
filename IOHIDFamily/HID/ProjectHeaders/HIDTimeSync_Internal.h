/*!
 * HIDTimeSync_Internal.h
 * HID
 *
 * Copyright © 2024-2025 Apple Inc. All rights reserved.
 */

#ifndef HIDTimeSync_Internal_h
#define HIDTimeSync_Internal_h

#import <HID/HIDTimeSync.h>

NS_ASSUME_NONNULL_BEGIN


/** Flags that will comprise the bitmask tracking the state of the object */
typedef enum {
    HIDTimeSyncStateInit         = 0,
    HIDTimeSyncStateActivate     = 1 << 0,
    HIDTimeSyncStateCancelling   = 1 << 3,
    HIDTimeSyncStateCancelled    = 1 << 4,
} HIDTimeSyncState;

/** Internal interfaces of the HIDTimeSync base class. */
@interface HIDTimeSync ()

/** User-provided dispatch queue to be execution context for asynchronous events. */
@property (nonatomic, readonly) dispatch_queue_t queue;

/** Handlers to be set by the user before activation. */
@property (nonatomic, readonly) HIDTimeSyncEventHandler eventHandler;
@property (nonatomic, readonly) HIDBlock cancelHandler;

/** Bitmask of HIDTimeSyncState. Atomic. */
@property (nonatomic, readwrite) uint32_t state;

/** Internal init for subclasses to call */
- (instancetype)initInternal NS_DESIGNATED_INITIALIZER;

/** Subclasses can override to implement protocol-specific activation behavior */
- (void)handleActivate;

/** Subclasses can override toimplement protocol-specific cancellation behavior */
- (void)handleCancel;

/** Return an `io_service_t` representing the IOHIDDevice kernel service  */
- (io_service_t)findDevice;

/** Return an `io_service_t` representing the IOHIDDevice kernel service backing the service registryID
 *  The reference should be released by the caller. */
+ (io_service_t)findDeviceForServiceID:(uint64_t)serviceID;

/** Helper to implement the above methods */
- (void)registerPropertyNotification:(io_service_t)service;

/** Query the current TimeSync property dictionary from the provider - device or event service
 *  handlePropertyUpdate: will be called when the properties are changed. */
- (NSDictionary * _Nullable)properties;

/** Set a property on the provider service */
- (BOOL)setProviderProperty:(nullable id)value forKey:(NSString *)key;

/** TimeSync implementations should override to handle property changes.
    TimeSync providers trigger invocation of this method by updating kIOHIDTimeSyncPropertiesKey. */
- (void)handlePropertyUpdate:(NSDictionary * _Nullable)properties;

@end

/** Wrapper for the factory method of the test protocol subclass.
 *  Implemented in IOHIDFamilyUnitTests.
 */
@interface HIDTimeSync (TestFactory)

/** Implemented in IOHIDFamilyUnitTests. */
+ (nullable instancetype)newTestTimeSync;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDTimeSync_Internal_h */
