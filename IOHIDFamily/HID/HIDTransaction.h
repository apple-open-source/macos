//
//  HIDTransaction.h
//  HID
//
//  Created by dekom on 10/5/17.
//

#ifndef HIDTransaction_h
#define HIDTransaction_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDDevice;
@class HIDElement;

/*!
 * @typedef HIDTransactionDirectionType
 *
 * @abstract
 * Enumerator of hid transaction types.
 */
typedef NS_ENUM(NSInteger, HIDTransactionDirectionType) {
    HIDTransactionDirectionTypeInput,
    HIDTransactionDirectionTypeOutput,
};

__attribute__((visibility("hidden")))
@interface HIDTransaction : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithDevice
 *
 * @abstract
 * Creates an IOHIDTransaction object for the specified device.
 *
 * @discussion
 * A HIDTransaction object can be used to either send or receive multiple
 * element values, depending on the transaction's direction.
 *
 * @param device
 * The HIDDevice the transaction is associated with.
 *
 * @result
 * Returns an instance of a HIDTransaction object on success.
 */
- (nullable instancetype)initWithDevice:(HIDDevice *)device;

/*!
 * @method commitElements
 *
 * @abstract
 * Commits the element transaction to the device.
 *
 * @discussion
 * Before committing the transaction, a direction should be specified using the
 * transaction's direction property. If no direction is specified, the default
 * kIOHIDTransactionDirectionTypeInput will be used.
 *
 * For input transactions, the element values will be updated on success.
 *
 * Transaction objects can be re-used during the lifetime of the HIDDevice.
 *
 * @param elements
 * An array of elements to be updated.
 *
 * @param outError
 * An error returned on failure.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
                 error:(out NSError * _Nullable * _Nullable)outError;

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
