//
//  SecurityPairing.m
//  Security_ios
//
//  Created by Love Hörnquist Åstrand on 2017-02-28.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecItemPriv.h>
#import <Security/SecureObjectSync/SOSAccount.h>
#import <Security/SecureObjectSync/SOSAccountPriv.h>
#import <Security/SecureObjectSync/SOSCircle.h>
#import <KeychainCircle/KeychainCircle.h>
#import <XCTest/XCTest.h>
#import "SecCFWrappers.h"
#import "SOSRegressionUtilities.h"

@interface FakeNSXPCConnection : NSObject
- (instancetype) initWithControl:(id<SOSControlProtocol>)control;
- (id)remoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler;
@end
@interface FakeNSXPCConnection ()
@property id<SOSControlProtocol> control;
@end
@implementation FakeNSXPCConnection
- (instancetype) initWithControl:(id<SOSControlProtocol>)control
{
    self = [super init];
    if (self) {
        _control = control;
    }
    return self;
}
- (id)remoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler
{
    (void)failureHandler;
    return _control;
}
@end


@interface KCPairingTest : XCTestCase

@end

@interface FCPairingFakeSOSControl : NSObject <SOSControlProtocol>
@property (assign) SecKeyRef accountPrivateKey;
@property (assign) SecKeyRef accountPublicKey;
@property (assign) SecKeyRef deviceKey;
@property (assign) SecKeyRef octagonSigningKey;
@property (assign) SecKeyRef octagonEncryptionKey;
@property (assign) SOSCircleRef circle;
@property (assign) SOSFullPeerInfoRef fullPeerInfo;
@property (assign) bool application;
@end

@implementation FCPairingFakeSOSControl

- (instancetype)initWithRandomAccountKey:(bool)randomAccountKey circle:(SOSCircleRef)circle
{
    if ((self = [super init])) {
        SecKeyRef publicKey = NULL;
        NSDictionary* parameters = @{
            (__bridge NSString*)kSecAttrKeyType:(__bridge NSString*) kSecAttrKeyTypeEC,
            (__bridge NSString*)kSecAttrKeySizeInBits: @(256),
            (__bridge NSString*)kSecAttrNoLegacy : @YES,
            (__bridge NSString*)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleAfterFirstUnlock,
            (__bridge id)kSecPrivateKeyAttrs : @{
                    (__bridge NSString*)kSecAttrLabel : @"delete me test case - private",
                    (__bridge NSString*)kSecAttrIsPermanent : @YES,
                    (__bridge NSString*)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleAfterFirstUnlock,
            },
            (__bridge id)kSecPublicKeyAttrs : @{
                    (__bridge NSString*)kSecAttrLabel : @"delete me test case - public",
                    (__bridge NSString*)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleAfterFirstUnlock,
            }
        };
        if(SecKeyGeneratePair((__bridge CFDictionaryRef)parameters, &publicKey, &_deviceKey) != 0) {
            NSLog(@"failed to create device key");
            return nil;
        }
        CFReleaseNull(publicKey);
        
        NSMutableDictionary* octagonParameters = [parameters mutableCopy];
        octagonParameters[(__bridge NSString*)kSecAttrKeySizeInBits] = @(384);
        if(SecKeyGeneratePair((__bridge CFDictionaryRef)octagonParameters, &publicKey, &_octagonSigningKey) != 0) {
            NSLog(@"failed to create octagon signing key");
            return nil;
        }
        CFReleaseNull(publicKey);

        if(SecKeyGeneratePair((__bridge CFDictionaryRef)octagonParameters, &publicKey, &_octagonEncryptionKey) != 0) {
            NSLog(@"failed to create octagon signing key");
            return nil;
        }
        CFReleaseNull(publicKey);


        _circle = (SOSCircleRef)CFRetain(circle);

        CFErrorRef error = NULL;

        CFDictionaryRef gestalt = (__bridge CFDictionaryRef)@{
            @"ComputerName" : @"name",
        };

        _fullPeerInfo = SOSFullPeerInfoCreate(NULL, gestalt, NULL, _deviceKey, _octagonSigningKey, _octagonEncryptionKey, &error);
        CFReleaseNull(error);

        if (randomAccountKey) {

            NSDictionary* accountParams = @{
                (__bridge NSString*)kSecAttrKeyType:(__bridge NSString*) kSecAttrKeyTypeEC,
                (__bridge NSString*)kSecAttrKeySizeInBits: @(256),
                (__bridge NSString*)kSecAttrNoLegacy : @YES,
                (__bridge NSString*)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleAfterFirstUnlock,
            };

            if(SecKeyGeneratePair((__bridge CFDictionaryRef)accountParams, &publicKey, &_accountPrivateKey) != 0) {
                NSLog(@"failed to create account signing key");
                return nil;
            }
            CFReleaseNull(publicKey);

            _accountPublicKey = SecKeyCopyPublicKey(_accountPrivateKey);

            [self signApplicationIfNeeded];
        }
    }
    return self;
}

- (void)dealloc
{
    if (_accountPrivateKey) {
        SecItemDelete((__bridge CFTypeRef)@{ (__bridge id)kSecValueRef : (__bridge id)_accountPrivateKey });
        CFReleaseNull(_accountPrivateKey);
    }
    if (_deviceKey) {
        SecItemDelete((__bridge CFTypeRef)@{ (__bridge id)kSecValueRef : (__bridge id)_deviceKey });
        CFReleaseNull(_deviceKey);
    }
    if (_octagonSigningKey) {
        SecItemDelete((__bridge CFTypeRef)@{ (__bridge id)kSecValueRef : (__bridge id)_octagonSigningKey });
        CFReleaseNull(_octagonSigningKey);
    }
    if (_octagonEncryptionKey) {
        SecItemDelete((__bridge CFTypeRef)@{ (__bridge id)kSecValueRef : (__bridge id)_octagonEncryptionKey });
        CFReleaseNull(_octagonEncryptionKey);
    }
    CFReleaseNull(_circle);
    CFReleaseNull(_fullPeerInfo);
}

- (SOSPeerInfoRef)peerInfo
{
    return SOSFullPeerInfoGetPeerInfo(_fullPeerInfo);
}

- (void)signApplicationIfNeeded
{
    CFErrorRef error = NULL;

    _application = SOSFullPeerInfoPromoteToApplication(_fullPeerInfo, _accountPrivateKey, &error);
    if (!_application)
        abort();
}

- (void)initialSyncCredentials:(uint32_t)flags complete:(void (^)(NSArray *, NSError *))complete
{
    complete(@[], NULL);
}

- (void)importInitialSyncCredentials:(NSArray *)items complete:(void (^)(bool success, NSError *))complete
{
    complete(true, NULL);
}

- (void)triggerSync:(NSArray<NSString *> *)peers complete:(void(^)(bool success, NSError *))complete
{
    complete(true, NULL);
}

//MARK - FCPairingFakeSOSControl SOSControlProtocol

- (void)userPublicKey:(void ((^))(BOOL trusted, NSData *spki, NSError *error))complete
{
    complete(false, NULL, NULL);
}

- (void)performanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))complete
{
    complete(@{});
}
- (void)kvsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))complete
{
    complete(@{});
}
- (void)idsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))complete
{
    complete(@{});
}
- (void)rateLimitingPerformanceCounters:(void(^)(NSDictionary <NSString *, NSString *> *))complete
{
    complete(@{});
}
- (void)stashedCredentialPublicKey:(void(^)(NSData *, NSError *error))complete
{
    NSData *publicKey = NULL;
    NSError *error = NULL;
    if (self.accountPrivateKey) {
        publicKey = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo(self.accountPrivateKey));
    } else {
        error = [NSError errorWithDomain:@"FCPairingFakeSOSControl" code:2 userInfo:NULL];
    }
    complete(publicKey, error);
}

- (void)assertStashedAccountCredential:(void(^)(BOOL result, NSError *error))complete
{
    complete(self.accountPrivateKey != NULL, NULL);
}

- (void)validatedStashedAccountCredential:(void(^)(NSData *credential, NSError *error))complete
{
    NSData *key = NULL;
    CFErrorRef error = NULL;
    if (self.accountPrivateKey) {
        key = CFBridgingRelease(SecKeyCopyExternalRepresentation(self.accountPrivateKey, &error));
    } else {
        error = (CFErrorRef)CFBridgingRetain([NSError errorWithDomain:@"FCPairingFakeSOSControl" code:1 userInfo:NULL]);
    }
    complete(key, (__bridge NSError *)error);
    CFReleaseNull(error);
}

- (void)stashAccountCredential:(NSData *)credential complete:(void(^)(bool success, NSError *error))complete
{
    SecKeyRef accountPrivateKey = NULL;
    CFErrorRef error = NULL;
    NSDictionary *attributes = @{
        (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
        (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
    };

    accountPrivateKey = SecKeyCreateWithData((__bridge CFDataRef)credential, (__bridge CFDictionaryRef)attributes, &error);
    if (accountPrivateKey == NULL) {
        complete(false, (__bridge NSError *)error);
        CFReleaseNull(error);
        return;
    }

    _accountPrivateKey = accountPrivateKey;
    _accountPublicKey = SecKeyCopyPublicKey(_accountPrivateKey);

    [self signApplicationIfNeeded];

    complete(true, NULL);
}

- (void)myPeerInfo:(void(^)(NSData *application, NSError *error))complete
{
    CFErrorRef error = NULL;

    [self signApplicationIfNeeded];

    NSData *application = CFBridgingRelease(SOSPeerInfoCopyEncodedData([self peerInfo], NULL, &error));
    complete(application, (__bridge NSError *)error);

    CFReleaseNull(error);
}

- (void)circleJoiningBlob:(NSData *)applicantData complete:(void (^)(NSData *blob, NSError *))complete
{
    CFErrorRef error = NULL;
    CFDataRef signature = NULL;
    SOSCircleRef prunedCircle = SOSCircleCopyCircle(NULL, _circle, &error);
    (void)SOSCirclePreGenerationSign(prunedCircle, _accountPublicKey, &error);

    SOSGenCountRef gencount = SOSGenerationIncrementAndCreate(SOSCircleGetGeneration(prunedCircle));
    if (gencount == NULL)
        abort();


    SOSPeerInfoRef applicant = SOSPeerInfoCreateFromData(NULL, &error, (__bridge CFDataRef)applicantData);
    if (applicant == NULL)
        abort();

    signature = SOSCircleCopyNextGenSignatureWithPeerAdded(prunedCircle, applicant, _deviceKey, &error);
    if(applicant) {
        CFRelease(applicant);
        applicant = NULL;
    }

    NSData *pbblob = CFBridgingRelease(SOSPiggyBackBlobCopyEncodedData(gencount, _deviceKey, signature, &error));

    CFReleaseNull(signature);
    CFReleaseNull(gencount);
    CFReleaseNull(prunedCircle);

    complete(pbblob, NULL);
}

- (void)joinCircleWithBlob:(NSData *)blob version:(PiggyBackProtocolVersion)version complete:(void (^)(bool success, NSError *))complete
{
    SOSGenCountRef gencount = NULL;
    SecKeyRef pubKey = NULL;
    CFDataRef signature = NULL;
    CFErrorRef error = NULL;
    bool setInitialSyncTimeoutToV0 = false;

    if (!SOSPiggyBackBlobCreateFromData(&gencount, &pubKey, &signature, (__bridge CFDataRef)blob, kPiggyV1, &setInitialSyncTimeoutToV0, &error)) {
        complete(true, (__bridge NSError *)error);
        CFReleaseNull(error);
        return;
    }

    (void)SOSCircleAcceptPeerFromHSA2(_circle,
                                      _accountPrivateKey,
                                      gencount,
                                      pubKey,
                                      signature,
                                      _fullPeerInfo,
                                      &error);

    CFReleaseNull(gencount);
    CFReleaseNull(pubKey);
    CFReleaseNull(signature);

    complete(true, (__bridge NSError *)error);

    CFReleaseNull(error);

}

- (void)getWatchdogParameters:(void (^)(NSDictionary*, NSError*))complete
{
    // intentionally left blank
    // these are used by the security/2 tool and are only declared here to make the compiler happy about conforming the protocol we shoved the methods into
}


- (void)setWatchdogParmeters:(NSDictionary*)parameters complete:(void (^)(NSError*))complete
{
    // intentionally left blank
    // these are used by the security/2 tool and are only declared here to make the compiler happy about conforming the protocol we shoved the methods into
}
@end

@implementation KCPairingTest

- (void)testSecPairBasicTest
{
    bool sp1compete = false, sp2compete = false;
    NSData *sp1data = NULL;
    NSData *sp2data = NULL;
    SOSCircleRef circle = NULL;
    unsigned count = 0;
    CFErrorRef cferror = NULL;
    KCPairingChannel *sp1, *sp2;

    circle = SOSCircleCreate(NULL, CFSTR("TEST DOMAIN"), NULL);
    XCTAssert(circle, "circle");

    FCPairingFakeSOSControl *fc1 = [[FCPairingFakeSOSControl alloc] initWithRandomAccountKey:false circle:circle];
    XCTAssert(fc1, "create fake soscontrol 1");

    FCPairingFakeSOSControl *fc2 = [[FCPairingFakeSOSControl alloc] initWithRandomAccountKey:true circle:circle];
    XCTAssert(fc2, "create fake soscontrol 2");


    XCTAssert(SOSCircleRequestAdmission(circle, fc2.accountPrivateKey, fc2.fullPeerInfo, &cferror), "SOSCircleRequestAdmission: %@", cferror);
    CFReleaseNull(cferror);

    XCTAssert(SOSCircleAcceptRequest(circle, fc2.accountPrivateKey, fc2.fullPeerInfo, [fc2 peerInfo], &cferror), "SOSCircleAcceptRequest device 1: %@", cferror);
    CFReleaseNull(cferror);

    XCTAssert(SOSCircleHasPeer(circle, [fc2 peerInfo], &cferror), "HasPeer 2: %@", cferror);
    CFReleaseNull(cferror);


    sp1 = [KCPairingChannel pairingChannelInitiator:NULL];
    [sp1 setXPCConnectionObject:(NSXPCConnection *)[[FakeNSXPCConnection alloc] initWithControl:fc1]];

    sp2 = [KCPairingChannel pairingChannelAcceptor:NULL];
    [sp2 setXPCConnectionObject:(NSXPCConnection *)[[FakeNSXPCConnection alloc] initWithControl:fc2]]   ;

    while(1) {
        NSError *error = NULL;

        sp1data = [sp1 exchangePacket:sp2data complete:&sp1compete error:&error];

        if (sp1compete && sp2compete) {
            XCTAssert(sp1data == NULL, "sp1 done, yet there is data");
            break;
        }
        XCTAssert(!sp2compete, "sp2 completed w/o sp1");

        XCTAssert(sp1data != NULL, "sp1 not done, yet there is no data: %@", error);
        if (sp1data == NULL)
            break;

        /* send sp1data to peer : BOB CHANNEL HERE */

        sp2data = [sp2 exchangePacket:sp1data complete:&sp2compete error:&error];
        XCTAssert(sp2data != NULL, "sp2 didn't return data: %@", error);
        if (sp2data == NULL)
            break;

        if (sp1compete && sp2compete)
            break;

        XCTAssert(!sp1compete, "sp2 completed w/o sp1");

        count++;
        if (count > 10)
            abort();
    };

    XCTAssert(sp1compete && sp2compete, "both parties not completed");

    XCTAssert(fc1.accountPrivateKey, "no accountPrivateKey in fc1");
    XCTAssert(fc2.accountPrivateKey, "no accountPrivateKey in fc2");
    XCTAssert(CFEqualSafe(fc1.accountPrivateKey, fc2.accountPrivateKey), "no accountPrivateKey not same in both");

    if (sp1compete && sp2compete)
        NSLog(@"pairing complete");

    XCTAssert(SOSCircleHasPeer(circle, [fc1 peerInfo], &cferror), "HasPeer 1: %@", cferror);
    CFReleaseNull(cferror);
    XCTAssert(SOSCircleHasPeer(circle, [fc2 peerInfo], &cferror), "HasPeer 2: %@", cferror);
    CFReleaseNull(cferror);

    XCTAssert(sp1.needInitialSync == false, "no longer need initial sync");


}

@end
