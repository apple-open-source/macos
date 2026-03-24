//
//  IOHIDDevicePrivate.h
//  IOKitUser
//
//  Created by dekom on 8/31/18.
//

#ifndef IOHIDDevicePrivate_h
#define IOHIDDevicePrivate_h

#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDTransaction.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

typedef struct {
    void              * context;
    IOHIDCallback       callback;
    IOHIDTransactionRef transaction;
} IOHIDTransactionCallbackInfo;

/*
 * @struct IOHIDAsyncCommitCallbackInfo
 * @abstract Tracks pending async commit callbacks for abortion/cleanup
 * @discussion 
 *   Lifetime: First commit with callback call lives until callback execution or Release.
 *   CFDataRef freed in unregister or abort/Release. Context freed on completion or Release 
 *   for aborted callbacks.
 */
typedef struct {
    void *                  context;
    CFRunLoopRef _Nullable  runLoop;
    IOHIDCallback           callback;
    IOHIDDeviceRef          device;
    Boolean                 valid;
} IOHIDAsyncCommitCallbackInfo;

/*
 * @struct IOHIDAsyncElementCallbackInfo  
 * @abstract Tracks pending async element callbacks for abortion/cleanup
 * @discussion
 *   Lifetime:
 *   CFDataRef freed in unregister or abort/Release. 
 *   Context freed on completion or device Release for aborted callbacks.
 */
typedef struct {
    void *                  context;
    CFRunLoopRef _Nullable  runLoop;
    Boolean                 valid;
} IOHIDAsyncElementCallbackInfo;

/*
 * @struct IOHIDAsyncReportCallbackInfo
 * @abstract Tracks pending async report callbacks for abortion/cleanup  
 * @discussion
 *   Lifetime:
 *   CFDataRef freed in unregister or abort/Release. 
 *   Context freed on completion or device Release for aborted callbacks.
 */
typedef struct {
    void *                  context;
    CFRunLoopRef _Nullable  runLoop;
    IOHIDReportType         type;
    uint32_t                reportID;
    uint8_t *               report;
    CFIndex                 reportLength;
    Boolean                 valid;
} IOHIDAsyncReportCallbackInfo;

CF_EXPORT
IOHIDDeviceRef _Nullable _IOHIDDeviceCreatePrivate(CFAllocatorRef _Nullable allocator);

CF_EXPORT
CFStringRef IOHIDDeviceCopyDescription(IOHIDDeviceRef device);

CF_EXPORT
void _IOHIDDeviceReleasePrivate(IOHIDDeviceRef device);

uint64_t IOHIDDeviceGetRegistryEntryID(IOHIDDeviceRef device);

//------------------------------------------------------------------------------
// __IOHIDDeviceRegisterAsyncCommitCallback
//------------------------------------------------------------------------------
/*
    @abstract   Registers an async commit callback to be executed later and adds it to the device's pending callback queue

    @discussion This function creates and stores a pending async commit callback that is tracked
    for potential abortion if the device is cancelled before the transaction commit completes.
    The function stores the callback information in the device's async commit callbacks dictionary
    using the context pointer as the key. Multiple callbacks can be registered simultaneously.

    @param      device    The IOHIDDevice to register the pending async commit callback with
    @param      context   Client-provided context pointer that will be passed to the callback when executed

    @result     Returns kIOReturnSuccess if successfully registered, kIOReturnNoMemory if memory allocation fails, 
                or kIOReturnNotReady if device has no async source

    @locking    Acquires locks in the following order:
                1. device->deviceLock (recursive unfair lock) - ensures async source doesn't change during registration
                2. device->callbackLock (recursive unfair lock) - protects dictionary operations on asyncCommitCallbacks
*/
IOReturn __IOHIDDeviceRegisterAsyncCommitCallback(IOHIDDeviceRef     device,
                                                  void *             context);

//------------------------------------------------------------------------------
// __IOHIDDeviceRegisterAsyncElementCallback
//------------------------------------------------------------------------------
/*
    @abstract   Registers an async element callback to be executed later and adds it to the device's pending callback queue

    @discussion This function creates and stores a pending async element callback entry that is tracked
    for potential abortion if the device is cancelled before the element operation completes. 
    The callback is stored with its associated context in the device's async element callbacks dictionary.

    @param      device    The IOHIDDevice to register the pending async element callback with
    @param      context   Client-provided context pointer that will be passed to the callback when executed

    @result     Returns kIOReturnSuccess if successfully registered, kIOReturnNoMemory if memory allocation fails, 
                or kIOReturnNotReady if device has no async source

    @locking    Acquires locks in the following order:
                1. device->deviceLock (recursive unfair lock) - ensures async source doesn't change during registration
                2. device->callbackLock (recursive unfair lock) - protects dictionary operations on asyncElementCallbacks
*/
IOReturn __IOHIDDeviceRegisterAsyncElementCallback(IOHIDDeviceRef     device,
                                                   void *             context);

//------------------------------------------------------------------------------
// __IOHIDDeviceRegisterAsyncReportCallback
//------------------------------------------------------------------------------
/*
    @abstract   Registers an async report callback to be executed later and adds it to the device's pending callback queue

    @discussion This function creates and stores a pending async report callback entry that is tracked
    for potential abortion if the device is cancelled before the report operation completes. 
    The callback is stored in the device's async report callbacks dictionary with the context as the key.

    @param      device        The IOHIDDevice to register the pending async report callback with
    @param      context       Client-provided context pointer that will be passed to the callback when executed
    @param      type          The type of HID report (input, output, or feature)
    @param      reportID      The report ID associated with this report callback
    @param      report        Pointer to the report buffer
    @param      reportLength  The length of the report buffer in bytes

    @result     Returns kIOReturnSuccess if successfully registered, kIOReturnNoMemory if memory allocation fails, 
                or kIOReturnNotReady if device has no async source

    @note       Report parameters: All report-specific parameters (type, ID, buffer, length) are stored
                with the callback for later execution

    @locking    Acquires locks in the following order:
                1. device->deviceLock (recursive unfair lock) - ensures async source doesn't change during registration
                2. device->callbackLock (recursive unfair lock) - protects dictionary operations on asyncReportCallbacks
*/
IOReturn __IOHIDDeviceRegisterAsyncReportCallback(IOHIDDeviceRef         device,
                                                  void *                 context,
                                                  IOHIDReportType        type, 
                                                  uint32_t               reportID, 
                                                  uint8_t *              report, 
                                                  CFIndex                reportLength);

//------------------------------------------------------------------------------
// __IOHIDDeviceUnregisterAsyncCommitCallback
//------------------------------------------------------------------------------
/*
    @abstract   Removes a pending async commit callback from the device's tracking array to abort it

    @discussion This function searches the device's pending async commit callbacks for a callback
    entry matching the specified context pointer and removes it from tracking. If the underlying operation
    later completes and attempts to invoke the callback, it will be dropped since it's no longer tracked.
    The callback buffer is freed upon successful removal.

    @param      device   The IOHIDDevice containing the pending callbacks to search
    @param      context  The context pointer used to identify the callback to remove

    @result     Returns true if the callback was found and successfully removed from tracking,
    false if no matching callback was found or device has no pending callbacks.

    @note       Callback dropping: If the underlying async commit operation completes after this
                function is called, the callback will be dropped and not executed since it's no
                longer being tracked
*/
Boolean __IOHIDDeviceUnregisterAsyncCommitCallback(IOHIDDeviceRef   device,
                                                   void *           context);
//------------------------------------------------------------------------------
// __IOHIDDeviceUnregisterAsyncElementCallback
//------------------------------------------------------------------------------
/*
    @abstract   Removes a pending async element callback from the device's tracking array to abort it

    @discussion This function searches the device's pending async element callbacks for a callback
    entry matching the specified context pointer and removes it from tracking. If the underlying operation
    later completes and attempts to invoke the callback, it will be dropped since it's no longer tracked.
    The callback buffer is freed upon successful removal.

    @param      device   The IOHIDDevice containing the pending callbacks to search
    @param      context  The context pointer used to identify the callback to remove

    @result     Returns true if the callback was found and successfully removed from tracking,
                false if no matching callback was found or device has no pending callbacks.

    @note       Callback dropping: If the underlying async commit operation completes after this
                function is called, the callback will be dropped and not executed since it's no
                longer being tracked
*/
Boolean __IOHIDDeviceUnregisterAsyncReportCallback(IOHIDDeviceRef   device,
                                                   void *           context);

//------------------------------------------------------------------------------
// __IOHIDDeviceUnregisterAsyncReportCallback
//------------------------------------------------------------------------------
/*
    @abstract   Removes a pending async report callback from the device's tracking array to abort it

    @discussion This function searches the device's pending async report callbacks for a callback
    entry matching the specified context pointer and removes it from tracking. If the underlying operation
    later completes and attempts to invoke the callback, it will be dropped since it's no longer tracked.
    The callback buffer is freed upon successful removal.

    @param      device   The IOHIDDevice containing the pending callbacks to search
    @param      context  The context pointer used to identify the callback to remove

    @result     Returns true if the callback was found and successfully removed from tracking,
    false if no matching callback was found or device has no pending callbacks.

    @note       Callback dropping: If the underlying async commit operation completes after this
                function is called, the callback will be dropped and not executed since it's no
                longer being tracked
*/
Boolean __IOHIDDeviceUnregisterAsyncElementCallback(IOHIDDeviceRef   device,
                                                    void *           context);


//------------------------------------------------------------------------------
// __IOHIDDeviceAbortPendingCallbacks
//------------------------------------------------------------------------------
/*
    @abstract   Aborts all pending async callbacks by executing them with kIOReturnAborted status
  
    @discussion Cancels all pending async commit, element, and report callbacks by invoking 
                each with kIOReturnAborted and NULL value parameters. Uses callback array 
                copies to avoid holding locks during execution.

    @param      device  The IOHIDDevice whose pending callbacks should be aborted

    @result     returns true on success

    @note       Safe to call when no pending callbacks exist
 */
Boolean __IOHIDDeviceAbortPendingCallbacks(IOHIDDeviceRef        device,
                                           CFRunLoopRef          runLoop);


CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* IOHIDDevicePrivate_h */
