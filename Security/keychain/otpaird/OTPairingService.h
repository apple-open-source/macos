#import <IDS/IDS.h>
#import "OTPairingConstants.h"

NS_ASSUME_NONNULL_BEGIN

typedef void (^OTPairingCompletionHandler)(bool success, NSError *error);

@interface OTPairingService : NSObject <IDSServiceDelegate>

@property (readonly, nullable) NSString *pairedDeviceNotificationName;

+ (instancetype)sharedService;
- (instancetype)init NS_UNAVAILABLE;

- (void)initiatePairingWithCompletion:(OTPairingCompletionHandler)completionHandler;

@end

NS_ASSUME_NONNULL_END
