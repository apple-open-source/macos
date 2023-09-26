#import <TargetConditionals.h>

NS_ASSUME_NONNULL_BEGIN

#if TARGET_OS_WATCH

void OTPairingInitiateWithCompletion(dispatch_queue_t queue, void (^completion)(bool success, NSError *));

#endif /* TARGET_OS_WATCH */

NS_ASSUME_NONNULL_END
