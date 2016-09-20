//
//  KCJoiningMessages.h
//  KeychainCircle
//
//

#import <Foundation/Foundation.h>

// Initial messages are versioned and not typed for negotiation.
NS_ASSUME_NONNULL_BEGIN

NSData* extractStartFromInitialMessage(NSData* initialMessage, NSError** error);

size_t sizeof_initialmessage(NSData*data);
uint8_t* _Nullable encode_initialmessage(NSData* data, NSError**error,
                               const uint8_t *der, uint8_t *der_end);
const uint8_t* _Nullable decode_initialmessage(NSData* _Nonnull * _Nonnull data, NSError** error,
                                     const uint8_t* der, const uint8_t *der_end);

size_t sizeof_seq_data_data(NSData*data1, NSData*data2, NSError** error);
uint8_t* _Nullable encode_seq_data_data(NSData* data, NSData*data2, NSError**error,
                                         const uint8_t *der, uint8_t *der_end);
const uint8_t* _Nullable decode_seq_data_data(NSData* _Nonnull * _Nonnull data1, NSData* _Nonnull * _Nonnull data2,
                                              NSError** error,
                                              const uint8_t* der, const uint8_t *der_end);

size_t sizeof_seq_string_data(NSString*string, NSData*data, NSError** error);
uint8_t* _Nullable encode_seq_string_data(NSString* string, NSData*data, NSError**error,
                                          const uint8_t *der, uint8_t *der_end);
const uint8_t* _Nullable decode_seq_string_data(NSString* _Nonnull * _Nonnull string, NSData* _Nonnull * _Nonnull data,
                                                NSError** error,
                                                const uint8_t* der, const uint8_t *der_end);

@interface NSData(KCJoiningMessages)

+ (nullable instancetype) dataWithEncodedString: (NSString*) string
                                          error: (NSError**) error;

+ (nullable instancetype) dataWithEncodedSequenceData: (NSData*) data1
                                                 data: (NSData*) data2
                                                error: (NSError**) error;

- (bool) decodeSequenceData: (NSData* _Nullable * _Nonnull) data1
                       data: (NSData* _Nullable * _Nonnull) data2
                      error: (NSError** _Nullable) error;


+ (nullable instancetype) dataWithEncodedSequenceString: (NSString*) string
                                                   data: (NSData*) data
                                                  error: (NSError**) error;

- (bool) decodeSequenceString: (NSString* _Nullable * _Nonnull) string
                         data: (NSData* _Nullable * _Nonnull) data
                        error: (NSError** _Nullable) error;
@end

@interface NSString(KCJoiningMessages)
+ (nullable instancetype) decodeFromDER: (NSData*)der error: (NSError** _Nullable) error;
@end

// Subsequent messages have a message type
typedef enum {
    kChallenge = 1,
    kResponse = 2,
    kVerification = 3,
    kPeerInfo = 4,
    kCircleBlob = 5,

    kError = 0,

    kUnknown = 255,

    kLargestMessageType = kUnknown,

} KCJoiningMessageType;


@interface KCJoiningMessage : NSObject

@property (readonly) KCJoiningMessageType type;
@property (readonly) NSData* firstData;
@property (nullable, readonly) NSData* secondData;

@property (readonly) NSData* der;

+ (nullable instancetype) messageWithDER: (NSData*) message
                                   error: (NSError**) error;

+ (nullable instancetype) messageWithType: (KCJoiningMessageType) type
                                     data: (NSData*) firstData
                                    error: (NSError**) error;

+ (nullable instancetype) messageWithType: (KCJoiningMessageType) type
                                     data: (NSData*) firstData
                                  payload: (NSData*) secondData
                                    error: (NSError**) error;


- (nullable instancetype) initWithDER: (NSData*) message
                                error: (NSError**) error NS_DESIGNATED_INITIALIZER;

- (nullable instancetype) initWithType: (KCJoiningMessageType) type
                                  data: (NSData*) firstData
                               payload: (nullable NSData*) secondData
                                 error: (NSError**) error  NS_DESIGNATED_INITIALIZER;


- (instancetype) init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

