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
#if !TARGET_OS_SIMULATOR
@property MKBAssertionRef lockAssertion;
#endif /* !TARGET_OS_SIMULATOR */
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
    if (self->_lockAssertion) {
        CFRelease(self->_lockAssertion);
        self->_lockAssertion = NULL;
    }
#endif /* !TARGET_OS_SIMULATOR */
}

#if !TARGET_OS_SIMULATOR
- (BOOL)acquireLockAssertion
{
    if (self->_lockAssertion == NULL) {
        CFErrorRef lockError = NULL;
        NSDictionary* lockOptions = @{
            (__bridge NSString *)kMKBAssertionTypeKey : (__bridge NSString *)kMKBAssertionTypeOther,
            (__bridge NSString *)kMKBAssertionTimeoutKey : @(60),
        };
        self->_lockAssertion = MKBDeviceLockAssertion((__bridge CFDictionaryRef)lockOptions, &lockError);

        if (self->_lockAssertion == NULL || lockError != NULL) {
            os_log(OS_LOG_DEFAULT, "Failed to obtain lock assertion: %@", lockError);
            if (lockError != NULL) {
                CFRelease(lockError);
            }
        }
    }

    return (self->_lockAssertion != NULL);
}
#endif /* !TARGET_OS_SIMULATOR */

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
