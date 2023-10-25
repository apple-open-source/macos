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

#if !TARGET_OS_SIMULATOR
@property (readwrite, nullable) MKBAssertionRef lockAssertion;
#endif /* !TARGET_OS_SIMULATOR */

- (instancetype)initAsInitiator:(bool)initiator deviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo identifier:(nullable NSString *)identifier;
- (instancetype)init NS_UNAVAILABLE;

- (void)addCompletionHandler:(OTPairingCompletionHandler)completionHandler;

- (void)didCompleteWithSuccess:(bool)success error:(NSError *)error;

@end

NS_ASSUME_NONNULL_END
