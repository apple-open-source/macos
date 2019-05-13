//
//  HIDElement.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

// *** Adopted from HID.framework ***

#ifndef HIDElement_h
#define HIDElement_h

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDDevice.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * @typedef HIDValueScaleType
 *
 * @abstract
 * Enumerator of hid value scale types.
 */
typedef NS_ENUM(NSInteger, HIDValueScaleType) {
    HIDValueScaleTypeCalibrated,
    HIDValueScaleTypePhysical,
    HIDValueScaleTypeExponent
};

/*
 * @typedef HIDElementType
 *
 * @abstract
 * Enumerator of hid element types.
 */
typedef NS_ENUM(NSInteger, HIDElementType) {
    HIDElementTypeInputMisc = 1,
    HIDElementTypeInputButton = 2,
    HIDElementTypeOutput = 129,
    HIDElementTypeFeature = 257,
    HIDElementTypeCollection = 513
};

@interface HIDElement : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method scaleValue
 *
 * @abstract
 * Returns an scaled representaion of the element's value based on the scale
 * type.
 *
 * @param type
 * The type of scaling to be performed.
 *
 * @result
 * Returns an scaled floating point representation of the value.
 */
- (double)scaleValue:(HIDValueScaleType)type;

/*!
 * @property parent
 *
 * @abstract
 * The parent element (if any).
 */
@property (readonly, nullable) HIDElement *parent;

/*!
 * @property children
 *
 * @abstract
 * An array of child HIDElements (if any).
 */
@property (readonly) NSArray<HIDElement *> *children;

/*!
 * @property integerValue
 *
 * @abstract
 * An integer representaion of the element value.
 */
@property NSInteger integerValue;

/*!
 * @property dataValue
 *
 * @abstract
 * An NSData representaion of the element value.
 */
@property (nullable, assign) NSData *dataValue;

/*!
 * @property type
 *
 * @abstract
 * The IOHIDElementType of the element.
 */
@property (readonly) HIDElementType type;

/*!
 * @property usagePage
 *
 * @abstract
 * The usage page of the element.
 */
@property (readonly) NSInteger usagePage;

/*!
 * @property usage
 *
 * @abstract
 * The usage of the element.
 */
@property (readonly) NSInteger usage;

/*!
 * @property reportID
 *
 * @abstract
 * The report ID associated with the element.
 */
@property (readonly) NSInteger reportID;

/*!
 * @property reportSize
 *
 * @abstract
 * The report size, in bits, of the element.
 */
@property (readonly) NSInteger reportSize;

/*!
 * @property unit
 *
 * @abstract
 * The unit of the element.
 */
@property (readonly) NSInteger unit;

/*!
 * @property unitExponent
 *
 * @abstract
 * The unit exponent of the element.
 */
@property (readonly) NSInteger unitExponent;

/*!
 * @property logicalMin
 *
 * @abstract
 * The logical minimum of the element.
 */
@property (readonly) NSInteger logicalMin;

/*!
 * @property logicalMax
 *
 * @abstract
 * The logical maximum of the element.
 */
@property (readonly) NSInteger logicalMax;

/*!
 * @property physicalMin
 *
 * @abstract
 * The physical minimum of the element.
 */
@property (readonly) NSInteger physicalMin;

/*!
 * @property physicalMax
 *
 * @abstract
 * The physical maximum of the element.
 */
@property (readonly) NSInteger physicalMax;

/*!
 * @property timestamp
 *
 * @abstract
 * The timestamp of the element in mach_absolute_time().
 */
@property (readonly) uint64_t timestamp;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDElement_h */
