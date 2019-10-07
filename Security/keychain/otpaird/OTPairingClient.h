#import <TargetConditionals.h>

#if TARGET_OS_WATCH

void OTPairingInitiateWithCompletion(dispatch_queue_t queue, void (^completion)(bool success, NSError *));

#endif /* TARGET_OS_WATCH */
