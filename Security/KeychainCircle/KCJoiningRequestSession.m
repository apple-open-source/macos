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

#include <utilities/debugging.h>

#import <KeychainCircle/NSError+KCCreationHelpers.h>

typedef enum {
    kExpectingB,
    kExpectingHAMK,
    kRequestSecretDone
} KCJoiningRequestSecretSessionState;

typedef enum {
    kExpectingCircleBlob,
    kRequestCircleDone
} KCJoiningRequestCircleSessionState;

@interface KCJoiningRequestSecretSession ()
@property (readonly) NSObject<KCJoiningRequestSecretDelegate>* secretDelegate;
@property (readonly) KCSRPClientContext* context;
@property (readonly) uint64_t dsid;
@property (readonly) KCJoiningRequestSecretSessionState state;

@property (readwrite) NSData* challenge;
@property (readwrite) NSData* salt;
@end

@implementation KCJoiningRequestSecretSession : NSObject

- (nullable NSData*) initialMessage: (NSError**) error {
    NSData* start = [self->_context copyStart: error];
    if (start == nil) return nil;

    NSMutableData* initialMessage = [NSMutableData dataWithLength: sizeof_initialmessage(start)];

    if (NULL == encode_initialmessage(start, error, initialMessage.mutableBytes, initialMessage.mutableBytes + initialMessage.length))
        return nil;
    
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

    return self.session != nil;
}

- (nullable NSData*) copyResponseForChallenge:(NSData*) challenge
                                         salt:(NSData*) salt
                                       secret: (NSString*) password
                                        error: (NSError**) error {
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
    NSData* challenge = nil;
    NSData* salt = nil;

    if (![challengeData decodeSequenceData:&salt data:&challenge error:error]) return nil;

    return [self copyResponseForChallenge:challenge salt:salt secret:password error:error];

}

- (nullable NSData*) handleChallenge: (KCJoiningMessage*) message
                              secret: (NSString*) password
                               error: (NSError**)error {
    // Parse the challenge message
    // Salt and Challenge packet
    if ([message type] != kChallenge) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected challenge!");
        return nil;
    }

    return [self handleChallengeData:[message firstData] secret:password error:error];
}

- (NSData*) handleChallenge: (KCJoiningMessage*) message error: (NSError**)error {
    return [self handleChallenge:message
                          secret:[self.secretDelegate secret]
                           error:error];

}

- (NSData*) handleVerification: (KCJoiningMessage*) message error: (NSError**) error {
    if ([message type] == kError) {
        bool newCode = [[message firstData] length] == 0;
        NSString* nextSecret = [self.secretDelegate verificationFailed: newCode];

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

        if (![self.secretDelegate processAccountCode:accountCode error:error]) return nil;
    }

    self->_state = kRequestSecretDone;

    return [NSData data];
}




// [self.delegate processCircleJoinData:circleData error:error];

- (NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error {
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

    self = [super init];

    self->_secretDelegate = secretDelegate;
    self->_state = kExpectingB;
    self->_dsid = dsid;
    
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

@end

@interface KCJoiningRequestCircleSession ()
@property (readonly) NSObject<KCJoiningRequestCircleDelegate>* circleDelegate;
@property (readonly) KCAESGCMDuplexSession* session;
@property (readwrite) KCJoiningRequestCircleSessionState state;
@end

@implementation KCJoiningRequestCircleSession
- (nullable NSData*) encryptedPeerInfo: (NSError**) error {
    // Get our peer info and send it along:
    if (self->_session == nil) {
        KCJoiningErrorCreate(kInternalError, error, @"Attempt to encrypt with no session");
        return nil;
    }

    SOSPeerInfoRef us = [self.circleDelegate copyPeerInfoError:error];
    if (us == NULL) return nil;
    CFErrorRef cfError = NULL;
    NSData* piEncoded = (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(us, NULL, &cfError);

    if (piEncoded == nil) {
        if (error != nil) {
            *error = (__bridge_transfer NSError*) cfError;
        }
        return nil;
    }

    return [self->_session encrypt:piEncoded error:error];
}

- (nullable NSData*) initialMessage: (NSError**) error {
    NSData* encryptedPi = [self encryptedPeerInfo:error];
    if (encryptedPi == nil) return nil;

    self->_state = kExpectingCircleBlob;

    return [[KCJoiningMessage messageWithType:kPeerInfo
                                         data:encryptedPi
                                        error:error] der];

}

- (NSData*) handleCircleBlob: (KCJoiningMessage*) message error: (NSError**) error {
    if ([message type] != kCircleBlob) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected CircleBlob!");
        return nil;
    }

    NSData* circleBlob = [self.session decryptAndVerify:message.firstData error:error];
    if (circleBlob == nil) return nil;

    if (![self.circleDelegate processCircleJoinData: circleBlob error:error])
        return nil;

    self->_state = kRequestCircleDone;

    return [NSData data]; // Success, an empty message.
}

- (NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error {
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

- (bool) isDone {
    return self.state = kRequestCircleDone;
}

+ (instancetype) sessionWithCircleDelegate: (NSObject<KCJoiningRequestCircleDelegate>*) circleDelegate
                                   session: (KCAESGCMDuplexSession*) session
                                     error: (NSError**) error {
    return [[KCJoiningRequestCircleSession alloc] initWithCircleDelegate:circleDelegate
                                                                 session:session
                                                                   error:error];
}

- (instancetype) initWithCircleDelegate: (NSObject<KCJoiningRequestCircleDelegate>*) circleDelegate
                                session: (KCAESGCMDuplexSession*) session
                                  error: (NSError**) error {
    self = [super init];

    self->_circleDelegate = circleDelegate;
    self->_session = session;
    self.state = kExpectingCircleBlob;

    return self;
}

@end

