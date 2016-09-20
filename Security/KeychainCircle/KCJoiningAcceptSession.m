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

#include <corecrypto/ccder.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>

#include <CommonCrypto/CommonRandomSPI.h>

typedef enum {
    kExpectingA,
    kExpectingM,
    kExpectingPeerInfo,
    kAcceptDone
} KCJoiningAcceptSessionState;

@interface KCJoiningAcceptSession ()
@property (readonly) uint64_t dsid;
@property (readonly) NSObject<KCJoiningAcceptSecretDelegate>* secretDelegate;
@property (readonly) NSObject<KCJoiningAcceptCircleDelegate>* circleDelegate;
@property (readonly) KCSRPServerContext* context;
@property (readonly) KCAESGCMDuplexSession* session;
@property (readonly) KCJoiningAcceptSessionState state;
@property (readwrite) NSData* startMessage;
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

- (bool) setupSession: (NSError**) error {
    NSData* key = [self->_context getKey];

    if (key == nil) {
        KCJoiningErrorCreate(kInternalError, error, @"No session key available");
        return nil;
    }

    self->_session = [KCAESGCMDuplexSession sessionAsReceiver:key context:self.dsid];

    return self.session != nil;
}

- (nullable instancetype) initWithSecretDelegate: (NSObject<KCJoiningAcceptSecretDelegate>*) secretDelegate
                                  circleDelegate: (NSObject<KCJoiningAcceptCircleDelegate>*) circleDelegate
                                            dsid: (uint64_t) dsid
                                             rng: (struct ccrng_state *)rng
                                           error: (NSError**) error {
    self = [super init];

    NSString* name = [NSString stringWithFormat: @"%llu", dsid];

    self->_context = [[KCSRPServerContext alloc] initWithUser: name
                                                     password: [secretDelegate secret]
                                                   digestInfo: ccsha256_di()
                                                        group: ccsrp_gp_rfc5054_3072()
                                                 randomSource: rng];
    self->_secretDelegate = secretDelegate;
    self->_circleDelegate = circleDelegate;
    self->_state = kExpectingA;
    self->_dsid = dsid;
    
    return self;
}

- (NSString*) stateString {
    switch (self.state) {
        case kExpectingA: return @"→A";
        case kExpectingM: return @"→M";
        case kExpectingPeerInfo: return @"→PeerInfo";
        case kAcceptDone: return @"done";
        default: return [NSString stringWithFormat:@"%d", self.state];
    }
}

- (NSString *)description {
    return [NSString stringWithFormat: @"<KCJoiningAcceptSession@%p %lld %@ %@>", self, self.dsid, [self stateString], self.context];
}

- (NSData*) copyChallengeMessage: (NSError**) error {
    NSData* challenge = [self.context copyChallengeFor: self.startMessage error: error];
    if (challenge == nil) return nil;

    NSData* srpMessage = [NSData dataWithEncodedSequenceData:self.context.salt data:challenge error:error];

    if (![self setupSession:error]) return nil;

    return srpMessage;
}

- (NSData*) processInitialMessage: (NSData*) initialMessage error: (NSError**) error {
    self.startMessage = extractStartFromInitialMessage(initialMessage, error);
    if (self.startMessage == nil) return nil;

    NSData* srpMessage = [self copyChallengeMessage: error];
    if (srpMessage == nil) return nil;

    self->_state = kExpectingM;
    return [[KCJoiningMessage messageWithType:kChallenge
                                         data:srpMessage
                                        error:error] der];
}

- (NSData*) processResponse: (KCJoiningMessage*) message error:(NSError**) error {
    if ([message type] != kResponse) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected response!");
        return nil;
    }

    // We handle failure, don't capture the error.
    NSData* confirmation = [self.context copyConfirmationFor:message.firstData error:NULL];
    if (!confirmation) {
        // Find out what kind of error we should send.
        NSData* errorData = nil;

        switch ([self.secretDelegate verificationFailed: error]) {
            case kKCRetryError:
                // We fill in an error if they didn't, but if they did this wont bother.
                KCJoiningErrorCreate(kInternalError, error, @"Delegate returned error without filling in error: %@", self.secretDelegate);
                return nil;
            case kKCRetryWithSameChallenge:
                errorData = [NSData data];
                break;
            case kKCRetryWithNewChallenge:
                if ([self.context resetWithPassword:[self.secretDelegate secret] error:error]) {
                    errorData = [self copyChallengeMessage: error];
                }
                break;
        }
        if (errorData == nil) return nil;

        return [[KCJoiningMessage messageWithType:kError
                                             data:errorData
                                            error:error] der];
    }

    NSData* encoded = [NSData dataWithEncodedString:[self.secretDelegate accountCode] error:error];
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


- (NSData*) processApplication: (KCJoiningMessage*) message error:(NSError**) error {
    if ([message type] != kPeerInfo) {
        KCJoiningErrorCreate(kUnexpectedMessage, error, @"Expected peerInfo!");
        return nil;
    }

    NSData* decryptedPayload = [self.session decryptAndVerify:message.firstData error:error];
    if (decryptedPayload == nil) return nil;

    CFErrorRef cfError = NULL;
    SOSPeerInfoRef ref = SOSPeerInfoCreateFromData(NULL, &cfError, (__bridge CFDataRef) decryptedPayload);
    if (ref == NULL) {
        if (error) *error = (__bridge_transfer NSError*) cfError;
        cfError = NULL;
        return nil;
    }

    NSData* joinData = [self.circleDelegate circleJoinDataFor:ref error:error];
    if (joinData == nil) return nil;

    NSData* encryptedOutgoing = [self.session encrypt:joinData error:error];
    if (encryptedOutgoing == nil) return nil;

    self->_state = kAcceptDone;

    return [[KCJoiningMessage messageWithType:kCircleBlob
                                         data:encryptedOutgoing
                                        error:error] der];
}


- (nullable NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error {
    NSData* result = nil;

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

- (bool) isDone {
    return self.state == kAcceptDone;
}

@end
