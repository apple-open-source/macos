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
   SecCBOR.h
*/

#import <Foundation/Foundation.h>
#import <Security/Security.h>

NS_ASSUME_NONNULL_BEGIN

// NOTE: This is not a full CBOR implementation, only the writer has been implemented
// along with the types below implemented.  The implemented field types are CTAP2 compliant
//
// https://w3c.github.io/webauthn/#sctn-conforming-all-classes
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#ctap2-canonical-cbor-encoding-form
typedef enum {
    SecCBORType_Unsigned = 0,
    SecCBORType_Negative = 1,
    SecCBORType_ByteString = 2,
    SecCBORType_String = 3,
    SecCBORType_Array = 4,
    SecCBORType_Map = 5,
    SecCBORType_None = -1,
} SecCBORType;


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORValue: NSObject

@property (nonatomic, readonly) SecCBORType fieldType;
@property (nonatomic, readonly) uint8_t fieldValue;

- (void)write:(NSMutableData *)output;
- (void)encodeStartItems:(uint64_t)items output:(NSMutableData *)output;
- (void)setAdditionalInformation:(uint8_t)item1 item2:(uint8_t)additionalInformation output:(NSMutableData *)output;
- (void)setUint:(uint8_t)item1 item2:(uint64_t)value output:(NSMutableData *)output;

@end


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORUnsigned : SecCBORValue {
    NSUInteger m_data;
}
- (instancetype)initWith:(NSUInteger)data;
- (void)write:(NSMutableData *)output;
- (NSComparisonResult)compare:(SecCBORUnsigned *)target;
- (NSString *)getLabel;

@end


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORNegative : SecCBORValue {
    NSInteger m_data;
}
- (instancetype)initWith:(NSInteger)data;
- (void)write:(NSMutableData *)output;
- (NSComparisonResult)compare:(SecCBORNegative *)target;
- (NSString *)getLabel;

@end


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORString : SecCBORValue {
    NSString *m_data;
}
- (instancetype)initWith:(NSString *)data;
- (void)write:(NSMutableData *)output;
- (NSComparisonResult)compare:(SecCBORString *)target;
- (NSString *)getLabel;

@end


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORData: SecCBORValue {
    NSData *m_data;
}
- (instancetype)initWith:(NSData *)data;
- (void)write:(NSMutableData *)output;

@end


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORArray: SecCBORValue {
    NSMutableArray *m_data;
}
- (instancetype)init;
- (instancetype)initWith:(NSArray *)data;
- (void)addObject:(SecCBORValue *)object;
- (void)write:(NSMutableData *)output;

@end


NS_REQUIRES_PROPERTY_DEFINITIONS @interface SecCBORMap: SecCBORValue {
    NSMapTable *m_data;
}
- (instancetype)init;
- (void)setKey:(SecCBORValue *)key value:(SecCBORValue *)data;
- (NSArray *)getSortedKeys;
- (NSDictionary *)dictionaryRepresentation;
- (void)write:(NSMutableData *)output;

@end

NS_ASSUME_NONNULL_END
