/*!
 * HIDTransaction.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDTransaction_h
#define HIDTransaction_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @typedef HIDTransactionDirectionType
 *
 * @abstract
 * Enumeration of HID transaction types.
 */
typedef NS_ENUM(NSInteger, HIDTransactionDirectionType) {
    HIDTransactionDirectionTypeInput,
    HIDTransactionDirectionTypeOutput,
};

/*!
 * @typedef HIDTransactionCommitCallback
 *
 * @abstract
 * The block type used for asynchronous transaction callbacks.
 */
typedef void (^HIDTransactionCommitCallback)(IOReturn status);

/*!
 * @typedef HIDTransaction
 *
 * @abstract
 * A convenience interface for updating multiple HIDElements.
 *
 * @discussion
 * HIDElements can be updated individually by issuing get or set reports for each report
 * ID. HIDTransactions reduce this effort by allowing mutliple HIDElements to be updated
 * in one call. During a transaction, a get or set report for the report associated with each
 * HIDElement will be issued and the associated elements will be updated.
 */

__attribute__((visibility("hidden")))
@interface HIDTransaction : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithDevice
 *
 * @abstract
 * Creates an IOHIDTransaction for the specified HID device.
 *
 * @discussion
 * A HIDTransaction can be used to either send or receive multiple
 * element values, depending on the transaction's direction.
 *
 * @param device
 * The HIDDevice the transaction is associated with.
 *
 * @result
 * A HIDTransaction instance on success, nil on failure.
 */
- (nullable instancetype)initWithDevice:(HIDDevice *)device;

/*!
 * @method commitElements
 *
 * @abstract
 * Commits the elements to the device.
 *
 * @discussion
 * Before committing the transaction, a direction should be specified using the
 * direction property. If no direction is specified, the default of
 * kIOHIDTransactionDirectionTypeInput will be used.
 *
 * Input transactions issue get reports to the associated HID device, and update the
 * local elements on success.
 * Output transactions issue set reports to the associated HID device, and update
 * the remote elements in the kernel service on success.
 *
 * Transactions can be re-used during the lifetime of the HIDDevice.
 *
 * The associated HID device must be open before calling this method.
 *
 * @param elements
 * An array of elements to be updated.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
                 error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method commitElements
 *
 * @abstract
 * Asynchronously commits the element transaction to the device.
 *
 * @discussion
 * Before committing the transaction, a direction should be specified using the
 * direction property. If no direction is specified, the default of
 * kIOHIDTransactionDirectionTypeInput will be used.
 *
 * Input transactions issue get reports to the associated HID device, and update the
 * local elements on success.
 * Output transactions issue set reports to the associated HID device, and update
 * the remote elements in the kernel service on success.
 *
 * Transactions can be re-used during the lifetime of the HIDDevice.
 *
 * The associated HIDDevice must be open and a dispatch queue must be set before calling
 * this method. If a callback is provided, this call will return once the request has been sent
 * to the device or the request fails. If true is returned, the callback will be triggered after the
 * device has processed the call or the timeout is hit. If false is returned,
 * the callback will not be triggered.
 *
 * @param elements
 * An array of elements to be updated.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @param timeout
 * If asynchronous, the time in milliseconds before the call will fail and the
 * callback invoked with a timeout error code.
 *
 * @param callback
 * The callback to invoke upon completion of an asynchronous call. If this is null
 * the call will run synchronously. If the initial call to commitElements returns false, the
 * callback will not be invoked.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
                 error:(out NSError * _Nullable * _Nullable)outError
               timeout:(NSInteger)timeout
              callback:(HIDTransactionCommitCallback _Nullable)callback;

/*!
 * @property direction
 *
 * @abstract
 * The direction of the transaction.
 *
 * @discussion
 * kIOHIDTransactionDirectionTypeOutput should be used when issuing element
 * updates to the device. kIOHIDTransactionDirectionTypeInput should be used
 * when requesting element values from the device.
 *
 * Note that only elements of type kIOHIDElementTypeFeature or
 * kIOHIDElementTypeOutput may be used with HIDTransactions.
 */
@property HIDTransactionDirectionType direction;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDTransaction_h */
