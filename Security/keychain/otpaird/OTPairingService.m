#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <IDS/IDS.h>
#import <KeychainCircle/KeychainCircle.h>
#import <os/assumes.h>
#import <xpc/private.h>

#if TARGET_OS_WATCH
#import <Security/OTControl.h>
#endif /* TARGET_OS_WATCH */

#if !TARGET_OS_SIMULATOR
#import <MobileKeyBag/MobileKeyBag.h>
#endif /* !TARGET_OS_SIMULATOR */

#import "OTPairingService.h"
#import "OTPairingPacketContext.h"
#import "OTPairingSession.h"
#import "OTPairingConstants.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"

#define WAIT_FOR_UNLOCK_DURATION (120ull)

@interface OTPairingService ()
@property dispatch_queue_t queue;
@property IDSService *service;
@property dispatch_source_t unlockTimer;
@property int notifyToken;
@property (nonatomic, strong) OTDeviceInformationActualAdapter *deviceInfo;
@property OTPairingSession *session;
@end

@implementation OTPairingService

+ (instancetype)sharedService
{
    static dispatch_once_t once;
    static OTPairingService *service;

    dispatch_once(&once, ^{
        service = [[OTPairingService alloc] init];
    });

    return service;
}

- (instancetype)init
{
    if ((self = [super init])) {
        self.queue = dispatch_queue_create("com.apple.security.otpaird", DISPATCH_QUEUE_SERIAL);
        self.service = [[IDSService alloc] initWithService:OTPairingIDSServiceName];
        [self.service addDelegate:self queue:self.queue];
        self.notifyToken = NOTIFY_TOKEN_INVALID;
        self.deviceInfo = [[OTDeviceInformationActualAdapter alloc] init];
    }
    return self;
}

- (NSString *)pairedDeviceNotificationName
{
	NSString *result = nil;
	for (IDSDevice *device in self.service.devices) {
		if (device.isDefaultPairedDevice) {
			result = [NSString stringWithFormat:@"ids-device-state-%@", device.uniqueIDOverride];
			break;
		}
	}
	return result;
}

#if TARGET_OS_WATCH
- (void)initiatePairingWithCompletion:(OTPairingCompletionHandler)completionHandler
{
    dispatch_assert_queue_not(self.queue);

    if ([self _octagonInClique]) {
        os_log(OS_LOG_DEFAULT, "already in octagon, bailing");
        completionHandler(false, [NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeAlreadyIn description:@"already in octagon"]);
        return;
    }

    dispatch_async(self.queue, ^{
        if (self.session != nil) {
            completionHandler(false, [NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeBusy description:@"pairing in progress"]);
            return;
        }

        self.session = [[OTPairingSession alloc] initWithDeviceInfo:self.deviceInfo];
        self.session.completionHandler = completionHandler;
        [self sendReplyToPacket];
    });
}
#endif /* TARGET_OS_WATCH */

// Should be a delegate method - future refactor
- (void)session:(__unused OTPairingSession *)session didCompleteWithSuccess:(bool)success error:(NSError *)error
{
    os_assert(self.session == session);

#if TARGET_OS_WATCH
    self.session.completionHandler(success, error);
#endif /* TARGET_OS_WATCH */

    self.session = nil;
    self.unlockTimer = nil;
}

#pragma mark -

- (void)sendReplyToPacket
{
    dispatch_assert_queue(self.queue);

#if !TARGET_OS_SIMULATOR
    NSDictionary *lockOptions;
    CFErrorRef lockError = NULL;

    lockOptions = @{
        (__bridge NSString *)kMKBAssertionTypeKey : (__bridge NSString *)kMKBAssertionTypeOther,
        (__bridge NSString *)kMKBAssertionTimeoutKey : @(60),
    };
    self.session.lockAssertion = MKBDeviceLockAssertion((__bridge CFDictionaryRef)lockOptions, &lockError);

    if (self.session.lockAssertion) {
        [self stopWaitingForDeviceUnlock];
        [self exchangePacketAndReply];
    } else {
        os_log(OS_LOG_DEFAULT, "Failed to obtain lock assertion: %@", lockError);
        if (lockError) {
            CFRelease(lockError);
        }
        [self waitForDeviceUnlock];
    }
#else /* TARGET_OS_SIMULATOR */
    [self exchangePacketAndReply];
#endif /* !TARGET_OS_SIMULATOR */
}

- (void)deviceUnlockTimedOut
{
    dispatch_assert_queue(self.queue);

    /* Should not happen; be safe. */
    if (self.session == nil) {
        return;
    }

#if TARGET_OS_IOS
    OTPairingPacketContext *packet = self.session.packet;
    self.session.packet = nil;

    os_assert(packet != nil); // the acceptor always responds to a request packet, it's never initiating

    NSError *unlockError = [NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeLock description:@"timed out waiting for companion unlock"];
    NSMutableDictionary *message = [[NSMutableDictionary alloc] init];
    message[OTPairingIDSKeyMessageType] = @(OTPairingIDSMessageTypeError);
    message[OTPairingIDSKeySession] = self.session.identifier;
    message[OTPairingIDSKeyErrorDescription] = unlockError.description;
    NSString *toID = packet.fromID;
    NSString *responseIdentifier = packet.outgoingResponseIdentifier;
    [self _sendMessage:message to:toID identifier:responseIdentifier];

    [self scheduleGizmoPoke];
#endif /* TARGET_OS_IOS */

    [self session:self.session didCompleteWithSuccess:false error:[NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeLock description:@"timed out waiting for unlock"]];
}

- (void)exchangePacketAndReply
{
    dispatch_assert_queue(self.queue);

    OTPairingPacketContext *packet = self.session.packet;
    self.session.packet = nil;

    [self.session.channel exchangePacket:packet.packetData complete:^(BOOL complete, NSData *responsePacket, NSError *channelError) {
        /* this runs on a variety of different queues depending on the step (caller's queue, or an NSXPC queue) */

        dispatch_async(self.queue, ^{
            NSString *toID;
            NSString *responseIdentifier;

            os_log(OS_LOG_DEFAULT, "exchangePacket: complete=%d responsePacket=%@ channelError=%@", complete, responsePacket, channelError);

            if (self.session == nil) {
                os_log(OS_LOG_DEFAULT, "pairing session went away, dropping exchangePacket response");
                return;
            }

            if (channelError != nil) {
#if TARGET_OS_IOS
                NSMutableDictionary *message = [[NSMutableDictionary alloc] init];
                message[OTPairingIDSKeyMessageType] = @(OTPairingIDSMessageTypeError);
                message[OTPairingIDSKeySession] = self.session.identifier;
                message[OTPairingIDSKeyErrorDescription] = channelError.description;
                os_assert(packet != nil); // the acceptor always responds to a request packet, it's never initiating
                toID = packet.fromID;
                responseIdentifier = packet.outgoingResponseIdentifier;
                [self _sendMessage:message to:toID identifier:responseIdentifier];
#endif

                [self session:self.session didCompleteWithSuccess:false error:[NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeKCPairing description:@"exchangePacket" underlying:channelError]];

                return;
            }

            if (responsePacket != nil) {
                NSDictionary *message = @{
                    OTPairingIDSKeyMessageType : @(OTPairingIDSMessageTypePacket),
                    OTPairingIDSKeySession : self.session.identifier,
                    OTPairingIDSKeyPacket : responsePacket,
                };
                toID = packet ? packet.fromID : IDSDefaultPairedDevice;
                responseIdentifier = packet ? packet.outgoingResponseIdentifier : nil;
                [self _sendMessage:message to:toID identifier:responseIdentifier];
            }

            if (complete) {
                [self session:self.session didCompleteWithSuccess:true error:nil];
            }
        });
    }];
}

- (void)_sendMessage:(NSDictionary *)message to:(NSString *)toID identifier:(NSString *)responseIdentifier
{
    [self _sendMessage:message to:toID identifier:responseIdentifier expectReply:YES];
}

- (void)_sendMessage:(NSDictionary *)message to:(NSString *)toID identifier:(NSString *)responseIdentifier expectReply:(BOOL)expectReply
{
    dispatch_assert_queue(self.queue);

    NSSet *destinations;
    NSMutableDictionary *options;
    NSString *identifier = nil;
    NSError *error = nil;
    BOOL sendResult = NO;

    destinations = [NSSet setWithObject:toID];

    options = [NSMutableDictionary new];
    options[IDSSendMessageOptionForceLocalDeliveryKey] = @YES;
    options[IDSSendMessageOptionExpectsPeerResponseKey] = @(expectReply); // TODO: when are we complete??
    if (responseIdentifier != nil) {
        options[IDSSendMessageOptionPeerResponseIdentifierKey] = responseIdentifier;
    }

    sendResult = [self.service sendMessage:message
                            toDestinations:destinations
                                  priority:IDSMessagePriorityDefault
                                   options:options
                                identifier:&identifier
                                     error:&error];
    if (sendResult) {
        /* sentMessageIdentifier is used to validate the next reply; do not set if no reply is expected. */
        if (expectReply) {
            self.session.sentMessageIdentifier = identifier;
        }
    } else {
        os_log(OS_LOG_DEFAULT, "send message failed (%@): %@", identifier, error);
        // On iOS, do nothing; watch will time out waiting for response.
        [self session:self.session didCompleteWithSuccess:false error:[NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeIDS description:@"IDS message send failure" underlying:error]];
    }
}

#pragma mark IDSServiceDelegate methods

- (void)service:(IDSService *)service account:(__unused IDSAccount *)account incomingMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context
{
    dispatch_assert_queue(self.queue);

    OTPairingPacketContext *packet = nil;
    bool validateIdentifier = true;

    os_log(OS_LOG_DEFAULT, "IDS message from %@: %@", fromID, message);

    packet = [[OTPairingPacketContext alloc] initWithMessage:message fromID:fromID context:context];

    if (packet.messageType == OTPairingIDSMessageTypePoke) {
#if TARGET_OS_WATCH
        // on self.queue now, but initiatePairingWithCompletion: must _not_ be on self.queue
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            os_log(OS_LOG_DEFAULT, "companion claims to be unlocked, retrying");
            [self initiatePairingWithCompletion:^(bool success, NSError *error) {
                if (success) {
                    os_log(OS_LOG_DEFAULT, "companion-unlocked retry succeeded");
                } else {
                    os_log(OS_LOG_DEFAULT, "companion-unlocked retry failed: %@", error);
                }
            }];
        });
#endif /* TARGET_OS_WATCH */
        return;
    }

    /*
     * Check for missing/invalid session identifier. Since the watch is the initiator,
     * iOS responds to an invalid session identifier by resetting state and starting over.
     */
    if (packet.sessionIdentifier == nil) {
        os_log(OS_LOG_DEFAULT, "ignoring message with no session identifier (old build?)");
        return;
    } else if (![packet.sessionIdentifier isEqualToString:self.session.identifier]) {
#if TARGET_OS_WATCH
        os_log(OS_LOG_DEFAULT, "unknown session identifier, dropping message");
        return;
#elif TARGET_OS_IOS
        os_log(OS_LOG_DEFAULT, "unknown session identifier %@, creating new session object", packet.sessionIdentifier);
        self.session = [[OTPairingSession alloc] initWithDeviceInfo:self.deviceInfo identifier:packet.sessionIdentifier];
        validateIdentifier = false;
#endif /* TARGET_OS_IOS */
    }

    if (validateIdentifier && ![self.session.sentMessageIdentifier isEqualToString:packet.incomingResponseIdentifier]) {
        os_log(OS_LOG_DEFAULT, "ignoring message with unrecognized incomingResponseIdentifier");
        return;
    }

    switch (packet.messageType) {
    case OTPairingIDSMessageTypeError:
        [self session:self.session didCompleteWithSuccess:false error:[NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeRemote description:@"companion error" underlying:packet.error]];
        break;
    case OTPairingIDSMessageTypePacket:
        self.session.packet = packet;
        [self sendReplyToPacket];
        break;
    case OTPairingIDSMessageTypePoke:
        /*impossible*/
        break;
    }
}

- (void)service:(__unused IDSService *)service account:(__unused IDSAccount *)account identifier:(NSString *)identifier didSendWithSuccess:(BOOL)success error:(NSError *)error context:(__unused IDSMessageContext *)context
{
    dispatch_assert_queue(self.queue);

    /* Only accept callback if it is for the recently-sent message. */
    if (![self.session.sentMessageIdentifier isEqualToString:identifier]) {
        os_log(OS_LOG_DEFAULT, "ignoring didSendWithSuccess callback for unexpected identifier: %@", identifier);
        return;
    }

    if (!success) {
        os_log(OS_LOG_DEFAULT, "unsuccessfully sent message (%@): %@", identifier, error);
        // On iOS, do nothing; watch will time out waiting for response.
        [self session:self.session didCompleteWithSuccess:false error:[NSError errorWithDomain:OTPairingErrorDomain code:OTPairingErrorTypeIDS description:@"IDS message failed to send" underlying:error]];
    }
}

#pragma mark lock state handling

#if !TARGET_OS_SIMULATOR
- (void)waitForDeviceUnlock
{
    dispatch_assert_queue(self.queue);

    static dispatch_once_t once;
    static dispatch_source_t lockStateCoalescingSource;
    uint32_t notify_status;
    int token;

    dispatch_once(&once, ^{
        // Everything here is on one queue, so we sometimes get several notifications while busy doing something else.
        lockStateCoalescingSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR, 0, 0, self.queue);
        dispatch_source_set_event_handler(lockStateCoalescingSource, ^{
            [self sendReplyToPacket];
        });
        dispatch_activate(lockStateCoalescingSource);
    });

    if (self.notifyToken == NOTIFY_TOKEN_INVALID) {
        notify_status = notify_register_dispatch(kMobileKeyBagLockStatusNotificationID, &token, self.queue, ^(__unused int t) {
            dispatch_source_merge_data(lockStateCoalescingSource, 1);
        });
        if (os_assumes_zero(notify_status) == NOTIFY_STATUS_OK) {
            self.notifyToken = token;
        }
    }

    self.unlockTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, self.queue);
    dispatch_source_set_timer(self.unlockTimer, dispatch_time(DISPATCH_TIME_NOW, WAIT_FOR_UNLOCK_DURATION * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_source_set_event_handler(self.unlockTimer, ^{
        [self stopWaitingForDeviceUnlock];
        [self deviceUnlockTimedOut];
    });
    dispatch_activate(self.unlockTimer);

    // double-check to prevent race condition, try again soon
    if (MKBGetDeviceLockState(NULL) == kMobileKeyBagDeviceIsUnlocked) {
        [self stopWaitingForDeviceUnlock];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5ull * NSEC_PER_SEC), self.queue, ^{
            [self sendReplyToPacket];
        });
    }
}

- (void)stopWaitingForDeviceUnlock
{
    dispatch_assert_queue(self.queue);

    uint32_t notify_status;

    if (self.notifyToken != NOTIFY_TOKEN_INVALID) {
        notify_status = notify_cancel(self.notifyToken);
        os_assumes_zero(notify_status);
        self.notifyToken = NOTIFY_TOKEN_INVALID;
    }

    if (self.unlockTimer != nil) {
        dispatch_source_cancel(self.unlockTimer);
        self.unlockTimer = nil;
    }
}
#endif /* !TARGET_OS_SIMULATOR */

#if TARGET_OS_IOS
- (void)scheduleGizmoPoke
{
    xpc_object_t criteria;

    criteria = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(criteria, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_MAINTENANCE);
    xpc_dictionary_set_int64(criteria, XPC_ACTIVITY_DELAY, 0ll);
    xpc_dictionary_set_int64(criteria, XPC_ACTIVITY_GRACE_PERIOD, 0ll);
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REPEATING, false);
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_ALLOW_BATTERY, true);
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_REQUIRES_CLASS_A, true);
    xpc_dictionary_set_bool(criteria, XPC_ACTIVITY_COMMUNICATES_WITH_PAIRED_DEVICE, true);

    os_log(OS_LOG_DEFAULT, "scheduling XPC Activity to inform gizmo of companion unlock");
    xpc_activity_register(OTPairingXPCActivityPoke, criteria, ^(xpc_activity_t activity) {
        xpc_activity_state_t state = xpc_activity_get_state(activity);
        if (state == XPC_ACTIVITY_STATE_RUN) {
            dispatch_sync(self.queue, ^{
                os_log(OS_LOG_DEFAULT, "poking gizmo now");
                NSDictionary *message = @{
                    OTPairingIDSKeyMessageType : @(OTPairingIDSMessageTypePoke),
                };
                [self _sendMessage:message to:IDSDefaultPairedDevice identifier:nil expectReply:NO];
            });
        }
    });
}
#endif /* TARGET_OS_IOS */

#pragma mark Octagon Clique Status

#if TARGET_OS_WATCH
- (bool)_octagonInClique
{
    __block bool result = false;
    NSError *ctlError = nil;

    OTControl *ctl = [OTControl controlObject:true error:&ctlError];
    if (ctl != nil) {
        OTOperationConfiguration *config = [OTOperationConfiguration new];
        config.useCachedAccountStatus = true;
        [ctl fetchCliqueStatus:nil context:OTDefaultContext configuration:config reply:^(CliqueStatus cliqueStatus, NSError * _Nullable error) {
            result = (cliqueStatus == CliqueStatusIn);
        }];
    } else {
        os_log(OS_LOG_DEFAULT, "failed to acquire OTControl: %@", ctlError);
    }

    return result;
}
#endif /* TARGET_OS_WATCH */

@end
