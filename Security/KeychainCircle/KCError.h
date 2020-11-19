//
//  KCError.h
//  Security
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef enum {
    kAllocationFailure,
    kDERUnknownEncoding,
    kDERStringEncodingFailed,
    kDEREncodingFailed,
    kDERSpaceExhausted,
    kKCTagMismatch,
    kUnexpectedMessage,
    kInternalError,
    kDERUnknownVersion,
    kProcessApplicationFailure,
    kUnsupportedTrustPlatform,
} KCJoiningError;

@interface NSError(KCJoiningError)
+ (instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString *) format
                                     arguments:(va_list) va NS_FORMAT_FUNCTION(2,0);;
+ (instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString *) format, ... NS_FORMAT_FUNCTION(2,3);;
- (instancetype) initWithJoiningError:(KCJoiningError) code
                                     userInfo:(NSDictionary *)dict;
@end

void KCJoiningErrorCreate(KCJoiningError code, NSError* _Nullable * _Nullable error, NSString* _Nonnull format, ...) NS_FORMAT_FUNCTION(3,4);;

NS_ASSUME_NONNULL_END
