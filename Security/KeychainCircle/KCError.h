//
//  KCError.h
//  Security
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(int64_t, KCJoiningError) {
    kAllocationFailure = 0,
    kDERUnknownEncoding = 1,
    kDERStringEncodingFailed = 2,
    kDEREncodingFailed = 3,
    kDERSpaceExhausted = 4,
    kKCTagMismatch = 5,
    kUnexpectedMessage = 6,
    kInternalError = 7,
    kDERUnknownVersion = 8,
    kProcessApplicationFailure = 9,
    kUnsupportedTrustPlatform = 10,
    kMissingAcceptorEpoch = 11,
    /* unused kTimedoutWaitingForPrepareRPC = 12 */
    kFailedToEncryptPeerInfo = 13,
    kSOSNotSupportedAndPiggyV2NotSupported = 14,
    kMissingVoucher = 15,
    /* unused kTimedoutWaitingForJoinRPC = 16 */
    kFailureToDecryptCircleBlob = 17,
    kFailureToProcessCircleBlob = 18,
    kStartMessageEmpty = 19,
    kUnableToPiggyBackDueToTrustSystemSupport = 20,
    /* unused kTimedoutWaitingForEpochRPC = 21 */
    /* unused kTimedOutWaitingForVoucher = 22 */
};

@interface NSError(KCJoiningError)
+ (instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString *) format
                                     arguments:(va_list) va NS_FORMAT_FUNCTION(2,0);
+ (instancetype) errorWithJoiningError:(KCJoiningError) code
                                        format:(NSString *) format, ... NS_FORMAT_FUNCTION(2,3);
- (instancetype) initWithJoiningError:(KCJoiningError) code
                                     userInfo:(NSDictionary *)dict;
@end

void KCJoiningErrorCreate(KCJoiningError code, NSError* _Nullable * _Nullable error, NSString* _Nonnull format, ...) NS_FORMAT_FUNCTION(3,4);

NS_ASSUME_NONNULL_END
