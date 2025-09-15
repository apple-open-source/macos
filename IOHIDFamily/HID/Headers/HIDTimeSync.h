/*!
 * HIDTimeSync.h
 * HID
 *
 * Copyright © 2024-2025 Apple Inc. All rights reserved.
 */

#ifndef HIDTimeSync_h
#define HIDTimeSync_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDDevice;
@class HIDEventService;
@class HIDServiceClient;

/*!
 * @typedef HIDTimeSyncEvent
 * @abstract HID timesync events
 * @discussion State changes to this timesync object impact ability to translate timestamps
 * and precision of the translation.
 * @constant HIDTimeSyncEventInactive
 * Ability to timesync is not available. Interfaces to translate timestamps will fail with an error.
 * The `precisionUS` parameter to `HIDTimeSyncEventHandler` does not apply.
 * @constant HIDTimeSyncEventActive
 * Ability to timesync is currently available. Interfaces to translate timestamps are functional.
 * The `precisionUS` parameter to `HIDTimeSyncEventHandler` indicates the current precision.
 */
typedef NS_ENUM(NSInteger, HIDTimeSyncEvent) {
    HIDTimeSyncEventInactive,
    HIDTimeSyncEventActive
};

/*!
 * @typedef HIDTimeSyncPrecision
 * @abstract Precision of the timesync translation
 * @discussion Implementation-specific indicator of the precision of the timesync system.
 * Some implementations may offer different precision depending on the current system or peripheral state.
 * @constant HIDTimeSyncPrecisionUnknown
 * Current precision is unspecified. This timesync implementation may be unable to notify the user about
 * dynamic changes to the precision of the translation.
 * @constant HIDTimeSyncPrecisionLow
 * Low precision. This indication is relative and implementation-defined.
 * @constant HIDTimeSyncPrecisionHigh
 * High precision. This indication is relative and implementation-defined.
 */
typedef NS_ENUM(NSInteger, HIDTimeSyncPrecision) {
    HIDTimeSyncPrecisionUnknown,
    HIDTimeSyncPrecisionLow,
    HIDTimeSyncPrecisionHigh,
};

/*!
 * @typedef HIDTimeSyncEventHandler
 * @abstract Block for handling timesync events
 * @param event New event to handle
 * @param precision New precision of the timesync translation
 */
typedef void (^HIDTimeSyncEventHandler)(HIDTimeSyncEvent event, HIDTimeSyncPrecision precision);

/*!
 * @class HIDTimeSync
 * @abstract Object tied to a HID device or HID service that enables syncing timestamps.
 * @discussion This class's availability is highly dependent on the specific HID device or HID service.
 * Clients must have special knowledge that timesync is available for their provider.
 *
 */
API_AVAILABLE(ios(19.0))
@interface HIDTimeSync : NSObject

- (instancetype)init ; //NS_UNAVAILABLE;

/*!
 * @method timesyncFromHIDDevice
 * @abstract Creates a HID timesync from a HID device
 * @param device
 * The HID device to have timestamps translated.
 * @result A timesync instance on success, nil on failure.
 */
+ (nullable instancetype)timeSyncFromHIDDevice:(HIDDevice *)device;

/*!
 * @method timesyncFromHIDEventService
 * @abstract Creates a HID timesync from a HID event service
 * @param service
 * The HID event service to have timestamps translated.
 * @result A timesync instance on success, nil on failure.
 */
+ (nullable instancetype)timeSyncFromHIDEventService:(HIDEventService *)service;

/*!
 * @method timesyncFromHIDServiceClient
 * @abstract Creates a HID timesync from a HID service client
 * @param client
 * The HID service client to have timestamps translated.
 * @result A timesync instance on success, nil on failure.
 */
+ (nullable instancetype)timeSyncFromHIDServiceClient:(HIDServiceClient *)client;

/*!
 * @method setEventHandler
 * @abstract Register an event handler to process state changes
 * @discussion The handler will run asynchronously  in the context of the queue set in `setDispatchQueue`.
 * It's necessary to call this before activation. The initial state will be delivered upon activation unless already cancelled.
 * @param handler
 * The event handler block to be registered
 */
- (void)setEventHandler:(HIDTimeSyncEventHandler)handler;

/*!
 * @method setDispatchQueue
 * @abstract Sets the dispatch queue to be associated with the HID timesync object.
 * @discussion It's necessary to call this before activation.
 * A call to setDispatchQueue should only be made once.
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method setCancelHandler
 * @abstract Sets a cancellation handler for the dispatch queue associated with the object.
 * @discussion The cancellation handler (if specified) will be submitted to the object's
 * dispatch queue in response to a call to cancel after all the events have been handled.
 * @param handler The cancellation handler block to be associated with the dispatch queue.
 */
- (void)setCancelHandler:(HIDBlock)handler;

/*!
 * @method activate
 * @abstract Activates the HID timesync object
 * @discussion The timesync object is created in an inactive state.
 * The timesync object  must be activated in order to receive asynchronous events and translate timestamps.
 * Before activation, a dispatch queue must be set via setDispatchQueue and event handler set via setEventHandler.
 * An activated timesync object must be cancelled via cancel. All set handler method calls should
 * be made before activation and not after cancellation.
 */
- (void)activate;

/*!
 * @method cancel
 * @abstract Cancels the HID timesync object, preventing any further invocation of its event handler block
 * or use of timestamp translation methods.
 * @discussion Cancelling prevents any further invocation of the event handler block for the
 * specified dispatch queue, but does not interrupt an event handler block that is already in progress.
 * Explicit cancellation of the HID timesync object is required. No implicit cancellation takes place.
 */
- (void)cancel;

/*!
 * @method syncedTimeFromData
 * @abstract Translate opaque time data object into sync'd mach time
 * @discussion It's valid to translate timestamps when HIDTimeSyncEventActive has been received.
 * @param timeData Opaque data representing time of an incoming data event from the provider.
 * The representation is specific to this provider's timesync implementation. The caller must know how to get
 * this data from the provider.
 * @param outError A reference to an NSError that will be filled with an error object on failure.
 * The reference will be unchanged on success.
 * @result On success, a mach absolute time that's been translated from the provider's domain,
 * or 0 upon failure.
 */
- (uint64_t)syncedTimeFromData:(NSData *)timeData error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method dataFromSyncedTime
 * @abstract Translate mach time into provider's opaque time data
 * @discussion It's valid to translate timestamps when HIDTimeSyncEventActive has been received.
 * @param syncedTime Mach absolute time to be translated into the provider's opaque time data representation.
 * @param outError A reference to an NSError that will be filled with an error object on failure.
 * The reference will be unchanged on success.
 * @result On success, data in the provider's time presentation translated from the mach time domain,
 * or 0 upon failure.
 */
- (NSData * _Nullable)dataFromSyncedTime:(uint64_t)syncedTime error:(out NSError * _Nullable * _Nullable)outError;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDTimeSync_h */
