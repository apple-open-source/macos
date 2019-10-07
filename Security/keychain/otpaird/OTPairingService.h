#import <IDS/IDS.h>
#import "OTPairingConstants.h"

typedef void (^OTPairingCompletionHandler)(bool success, NSError *error);

@interface OTPairingService : NSObject <IDSServiceDelegate>

+ (instancetype)sharedService;

#if TARGET_OS_WATCH
- (void)initiatePairingWithCompletion:(OTPairingCompletionHandler)completionHandler;
#endif /* TARGET_OS_WATCH */

@end
