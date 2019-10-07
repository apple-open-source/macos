/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef SECURITY_SFSQL_OBJCTYPE_H
#define SECURITY_SFSQL_OBJCTYPE_H 1

#if __OBJC2__

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, SFObjCTypeCode) {
    SFObjCTypeChar                = 0,  // 'c'
    SFObjCTypeShort               = 1,  // 's'
    SFObjCTypeInt                 = 2,  // 'i'
    SFObjCTypeLong                = 3,  // 'l'
    SFObjCTypeLongLong            = 4,  // 'q'
    SFObjCTypeUnsignedChar        = 5,  // 'C'
    SFObjCTypeUnsignedShort       = 6,  // 'S'
    SFObjCTypeUnsignedInt         = 7,  // 'I'
    SFObjCTypeUnsignedLong        = 8,  // 'L'
    SFObjCTypeUnsignedLongLong    = 9,  // 'Q'
    SFObjCTypeFloat               = 10, // 'f'
    SFObjCTypeDouble              = 11, // 'd'
    SFObjCTypeBool                = 12, // 'b'
    SFObjCTypeVoid                = 13, // 'v'
    SFObjCTypeCharPointer         = 14, // '*'
    SFObjCTypeObject              = 15, // '@'
    SFObjCTypeClass               = 16, // '#'
    SFObjCTypeSelector            = 17, // ':'
    SFObjCTypeArray               = 18, // '[' type ']'
    SFObjCTypeStructure           = 19, // '{' name '=' type... '}'
    SFObjCTypeUnion               = 20, // '(' name '=' type... ')'
    SFObjCTypeBitfield            = 21, // 'b' number
    SFObjCTypePointer             = 22, // '^' type
    SFObjCTypeUnknown             = 23, // '?'
};

typedef NS_ENUM(NSInteger, SFObjCTypeFlag) {
    SFObjCTypeFlagIntegerNumber       = 0x1,
    SFObjCTypeFlagFloatingPointNumber = 0x2,
    
    SFObjCTypeFlagNone                = 0x0,
    SFObjCTypeFlagNumberMask          = 0x3,
};

@interface SFObjCType : NSObject {
    SFObjCTypeCode _code;
    NSString* _encoding;
    NSString* _name;
    NSString* _className;
    NSUInteger _size;
    NSUInteger _flags;
}

+ (SFObjCType *)typeForEncoding:(const char *)encoding;
+ (SFObjCType *)typeForValue:(NSValue *)value;

@property (nonatomic, readonly, assign) SFObjCTypeCode    code;
@property (nonatomic, readonly, strong) NSString           *encoding;
@property (nonatomic, readonly, strong) NSString           *name;
@property (nonatomic, readonly, strong) NSString           *className;
@property (nonatomic, readonly, assign) NSUInteger          size;
@property (nonatomic, readonly, assign) NSUInteger          flags;

@property (nonatomic, readonly, assign, getter=isNumber)              BOOL    number;
@property (nonatomic, readonly, assign, getter=isIntegerNumber)       BOOL    integerNumber;
@property (nonatomic, readonly, assign, getter=isFloatingPointNumber) BOOL    floatingPointNumber;
@property (nonatomic, readonly, assign, getter=isObject)              BOOL    object;

- (id)objectWithBytes:(const void *)bytes;
- (void)getBytes:(void *)bytes forObject:(id)object;

@end

#endif
#endif /* SECURITY_SFSQL_OBJCTYPE_H */
