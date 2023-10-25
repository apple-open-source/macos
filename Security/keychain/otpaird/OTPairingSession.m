#import <KeychainCircle/KeychainCircle.h>
#import <os/assumes.h>

#if !TARGET_OS_SIMULATOR
#import <MobileKeyBag/MobileKeyBag.h>
#endif /* !TARGET_OS_SIMULATOR */

#import "OTPairingSession.h"

@interface OTPairingSession ()
@property (readwrite) NSString *identifier;
@property (readwrite) KCPairingChannel *channel;
@property (readwrite) NSMutableArray<OTPairingCompletionHandler>* completionHandlers;
@end

@implementation OTPairingSession

- (instancetype)initAsInitiator:(bool)initiator deviceInfo:(OTDeviceInformationActualAdapter *)deviceInfo identifier:(NSString *)identifier
{
    KCPairingChannelContext *channelContext = nil;

    if ((self = [super init])) {
        channelContext = [[KCPairingChannelContext alloc] init];
        channelContext.uniqueClientID = [NSUUID UUID].UUIDString;
        channelContext.uniqueDeviceID = [NSUUID UUID].UUIDString;
        channelContext.intent = KCPairingIntent_Type_SilentRepair;
        channelContext.model = deviceInfo.modelID;
        channelContext.osVersion = deviceInfo.osVersion;

        if (initiator) {
            os_assert(identifier == nil);
            self.identifier = [[NSUUID UUID] UUIDString];
            self.channel = [KCPairingChannel pairingChannelInitiator:channelContext];
        } else {
            os_assert(identifier != nil);
            self.identifier = identifier;
            self.channel = [KCPairingChannel pairingChannelAcceptor:channelContext];
        }
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

- (void)addCompletionHandler:(OTPairingCompletionHandler)completionHandler
{
    if (self.completionHandlers == nil) {
        self.completionHandlers = [[NSMutableArray alloc] init];
    }
    [self.completionHandlers addObject:completionHandler];
}

- (void)didCompleteWithSuccess:(bool)success error:(NSError *)error
{
    for (OTPairingCompletionHandler completionHandler in self.completionHandlers) {
        completionHandler(success, error);
    }
}

@end
