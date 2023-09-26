/*!
 * HIDEvent+HIDEventFields.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <HID/HIDEventFields.h>
#import <HID/HIDEvent.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @typedef HIDEventFieldInfoBlock
 *
 * @abstract
 * The block type used for enumerateFieldsWithBlock.
 */
typedef void (^HIDEventFieldInfoBlock) (HIDEventFieldInfo * eventField);

/*!
 * @category HIDEvent
 *
 * @abstract
 * HIDEvent extension to allow enumeration of
 */
@interface HIDEvent (HIDEventDesc)

/*!
 * @method enumerateFieldsWithBlock
 *
 * @abstract
 * Enumerates the event fields of the HIDEvent.
 *
 * @discussion
 * The block provided as a parameter is
 * called with a HIDEventFieldInfo argument describing each
 * field type.
 *
 * @param block
 * A block which will be called for each event field.
 */
- (void)enumerateFieldsWithBlock:(HIDEventFieldInfoBlock)block;

@end

NS_ASSUME_NONNULL_END
