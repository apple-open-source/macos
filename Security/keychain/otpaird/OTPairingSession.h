#import <Foundation/Foundation.h>
#import "OTPairingPacketContext.h"
#import "OTPairingService.h"

#import "keychain/ot/OTDeviceInformationAdapter.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTPairingSession : NSObject

@property (readonly) NSString *identifier;
@property (readwrite, nullable) OTPairingPacketContext *packet;
@property (readonly) KCPairingChannel *channel;
@property (readwrite) NSString *sentMessageIdentifier;

- (instancetype)initAsInitiator:(bool)initiator deviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo identifier:(nullable NSString *)identifier;
- (instancetype)init NS_UNAVAILABLE;

#if !TARGET_OS_SIMULATOR
- (BOOL)acquireLockAssertion;
#endif /* !TARGET_OS_SIMULATOR */

- (void)addCompletionHandler:(OTPairingCompletionHandler)completionHandler;

- (void)didCompleteWithSuccess:(bool)success error:(NSError *)error;

@end

NS_ASSUME_NONNULL_END
