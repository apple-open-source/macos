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
} KCJoiningError;

@interface NSError(KCJoiningError)
+ (instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString *) format
                                     arguments:(va_list) va;
+ (instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString *) format, ...;
- (instancetype) initWithJoiningError:(KCJoiningError) code
                                     userInfo:(NSDictionary *)dict;
@end

void KCJoiningErrorCreate(KCJoiningError code, NSError* _Nullable * _Nullable error, NSString* _Nonnull format, ...);

NS_ASSUME_NONNULL_END
