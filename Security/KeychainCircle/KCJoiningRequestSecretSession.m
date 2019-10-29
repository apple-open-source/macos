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
#include <CommonCrypto/CommonRandomSPI.h>
#import <Security/SecureObjectSync/SOSTypes.h>
#include <utilities/debugging.h>

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
#import "keychain/ot/proto/generated_source/OTPairingMessage.h"
#endif
#import <KeychainCircle/NSError+KCCreationHelpers.h>

typedef enum {
    kExpectingB,
    kExpectingHAMK,
    kRequestSecretDone
} KCJoiningRequestSecretSessionState;

#if OCTAGON
static bool KCJoiningOctagonPiggybackingDefault = false;
bool KCSetJoiningOctagonPiggybackingEnabled(bool value)
{
    KCJoiningOctagonPiggybackingDefault = value;
    return value;
}

// defaults write com.apple.security.octagon enable -bool YES
bool KCJoiningOctagonPiggybackingEnabled() {
    bool result = KCJoiningOctagonPiggybackingDefault ? KCJoiningOctagonPiggybackingDefault : OctagonIsEnabled();
    secnotice("octagon", "Octagon Piggybacking is %@ ", result ? @"on" : @"off");
    return result;
}
#endif


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
#if OCTAGON
@property (nonatomic, strong) OTJoiningConfiguration* joiningConfiguration;
@property (nonatomic, strong) OTControl *otControl;
#endif
@property (nonatomic, strong) NSMutableDictionary *defaults;
@end

@implementation KCJoiningRequestSecretSession : NSObject


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
    NSData* start = [self->_context copyStart: error];
    if (start == nil) return nil;
    
    NSMutableData* initialMessage = NULL;
    secnotice("joining", "joining: KCJoiningRequestSecretSession initialMessage called");

    if(self.piggy_version == kPiggyV2){
#if OCTAGON
        if(KCJoiningOctagonPiggybackingEnabled()){
            NSData* uuidData = [self createUUID];

            NSString* version = @"o";
            NSData* octagonVersion = [version dataUsingEncoding:kCFStringEncodingUTF8];

            initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage_version2(start, kPiggyV1, uuidData, octagonVersion)];

            if (NULL == encode_initialmessage_version2(start, uuidData, octagonVersion, error, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length)){
                secerror("failed to create version 2 message");
                return nil;
            }
        }
#endif
    }
    else if(self.piggy_version == kPiggyV1){
        NSData* uuidData = [self createUUID];
        initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage_version1(start, kPiggyV1, uuidData)];

        if (NULL == encode_initialmessage_version1(start, uuidData, kPiggyV1, error, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length)){
            secerror("failed to create version 1 message: %@", *error);
            return nil;
        }
    }
    else{
        initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage(start)];
        if (NULL == encode_initialmessage(start, error, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length)){
            return nil;
        }
    }
    
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
    self.session.pairingUUID = self.joiningConfiguration.pairingUUID;
    self.session.piggybackingVersion = self.piggy_version;

    return self.session != nil;
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
    if(KCJoiningOctagonPiggybackingEnabled()){
        self.piggy_version = [message secondData] ? kPiggyV2 : kPiggyV1;

        // The session may or may not exist at this point. If it doesn't, the version will be set at object creation time.
        self.session.piggybackingVersion = self.piggy_version;

        if(self.piggy_version == kPiggyV2){
            OTPairingMessage* pairingMessage = [[OTPairingMessage alloc]initWithData: [message secondData]];

            if(pairingMessage.epoch.epoch){
                secnotice("octagon", "received epoch");
                self.epoch = pairingMessage.epoch.epoch;
            }
            else{
                secerror("octagon: acceptor did not send its epoch. discontinuing octagon protocol. downgrading to verison 1");
                self.piggy_version = kPiggyV1;
            }
        }
    }else{
        self.piggy_version = kPiggyV1;
    }
#endif
    return [self handleChallengeData:[message firstData] secret:password error:error];
}

- (NSData*) handleChallenge: (KCJoiningMessage*) message error: (NSError**)error {
    return [self handleChallenge:message
                          secret:[self.secretDelegate secret]
                           error:error];

}

- (NSData*) handleVerification: (KCJoiningMessage*) message error: (NSError**) error {
    secnotice("joining", "joining: KCJoiningRequestSecretSession handleVerification called");
    id<KCJoiningRequestSecretDelegate> secretDelegate = self.secretDelegate;

    if ([message type] == kError) {
        bool newCode = [[message firstData] length] == 0;
        NSString* nextSecret = [secretDelegate verificationFailed: newCode];

        if (nextSecret) {
            if (newCode) {
                return [self copyResponseForSecret:nextSecret error:error];
            } else {
                return [self handleChallengeData:[message firstData] secret:nextSecret error:error];
            }
        } else {
            return nil;
        }
    }

    if ([message type] != kVerification) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected verification!");
        return nil;
    }

    if (![self.context verifyConfirmation:[message firstData] error:error]) {
        // Sender thought we had it right, but he can't prove he has it right!
        KCJoiningErrorCreate(kInternalError, error, @"Got verification but  acceptor doesn't have matching secret: %@", self);
        secnotice("request-session", "Verification failed: %@", self);
        return nil;
    }

    {
        NSData* payload = [self.session decryptAndVerify:[message secondData] error:error];
        if (payload == nil) return nil;

        NSString* accountCode = [NSString decodeFromDER:payload error:error];
        if (accountCode == nil) return nil;

        if (![secretDelegate processAccountCode:accountCode error:error]) return nil;
    }

    self->_state = kRequestSecretDone;

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

+ (nullable instancetype)sessionWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                              dsid: (uint64_t)dsid
                                             error: (NSError**) error {
    return [[KCJoiningRequestSecretSession alloc] initWithSecretDelegate:secretDelegate
                                                                    dsid:dsid
                                                                   error:error];
}

- (nullable instancetype)initWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                           dsid: (uint64_t)dsid
                                          error: (NSError**)error {
    int cc_error = 0;
    struct ccrng_state * rng = ccrng(&cc_error);

    if (rng == nil) {
        CoreCryptoError(cc_error, error, @"RNG fetch failed");
        return nil;
    }

    return [self initWithSecretDelegate: secretDelegate
                                   dsid: dsid
                                    rng: rng
                                  error: error];
}

- (nullable instancetype)initWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                           dsid: (uint64_t)dsid
                                            rng: (struct ccrng_state *)rng
                                          error: (NSError**)error {
    secnotice("joining", "joining: initWithSecretDelegate called");
    self = [super init];

    self->_secretDelegate = secretDelegate;
    self->_state = kExpectingB;
    self->_dsid = dsid;
    self->_defaults = [NSMutableDictionary dictionary];

#if OCTAGON
    self->_piggy_version = KCJoiningOctagonPiggybackingEnabled() ? kPiggyV2 : kPiggyV1;
    self->_otControl = [OTControl controlObject:true error:error];
    self->_joiningConfiguration = [[OTJoiningConfiguration alloc]initWithProtocolType:OTProtocolPiggybacking
                                                                       uniqueDeviceID:@"requester-id"
                                                                       uniqueClientID:@"requester-id"
                                                                        containerName:nil
                                                                            contextID:OTDefaultContext
                                                                                epoch:0
                                                                          isInitiator:true];
#else
    self->_piggy_version = kPiggyV1;
#endif

    secnotice("joining", "joining: initWithSecretDelegate called, uuid=%@", self.joiningConfiguration.pairingUUID);

    NSString* name = [NSString stringWithFormat: @"%llu", dsid];
    
    self->_context = [[KCSRPClientContext alloc] initWithUser: name
                                                   digestInfo: ccsha256_di()
                                                        group: ccsrp_gp_rfc5054_3072()
                                                 randomSource: rng];

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
#if OCTAGON
/* for test */
-(void)setControlObject:(OTControl*)control
{
    self.otControl = control;
}

- (void)setConfiguration:(OTJoiningConfiguration *)config
{
    self.joiningConfiguration = config;
}
#endif

@end
