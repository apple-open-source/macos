//
//  KCJoiningAcceptSession.m
//  Security
//
//

#import <Foundation/Foundation.h>

#import <KeychainCircle/KCJoiningSession.h>

#import <KeychainCircle/KCError.h>
#import <KeychainCircle/KCDer.h>
#import <KeychainCircle/KCJoiningMessages.h>

#import <KeychainCircle/NSError+KCCreationHelpers.h>
#import "KCInitialMessageData.h"

#include <corecrypto/ccder.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>
#include <utilities/debugging.h>
#include <notify.h>

#if OCTAGON
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "KeychainCircle/KCJoiningAcceptSession+Internal.h"
#import "keychain/ot/proto/generated_source/OTApplicantToSponsorRound2M1.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound2M2.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound1M2.h"
#import "keychain/ot/proto/generated_source/OTGlobalEnums.h"
#import "keychain/ot/proto/generated_source/OTSupportSOSMessage.h"
#import "keychain/ot/proto/generated_source/OTSupportOctagonMessage.h"
#import "keychain/ot/proto/generated_source/OTPairingMessage.h"
#endif

#import <KeychainCircle/PairingChannel.h>

typedef enum {
    kExpectingA,
    kExpectingM,
    kExpectingPeerInfo,
    kAcceptDone
} KCJoiningAcceptSessionState;

@interface KCJoiningAcceptSession ()
@property (readonly) uint64_t dsid;
@property (weak) id<KCJoiningAcceptSecretDelegate> secretDelegate;
@property (weak) id<KCJoiningAcceptCircleDelegate> circleDelegate;
@property (readonly) KCSRPServerContext* context;
@property (readonly) KCAESGCMDuplexSession* session;
@property (readonly) KCJoiningAcceptSessionState state;
@property (readwrite) NSData* startMessage;
@property (readwrite) NSString *piggy_uuid;
@property (readwrite) PiggyBackProtocolVersion piggy_version;
@property (readwrite) NSData* octagon;
#if OCTAGON
@property (nonatomic, strong) OTJoiningConfiguration* joiningConfiguration;
@property (nonatomic, strong) OTControlArguments* controlArguments;
@property (nonatomic, strong) OTControl* otControl;
#endif
@property (nonatomic, strong) NSMutableDictionary *defaults;
@end

@implementation KCJoiningAcceptSession

+ (nullable instancetype) sessionWithInitialMessage: (NSData*) message
                                     secretDelegate: (NSObject<KCJoiningAcceptSecretDelegate>*) secretDelegate
                                     circleDelegate: (NSObject<KCJoiningAcceptCircleDelegate>*) circleDelegate
                                               dsid: (uint64_t) dsid
                                              error: (NSError**) error {

    int cc_error = 0;
    struct ccrng_state * rng = ccrng(&cc_error);

    if (rng == nil) {
        CoreCryptoError(cc_error, error, @"RNG fetch failed");
        return nil;
    }

    return [[KCJoiningAcceptSession alloc] initWithSecretDelegate: secretDelegate
                                                   circleDelegate: circleDelegate
                                                             dsid: dsid
                                                              rng: rng
                                                            error: error];
}

- (bool)setupSession:(NSError**)error {
    NSData* key = [self->_context getKey];

    if (key == nil) {
        KCJoiningErrorCreate(kInternalError, error, @"No session key available");
        return nil;
    }

    self->_session = [KCAESGCMDuplexSession sessionAsReceiver:key context:self.dsid];
#if OCTAGON
    self.session.pairingUUID = self.joiningConfiguration.pairingUUID;
    self.session.altDSID = self.controlArguments.altDSID;
#endif
    self.session.piggybackingVersion = self.piggy_version;

    return self.session != nil;
}

- (nullable instancetype) initWithSecretDelegate: (NSObject<KCJoiningAcceptSecretDelegate>*) secretDelegate
                                  circleDelegate: (NSObject<KCJoiningAcceptCircleDelegate>*) circleDelegate
                                            dsid: (uint64_t) dsid
                                             rng: (struct ccrng_state *)rng
                                           error: (NSError**) error {
    if ((self = [super init])) {

        secnotice("accepting", "initWithSecretDelegate");

        NSString* name = [NSString stringWithFormat: @"%llu", dsid];

        self->_context = [[KCSRPServerContext alloc] initWithUser: name
                                                         password: [secretDelegate secret]
                                                       digestInfo: ccsha256_di()
                                                            group: ccsrp_gp_rfc5054_3072()
                                                     randomSource: rng];
        self.secretDelegate = secretDelegate;
        self.circleDelegate = circleDelegate;
        self->_state = kExpectingA;
        self->_dsid = dsid;
        self->_piggy_uuid = nil;
        self->_defaults = [NSMutableDictionary dictionary];

#if OCTAGON
        self->_otControl = [OTControl controlObject:true error:error];
        self->_piggy_version = kPiggyV2;
        self->_joiningConfiguration = [[OTJoiningConfiguration alloc]initWithProtocolType:@"OctagonPiggybacking"
                                                                           uniqueDeviceID:@"acceptor-deviceid"
                                                                           uniqueClientID:@"requester-deviceid"
                                                                              pairingUUID:[[NSUUID UUID] UUIDString]
                                                                                    epoch:0
                                                                              isInitiator:false];
        self->_controlArguments = [[OTControlArguments alloc] init];
#else
        self->_piggy_version = kPiggyV1;
#endif
    }    
    return self;
}

- (NSString*)stateString {
    switch (self.state) {
        case kExpectingA: return @"→A";
        case kExpectingM: return @"→M";
        case kExpectingPeerInfo: return @"→PeerInfo";
        case kAcceptDone: return @"done";
        default: return [NSString stringWithFormat:@"%d", self.state];
    }
}

- (NSString *)description {
    return [NSString stringWithFormat: @"<KCJoiningAcceptSession: %lld %@ %@ uuid: %@>", self.dsid, [self stateString], self.context, self.piggy_uuid];
}

- (NSData*) copyChallengeMessage: (NSError**) error {
    NSData* challenge = [self.context copyChallengeFor: self.startMessage error: error];
    if (challenge == nil) return nil;

    NSData* srpMessage = [NSData dataWithEncodedSequenceData:self.context.salt data:challenge error:error];

    if (![self setupSession:error]) return nil;

    return srpMessage;
}

#if OCTAGON
- (BOOL)shouldAcceptOctagonRequests {
    __block BOOL result = NO;

    OTOperationConfiguration* configuration = [[OTOperationConfiguration alloc] init];
    configuration.discretionaryNetwork = TRUE;

    [self.otControl fetchTrustStatus:self.controlArguments
                       configuration:configuration
                               reply:^(CliqueStatus status,
                                       NSString* peerID,
                                       NSNumber * _Nullable numberOfPeersInOctagon,
                                       BOOL isExcluded, NSError* _Nullable error)
     {
         secerror("octagon haveSelfEgo: status %d: %@ %@ %d: %@", (int)status,
                  peerID, numberOfPeersInOctagon, isExcluded, error);

         if (status == CliqueStatusIn) {
             result = YES;
         }
     }];

    return result;
}
#endif

- (NSData*)processInitialMessage:(NSData*)initialMessage error:(NSError**)error {
    __block uint64_t version = 0;
    NSString *uuid = nil;
    NSData *octagon = nil;

    self.startMessage = extractStartFromInitialMessage(initialMessage, &version, &uuid, &octagon, error);
    if (self.startMessage == NULL) {
        secerror("joining: failed to extract startMessage: %@", error ? * error : nil);
        return nil;
    }

    if (version == kPiggyV2) {
        /* before we go ahead with octagon, let see if we are an octagon peer */

        if (![self shouldAcceptOctagonRequests]) {
            secerror("joining: octagon refusing octagon acceptor since we don't have a selfEgo");
            if (SOSCCIsSOSTrustAndSyncingEnabled() == NO && self.joiningConfiguration.testsEnabled == NO) {
                secerror("joining: device does not support SOS, failing flow");
                KCJoiningErrorCreate(kUnableToPiggyBackDueToTrustSystemSupport, error, @"Unable to piggyback with device due to lack of trust system support");
                return nil;
            } else {
                secnotice("joining", "device supports SOS, continuing flow with piggyV1");
                version = kPiggyV1;
            }
        } else {
            self.octagon = octagon;
        }
    }

    self.piggy_uuid = uuid;
    self.piggy_version = (PiggyBackProtocolVersion)version;

    NSData* srpMessage = [self copyChallengeMessage: error];
    if (srpMessage == nil) {
        secerror("joining: failed to copy srpMessage: %@", error ? * error : nil);
        return nil;
    }

    self->_state = kExpectingM;

    NSString* piggyVersionMessage = [[NSString alloc]initWithData:self.octagon encoding:NSUTF8StringEncoding];
    __block NSError *captureError = nil;

    if (version == kPiggyV2 && piggyVersionMessage && [piggyVersionMessage isEqualToString:@"o"]) {
        __block NSData* next = nil;

        //fetch epoch
        [self.otControl rpcEpochWithArguments:self.controlArguments
                                configuration:self.joiningConfiguration
                                        reply:^(uint64_t epoch, NSError * _Nullable epochError) {
            if (epochError) {
                secerror("error retrieving next message! :%@", epochError);
                captureError = epochError;
            } else {
                OTPairingMessage* responseMessage = [[OTPairingMessage alloc] init];
                responseMessage.supportsSOS = [[OTSupportSOSMessage alloc] init];
                responseMessage.supportsOctagon = [[OTSupportOctagonMessage alloc] init];

                responseMessage.epoch = [[OTSponsorToApplicantRound1M2 alloc] init];
                responseMessage.epoch.epoch = epoch;

                responseMessage.supportsSOS.supported = SOSCCIsSOSTrustAndSyncingEnabled() ? OTSupportType_supported : OTSupportType_not_supported;
                responseMessage.supportsOctagon.supported = OTSupportType_supported;
                next = responseMessage.data;
            }
        }];

        if (captureError) {
            if (error) {
                *error = captureError;
            }
            return nil;
        }
        return [[KCJoiningMessage messageWithType:kChallenge
                                             data:srpMessage
                                          payload:next
                                            error:error] der];
    } else if (SOSCCIsSOSTrustAndSyncingEnabled() || self.joiningConfiguration.testsEnabled) {

        return [[KCJoiningMessage messageWithType:kChallenge
                                             data:srpMessage
                                            error:error] der];
    }

    KCJoiningErrorCreate(kUnableToPiggyBackDueToTrustSystemSupport, error, @"Unable to piggyback with device due to lack of trust system support");
    return nil;
}

- (NSData*)processResponse:(KCJoiningMessage*)message error:(NSError**)error {
    if ([message type] != kResponse) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected response!");
        return nil;
    }

    id<KCJoiningAcceptSecretDelegate> secretDelegate = self.secretDelegate;

    // We handle failure, don't capture the error.
    NSData* confirmation = [self.context copyConfirmationFor:message.firstData error:NULL];
    if (!confirmation) {
        // Find out what kind of error we should send.
        NSData* errorData = nil;

        KCRetryOrNot status = [secretDelegate verificationFailed: error];
        secerror("processResponse: handle error: %d", (int)status);

        switch (status) {
            case kKCRetryError:
                // We fill in an error if they didn't, but if they did this wont bother.
                KCJoiningErrorCreate(kInternalError, error, @"Delegate returned error without filling in error: %@", secretDelegate);
                return nil;
            case kKCRetryWithSameChallenge:
                errorData = [NSData data];
                break;
            case kKCRetryWithNewChallenge:
                if ([self.context resetWithPassword:[secretDelegate secret] error:error]) {
                    errorData = [self copyChallengeMessage: error];
                }
                break;
        }
        if (errorData == nil) return nil;

        return [[KCJoiningMessage messageWithType:kError
                                             data:errorData
                                            error:error] der];
    }

    NSData* encoded = [NSData dataWithEncodedString:[secretDelegate accountCode] error:error];
    if (encoded == nil)
        return nil;

    NSData* encrypted = [self.session encrypt:encoded error:error];
    if (encrypted == nil) return nil;

    self->_state = kExpectingPeerInfo;

    return [[KCJoiningMessage messageWithType:kVerification
                                         data:confirmation
                                      payload:encrypted
                                        error:error] der];
}

- (NSData*)processSOSApplication:(NSData*) message error:(NSError**)error
{
    NSData* decryptedPayload = [self.session decryptAndVerify:message error:error];
    if (decryptedPayload == nil) return nil;

    id<KCJoiningAcceptCircleDelegate> circleDelegate = self.circleDelegate;

    CFErrorRef cfError = NULL;
    SOSPeerInfoRef ref = SOSPeerInfoCreateFromData(NULL, &cfError, (__bridge CFDataRef) decryptedPayload);
    if (ref == NULL) {
        if (error) *error = (__bridge_transfer NSError*) cfError;
        cfError = NULL;
        return nil;
    }

    NSData* joinData = [circleDelegate circleJoinDataFor:ref error:error];
    if (ref) {
        CFRelease(ref);
        ref = NULL;
    }

    if (joinData == nil) return nil;

    SOSInitialSyncFlags flags = 0;
    switch (self.piggy_version) {
        case kPiggyV0:
            break;
        case kPiggyV1:
            secnotice("acceptor", "piggy version is 1");
            flags |= kSOSInitialSyncFlagTLKs | kSOSInitialSyncFlagiCloudIdentity;
            break;
        case kPiggyV2:
            secnotice("acceptor", "piggy version is 2");
            flags |= kSOSInitialSyncFlagiCloudIdentity;
            break;
    }

    if (flags) {
        //grab iCloud Identities, TLKs
        NSError *localISVError = nil;
        NSData* initialSyncData = [circleDelegate circleGetInitialSyncViews:flags error:&localISVError];

        NSMutableData* growPacket = [[NSMutableData alloc] initWithData:joinData];

        if (initialSyncData == nil) {
            secnotice("piggy", "PB threw an error: %@", localISVError);
        } else {
            [growPacket appendData:initialSyncData];
        }
        joinData = growPacket;

    }

    NSData* encryptedOutgoing = [self.session encrypt:joinData error:error];
    if (encryptedOutgoing == nil) return nil;
    return encryptedOutgoing;
}

#if OCTAGON
- (OTPairingMessage *)createPairingMessageFromJoiningMessage:(KCJoiningMessage *)message error:(NSError**)error
{
    NSData *decryptInitialMessage = [self.session decryptAndVerify:message.firstData error:error];
    if (!decryptInitialMessage) {
        secinfo("KeychainCircle", "Failed to decrypt message first data: %@. Trying legacy OTPairingMessage construction.", error ? *error : @"");
        return [[OTPairingMessage alloc] initWithData:message.firstData];
    } else {
        KCInitialMessageData *initialMessage = [[KCInitialMessageData alloc] initWithData:decryptInitialMessage];
        if (!initialMessage) {
            secerror("Failed to parse InitialMessageData from decrypted message data");
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Failed to parse InitialMessageData from decrypted message data");
            return nil;
        }

        if (!initialMessage.hasPrepareMessage) {
            secerror("InitialMessageData does not contain prepare message");
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected prepare message inside InitialMessageData");
            return nil;
        }

        return [[OTPairingMessage alloc] initWithData:initialMessage.prepareMessage];
    }
}
#endif

- (NSData*)createTLKRequestResponse:(NSError**) error {
    NSError* localError = NULL;
    NSData* initialSync = [self.circleDelegate circleGetInitialSyncViews:kSOSInitialSyncFlagTLKsRequestOnly error:&localError];
    if (!initialSync) {
        secnotice("joining", "Failed to get initial sync view: %@", localError);
        if ( error!=NULL && localError != NULL )
            *error = localError;
        return nil;
    }
    
    NSData* encryptedOutgoing = [self.session encrypt:initialSync error:&localError];
    if (!encryptedOutgoing) {
        secnotice("joining", "TLK request failed to encrypt: %@", localError);
        if ( error!=NULL && localError != NULL )
            *error = localError;
        return nil;
    }
    self->_state = kAcceptDone;

    secnotice("joining", "TLKRequest done.");

    return [[KCJoiningMessage messageWithType:kTLKRequest
                                         data:encryptedOutgoing
                                        error:error] der];
}


- (BOOL)shouldProcessSOSApplication:(KCJoiningMessage*)message pairingMessage:(OTPairingMessage*)pairingMessage
{
    BOOL shouldProcess = YES;

    if (SOSCCIsSOSTrustAndSyncingEnabled() == false) {
        secnotice("joining", "platform does not support SOS");
        shouldProcess = NO;
    } else if (message.secondData == nil) {
        secnotice("joining", "message does not contain SOS data");
        shouldProcess = NO;
    } else if (pairingMessage.hasSupportsSOS && pairingMessage.supportsSOS.supported == OTSupportType_not_supported) {
        secnotice("joining", "requester explicitly does not support SOS");
        shouldProcess = NO;
    }

    return shouldProcess;
}


- (NSData*)processApplication:(KCJoiningMessage*)message error:(NSError**)error {

    if ([message type] == kTLKRequest) {
        return [self createTLKRequestResponse: error];
    }
    
    if ([message type] != kPeerInfo) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected peerInfo!");
        return nil;
    }

    if (self.piggy_version == kPiggyV2) {
        __block NSData* next = nil;
        __block NSError* localError = nil;

        OTPairingMessage *pairingMessage = [self createPairingMessageFromJoiningMessage:message error:error];
        if (!pairingMessage) {
            secerror("octagon, failed to create pairing message from JoiningMessage");
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Failed to create pairing message from JoiningMessage");
            return nil;
        }

        if (!pairingMessage.hasPrepare) {
            secerror("octagon, message does not contain prepare message");
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected prepare message!");
            return nil;
        }
        OTApplicantToSponsorRound2M1 *prepareMessage = pairingMessage.prepare;

        // Max Capability - assume non-full and full-peers are allowed through piggybacking
        NSString* maxCap = KCPairingIntent_Capability_FullPeer;
        
        //handle identity, fetch voucher
        [self.otControl rpcVoucherWithArguments:self.controlArguments
                                  configuration:self.joiningConfiguration
                                         peerID:prepareMessage.peerID
                                  permanentInfo:prepareMessage.permanentInfo
                               permanentInfoSig:prepareMessage.permanentInfoSig
                                     stableInfo:prepareMessage.stableInfo
                                  stableInfoSig:prepareMessage.stableInfoSig 
                                  maxCapability:maxCap
                                          reply:^(NSData *voucher,
                                                                                     NSData *voucherSig,
                                                                                     NSError *err) {
            if (err) {
                secerror("error producing octagon voucher: %@", err);
                localError = err;
            } else {
                OTPairingMessage *pairingResponse = [[OTPairingMessage alloc] init];
                pairingResponse.supportsSOS = [[OTSupportSOSMessage alloc] init];
                pairingResponse.supportsOctagon = [[OTSupportOctagonMessage alloc] init];
                pairingResponse.voucher = [[OTSponsorToApplicantRound2M2 alloc] init];
                pairingResponse.voucher.voucher = voucher;
                pairingResponse.voucher.voucherSignature = voucherSig;

                pairingMessage.supportsSOS.supported = SOSCCIsSOSTrustAndSyncingEnabled() ? OTSupportType_supported : OTSupportType_not_supported;
                pairingMessage.supportsOctagon.supported = OTSupportType_supported;
                next = pairingResponse.data;
            }
        }];

        if (next == NULL) {
            if (error && localError) {
                *error = localError;
            }
            return nil;
        }

        NSData* encryptedOutgoing = nil;
        if ([self shouldProcessSOSApplication:message pairingMessage:pairingMessage]) {
            secnotice("joining", "doing SOS processSOSApplication");
            encryptedOutgoing = [self processSOSApplication: message.secondData error:error];
            if (encryptedOutgoing == nil) {
                secerror("joining: failed to process SOS application: %@", error && *error ? *error : nil);
                KCJoiningErrorCreate(kProcessApplicationFailure, error, @"message failed to process application");
                return nil;
            }
        }

        self->_state = kAcceptDone;

        //note we are stuffing SOS into the payload
        return [[KCJoiningMessage messageWithType:kCircleBlob
                                             data:next
                                          payload:encryptedOutgoing
                                            error:error] der];
    }

    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSString *description = [NSString stringWithFormat:@"cannot join piggyback version %d with SOS disabled", (int)self.piggy_version];
        secerror("joining: %s", [description UTF8String]);
        if (error != nil) {
            *error = [NSError errorWithJoiningError:kInternalError format:@"%@", description];
        }
        return nil;
    }

    NSData* encryptedOutgoing = [self processSOSApplication: message.firstData error:error];
    
    self->_state = kAcceptDone;

    secnotice("joining", "posting kSOSCCCircleOctagonKeysChangedNotification");
    notify_post(kSOSCCCircleOctagonKeysChangedNotification);
    
    return [[KCJoiningMessage messageWithType:kCircleBlob
                                         data:encryptedOutgoing
                                        error:error] der];
}


- (nullable NSData*)processMessage:(NSData*)incomingMessage error:(NSError**) error {
    NSData* result = nil;

    secnotice("acceptor", "processMessages: %@", [self description]);

    KCJoiningMessage *message = (self.state != kExpectingA) ? [KCJoiningMessage messageWithDER:incomingMessage error:error] : nil;

    switch(self.state) {
        case kExpectingA:
            return [self processInitialMessage:incomingMessage error: error];
        case kExpectingM:
            if (message == nil) return nil;
            return [self processResponse:message error: error];
            break;
        case kExpectingPeerInfo:
            if (message == nil) return nil;
            return [self processApplication:message error: error];
            break;
        case kAcceptDone:
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Unexpected message while done");
            break;
    }
    return result;
}

- (bool)isDone {
    return self.state == kAcceptDone;
}

/* for test*/
#if OCTAGON
- (void)setControlObject:(OTControl *)control
{
    self.otControl = control;
}


- (void)setSessionControlArguments:(OTControlArguments*)controlArguments
{
    self.controlArguments = controlArguments;
}

- (void)setConfiguration:(OTJoiningConfiguration *)config
{
    self.joiningConfiguration = config;
}

- (KCAESGCMDuplexSession*)accessSession
{
    return self.session;
}
#endif

@end
