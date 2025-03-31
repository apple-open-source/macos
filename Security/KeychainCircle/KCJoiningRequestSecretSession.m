//
//  KCJoiningSession.m
//  Security
//
//

#import <Foundation/Foundation.h>

#import <KeychainCircle/KCJoiningSession.h>

#import <KeychainCircle/KCError.h>
#import <KeychainCircle/KCDer.h>
#import <KeychainCircle/KCSRPContext.h>

#import <KeychainCircle/KCJoiningMessages.h>

#include <corecrypto/ccrng.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>
#include <corecrypto/ccder.h>
#import <Security/SecureObjectSync/SOSTypes.h>
#include <utilities/debugging.h>

#import <Security/OTConstants.h>
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
    kExpectingB,
    kExpectingHAMK,
    kRequestSecretDone
} KCJoiningRequestSecretSessionState;


@interface KCJoiningRequestSecretSession ()
@property (weak) id<KCJoiningRequestSecretDelegate> secretDelegate;
@property (readonly) KCSRPClientContext* context;
@property (readonly) uint64_t dsid;
@property (readonly) KCJoiningRequestSecretSessionState state;
@property (readwrite) NSString* piggy_uuid;
@property (readwrite) uint64_t piggy_version;
@property (readwrite) uint64_t epoch;
@property (readwrite) NSData* challenge;
@property (readwrite) NSData* salt;
@property (readwrite) NSString* sessionUUID;
// Used for metrics collection
@property (nullable, strong) NSString* flowID;
@property (nullable, strong) NSString* deviceSessionID;

@property (nonatomic, strong) NSMutableDictionary *defaults;
@end

@implementation KCJoiningRequestSecretSession : NSObject
@synthesize altDSID = _altDSID;


- (nullable NSData*) createUUID
{
    NSUUID *uuid = [NSUUID UUID];
    uuid_t uuidBytes;

    self.piggy_uuid = [uuid UUIDString];
    [uuid getUUIDBytes:uuidBytes];
    NSData *uuidData = [NSData dataWithBytes:uuidBytes length:sizeof(uuid_t)];
    return uuidData;
}

- (nullable NSData*) initialMessage: (NSError**) error {
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.altDSID
                                                                                                  flowID:self.flowID
                                                                                         deviceSessionID:self.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNamePiggybackingSessionInitiatorInitialMessage
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    NSError* localError = nil;
    NSData* start = [self->_context copyStart:&localError];
    if (start == nil || localError) {
        if (localError == nil) {
            localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCopyStart description:@"Failed to copy start message"];
        }
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        if (error) {
            *error = localError;
        }
        return nil;
    }
    NSMutableData* initialMessage = NULL;
    secnotice("joining", "joining: KCJoiningRequestSecretSession initialMessage called");

    if(self.piggy_version == kPiggyV2){
#if OCTAGON
        NSData* uuidData = [self createUUID];

        NSString* version = @"o";
        NSData* octagonVersion = [version dataUsingEncoding:kCFStringEncodingUTF8];

        initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage_version2(start, kPiggyV1, uuidData, octagonVersion)];

        if (NULL == encode_initialmessage_version2(start, uuidData, octagonVersion, &localError, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length)){
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCreateVersion2Message description:@"failed to create version 2 message"];
            }
            secerror("failed to create version 2 message: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }
#endif
    }
    else if(self.piggy_version == kPiggyV1){
        NSData* uuidData = [self createUUID];
        initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage_version1(start, kPiggyV1, uuidData)];

        if (NULL == encode_initialmessage_version1(start, uuidData, kPiggyV1, &localError, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length)){
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCreateVersion1Message description:@"failed to create version 1 message"];
            }
            secerror("failed to create version 1 message: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }
    }
    else {
        initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage(start)];
        if (NULL == encode_initialmessage(start, &localError, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length)){
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToEncodeInitialMessage description:@"failed to create initial message"];
            }
            secerror("failed to create version initial message: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }
    }

    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
    return initialMessage;
}

- (bool) isDone {
    return self->_state == kRequestSecretDone;
}

- (bool) setupSession: (NSError**) error {
    NSData* key = [self->_context getKey];

    if (key == nil) {
        KCJoiningErrorCreate(kInternalError, error, @"No session key available");
        return nil;
    }

    self->_session = [KCAESGCMDuplexSession sessionAsSender:key context:self.dsid];
    self.session.pairingUUID = self.sessionUUID;
    self.session.altDSID = self.altDSID;
    self.session.piggybackingVersion = self.piggy_version;

    return self.session != nil;
}

- (NSString*)altDSID
{
    return _altDSID;
}

- (void)setAltDSID:(NSString*)altDSID
{
    _altDSID = altDSID;

    // The session may or may not exist at this point. If it doesn't, we'll inject the altDSID at creation time.
    self.session.altDSID = altDSID;
}

- (nullable NSData*) copyResponseForChallenge:(NSData*) challenge
                                         salt:(NSData*) salt
                                       secret: (NSString*) password
                                        error: (NSError**) error {

    secnotice("joining", "joining: KCJoiningRequestSecretSession copyResponseForChallenge called");
    NSData* response = [self->_context copyResposeToChallenge:challenge
                                                     password:password
                                                         salt:salt
                                                        error:error];

    if (!response) {
        // @@@ return error to other side???
        return nil;
    } else {
        if (![self setupSession: error]) return nil;

        self.challenge = challenge;
        self.salt = salt;

        self->_state = kExpectingHAMK;
        return [[KCJoiningMessage messageWithType:kResponse
                                             data:response
                                            error:error] der];
    }
}


- (nullable NSData*) copyResponseForSecret: (NSString*) password
                                     error: (NSError**) error {
    return [self copyResponseForChallenge:self.challenge salt:self.salt secret:password error:error];
}

- (nullable NSData*) handleChallengeData: (NSData*) challengeData
                                  secret: (NSString*) password
                                   error: (NSError**) error {
    secnotice("joining", "joining: KCJoiningRequestSecretSession handleChallengeData called");
    NSData* challenge = nil;
    NSData* salt = nil;

    if (![challengeData decodeSequenceData:&salt data:&challenge error:error]) return nil;

    return [self copyResponseForChallenge:challenge salt:salt secret:password error:error];

}

- (nullable NSData*) handleChallenge: (KCJoiningMessage*) message
                              secret: (NSString*) password
                               error: (NSError**)error {
    secnotice("joining", "joining: KCJoiningRequestSecretSession handleChallenge called");
    // Parse the challenge message
    // Salt and Challenge packet
    if ([message type] != kChallenge) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected challenge!");
        return nil;
    }
#if OCTAGON
    //handle octagon data if it exists
    self.piggy_version = [message secondData] ? kPiggyV2 : kPiggyV1;

    // The session may or may not exist at this point. If it doesn't, the version will be set at object creation time.
    self.session.piggybackingVersion = self.piggy_version;
    self.session.altDSID = self.altDSID;

    if (self.piggy_version == kPiggyV2){
        OTPairingMessage* pairingMessage = [[OTPairingMessage alloc]initWithData: [message secondData]];
        if (pairingMessage.hasEpoch) {
            secnotice("octagon", "received epoch message: %@", [pairingMessage.epoch dictionaryRepresentation]);
            self.epoch = pairingMessage.epoch.epoch;
        }
        else{
            secerror("octagon: acceptor did not send its epoch. discontinuing octagon protocol. downgrading to verison 1");
            self.piggy_version = kPiggyV1;
        }
    }
#endif
    return [self handleChallengeData:[message firstData] secret:password error:error];
}

- (NSData*) handleChallenge: (KCJoiningMessage*) message error: (NSError**)error {

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.altDSID
                                                                                                  flowID:self.flowID
                                                                                         deviceSessionID:self.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNamePiggybackingSessionInitiatorHandleChallenge
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    NSError* localError = nil;
    NSData* messageOut = [self handleChallenge:message
                                        secret:[self.secretDelegate secret]
                                         error:&localError];

    if (messageOut == nil || localError) {
        if (localError == nil) {
            localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCreateHandleChallengeMessage description:@"failed to create response message"];
        }
        secerror("Failed to create response message: %@", localError);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        if (error) {
            *error = localError;
        }
    } else {
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
    }
    return messageOut;
}

- (NSData*) handleVerification: (KCJoiningMessage*) message error: (NSError**) error {
    secnotice("joining", "joining: KCJoiningRequestSecretSession handleVerification called");

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.altDSID
                                                                                                  flowID:self.flowID
                                                                                         deviceSessionID:self.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNamePiggybackingSessionInitiatorHandleVerification
                                                                                         testsAreEnabled:MetricsOverrideTestsAreEnabled()
                                                                                          canSendMetrics:YES
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    id<KCJoiningRequestSecretDelegate> secretDelegate = self.secretDelegate;

    NSError* localError = nil;
    if ([message type] == kError) {
        bool newCode = [[message firstData] length] == 0;
        NSString* nextSecret = [secretDelegate verificationFailed: newCode];

        if (nextSecret) {
            if (newCode) {
                NSData* messageOut = [self copyResponseForSecret:nextSecret error:&localError];
                if (messageOut == nil || localError) {
                    if (localError == nil) {
                        localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToCopyResponseForSecret description:@"failed to copy response"];
                    }
                    secerror("joining: Failed to copy response message: %@", localError);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
                    if (error) {
                        *error = localError;
                    }
                } else {
                    secnotice("joining", "successfully copied response message");
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                }
                return messageOut;
            } else {
                NSData* messageOut = [self handleChallengeData:[message firstData] secret:nextSecret error:&localError];
                if (messageOut == nil || localError) {
                    if (localError == nil) {
                        localError = [NSError errorWithDomain:KCErrorDomain code:kFailedToHandleChallengeData description:@"failed to handle challenge data"];
                    }
                    secerror("joining: failed to handle challenge data: %@", localError);
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
                    if (error) {
                        *error = localError;
                    }
                } else {
                    secnotice("joining", "successfully handled challenge data");
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                }
                return messageOut;
            }
        } else {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kNilNextSecret description:@"next secret is nil"];
            }
            secerror("joining: next secret is nil: %@", localError);

            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }
    }

    if ([message type] != kVerification) {
        localError = [NSError errorWithDomain:KCErrorDomain code:kExpectedVerificationMessageType description:@"Expected verification!"];
        secerror("joining: expected vertification message type: %@", localError);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        if (error) {
            *error = localError;
        }
        return nil;
    }

    if (![self.context verifyConfirmation:[message firstData] error:&localError]) {
        // Sender thought we had it right, but he can't prove he has it right!

        if (localError == nil) {
            localError = [NSError errorWithDomain:KCErrorDomain code:kVerifyConfirmationFailed description:[NSString stringWithFormat:@"Got verification but  acceptor doesn't have matching secret: %@", self]];
        }
        secerror("joining: Verification failed: %@, error: %@", self, localError);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        if (error) {
            *error = localError;
        }
        return nil;
    }

    {
        NSData* payload = [self.session decryptAndVerify:[message secondData] error:&localError];
        if (payload == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kDecryptAndVerifyFailed description:@"decrypt and verify failed"];
            }
            secerror("joining: decrypt and verify failed: %@", localError);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

        NSString* accountCode = [NSString decodeFromDER:payload error:&localError];
        if (accountCode == nil || localError) {
            if (localError == nil) {
                localError = [NSError errorWithDomain:KCErrorDomain code:kDecodeFromDERFailed description:@"decode from der failed"];
            }
            secerror("joining: decode from der failed: %@", localError);

            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
            if (error) {
                *error = localError;
            }
            return nil;
        }

        if (![secretDelegate processAccountCode:accountCode error:error]) return nil;
    }

    self->_state = kRequestSecretDone;

    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];

    return [NSData data];
}

- (NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error {
    secnotice("joining", "joining: KCJoiningRequestSecretSession processMessage called");
    NSData* result = nil;
    KCJoiningMessage* message = [KCJoiningMessage messageWithDER: incomingMessage error: error];
    if (message == nil) return nil;

    switch(self->_state) {
        case kExpectingB:
            return [self handleChallenge:message error: error];
            break;
        case kExpectingHAMK:
            return [self handleVerification:message error:error];
            break;
        case kRequestSecretDone:
            KCJoiningErrorCreate(kUnexpectedMessage, error, @"Done, no messages expected.");
            break;
    }

    return result;
}

+ (nullable instancetype)sessionWithSecretDelegate:(NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                              dsid:(uint64_t)dsid
                                           altDSID:(NSString*)altDSID
                                            flowID:(NSString*)flowID
                                   deviceSessionID:(NSString*)deviceSessionID
                                             error:(NSError**)error
{
    return [[KCJoiningRequestSecretSession alloc] initWithSecretDelegate:secretDelegate
                                                                    dsid:dsid
                                                                 altDSID:altDSID
                                                                  flowID:flowID
                                                         deviceSessionID:deviceSessionID
                                                                   error:error];
}

+ (nullable instancetype)sessionWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                              dsid: (uint64_t)dsid

                                             error: (NSError**) error {
    return [[KCJoiningRequestSecretSession alloc] initWithSecretDelegate:secretDelegate
                                                                    dsid:dsid
                                                                 altDSID:nil
                                                                  flowID:nil
                                                         deviceSessionID:nil
                                                                   error:error];
}

- (nullable instancetype)initWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                           dsid: (uint64_t)dsid
                                        altDSID:(NSString*)altDSID
                                         flowID:(NSString*)flowID
                                deviceSessionID:(NSString*)deviceSessionID
                                          error: (NSError**)error {
    int cc_error = 0;
    struct ccrng_state * rng = ccrng(&cc_error);

    if (rng == nil) {
        CoreCryptoError(cc_error, error, @"RNG fetch failed");
        return nil;
    }

    return [self initWithSecretDelegate: secretDelegate
                                   dsid: dsid
                                altDSID:altDSID
                                 flowID:flowID
                        deviceSessionID:deviceSessionID
                                    rng: rng
                                  error: error];
}

- (nullable instancetype)initWithSecretDelegate:(NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                           dsid:(uint64_t)dsid
                                        altDSID:(NSString*)altDSID
                                         flowID:(NSString*)flowID
                                deviceSessionID:(NSString*)deviceSessionID
                                            rng:(struct ccrng_state *)rng
                                          error:(NSError**)error {
    secnotice("joining", "joining: initWithSecretDelegate called");
    if ((self = [super init])) {
        _secretDelegate = secretDelegate;
        _state = kExpectingB;
        _dsid = dsid;
        _altDSID = altDSID;
        _flowID = flowID;
        _deviceSessionID = deviceSessionID;
        _defaults = [NSMutableDictionary dictionary];

#if OCTAGON
        _piggy_version = kPiggyV2;

        _sessionUUID = [[NSUUID UUID] UUIDString];
#else
        _piggy_version = kPiggyV1;
#endif

        secnotice("joining", "joining: initWithSecretDelegate called, uuid=%@", self.sessionUUID);

        NSString* name = [NSString stringWithFormat: @"%llu", dsid];

        _context = [[KCSRPClientContext alloc] initWithUser: name
                                                 digestInfo: ccsha256_di()
                                                      group: ccsrp_gp_rfc5054_3072()
                                               randomSource: rng];
    }
    return self;
}

- (NSString*) stateString {
    switch (self.state) {
        case kExpectingB: return @"→B";
        case kExpectingHAMK: return @"→HAMK";
        case kRequestSecretDone: return @"SecretDone";
        default: return [NSString stringWithFormat:@"%d", self.state];
    }
}

- (NSString *)description {
    return [NSString stringWithFormat: @"<KCJoiningAcceptSession@%p %lld %@ %@>", self, self.dsid, [self stateString], self.context];
}

@end
