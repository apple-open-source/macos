#import <KeychainCircle/KeychainCircle.h>

#if !TARGET_OS_SIMULATOR
#import <MobileKeyBag/MobileKeyBag.h>
#endif /* !TARGET_OS_SIMULATOR */

#import "OTPairingSession.h"

@interface OTPairingSession ()
@property (readwrite) NSString *identifier;
@property (readwrite) KCPairingChannel *channel;
@end

@implementation OTPairingSession

- (instancetype)initWithDeviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo
{
    return [self initWithDeviceInfo:deviceInfo identifier:[[NSUUID UUID] UUIDString]];
}

- (instancetype)initWithDeviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo identifier:(NSString *)identifier
{
    KCPairingChannelContext *channelContext = nil;

    if ((self = [super init])) {
        self.identifier = identifier;

        channelContext = [KCPairingChannelContext new];
        channelContext.uniqueClientID = [NSUUID UUID].UUIDString;
        channelContext.uniqueDeviceID = [NSUUID UUID].UUIDString;
        channelContext.intent = KCPairingIntent_Type_SilentRepair;
        channelContext.model = deviceInfo.modelID;
        channelContext.osVersion = deviceInfo.osVersion;

#if TARGET_OS_WATCH
        self.channel = [KCPairingChannel pairingChannelInitiator:channelContext];
#elif TARGET_OS_IOS
        self.channel = [KCPairingChannel pairingChannelAcceptor:channelContext];
#endif
    }
    return self;
}

- (void)dealloc
{
#if !TARGET_OS_SIMULATOR
    if (self.lockAssertion) {
        CFRelease(self.lockAssertion);
        self.lockAssertion = NULL;
    }
#endif /* !TARGET_OS_SIMULATOR */
}

@end
