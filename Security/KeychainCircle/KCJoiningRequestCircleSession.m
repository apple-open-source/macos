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
#import <KeychainCircle/NSError+KCCreationHelpers.h>
#import "keychain/categories/NSError+UsefulConstructors.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#import "MetricsOverrideForTests.h"

typedef enum {
    kExpectingCircleBlob,
    kRequestCircleDone
} KCJoiningRequestCircleSessionState;

@interface KCJoiningRequestCircleSession ()
@property (readonly) NSObject<KCJoiningRequestCircleDelegate>* circleDelegate;
@property (readonly) KCAESGCMDuplexSession* session;
@property (readwrite) KCJoiningRequestCircleSessionState state;
@property (nonatomic) uint64_t piggy_version;
@property (nonatomic, strong) NSString* altDSID;
@property (nonatomic, strong) NSString* flowID;
@property (nonatomic, strong) NSString* deviceSessionID;
#if OCTAGON
@property (nonatomic, strong) OTControl *otControl;

@property (nonatomic, strong) OTJoiningConfiguration* joiningConfiguration;
@property (nonatomic, strong) OTControlArguments* controlArguments;
#endif
//test only
@property (nonatomic) uint64_t piggybacking_version_for_tests;
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

- (void)setPiggybackingVersion:(uint64_t)version
{
    self.piggybacking_version_for_tests = version;
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
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.altDSID
                                                                                                  flowID:self.flowID
                                                                                         deviceSessionID:self.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNamePiggybackingCircleInitiatorInitialMessage
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    if (self.piggy_version == kPiggyV2) {
        __block NSData* next = nil;
        __block NSError* localError = nil;

        if (!self.joiningConfiguration.epoch) {
            localError = [NSError errorWithDomain:KCErrorDomain code: kMissingAcceptorEpoch description:@"expected acceptor epoch"];
            secerror("joining: expected acceptor epoch! returning nil. error: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

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
                secerror("joining: error preparing identity: %@", err);
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

                //secure piggybacking version
                pairingMessage.version = self.piggybacking_version_for_tests?: kPiggyV3;

                next = pairingMessage.data;
            }
        }];

        if (localError) {
            secerror("joining: failed to prepare identity: %@", localError);
            if (error) {
                *error = localError;
            }
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            return nil;
        }

        NSData* encryptedPi = nil;
        if (SOSCCIsSOSTrustAndSyncingEnabled()) {
            encryptedPi = [self encryptPeerInfo:&localError];
            if (encryptedPi == nil || localError) {
                if (localError == nil) {
                    localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToEncryptPeerInfo description:@"failed to encrypt peer info"];
                }
                secerror("joining: failed to create encrypted peer info: %@", localError);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
                if (error) {
                    *error = localError;
                }
                return nil;
            }
        } else {
            secnotice("joining", "SOS not enabled, skipping peer info encryption");
        }

        self->_state = kExpectingCircleBlob;
        NSData *encryptedInitialMessage = [self encryptedInitialMessage:next error:&localError];
        if (encryptedInitialMessage == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToEncryptInitialMessage description:@"failed to encrypt initial message"];
            }
            secerror("joining: failed to encrypt initial message: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

        NSData* messageOut = [[KCJoiningMessage messageWithType:kPeerInfo
                                                           data:encryptedInitialMessage
                                                        payload:encryptedPi
                                                          error:&localError] der];
        if (messageOut == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCreatePeerInfoResponse description:@"failed to create peerinfo response"];
            }
            secerror("joining: initial message creation failed: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
        } else {
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        }
        return messageOut;
    }

    NSError* localError = nil;
    if (SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSData* encryptedPi = [self encryptPeerInfo:&localError];
        if (encryptedPi == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCreatePeerInfoResponse description:@"failed to encrypt peer info"];
            }
            secerror("joining: failed to create encrypted peer info: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

        self->_state = kExpectingCircleBlob;

        NSData* messageOut = [[KCJoiningMessage messageWithType:kPeerInfo
                                                           data:encryptedPi
                                                          error:&localError] der];
        if (messageOut == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCreatePeerInfoResponse description:@"failed to initial peerinfo message"];
            }
            secerror("joining: initial message creation failed: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
        } else {
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        }
        return messageOut;
    }

    secerror("joining: device does not support SOS nor piggybacking version 2");
    localError = [NSError errorWithDomain:KCErrorDomain code:kSOSNotSupportedAndPiggyV2NotSupported description:@"device does not support SOS nor piggybacking version 2"];
    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
    if (error) {
        *error = localError;
    }
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

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.altDSID
                                                                                                  flowID:self.flowID
                                                                                         deviceSessionID:self.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNamePiggybackingCircleInitiatorHandleCircleBlobMessage
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];


    NSError* localError = nil;
    if ([message type] != kCircleBlob) {
        localError = [NSError errorWithDomain:KCErrorDomain code:kReceivedUnexpectedMessageTypeRequireCircleBlob description:@"Expected CircleBlob!"];
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        if (error) {
            *error = localError;
        }
        return nil;
    }
#if OCTAGON
    if (self.piggy_version == kPiggyV2 && message.firstData != nil) {
        __block NSData* nextMessage = nil;
        __block NSError* joinError = nil;

        OTPairingMessage* pairingMessage = nil;
        NSError* decryptError = nil;
        NSData* decryptedPayload = [self.session decryptAndVerify:message.firstData error:&decryptError];
        if (decryptedPayload == nil || decryptError) {
            secnotice("joining", "failed to decrypt voucher packet, fall back to legacy path, error: %@", decryptError);
            pairingMessage = [[OTPairingMessage alloc] initWithData:message.firstData];
            AAFAnalyticsEventSecurity *event = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                        altDSID:self.altDSID
                                                                                                         flowID:self.flowID
                                                                                                deviceSessionID:self.deviceSessionID
                                                                                                      eventName:kSecurityRTCEventNamePiggybackingAcceptorPreVersion3Change
                                                                                                testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                                 canSendMetrics:YES
                                                                                                       category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:event success:YES error:nil];
        } else {
            pairingMessage = [[OTPairingMessage alloc] initWithData:decryptedPayload];
            if (pairingMessage.hasVersion == NO || pairingMessage.version < kPiggyV3) {
                secerror("joining: unexpected piggybacking version, received: %llu", pairingMessage.version);
                if (error) {
                    *error = [NSError errorWithDomain:KCErrorDomain code:kUnexpectedVersion description:@"Unexpected piggybacking version"];
                }
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
                return nil;
            } else  {
                AAFAnalyticsEventSecurity *channelSecuredEvent = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                                          altDSID:self.altDSID
                                                                                                                           flowID:self.flowID
                                                                                                                  deviceSessionID:self.deviceSessionID
                                                                                                                        eventName:kSecurityRTCEventNamePiggybackingInitiatorChannelSecured
                                                                                                                  testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                                                   canSendMetrics:YES
                                                                                                                         category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:channelSecuredEvent success:YES error:nil];
            }
        }

        if (!pairingMessage.hasVoucher) {
            secerror("octagon: expected voucher! returning from piggybacking.");
            localError = [NSError errorWithDomain:KCErrorDomain code:kMissingVoucher description:@"Missing voucher from acceptor"];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
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
                joinError = err;
            }else{
                secnotice("octagon", "successfully joined octagon");
            }
        }];

        if (joinError) {
            secerror("joining: failed to join octagon: %@", joinError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:joinError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

        if ([self shouldJoinSOS:message pairingMessage:pairingMessage]) {
            secnotice("joining", "doing SOS processCircleJoinData");
            //note we are stuffing SOS into the payload "secondData"
            NSData* circleBlob = [self.session decryptAndVerify:message.secondData error:&localError];
            if (circleBlob == nil || localError) {
                if (localError == nil) {
                    localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToDecryptCircleBlob description:@"Failed to decrypt and verify message"];
                }
                secnotice("joining", "decryptAndVerify failed: %@", localError);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
                if (error) {
                    *error = localError;
                }
                return nil;
            }
            if (![self.circleDelegate processCircleJoinData: circleBlob version:kPiggyV1 error:&localError]) {
                if (localError == nil || localError) {
                    localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToProcessCircleJoinData description:@"Failed to process circle join data"];
                }
                secerror("joining: processCircleJoinData failed %@", localError);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
                if (error) {
                    *error = localError;
                }
                return nil;
            }
        }

        self->_state = kRequestCircleDone;

        NSData* final = nil;
        if (nextMessage == nil) {
            final = [NSData data];
        }
        self->_state = kRequestCircleDone;
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];

        return final;
    }
#endif
    if (SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSData* circleBlob = [self.session decryptAndVerify:message.firstData error:&localError];
        if (circleBlob == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailureToDecryptCircleBlob description:@"Failed to decrypt and verify circleBlob"];
            }
            secerror("joining: failed to decrypt and verify circle blob message failed %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

        if (![self.circleDelegate processCircleJoinData: circleBlob version:kPiggyV1 error:&localError]) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailureToProcessCircleBlob description:@"Failed to process circleBlob"];
            }
            secerror("joining: failed to process SOS circle: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
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
    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];

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
                                  altDSID:(NSString* _Nullable)altDSID
                                   flowID:(NSString* _Nullable)flowID
                          deviceSessionID:(NSString* _Nullable)deviceSessionID
                                    error:(NSError**)error {
    return [[KCJoiningRequestCircleSession alloc] initWithCircleDelegate:circleDelegate
                                                                 session:session
                                                                 altDSID:altDSID
                                                                  flowID:flowID
                                                         deviceSessionID:deviceSessionID
                                                                   error:error];
}

+ (instancetype)sessionWithCircleDelegate:(NSObject<KCJoiningRequestCircleDelegate>*)circleDelegate
                                   session:(KCAESGCMDuplexSession*)session
                                     error:(NSError**)error {
    return [[KCJoiningRequestCircleSession alloc] initWithCircleDelegate:circleDelegate
                                                                 session:session
                                                                 altDSID:nil
                                                                  flowID:nil
                                                         deviceSessionID:nil
                                                                   error:error];
}

- (instancetype)initWithCircleDelegate:(NSObject<KCJoiningRequestCircleDelegate>*)circleDelegate
                                session:(KCAESGCMDuplexSession*)session
                               altDSID:(NSString* _Nullable)altDSID
                                flowID:(NSString* _Nullable)flowID
                       deviceSessionID:(NSString* _Nullable)deviceSessionID
                                  error:(NSError**)error {
    return [self initWithCircleDelegate:circleDelegate
                                session:session
                              otcontrol:[OTControl controlObject:true error:error]
                                altDSID:nil
                                 flowID:nil
                        deviceSessionID:nil
                                  error:error];
}

- (instancetype)initWithCircleDelegate:(NSObject<KCJoiningRequestCircleDelegate>*)circleDelegate
                               session:(KCAESGCMDuplexSession*)session
                             otcontrol:(OTControl*)otcontrol
                               altDSID:(NSString* _Nullable)altDSID
                                flowID:(NSString* _Nullable)flowID
                       deviceSessionID:(NSString* _Nullable)deviceSessionID
                                 error:(NSError**)error
{
    secnotice("joining", "joining: KCJoiningRequestCircleSession initWithCircleDelegate called, uuid=%@", session.pairingUUID);
    if ((self = [super init])) {
        _circleDelegate = circleDelegate;
        _session = session;
        _state = kExpectingCircleBlob;
        _altDSID = altDSID;
        _flowID = flowID;
        _deviceSessionID = deviceSessionID;
#if OCTAGON
        _otControl = otcontrol;
        _joiningConfiguration = [[OTJoiningConfiguration alloc]initWithProtocolType:@"OctagonPiggybacking"
                                                                           uniqueDeviceID:@"requester-id"
                                                                           uniqueClientID:@"requester-id"
                                                                              pairingUUID:session.pairingUUID
                                                                                    epoch:session.epoch
                                                                              isInitiator:true];
        _controlArguments = [[OTControlArguments alloc] initWithAltDSID:session.altDSID];

        _piggy_version = session.piggybackingVersion;
#else
        _piggy_version = kPiggyV1;
#endif
    }
    return self;
}

@end

