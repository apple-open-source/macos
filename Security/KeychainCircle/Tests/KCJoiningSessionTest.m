//
//  KCJoiningSessionTest.m
//  Security
//
//

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>

#import <KeychainCircle/KCJoiningSession.h>
#import <KeychainCircle/KCError.h>
#import <KeychainCircle/NSError+KCCreationHelpers.h>
#import <KeychainCircle/KCAESGCMDuplexSession.h>

#include <Security/SecBase.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>

#include <CommonCrypto/CommonRandomSPI.h>


__unused static SOSFullPeerInfoRef SOSNSFullPeerInfoCreate(NSDictionary* gestalt,
                                                  NSData* backupKey, SecKeyRef signingKey,
                                                  NSError**error)
{
    CFErrorRef errorRef = NULL;

    SOSFullPeerInfoRef result = SOSFullPeerInfoCreate(NULL, (__bridge CFDictionaryRef) gestalt, (__bridge CFDataRef) backupKey, signingKey, &errorRef);

    if (errorRef && error) {
        *error = (__bridge_transfer NSError*) errorRef;
        errorRef = NULL;
    }

    return result;
}

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


__unused static SOSFullPeerInfoRef SOSCreateFullPeerInfoFromName(NSString* name, SecKeyRef* outSigningKey, NSError** error)
{
    if (outSigningKey == NULL)
        return NULL;

    *outSigningKey = GenerateFullECKey(256, error);
    if (*outSigningKey == NULL)
        return NULL;

    return SOSNSFullPeerInfoCreate(@{(__bridge NSString*)kPIUserDefinedDeviceNameKey:name}, nil, *outSigningKey, error);
}


@interface KCJoiningRequestTestDelegate : NSObject <KCJoiningRequestSecretDelegate, KCJoiningRequestCircleDelegate>
@property (readwrite) NSString* sharedSecret;

@property (readonly) NSString* accountCode;
@property (readonly) NSData* circleJoinData;
@property (readwrite) SOSPeerInfoRef peerInfo;

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
- (bool) processCircleJoinData: (NSData*) circleJoinData error: (NSError**)error ;
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
    self = [super init];

    SecKeyRef signingKey = GenerateFullECKey(256, NULL);

    self.peerInfo = SOSPeerInfoCreate(NULL, (__bridge CFDictionaryRef) @{(__bridge NSString*)kPIUserDefinedDeviceNameKey:@"Fakey"}, NULL, signingKey, NULL);

    if (self.peerInfo == NULL)
        return nil;

    self.sharedSecret = secret;
    self.incorrectSecret = incorrectSecret;
    self.incorrectTries = retries;

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
    return self.peerInfo;
}

- (bool) processCircleJoinData: (NSData*) circleJoinData error: (NSError**)error {
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


@end


@interface KCJoiningSessionTest : XCTestCase

@end

@implementation KCJoiningSessionTest

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testJoiningSession {
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

    error = nil;
    NSData* peerInfoMessage = [requestSecretSession initialMessage: &error];

    XCTAssertNotNil(peerInfoMessage, @"No peerInfo message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertEqualObjects(requestDelegate.accountCode, acceptDelegate.codeToUse, @"Code made it");

    error = nil;
    NSData* blobMessage = [acceptSession processMessage:peerInfoMessage error: &error];

    XCTAssertNotNil(blobMessage, @"No blob message");
    XCTAssertNil(error, @"Got error %@", error);

    // We have different peer_info types due to wierd linking of our tests.
    // Compare the der representations:
    NSData* rp_der = requestDelegate.peerInfo != nil ? (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(requestDelegate.peerInfo, NULL, NULL) : nil;
    NSData* ap_der = acceptDelegate.peerInfo != nil ? (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(acceptDelegate.peerInfo, NULL, NULL) : nil;

    XCTAssertEqualObjects(rp_der, ap_der, @"Peer infos match");

    error = nil;
    NSData* nothing = [requestSecretSession processMessage:blobMessage error: &error];

    XCTAssertEqualObjects(requestDelegate.circleJoinData, acceptDelegate.circleJoinData);

    XCTAssertNotNil(nothing, @"No initial message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertTrue([requestSecretSession isDone], @"requesor done");
    XCTAssertTrue([acceptSession isDone], @"acceptor done");

}

- (void)testJoiningSessionRetry {
    NSError* error = nil;

    NSString* secret = @"123456";
    NSString* code = @"987654";

    uint64_t dsid = 0x1234567887654321;

    KCJoiningRequestTestDelegate* requestDelegate = [KCJoiningRequestTestDelegate requestDelegateWithSecret: secret incorrectSecret:@"777888" incorrectTries:3];
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

    NSData* response = nil;
    NSData* verification = nil;

    NSData* nextChallenge = challenge;
    for (int tries = 0; tries < 4; ++tries) {
        error = nil;
        response = [requestSession processMessage: nextChallenge error: &error];

        XCTAssertNotNil(response, @"No response message");
        XCTAssertNil(error, @"Got error %@", error);

        XCTAssertNotEqualObjects(requestDelegate.accountCode, acceptDelegate.codeToUse, @"Code should not make it");

        error = nil;
        verification = [acceptSession processMessage: response error: &error];

        XCTAssertNotNil(verification, @"No verification message");
        XCTAssertNil(error, @"Got error %@", error);

        nextChallenge = verification;
    }

    error = nil;
    NSData* doneMessage = [requestSession processMessage: verification error: &error];

    XCTAssertNotNil(doneMessage, @"No response message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertTrue([requestSession isDone], @"SecretSession done");
    XCTAssertFalse([acceptSession isDone], @"Unexpected accept session done");

    KCAESGCMDuplexSession* aesSession = [requestSession session];
    requestSession = nil;

    error = nil;
    KCJoiningRequestCircleSession* requestSecretSession = [KCJoiningRequestCircleSession sessionWithCircleDelegate:requestDelegate session:aesSession error:&error];

    XCTAssertNotNil(requestSecretSession, @"No request secret session");
    XCTAssertNil(error, @"Got error %@", error);

    error = nil;
    NSData* peerInfoMessage = [requestSecretSession initialMessage: &error];

    XCTAssertNotNil(peerInfoMessage, @"No peerInfo message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertEqualObjects(requestDelegate.accountCode, acceptDelegate.codeToUse, @"Code made it");

    error = nil;
    NSData* blobMessage = [acceptSession processMessage:peerInfoMessage error: &error];

    XCTAssertNotNil(blobMessage, @"No blob message");
    XCTAssertNil(error, @"Got error %@", error);

    // We have different peer_info types due to wierd linking of our tests.
    // Compare the der representations:
    NSData* rp_der = requestDelegate.peerInfo != nil ? (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(requestDelegate.peerInfo, NULL, NULL) : nil;
    NSData* ap_der = acceptDelegate.peerInfo != nil ? (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(acceptDelegate.peerInfo, NULL, NULL) : nil;

    XCTAssertEqualObjects(rp_der, ap_der, @"Peer infos match");

    error = nil;
    NSData* nothing = [requestSecretSession processMessage:blobMessage error: &error];

    XCTAssertEqualObjects(requestDelegate.circleJoinData, acceptDelegate.circleJoinData);

    XCTAssertNotNil(nothing, @"No initial message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertTrue([requestSecretSession isDone], @"requesor done");
    XCTAssertTrue([acceptSession isDone], @"acceptor done");
    
}

- (void)testJoiningSessionCodeChange {
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

    KCJoiningAcceptTestDelegate* acceptDelegate = [KCJoiningAcceptTestDelegate acceptDelegateWithSecrets:@[@"222222", @"3333333", secret] retries:1 code:code];
    KCJoiningAcceptSession* acceptSession = [[KCJoiningAcceptSession alloc] initWithSecretDelegate:acceptDelegate
                                                                                    circleDelegate:acceptDelegate
                                                                                              dsid:dsid
                                                                                               rng:ccDRBGGetRngState()
                                                                                             error:&error];

    error = nil;
    NSData* challenge = [acceptSession processMessage: initialMessage error: &error];

    XCTAssertNotNil(challenge, @"No initial message");
    XCTAssertNil(error, @"Got error %@", error);

    NSData* response = nil;
    NSData* verification = nil;

    NSData* nextChallenge = challenge;
    for (int tries = 0; tries < 5; ++tries) {
        error = nil;
        response = [requestSession processMessage: nextChallenge error: &error];

        XCTAssertNotNil(response, @"No response message");
        XCTAssertNil(error, @"Got error %@", error);

        XCTAssertNotEqualObjects(requestDelegate.accountCode, acceptDelegate.codeToUse, @"Code should not make it");

        error = nil;
        verification = [acceptSession processMessage: response error: &error];

        XCTAssertNotNil(verification, @"No verification message");
        XCTAssertNil(error, @"Got error %@", error);

        nextChallenge = verification;
    }

    error = nil;
    NSData* doneMessage = [requestSession processMessage: verification error: &error];

    XCTAssertNotNil(doneMessage, @"No response message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertTrue([requestSession isDone], @"SecretSession done");
    XCTAssertFalse([acceptSession isDone], @"Unexpected accept session done");

    KCAESGCMDuplexSession* aesSession = [requestSession session];
    requestSession = nil;

    error = nil;
    KCJoiningRequestCircleSession* requestSecretSession = [KCJoiningRequestCircleSession sessionWithCircleDelegate:requestDelegate session:aesSession error:&error];

    XCTAssertNotNil(requestSecretSession, @"No request secret session");
    XCTAssertNil(error, @"Got error %@", error);

    error = nil;
    NSData* peerInfoMessage = [requestSecretSession initialMessage: &error];

    XCTAssertNotNil(peerInfoMessage, @"No peerInfo message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertEqualObjects(requestDelegate.accountCode, acceptDelegate.codeToUse, @"Code made it");

    error = nil;
    NSData* blobMessage = [acceptSession processMessage:peerInfoMessage error: &error];

    XCTAssertNotNil(blobMessage, @"No blob message");
    XCTAssertNil(error, @"Got error %@", error);

    // We have different peer_info types due to wierd linking of our tests.
    // Compare the der representations:
    NSData* rp_der = requestDelegate.peerInfo != nil ? (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(requestDelegate.peerInfo, NULL, NULL) : nil;
    NSData* ap_der = acceptDelegate.peerInfo != nil ? (__bridge_transfer NSData*) SOSPeerInfoCopyEncodedData(acceptDelegate.peerInfo, NULL, NULL) : nil;

    XCTAssertEqualObjects(rp_der, ap_der, @"Peer infos match");

    error = nil;
    NSData* nothing = [requestSecretSession processMessage:blobMessage error: &error];

    XCTAssertEqualObjects(requestDelegate.circleJoinData, acceptDelegate.circleJoinData);

    XCTAssertNotNil(nothing, @"No initial message");
    XCTAssertNil(error, @"Got error %@", error);

    XCTAssertTrue([requestSecretSession isDone], @"requesor done");
    XCTAssertTrue([acceptSession isDone], @"acceptor done");
    
}

@end
