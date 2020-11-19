
#import "FakeSOSControl.h"

@implementation FakeNSXPCConnection
- (instancetype) initWithControl:(id<SOSControlProtocol>)control
{
    if ((self = [super init])) {
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

@implementation FCPairingFakeSOSControl

- (instancetype)initWithRandomAccountKey:(bool)randomAccountKey circle:(SOSCircleRef)circle
{
    if ((self = [super init])) {
        SecKeyRef publicKey = NULL;
        NSDictionary* parameters = @{
                                     (__bridge NSString*)kSecAttrKeyType:(__bridge NSString*) kSecAttrKeyTypeEC,
                                     (__bridge NSString*)kSecAttrKeySizeInBits: @(256),
                                     (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
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
                                            (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
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
    // Make up a fake TLK
    NSMutableArray<NSDictionary *> *items = [NSMutableArray array];
    if (flags & SOSControlInitialSyncFlagTLK) {
        NSString *tlkUUID = [[NSUUID UUID] UUIDString];
        NSDictionary *fakeTLK = @{
            @"class": @"inet",
            @"agrp": @"com.apple.security.ckks",
            @"vwht": @"PCS-master",
            @"pdmn": @"ck",
            @"desc": @"tlk",
            @"srvr": @"fakeZone",
            @"acct": tlkUUID,
            @"path": tlkUUID,
            @"v_Data": [NSData data],
        };
        [items addObject:fakeTLK];
    }
    complete(items, nil);
}

- (void)importInitialSyncCredentials:(NSArray *)items complete:(void (^)(bool success, NSError *))complete
{
    complete(true, nil);
}

- (void)rpcTriggerSync:(NSArray<NSString *> *)peers complete:(void(^)(bool success, NSError *))complete
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

- (void)circleHash:(void (^)(NSString *, NSError *))complete
{
    NSString *data = CFBridgingRelease(SOSCircleCopyHashString(_circle));
    complete(data, NULL);
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

- (void)ghostBust:(SOSAccountGhostBustingOptions)options complete:(void (^)(bool, NSError *))complete {
    complete(false, nil);
}

- (void)ghostBustPeriodic:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete{
    complete(false, nil);
}

- (void)ghostBustTriggerTimed:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete {
    complete(false, nil);
}

- (void) ghostBustInfo: (void(^)(NSData *json, NSError *error))complete {
    complete(nil, nil);
}

- (void)iCloudIdentityStatus_internal: (void(^)(NSDictionary *tableSpid, NSError *error))complete {
    complete(nil, nil);
}

- (void) iCloudIdentityStatus: (void(^)(NSData *json, NSError *error))complete {
    complete(nil, nil);
}

- (void)rpcTriggerBackup:(NSArray<NSString *> *)backupPeers complete:(void (^)(NSError *))complete {
    complete(nil);
}

- (void)rpcTriggerRingUpdate:(void (^)(NSError *))complete {
    complete(nil);
}


@end
