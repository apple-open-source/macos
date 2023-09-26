/*!
 * HIDEvent.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDEvent_h
#define HIDEvent_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDEventBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @enum HIDEventSerializationType
 *
 * @abstract
 * Enumeration of serialization types.
 *
 * @field HIDEventSerializationTypeFast
 * Fast serialization creates an NSData representation of the HIDEvent. It can
 * be useful for process to process event migration, but is not considered a
 * stable form of serialization, as an event can change over time.
 */
typedef NS_ENUM(NSInteger, HIDEventSerializationType) {
    HIDEventSerializationTypeFast
};

/*!
 * @category HIDEvent
 *
 * @abstract
 * HID action that relays information about an output event to clients of a HID service.
 *
 * @discussion
 * HIDEvents are the primary way that a HID service relays output to its clients. An example
 * is a HID service representing a keyboard device that dispatches HID events on keystrokes.
 * A UI application that is a client of the keyboard service can receive the keyboard events in
 * order to operate the GUI.
 */
@interface HIDEvent (HIDFramework) <NSCopying>

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithType
 *
 * @abstract
 * Creates a HIDEvent of the specified type.
 *
 * @param type
 * The type of the event. Event types can be found in
 * <IOKit/hid/IOHIDEventTypes.h>.
 *
 * @param timestamp
 * The timestamp of the event in mach absolute time.
 *
 * @result
 * A HIDEvent instance on success, nil on failure.
 */
- (nullable instancetype)initWithType:(IOHIDEventType)type
                            timestamp:(uint64_t)timestamp
                             senderID:(uint64_t)senderID;

/*!
 * @method initWithData
 *
 * @abstract
 * Creates a HIDEvent from serialized event data.
 *
 * @param data
 * Serialized event data, generally obtained by using the serialize method.
 *
 * @result
 * A HIDEvent instance on success, nil on failure.
 */
- (nullable instancetype)initWithData:(NSData *)data;

/*!
 * @method initWithBytes
 *
 * @abstract
 * Creates a HIDEvent from serialized event data.
 *
 * @param bytes
 * Serialized event bytes, generally obtained by using the serialize method.
 *
 * @param length
 * The length of the bytes being passed in.
 *
 * @result
 * A HIDEvent instance on success, nil on failure.
 */
- (nullable instancetype)initWithBytes:(const void *)bytes
                                length:(NSInteger)length;

/*!
 * @method isEqualToHIDEvent
 *
 * @abstract
 * Compares two HIDEvents.
 *
 * @param event
 * The HIDEvent to compare against.
 *
 * @result
 * true if the HID events are equal, false if the events are not equal.
 */
- (BOOL)isEqualToHIDEvent:(HIDEvent *)event;

/*!
 * @method serialize
 *
 * @abstract
 * Creates a serialized representation of the HIDEvent.
 *
 * @param type
 * The desired type of serialization. See the HIDEventSerializationType enum
 * for serialization types and their return types.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * A serialized representation of the event on success, nil on failure.
 */
- (nullable NSData *)serialize:(HIDEventSerializationType)type
                         error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method integerValueForField
 *
 * @abstract
 * Gets the integer value of the specified event field.
 *
 * @param field
 * The event field to query. Event fields can be found in
 * <IOKit/hid/IOHIDEventFieldDefs.h>.
 *
 * @result
 * An integer value representation of the specified field.
 */
- (NSInteger)integerValueForField:(IOHIDEventField)field;

/*!
 * @method setIntegerValue
 *
 * @abstract
 * Sets the integer value of the specified event field.
 *
 * @param value
 * The value to set.
 *
 * @param field
 * The event field to set. Event fields can be found in
 * <IOKit/hid/IOHIDEventFieldDefs.h>.
 */
- (void)setIntegerValue:(NSInteger)value
               forField:(IOHIDEventField)field;

/*!
 * @method doubleValueForField
 *
 * @abstract
 * Gets the double value of the specified event field.
 *
 * @param field
 * The event field to query. Event fields can be found in
 * <IOKit/hid/IOHIDEventFieldDefs.h>.
 *
 * @result
 * Returns a double value representation of the specified field.
 */
- (double)doubleValueForField:(IOHIDEventField)field;

/*!
 * @method setDoubleValue
 *
 * @abstract
 * Sets the double value of the specified event field.
 *
 * @param value
 * The value to set.
 *
 * @param field
 * The event field to set. Event fields can be found in
 * <IOKit/hid/IOHIDEventFieldDefs.h>.
 */
- (void)setDoubleValue:(double)value
              forField:(IOHIDEventField)field;

/*!
 * @method dataValueForField
 *
 * @abstract
 * Gets the data value of the specified event field.
 *
 * @param field
 * The event field to query. Event fields can be found in
 * <IOKit/hid/IOHIDEventFieldDefs.h>.
 *
 * @result
 * Returns a pointer to the data value representation of the specified field.
 */
- (void *)dataValueForField:(IOHIDEventField)field;

/*!
 * @method appendEvent
 *
 * @abstract
 * Appends a child HID event to the HIDEvent.
 *
 * @param event
 * The HID event to append.
 */
- (void)appendEvent:(HIDEvent *)event;

/*!
 * @method removeEvent
 *
 * @abstract
 * Removes a child HID event from the HIDEvent.
 *
 * @discussion
 * Comparison is done by
 * making sure the event type, options, and event data match the passed in
 * HIDEvent.
 *
 * @param event
 * The HID event to remove.
 */
- (void)removeEvent:(HIDEvent *)event;

/*!
 * @method removeAllEvents
 *
 * @abstract
 * Removes all child HID events from the HIDEvent.
 */
- (void)removeAllEvents;

/*!
 * @method conformsToEventType
 *
 * @abstract
 * Iterates through the event and its children to see if the event conforms to
 * the provided event type.
 *
 * @param type
 * The desired event type to check.
 *
 * @result
 * Returns true if the event or one of its children conforms to the provided type.
 */
- (BOOL)conformsToEventType:(IOHIDEventType)type;

/*!
 * @property timestamp
 *
 * @abstract
 * The timestamp of the event in mach absolute time.
 */
@property uint64_t timestamp;

/*!
 * @property senderID
 *
 * @abstract
 * The sender ID of the event.
 */
@property (readonly) uint64_t senderID;

/*!
 * @property type
 *
 * @abstract
 * The event type.
 */
@property (readonly) IOHIDEventType type;

/*!
 * @property options
 *
 * @abstract
 * The event options. Options are defined in <IOKit/hid/IOHIDKeys.h>
 */
@property uint32_t options;

/*!
 * @property parent
 *
 * @abstract
 * The parent event (if any).
 */
@property (readonly, nullable) HIDEvent * parent;

/*!
 * @property children
 *
 * @abstract
 * An array of child HIDEvents (if any).
 */
@property (readonly, nullable) NSArray<HIDEvent *> * children;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDEvent_h */
