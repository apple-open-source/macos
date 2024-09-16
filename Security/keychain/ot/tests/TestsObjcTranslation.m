#import "TestsObjcTranslation.h"
#import <OCMock/OCMock.h>
#import <Foundation/Foundation.h>
#import <Security/SecItemPriv.h>
#import <SecurityFoundation/SecurityFoundation.h>
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/securityd/SOSCloudCircleServer.h"
#import "keychain/SecureObjectSync/SOSAccountPriv.h"
#import "keychain/OctagonTrust/OctagonTrust.h"
#import "keychain/securityd/SecItemServer.h"
#import "KeychainCircle/PairingChannel.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/Affordance_OTConstants.h"
#import <SoftLinking/SoftLinking.h>


static const uint8_t signingKey_384[] = {
    0x04, 0xe4, 0x1b, 0x3e, 0x88, 0x81, 0x9f, 0x3b, 0x80, 0xd0, 0x28, 0x1c,
    0xd9, 0x07, 0xa0, 0x8c, 0xa1, 0x89, 0xa8, 0x3b, 0x69, 0x91, 0x17, 0xa7,
    0x1f, 0x00, 0x31, 0x91, 0x82, 0x89, 0x1f, 0x5c, 0x44, 0x2d, 0xd6, 0xa8,
    0x22, 0x1f, 0x22, 0x7d, 0x27, 0x21, 0xf2, 0xc9, 0x75, 0xf2, 0xda, 0x41,
    0x61, 0x55, 0x29, 0x11, 0xf7, 0x71, 0xcf, 0x66, 0x52, 0x2a, 0x27, 0xfe,
    0x77, 0x1e, 0xd4, 0x3d, 0xfb, 0xbc, 0x59, 0xe4, 0xed, 0xa4, 0x79, 0x2a,
    0x9b, 0x73, 0x3e, 0xf4, 0xf4, 0xe3, 0xaf, 0xf2, 0x8d, 0x34, 0x90, 0x92,
    0x47, 0x53, 0xd0, 0x34, 0x1e, 0x49, 0x87, 0xeb, 0x11, 0x89, 0x0f, 0x9c,
    0xa4, 0x99, 0xe8, 0x4f, 0x39, 0xbe, 0x21, 0x94, 0x88, 0xba, 0x4c, 0xa5,
    0x6a, 0x60, 0x1c, 0x2f, 0x77, 0x80, 0xd2, 0x73, 0x14, 0x33, 0x46, 0x5c,
    0xda, 0xee, 0x13, 0x8a, 0x3a, 0xdb, 0x4e, 0x05, 0x4d, 0x0f, 0x6d, 0x96,
    0xcd, 0x28, 0xab, 0x52, 0x4c, 0x12, 0x2b, 0x79, 0x80, 0xfe, 0x9a, 0xe4,
    0xf4
};

@implementation TestsObjectiveC : NSObject
+ (void)setNewRecoveryKeyWithData:(OTConfigurationContext*)ctx
                      recoveryKey:(NSString*)recoveryKey
                            reply:(void(^)(void* rk,
                                           NSError* _Nullable error))reply
{
    [OTClique setNewRecoveryKeyWithData:ctx recoveryKey:recoveryKey reply:^(SecRecoveryKey * _Nullable rk, NSError * _Nullable error) {
        reply((__bridge void*)rk, error);
    }];
}

+ (BOOL)saveCoruptDataToKeychainForContainer:(NSString*)containerName
                                   contextID:(NSString*)contextID
                                       error:(NSError**)error
{
    NSData* signingFromBytes = [[NSData alloc] initWithBytes:signingKey_384 length:sizeof(signingKey_384)];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccessGroup: @"com.apple.security.octagon",
                                    (id)kSecAttrDescription: [NSString stringWithFormat:@"Octagon Account State (%@,%@)", containerName, contextID],
                                    (id)kSecAttrServer: [NSString stringWithFormat:@"octagon-%@", containerName],
                                    (id)kSecAttrAccount: [NSString stringWithFormat:@"octagon-%@", containerName], // Really should be alt-DSID, no?
                                    (id)kSecAttrPath: [NSString stringWithFormat:@"octagon-%@", contextID],
                                    (id)kSecAttrIsInvisible: @YES,
                                    (id)kSecValueData : signingFromBytes,
                                    (id)kSecAttrSynchronizable : @NO,
                                    (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
                                    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, &result);

    NSError* localerror = nil;

    // Did SecItemAdd fall over due to an existing item?
    if(status == errSecDuplicateItem) {
        // Add every primary key attribute to this find dictionary
        NSMutableDictionary* findQuery = [[NSMutableDictionary alloc] init];
        findQuery[(id)kSecClass]              = query[(id)kSecClass];
        findQuery[(id)kSecAttrSynchronizable] = query[(id)kSecAttrSynchronizable];
        findQuery[(id)kSecAttrSyncViewHint]   = query[(id)kSecAttrSyncViewHint];
        findQuery[(id)kSecAttrAccessGroup]    = query[(id)kSecAttrAccessGroup];
        findQuery[(id)kSecAttrAccount]        = query[(id)kSecAttrAccount];
        findQuery[(id)kSecAttrServer]         = query[(id)kSecAttrServer];
        findQuery[(id)kSecAttrPath]           = query[(id)kSecAttrPath];
        findQuery[(id)kSecUseDataProtectionKeychain] = query[(id)kSecUseDataProtectionKeychain];

        NSMutableDictionary* updateQuery = [query mutableCopy];
        updateQuery[(id)kSecClass] = nil;

        status = SecItemUpdate((__bridge CFDictionaryRef)findQuery, (__bridge CFDictionaryRef)updateQuery);

        if(status) {
            localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                             code:status
                                      description:[NSString stringWithFormat:@"SecItemUpdate: %d", (int)status]];
        }
    } else if(status != 0) {
        localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                  description: [NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];
    }

    if(localerror) {
        if(error) {
            *error = localerror;
        }
        return false;
    } else {
        return true;
    }
}

+ (NSData* _Nullable)copyInitialSyncData:(SOSInitialSyncFlags)flags error:(NSError**)error
{
    CFErrorRef cferror = NULL;
    NSData* result = CFBridgingRelease(SOSCCCopyInitialSyncData_Server(flags, &cferror));

    if(cferror && error) {
        *error = CFBridgingRelease(cferror);
    }

    return result;
}

+ (NSDictionary* _Nullable)copyPiggybackingInitialSyncData:(NSData*)data
{
    const uint8_t* der = [data bytes];
    const uint8_t *der_end = der + [data length];

    NSDictionary* results = SOSPiggyCopyInitialSyncData(&der, der_end);
    return results;
}

+ (NSData* _Nullable)keychainPersistentRefForKey:(SecKeyRef)key error:(NSError**)error
{
    NSDictionary* query = @{
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecValueRef : (__bridge id)key,
        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
    };

    CFTypeRef foundRef = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &foundRef);

    if (status == errSecSuccess && CFGetTypeID(foundRef) == CFDataGetTypeID()) {
        return (NSData*)CFBridgingRelease(foundRef);

    } else {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                     userInfo:nil];
        }
        return nil;
    }
}

+ (BOOL)testSecKey:(CKKSSelves*)octagonSelf error:(NSError**)error
{
    id<CKKSSelfPeer> currentSelfPeer = octagonSelf.currentSelf;

    NSData* signingFullKey = currentSelfPeer.signingKey.keyData;

    SecKeyRef octagonSigningPubSecKey = CFRetainSafe(currentSelfPeer.publicSigningKey.secKey);
    SecKeyRef octagonEncryptionPubSecKey = CFRetainSafe(currentSelfPeer.publicEncryptionKey.secKey);

    NSError* localerror = nil;

    bool savedSigningKey = SOSCCSaveOctagonKeysToKeychain(@"Octagon Peer Signing ID for Test-ak",
                                                          signingFullKey,
                                                          384,
                                                          octagonSigningPubSecKey,
                                                          &localerror);
    if(!savedSigningKey) {
        if(error) {
            *error = localerror;
        }
        CFReleaseNull(octagonSigningPubSecKey);
        CFReleaseNull(octagonEncryptionPubSecKey);
        return NO;
    }

    // Okay, can we load this key pair?

    // Try the SPI route first
    CFErrorRef cferror = NULL;
    SecKeyRef signingPrivateKey = SecKeyCopyMatchingPrivateKey(octagonSigningPubSecKey, &cferror);
    if(!signingPrivateKey) {
        if(error) {
            *error = CFBridgingRelease(cferror);
        } else {
            CFReleaseNull(cferror);
        }
        CFReleaseNull(octagonSigningPubSecKey);
        CFReleaseNull(octagonEncryptionPubSecKey);
        return NO;
    }

    // and can you get the persistent ref from that private key?
    NSData* signingPrivatePref = [self keychainPersistentRefForKey:signingPrivateKey error:error];
    if(signingPrivatePref == nil) {
        CFReleaseNull(octagonSigningPubSecKey);
        CFReleaseNull(octagonEncryptionPubSecKey);
        return NO;
    }

    SFECKeyPair *signingFullKeyPair = [[SFECKeyPair alloc] initWithData:signingFullKey
                                                              specifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]
                                                                  error:&localerror];
    if(!signingFullKey) {
        if(error) {
            *error = localerror;
        }
        CFReleaseNull(octagonSigningPubSecKey);
        CFReleaseNull(octagonEncryptionPubSecKey);
        return NO;
    }

    NSData* signingFullPairPref = [self keychainPersistentRefForKey:signingFullKeyPair.secKey error:error];
    if(signingFullPairPref == nil) {
        CFReleaseNull(octagonSigningPubSecKey);
        CFReleaseNull(octagonEncryptionPubSecKey);
        return NO;
    }

    CFReleaseNull(octagonSigningPubSecKey);
    CFReleaseNull(octagonEncryptionPubSecKey);

    return YES;
}

#pragma clang diagnostic pop

+ (BOOL)addNRandomKeychainItemsWithoutUpgradedPersistentRefs:(int64_t)number
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
    
    NSDictionary* addQuery = nil;
    CFTypeRef result = NULL;
    
    for(int i = 0; i< number; i++) {
        addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                      (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                      (id)kSecAttrAccount : [NSString stringWithFormat:@"testKeychainItemUpgradePhase%dAccount%d", i, i],
                      (id)kSecAttrService : @"TestUUIDPersistentRefService",
                      (id)kSecUseDataProtectionKeychain : @(YES),
                      (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                      (id)kSecReturnAttributes : @(YES),
                      (id)kSecReturnPersistentRef : @(YES)
        };
        
        result = NULL;
        SecItemAdd((__bridge CFDictionaryRef)addQuery, &result);
    }
    
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);
    return YES;
}

+ (BOOL)expectXNumberOfItemsUpgraded:(int)expected
{
    int64_t upgraded = 0;
    
    NSDictionary *query = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                             (id)kSecUseDataProtectionKeychain : @(YES),
                             (id)kSecReturnAttributes : @(YES),
                             (id)kSecReturnPersistentRef : @(YES),
                             (id)kSecMatchLimit : (id)kSecMatchLimitAll,
    };
    
    CFTypeRef items = NULL;
    SecItemCopyMatching((__bridge CFDictionaryRef)query, &items);
    
    for (NSDictionary *item in (__bridge NSArray*)items) {
        NSData* pref = item[(id)kSecValuePersistentRef];
        if ([pref length] == 20) {
            upgraded+=1;
        }
    }
    
    return (upgraded == expected);
}

+ (BOOL)checkAllPersistentRefBeenUpgraded
{
    BOOL allUpgraded = YES;
    
    NSDictionary *query = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                             (id)kSecUseDataProtectionKeychain : @(YES),
                             (id)kSecReturnAttributes : @(YES),
                             (id)kSecReturnPersistentRef : @(YES),
                             (id)kSecMatchLimit : (id)kSecMatchLimitAll,
    };
    
    CFTypeRef items = NULL;
    SecItemCopyMatching((__bridge CFDictionaryRef)query, &items);
    
    for (NSDictionary *item in (__bridge NSArray*)items) {
        NSData* pref = item[(id)kSecValuePersistentRef];
        if ([pref length] == 20) {
            allUpgraded &= YES;
        } else {
            allUpgraded &= NO;
        }
    }
    
    return allUpgraded;
}

+ (NSNumber* _Nullable)lastRowID
{
    return (__bridge NSNumber*) lastRowIDHandledForTests();
}

+ (void)setError:(int)errorCode
{
    NSString* descriptionString = [NSString stringWithFormat:@"Fake error %d for testing", errorCode];
    CFErrorRef error = (__bridge CFErrorRef)[NSError errorWithDomain:(id)kSecErrorDomain code:errorCode userInfo:@{NSLocalizedDescriptionKey : descriptionString}];
    setExpectedErrorForTests(error);
}

+ (void)clearError
{
    clearTestError();
}

+ (void)clearLastRowID
{
    clearLastRowIDHandledForTests();
}

+ (void)clearErrorInsertionDictionary
{
    clearRowIDAndErrorDictionary();
}

+ (void)setErrorAtRowID:(int)errorCode
{
    CFMutableDictionaryRef rowIDToErrorDictionary = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    NSString* descriptionString = [NSString stringWithFormat:@"Fake error %d for testing", errorCode];

    CFErrorRef error = (__bridge CFErrorRef)[NSError errorWithDomain:(id)kSecErrorDomain code:errorCode userInfo:@{NSLocalizedDescriptionKey : descriptionString}];
    CFNumberRef rowID = CFBridgingRetain([[NSNumber alloc]initWithInt:150]);
    CFDictionaryAddValue(rowIDToErrorDictionary, rowID, error);
    
    setRowIDToErrorDictionary(rowIDToErrorDictionary);

    CFReleaseNull(rowID);
}

static int invocationCount = 0;

+ (int)getInvocationCount
{
    return invocationCount;
}

+ (void)clearInvocationCount
{
    invocationCount = 0;
}

+ (NSArray*)testAA_AppleAccountsWithInvalidationError:(NSError* __autoreleasing *)error
{
    invocationCount++;
    if (error) {
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSXPCConnectionInterrupted description:@"test xpc connection interrupted error"];
    }
    return nil;
}

+ (void)setACAccountStoreWithInvalidationError:(id<OTAccountsAdapter>)adapter
{
    id mockStore = OCMClassMock([ACAccountStore class]);
    OCMStub([mockStore aa_appleAccountsWithError:[OCMArg anyObjectRef]]).andCall(self, @selector(testAA_AppleAccountsWithInvalidationError:));
    [adapter setAccountStore:mockStore];
}

+ (NSArray*)testAA_AppleAccountsWithRandomError:(NSError* __autoreleasing *)error
{
    invocationCount++;
    if (error) {
        *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoNetwork description:@"test random error"];
    }
    return nil;
}

+ (void)setACAccountStoreWithRandomError:(id<OTAccountsAdapter>)adapter
{
    id mockStore = OCMClassMock([ACAccountStore class]);
    OCMStub([mockStore aa_appleAccountsWithError:[OCMArg anyObjectRef]]).andCall(self, @selector(testAA_AppleAccountsWithRandomError:));
    [adapter setAccountStore:mockStore];
}

+ (BOOL)isPlatformHomepod
{
    return (MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid) == MGDeviceClassAudioAccessory);
}


@end

@interface OctagonTrustCliqueBridge ()
@property OTClique* clique;
@end
@implementation OctagonTrustCliqueBridge
- (instancetype)initWithClique:(OTClique*)clique
{
    if((self = [super init])) {
        _clique = clique;
    }
    return self;
}

- (BOOL)setLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                error:(NSError**)error
{
    return [self.clique setLocalSecureElementIdentity:secureElementIdentity
                                                error:error];
}

- (BOOL)removeLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                         error:(NSError**)error
{
    return [self.clique removeLocalSecureElementIdentityPeerID:sePeerID
                                                         error:error];
}


- (OTCurrentSecureElementIdentities* _Nullable)fetchTrustedSecureElementIdentities:(NSError**)error
{
    return [self.clique fetchTrustedSecureElementIdentities:error];
}

- (BOOL)setAccountSetting:(OTAccountSettings*)setting error:(NSError**)error
{
    return [self.clique setAccountSetting:setting error:error];
}

- (OTAccountSettings* _Nullable)fetchAccountSettings:(NSError**)error
{
    return [self.clique fetchAccountSettings:error];
}

+ (OTAccountSettings* _Nullable)fetchAccountWideSettings:(OTConfigurationContext*)context error:(NSError**)error
{
    return [OTClique fetchAccountWideSettings:context error:error];
}

+ (OTAccountSettings* _Nullable)fetchAccountWideSettingsWithForceFetch:(bool)forceFetch
                                                         configuration:(OTConfigurationContext*)context
                                                                 error:(NSError**)error
{
    return [OTClique fetchAccountWideSettingsWithForceFetch:forceFetch configuration:context error:error];
}

+ (OTAccountSettings* _Nullable)fetchAccountWideSettingsDefaultWithForceFetch:(bool)forceFetch
                                                                configuration:(OTConfigurationContext*)context
                                                                        error:(NSError**)error
{
    return [OTClique fetchAccountWideSettingsDefaultWithForceFetch:forceFetch configuration:context error:error];
}

- (BOOL)waitForPriorityViewKeychainDataRecovery:(NSError**)error
{
    return [self.clique waitForPriorityViewKeychainDataRecovery:error];
}

- (NSString*)createAndSetRecoveryKeyWithContext:(OTConfigurationContext*)context error:(NSError**)error
{
    return [OTClique createAndSetRecoveryKeyWithContext:context error:error];
}

- (BOOL)registerRecoveryKeyWithContext:(OTConfigurationContext*)context recoveryKey:(NSString*)recoveryKey error:(NSError**)error
{
    return [OTClique registerRecoveryKeyWithContext:context recoveryKey:recoveryKey error:error];
}

+ (BOOL)isRecoveryKeySet:(OTConfigurationContext*)ctx error:(NSError**)error
{
    return [OTClique isRecoveryKeySet:ctx error:error];
}

+ (BOOL)recoverWithRecoveryKey:(OTConfigurationContext*)ctx
                   recoveryKey:(NSString*)recoveryKey
                         error:(NSError**)error
{
    return [OTClique recoverWithRecoveryKey:ctx recoveryKey:recoveryKey error:error];
}

- (BOOL)removeRecoveryKeyWithContext:(OTConfigurationContext*)context error:(NSError**)error
{
    return [self.clique removeRecoveryKey:context error:error];
}

- (bool)preflightRecoveryKey:(OTConfigurationContext*)context recoveryKey:(NSString*)recoveryKey error:(NSError**)error
{
    return [OTClique preflightRecoverOctagonUsingRecoveryKey:context recoveryKey:recoveryKey error:error] ? true : false;
}

+ (bool)setRecoveryKeyWithContext:(OTConfigurationContext*)ctx
                      recoveryKey:(NSString*)recoveryKey
                            error:(NSError**)error
{
    return [OTClique setRecoveryKeyWithContext:ctx recoveryKey:recoveryKey error:error];
}

+ (NSNumber * _Nullable)totalTrustedPeers:(OTConfigurationContext*)ctx error:(NSError * __autoreleasing *)error
{
    return [OTClique totalTrustedPeers:ctx error:error];
}

+ (BOOL)areRecoveryKeysDistrusted:(OTConfigurationContext*)ctx error:(NSError* __autoreleasing *)error __attribute__((swift_error(nonnull_error)))
{
    return [OTClique areRecoveryKeysDistrusted:ctx error:error];
}

+ (void)mockEpochXPCErrorWithArguments:(OTControlArguments*)arguments
                         configuration:(OTJoiningConfiguration*)config
                                 reply:(void (^)(uint64_t epoch,
                                                 NSError * _Nullable error))reply
{
    invocationCount++;
    reply(0, [NSError errorWithDomain:NSCocoaErrorDomain code:NSXPCConnectionInterrupted description:@"test xpc connection interrupted error"]);
}

+ (void)mockTrustStatusWithArguments:(OTControlArguments*)arguments
                     configuration:(OTJoiningConfiguration*)config
                               reply:(void (^)(CliqueStatus status,
                                               NSString * _Nullable peerID,
                                               NSNumber * _Nullable numberOfOctagonPeers,
                                               BOOL isExcluded,
                                               NSError * _Nullable retError))reply
{
    reply(CliqueStatusIn, nil, nil, NO, nil);
}
 
+ (OTControl*)makeMockOTControlObjectWithFailingEpochFetchWithXPCError
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcEpochWithArguments:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockEpochXPCErrorWithArguments:configuration:reply:));
    OCMStub([mockOTControl fetchTrustStatus:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockTrustStatusWithArguments:configuration:reply:));

    return mockOTControl;
}

+ (void)mockEpochRandomErrorWithArguments:(OTControlArguments*)arguments
                            configuration:(OTJoiningConfiguration*)config
                                    reply:(void (^)(uint64_t epoch,
                                                    NSError * _Nullable error))reply
{
    invocationCount++;
    reply(0, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoIdentity description:@"test no identity error"]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingEpochFetchWithRandomError
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcEpochWithArguments:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockEpochRandomErrorWithArguments:configuration:reply:));
    OCMStub([mockOTControl fetchTrustStatus:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockTrustStatusWithArguments:configuration:reply:));

    return mockOTControl;
}

+ (void)mockVoucherWithArguments:(OTControlArguments*)arguments
                   configuration:(OTJoiningConfiguration*)config
                          peerID:(NSString*)peerID
                   permanentInfo:(NSData *)permanentInfo
                permanentInfoSig:(NSData *)permanentInfoSig
                      stableInfo:(NSData *)stableInfo
                   stableInfoSig:(NSData *)stableInfoSig
                   maxCapability:(NSString*)maxCapability
                           reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    invocationCount++;
    reply(nil, nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSXPCConnectionInterrupted description:@"test xpc connection interrupted error"]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingVoucherFetchWithXPCError
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcVoucherWithArguments:[OCMArg any]
                                     configuration:[OCMArg any]
                                            peerID:[OCMArg any]
                                     permanentInfo:[OCMArg any]
                                  permanentInfoSig:[OCMArg any]
                                        stableInfo:[OCMArg any]
                                     stableInfoSig:[OCMArg any]
                                     maxCapability:[OCMArg any]
                                             reply:[OCMArg any]]).andCall(self, @selector(mockVoucherWithArguments:configuration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:maxCapability:reply:));
    OCMStub([mockOTControl fetchTrustStatus:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockTrustStatusWithArguments:configuration:reply:));

    return mockOTControl;
}

+ (void)mockVoucherErrorNetworkFailureWithArguments:(OTControlArguments*)arguments
                                      configuration:(OTJoiningConfiguration*)config
                                             peerID:(NSString*)peerID
                                      permanentInfo:(NSData *)permanentInfo
                                   permanentInfoSig:(NSData *)permanentInfoSig
                                         stableInfo:(NSData *)stableInfo
                                      stableInfoSig:(NSData *)stableInfoSig
                                      maxCapability:(NSString*)maxCapability
                                              reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    invocationCount++;
    reply(nil, nil, [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorTimedOut description:@"The request timed out."]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingVoucherFetchWithNetworkError
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcVoucherWithArguments:[OCMArg any]
                                     configuration:[OCMArg any]
                                            peerID:[OCMArg any]
                                     permanentInfo:[OCMArg any]
                                  permanentInfoSig:[OCMArg any]
                                        stableInfo:[OCMArg any]
                                     stableInfoSig:[OCMArg any]
                                     maxCapability:[OCMArg any]
                                             reply:[OCMArg any]]).andCall(self, @selector(mockVoucherErrorNetworkFailureWithArguments:configuration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:maxCapability:reply:));
    
    OCMStub([mockOTControl fetchTrustStatus:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockTrustStatusWithArguments:configuration:reply:));

    return mockOTControl;
}

+ (void)mockVoucherErrorUnderlyingNetworkFailureWithArguments:(OTControlArguments*)arguments
                                                configuration:(OTJoiningConfiguration*)config
                                                       peerID:(NSString*)peerID
                                                permanentInfo:(NSData *)permanentInfo
                                             permanentInfoSig:(NSData *)permanentInfoSig
                                                   stableInfo:(NSData *)stableInfo
                                                stableInfoSig:(NSData *)stableInfoSig
                                                maxCapability:(NSString*)maxCapability
                                                        reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    invocationCount++;
    NSError* networkError = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorTimedOut description:@"The request timed out."];
    NSError* ckError = [NSError errorWithDomain:CKErrorDomain code:CKErrorNetworkFailure description:@"NSURLErrorDomain: -1001" underlying:networkError];

    reply(nil, nil, ckError);
}

+ (OTControl*)makeMockOTControlObjectWithFailingVoucherFetchWithUnderlyingNetworkError
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcVoucherWithArguments:[OCMArg any]
                                     configuration:[OCMArg any]
                                            peerID:[OCMArg any]
                                     permanentInfo:[OCMArg any]
                                  permanentInfoSig:[OCMArg any]
                                        stableInfo:[OCMArg any]
                                     stableInfoSig:[OCMArg any]
                                     maxCapability:[OCMArg any]
                                             reply:[OCMArg any]]).andCall(self, @selector(mockVoucherErrorUnderlyingNetworkFailureWithArguments:configuration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:maxCapability:reply:));
    return mockOTControl;
}

+ (void)mockVoucherErrorUnderlyingNetworkConnectionLostWithArguments:(OTControlArguments*)arguments
                                                       configuration:(OTJoiningConfiguration*)config
                                                              peerID:(NSString*)peerID
                                                       permanentInfo:(NSData *)permanentInfo
                                                    permanentInfoSig:(NSData *)permanentInfoSig
                                                          stableInfo:(NSData *)stableInfo
                                                       stableInfoSig:(NSData *)stableInfoSig
                                                       maxCapability:(NSString*)maxCapability
                                                               reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    invocationCount++;
    NSError* networkError = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorNetworkConnectionLost description:@"The network connection was lost."];
    NSError* ckError = [NSError errorWithDomain:CKErrorDomain code:CKErrorNetworkFailure description:@"NSURLErrorDomain: -1005" underlying:networkError];

    reply(nil, nil, ckError);
}

+ (OTControl*)makeMockOTControlObjectWithFailingVoucherFetchWithUnderlyingNetworkErrorConnectionLost
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcVoucherWithArguments:[OCMArg any]
                                     configuration:[OCMArg any]
                                            peerID:[OCMArg any]
                                     permanentInfo:[OCMArg any]
                                  permanentInfoSig:[OCMArg any]
                                        stableInfo:[OCMArg any]
                                     stableInfoSig:[OCMArg any]
                                     maxCapability:[OCMArg any]
                                             reply:[OCMArg any]]).andCall(self, @selector(mockVoucherErrorUnderlyingNetworkConnectionLostWithArguments:configuration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:maxCapability:reply:));
    return mockOTControl;
}


+ (void)mockVoucherWithRandomErrorWithArguments:(OTControlArguments*)arguments
                                  configuration:(OTJoiningConfiguration*)config
                                         peerID:(NSString*)peerID
                                  permanentInfo:(NSData *)permanentInfo
                               permanentInfoSig:(NSData *)permanentInfoSig
                                     stableInfo:(NSData *)stableInfo
                                  stableInfoSig:(NSData *)stableInfoSig
                                  maxCapability:(NSString*)maxCapability
                                          reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    invocationCount++;
    reply(nil, nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNotSignedIn description:@"test not signed in error"]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingVoucherFetchWithRandomError {
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcVoucherWithArguments:[OCMArg any]
                                     configuration:[OCMArg any]
                                            peerID:[OCMArg any]
                                     permanentInfo:[OCMArg any]
                                  permanentInfoSig:[OCMArg any]
                                        stableInfo:[OCMArg any]
                                     stableInfoSig:[OCMArg any]
                                     maxCapability:[OCMArg any]
                                             reply:[OCMArg any]]).andCall(self, @selector(mockVoucherWithRandomErrorWithArguments:configuration:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:maxCapability:reply:));
    
    OCMStub([mockOTControl fetchTrustStatus:[OCMArg any] configuration:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockTrustStatusWithArguments:configuration:reply:));

    return mockOTControl;
}

+ (void)mockPrepareIdentityAsApplicantWithRandomErrorArguments:(OTControlArguments*)arguments
                                                 configuration:(OTJoiningConfiguration*)config
                                                         reply:(void (^)(NSString * _Nullable peerID,
                                                                         NSData * _Nullable permanentInfo,
                                                                         NSData * _Nullable permanentInfoSig,
                                                                         NSData * _Nullable stableInfo,
                                                                         NSData * _Nullable stableInfoSig,
                                                                         NSError * _Nullable error))reply
{
    invocationCount++;
    reply(nil, nil, nil, nil, nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNotSignedIn description:@"test not signed in error"]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingPrepareFetchWithRandomError {
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcPrepareIdentityAsApplicantWithArguments:[OCMArg any]
                                                        configuration:[OCMArg any]
                                                                reply:[OCMArg any]]).andCall(self, @selector(mockPrepareIdentityAsApplicantWithRandomErrorArguments:configuration:reply:));
    return mockOTControl;
}

+ (void)mockRPCPrepareIdentityAsApplicantWithXPCErrorArguments:(OTControlArguments*)arguments
                                                 configuration:(OTJoiningConfiguration*)config
                                                         reply:(void (^)(NSString * _Nullable peerID,
                                                                         NSData * _Nullable permanentInfo,
                                                                         NSData * _Nullable permanentInfoSig,
                                                                         NSData * _Nullable stableInfo,
                                                                         NSData * _Nullable stableInfoSig,
                                                                         NSError * _Nullable error))reply
{
    invocationCount++;
    reply(nil, nil, nil, nil, nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSXPCConnectionInterrupted description:@"test xpc connection interrupted error"]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingPrepareFetchWithXPCError {
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcPrepareIdentityAsApplicantWithArguments:[OCMArg any]
                                                        configuration:[OCMArg any]
                                                                reply:[OCMArg any]]).andCall(self, @selector(mockRPCPrepareIdentityAsApplicantWithXPCErrorArguments:configuration:reply:));
    return mockOTControl;
}

+ (void)mockRPCPrepareIdentityAsApplicantWithOctagonErrorICloudAccountStateUnknownArguments:(OTControlArguments*)arguments
                                                                              configuration:(OTJoiningConfiguration*)config
                                                                                      reply:(void (^)(NSString * _Nullable peerID,
                                                                                                      NSData * _Nullable permanentInfo,
                                                                                                      NSData * _Nullable permanentInfoSig,
                                                                                                      NSData * _Nullable stableInfo,
                                                                                                      NSData * _Nullable stableInfoSig,
                                                                                                      NSError * _Nullable error))reply
{
    invocationCount++;
    reply(nil, nil, nil, nil, nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorICloudAccountStateUnknown description:@"test OctagonErrorICloudAccountStateUnknown error"]);
}

+ (OTControl*)makeMockOTControlObjectWithFailingPrepareFetchWithOctagonErrorICloudAccountStateUnknown {
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcPrepareIdentityAsApplicantWithArguments:[OCMArg any]
                                                        configuration:[OCMArg any]
                                                                reply:[OCMArg any]]).andCall(self, @selector(mockRPCPrepareIdentityAsApplicantWithOctagonErrorICloudAccountStateUnknownArguments:configuration:reply:));
    return mockOTControl;
}

+ (void)mockJoinWithXPCErrorWithArguments:(OTControlArguments*)arguments
                            configuration:(OTJoiningConfiguration*)config
                                vouchData:(NSData*)vouchData
                                 vouchSig:(NSData*)vouchSig
                                    reply:(void (^)(NSError * _Nullable error))reply
{
    invocationCount++;
    reply([NSError errorWithDomain:NSCocoaErrorDomain code:NSXPCConnectionInterrupted description:@"test xpc connection interrupted error"]);
}


+ (OTControl*)makeMockOTControlObjectWithFailingJoinWithXPCError {
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcJoinWithArguments:[OCMArg any]
                                  configuration:[OCMArg any]
                                      vouchData:[OCMArg any]
                                       vouchSig:[OCMArg any]
                                          reply:[OCMArg any]]).andCall(self, @selector(mockJoinWithXPCErrorWithArguments:configuration:vouchData:vouchSig:reply:));
    return mockOTControl;
}

+ (void)mockJoinWithRandomErrorArguments:(OTControlArguments*)arguments
                           configuration:(OTJoiningConfiguration*)config
                               vouchData:(NSData*)vouchData
                                vouchSig:(NSData*)vouchSig
                                   reply:(void (^)(NSError * _Nullable error))reply
{
    invocationCount++;
    reply([NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNotInSOS description:@"test not in SOS error"]);
}


+ (OTControl*)makeMockOTControlObjectWithFailingJoinWithRandomError {
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl rpcJoinWithArguments:[OCMArg any]
                                  configuration:[OCMArg any]
                                      vouchData:[OCMArg any]
                                       vouchSig:[OCMArg any]
                                          reply:[OCMArg any]]).andCall(self, @selector(mockJoinWithRandomErrorArguments:configuration:vouchData:vouchSig:reply:));
    return mockOTControl;
}

+ (void)mockFetchEgoPeerIDWithArguments:(OTControlArguments*)arguments
                                  reply:(void (^)(NSString* peerID, NSError * _Nullable error))reply
{
    reply(nil, [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInteractionNotAllowed description:@"device is locked"]);
}

+ (OTControl*)makeMockFetchEgoPeerID
{
    id mockOTControl = OCMClassMock([OTControl class]);
    OCMStub([mockOTControl fetchEgoPeerID:[OCMArg any] reply:[OCMArg any]]).andCall(self, @selector(mockFetchEgoPeerIDWithArguments:reply:));

    return mockOTControl;
}

@end

