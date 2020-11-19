/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef IOHIDElement_h
#define IOHIDElement_h

#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDElement.h>
#import <IOKit/hid/IOHIDValue.h>
#import "IOHIDLibUserClient.h"

@interface HIDLibElement : NSObject {
    IOHIDElementRef     _element;
    IOHIDValueRef       _value;
    IOHIDValueRef       _defaultValue;
    NSString            *_psKey;
    IOHIDElementStruct  _elementStruct;
    BOOL                _isConstant;
    BOOL                _isUpdated;
    
}

- (nullable instancetype)initWithElementRef:(nonnull IOHIDElementRef)elementRef;
- (nullable instancetype)initWithElementStruct:(nonnull IOHIDElementStruct *)elementStruct
                                        parent:(nullable IOHIDElementRef)parent
                                         index:(uint32_t)index;

- (void)setIntegerValue:(NSInteger)integerValue;

@property (nullable)        IOHIDElementRef     elementRef;
@property (nullable)        IOHIDValueRef       valueRef;
@property (nullable)        IOHIDValueRef       defaultValueRef;
@property                   NSInteger           integerValue;
@property (nullable)        NSData              *dataValue;
@property (nullable, copy)  NSString            *psKey;
@property (readonly)        uint64_t            timestamp;
@property (readonly)        NSInteger           length;
@property (readonly)        IOHIDElementStruct  elementStruct;
@property                   BOOL                isConstant;
@property                   BOOL                isUpdated;

/*
 * These properties can be predicated against using the kIOHIDElement keys.
 * The property names must be consistent with the key names in order for us
 * to be able to predicate properly.
 */
@property (readonly)        uint32_t                    elementCookie;
@property (readonly)        uint32_t                    collectionCookie;
@property (readonly)        IOHIDElementType            type;
@property (readonly)        uint32_t                    usage;
@property (readonly)        uint32_t                    usagePage;
@property (readonly)        uint32_t                    unit;
@property (readonly)        uint8_t                     unitExponent;
@property (readonly)        uint8_t                     reportID;
@property (readonly)        uint32_t                    usageMin;
@property (readonly)        uint32_t                    usageMax;
@property (readonly)        IOHIDElementCollectionType  collectionType;

@end

#endif /* IOHIDElement_h */
