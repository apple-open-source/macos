//
//  KCError.h
//  Security
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString* KCErrorDomain;

typedef NS_ERROR_ENUM(KCErrorDomain, KCJoiningError) {
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
    kFailedToCopyStart = 23,
    kFailedToCreateVersion2Message = 24,
    kFailedToCreateVersion1Message = 25,
    kFailedToEncodeInitialMessage = 26,
    kFailedToCreateHandleChallengeMessage = 27,
    kFailedToHandleChallengeData = 28,
    kNilNextSecret = 29,
    kExpectedVerificationMessageType = 30,
    kVerifyConfirmationFailed = 31,
    kDecryptAndVerifyFailed = 32,
    kDecodeFromDERFailed = 33,
    kFailedToCopyResponseForSecret = 34,
    kFailedToCopyChallengeMessage = 35,
    kFailedToCreateChallengeMessage = 36,
    kUnexpectedMessageTypeExpectedResponse = 37,
    kRetryError = 38,
    kNilErrorData = 39,
    kFailedToCreateErrorResponseMessage = 40,
    kFailedToEncodeData = 41,
    kFailedToEncryptEncodedData = 42,
    kFailedToCreateVerificationMessage = 43,
    kFailedToCreateTLKRequestResponse = 44,
    kReceivedUnexpectedMessageTypeExpectedPeerInfo = 45,
    kFailedToCreateMessageFromJoiningMessage = 46,
    kMessageDoesNotContainPeerInfoData = 47,
    kVoucherCreationFailed = 48,
    kFailedToCreateCircleBlobMessage = 49,
    kFailedToProcessSOSApplication = 50,
    kFailedToExtractStartMessage = 51,
    kFailedToEncryptInitialMessage = 52,
    kFailedToCreatePeerInfoResponse = 53,
    kReceivedUnexpectedMessageTypeRequireCircleBlob = 54,
    kFailedToDecryptCircleBlob = 55,
    kFailedToProcessCircleJoinData = 56,
    kFailedToEncryptVoucherData = 57,
    kUnexpectedVersion = 58,
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
