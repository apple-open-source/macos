/*
 * Copyright (c) 2020, 2022 Apple Inc. All Rights Reserved.
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

/*
   SecCBOR.m
*/

#import "SecCBOR.h"

// Mask selecting the low-order 5 bits of the "initial byte", which is where
// the additional information is encoded.
static const uint8_t kAdditionalInformationMask = 0x1F;
// Mask selecting the high-order 3 bits of the "initial byte", which indicates
// the major type of the encoded value.
//static const uint8_t kMajorTypeMask = 0xE0;
// Indicates the number of bits the "initial byte" needs to be shifted to the
// right after applying |kMajorTypeMask| to produce the major type in the
// lowermost bits.
static const uint8_t kMajorTypeBitShift = 5u;
// Indicates the integer is in the following byte.
static const uint8_t kAdditionalInformation1Byte = 24u;
// Indicates the integer is in the next 2 bytes.
static const uint8_t kAdditionalInformation2Bytes = 25u;
// Indicates the integer is in the next 4 bytes.
static const uint8_t kAdditionalInformation4Bytes = 26u;
// Indicates the integer is in the next 8 bytes.
static const uint8_t kAdditionalInformation8Bytes = 27u;


@implementation SecCBORValue

- (SecCBORType) fieldType { return SecCBORType_None; }
- (uint8_t) fieldValue { return [self fieldType] << kMajorTypeBitShift; }

- (void)write:(NSMutableData *)output {
    [self encodeStartItems:0 output:output];
}
- (void)encodeStartItems:(uint64_t)items output:(NSMutableData *)output{
    [self setUint:[self fieldValue] item2:items output:output];
}

- (void)setAdditionalInformation:(uint8_t)item1 item2:(uint8_t)additionalInformation output:(NSMutableData *)output {
    int v = item1 | (additionalInformation & kAdditionalInformationMask);
    uint8_t c = v;
    [output appendBytes:&c length:1];
}

- (void)setUint:(uint8_t)item1 item2:(uint64_t)value output:(NSMutableData *)output {
    size_t count = [self getNumUintBytes:value];
    int shift = -1;
    // Values under 24 are encoded directly in the initial byte.
    // Otherwise, the last 5 bits of the initial byte contains the length
    // of unsigned integer, which is encoded in following bytes.
    switch (count) {
    case 0:
        [self setAdditionalInformation:item1 item2:value output:output];
        break;
    case 1:
        [self setAdditionalInformation:item1 item2:kAdditionalInformation1Byte output:output];
        shift = 0;
        break;
    case 2:
        [self setAdditionalInformation:item1 item2:kAdditionalInformation2Bytes output:output];
        shift = 1;
        break;
    case 4:
        [self setAdditionalInformation:item1 item2:kAdditionalInformation4Bytes output:output];
        shift = 3;
        break;
    case 8:
        [self setAdditionalInformation:item1 item2:kAdditionalInformation8Bytes output:output];
        shift = 7;
        break;
    default:
        break;
    }
    for (; shift >= 0; shift--) {
        uint8_t c = 0xFF & (value >> (shift * 8));
        [output appendBytes:&c length:1];
    }
}

- (size_t)getNumUintBytes:(uint64_t) value {
    if (value < 24)
        return 0;
    if (value <= 0xFF)
        return 1;
    if (value <= 0xFFFF)
        return 2;
    if (value <= 0xFFFFFFFF)
        return 4;
    return 8;
}

@end


@implementation SecCBORUnsigned

- (SecCBORType) fieldType { return SecCBORType_Unsigned; }

- (instancetype)initWith:(NSUInteger)data {
    self = [super init];
    if (self) {
        m_data = data;
    }
    return self;
}

- (void)write:(NSMutableData *)output {
    [self encodeStartItems:(uint64_t)m_data output:output];
}

- (NSComparisonResult)compare:(SecCBORUnsigned *)other {
    if (m_data < other->m_data) {
        return NSOrderedAscending;
    } else if (m_data > other->m_data) {
        return NSOrderedDescending;
    } else {
        return NSOrderedSame;
    }
}

- (NSString *)getLabel { return [NSString stringWithFormat:@"%ld", (long)m_data]; }

@end


@implementation SecCBORNegative

- (SecCBORType) fieldType { return SecCBORType_Negative; }

- (instancetype)initWith:(NSInteger)data {
    self = [super init];
    if (self) {
        m_data = data;
    }
    return self;
}

- (void)write:(NSMutableData *)output {
    [self encodeStartItems:(uint64_t)(-(m_data+1)) output:output];
}

- (NSComparisonResult)compare:(SecCBORNegative *)other {
    if (m_data > other->m_data) {
        return NSOrderedAscending;
    } else if (m_data < other->m_data) {
        return NSOrderedDescending;
    } else {
        return NSOrderedSame;
    }
}

- (NSString *)getLabel { return [NSString stringWithFormat:@"%ld", (long)m_data]; }

@end


@implementation SecCBORString

- (SecCBORType) fieldType { return SecCBORType_String; }

- (instancetype)initWith:(NSString *)data {
    self = [super init];
    if (self) {
        m_data = [[NSString alloc] initWithString:data];
    }
    return self;
}

- (void)write:(NSMutableData *)output {
    [self encodeStartItems:[m_data length] output:output];
    NSData *bytes = [m_data dataUsingEncoding:NSUTF8StringEncoding];
    [output appendBytes:[bytes bytes] length:[bytes length]];
}

- (NSComparisonResult)compare:(SecCBORString *)other {
    NSUInteger myLength = [m_data length];
    NSUInteger otherLength = [other->m_data length];
    if (myLength < otherLength) {
        return NSOrderedAscending;
    } else if (myLength > otherLength) {
        return NSOrderedDescending;
    } else {
        return [m_data compare:other->m_data];
    }
}

- (NSString *)getLabel { return m_data; }

@end


@implementation SecCBORData

- (SecCBORType) fieldType { return SecCBORType_ByteString; }

- (instancetype)initWith:(NSData *)data {
    self = [super init];
    if (self) {
        m_data = [[NSData alloc] initWithData:data];
    }
    return self;
}

- (void)write:(NSMutableData *)output {
    [self encodeStartItems:[m_data length] output:output];
    [output appendBytes:[m_data bytes] length:[m_data length]];
}

@end


@implementation SecCBORArray

- (SecCBORType) fieldType { return SecCBORType_Array; }

- (instancetype)init {
    self = [super init];
    if (self) {
        m_data = [[NSMutableArray alloc] init];
    }
    return self;
}

- (instancetype)initWith:(NSArray *)data {
    self = [super init];
    if (self) {
        m_data = [[NSMutableArray alloc] initWithArray:data];
    }
    return self;
}

- (void)addObject:(SecCBORValue *)object {
    [m_data addObject:object];
}

- (void)write:(NSMutableData *)output {
    [self encodeStartItems:[m_data count] output:output];
    for( NSUInteger i=0; i<[m_data count]; ++i) {
        [m_data[i] write:output];
    }
}

@end


@implementation SecCBORMap

- (SecCBORType) fieldType { return SecCBORType_Map; }

- (instancetype)init {
    self = [super init];
    if (self) {
        m_data = [[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:0];
    }
    return self;
}

- (void)setKey:(SecCBORValue *)key value:(SecCBORValue *)data {
    [m_data setObject:data forKey:key];
}

- (NSArray *)getSortedKeys {
    NSArray* unsortedKeys = NSAllMapTableKeys(m_data);
    NSArray *sortedKeys = [unsortedKeys sortedArrayUsingComparator:^NSComparisonResult(id obj1, id obj2) {
        SecCBORType type1 = [obj1 fieldType];
        SecCBORType type2 = [obj2 fieldType];
        if (type1 < type2) {
            return NSOrderedAscending;
        } else if (type1 > type2) {
            return NSOrderedDescending;
        } else {
            if ([obj1 isKindOfClass:[SecCBORUnsigned class]] && [obj2 isKindOfClass:[SecCBORUnsigned class]]) {
                SecCBORUnsigned* val1 = obj1;
                SecCBORUnsigned* val2 = obj2;
                return [val1 compare:val2];
            } else if ([obj1 isKindOfClass:[SecCBORNegative class]] && [obj2 isKindOfClass:[SecCBORNegative class]]) {
                SecCBORNegative* val1 = obj1;
                SecCBORNegative* val2 = obj2;
                return [val1 compare:val2];
            } else if ([obj1 isKindOfClass:[SecCBORString class]] && [obj2 isKindOfClass:[SecCBORString class]]) {
                SecCBORString* val1 = obj1;
                SecCBORString* val2 = obj2;
                return [val1 compare:val2];
            }
        }
        return NSOrderedSame;
    }];
    return sortedKeys;
}

- (NSDictionary *)dictionaryRepresentation {
    return [m_data dictionaryRepresentation];
}

- (void)write:(NSMutableData *)output {
    NSArray *sortedKeys = [self getSortedKeys];
    
    NSEnumerator *ke = [sortedKeys objectEnumerator];
    id key, value;
    [self encodeStartItems:[m_data count] output:output];
    while (key = [ke nextObject]) {
        value = [m_data objectForKey:key];
        [key write:output];
        [value write:output];
    }
}

@end
