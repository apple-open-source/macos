//
//  KCJoiningRequestCircleSession.m
//  Security
//

#import <Foundation/Foundation.h>

#import <KeychainCircle/KCJoiningSession.h>

#import <KeychainCircle/KCError.h>
#import <KeychainCircle/KCDer.h>
#import <KeychainCircle/KCSRPContext.h>

#import <KeychainCircle/KCJoiningMessages.h>

#include <utilities/debugging.h>
#include "KCInitialMessageData.h"

#if OCTAGON
#import <Security/OTConstants.h>
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "KeychainCircle/KCJoiningRequestSession+Internal.h"
#import "keychain/ot/proto/generated_source/OTApplicantToSponsorRound2M1.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound2M2.h"
#import "keychain/ot/proto/generated_source/OTSponsorToApplicantRound1M2.h"
#import "keychain/ot/proto/generated_source/OTGlobalEnums.h"
#import "keychain/ot/proto/generated_source/OTSupportSOSMessage.h"
#import "keychain/ot/proto/generated_source/OTSupportOctagonMessage.h"
#import "keychain/ot/proto/generated_source/OTPairingMessage.h"
#endif
#import <KeychainCircle/NSError+KCCreationHelpers.h>
#import "keychain/categories/NSError+UsefulConstructors.h"

typedef enum {
    kExpectingCircleBlob,
    kRequestCircleDone
} KCJoiningRequestCircleSessionState;

@interface KCJoiningRequestCircleSession ()
@property (readonly) NSObject<KCJoiningRequestCircleDelegate>* circleDelegate;
@property (readonly) KCAESGCMDuplexSession* session;
@property (readwrite) KCJoiningRequestCircleSessionState state;
@property (nonatomic) uint64_t piggy_version;
#if OCTAGON
@property (nonatomic, strong) OTControl *otControl;

@property (nonatomic, strong) OTJoiningConfiguration* joiningConfiguration;
@property (nonatomic, strong) OTControlArguments* controlArguments;
#endif
@end

@implementation KCJoiningRequestCircleSession

#if OCTAGON
- (void)setControlObject:(OTControl *)control{
    self.otControl = control;
}
- (void)setContextIDForSession:(NSString*)contextID
{
    self.controlArguments = [[OTControlArguments alloc] initWithContainerName:self.controlArguments.containerName
                                                                    contextID:contextID
                                                                      altDSID:self.controlArguments.altDSID];
}
- (KCAESGCMDuplexSession*)accessSession
{
    return self.session;
}

#endif

- (nullable NSData*)encryptedPeerInfo:(NSError**)error {
    // Get our peer info and send it along:
    if (self->_session == nil) {
        KCJoiningErrorCreate(kInternalError, error, @"Attempt to encrypt with no session");
        return nil;
    }

    SOSPeerInfoRef application = [self.circleDelegate copyPeerInfoError:error];
    if (application == NULL) return nil;
    CFErrorRef cfError = NULL;
    NSData* piEncoded = (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(application, NULL, &cfError);
    if (application) {
        CFRelease(application);
        application = NULL;
    }

    if (piEncoded == nil) {
        if (error != nil) {
            *error = (__bridge_transfer NSError*) cfError;
        }
        return nil;
    }

    return [self->_session encrypt:piEncoded error:error];
}

- (nullable NSData*)encryptedInitialMessage:(NSData*)prepareMessage error:(NSError**)error {

    if (self->_session == nil) {
        KCJoiningErrorCreate(kInternalError, error, @"Attempt to encrypt with no session");
        return nil;
    }

    KCInitialMessageData *initialMessage = [[KCInitialMessageData alloc] init];
    [initialMessage setPrepareMessage:prepareMessage];

    return [self->_session encrypt:initialMessage.data error:error];
}

- (NSData*)encryptPeerInfo:(NSError**)error {
    NSData* encryptedPi = nil;
    secnotice("joining", "doing SOS encryptedPeerInfo");

    NSError* encryptError = nil;
    encryptedPi = [self encryptedPeerInfo:&encryptError];
    if (encryptedPi == nil || encryptError) {
        secerror("joining: failed to create encrypted peerInfo: %@", encryptError);
        if (encryptError) {
            if (error) {
                *error = encryptError;
            }
        } else {
            KCJoiningErrorCreate(kFailedToEncryptPeerInfo, error, @"failed to encrypt the SOS peer info");
        }
        return nil;
    }

    return encryptedPi;
}

- (nullable NSData*)initialMessage:(NSError**)error {
    secnotice("joining", "joining: KCJoiningRequestCircleSession initialMessage called");

    if (self.piggy_version == kPiggyV2) {
        __block NSData* next = nil;
        __block NSError* localError = nil;

        if (!self.joiningConfiguration.epoch) {
            secerror("joining: expected acceptor epoch! returning nil.");
            KCJoiningErrorCreate(kMissingAcceptorEpoch, error, @"expected acceptor epoch");
            return nil;
        }

        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        //giving securityd the epoch, expecting identity message
        [self.otControl rpcPrepareIdentityAsApplicantWithArguments:self.controlArguments
                                                     configuration:self.joiningConfiguration
                                                             reply:^(NSString *peerID,
                                                                     NSData *permanentInfo,
                                                                     NSData *permanentInfoSig,
                                                                     NSData *stableInfo,
                                                                     NSData *stableInfoSig,
                                                                     NSError *err) {
            if (err) {
                secerror("octagon: error preparing identity: %@", err);
                localError = err;
            } else{
                OTPairingMessage *pairingMessage = [[OTPairingMessage alloc]init];
                pairingMessage.supportsSOS = [[OTSupportSOSMessage alloc] init];
                pairingMessage.supportsOctagon = [[OTSupportOctagonMessage alloc] init];
                OTApplicantToSponsorRound2M1 *prepareMessage = [[OTApplicantToSponsorRound2M1 alloc]init];
                prepareMessage.peerID = peerID;
                prepareMessage.permanentInfo = permanentInfo;
                prepareMessage.permanentInfoSig = permanentInfoSig;
                prepareMessage.stableInfo = stableInfo;
                prepareMessage.stableInfoSig = stableInfoSig;

                pairingMessage.prepare = prepareMessage;

                pairingMessage.supportsSOS.supported = SOSCCIsSOSTrustAndSyncingEnabled() ? OTSupportType_supported : OTSupportType_not_supported;
                pairingMessage.supportsOctagon.supported = OTSupportType_supported;

                next = pairingMessage.data;
            }
            dispatch_semaphore_signal(sema);
        }];

        if (dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
            secerror("octagon: timed out preparing identity");
            KCJoiningErrorCreate(kTimedoutWaitingForPrepareRPC, error, @"timed out waiting for rpcPrepareIdentityAsApplicantWithArguments");
            return nil;
        }

        if (localError) {
            if (error) {
                *error = localError;
            }
            return nil;
        }

        NSData* encryptedPi = nil;
        if (SOSCCIsSOSTrustAndSyncingEnabled()) {
            encryptedPi = [self encryptPeerInfo:error];
            if (encryptedPi == nil) {
                return nil;
            }
        } else {
            secnotice("joining", "SOS not enabled, skipping peer info encryption");
        }

        self->_state = kExpectingCircleBlob;
        NSData *encryptedInitialMessage = [self encryptedInitialMessage:next error:error];

        return [[KCJoiningMessage messageWithType:kPeerInfo
                                             data:encryptedInitialMessage
                                          payload:encryptedPi
                                            error:error] der];
    }

    if (SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSData* encryptedPi = [self encryptPeerInfo:error];
        if (encryptedPi == nil) {
            return nil;
        }

        self->_state = kExpectingCircleBlob;

        return [[KCJoiningMessage messageWithType:kPeerInfo
                                             data:encryptedPi
                                            error:error] der];
    } 

    secerror("joining: device does not support SOS nor piggybacking version 2");
    KCJoiningErrorCreate(kSOSNotSupportedAndPiggyV2NotSupported, error, @"device does not support SOS nor piggybacking version 2");
    return nil;
}

- (void)waitForOctagonUpgrade
{
#if OCTAGON
    [self.otControl waitForOctagonUpgrade:self.controlArguments reply:^(NSError *error) {
        if (error) {
            secerror("pairing: failed to upgrade initiator into Octagon: %@", error);
        }
    }];
#endif
}

- (BOOL)shouldJoinSOS:(KCJoiningMessage*)message pairingMessage:(OTPairingMessage*)pairingMessage
{

    BOOL shouldJoin = YES;

    if (SOSCCIsSOSTrustAndSyncingEnabled() == false) {
        secnotice("joining", "platform does not support SOS");
        shouldJoin = NO;
    } else if (message.secondData == nil) {
        secnotice("joining", "message does not contain SOS data");
        shouldJoin = NO;
    } else if (pairingMessage.hasSupportsSOS && pairingMessage.supportsSOS.supported == OTSupportType_not_supported) {
        secnotice("joining", "acceptor explicitly does not support SOS");
        shouldJoin = NO;
    }

    return shouldJoin;
}

- (NSData*)handleCircleBlob:(KCJoiningMessage*) message error:(NSError**) error {
    secnotice("joining", "joining: KCJoiningRequestCircleSession handleCircleBlob called");
    if ([message type] != kCircleBlob) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected CircleBlob!");
        return nil;
    }
#if OCTAGON
    if (self.piggy_version == kPiggyV2 && message.firstData != nil) {
        __block NSData* nextMessage = nil;
        __block NSError* localError = nil;
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);

        OTPairingMessage* pairingMessage = [[OTPairingMessage alloc] initWithData:message.firstData];
        if (!pairingMessage.hasVoucher) {
            secerror("octagon: expected voucher! returning from piggybacking.");
            KCJoiningErrorCreate(kMissingVoucher, error, @"Missing voucher from acceptor");
            return nil;
        }

        OTSponsorToApplicantRound2M2 *voucher = pairingMessage.voucher;

        //handle voucher message then join octagon
        [self.otControl rpcJoinWithArguments:self.controlArguments
                               configuration:self.joiningConfiguration
                                   vouchData:voucher.voucher
                                    vouchSig:voucher.voucherSignature
                                       reply:^(NSError * _Nullable err) {
            if (err) {
                secerror("octagon: error joining octagon: %@", err);
                localError = err;
            }else{
                secnotice("octagon", "successfully joined octagon");
            }
            dispatch_semaphore_signal(sema);
        }];

        if (dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 30)) != 0) {
            secerror("octagon: timed out joining octagon");
            KCJoiningErrorCreate(kTimedoutWaitingForJoinRPC, error, @"Timed out waiting for join rpc");
            return nil;
        }

        if ([self shouldJoinSOS:message pairingMessage:pairingMessage]) {
            secnotice("joining", "doing SOS processCircleJoinData");
            //note we are stuffing SOS into the payload "secondData"
            NSData* circleBlob = [self.session decryptAndVerify:message.secondData error:error];
            if (circleBlob == nil) {
                secnotice("joining", "decryptAndVerify failed: %@", error && *error ? *error : nil);
                return nil;
            }
            if (![self.circleDelegate processCircleJoinData: circleBlob version:kPiggyV1 error:error]) {
                secerror("joining: processCircleJoinData failed %@", error && *error ? *error : nil);
                return nil;
            }
        }

        self->_state = kRequestCircleDone;

        NSData* final = nil;
        if (nextMessage == nil) {
            final = [NSData data];
        }
        self->_state = kRequestCircleDone;
        return final;
    }
#endif
    if (SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSData* circleBlob = [self.session decryptAndVerify:message.firstData error:error];
        if (circleBlob == nil) {
            secerror("joining: circleBlob is nil");
            KCJoiningErrorCreate(kFailureToDecryptCircleBlob, error, @"Failed to decrypt circleBlob");
            return nil;
        }

        if (![self.circleDelegate processCircleJoinData: circleBlob version:kPiggyV1 error:error]) {
            secerror("joining: failed to process SOS circle");
            KCJoiningErrorCreate(kFailureToProcessCircleBlob, error, @"Failed to process circleBlob");
            return nil;
        } else {
            secnotice("joining", "joined the SOS circle!");
#if OCTAGON
            secnotice("joining", "kicking off SOS Upgrade into Octagon!");
            [self waitForOctagonUpgrade];
#endif
        }
    } else {
        secnotice("joining", "SOS not enabled for this platform");
    }
    self->_state = kRequestCircleDone;

    return [NSData data]; // Success, an empty message.
}

- (NSData*)processMessage:(NSData*) incomingMessage error:(NSError**)error {
    secnotice("joining", "joining: KCJoiningRequestCircleSession processMessage called");
    NSData* result = nil;
    KCJoiningMessage* message = [KCJoiningMessage messageWithDER: incomingMessage error: error];
    if (message == nil) return nil;

    switch(self.state) {
        case kExpectingCircleBlob:
            return [self handleCircleBlob:message error:error];
        case kRequestCircleDone:
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Done, no messages expected.");
            break;
    }

    return result;
}

- (bool)isDone {
    return self.state = kRequestCircleDone;
}

+ (instancetype)sessionWithCircleDelegate:(NSObject<KCJoiningRequestCircleDelegate>*)circleDelegate
                                   session:(KCAESGCMDuplexSession*)session
                                     error:(NSError**)error {
    return [[KCJoiningRequestCircleSession alloc] initWithCircleDelegate:circleDelegate
                                                                 session:session
                                                                   error:error];
}

- (instancetype)initWithCircleDelegate:(NSObject<KCJoiningRequestCircleDelegate>*)circleDelegate
                                session:(KCAESGCMDuplexSession*)session
                                  error:(NSError**)error {
    return [self initWithCircleDelegate:circleDelegate
                                session:session
                              otcontrol:[OTControl controlObject:true error:error]
                                  error:error];
}

- (instancetype)initWithCircleDelegate:(NSObject<KCJoiningRequestCircleDelegate>*)circleDelegate
                               session:(KCAESGCMDuplexSession*)session
                             otcontrol:(OTControl*)otcontrol
                                 error:(NSError**)error
{
    secnotice("joining", "joining: KCJoiningRequestCircleSession initWithCircleDelegate called, uuid=%@", session.pairingUUID);
    if ((self = [super init])) {
        self->_circleDelegate = circleDelegate;
        self->_session = session;
        self.state = kExpectingCircleBlob;
#if OCTAGON
        self->_otControl = otcontrol;
        self->_joiningConfiguration = [[OTJoiningConfiguration alloc]initWithProtocolType:@"OctagonPiggybacking"
                                                                           uniqueDeviceID:@"requester-id"
                                                                           uniqueClientID:@"requester-id"
                                                                              pairingUUID:session.pairingUUID
                                                                                    epoch:session.epoch
                                                                              isInitiator:true];
        self->_controlArguments = [[OTControlArguments alloc] initWithAltDSID:session.altDSID];

        self->_piggy_version = session.piggybackingVersion;
#else
        self->_piggy_version = kPiggyV1;
#endif
    }
    return self;
}

@end

