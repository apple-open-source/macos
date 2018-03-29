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

#if __OBJC2__

#import "SFObjCType.h"

static NSArray<SFObjCType *> *_SFObjCTypesByCode = nil;


#define OBJC_TYPE(_code, _encoding, _name, _size, _flags) \
    [[SFObjCType alloc] initWithCode:SFObjCType##_code encoding:_encoding name:_name className:nil size:_size flags:SFObjCTypeFlag##_flags]

@implementation SFObjCType

@synthesize code = _code;
@synthesize encoding = _encoding;
@synthesize name = _name;
@synthesize className = _className;
@synthesize size = _size;
@synthesize flags = _flags;

+ (SFObjCType *)typeForEncoding:(const char *)encodingUTF8 {
    NSString *encoding = @(encodingUTF8);

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        _SFObjCTypesByCode = @[
            OBJC_TYPE(Char,             @"c",   @"char",                sizeof(char),               IntegerNumber),
            OBJC_TYPE(Short,            @"s",   @"short",               sizeof(short),              IntegerNumber),
            OBJC_TYPE(Int,              @"i",   @"int",                 sizeof(int),                IntegerNumber),
            OBJC_TYPE(Long,             @"l",   @"long",                sizeof(long),               IntegerNumber),
            OBJC_TYPE(LongLong,         @"q",   @"long long",           sizeof(long long),          IntegerNumber),
            OBJC_TYPE(UnsignedChar,     @"C",   @"unsigned char",       sizeof(unsigned char),      IntegerNumber),
            OBJC_TYPE(UnsignedShort,    @"S",   @"unsigned short",      sizeof(unsigned short),     IntegerNumber),
            OBJC_TYPE(UnsignedInt,      @"I",   @"unsigned int",        sizeof(unsigned int),       IntegerNumber),
            OBJC_TYPE(UnsignedLong,     @"L",   @"unsigned long",       sizeof(unsigned long),      IntegerNumber),
            OBJC_TYPE(UnsignedLongLong, @"Q",   @"unsigned long long",  sizeof(unsigned long long), IntegerNumber),
            OBJC_TYPE(Float,            @"f",   @"float",               sizeof(float),              FloatingPointNumber),
            OBJC_TYPE(Double,           @"d",   @"double",              sizeof(double),             FloatingPointNumber),
            OBJC_TYPE(Bool,             @"B",   @"bool",                sizeof(bool),               IntegerNumber),
            OBJC_TYPE(Void,             @"v",   @"void",                sizeof(void),               None),
            OBJC_TYPE(CharPointer,      @"*",   @"char*",               sizeof(char*),              None),
            OBJC_TYPE(Object,           @"@",   @"id",                  sizeof(id),                 None),
            OBJC_TYPE(Class,            @"#",   @"Class",               sizeof(Class),              None),
        ];
    });
    
    switch (encodingUTF8[0]) {
        case 'c': return _SFObjCTypesByCode[SFObjCTypeChar];
        case 's': return _SFObjCTypesByCode[SFObjCTypeShort];
        case 'i': return _SFObjCTypesByCode[SFObjCTypeInt];
        case 'l': return _SFObjCTypesByCode[SFObjCTypeLong];
        case 'q': return _SFObjCTypesByCode[SFObjCTypeLongLong];
        case 'C': return _SFObjCTypesByCode[SFObjCTypeUnsignedChar];
        case 'S': return _SFObjCTypesByCode[SFObjCTypeUnsignedShort];
        case 'I': return _SFObjCTypesByCode[SFObjCTypeUnsignedInt];
        case 'L': return _SFObjCTypesByCode[SFObjCTypeUnsignedLong];
        case 'Q': return _SFObjCTypesByCode[SFObjCTypeUnsignedLongLong];
        case 'f': return _SFObjCTypesByCode[SFObjCTypeFloat];
        case 'd': return _SFObjCTypesByCode[SFObjCTypeDouble];
        case 'B': return _SFObjCTypesByCode[SFObjCTypeBool];
        case 'v': return _SFObjCTypesByCode[SFObjCTypeVoid];
        case '*': return _SFObjCTypesByCode[SFObjCTypeCharPointer];
        case '@': {
            if (encoding.length > 3 && [encoding characterAtIndex:1] == '"' && [encoding characterAtIndex:encoding.length-1] == '"') {
                NSString *className = [encoding substringWithRange:NSMakeRange(2, encoding.length-3)];
                NSString *name = [className stringByAppendingString:@"*"];
                return [[SFObjCType alloc] initWithCode:SFObjCTypeObject encoding:encoding name:name className:className size:sizeof(id) flags:SFObjCTypeFlagNone];
            } else {
                return _SFObjCTypesByCode[SFObjCTypeObject];
            }
        }
        case '#': return _SFObjCTypesByCode[SFObjCTypeClass];
        case ':': return _SFObjCTypesByCode[SFObjCTypeSelector];
        case '[': return OBJC_TYPE(Array,       encoding,   @"array",       0,              None);
        case '{': return OBJC_TYPE(Structure,   encoding,   @"structure",   0,              None);
        case '(': return OBJC_TYPE(Union,       encoding,   @"union",       0,              None);
        case 'b': return OBJC_TYPE(Bitfield,    encoding,   @"bitfield",    0,              None);
        case '^': return OBJC_TYPE(Pointer,     encoding,   @"pointer",     sizeof(void*),  None);
        case '?':
        default:
            return [[SFObjCType alloc] initWithCode:SFObjCTypeUnknown encoding:encoding name:@"unknown" className:nil size:0 flags:0];
    }
}

+ (SFObjCType *)typeForValue:(NSValue *)value {
    NS_VALID_UNTIL_END_OF_SCOPE NSValue *arcSafeValue = value;
    return [SFObjCType typeForEncoding:[arcSafeValue objCType]];
}

- (id)initWithCode:(SFObjCTypeCode)code encoding:(NSString *)encoding name:(NSString *)name className:(NSString *)className size:(NSUInteger)size flags:(NSUInteger)flags {
    if ((self = [super init])) {
        _code = code;
        _encoding = encoding;
        _name = name;
        _className = className;
        _size = size;
        _flags = flags;
    }
    return self;
}

- (BOOL)isNumber {
    return (_flags & SFObjCTypeFlagNumberMask) != 0;
}

- (BOOL)isIntegerNumber {
    return (_flags & SFObjCTypeFlagIntegerNumber) != 0;
}

- (BOOL)isFloatingPointNumber {
    return (_flags & SFObjCTypeFlagFloatingPointNumber) != 0;
}

- (BOOL)isObject {
    return _code == SFObjCTypeObject;
}

- (id)objectWithBytes:(const void *)bytes {
    switch (_code) {
        case SFObjCTypeChar:              return [NSNumber numberWithChar:*((const char *)bytes)];
        case SFObjCTypeShort:             return [NSNumber numberWithShort:*((const short *)bytes)];
        case SFObjCTypeInt:               return [NSNumber numberWithInt:*((const int *)bytes)];
        case SFObjCTypeLong:              return [NSNumber numberWithLong:*((const long *)bytes)];
        case SFObjCTypeLongLong:          return [NSNumber numberWithLongLong:*((const long long *)bytes)];
        case SFObjCTypeUnsignedChar:      return [NSNumber numberWithUnsignedChar:*((const unsigned char *)bytes)];
        case SFObjCTypeUnsignedShort:     return [NSNumber numberWithUnsignedShort:*((const unsigned short *)bytes)];
        case SFObjCTypeUnsignedInt:       return [NSNumber numberWithUnsignedInt:*((const unsigned int *)bytes)];
        case SFObjCTypeUnsignedLong:      return [NSNumber numberWithUnsignedLong:*((const unsigned long *)bytes)];
        case SFObjCTypeUnsignedLongLong:  return [NSNumber numberWithUnsignedLongLong:*((const unsigned long long *)bytes)];
        case SFObjCTypeFloat:             return [NSNumber numberWithFloat:*((const float *)bytes)];
        case SFObjCTypeDouble:            return [NSNumber numberWithDouble:*((const double *)bytes)];
        case SFObjCTypeBool:              return [NSNumber numberWithBool:(BOOL)*((const _Bool *)bytes)];
        case SFObjCTypeObject:            return (__bridge id)((const void *)(*((uintptr_t *)bytes)));
        default:
            [NSException raise:NSInternalInconsistencyException format:@"For class %@, Unsupported boxing type: %@", _className, _name];
            return nil;
    }
}

- (void)getBytes:(void *)bytes forObject:(id)object {
    if ([object isKindOfClass:[NSValue class]]) {
        [object getValue:bytes];
    } else {
        [NSException raise:NSInternalInconsistencyException format:@"Unsupported unboxing type: %@", _name];
    }
}

@end

#endif
