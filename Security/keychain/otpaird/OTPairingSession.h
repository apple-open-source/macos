#import <Foundation/Foundation.h>
#import "OTPairingPacketContext.h"
#import "OTPairingService.h"

#import "keychain/ot/OTDeviceInformationAdapter.h"

@interface OTPairingSession : NSObject

@property (readonly) NSString *identifier;
@property (readwrite) OTPairingPacketContext *packet;
@property (readonly) KCPairingChannel *channel;
@property (readwrite) NSString *sentMessageIdentifier;

#if TARGET_OS_WATCH
@property OTPairingCompletionHandler completionHandler;
#endif /* TARGET_OS_WATCH */

#if !TARGET_OS_SIMULATOR
@property (readwrite) MKBAssertionRef lockAssertion;
#endif /* !TARGET_OS_SIMULATOR */

- (instancetype)initWithDeviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo;
- (instancetype)initWithDeviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo identifier:(NSString *)identifier;

@end
