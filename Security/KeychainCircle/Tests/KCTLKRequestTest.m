#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>

#import <KeychainCircle/KCJoiningSession.h>
#import <KeychainCircle/KCAccountKCCircleDelegate.h>
#import <KeychainCircle/KCError.h>
#import <KeychainCircle/KCJoiningMessages.h>
#import <KeychainCircle/NSError+KCCreationHelpers.h>
#import <KeychainCircle/KCAESGCMDuplexSession.h>

#include <Security/SecBase.h>
#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#include "keychain/SecureObjectSync/SOSPeerInfoInternal.h"

#include <CommonCrypto/CommonRandomSPI.h>


static SecKeyRef GenerateFullECKey_internal(int keySize,  NSError** error)
{
    SecKeyRef full_key = NULL;

    NSDictionary* keygen_parameters = @{ (__bridge NSString*)kSecAttrKeyType:(__bridge NSString*) kSecAttrKeyTypeEC,
                                         (__bridge NSString*)kSecAttrKeySizeInBits: [NSNumber numberWithInt: keySize] };


    (void) OSStatusError(SecKeyGeneratePair((__bridge CFDictionaryRef)keygen_parameters, NULL, &full_key), error, @"Generate Key failed");

    return full_key;
}

static SecKeyRef GenerateFullECKey(int keySize, NSError** error) {
    return GenerateFullECKey_internal(keySize, error);
}

static NSData* createTlkRequestMessage (KCAESGCMDuplexSession* aesSession) {
    char someData[] = {1,2,3,4,5,6};
    NSError* error = NULL;
    NSData* rndPadding = [NSData dataWithBytes:(void*)someData length:sizeof(someData)];
    KCJoiningMessage* tlkRequestMessage = [KCJoiningMessage messageWithType: kTLKRequest data:rndPadding error:&error];
    return [tlkRequestMessage der];
}

@interface KCJoiningRequestTestDelegate : NSObject <KCJoiningRequestSecretDelegate, KCJoiningRequestCircleDelegate>
@property (readwrite) NSString* sharedSecret;

@property (readonly) NSString* accountCode;
@property (readonly) NSData* circleJoinData;

@property (readwrite) NSString* incorrectSecret;
@property (readwrite) int incorrectTries;


+ (id) requestDelegateWithSecret:(NSString*) secret;
- (id) init NS_UNAVAILABLE;
- (id) initWithSecret: (NSString*) secret
      incorrectSecret: (NSString*) wrongSecret
       incorrectTries: (int) retries NS_DESIGNATED_INITIALIZER;
- (NSString*) secret;
- (NSString*) verificationFailed: (bool) codeChanged;
- (SOSPeerInfoRef) copyPeerInfoError: (NSError**) error;
- (bool) processCircleJoinData: (NSData*) circleJoinData version:(PiggyBackProtocolVersion)version error: (NSError**)error ;
- (bool) processAccountCode: (NSString*) accountCode error: (NSError**)error;

@end

@implementation KCJoiningRequestTestDelegate

+ (id) requestDelegateWithSecret:(NSString*) secret {
    return [[KCJoiningRequestTestDelegate alloc] initWithSecret:secret
                                                incorrectSecret:@""
                                                 incorrectTries:0];
}

+ (id) requestDelegateWithSecret:(NSString*) secret
                 incorrectSecret:(NSString*) wrongSecret
                  incorrectTries:(int) retries {
    return [[KCJoiningRequestTestDelegate alloc] initWithSecret:secret
                                                incorrectSecret:wrongSecret
                                                 incorrectTries:retries];
}


- (id) initWithSecret: (NSString*) secret
      incorrectSecret: (NSString*) incorrectSecret
       incorrectTries: (int) retries {
    if ( self = [super init] ) {
        self.sharedSecret = secret;
        self.incorrectSecret = incorrectSecret;
        self.incorrectTries = retries;
    }
    return self;
}

- (NSString*) nextSecret {
    if (self.incorrectTries > 0) {
        self.incorrectTries -= 1;
        return self.incorrectSecret;
    }
    return self.sharedSecret;
}

- (NSString*) secret {
    return [self nextSecret];
}

- (NSString*) verificationFailed: (bool) codeChanged {
    return [self nextSecret];
}

- (SOSPeerInfoRef) copyPeerInfoError: (NSError**) error {
    return NULL;
}

- (bool) processCircleJoinData: (NSData*) circleJoinData version:(PiggyBackProtocolVersion)version error: (NSError**)error {
    self->_circleJoinData = circleJoinData;
    return true;
}

- (bool) processAccountCode: (NSString*) accountCode error: (NSError**)error {
    self->_accountCode = accountCode;
    return true;
}

@end

@interface KCJoiningAcceptTestDelegate : NSObject <KCJoiningAcceptSecretDelegate, KCJoiningAcceptCircleDelegate>
@property (readonly) NSArray<NSString*>* secrets;
@property (readwrite) NSUInteger currentSecret;
@property (readwrite) int retriesLeft;
@property (readwrite) int retriesPerSecret;

@property (readonly) NSString* codeToUse;
@property (readonly) NSData* circleJoinData;
@property (readonly) SOSPeerInfoRef peerInfo;

+ (id) acceptDelegateWithSecret: (NSString*) secret code: (NSString*) code;
+ (id) acceptDelegateWithSecrets: (NSArray<NSString*>*) secrets retries: (int) retries code: (NSString*) code;
- (id) initWithSecrets: (NSArray<NSString*>*) secrets retries: (int) retries code: (NSString*) code NS_DESIGNATED_INITIALIZER;

- (NSString*) secret;
- (NSString*) accountCode;

- (KCRetryOrNot) verificationFailed: (NSError**) error;
- (NSData*) circleJoinDataFor: (SOSPeerInfoRef) peer
                        error: (NSError**) error;

- (id) init NS_UNAVAILABLE;

@end

@implementation KCJoiningAcceptTestDelegate

+ (id) acceptDelegateWithSecrets: (NSArray<NSString*>*) secrets retries: (int) retries code: (NSString*) code {
    return [[KCJoiningAcceptTestDelegate alloc] initWithSecrets:secrets retries:retries code:code];

}

+ (id) acceptDelegateWithSecret: (NSString*) secret code: (NSString*) code {
    return [[KCJoiningAcceptTestDelegate alloc] initWithSecret:secret code:code];
}

- (id) initWithSecret: (NSString*) secret code: (NSString*) code {
    return [self initWithSecrets:@[secret] retries:3 code:code];
}

- (id) initWithSecrets: (NSArray<NSString*>*) secrets retries: (int) retries code: (NSString*) code {
    self = [super init];

    self->_secrets = secrets;
    self.currentSecret = 0;
    self->_retriesPerSecret = retries;
    self->_retriesLeft = self.retriesPerSecret;

    self->_codeToUse = code;

    uint8_t joinDataBuffer[] = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
    self->_circleJoinData = [NSData dataWithBytes: joinDataBuffer length: sizeof(joinDataBuffer) ];

    return self;
}

- (KCRetryOrNot) advanceSecret {
    if (self.retriesLeft == 0) {
        self.currentSecret += 1;
        if (self.currentSecret >= [self.secrets count]) {
            self.currentSecret = [self.secrets count] - 1;
        }
        self.retriesLeft = self.retriesPerSecret;
        return kKCRetryWithNewChallenge;
    } else {
        self.retriesLeft -= 1;
        return kKCRetryWithSameChallenge;
    }
}

- (NSString*) secret {
    return self.secrets[self.currentSecret];
}
- (NSString*) accountCode {
    return self.codeToUse;
}

- (KCRetryOrNot) verificationFailed: (NSError**) error {
    return [self advanceSecret];
}

- (NSData*) circleJoinDataFor: (SOSPeerInfoRef) peer
                        error: (NSError**) error {
    uint8_t joinDataBuffer[] = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };

    self->_peerInfo = peer;
    return [NSData dataWithBytes: joinDataBuffer length: sizeof(joinDataBuffer) ];
}

-(NSData*) circleGetInitialSyncViews:(SOSInitialSyncFlags)flags error:(NSError**) error{
    char testData[] = {0,1,2,3,4,5,6,7,8,9};
    return [NSData dataWithBytes:testData length:sizeof(testData)];
    //return [[KCJoiningAcceptAccountCircleDelegate delegate] circleGetInitialSyncViews:flags error:error]; //Need security entitlements!
}

@end

@interface KCTLKRequestTest : XCTestCase

@end

@implementation KCTLKRequestTest

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testTLKRequest {
    NSError* error = nil;

    NSString* secret = @"123456";
    NSString* code = @"987654";

    uint64_t dsid = 0x1234567887654321;

    KCJoiningRequestTestDelegate* requestDelegate = [KCJoiningRequestTestDelegate requestDelegateWithSecret: secret];
    KCJoiningRequestSecretSession *requestSession = [[KCJoiningRequestSecretSession alloc] initWithSecretDelegate:requestDelegate
                                                                                                             dsid:dsid
                                                                                                              rng:ccDRBGGetRngState()
                                                                                                            error:&error];

    NSData* initialMessage = [requestSession initialMessage: &error];

    XCTAssertNotNil(initialMessage, @"No initial message");
    XCTAssertNil(error, @"Got error %@", error);

    KCJoiningAcceptTestDelegate* acceptDelegate = [KCJoiningAcceptTestDelegate acceptDelegateWithSecret:secret code:code];
    KCJoiningAcceptSession* acceptSession = [[KCJoiningAcceptSession alloc] initWithSecretDelegate:acceptDelegate
                                                                                    circleDelegate:acceptDelegate
                                                                                              dsid:dsid
                                                                                               rng:ccDRBGGetRngState()
                                                                                             error:&error];
    
    error = nil;
    NSData* challenge = [acceptSession processMessage: initialMessage error: &error];

    XCTAssertNotNil(challenge, @"No initial message");
    XCTAssertNil(error, @"Got error %@", error);

    error = nil;
    NSData* response = [requestSession processMessage: challenge error: &error];

    XCTAssertNotNil(response, @"No response message");
    XCTAssertNil(error, @"Got error %@", error);

    error = nil;
    NSData* verification = [acceptSession processMessage: response error: &error];

    XCTAssertNotNil(verification, @"No verification message");
    XCTAssertNil(error, @"Got error %@", error);

    error = nil;
    NSData* doneMessage = [requestSession processMessage: verification error: &error];

    XCTAssertNotNil(doneMessage, @"No response message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertTrue([requestSession isDone], @"SecretSession done");
    XCTAssertFalse([acceptSession isDone], @"Unexpected accept session done");

    KCAESGCMDuplexSession* aesSession = [requestSession session];
    requestSession = nil;

    KCJoiningRequestCircleSession* requestSecretSession = [KCJoiningRequestCircleSession sessionWithCircleDelegate:requestDelegate session:aesSession error:&error];

    XCTAssertNotNil(requestSecretSession, @"No request secret session");
    XCTAssertNil(error, @"Got error %@", error);

    NSData* tlkRequestMessage = createTlkRequestMessage(aesSession);
    XCTAssertNotNil(tlkRequestMessage, @"No TLKRequest message");
    
    NSData* tlkMessage = [acceptSession processMessage:tlkRequestMessage error:&error];
    XCTAssertNotNil(tlkMessage, @"No tlkData message");
    XCTAssertNil(error, @"Got error %@", error);
        
    KCJoiningMessage* receivedKCJoinMessage = [KCJoiningMessage messageWithDER:tlkMessage error:&error];
    XCTAssertNotNil(receivedKCJoinMessage, @"No receivedKCJoinMessage message");
    XCTAssertNil(error, @"Got error %@", error);

    NSData* tlkDecryptedData = [aesSession decryptAndVerify:receivedKCJoinMessage.firstData  error:&error];
    XCTAssertNotNil(tlkDecryptedData, @"No tlkDecryptedData message");
    XCTAssertNil(error, @"Got error %@", error);
    
    //check for tlkc content
    NSData* initialSync = [acceptDelegate circleGetInitialSyncViews:kSOSInitialSyncFlagTLKs error:&error];
    XCTAssertNotNil(initialSync, @"No initialSync data");
    XCTAssertNil(error, @"Got error %@", error);
    
    XCTAssertEqualObjects(initialSync, tlkDecryptedData, @"TLK data is different.");
}
@end
