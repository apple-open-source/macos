//
//  KCJoiningMessages.m
//  Security
//
//  Created by Mitch Adler on 2/17/16.
//
//

#import <Foundation/Foundation.h>
#import <AssertMacros.h>

#import <KeychainCircle/KCDer.h>
#import <KeychainCircle/KCError.h>
#import <KeychainCircle/KCJoiningMessages.h>
#import <utilities/debugging.h>

#include <corecrypto/ccder.h>


@implementation KCJoiningMessage

+ (nullable instancetype) messageWithDER: (NSData*) message
                                   error: (NSError**) error {
    return [[KCJoiningMessage alloc] initWithDER: message error: nil];
}

+ (nullable instancetype) messageWithType: (KCJoiningMessageType) type
                                     data: (NSData*) firstData
                                    error: (NSError**) error {
    return [[KCJoiningMessage alloc] initWithType:type data:firstData payload:nil error:error];
}

+ (nullable instancetype) messageWithType: (KCJoiningMessageType) type
                                     data: (NSData*) firstData
                                  payload: (NSData*) secondData
                                    error: (NSError**) error {
    return [[KCJoiningMessage alloc] initWithType:type data:firstData payload:secondData error:error];
    
}

- (bool) inflatePartsOfEncoding: (NSError**) error {
    const uint8_t *der = self.der.bytes;
    const uint8_t *der_end = der + self.der.length;

    const uint8_t *sequence_end = 0;

    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der, der_end);

    if (der == 0) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Not sequence");
        return false;
    }

    if (sequence_end != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Extra data at end of message");
        return false;
    }

    uint64_t type;
    der = ccder_decode_uint64(&type, der, der_end);

    self->_type = (type > kLargestMessageType) ? kUnknown : (KCJoiningMessageType) type;

    NSData* firstData;
    NSData* secondData;

    der = kcder_decode_data_nocopy(&firstData, error, der, der_end);

    if (der != der_end) {
        der = kcder_decode_data_nocopy(&secondData, error, der, der_end);
    }

    self->_firstData = firstData;
    self->_secondData = secondData;

    if (der != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Extra in sequence");
        return false;
    }

    return true;
}

+ (size_t) encodedSizeType: (KCJoiningMessageType) type
                      data: (NSData*) firstData
                   payload: (nullable NSData*) secondData
                     error: (NSError**) error {
    size_t type_size = ccder_sizeof_uint64(type);

    size_t srp_data_size = kcder_sizeof_data(firstData, error);
    if (srp_data_size == 0) return 0;

    size_t encrypted_payload_size = 0;

    if (secondData != nil) {
        encrypted_payload_size = kcder_sizeof_data(secondData, error);
        if (srp_data_size == 0) return 0;
    }


    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, type_size + srp_data_size + encrypted_payload_size);
}

+ (nullable NSData*) encodeToDERType: (KCJoiningMessageType) type
                                data: (NSData*) firstData
                             payload: (nullable NSData*) secondData
                               error: (NSError**) error {

    size_t length = [KCJoiningMessage encodedSizeType:type
                                                 data:firstData
                                              payload:secondData
                                                error: error];
    if (length == 0) return nil;

    NSMutableData* encoded = [NSMutableData dataWithLength: length];

    uint8_t* der = encoded.mutableBytes;
    uint8_t* der_end = der + encoded.length;

    uint8_t* encode_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                ccder_encode_uint64(type, der,
                                kcder_encode_data(firstData, error, der,
                                kcder_encode_data_optional(secondData, error, der, der_end))));

    if (encode_end == NULL) return nil;
    if (encode_end != der) {
        KCJoiningErrorCreate(kDEREncodingFailed, error, @"Size didn't match encoding");
        return nil;
    }

    return encoded;
}

- (nullable instancetype) initWithDER: (NSData*) message
                       error: (NSError**) error {
    self = [super init];

    self->_der = [NSData dataWithData: message];

    return [self inflatePartsOfEncoding: error] ? self : nil;
}

- (nullable instancetype) initWithType: (KCJoiningMessageType) type
                         data: (NSData*) firstData
                      payload: (nullable NSData*) secondData
                        error: (NSError**) error {
    self = [super init];

    self->_der = [KCJoiningMessage encodeToDERType:type
                                              data:firstData
                                           payload:secondData
                                             error:error];
    if (self->_der == nil) return nil;

    return [self inflatePartsOfEncoding: error] ? self : nil;
}

@end


@implementation NSData(KCJoiningMessages)

+ (nullable instancetype) dataWithEncodedString: (NSString*) string
                                          error: (NSError**) error {
    size_t result_size = kcder_sizeof_string(string, error);
    if (result_size == 0) return nil;

    NSMutableData *result = [NSMutableData dataWithLength: result_size];

    uint8_t *der = result.mutableBytes;
    uint8_t *der_end = der + result.length;

    uint8_t *encode_done = kcder_encode_string(string, error,
                                                der, der_end);

    if (encode_done != der) {
        KCJoiningErrorCreate(kDEREncodingFailed, error, @"extra data");
        return nil;
    }

    return result;
}

+ (nullable instancetype) dataWithEncodedSequenceData: (NSData*) data1
                                                 data: (NSData*) data2
                                                error: (NSError**) error {
    size_t result_size = sizeof_seq_data_data(data1, data2, error);
    if (result_size == 0) return nil;

    NSMutableData *result = [NSMutableData dataWithLength: result_size];

    uint8_t *der = result.mutableBytes;
    uint8_t *der_end = der + result.length;

    uint8_t *encode_done = encode_seq_data_data(data1, data2, error,
                                                der, der_end);

    if (encode_done != der) {
        KCJoiningErrorCreate(kDEREncodingFailed, error, @"extra data");
        return nil;
    }

    return result;
}

- (bool) decodeSequenceData: (NSData* _Nullable * _Nonnull) data1
                       data: (NSData* _Nullable * _Nonnull) data2
                      error: (NSError** _Nullable) error {

    return NULL != decode_seq_data_data(data1, data2, error, self.bytes, self.bytes + self.length);
}

+ (nullable instancetype) dataWithEncodedSequenceString: (NSString*) string
                                                   data: (NSData*) data
                                                  error: (NSError**) error {
    size_t result_size = sizeof_seq_string_data(string, data, error);
    if (result_size == 0) return nil;

    NSMutableData *result = [NSMutableData dataWithLength: result_size];

    uint8_t *der = result.mutableBytes;
    uint8_t *der_end = der + result.length;

    uint8_t *encode_done = encode_seq_string_data(string, data, error,
                                                der, der_end);

    if (encode_done != der) {
        KCJoiningErrorCreate(kDEREncodingFailed, error, @"extra data");
        return nil;
    }

    return result;
}

- (bool) decodeSequenceString: (NSString* _Nullable * _Nonnull) string
                         data: (NSData* _Nullable * _Nonnull) data
                        error: (NSError** _Nullable) error {
    return NULL != decode_seq_string_data(string, data, error, self.bytes, self.bytes + self.length);
}

@end

@implementation NSString(KCJoiningMessages)
+ (nullable instancetype) decodeFromDER: (NSData*)der error: (NSError** _Nullable) error {
    NSString* result = nil;
    const uint8_t* decode_result = kcder_decode_string(&result, error, der.bytes, der.bytes+der.length);
    if (decode_result == nil) return nil;
    if (decode_result != der.bytes + der.length) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"extra data in string");
        return nil;
    }

    return result;
}
@end


NSData* extractStartFromInitialMessage(NSData* initialMessage, uint64_t* version, NSString** uuidString, NSError** error) {
    NSData* result = nil;
    const uint8_t *der = [initialMessage bytes];
    const uint8_t *der_end = der + [initialMessage length];
    const uint8_t *parse_end = decode_initialmessage(&result, error, der, der_end);

    // Allow extra stuff in here for future start messages.
    if (parse_end == NULL) {
        return nil;
    }
    else if (parse_end != der_end) {
        NSData *extraStuff = nil;
        NSData *uuid = nil;
        uint64_t piggy_version = 0;
        parse_end = decode_version1(&extraStuff, &uuid, &piggy_version, error, parse_end, der_end);
        require_action_quiet(parse_end != NULL, fail, secerror("decoding piggybacking uuid and version failed (v1)"));
        *uuidString = [[NSString alloc] initWithData:uuid encoding:NSUTF8StringEncoding];
        *version = piggy_version;
    }
fail:
    return result;

}

const uint8_t* decode_version1(NSData** data, NSData** uuid, uint64_t *piggy_version, NSError** error,
                                     const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    uint64_t versionFromBlob = 0;
    der = ccder_decode_uint64(&versionFromBlob, der, der_end);
    
    if (der == NULL) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Version mising");
        return nil;
    }
    
    if(versionFromBlob == 1){ //decode uuid 
        size_t payload_size = 0;
        const uint8_t *payload = ccder_decode_tl(CCDER_OCTET_STRING, &payload_size, der, der_end);
        
        *uuid = [NSData dataWithBytes: (void*)payload length: payload_size];
        *piggy_version = versionFromBlob;
    }
    else{
        KCJoiningErrorCreate(kDERUnknownVersion, error, @"Bad version: %llu", versionFromBlob);
        return nil;
    }
    
    return der;
}
const uint8_t* decode_initialmessage(NSData** data, NSError** error,
                                     const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    const uint8_t *sequence_end = 0;
    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der, der_end);

    if (der == NULL || sequence_end != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Decoding failed");
        return nil;
    }
    uint64_t version = 0;
    der = ccder_decode_uint64(&version, der, der_end);

    if (der == NULL) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Version mising");
        return nil;
    }

    if (version != 0) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"Bad version: %llu", version);
        return nil;
    }
    
    return kcder_decode_data(data, error, der, der_end);
}

size_t sizeof_initialmessage(NSData*data) {
    size_t version_size = ccder_sizeof_uint64(0);
    if (version_size == 0) {
        return 0;
    }
    size_t message_size = kcder_sizeof_data(data, nil);
    if (message_size == 0) {
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, version_size + message_size);
}

uint8_t* encode_initialmessage(NSData* data, NSError**error,
                               const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                       ccder_encode_uint64(0, der,
                                                           kcder_encode_data(data, error, der, der_end)));
}

size_t sizeof_initialmessage_version1(NSData*data, uint64_t version1, NSData *uuid) {
    size_t version_size = ccder_sizeof_uint64(0);
    if (version_size == 0) {
        return 0;
    }
    size_t message_size = kcder_sizeof_data(data, nil);
    if (message_size == 0) {
        return 0;
    }
    size_t version1_size = ccder_sizeof_uint64(version1);
    if (version1_size == 0) {
        return 0;
    }
    size_t uuid_size = kcder_sizeof_data(uuid, nil);
    if (message_size == 0) {
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, version_size + message_size + version1_size + uuid_size);
}


uint8_t*  encode_initialmessage_version1(NSData* data, NSData* uuidData, uint64_t piggy_version, NSError**error,
                                         const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                       ccder_encode_uint64(0, der,
                                                           kcder_encode_data(data, error, der,
                                                                             ccder_encode_uint64(piggy_version, der,
                                                                                                 kcder_encode_data(uuidData, error, der, der_end)))));
    
}

size_t sizeof_seq_data_data(NSData*data1, NSData*data2, NSError**error) {
    size_t data1_size = kcder_sizeof_data(data1, error);
    if (data1_size == 0) {
        return 0;
    }
    size_t data2_size = kcder_sizeof_data(data2, error);
    if (data2_size == 0) {
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, data1_size + data2_size);
}

uint8_t* encode_seq_data_data(NSData* data1, NSData*data2, NSError**error,
                                        const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                kcder_encode_data(data1, error, der,
                kcder_encode_data(data2, error, der, der_end)));
}

const uint8_t* decode_seq_data_data(NSData** data1, NSData** data2, NSError** error,
                                    const uint8_t* der, const uint8_t *der_end) {
    if (NULL == der)
        return NULL;

    const uint8_t *sequence_end = 0;
    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der, der_end);

    if (der == NULL || sequence_end != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"decode failed");
        return nil;
    }

    der = kcder_decode_data(data1, error, der, der_end);
    return kcder_decode_data(data2, error, der, der_end);
}

size_t sizeof_seq_string_data(NSString*string, NSData*data, NSError** error) {
    size_t string_size = kcder_sizeof_string(string, error);
    if (string_size == 0) {
        return 0;
    }
    size_t data_size = kcder_sizeof_data(data, error);
    if (data_size == 0) {
        return 0;
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, string_size + data_size);
}

uint8_t* _Nullable encode_seq_string_data(NSString* string, NSData*data, NSError**error,
                                          const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                 kcder_encode_string(string, error, der,
                 kcder_encode_data(data, error, der, der_end)));
}

const uint8_t* _Nullable decode_seq_string_data(NSString* _Nonnull * _Nonnull string, NSData* _Nonnull * _Nonnull data,
                                                NSError** error,
                                                const uint8_t* der, const uint8_t *der_end) {
    if (NULL == der)
        return NULL;

    const uint8_t *sequence_end = 0;
    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der, der_end);

    if (der == NULL || sequence_end != der_end) {
        KCJoiningErrorCreate(kDERUnknownEncoding, error, @"decode failed");
        return nil;
    }

    der = kcder_decode_string(string, error, der, der_end);
    return kcder_decode_data(data, error, der, der_end);
}
