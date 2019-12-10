/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 */

/*
 * SOSAccount.c -  Implementation of the secure object syncing account.
 * An account contains a SOSCircle for each protection domain synced.
 */

#import <Foundation/Foundation.h>

#import "keychain/SecureObjectSync/SOSAccount.h"
#import <Security/SecureObjectSync/SOSPeerInfo.h>
#import "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#import "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#import "keychain/SecureObjectSync/SOSTransportCircle.h"
#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#import "keychain/SecureObjectSync/SOSTransportMessage.h"
#import "keychain/SecureObjectSync/SOSTransportMessageKVS.h"
#import "keychain/SecureObjectSync/SOSTransportKeyParameter.h"
#import "keychain/SecureObjectSync/SOSKVSKeys.h"
#import "keychain/SecureObjectSync/SOSTransport.h"
#import "keychain/SecureObjectSync/SOSPeerCoder.h"
#import "keychain/SecureObjectSync/SOSInternal.h"
#import "keychain/SecureObjectSync/SOSRing.h"
#import "keychain/SecureObjectSync/SOSRingUtils.h"
#import "keychain/SecureObjectSync/SOSRingRecovery.h"
#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#import "keychain/SecureObjectSync/SOSAccountGhost.h"
#import "keychain/SecureObjectSync/SOSPiggyback.h"
#import "keychain/SecureObjectSync/SOSControlHelper.h"
#import "keychain/SecureObjectSync/SOSAuthKitHelpers.h"

#import "keychain/SecureObjectSync/SOSAccountTrust.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Identity.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Retirement.h"
#import "keychain/SecureObjectSync/SOSPeerOTRTimer.h"
#import "keychain/SecureObjectSync/SOSPeerRateLimiter.h"
#import "keychain/SecureObjectSync/SOSTypes.h"
#if OCTAGON
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSPBFileStorage.h"

#import "keychain/ot/OTManager.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#endif
#include <Security/SecItemInternal.h>
#include <Security/SecEntitlements.h>
#include "keychain/securityd/SecItemServer.h"

#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"
#include "keychain/SecureObjectSync/generated_source/SOSAccountConfiguration.h"


#import "SecdWatchdog.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecADWrapper.h>
#include <utilities/SecFileLocations.h>

#include <os/activity.h>
#include <os/state_private.h>

#include <utilities/SecCoreCrypto.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>


const CFStringRef kSOSAccountName = CFSTR("AccountName");
const CFStringRef kSOSEscrowRecord = CFSTR("EscrowRecord");
const CFStringRef kSOSUnsyncedViewsKey = CFSTR("unsynced");
const CFStringRef kSOSInitialSyncTimeoutV0 = CFSTR("initialsynctimeout");
const CFStringRef kSOSPendingEnableViewsToBeSetKey = CFSTR("pendingEnableViews");
const CFStringRef kSOSPendingDisableViewsToBeSetKey = CFSTR("pendingDisableViews");
const CFStringRef kSOSTestV2Settings = CFSTR("v2dictionary");
const CFStringRef kSOSRecoveryKey = CFSTR("RecoveryKey");
const CFStringRef kSOSRecoveryRing = CFSTR("RecoveryRing");
const CFStringRef kSOSAccountUUID = CFSTR("UUID");
const CFStringRef kSOSRateLimitingCounters = CFSTR("RateLimitCounters");
const CFStringRef kSOSAccountPeerNegotiationTimeouts = CFSTR("PeerNegotiationTimeouts"); //Dictionary<CFStringRef, CFNumberRef>
const CFStringRef kSOSAccountPeerLastSentTimestamp = CFSTR("kSOSAccountPeerLastSentTimestamp"); //Dictionary<CFStringRef, CFDateRef>
const CFStringRef kSOSAccountRenegotiationRetryCount = CFSTR("NegotiationRetryCount");
const CFStringRef kOTRConfigVersion = CFSTR("OTRConfigVersion");
NSString* const SecSOSAggdReattemptOTRNegotiation   = @"com.apple.security.sos.otrretry";
NSString* const SOSAccountUserDefaultsSuite = @"com.apple.security.sosaccount";
NSString* const SOSAccountLastKVSCleanup = @"lastKVSCleanup";

const uint64_t max_packet_size_over_idms = 500;


#define kPublicKeyNotAvailable "com.apple.security.publickeynotavailable"

#define DATE_LENGTH 25
const CFStringRef kSOSAccountDebugScope = CFSTR("Scope");

#if OCTAGON
static NSDictionary<OctagonState*, NSNumber*>* SOSStateMap(void);

@interface SOSAccount () <OctagonStateMachineEngine>
@property dispatch_queue_t stateMachineQueue;
@property (readwrite) OctagonStateMachine* stateMachine;

@property (readwrite) CKKSPBFileStorage<SOSAccountConfiguration*>* accountConfiguration;

@property CKKSNearFutureScheduler *performBackups;
@property CKKSNearFutureScheduler *performRingUpdates;
@end
#endif

@implementation SOSAccount

- (void)dealloc {
    if(self) {
        CFReleaseNull(self->_accountKey);
        CFReleaseNull(self->_accountPrivateKey);
        CFReleaseNull(self->_previousAccountKey);
#if OCTAGON
        [self.performBackups cancel];
        [self.performRingUpdates cancel];
        [self.stateMachine haltOperation];
#endif
    }
}

@synthesize accountKey = _accountKey;

- (void) setAccountKey: (SecKeyRef) key {
    CFRetainAssign(self->_accountKey, key);
}

@synthesize previousAccountKey = _previousAccountKey;

- (void) setPreviousAccountKey: (SecKeyRef) key {
    CFRetainAssign(self->_previousAccountKey, key);
}

@synthesize accountPrivateKey = _accountPrivateKey;

- (void) setAccountPrivateKey: (SecKeyRef) key {
    CFRetainAssign(self->_accountPrivateKey, key);
}

// Syntactic sugar getters

- (BOOL) hasPeerInfo {
    return self.fullPeerInfo != nil;
}

- (SOSPeerInfoRef) peerInfo {
    return self.trust.peerInfo;
}

- (SOSFullPeerInfoRef) fullPeerInfo {
    return self.trust.fullPeerInfo;
}

- (NSString*) peerID {
    return self.trust.peerID;
}

-(bool) ensureFactoryCircles
{
    if (self.factory == nil){
        return false;
    }

    NSString* circle_name = CFBridgingRelease(SOSDataSourceFactoryCopyName(self.factory));
    if (!circle_name){
        return false;
    }

    CFReleaseSafe(SOSAccountEnsureCircle(self, (__bridge CFStringRef) circle_name, NULL));

    return SOSAccountInflateTransports(self, (__bridge CFStringRef) circle_name, NULL);
}

-(void)ensureOctagonPeerKeys
{
#if OCTAGON
    CKKSLockStateTracker *tracker = [CKKSViewManager manager].lockStateTracker;
    if (tracker && tracker.isLocked == false) {
        [self.trust ensureOctagonPeerKeys:self.circle_transport];
    }
#endif
}

-(id) initWithGestalt:(CFDictionaryRef)newGestalt factory:(SOSDataSourceFactoryRef)f
{
    self = [super init];
    if(self){
        self.queue = dispatch_queue_create("Account Queue", DISPATCH_QUEUE_SERIAL);

        self.gestalt = [[NSDictionary alloc] initWithDictionary:(__bridge NSDictionary * _Nonnull)(newGestalt)];

        SOSAccountTrustClassic *t = [[SOSAccountTrustClassic alloc] initWithRetirees:[NSMutableSet set] fpi:NULL circle:NULL departureCode:kSOSDepartureReasonError peerExpansion:[NSMutableDictionary dictionary]];
        
        self.trust = t;
        self.factory = f; // We adopt the factory. kthanksbai.

        self.isListeningForSync = false;

        self.accountPrivateKey = NULL;
        self._password_tmp = NULL;
        self.user_private_timer = NULL;
        self.lock_notification_token = NOTIFY_TOKEN_INVALID;

        self.change_blocks = [NSMutableArray array];

        self.key_transport = nil;
        self.circle_transport = NULL;
        self.ck_storage = nil;
        self.kvs_message_transport = nil;
        
        self.circle_rings_retirements_need_attention = false;
        self.engine_peer_state_needs_repair = false;
        self.key_interests_need_updating = false;
        
        self.backup_key =nil;
        self.deviceID = nil;

        self.waitForInitialSync_blocks = [NSMutableDictionary dictionary];
        self.accountKeyIsTrusted = false;
        self.accountKeyDerivationParamters = NULL;
        self.accountKey = NULL;
        self.previousAccountKey = NULL;

        self.saveBlock = nil;

        self.settings =  [[NSUserDefaults alloc] initWithSuiteName:SOSAccountUserDefaultsSuite];

        [self ensureFactoryCircles];
        SOSAccountEnsureUUID(self);

#if OCTAGON
        [self setupStateMachine];
#endif
    }
    return self;
}

- (void)startStateMachine
{
#if OCTAGON
    [self.stateMachine startOperation];
#endif
}

-(BOOL)isEqual:(id) object
{
    if(![object isKindOfClass:[SOSAccount class]])
        return NO;

    SOSAccount* left = object;
    return ([self.gestalt isEqual: left.gestalt] &&
            CFEqualSafe(self.trust.trustedCircle, left.trust.trustedCircle) &&
            [self.trust.expansion isEqual: left.trust.expansion] &&
            CFEqualSafe(self.trust.fullPeerInfo, left.trust.fullPeerInfo));

}

- (void)userPublicKey:(void ((^))(BOOL trusted, NSData *spki, NSError *error))reply
{
    dispatch_async(self.queue, ^{
        if (!self.accountKeyIsTrusted || self.accountKey == NULL) {
            NSDictionary *userinfo = @{
                (id)kCFErrorDescriptionKey : @"User public key not trusted",
            };
            reply(self.accountKeyIsTrusted, NULL, [NSError errorWithDomain:(__bridge NSString *)kSOSErrorDomain code:kSOSErrorPublicKeyAbsent userInfo:userinfo]);
            return;
        }

        NSData *data = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo(self.accountKey));
        if (data == NULL) {
            NSDictionary *userinfo = @{
                (id)kCFErrorDescriptionKey : @"User public not available",
            };
            reply(self.accountKeyIsTrusted, NULL, [NSError errorWithDomain:(__bridge NSString *)kSOSErrorDomain code:kSOSErrorPublicKeyAbsent userInfo:userinfo]);
            return;
        }
        reply(self.accountKeyIsTrusted, data, NULL);
    });
}

- (void)kvsPerformanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *))reply
{
    /* Need to collect performance counters from all subsystems, not just circle transport, don't have counters yet though */
    SOSCloudKeychainRequestPerfCounters(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef error)
    {
        reply((__bridge NSDictionary *)returnedValues);
    });
}

- (void)rateLimitingPerformanceCounters:(void(^)(NSDictionary <NSString *, NSString *> *))reply
{
    CFErrorRef error = NULL;
    CFDictionaryRef rateLimitingCounters = (CFDictionaryRef)SOSAccountGetValue(self, kSOSRateLimitingCounters, &error);
    reply((__bridge NSDictionary*)rateLimitingCounters ? (__bridge NSDictionary*)rateLimitingCounters : [NSDictionary dictionary]);
}

- (void)stashedCredentialPublicKey:(void(^)(NSData *, NSError *error))reply
{
    dispatch_async(self.queue, ^{
        CFErrorRef error = NULL;

        SecKeyRef user_private = SOSAccountCopyStashedUserPrivateKey(self, &error);
        if (user_private == NULL) {
            reply(NULL, (__bridge NSError *)error);
            CFReleaseNull(error);
            return;
        }

        NSData *publicKey = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo(user_private));
        CFReleaseNull(user_private);
        reply(publicKey, NULL);
    });
}

- (void)assertStashedAccountCredential:(void(^)(BOOL result, NSError *error))complete
{
    dispatch_async(self.queue, ^{
        CFErrorRef error = NULL;
        bool result = SOSAccountAssertStashedAccountCredential(self, &error);
        complete(result, (__bridge NSError *)error);
        CFReleaseNull(error);
    });
}

static bool SyncKVSAndWait(CFErrorRef *error) {
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    
    __block bool success = false;
    
    secnoticeq("fresh", "EFP calling SOSCloudKeychainSynchronizeAndWait");
    
    os_activity_initiate("CloudCircle EFRESH", OS_ACTIVITY_FLAG_DEFAULT, ^(void) {
        SOSCloudKeychainSynchronizeAndWait(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(__unused CFDictionaryRef returnedValues, CFErrorRef sync_error) {
            secnotice("fresh", "EFP returned, callback error: %@", sync_error);
            
            success = (sync_error == NULL);
            if (error) {
                CFRetainAssign(*error, sync_error);
            }
            
            dispatch_semaphore_signal(wait_for);
        });
        
        
        dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
        secnotice("fresh", "EFP complete: %s %@", success ? "success" : "failure", error ? *error : NULL);
    });
    
    return success;
}

static bool Flush(CFErrorRef *error) {
    __block bool success = false;
    
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    secnotice("flush", "Starting");
    
    SOSCloudKeychainFlush(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        success = (sync_error == NULL);
        if (error) {
            CFRetainAssign(*error, sync_error);
        }
        
        dispatch_semaphore_signal(wait_for);
    });
    
    dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    
    secnotice("flush", "Returned %s", success? "Success": "Failure");
    
    return success;
}

- (bool)syncWaitAndFlush:(CFErrorRef *)error
{
    secnotice("pairing", "sync and wait starting");

    if (!SyncKVSAndWait(error)) {
        secnotice("pairing", "failed sync and wait: %@", error ? *error : NULL);
        return false;
    }
    if (!Flush(error)) {
        secnotice("pairing", "failed flush: %@", error ? *error : NULL);
        return false;
    }
    secnotice("pairing", "finished sync and wait");
    return true;
}

- (void)validatedStashedAccountCredential:(void(^)(NSData *credential, NSError *error))complete
{
    CFErrorRef syncerror = NULL;

    if (![self syncWaitAndFlush:&syncerror]) {
        complete(NULL, (__bridge NSError *)syncerror);
        CFReleaseNull(syncerror);
        return;
    }

    dispatch_async(self.queue, ^{
        CFErrorRef error = NULL;
        SecKeyRef key = NULL;
        key = SOSAccountCopyStashedUserPrivateKey(self, &error);
        if (key == NULL) {
            secnotice("pairing", "no stashed credential");
            complete(NULL, (__bridge NSError *)error);
            CFReleaseNull(error);
            return;
        }

        SecKeyRef publicKey = SecKeyCopyPublicKey(key);
        if (publicKey) {
            secnotice("pairing", "returning stash credential: %@", publicKey);
            CFReleaseNull(publicKey);
        }

        NSData *keydata = CFBridgingRelease(SecKeyCopyExternalRepresentation(key, &error));
        CFReleaseNull(key);
        complete(keydata, (__bridge NSError *)error);
        CFReleaseNull(error);
    });
}

- (void)stashAccountCredential:(NSData *)credential complete:(void(^)(bool success, NSError *error))complete
{
    CFErrorRef syncerror = NULL;

    if (![self syncWaitAndFlush:&syncerror]) {
        complete(NULL, (__bridge NSError *)syncerror);
        CFReleaseNull(syncerror);
        return;
    }

    sleep(1); // make up for keygen time in password based version - let syncdefaults catch up

    [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        SecKeyRef accountPrivateKey = NULL;
        CFErrorRef error = NULL;
        NSDictionary *attributes = @{
            (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
            (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
        };

        accountPrivateKey = SecKeyCreateWithData((__bridge CFDataRef)credential, (__bridge CFDictionaryRef)attributes, &error);
        if (accountPrivateKey == NULL) {
            complete(false, (__bridge NSError *)error);
            secnotice("pairing", "SecKeyCreateWithData failed: %@", error);
            CFReleaseNull(error);
            return;
        }

        if (!SOSAccountTryUserPrivateKey(self, accountPrivateKey, &error)) {
            CFReleaseNull(accountPrivateKey);
            complete(false, (__bridge NSError *)error);
            secnotice("pairing", "SOSAccountTryUserPrivateKey failed: %@", error);
            CFReleaseNull(error);
            return;
        }
        
        secnotice("pairing", "SOSAccountTryUserPrivateKey succeeded");

        CFReleaseNull(accountPrivateKey);
        complete(true, NULL);
    }];
    
    // This makes getting the private key the same as Asserting the password - we read all the other things
    // that we just expressed interest in.
    CFErrorRef error = NULL;
    if (!Flush(&error)) {
        secnotice("pairing", "failed final flush: %@", error ? error : NULL);
        return;
    }
    CFReleaseNull(error);
}

- (void)myPeerInfo:(void (^)(NSData *, NSError *))complete
{
    __block CFErrorRef localError = NULL;
    __block NSData *applicationBlob = NULL;
    [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        SOSPeerInfoRef application = SOSAccountCopyApplication(txn.account, &localError);
        if (application) {
            applicationBlob = CFBridgingRelease(SOSPeerInfoCopyEncodedData(application, kCFAllocatorDefault, &localError));
            CFReleaseNull(application);
        }
    }];
    complete(applicationBlob, (__bridge NSError *)localError);
    CFReleaseNull(localError);
}

- (void)circleHash:(void (^)(NSString *, NSError *))complete
{
    __block CFErrorRef localError = NULL;
    __block NSString *circleHash = NULL;
    SecAKSDoWithUserBagLockAssertion(&localError, ^{
        [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
            circleHash = CFBridgingRelease(SOSCircleCopyHashString(txn.account.trust.trustedCircle));
        }];
    });
    complete(circleHash, (__bridge NSError *)localError);
    CFReleaseNull(localError);

}


#if TARGET_OS_OSX || TARGET_OS_IOS


+ (SOSAccountGhostBustingOptions) ghostBustGetRampSettings {
    OTManager *otm = [OTManager manager];
    SOSAccountGhostBustingOptions options = ([otm ghostbustByMidEnabled] == YES ? SOSGhostBustByMID: 0) |
                                            ([otm ghostbustBySerialEnabled] == YES ? SOSGhostBustBySerialNumber : 0) |
                                            ([otm ghostbustByAgeEnabled] == YES ? SOSGhostBustSerialByAge: 0);
    return options;
}


#define GHOSTBUSTDATE @"ghostbustdate"

- (NSDate *) ghostBustGetDate {
    if(! self.settings) {
        self.settings =  [[NSUserDefaults alloc] initWithSuiteName:SOSAccountUserDefaultsSuite];
    }
    return [self.settings valueForKey:GHOSTBUSTDATE];
}

- (bool) ghostBustCheckDate {
    NSDate *ghostBustDate = [self ghostBustGetDate];
    if(ghostBustDate && ([ghostBustDate timeIntervalSinceNow] <= 0)) return true;
    return false;
}

- (void) ghostBustFollowup {
    if(! self.settings) {
        self.settings =  [[NSUserDefaults alloc] initWithSuiteName:SOSAccountUserDefaultsSuite];
    }
    NSTimeInterval earliestGB   = 60*60*24*3;  // wait at least 3 days
    NSTimeInterval latestGB     = 60*60*24*7;  // wait at most 7 days
    NSDate *ghostBustDate = SOSCreateRandomDateBetweenNowPlus(earliestGB, latestGB);
    [self.settings setValue:ghostBustDate forKey:GHOSTBUSTDATE];
}

// GhostBusting initial scheduling
- (void)ghostBustSchedule {
    NSDate *ghostBustDate = [self ghostBustGetDate];
    if(!ghostBustDate) {
        [self ghostBustFollowup];
    }
}

- (void) ghostBust:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete {
    __block bool result = false;
    __block CFErrorRef localError = NULL;
    
    if([SOSAuthKitHelpers accountIsHSA2]) {
        [SOSAuthKitHelpers activeMIDs:^(NSSet <SOSTrustedDeviceAttributes *> * _Nullable activeMIDs, NSError * _Nullable error) {
            SOSAuthKitHelpers *akh = [[SOSAuthKitHelpers alloc] initWithActiveMIDS:activeMIDs];
            if(akh) {
                SecAKSDoWithUserBagLockAssertion(&localError, ^{
                    [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
                        result = SOSAccountGhostBustCircle(txn.account, akh, options, 1);
                        [self ghostBustFollowup];
                    }];
                });
            }
            complete(result, NULL);
        }];
    } else {
        complete(false, NULL);
    }
}

- (void) ghostBustPeriodic:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete {
    NSDate *ghostBustDate = [self ghostBustGetDate];
    if(([ghostBustDate timeIntervalSinceNow] <= 0)) {
        if(options) {
            [self ghostBust: options complete: complete];
        } else {
            complete(false, nil);
        }
    }
}

- (void)ghostBustTriggerTimed:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete {
    // if no particular options are presented use the ramp options.
    // If TLKs haven't been set yet this will cause a deadlock.  THis interface should only be used by the security tool for internal testing.
    if(options == 0) {
        options = [SOSAccount ghostBustGetRampSettings];
    }
    [self ghostBust: options complete: complete];
}

- (void) ghostBustInfo: (void(^)(NSData *json, NSError *error))complete {
    // If TLKs haven't been set yet this will cause a deadlock.  THis interface should only be used by the security tool for internal testing.
    NSMutableDictionary *gbInfoDictionary = [NSMutableDictionary new];
    SOSAccountGhostBustingOptions options = [SOSAccount ghostBustGetRampSettings];
    NSString *ghostBustDate = [[self ghostBustGetDate] description];
    
    gbInfoDictionary[@"SOSGhostBustBySerialNumber"] = (SOSGhostBustBySerialNumber & options) ? @"ON": @"OFF";
    gbInfoDictionary[@"SOSGhostBustByMID"] = (SOSGhostBustByMID & options) ? @"ON": @"OFF";
    gbInfoDictionary[@"SOSGhostBustSerialByAge"] = (SOSGhostBustSerialByAge & options) ? @"ON": @"OFF";
    gbInfoDictionary[@"SOSAccountGhostBustDate"] = ghostBustDate;
    
    NSError *err = nil;
    NSData *json = [NSJSONSerialization dataWithJSONObject:gbInfoDictionary
                                                   options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys)
                                                     error:&err];
    if (!json) {
        secnotice("ghostbust", "Error during ghostBustInfo JSONification: %@", err.localizedDescription);
    }
    complete(json, err);
}

#else

+ (SOSAccountGhostBustingOptions) ghostBustGetRampSettings {
    return 0;
}

- (NSDate *) ghostBustGetDate {
    return nil;
}

- (void) ghostBustFollowup {
}

- (void)ghostBustSchedule {
}

- (void) ghostBust:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete {
    complete(false, NULL);
}

- (void) ghostBustPeriodic:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool busted, NSError *error))complete {
    complete(false, NULL);
}

- (void)ghostBustTriggerTimed:(SOSAccountGhostBustingOptions)options complete: (void(^)(bool ghostBusted, NSError *error))complete {
    complete(false, nil);
}

- (void) ghostBustInfo: (void(^)(NSData *json, NSError *error))complete {
    complete(nil, nil);
}

- (bool) ghostBustCheckDate {
    return false;
}

#endif

- (void)circleJoiningBlob:(NSData *)applicant complete:(void (^)(NSData *blob, NSError *))complete
{
    __block CFErrorRef localError = NULL;
    __block NSData *blob = NULL;
    SOSPeerInfoRef peer = SOSPeerInfoCreateFromData(NULL, &localError, (__bridge CFDataRef)applicant);
    if (peer == NULL) {
        complete(NULL, (__bridge NSError *)localError);
        CFReleaseNull(localError);
        return;
    }

    SecAKSDoWithUserBagLockAssertionSoftly(^{
        [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
            blob = CFBridgingRelease(SOSAccountCopyCircleJoiningBlob(txn.account, peer, &localError));
        }];
    });

    CFReleaseNull(peer);

    complete(blob, (__bridge NSError *)localError);
    CFReleaseNull(localError);
}

- (void)joinCircleWithBlob:(NSData *)blob version:(PiggyBackProtocolVersion)version complete:(void (^)(bool success, NSError *))complete
{
    __block CFErrorRef localError = NULL;
    __block bool res = false;

    [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        res = SOSAccountJoinWithCircleJoiningBlob(txn.account, (__bridge CFDataRef)blob, version, &localError);
    }];

    complete(res, (__bridge NSError *)localError);
    CFReleaseNull(localError);
}

- (void)initialSyncCredentials:(uint32_t)flags complete:(void (^)(NSArray *, NSError *))complete
{
    CFErrorRef error = NULL;
    uint32_t isflags = 0;

    if (flags & SOSControlInitialSyncFlagTLK)
        isflags |= SecServerInitialSyncCredentialFlagTLK;
    if (flags & SOSControlInitialSyncFlagPCS)
        isflags |= SecServerInitialSyncCredentialFlagPCS;
    if (flags & SOSControlInitialSyncFlagPCSNonCurrent)
        isflags |= SecServerInitialSyncCredentialFlagPCSNonCurrent;
    if (flags & SOSControlInitialSyncFlagBluetoothMigration)
        isflags |= SecServerInitialSyncCredentialFlagBluetoothMigration;


    NSArray *array = CFBridgingRelease(_SecServerCopyInitialSyncCredentials(isflags, &error));
    complete(array, (__bridge NSError *)error);
    CFReleaseNull(error);
}

- (void)importInitialSyncCredentials:(NSArray *)items complete:(void (^)(bool success, NSError *))complete
{
    CFErrorRef error = NULL;
    bool res = _SecServerImportInitialSyncCredentials((__bridge CFArrayRef)items, &error);
    complete(res, (__bridge NSError *)error);
    CFReleaseNull(error);
}

- (void)rpcTriggerSync:(NSArray <NSString *> *)peers complete:(void(^)(bool success, NSError *))complete
{
    __block CFErrorRef localError = NULL;
    __block bool res = false;
    
    secnotice("sync", "trigger a forced sync for %@", peers);
    
    SecAKSDoWithUserBagLockAssertion(&localError, ^{
        [self performTransaction:^(SOSAccountTransaction *txn) {
            if ([peers count]) {
                NSSet *peersSet = [NSSet setWithArray:peers];
                CFMutableSetRef handledPeers = SOSAccountSyncWithPeers(txn, (__bridge CFSetRef)peersSet, &localError);
                if (handledPeers && CFSetGetCount(handledPeers) == (CFIndex)[peersSet count]) {
                    res = true;
                }
                CFReleaseNull(handledPeers);
            } else {
                res = SOSAccountRequestSyncWithAllPeers(txn, &localError);
            }
        }];
    });
    complete(res, (__bridge NSError *)localError);
    CFReleaseNull(localError);
}

- (void)rpcTriggerBackup:(NSArray<NSString *>* _Nullable)backupPeers complete:(void (^)(NSError *error))complete
{
    __block CFErrorRef localError = NULL;

    if (backupPeers.count == 0) {
        SOSEngineRef engine = (SOSEngineRef) [self.kvs_message_transport SOSTransportMessageGetEngine];
        backupPeers = CFBridgingRelease(SOSEngineCopyBackupPeerNames(engine, &localError));
    }

#if OCTAGON
    [self triggerBackupForPeers:backupPeers];
#endif

    complete((__bridge NSError *)localError);
    CFReleaseNull(localError);
}

- (void)rpcTriggerRingUpdate:(void (^)(NSError *error))complete
{
#if OCTAGON
    [self triggerRingUpdate];
#endif
    complete(NULL);
}

- (void)getWatchdogParameters:(void (^)(NSDictionary* parameters, NSError* error))complete
{
    // SecdWatchdog is only available in the secd/securityd - no other binary will contain that class
    Class watchdogClass = NSClassFromString(@"SecdWatchdog");
    if (watchdogClass) {
        NSDictionary* parameters = [[watchdogClass watchdog] watchdogParameters];
        complete(parameters, nil);
    }
    else {
        complete(nil, [NSError errorWithDomain:@"com.apple.securityd.watchdog" code:1 userInfo:@{NSLocalizedDescriptionKey : @"failed to lookup SecdWatchdog class from ObjC runtime"}]);
    }
}

- (void)setWatchdogParmeters:(NSDictionary*)parameters complete:(void (^)(NSError* error))complete
{
    // SecdWatchdog is only available in the secd/securityd - no other binary will contain that class
    NSError* error = nil;
    Class watchdogClass = NSClassFromString(@"SecdWatchdog");
    if (watchdogClass) {
        [[watchdogClass watchdog] setWatchdogParameters:parameters error:&error];
        complete(error);
    }
    else {
        complete([NSError errorWithDomain:@"com.apple.securityd.watchdog" code:1 userInfo:@{NSLocalizedDescriptionKey : @"failed to lookup SecdWatchdog class from ObjC runtime"}]);
    }
}

//
// MARK: Save Block
//

- (void) flattenToSaveBlock {
    if (self.saveBlock) {
        NSError* error = nil;
        NSData* saveData = [self encodedData:&error];

        (self.saveBlock)((__bridge CFDataRef) saveData, (__bridge CFErrorRef) error);
    }
}

CFDictionaryRef SOSAccountCopyGestalt(SOSAccount*  account) {
    return CFDictionaryCreateCopy(kCFAllocatorDefault, (__bridge CFDictionaryRef)account.gestalt);
}

CFDictionaryRef SOSAccountCopyV2Dictionary(SOSAccount*  account) {
    CFDictionaryRef v2dict = SOSAccountGetValue(account, kSOSTestV2Settings, NULL);
    return CFDictionaryCreateCopy(kCFAllocatorDefault, v2dict);
}

static bool SOSAccountUpdateDSID(SOSAccount* account, CFStringRef dsid){
    SOSAccountSetValue(account, kSOSDSIDKey, dsid, NULL);
    //send new DSID over account changed
    [account.circle_transport kvsSendOfficialDSID:dsid err:NULL];
    return true;
}

void SOSAccountAssertDSID(SOSAccount*  account, CFStringRef dsid) {
    CFStringRef accountDSID = SOSAccountGetValue(account, kSOSDSIDKey, NULL);
    if(accountDSID == NULL) {
        secdebug("updates", "Setting dsid, current dsid is empty for this account: %@", dsid);

        SOSAccountUpdateDSID(account, dsid);
    } else if(CFStringCompare(dsid, accountDSID, 0) != kCFCompareEqualTo) {
        secnotice("updates", "Changing DSID from: %@ to %@", accountDSID, dsid);

        //DSID has changed, blast the account!
        SOSAccountSetToNew(account);

        //update DSID to the new DSID
        SOSAccountUpdateDSID(account, dsid);
    } else {
        secnotice("updates", "Not Changing DSID: %@ to %@", accountDSID, dsid);
    }
}


void SOSAccountPendDisableViewSet(SOSAccount*  account, CFSetRef disabledViews)
{
    [account.trust valueUnionWith:kSOSPendingDisableViewsToBeSetKey valuesToUnion:disabledViews];
    [account.trust valueSubtractFrom:kSOSPendingEnableViewsToBeSetKey valuesToSubtract:disabledViews];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
SOSViewResultCode SOSAccountVirtualV0Behavior(SOSAccount*  account, SOSViewActionCode actionCode) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    // The V0 view switches on and off all on it's own, we allow people the delusion
    // of control and status if it's what we're stuck at., otherwise error.
    if (SOSAccountSyncingV0(account)) {
        require_action_quiet(actionCode == kSOSCCViewDisable, errOut, CFSTR("Can't disable V0 view and it's on right now"));
        retval = kSOSCCViewMember;
    } else {
        require_action_quiet(actionCode == kSOSCCViewEnable, errOut, CFSTR("Can't enable V0 and it's off right now"));
        retval = kSOSCCViewNotMember;
    }
errOut:
    return retval;
}
#pragma clang diagnostic pop

SOSAccount*  SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory) {

    SOSAccount* a = [[SOSAccount alloc] initWithGestalt:gestalt factory:factory];
    dispatch_sync(a.queue, ^{
        secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountCreate");
        a.key_interests_need_updating = true;
    });
    
    return a;
}

static OSStatus do_delete(CFDictionaryRef query) {
    OSStatus result;
    
    result = SecItemDelete(query);
    if (result) {
        secerror("SecItemDelete: %d", (int)result);
    }
     return result;
}

static int
do_keychain_delete_aks_bags()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                 kSecClass,           kSecClassGenericPassword,
                                 kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
                                 kSecAttrAccount,     CFSTR("SecureBackupPublicKeybag"),
                                 kSecAttrService,     CFSTR("SecureBackupService"),
                                 kSecAttrSynchronizable, kCFBooleanTrue,
                                 kSecUseTombstones,     kCFBooleanFalse,
                                 NULL);

    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

static int
do_keychain_delete_identities()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                kSecClass, kSecClassKey,
                                kSecAttrSynchronizable, kCFBooleanTrue,
                                kSecUseTombstones, kCFBooleanFalse,
                                kSecAttrAccessGroup, CFSTR("com.apple.security.sos"),
                                NULL);
  
    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

static int
do_keychain_delete_lakitu()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecClass, kSecClassGenericPassword,
                                                        kSecAttrSynchronizable, kCFBooleanTrue,
                                                        kSecUseTombstones, kCFBooleanFalse,
                                                        kSecAttrAccessGroup, CFSTR("com.apple.lakitu"),
                                                        kSecAttrAccount, CFSTR("EscrowServiceBypassToken"),
                                                        kSecAttrService, CFSTR("EscrowService"),
                                                        NULL);
    
    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

static int
do_keychain_delete_sbd()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecClass, kSecClassGenericPassword,
                                                        kSecAttrSynchronizable, kCFBooleanTrue,
                                                        kSecUseTombstones, kCFBooleanFalse,
                                                        kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
                                                        NULL);
    
    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

void SOSAccountSetToNew(SOSAccount*  a)
{
    secnotice("accountChange", "Setting Account to New");
    int result = 0;

    /* remove all syncable items */
    result = do_keychain_delete_aks_bags(); (void) result;
    secdebug("set to new", "result for deleting aks bags: %d", result);

    result = do_keychain_delete_identities(); (void) result;
    secdebug("set to new", "result for deleting identities: %d", result);
 
    result = do_keychain_delete_lakitu(); (void) result;
    secdebug("set to new", "result for deleting lakitu: %d", result);
    
    result = do_keychain_delete_sbd(); (void) result;
    secdebug("set to new", "result for deleting sbd: %d", result);

    a.accountKeyIsTrusted = false;

    if (a.user_private_timer) {
        dispatch_source_cancel(a.user_private_timer);
        a.user_private_timer = NULL;
        xpc_transaction_end();

    }
    if (a.lock_notification_token != NOTIFY_TOKEN_INVALID) {
        notify_cancel(a.lock_notification_token);
        a.lock_notification_token = NOTIFY_TOKEN_INVALID;
    }

    // keeping gestalt;
    // keeping factory;
    // Live Notification
    // change_blocks;
    // update_interest_block;
    // update_block;
    SOSUnregisterTransportKeyParameter(a.key_transport);
    SOSUnregisterTransportMessage(a.kvs_message_transport);
    SOSUnregisterTransportCircle(a.circle_transport);

    a.circle_transport = NULL;
    a.kvs_message_transport = nil;

    a.trust = nil;
    a.trust = [[SOSAccountTrustClassic alloc]initWithRetirees:[NSMutableSet set] fpi:NULL circle:NULL departureCode:kSOSDepartureReasonError peerExpansion:[NSMutableDictionary dictionary]];

    [a ensureFactoryCircles]; // Does rings too

    // By resetting our expansion dictionary we've reset our UUID, so we'll be notified properly
    SOSAccountEnsureUUID(a);
    secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountSetToNew");
    a.key_interests_need_updating = true;
}

bool SOSAccountIsNew(SOSAccount*  account, CFErrorRef *error){
    bool result = false;
    SOSAccountTrustClassic* trust = account.trust;
    if(account.accountKeyIsTrusted != false || trust.departureCode != kSOSNeverAppliedToCircle ||
       CFSetGetCount((__bridge CFSetRef)trust.retirees) != 0)
        return result;

    if(trust.retirees != nil)
        return result;
    if(trust.expansion != nil)
        return result;

    if(account.user_private_timer != NULL || account.lock_notification_token != NOTIFY_TOKEN_INVALID)
        return result;

    result = true;

    return result;
}

dispatch_queue_t SOSAccountGetQueue(SOSAccount*  account) {
    return account.queue;
}

void SOSAccountSetUserPublicTrustedForTesting(SOSAccount*  account){
    account.accountKeyIsTrusted = true;
}

-(SOSCCStatus) getCircleStatus:(CFErrorRef*) error
{
    SOSCCStatus circleStatus = [self.trust getCircleStatusOnly:error];
    if (!SOSAccountHasPublicKey(self, error)) {
        if(circleStatus == kSOSCCInCircle) {
            if(error) {
                CFReleaseNull(*error);
                SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Public Key isn't available, this peer is in the circle, but invalid. The iCloud Password must be provided to keychain syncing subsystem to repair this."), NULL, error);
            }
        }
        circleStatus = kSOSCCError;
    }
    return circleStatus;
}

-(bool) isInCircle:(CFErrorRef *)error
{
    SOSCCStatus result = [self getCircleStatus:error];
    if (result != kSOSCCInCircle) {
        SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("Not in circle"));
        return false;
    }
    return true;
}


bool SOSAccountScanForRetired(SOSAccount*  account, SOSCircleRef circle, CFErrorRef *error) {

    SOSAccountTrustClassic *trust = account.trust;
    NSMutableSet* retirees = trust.retirees;
    SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
        CFSetSetValue((__bridge CFMutableSetRef) retirees, peer);
        CFErrorRef cleanupError = NULL;
        if (![account.trust cleanupAfterPeer:account.kvs_message_transport circleTransport:account.circle_transport seconds:RETIREMENT_FINALIZATION_SECONDS circle:circle cleanupPeer:peer err:&cleanupError]) {
            secnotice("retirement", "Error cleaning up after peer, probably orphaned some stuff in KVS: (%@) – moving on", cleanupError);
        }
        CFReleaseSafe(cleanupError);
    });
    return true;
}

SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccount*  account, SOSCircleRef starting_circle, CFErrorRef *error) {
    SOSCircleRef new_circle = SOSCircleCopyCircle(NULL, starting_circle, error);
    SOSPeerInfoRef me = account.peerInfo;
    bool iAmApplicant = me && SOSCircleHasApplicant(new_circle, me, NULL);

    SOSAccountTrustClassic *trust = account.trust;
    NSMutableSet* retirees = trust.retirees;

    if(!new_circle) return NULL;
    __block bool workDone = false;
    if (retirees) {
        CFSetForEach((__bridge CFSetRef)retirees, ^(const void* value) {
            SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
            if (isSOSPeerInfo(pi)) {
                SOSCircleUpdatePeerInfo(new_circle, pi);
                workDone = true;
            }
        });
    }

    if(workDone && SOSCircleCountPeers(new_circle) == 0) {
        SecKeyRef userPrivKey = SOSAccountGetPrivateCredential(account, error);
 
        if(iAmApplicant) {
            if(userPrivKey) {
                secnotice("resetToOffering", "Reset to offering with last retirement and me as applicant");
                if(!SOSCircleResetToOffering(new_circle, userPrivKey, account.fullPeerInfo, error) ||
                   ![account.trust addiCloudIdentity:new_circle key:userPrivKey err:error]){
                    CFReleaseNull(new_circle);
                    return NULL;
                }
                account.notifyBackupOnExit = true;
            } else {
                // Do nothing.  We can't resetToOffering without a userPrivKey.  If we were to resetToEmpty
                // we won't push the result later in handleUpdateCircle.  If we leave the circle as it is
                // we have a chance to set things right with a SetCreds/Join sequence.  This will cause
                // handleUpdateCircle to return false.
                CFReleaseNull(new_circle);
                return NULL;
            }
        } else {
            // This case is when we aren't an applicant and the circle is retirement-empty.
            secnotice("circleOps", "Reset to empty with last retirement");
            SOSCircleResetToEmpty(new_circle, NULL);
        }
    }

    return new_circle;
}

//
// MARK: Circle Membership change notificaion
//

void SOSAccountAddChangeBlock(SOSAccount*  a, SOSAccountCircleMembershipChangeBlock changeBlock) {
    SOSAccountCircleMembershipChangeBlock copy = changeBlock;
    [a.change_blocks addObject:copy];
}

void SOSAccountRemoveChangeBlock(SOSAccount*  a, SOSAccountCircleMembershipChangeBlock changeBlock) {
    [a.change_blocks removeObject:changeBlock];
}

void SOSAccountPurgeIdentity(SOSAccount*  account) {
    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;

    if (identity) {
        // Purge private key but don't return error if we can't.
        CFErrorRef purgeError = NULL;
        if (!SOSFullPeerInfoPurgePersistentKey(identity, &purgeError)) {
            secwarning("Couldn't purge persistent key for %@ [%@]", identity, purgeError);
        }
        CFReleaseNull(purgeError);

        trust.fullPeerInfo = nil;
    }
}

bool sosAccountLeaveCircleWithAnalytics(SOSAccount* account, SOSCircleRef circle, NSData* parentData, CFErrorRef* error) {
    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;
    NSMutableSet* retirees = trust.retirees;

    NSError* localError = nil;
    SFSignInAnalytics* parent = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFSignInAnalytics class] fromData:parentData error:&localError];
    
    SOSFullPeerInfoRef fpi = identity;
    if(!fpi) return false;

    CFErrorRef retiredError = NULL;

    bool retval = false;

    SFSignInAnalytics *promoteToRetiredEvent = [parent newSubTaskForEvent:@"promoteToRetiredEvent"];
    SOSPeerInfoRef retire_peer = SOSFullPeerInfoPromoteToRetiredAndCopy(fpi, &retiredError);
    if(retiredError){
        [promoteToRetiredEvent logRecoverableError:(__bridge NSError*)retiredError];
        secerror("SOSFullPeerInfoPromoteToRetiredAndCopy error: %@", retiredError);
        if(error){
            *error = retiredError;
        }else{
            CFReleaseNull(retiredError);
        }
    }
    [promoteToRetiredEvent stopWithAttributes:nil];

    if (!retire_peer) {
        secerror("Create ticket failed for peer %@: %@", fpi, localError);
    } else {
        // See if we need to repost the circle we could either be an applicant or a peer already in the circle
        if(SOSCircleHasApplicant(circle, retire_peer, NULL)) {
            // Remove our application if we have one.
            SOSCircleWithdrawRequest(circle, retire_peer, NULL);
        } else if (SOSCircleHasPeer(circle, retire_peer, NULL)) {
            if (SOSCircleUpdatePeerInfo(circle, retire_peer)) {
                CFErrorRef cleanupError = NULL;
                SFSignInAnalytics *cleanupEvent = [parent newSubTaskForEvent:@"cleanupAfterPeerEvent"];
                if (![account.trust cleanupAfterPeer:account.kvs_message_transport circleTransport:account.circle_transport seconds:RETIREMENT_FINALIZATION_SECONDS circle:circle cleanupPeer:retire_peer err:&cleanupError]) {
                    secerror("Error cleanup up after peer (%@): %@", retire_peer, cleanupError);
                }
                [cleanupEvent stopWithAttributes:nil];
                CFReleaseSafe(cleanupError);
            }
        }

        // Store the retirement record locally.
        CFSetAddValue((__bridge CFMutableSetRef)retirees, retire_peer);

        trust.retirees = retirees;

        // Write retirement to Transport
        CFErrorRef postError = NULL;
        SFSignInAnalytics *postRestirementEvent = [parent newSubTaskForEvent:@"postRestirementEvent"];
        if(![account.circle_transport postRetirement:SOSCircleGetName(circle) peer:retire_peer err:&postError]){
            [postRestirementEvent logRecoverableError:(__bridge NSError*)postError];
            secwarning("Couldn't post retirement (%@)", postError);
        }
        [postRestirementEvent stopWithAttributes:nil];

        SFSignInAnalytics *flushChangesEvent = [parent newSubTaskForEvent:@"flushChangesEvent"];

        if(![account.circle_transport flushChanges:&postError]){
            [flushChangesEvent logRecoverableError:(__bridge NSError*)postError];
            secwarning("Couldn't flush retirement data (%@)", postError);
        }
        [flushChangesEvent stopWithAttributes:nil];
        CFReleaseNull(postError);
    }
    SFSignInAnalytics *purgeIdentityEvent = [parent newSubTaskForEvent:@"purgeIdentityEvent"];
    SOSAccountPurgeIdentity(account);
    [purgeIdentityEvent stopWithAttributes:nil];
    retval = true;

    CFReleaseNull(retire_peer);
    return retval;
}

bool sosAccountLeaveCircle(SOSAccount* account, SOSCircleRef circle, CFErrorRef* error) {
    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;
    NSMutableSet* retirees = trust.retirees;

    SOSFullPeerInfoRef fpi = identity;
    if(!fpi) return false;

	CFErrorRef localError = NULL;

    bool retval = false;

    SOSPeerInfoRef retire_peer = SOSFullPeerInfoPromoteToRetiredAndCopy(fpi, &localError);
    if (!retire_peer) {
        secerror("Create ticket failed for peer %@: %@", fpi, localError);
    } else {
        // See if we need to repost the circle we could either be an applicant or a peer already in the circle
        if(SOSCircleHasApplicant(circle, retire_peer, NULL)) {
            // Remove our application if we have one.
            SOSCircleWithdrawRequest(circle, retire_peer, NULL);
        } else if (SOSCircleHasPeer(circle, retire_peer, NULL)) {
            if (SOSCircleUpdatePeerInfo(circle, retire_peer)) {
                CFErrorRef cleanupError = NULL;
                if (![account.trust cleanupAfterPeer:account.kvs_message_transport circleTransport:account.circle_transport seconds:RETIREMENT_FINALIZATION_SECONDS circle:circle cleanupPeer:retire_peer err:&cleanupError]) {
                    secerror("Error cleanup up after peer (%@): %@", retire_peer, cleanupError);
                }
                CFReleaseSafe(cleanupError);
            }
        }

        // Store the retirement record locally.
        CFSetAddValue((__bridge CFMutableSetRef)retirees, retire_peer);

        trust.retirees = retirees;
        
        // Write retirement to Transport
        CFErrorRef postError = NULL;
        if(![account.circle_transport postRetirement:SOSCircleGetName(circle) peer:retire_peer err:&postError]){
            secwarning("Couldn't post retirement (%@)", postError);
        }
        if(![account.circle_transport flushChanges:&postError]){
            secwarning("Couldn't flush retirement data (%@)", postError);
        }
        CFReleaseNull(postError);
    }

    SOSAccountPurgeIdentity(account);

    retval = true;

    CFReleaseNull(localError);
    CFReleaseNull(retire_peer);
    return retval;
}

bool sosAccountLeaveRing(SOSAccount*  account, SOSRingRef ring, CFErrorRef* error) {
    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;

    SOSFullPeerInfoRef fpi = identity;
    if(!fpi) return false;
    SOSPeerInfoRef     pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFStringRef        peerID = SOSPeerInfoGetPeerID(pi);
    
    CFErrorRef localError = NULL;
    
    bool retval = false;
    bool writeRing = false;
    bool writePeerInfo = false;
    
    if(SOSRingHasPeerID(ring, peerID)) {
        writePeerInfo = true;
    }

    if(writePeerInfo || writeRing) {
        SOSRingWithdraw(ring, NULL, fpi, error);
    }
    
    if (writeRing) {
        CFDataRef ring_data = SOSRingCopyEncodedData(ring, error);
        
        if (ring_data) {
            [account.circle_transport kvsRingPostRing:SOSRingGetName(ring) ring:ring_data err:NULL];
        }
        CFReleaseNull(ring_data);
    }
    retval = true;
    CFReleaseNull(localError);
    return retval;
}

bool SOSAccountPostDebugScope(SOSAccount*  account, CFTypeRef scope, CFErrorRef *error) {
    bool result = false;
    if (account.circle_transport) {
        result = [account.circle_transport kvssendDebugInfo:kSOSAccountDebugScope debug:scope err:error];
    }
    return result;
}

/*
 NSUbiquitousKeyValueStoreInitialSyncChange is only posted if there is any
 local value that has been overwritten by a distant value. If there is no
 conflict between the local and the distant values when doing the initial
 sync (e.g. if the cloud has no data stored or the client has not stored
 any data yet), you'll never see that notification.

 NSUbiquitousKeyValueStoreInitialSyncChange implies an initial round trip
 with server but initial round trip with server does not imply
 NSUbiquitousKeyValueStoreInitialSyncChange.
 */


//
// MARK: Status summary
//


CFStringRef SOSAccountGetSOSCCStatusString(SOSCCStatus status) {
    switch(status) {
        case kSOSCCInCircle: return CFSTR("kSOSCCInCircle");
        case kSOSCCNotInCircle: return CFSTR("kSOSCCNotInCircle");
        case kSOSCCRequestPending: return CFSTR("kSOSCCRequestPending");
        case kSOSCCCircleAbsent: return CFSTR("kSOSCCCircleAbsent");
        case kSOSCCError: return CFSTR("kSOSCCError");
    }
    return CFSTR("kSOSCCError");
}
SOSCCStatus SOSAccountGetSOSCCStatusFromString(CFStringRef status) {
    if(CFEqualSafe(status, CFSTR("kSOSCCInCircle"))) {
        return kSOSCCInCircle;
    } else if(CFEqualSafe(status, CFSTR("kSOSCCInCircle"))) {
        return kSOSCCInCircle;
    } else if(CFEqualSafe(status, CFSTR("kSOSCCNotInCircle"))) {
        return kSOSCCNotInCircle;
    } else if(CFEqualSafe(status, CFSTR("kSOSCCRequestPending"))) {
        return kSOSCCRequestPending;
    } else if(CFEqualSafe(status, CFSTR("kSOSCCCircleAbsent"))) {
        return kSOSCCCircleAbsent;
    } else if(CFEqualSafe(status, CFSTR("kSOSCCError"))) {
        return kSOSCCError;
    }
    return kSOSCCError;
}

//
// MARK: Account Reset Circles
//

// This needs to be called within a [trust modifyCircle()] block


bool SOSAccountRemoveIncompleteiCloudIdentities(SOSAccount*  account, SOSCircleRef circle, SecKeyRef privKey, CFErrorRef *error) {
    bool retval = false;

    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;

    CFMutableSetRef iCloud2Remove = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            SOSFullPeerInfoRef icfpi = SOSFullPeerInfoCreateCloudIdentity(kCFAllocatorDefault, peer, NULL);
            if(!icfpi) {
                CFSetAddValue(iCloud2Remove, peer);
            }
            CFReleaseNull(icfpi);
        }
    });
    
    if(CFSetGetCount(iCloud2Remove) > 0) {
        retval = true;
        SOSCircleRemovePeers(circle, privKey, identity, iCloud2Remove, error);
    }
    CFReleaseNull(iCloud2Remove);
    return retval;
}

//
// MARK: start backups
//


- (bool)_onQueueEnsureInBackupRings {
    __block bool result = false;
    __block CFErrorRef error = NULL;
    secnotice("backup", "Ensuring in rings");

    dispatch_assert_queue(self.queue);

    if(!self.backup_key){
        return true;
    }

    if(!SOSBSKBIsGoodBackupPublic((__bridge CFDataRef)self.backup_key, &error)){
        secnotice("backupkey", "account backup key isn't valid: %@", error);
        self.backup_key = nil;
        CFReleaseNull(error);
        return false;
    }

    NSData *peerBackupKey = (__bridge_transfer NSData*)SOSPeerInfoCopyBackupKey(self.peerInfo);
    if(![peerBackupKey isEqual:self.backup_key]) {
        result = SOSAccountUpdatePeerInfo(self, CFSTR("Backup public key"), &error, ^bool(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
            return SOSFullPeerInfoUpdateBackupKey(fpi, (__bridge CFDataRef)(self.backup_key), error);
        });
        if (!result) {
            secnotice("backupkey", "Failed to setup backup public key in peerInfo from account: %@", error);
            CFReleaseNull(error);
            return result;
        }
    }

    // It's a good key, we're going with it. Stop backing up the old way.
    CFErrorRef localError = NULL;
    if (!SOSDeleteV0Keybag(&localError)) {
        secerror("Failed to delete v0 keybag: %@", localError);
    }
    CFReleaseNull(localError);

    // Setup backups the new way.
    SOSAccountForEachBackupView(self, ^(const void *value) {
        CFStringRef viewName = asString(value, NULL);
        bool resetRing = SOSAccountValidateBackupRingForView(self, viewName, NULL);
        if(resetRing) {
            SOSAccountUpdateBackupRing(self, viewName, NULL, ^SOSRingRef(SOSRingRef existing, CFErrorRef *error) {
                SOSRingRef newRing = SOSAccountCreateBackupRingForView(self, viewName, error);
                return newRing;
            });
        }
    });

    if (!result) {
        secnotice("backupkey", "Failed to setup backup public key: %@", error);
    }
    CFReleaseNull(error);
    return result;
}

//
// MARK: Recovery Public Key Functions
//

bool SOSAccountRegisterRecoveryPublicKey(SOSAccountTransaction* txn, CFDataRef recovery_key, CFErrorRef *error){
    bool retval = SOSAccountSetRecoveryKey(txn.account, recovery_key, error);
    if(retval) secnotice("recovery", "successfully registered recovery public key");
    else secnotice("recovery", "could not register recovery public key: %@", *error);
    SOSClearErrorIfTrue(retval, error);
    return retval;
}

bool SOSAccountClearRecoveryPublicKey(SOSAccountTransaction* txn, CFDataRef recovery_key, CFErrorRef *error){
    bool retval = SOSAccountRemoveRecoveryKey(txn.account, error);
    if(retval) secnotice("recovery", "RK Cleared");
    else secnotice("recovery", "Couldn't clear RK(%@)", *error);
    SOSClearErrorIfTrue(retval, error);
    return retval;
}

CFDataRef SOSAccountCopyRecoveryPublicKey(SOSAccountTransaction* txn, CFErrorRef *error){
    CFDataRef result = NULL;
    result = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, txn.account, error);
    if(!result)  secnotice("recovery", "Could not retrieve the recovery public key from the ring: %@", *error);

    if (!isData(result)) {
        CFReleaseNull(result);
    }
    SOSClearErrorIfTrue(result != NULL, error);

    return result;
}

//
// MARK: Joining
//

static bool SOSAccountJoinCircleWithAnalytics(SOSAccountTransaction* aTxn, SecKeyRef user_key,
                                 bool use_cloud_peer, NSData* parentEvent, CFErrorRef* error) {
    SOSAccount* account = aTxn.account;
    SOSAccountTrustClassic *trust = account.trust;
    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;
    SFSignInAnalytics *ensureFullPeerAvailableEvent = nil;
    NSError* localError = nil;
    SFSignInAnalytics* parent = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFSignInAnalytics class] fromData:parentEvent error:&localError];

    require_action_quiet(trust.trustedCircle, fail, SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("Don't have circle when joining???")));
    ensureFullPeerAvailableEvent = [parent newSubTaskForEvent:@"ensureFullPeerAvailableEvent"];
    require_quiet([account.trust ensureFullPeerAvailable:(__bridge CFDictionaryRef)account.gestalt deviceID:(__bridge CFStringRef)account.deviceID backupKey:(__bridge CFDataRef)account.backup_key err:error], fail);
    [ensureFullPeerAvailableEvent stopWithAttributes:nil];
    
    SOSFullPeerInfoRef myCirclePeer = trust.fullPeerInfo;
    if (SOSCircleCountPeers(trust.trustedCircle) == 0 || SOSAccountGhostResultsInReset(account)) {
        secnotice("resetToOffering", "Resetting circle to offering since there are no peers");
        // this also clears initial sync data
        result = [account.trust resetCircleToOffering:aTxn userKey:user_key err:error];
    } else {
        SOSAccountInitializeInitialSync(account);
        if (use_cloud_peer) {
            cloud_full_peer = SOSCircleCopyiCloudFullPeerInfoRef(trust.trustedCircle, NULL);
        }
        SFSignInAnalytics *acceptApplicantEvent = [parent newSubTaskForEvent:@"acceptApplicantEvent"];
        [account.trust modifyCircle:account.circle_transport err:error action:^bool(SOSCircleRef circle) {
            result = SOSAccountAddEscrowToPeerInfo(account, myCirclePeer, error);
            result &= SOSCircleRequestAdmission(circle, user_key, myCirclePeer, error);
            trust.departureCode = kSOSNeverLeftCircle;
            if(result && cloud_full_peer) {
                CFErrorRef localError = NULL;
                CFStringRef cloudid = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(cloud_full_peer));
                require_quiet(cloudid, finish);
                require_quiet(SOSCircleHasActivePeerWithID(circle, cloudid, &localError), finish);
                require_quiet(SOSCircleAcceptRequest(circle, user_key, cloud_full_peer, SOSFullPeerInfoGetPeerInfo(myCirclePeer), &localError), finish);
            finish:
                if (localError){
                    [acceptApplicantEvent logRecoverableError:(__bridge NSError *)(localError)];
                    secerror("Failed to join with cloud identity: %@", localError);
                    CFReleaseNull(localError);
                }
            }
            return result;
        }];
        [acceptApplicantEvent stopWithAttributes:nil];
        if (use_cloud_peer) {
            SFSignInAnalytics *updateOutOfDateSyncViewsEvent = [acceptApplicantEvent newSubTaskForEvent:@"updateOutOfDateSyncViewsEvent"];
            SOSAccountUpdateOutOfSyncViews(aTxn, SOSViewsGetAllCurrent());
            [updateOutOfDateSyncViewsEvent stopWithAttributes:nil];
        }
    }
fail:
    CFReleaseNull(cloud_full_peer);
    return result;
}

static bool SOSAccountJoinCircle(SOSAccountTransaction* aTxn, SecKeyRef user_key,
                                 bool use_cloud_peer, CFErrorRef* error) {
    SOSAccount* account = aTxn.account;
    SOSAccountTrustClassic *trust = account.trust;
    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;
    require_action_quiet(trust.trustedCircle, fail, SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("Don't have circle when joining???")));
    require_quiet([account.trust ensureFullPeerAvailable:(__bridge CFDictionaryRef)account.gestalt deviceID:(__bridge CFStringRef)account.deviceID backupKey:(__bridge CFDataRef)account.backup_key err:error], fail);
    SOSFullPeerInfoRef myCirclePeer = trust.fullPeerInfo;
    if (SOSCircleCountPeers(trust.trustedCircle) == 0 || SOSAccountGhostResultsInReset(account)) {
        secnotice("resetToOffering", "Resetting circle to offering since there are no peers");
        // this also clears initial sync data
        result = [account.trust resetCircleToOffering:aTxn userKey:user_key err:error];
    } else {
        SOSAccountInitializeInitialSync(account);
        if (use_cloud_peer) {
            cloud_full_peer = SOSCircleCopyiCloudFullPeerInfoRef(trust.trustedCircle, NULL);
        }
        [account.trust modifyCircle:account.circle_transport err:error action:^bool(SOSCircleRef circle) {
            result = SOSAccountAddEscrowToPeerInfo(account, myCirclePeer, error);
            result &= SOSCircleRequestAdmission(circle, user_key, myCirclePeer, error);
            trust.departureCode = kSOSNeverLeftCircle;
            if(result && cloud_full_peer) {
                CFErrorRef localError = NULL;
                CFStringRef cloudid = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(cloud_full_peer));
                require_quiet(cloudid, finish);
                require_quiet(SOSCircleHasActivePeerWithID(circle, cloudid, &localError), finish);
                require_quiet(SOSCircleAcceptRequest(circle, user_key, cloud_full_peer, SOSFullPeerInfoGetPeerInfo(myCirclePeer), &localError), finish);
            finish:
                if (localError){
                    secerror("Failed to join with cloud identity: %@", localError);
                    CFReleaseNull(localError);
                }
            }
            return result;
        }];
        if (use_cloud_peer) {
            SOSAccountUpdateOutOfSyncViews(aTxn, SOSViewsGetAllCurrent());
        }
    }
fail:
    CFReleaseNull(cloud_full_peer);
    return result;
}

static bool SOSAccountJoinCirclesWithAnalytics_internal(SOSAccountTransaction* aTxn, bool use_cloud_identity, NSData* parentEvent, CFErrorRef* error) {
    SOSAccount* account = aTxn.account;
    SOSAccountTrustClassic *trust = account.trust;
    bool success = false;

    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    require_quiet(user_key, done); // Fail if we don't get one.

    require_action_quiet(trust.trustedCircle, done, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to join")));

    if (trust.fullPeerInfo != NULL) {
        SOSPeerInfoRef myPeer = trust.peerInfo;
        success = SOSCircleHasPeer(trust.trustedCircle, myPeer, NULL);
        require_quiet(!success, done);

        SOSCircleRemoveRejectedPeer(trust.trustedCircle, myPeer, NULL); // If we were rejected we should remove it now.

        if (!SOSCircleHasApplicant(trust.trustedCircle, myPeer, NULL)) {
            secerror("Resetting my peer (ID: %@) for circle '%@' during application", SOSPeerInfoGetPeerID(myPeer), SOSCircleGetName(trust.trustedCircle));

            trust.fullPeerInfo = NULL;
        }
    }

    success = SOSAccountJoinCircleWithAnalytics(aTxn, user_key, use_cloud_identity, parentEvent, error);

    require_quiet(success, done);

    trust.departureCode = kSOSNeverLeftCircle;

done:
    return success;
}

static bool SOSAccountJoinCircles_internal(SOSAccountTransaction* aTxn, bool use_cloud_identity, CFErrorRef* error) {
    SOSAccount* account = aTxn.account;
    SOSAccountTrustClassic *trust = account.trust;
    bool success = false;

    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    require_quiet(user_key, done); // Fail if we don't get one.

    require_action_quiet(trust.trustedCircle, done, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to join")));
    
    if (trust.fullPeerInfo != NULL) {
        SOSPeerInfoRef myPeer = trust.peerInfo;
        success = SOSCircleHasPeer(trust.trustedCircle, myPeer, NULL);
        require_quiet(!success, done);

        SOSCircleRemoveRejectedPeer(trust.trustedCircle, myPeer, NULL); // If we were rejected we should remove it now.

        if (!SOSCircleHasApplicant(trust.trustedCircle, myPeer, NULL)) {
        	secerror("Resetting my peer (ID: %@) for circle '%@' during application", SOSPeerInfoGetPeerID(myPeer), SOSCircleGetName(trust.trustedCircle));
            
            trust.fullPeerInfo = NULL;
        }
    }
    
    success = SOSAccountJoinCircle(aTxn, user_key, use_cloud_identity, error);

    require_quiet(success, done);
       
    trust.departureCode = kSOSNeverLeftCircle;

done:
    return success;
}

bool SOSAccountJoinCirclesWithAnalytics(SOSAccountTransaction* aTxn, NSData* parentEvent, CFErrorRef* error) {
    secnotice("circleOps", "Normal path circle join (SOSAccountJoinCirclesWithAnalytics)");
    return SOSAccountJoinCirclesWithAnalytics_internal(aTxn, false, parentEvent, error);
}

bool SOSAccountJoinCircles(SOSAccountTransaction* aTxn, CFErrorRef* error) {
	secnotice("circleOps", "Normal path circle join (SOSAccountJoinCircles)");
    return SOSAccountJoinCircles_internal(aTxn, false, error);
}

bool SOSAccountJoinCirclesAfterRestore(SOSAccountTransaction* aTxn, CFErrorRef* error) {
	secnotice("circleOps", "Joining after restore (SOSAccountJoinCirclesAfterRestore)");
    return SOSAccountJoinCircles_internal(aTxn, true, error);
}

bool SOSAccountJoinCirclesAfterRestoreWithAnalytics(SOSAccountTransaction* aTxn, NSData* parentEvent, CFErrorRef* error) {
    secnotice("circleOps", "Joining after restore (SOSAccountJoinCirclesAfterRestore)");
    return SOSAccountJoinCirclesWithAnalytics_internal(aTxn, true, parentEvent, error);
}

bool SOSAccountRemovePeersFromCircle(SOSAccount*  account, CFArrayRef peers, CFErrorRef* error)
{
    bool result = false;
    CFMutableSetRef peersToRemove = NULL;
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if(!user_key){
        secnotice("circleOps", "Can't remove without userKey");
        return result;
    }
    SOSFullPeerInfoRef me_full = account.fullPeerInfo;
    SOSPeerInfoRef me = account.peerInfo;
    if(!(me_full && me))
    {
        secnotice("circleOps", "Can't remove without being active peer");
        SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("Can't remove without being active peer"));
        return result;
    }
    
    result = true; // beyond this point failures would be rolled up in AccountModifyCircle.

    peersToRemove = CFSetCreateMutableForSOSPeerInfosByIDWithArray(kCFAllocatorDefault, peers);
    if(!peersToRemove)
    {
        CFReleaseNull(peersToRemove);
        secnotice("circleOps", "No peerSet to remove");
        return result;
    }

    // If we're one of the peers expected to leave - note that and then remove ourselves from the set (different handling).
    bool leaveCircle = CFSetContainsValue(peersToRemove, me);
    CFSetRemoveValue(peersToRemove, me);

    result &= [account.trust modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
        bool success = false;

        if(CFSetGetCount(peersToRemove) != 0) {
            require_quiet(SOSCircleRemovePeers(circle, user_key, me_full, peersToRemove, error), done);
            success = SOSAccountGenerationSignatureUpdate(account, error);
        } else success = true;

        if (success && leaveCircle) {
            secnotice("circleOps", "Leaving circle by client request (SOSAccountRemovePeersFromCircle)");
            success = sosAccountLeaveCircle(account, circle, error);
        }

    done:
        return success;

    }];
    
    if(result) {
        CFStringSetPerformWithDescription(peersToRemove, ^(CFStringRef description) {
            secnotice("circleOps", "Removed Peers from circle %@", description);
        });
    }

    CFReleaseNull(peersToRemove);
    return result;
}


bool SOSAccountRemovePeersFromCircleWithAnalytics(SOSAccount*  account, CFArrayRef peers, NSData* parentEvent, CFErrorRef* error)
{

    NSError* localError = nil;
    SFSignInAnalytics* parent = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFSignInAnalytics class] fromData:parentEvent error:&localError];

    bool result = false;
    CFMutableSetRef peersToRemove = NULL;
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if(!user_key){
        secnotice("circleOps", "Can't remove without userKey");
        return result;
    }
    SOSFullPeerInfoRef me_full = account.fullPeerInfo;
    SOSPeerInfoRef me = account.peerInfo;
    if(!(me_full && me))
    {
        secnotice("circleOps", "Can't remove without being active peer");
        SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("Can't remove without being active peer"));
        return result;
    }

    result = true; // beyond this point failures would be rolled up in AccountModifyCircle.

    peersToRemove = CFSetCreateMutableForSOSPeerInfosByIDWithArray(kCFAllocatorDefault, peers);
    if(!peersToRemove)
    {
        CFReleaseNull(peersToRemove);
        secnotice("circleOps", "No peerSet to remove");
        return result;
    }

    // If we're one of the peers expected to leave - note that and then remove ourselves from the set (different handling).
    bool leaveCircle = CFSetContainsValue(peersToRemove, me);
    CFSetRemoveValue(peersToRemove, me);

    result &= [account.trust modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
        bool success = false;

        if(CFSetGetCount(peersToRemove) != 0) {
            require_quiet(SOSCircleRemovePeers(circle, user_key, me_full, peersToRemove, error), done);
            SFSignInAnalytics *generationSignatureUpdateEvent = [parent newSubTaskForEvent:@"generationSignatureUpdateEvent"];
            success = SOSAccountGenerationSignatureUpdate(account, error);
            if(error && *error){
                NSError* signatureUpdateError = (__bridge NSError*)*error;
                [generationSignatureUpdateEvent logUnrecoverableError:signatureUpdateError];
            }
            [generationSignatureUpdateEvent stopWithAttributes:nil];
        } else success = true;

        if (success && leaveCircle) {
            secnotice("circleOps", "Leaving circle by client request (SOSAccountRemovePeersFromCircle)");
            success = sosAccountLeaveCircleWithAnalytics(account, circle, parentEvent, error);
        }

    done:
        return success;

    }];

    if(result) {
        CFStringSetPerformWithDescription(peersToRemove, ^(CFStringRef description) {
            secnotice("circleOps", "Removed Peers from circle %@", description);
        });
    }

    CFReleaseNull(peersToRemove);
    return result;
}

bool SOSAccountBail(SOSAccount*  account, uint64_t limit_in_seconds, CFErrorRef* error) {
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t group = dispatch_group_create();
    SOSAccountTrustClassic *trust = account.trust;
    __block bool result = false;
    secnotice("circle", "Attempting to leave circle - best effort - in %llu seconds\n", limit_in_seconds);
    // Add a task to the group
    dispatch_group_async(group, queue, ^{
        [trust modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
            secnotice("circleOps", "Leaving circle by client request (Bail)");
            return sosAccountLeaveCircle(account, circle, error);
        }];
    });
    dispatch_time_t milestone = dispatch_time(DISPATCH_TIME_NOW, limit_in_seconds * NSEC_PER_SEC);
    dispatch_group_wait(group, milestone);

    trust.departureCode = kSOSWithdrewMembership;

    return result;
}


//
// MARK: Application
//

static void for_each_applicant_in_each_circle(SOSAccount*  account, CFArrayRef peer_infos,
                                              bool (^action)(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer)) {
    SOSAccountTrustClassic *trust = account.trust;

    SOSPeerInfoRef me = trust.peerInfo;
    CFErrorRef peer_error = NULL;
    if (trust.trustedCircle && me &&
        SOSCircleHasPeer(trust.trustedCircle, me, &peer_error)) {
        [account.trust modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle) {
            __block bool modified = false;
            CFArrayForEach(peer_infos, ^(const void *value) {
                SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
                if (isSOSPeerInfo(peer) && SOSCircleHasApplicant(circle, peer, NULL)) {
                    if (action(circle, trust.fullPeerInfo, peer)) {
                        modified = true;
                    }
                }
            });
            return modified;
        }];
    }
    if (peer_error)
        secerror("Got error in SOSCircleHasPeer: %@", peer_error);
    CFReleaseSafe(peer_error); // TODO: We should be accumulating errors here.
}

bool SOSAccountAcceptApplicants(SOSAccount*  account, CFArrayRef applicants, CFErrorRef* error) {
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    __block int64_t acceptedPeers = 0;

    for_each_applicant_in_each_circle(account, applicants, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer) {
        bool accepted = SOSCircleAcceptRequest(circle, user_key, myCirclePeer, peer, error);
        if (accepted)
            acceptedPeers++;
        return accepted;
    });

    if (acceptedPeers == CFArrayGetCount(applicants))
        return true;
    return false;
}

bool SOSAccountRejectApplicants(SOSAccount*  account, CFArrayRef applicants, CFErrorRef* error) {
    __block bool success = true;
    __block int64_t num_peers = 0;

    for_each_applicant_in_each_circle(account, applicants, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer) {
        bool rejected = SOSCircleRejectRequest(circle, myCirclePeer, peer, error);
        if (!rejected)
            success = false;
		else
			num_peers = MAX(num_peers, SOSCircleCountPeers(circle));
        return rejected;
    });

    return success;
}


CFStringRef SOSAccountCopyIncompatibilityInfo(SOSAccount*  account, CFErrorRef* error) {
    return CFSTR("We're compatible, go away");
}

enum DepartureReason SOSAccountGetLastDepartureReason(SOSAccount*  account, CFErrorRef* error) {
    SOSAccountTrustClassic *trust = account.trust;
    return trust.departureCode;
}

void SOSAccountSetLastDepartureReason(SOSAccount*  account, enum DepartureReason reason) {
    SOSAccountTrustClassic *trust = account.trust;
	trust.departureCode = reason;
}


CFArrayRef SOSAccountCopyGeneration(SOSAccount*  account, CFErrorRef *error) {
    CFArrayRef result = NULL;
    CFNumberRef generation = NULL;
    SOSAccountTrustClassic *trust = account.trust;

    require_quiet(SOSAccountHasPublicKey(account, error), fail);
    require_action_quiet(trust.trustedCircle, fail, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle")));

    generation = (CFNumberRef)SOSCircleGetGeneration(trust.trustedCircle);
    result = CFArrayCreateForCFTypes(kCFAllocatorDefault, generation, NULL);

fail:
    return result;
}

bool SOSValidateUserPublic(SOSAccount*  account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;

    return account.accountKeyIsTrusted;
}

bool SOSAccountEnsurePeerRegistration(SOSAccount*  account, CFErrorRef *error) {
    // TODO: this result is never set or used
    bool result = true;
    SOSAccountTrustClassic *trust = account.trust;

    secnotice("updates", "Ensuring peer registration.");

    if(!trust) {
        secnotice("updates", "Failed to get trust object in Ensuring peer registration.");
        return result;
    }

    if([account getCircleStatus: NULL] != kSOSCCInCircle) {
        return result;
    }

    // If we are not in the circle, there is no point in setting up peers
    if(!SOSAccountIsMyPeerActive(account, NULL)) {
        return result;
    }

    // This code only uses the SOSFullPeerInfoRef for two things:
    //  - Finding out if this device is in the trusted circle
    //  - Using the peerID for this device to see if the current peer is "me"
    //  - It is used indirectly by passing trust.fullPeerInfo to SOSEngineInitializePeerCoder
    
    CFStringRef my_id = SOSPeerInfoGetPeerID(trust.peerInfo);

    SOSCircleForEachValidSyncingPeer(trust.trustedCircle, account.accountKey, ^(SOSPeerInfoRef peer) {
        if (!SOSPeerInfoPeerIDEqual(peer, my_id)) {
            CFErrorRef localError = NULL;
            
            SOSEngineInitializePeerCoder((SOSEngineRef)[account.kvs_message_transport SOSTransportMessageGetEngine], trust.fullPeerInfo, peer, &localError);
            if (localError)
                secnotice("updates", "can't initialize transport for peer %@ with %@ (%@)", peer, trust.fullPeerInfo, localError);
            CFReleaseNull(localError);
        }
    });
    
    return result;
}

//
// Value manipulation
//

CFTypeRef SOSAccountGetValue(SOSAccount*  account, CFStringRef key, CFErrorRef *error) {
    SOSAccountTrustClassic *trust = account.trust;
    if (!trust.expansion) {
        return NULL;
    }
    return (__bridge CFTypeRef)([trust.expansion objectForKey:(__bridge NSString* _Nonnull)(key)]);
}

bool SOSAccountAddEscrowRecords(SOSAccount*  account, CFStringRef dsid, CFDictionaryRef record, CFErrorRef *error){
    CFMutableDictionaryRef escrowRecords = (CFMutableDictionaryRef)SOSAccountGetValue(account, kSOSEscrowRecord, error);
    CFMutableDictionaryRef escrowCopied = NULL;
    bool success = false;
    
    if(isDictionary(escrowRecords) && escrowRecords != NULL)
        escrowCopied = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(escrowRecords), escrowRecords);
    else
        escrowCopied = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionaryAddValue(escrowCopied, dsid, record);
    SOSAccountSetValue(account, kSOSEscrowRecord, escrowCopied, error);
  
    if(*error == NULL)
        success = true;
    
    CFReleaseNull(escrowCopied);
    
    return success;
    
}

bool SOSAccountAddEscrowToPeerInfo(SOSAccount*  account, SOSFullPeerInfoRef myPeer, CFErrorRef *error){
    bool success = false;
    
    CFDictionaryRef escrowRecords = SOSAccountGetValue(account, kSOSEscrowRecord, error);
    success = SOSFullPeerInfoReplaceEscrowRecords(myPeer, escrowRecords, error);
    
    return success;
}

- (void)_onQueueRecordRetiredPeersInCircle {

    dispatch_assert_queue(self.queue);

    if (![self isInCircle:NULL]) {
        return;
    }
    __block bool updateRings = false;
    SOSAccountTrustClassic *trust = self.trust;
    [trust modifyCircle:self.circle_transport err:NULL action:^bool (SOSCircleRef circle) {
        __block bool updated = false;
        CFSetForEach((__bridge CFMutableSetRef)trust.retirees, ^(CFTypeRef element){
            SOSPeerInfoRef retiree = asSOSPeerInfo(element);

            if (retiree && SOSCircleUpdatePeerInfo(circle, retiree)) {
                updated = true;
                secnotice("retirement", "Updated retired peer %@ in %@", retiree, circle);
                CFErrorRef cleanupError = NULL;
                if (![self.trust cleanupAfterPeer:self.kvs_message_transport circleTransport:self.circle_transport seconds:RETIREMENT_FINALIZATION_SECONDS circle:circle cleanupPeer:retiree err:&cleanupError])
                    secerror("Error cleanup up after peer (%@): %@", retiree, cleanupError);
                CFReleaseSafe(cleanupError);
                updateRings = true;
            }
        });
        return updated;
    }];
    if(updateRings) {
        SOSAccountProcessBackupRings(self, NULL);
    }
}

static const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;

static CFDictionaryRef SOSAccountGetObjectsFromCloud(dispatch_queue_t processQueue, CFErrorRef *error)
{
    __block CFTypeRef object = NULL;
    
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
        
    CloudKeychainReplyBlock replyBlock =
    ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        object = returnedValues;
        if (object)
            CFRetain(object);
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
        }
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    SOSCloudKeychainGetAllObjectsFromCloud(processQueue, replyBlock);
    
    dispatch_semaphore_wait(waitSemaphore, finishTime);
    if (object && (CFGetTypeID(object) == CFNullGetTypeID()))   // return a NULL instead of a CFNull
    {
        CFRelease(object);
        object = NULL;
    }
    return asDictionary(object, NULL); // don't propogate "NULL is not a dictionary" errors
}


static void SOSAccountRemoveKVSKeys(SOSAccount* account, NSArray* keysToRemove, dispatch_queue_t processQueue)
{
    CFStringRef uuid = SOSAccountCopyUUID(account);
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error){
        if (error){
            secerror("SOSCloudKeychainRemoveKeys returned error: %@", error);
        }
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    SOSCloudKeychainRemoveKeys((__bridge CFArrayRef)(keysToRemove), uuid, processQueue, replyBlock);
    dispatch_semaphore_wait(waitSemaphore, finishTime);
    CFReleaseNull(uuid);
}

static void SOSAccountWriteLastCleanupTimestampToKVS(SOSAccount* account)
{
    NSDate *now = [NSDate date];
    [account.settings setObject:now forKey:SOSAccountLastKVSCleanup];
    
    NSMutableDictionary *writeTimestamp = [NSMutableDictionary dictionary];
        
    CFMutableStringRef timeDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));

    CFAbsoluteTime currentTimeAndDate = CFAbsoluteTimeGetCurrent();
    
    withStringOfAbsoluteTime(currentTimeAndDate, ^(CFStringRef decription) {
        CFStringAppend(timeDescription, decription);
    });
    CFStringAppend(timeDescription, CFSTR("]"));
    
    [writeTimestamp setObject:(__bridge NSString*)(timeDescription) forKey:(__bridge NSString*)kSOSKVSLastCleanupTimestampKey];
    CFReleaseNull(timeDescription);
    
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    
    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error){
        if (error){
            secerror("SOSCloudKeychainPutObjectsInCloud returned error: %@", error);
        }
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    SOSCloudKeychainPutObjectsInCloud((__bridge CFDictionaryRef)(writeTimestamp), processQueue, replyBlock);
    dispatch_semaphore_wait(waitSemaphore, finishTime);
}

// set the cleanup frequency to 3 days.
#define KVS_CLEANUP_FREQUENCY_LIMIT 60*60*24*3

//Get all the key/values in KVS and remove old entries
bool SOSAccountCleanupAllKVSKeys(SOSAccount* account, CFErrorRef* error)
{
    // This should only happen on some number of days
    NSDate *lastKVSCleanup = [account.settings objectForKey:SOSAccountLastKVSCleanup];
    NSDate *now = [NSDate date];
    NSTimeInterval timeSinceCleanup = [now timeIntervalSinceDate:lastKVSCleanup];

    if(timeSinceCleanup < KVS_CLEANUP_FREQUENCY_LIMIT) {
        return true;
    }

    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    
    NSDictionary *keysAndValues = (__bridge_transfer NSDictionary*)SOSAccountGetObjectsFromCloud(processQueue, error);
    NSMutableArray *peerIDs = [NSMutableArray array];
    NSMutableArray *keysToRemove = [NSMutableArray array];

    CFArrayRef peers = SOSAccountCopyActiveValidPeers(account, error);
    CFArrayForEach(peers, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
        NSString* peerID = (__bridge NSString*) SOSPeerInfoGetPeerID(peer);
        
        //any peerid that is not ours gets added
        if(![[account.trust peerID] isEqualToString:peerID])
            [peerIDs addObject:peerID];
    });
    CFReleaseNull(peers);

    [keysAndValues enumerateKeysAndObjectsUsingBlock:^(NSString * KVSKey, NSNumber * KVSValue, BOOL *stop) {
        __block bool keyMatchesPeerID = false;
        
        //checks for full peer ids
        [peerIDs enumerateObjectsUsingBlock:^(id  _Nonnull PeerID, NSUInteger idx, BOOL * _Nonnull stop) {
            //if key contains peerid of one active peer
            if([KVSKey containsString:PeerID]){
                secnotice("key-cleanup","key: %@", KVSKey);
                keyMatchesPeerID = true;
            }
        }];
        if((([KVSKey hasPrefix:@"ak"] || [KVSKey hasPrefix:@"-ak"]) && !keyMatchesPeerID)
           || [KVSKey hasPrefix:@"po"])
            [keysToRemove addObject:KVSKey];
    }];
    
    secnotice("key-cleanup", "message keys that we should remove! %@", keysToRemove);
    secnotice("key-cleanup", "total keys: %lu, cleaning up %lu", (unsigned long)[keysAndValues count], (unsigned long)[keysToRemove count]);
    
    SOSAccountRemoveKVSKeys(account, keysToRemove, processQueue);
    
    //add last cleanup timestamp
    SOSAccountWriteLastCleanupTimestampToKVS(account);
    return true;
    
}

bool SOSAccountPopulateKVSWithBadKeys(SOSAccount*  account, CFErrorRef* error) {
    
    NSMutableDictionary *testKeysAndValues = [NSMutableDictionary dictionary];
    [testKeysAndValues setObject:@"deadbeef" forKey:@"-ak|asdfjkl;asdfjk;"];
    [testKeysAndValues setObject:@"foobar" forKey:@"ak|asdfasdfasdf:qwerqwerqwer"];
    [testKeysAndValues setObject:@"oldhistorycircle" forKey:@"poak|asdfasdfasdfasdf"];
    [testKeysAndValues setObject:@"oldhistorycircle" forKey:@"po|asdfasdfasdfasdfasdfasdf"];
    [testKeysAndValues setObject:@"oldhistorycircle" forKey:@"k>KeyParm"];
    
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error){
        if (error){
            secerror("SOSCloudKeychainPutObjectsInCloud returned error: %@", error);
        }
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    SOSCloudKeychainPutObjectsInCloud((__bridge CFDictionaryRef)(testKeysAndValues), processQueue, replyBlock);
    dispatch_semaphore_wait(waitSemaphore, finishTime);

    return true;
}

SOSPeerInfoRef SOSAccountCopyApplication(SOSAccount*  account, CFErrorRef* error) {
    SOSPeerInfoRef applicant = NULL;
    SOSAccountTrustClassic *trust = account.trust;
    SecKeyRef userKey = SOSAccountGetPrivateCredential(account, error);
    if(!userKey) return false;
    if(![trust ensureFullPeerAvailable:(__bridge CFDictionaryRef)(account.gestalt) deviceID:(__bridge CFStringRef)(account.deviceID) backupKey:(__bridge CFDataRef)(account.backup_key) err:error])
        return applicant;
    if(!SOSFullPeerInfoPromoteToApplication(trust.fullPeerInfo, userKey, error))
        return applicant;
    applicant = SOSPeerInfoCreateCopy(kCFAllocatorDefault, trust.peerInfo, error);

    return applicant;
}


static void
AddStrippedResults(NSMutableArray *results, NSArray *keychainItems, NSMutableSet *seenUUID, bool authoriative)
{
    [keychainItems enumerateObjectsUsingBlock:^(NSDictionary* keychainItem, NSUInteger idx, BOOL * _Nonnull stop) {
        NSString* parentUUID = keychainItem[(id)kSecAttrPath];
        NSString* viewUUID = keychainItem[(id)kSecAttrAccount];
        NSString *viewName = [keychainItem objectForKey:(id)kSecAttrServer];

        if (parentUUID == NULL || viewUUID == NULL || viewName == NULL)
            return;

        if([parentUUID isEqualToString:viewUUID] || authoriative){

            /* check if we already have this entry */
            if ([seenUUID containsObject:viewUUID])
                return;

            NSData* v_data = [keychainItem objectForKey:(id)kSecValueData];
            NSData *key = [[NSData alloc] initWithBase64EncodedData:v_data options:0];

            if (key == NULL)
                return;

            secnotice("piggy", "fetched TLK %@ with name %@", viewName, viewUUID);

            NSMutableDictionary* strippedDown = [@{
                (id)kSecValueData : key,
                (id)kSecAttrServer : viewName,
                (id)kSecAttrAccount : viewUUID
            } mutableCopy];
            if (authoriative)
                strippedDown[@"auth"] = @YES;

            [results addObject:strippedDown];
            [seenUUID addObject:viewUUID];
        }
    }];
}

static void
AddViewManagerResults(NSMutableArray *results, NSMutableSet *seenUUID)
{
#if OCTAGON
    CKKSViewManager* manager = [CKKSViewManager manager];

    NSDictionary<NSString *,NSString *> *items = [manager activeTLKs];

    for (NSString *view in items) {
        NSString *uuid = items[view];
        NSDictionary *query = @{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecUseDataProtectionKeychain : @YES,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrAccount : uuid,
            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
            (id)kSecMatchLimit : (id)kSecMatchLimitAll,
            (id)kSecReturnAttributes: @YES,
            (id)kSecReturnData: @YES,
        };
        CFTypeRef result = NULL;
        if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) == 0) {
            AddStrippedResults(results, (__bridge NSArray*)result, seenUUID, true);
        }
        CFReleaseNull(result);
    }
#endif
}


NSArray<NSDictionary *>*
SOSAccountGetAllTLKs(void)
{
    CFTypeRef result = NULL;
    NSMutableArray<NSDictionary *>* results = [NSMutableArray array];
    NSMutableSet *seenUUID = [NSMutableSet set];

    // first use the TLK from the view manager
    AddViewManagerResults(results, seenUUID);

    //try to grab tlk-piggy items
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrDescription: @"tlk",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    };

    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) == 0) {
        AddStrippedResults(results, (__bridge NSArray*)result, seenUUID, false);
    }
    CFReleaseNull(result);

    //try to grab tlk-piggy items
    query = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrDescription: @"tlk-piggy",
        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    };

    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) == 0) {
        AddStrippedResults(results, (__bridge NSArray*)result, seenUUID, false);
    }
    CFReleaseNull(result);

    secnotice("piggy", "Found %d TLKs", (int)[results count]);

    return results;
}

static uint8_t* encode_tlk(kTLKTypes type, NSString *name, NSData *keychainData, NSData* uuid,
                           const uint8_t *der, uint8_t *der_end)
{
    if (type != kTLKUnknown) {
        return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                           piggy_encode_data(keychainData, der,
                                                             piggy_encode_data(uuid, der,
                                                                               ccder_encode_uint64((uint64_t)type, der, der_end))));
    } else {
        return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                           piggy_encode_data(keychainData, der,
                                                             piggy_encode_data(uuid, der,
                                                                               der_encode_string((__bridge CFStringRef)name, NULL, der, der_end))));
    }
}

static uint8_t* piggy_encode_data(NSData* data,
                                  const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_tl(CCDER_OCTET_STRING, data.length, der,
                           ccder_encode_body(data.length, data.bytes, der, der_end));
    
}

// you can not add more items here w/o also adding a version, older clients wont understand newer numbers
static kTLKTypes
name2type(NSString *view)
{
    if ([view isEqualToString:@"Manatee"])
        return kTLKManatee;
    else if ([view isEqualToString:@"Engram"])
        return kTLKEngram;
    else if ([view isEqualToString:@"AutoUnlock"])
        return kTLKAutoUnlock;
    if ([view isEqualToString:@"Health"])
        return kTLKHealth;
    return kTLKUnknown;
}

// you can not add more items here w/o also adding a version, older clients wont understand newer numbers
static unsigned
rank_type(NSString *view)
{
    if ([view isEqualToString:@"Manatee"])
        return 5;
    else if ([view isEqualToString:@"Engram"])
        return 4;
    else if ([view isEqualToString:@"AutoUnlock"])
        return 3;
    if ([view isEqualToString:@"Health"])
        return 2;
    return 0;
}

static NSData *
parse_uuid(NSString *uuidString)
{
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    uuid_t uuidblob;
    [uuid getUUIDBytes:uuidblob];
    return [NSData dataWithBytes:uuidblob length:sizeof(uuid_t)];
}
static size_t
piggy_sizeof_data(NSData* data)
{
    return ccder_sizeof(CCDER_OCTET_STRING, [data length]);
}

static size_t sizeof_keychainitem(kTLKTypes type, NSString *name, NSData* keychainData, NSData* uuid) {
    if (type != kTLKUnknown) {
        return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                            piggy_sizeof_data(keychainData) +
                            piggy_sizeof_data(uuid) +
                            ccder_sizeof_uint64(type));
    } else {
        return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                            piggy_sizeof_data(keychainData) +
                            piggy_sizeof_data(uuid) +
                            der_sizeof_string((__bridge CFStringRef)name, NULL));
    }
}

NSArray<NSDictionary*>*
SOSAccountSortTLKS(NSArray<NSDictionary*>* tlks)
{
    NSMutableArray<NSDictionary*>* sortedTLKs = [tlks mutableCopy];

    [sortedTLKs sortUsingComparator:^NSComparisonResult(NSDictionary *obj1, NSDictionary *obj2) {
        unsigned rank1 = rank_type(obj1[(__bridge id)kSecAttrServer]);
        if (obj1[@"auth"] != NULL)
            rank1 += 1000;
        unsigned rank2 = rank_type(obj2[(__bridge id)kSecAttrServer]);
        if (obj2[@"auth"] != NULL)
            rank2 += 1000;

        /*
         * Sort by rank (higher better), but prefer TLK that are authoriative (ie used by CKKSViewManager),
         * since we are sorting backward, the Ascending/Descending looks wrong below.
         */
        if (rank1 > rank2) {
            return NSOrderedAscending;
        } else if (rank1 < rank2) {
            return NSOrderedDescending;
        }
        return NSOrderedSame;
    }];

    return sortedTLKs;
}

static NSArray<NSData *> *
build_tlks(NSArray<NSDictionary*>* tlks)
{
    NSMutableArray *array = [NSMutableArray array];
    NSArray<NSDictionary*>* sortedTLKs = SOSAccountSortTLKS(tlks);

    for (NSDictionary *item in sortedTLKs) {
        NSData* keychainData = item[(__bridge id)kSecValueData];
        NSString* name = item[(__bridge id)kSecAttrServer];
        NSString *uuidString = item[(__bridge id)kSecAttrAccount];
        NSData* uuid = parse_uuid(uuidString);

        NSMutableData *tlk = [NSMutableData dataWithLength:sizeof_keychainitem(name2type(name), name, keychainData, uuid)];

        unsigned char *der = [tlk mutableBytes];
        unsigned char *der_end = der + [tlk length];

        if (encode_tlk(name2type(name), name, keychainData, uuid, der, der_end) == NULL)
            return NULL;

        secnotice("piggy", "preparing TLK in order: %@: %@", name, uuidString);

        [array addObject:tlk];
    }
    return array;
}

static NSArray<NSData *> *
build_identities(NSArray<NSData *>* identities)
{
    NSMutableArray *array = [NSMutableArray array];
    for (NSData *item in identities) {
        NSMutableData *ident = [NSMutableData dataWithLength:ccder_sizeof_raw_octet_string([item length])];

        unsigned char *der = [ident mutableBytes];
        unsigned char *der_end = der + [ident length];

        ccder_encode_raw_octet_string([item length], [item bytes], der, der_end);
        [array addObject:ident];
    }
    return array;
}



static unsigned char *
encode_data_array(NSArray<NSData*>* data, unsigned char *der, unsigned char *der_end)
{
    unsigned char *body_end = der_end;
    for (NSData *datum in data) {
        der_end = ccder_encode_body([datum length], [datum bytes], der, der_end);
        if (der_end == NULL)
            return NULL;
    }
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, body_end, der, der_end);
}

static size_t sizeof_piggy(size_t identities_size, size_t tlk_size)
{
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, identities_size) +
                        ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, tlk_size));
}

static NSData *encode_piggy(size_t IdentitiesBudget,
                            size_t TLKBudget,
                            NSArray<NSData*>* identities,
                            NSArray<NSDictionary*>* tlks)
{
    NSArray<NSData *> *encodedTLKs = build_tlks(tlks);
    NSArray<NSData *> *encodedIdentities = build_identities(identities);
    NSMutableArray<NSData *> *budgetArray = [NSMutableArray array];
    NSMutableArray<NSData *> *identitiesArray = [NSMutableArray array];
    size_t payloadSize = 0, identitiesSize = 0;
    NSMutableData *result = NULL;

    for (NSData *tlk in encodedTLKs) {
        if (TLKBudget - payloadSize < [tlk length])
            break;
        [budgetArray addObject:tlk];
        payloadSize += tlk.length;
    }
    secnotice("piggy", "sending %d tlks", (int)budgetArray.count);

    for (NSData *ident in encodedIdentities) {
        if (IdentitiesBudget - identitiesSize < [ident length])
            break;
        [identitiesArray addObject:ident];
        identitiesSize += ident.length;
    }
    secnotice("piggy", "sending %d identities", (int)identitiesArray.count);


    size_t piggySize = sizeof_piggy(identitiesSize, payloadSize);

    result = [NSMutableData dataWithLength:piggySize];

    unsigned char *der = [result mutableBytes];
    unsigned char *der_end = der + [result length];

    if (ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                    encode_data_array(identitiesArray, der,
                                    encode_data_array(budgetArray, der, der_end))) != [result mutableBytes])
        return NULL;

    return result;
}

static const size_t SOSCCIdentitiesBudget = 120;
static const size_t SOSCCTLKBudget = 500;

NSData *
SOSPiggyCreateInitialSyncData(NSArray<NSData*>* identities, NSArray<NSDictionary *>* tlks)
{
    return encode_piggy(SOSCCIdentitiesBudget, SOSCCTLKBudget, identities, tlks);
}

CF_RETURNS_RETAINED CFMutableArrayRef SOSAccountCopyiCloudIdentities(SOSAccount* account)
{
    CFMutableArrayRef identities = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSCircleForEachActivePeer(account.trust.trustedCircle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            CFArrayAppendValue(identities, peer);
        }
    });
    return identities;
}

CFDataRef SOSAccountCopyInitialSyncData(SOSAccount* account, SOSInitialSyncFlags flags, CFErrorRef *error) {

    NSMutableArray<NSData *>* encodedIdenities = [NSMutableArray array];
    NSArray<NSDictionary *>* tlks = nil;

    if (flags & kSOSInitialSyncFlagiCloudIdentity) {
        CFMutableArrayRef identities = SOSAccountCopyiCloudIdentities(account);
        secnotice("piggy", "identities: %@", identities);

        CFIndex i, count = CFArrayGetCount(identities);
        for (i = 0; i < count; i++) {
            SOSPeerInfoRef fpi = (SOSPeerInfoRef)CFArrayGetValueAtIndex(identities, i);
            NSData *data = CFBridgingRelease(SOSPeerInfoCopyData(fpi, error));
            if (data)
                [encodedIdenities addObject:data];
        }
        CFRelease(identities);
    }

    if (flags & kSOSInitialSyncFlagTLKs) {
        tlks = SOSAccountGetAllTLKs();
    }

    return CFBridgingRetain(SOSPiggyCreateInitialSyncData(encodedIdenities, tlks));
}

static void pbNotice(CFStringRef operation, SOSAccount*  account, SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, PiggyBackProtocolVersion version) {
    CFStringRef pkeyID = SOSCopyIDOfKey(pubKey, NULL);
    if(pkeyID == NULL) pkeyID = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("Unknown"));
    CFStringRef sigID = SOSCopyIDOfDataBuffer(signature, NULL);
    if(sigID == NULL) sigID = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("No Signature"));
    CFStringRef accountName = SOSAccountGetValue(account, kSOSAccountName, NULL);
    if(accountName == NULL) {
        accountName = CFSTR("Unavailable");
    }
    CFStringRef circleHash = SOSCircleCopyHashString(account.trust.trustedCircle);

    secnotice("circleOps",
              "%@: Joining blob for account: %@ for piggyback (V%d) gencount: %@  pubkey: %@ signatureID: %@  starting circle hash: %@",
              operation, accountName, version, gencount, pkeyID, sigID, circleHash);
    CFReleaseNull(pkeyID);
    CFReleaseNull(sigID);
    CFReleaseNull(circleHash);
}

CFDataRef SOSAccountCopyCircleJoiningBlob(SOSAccount*  account, SOSPeerInfoRef applicant, CFErrorRef *error) {
    SOSGenCountRef gencount = NULL;
    CFDataRef signature = NULL;
    SecKeyRef ourKey = NULL;

    CFDataRef pbblob = NULL;
    SOSCircleRef prunedCircle = NULL;

	secnotice("circleOps", "Making circle joining piggyback blob as sponsor (SOSAccountCopyCircleJoiningBlob)");

    SOSCCStatus circleStat = [account getCircleStatus:error];
    if(circleStat != kSOSCCInCircle) {
        secnotice("circleOps", "Invalid circle status: %@ to accept piggyback as sponsor (SOSAccountCopyCircleJoiningBlob)", SOSCCGetStatusDescription(circleStat));
        return NULL;
    }

    SecKeyRef userKey = SOSAccountGetTrustedPublicCredential(account, error);
    require_quiet(userKey, errOut);

    require_action_quiet(applicant, errOut, SOSCreateError(kSOSErrorProcessingFailure, CFSTR("No applicant provided"), (error != NULL) ? *error : NULL, error));
    require_action_quiet(SOSPeerInfoApplicationVerify(applicant, userKey, error), errOut,
                         secnotice("circleOps", "Peer application wasn't signed with the correct userKey"));

    {
        SOSFullPeerInfoRef fpi = account.fullPeerInfo;
        ourKey = SOSFullPeerInfoCopyDeviceKey(fpi, error);
        require_quiet(ourKey, errOut);
    }

    SOSCircleRef currentCircle = [account.trust getCircle:error];
    require_quiet(currentCircle, errOut);

    prunedCircle = SOSCircleCopyCircle(NULL, currentCircle, error);
    require_quiet(prunedCircle, errOut);
    require_quiet(SOSCirclePreGenerationSign(prunedCircle, userKey, error), errOut);

    gencount = SOSGenerationIncrementAndCreate(SOSCircleGetGeneration(prunedCircle));

    signature = SOSCircleCopyNextGenSignatureWithPeerAdded(prunedCircle, applicant, ourKey, error);
    require_quiet(signature, errOut);
    pbNotice(CFSTR("Accepting"), account, gencount, ourKey, signature, kPiggyV1);
    pbblob = SOSPiggyBackBlobCopyEncodedData(gencount, ourKey, signature, error);
    
errOut:
    CFReleaseNull(prunedCircle);
    CFReleaseNull(gencount);
    CFReleaseNull(signature);
    CFReleaseNull(ourKey);

	if(!pbblob && error != NULL) {
		secnotice("circleOps", "Failed to make circle joining piggyback blob as sponsor %@", *error);
	}

    return pbblob;
}

bool SOSAccountJoinWithCircleJoiningBlob(SOSAccount*  account, CFDataRef joiningBlob, PiggyBackProtocolVersion version, CFErrorRef *error) {
    bool retval = false;
    SecKeyRef userKey = NULL;
    SOSAccountTrustClassic *trust = account.trust;
    SOSGenCountRef gencount = NULL;
    CFDataRef signature = NULL;
    SecKeyRef pubKey = NULL;
    bool setInitialSyncTimeoutToV0 = false;
    
    secnotice("circleOps", "Joining circles through piggyback (SOSAccountCopyCircleJoiningBlob)");

    if (!isData(joiningBlob)) {
        secnotice("circleOps", "Bad data blob: piggyback (SOSAccountCopyCircleJoiningBlob)");
        return false;
    }

    userKey = SOSAccountGetPrivateCredential(account, error);
    if(!userKey) {
        secnotice("circleOps", "Failed - no private credential %@: piggyback (SOSAccountCopyCircleJoiningBlob)", *error);
        return retval;
    }

    if (!SOSPiggyBackBlobCreateFromData(&gencount, &pubKey, &signature, joiningBlob, version, &setInitialSyncTimeoutToV0, error)) {
        secnotice("circleOps", "Failed - decoding blob %@: piggyback (SOSAccountCopyCircleJoiningBlob)", *error);
        return retval;
    }

    if(setInitialSyncTimeoutToV0){
        secnotice("circleOps", "setting flag in account for piggyback v0");
        SOSAccountSetValue(account, kSOSInitialSyncTimeoutV0, kCFBooleanTrue, NULL);
    } else {
        secnotice("circleOps", "clearing flag in account for piggyback v0");
        SOSAccountClearValue(account, kSOSInitialSyncTimeoutV0, NULL);
    }
    SOSAccountInitializeInitialSync(account);

    pbNotice(CFSTR("Joining"), account, gencount, pubKey, signature, version);

    retval = [trust modifyCircle:account.circle_transport err:error action:^bool(SOSCircleRef copyOfCurrent) {
        return SOSCircleAcceptPeerFromHSA2(copyOfCurrent, userKey,
                                           gencount,
                                           pubKey,
                                           signature,
                                           trust.fullPeerInfo, error);;
    }];
    
    CFReleaseNull(gencount);
    CFReleaseNull(pubKey);
    CFReleaseNull(signature);

    return retval;
}

static char boolToChars(bool val, char truechar, char falsechar) {
    return val? truechar: falsechar;
}

#define ACCOUNTLOGSTATE "accountLogState"
void SOSAccountLogState(SOSAccount*  account) {
    bool hasPubKey = account.accountKey != NULL;
    SOSAccountTrustClassic *trust = account.trust;
    bool pubTrusted = account.accountKeyIsTrusted;
    bool hasPriv = account.accountPrivateKey != NULL;
    SOSCCStatus stat = [account getCircleStatus:NULL];
    
    CFStringRef userPubKeyID =  (account.accountKey) ? SOSCopyIDOfKeyWithLength(account.accountKey, 8, NULL):
            CFStringCreateCopy(kCFAllocatorDefault, CFSTR("*No Key*"));

    secnotice(ACCOUNTLOGSTATE, "Start");

    secnotice(ACCOUNTLOGSTATE, "ACCOUNT: [keyStatus: %c%c%c hpub %@] [SOSCCStatus: %@]",
              boolToChars(hasPubKey, 'U', 'u'), boolToChars(pubTrusted, 'T', 't'), boolToChars(hasPriv, 'I', 'i'),
              userPubKeyID,
              SOSAccountGetSOSCCStatusString(stat)
              );
    CFReleaseNull(userPubKeyID);
    if(trust.trustedCircle)  SOSCircleLogState(ACCOUNTLOGSTATE, trust.trustedCircle, account.accountKey, (__bridge CFStringRef)(account.peerID));
    else secnotice(ACCOUNTLOGSTATE, "ACCOUNT: No Circle");
}

void SOSAccountLogViewState(SOSAccount*  account) {
    bool isInCircle = [account.trust isInCircleOnly:NULL];
    require_quiet(isInCircle, imOut);
    SOSPeerInfoRef mpi = account.peerInfo;
    bool isInitialComplete = SOSAccountHasCompletedInitialSync(account);
    bool isBackupComplete = SOSAccountHasCompletedRequiredBackupSync(account);

    CFSetRef views = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;
    CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
        secnotice(ACCOUNTLOGSTATE, "Sync: %c%c PeerViews: %@",
                  boolToChars(isInitialComplete, 'I', 'i'),
                  boolToChars(isBackupComplete, 'B', 'b'),
                  description);
    });
    CFReleaseNull(views);
    CFSetRef unsyncedViews = SOSAccountCopyOutstandingViews(account);
    CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
        secnotice(ACCOUNTLOGSTATE, "outstanding views: %@", description);
    });
    CFReleaseNull(unsyncedViews);

imOut:
    secnotice(ACCOUNTLOGSTATE, "Finish");

    return;
}


void SOSAccountSetTestSerialNumber(SOSAccount*  account, CFStringRef serial) {
    if(!isString(serial)) return;
    CFMutableDictionaryRef newv2dict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(newv2dict, sSerialNumberKey, serial);
    [account.trust updateV2Dictionary:account v2:newv2dict];
}

void SOSAccountResetOTRNegotiationCoder(SOSAccount* account, CFStringRef peerid)
{
    secnotice("otrtimer", "timer fired!");
    CFErrorRef error = NULL;
    SecADAddValueForScalarKey((__bridge CFStringRef) SecSOSAggdReattemptOTRNegotiation,1);

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account.factory, SOSCircleGetName(account.trust.trustedCircle), NULL);
    SOSEngineWithPeerID(engine, peerid, &error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        if(SOSCoderIsCoderInAwaitingState(coder)){
            secnotice("otrtimer", "coder is in awaiting state, restarting coder");
            CFErrorRef localError = NULL;
            SOSCoderReset(coder);
            if(SOSCoderStart(coder, &localError) == kSOSCoderFailure){
                secerror("Attempt to recover coder failed to restart: %@", localError);
            }
            else{
                secnotice("otrtimer", "coder restarted!");
                SOSEngineSetCodersNeedSaving(engine, true);
                SOSPeerSetMustSendMessage(peer, true);
                SOSCCRequestSyncWithPeer(SOSPeerGetID(peer));
            }
            SOSPeerOTRTimerIncreaseOTRNegotiationRetryCount(account, (__bridge NSString*)SOSPeerGetID(peer));
            SOSPeerRemoveOTRTimerEntry(peer);
            SOSPeerOTRTimerRemoveRTTTimeoutForPeer(account,  (__bridge NSString*)SOSPeerGetID(peer));
            SOSPeerOTRTimerRemoveLastSentMessageTimestamp(account, (__bridge NSString*)SOSPeerGetID(peer));
        }
        else{
            secnotice("otrtimer", "time fired but out of negotiation! Not restarting coder");
        }
    });
    if(error)
    {
        secnotice("otrtimer","error grabbing engine for peer id: %@, error:%@", peerid, error);
    }
    CFReleaseNull(error);
}

void SOSAccountTimerFiredSendNextMessage(SOSAccountTransaction* txn, NSString* peerid, NSString* accessGroup)
{
    __block SOSAccount* account = txn.account;
    CFErrorRef error = NULL;
    
    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(txn.account.factory, SOSCircleGetName(account.trust.trustedCircle), NULL);
    SOSEngineWithPeerID(engine, (__bridge CFStringRef)peerid, &error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        
        NSString *peer_id = (__bridge NSString*)SOSPeerGetID(peer);
        PeerRateLimiter *limiter = (__bridge PeerRateLimiter*)SOSPeerGetRateLimiter(peer);
        CFErrorRef error = NULL;
        NSData* message = [limiter.accessGroupToNextMessageToSend objectForKey:accessGroup];
        
        if(message){
            secnotice("ratelimit","SOSPeerRateLimiter timer went off! sending:%@ \n to peer:%@", message, peer_id);
            bool sendResult = [account.kvs_message_transport SOSTransportMessageSendMessage:account.kvs_message_transport id:(__bridge CFStringRef)peer_id messageToSend:(__bridge CFDataRef)message err:&error];
            
            if(!sendResult || error){
                secnotice("ratelimit", "could not send message: %@", error);
            }
        }
        [limiter.accessGroupRateLimitState setObject:[[NSNumber alloc]initWithLong:RateLimitStateCanSend] forKey:accessGroup];
        [limiter.accessGroupToTimer removeObjectForKey:accessGroup];
        [limiter.accessGroupToNextMessageToSend removeObjectForKey:accessGroup];
    });
    
    if(error)
    {
        secnotice("otrtimer","error grabbing engine for peer id: %@, error:%@", peerid, error);
    }
    CFReleaseNull(error);
}

#if OCTAGON

/*
 * State machine
 */

OctagonFlag* SOSFlagTriggerBackup = (OctagonFlag*)@"trigger_backup";
OctagonFlag* SOSFlagTriggerRingUpdate = (OctagonFlag*)@"trigger_ring_update";

OctagonState* SOSStateReady = (OctagonState*)@"ready";
OctagonState* SOSStateError = (OctagonState*)@"error";
OctagonState* SOSStatePerformBackup = (OctagonState*)@"perform_backup";
OctagonState* SOSStatePerformRingUpdate = (OctagonState*)@"perform_ring_update";

static NSDictionary<OctagonState*, NSNumber*>* SOSStateMap(void) {
    static NSDictionary<OctagonState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
            SOSStateReady:                              @0U,
            SOSStateError:                              @1U,
            SOSStatePerformBackup:                      @2U,
            SOSStatePerformRingUpdate:                  @3U,
        };
    });
    return map;
}

static NSSet<OctagonFlag*>* SOSFlagsSet(void) {
    static NSSet<OctagonFlag*>* set = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        set = [NSSet setWithArray:@[
            SOSFlagTriggerBackup,
            SOSFlagTriggerRingUpdate,
        ]];
    });
    return set;
}



+ (NSURL *)urlForSOSAccountSettings {
    return (__bridge NSURL *)SecCopyURLForFileInKeychainDirectory(CFSTR("SOSAccountSettings.pb"));
}


- (void)setupStateMachine {
    WEAKIFY(self);

    self.accountConfiguration = [[CKKSPBFileStorage alloc] initWithStoragePath:[[self class] urlForSOSAccountSettings]
                                                                   storageClass:[SOSAccountConfiguration class]];

    NSAssert(self.stateMachine == nil, @"cant bootstrap more the once");

    self.stateMachineQueue = dispatch_queue_create("SOS-statemachine", NULL);

    self.stateMachine = [[OctagonStateMachine alloc] initWithName:@"sosaccount"
                                                           states:[NSSet setWithArray:[SOSStateMap() allKeys]]
                                                            flags:SOSFlagsSet()
                                                     initialState:SOSStateReady
                                                            queue:self.stateMachineQueue
                                                      stateEngine:self
                                                 lockStateTracker:[CKKSLockStateTracker globalTracker]];


    self.performBackups = [[CKKSNearFutureScheduler alloc] initWithName:@"performBackups"
                                                           initialDelay:5*NSEC_PER_SEC
                                                        continuingDelay:30*NSEC_PER_SEC
                                                       keepProcessAlive:YES
                                              dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                  block:^{
        STRONGIFY(self);
        [self addBackupFlag];
    }];

    self.performRingUpdates = [[CKKSNearFutureScheduler alloc] initWithName:@"performRingUpdates"
                                                               initialDelay:5*NSEC_PER_SEC
                                                           expontialBackoff:2.0
                                                               maximumDelay:15*60*NSEC_PER_SEC
                                                           keepProcessAlive:YES
                                                  dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                      block:^{
        STRONGIFY(self);
        [self addRingUpdateFlag];
    }];

    SOSAccountConfiguration *conf = self.accountConfiguration.storage;

    if (conf.pendingBackupPeers.count) {
        [self addBackupFlag];
    }
    if (conf.ringUpdateFlag) {
        [self addRingUpdateFlag];
    }
}


/*
 * Flag adding to state machine
 */

- (void)addBackupFlag {
    OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:SOSFlagTriggerBackup
                                                                    conditions:OctagonPendingConditionsDeviceUnlocked];
    [self.stateMachine handlePendingFlag:pendingFlag];
}

- (void)addRingUpdateFlag {
    OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:SOSFlagTriggerRingUpdate
                                                                    conditions:OctagonPendingConditionsDeviceUnlocked];
    [self.stateMachine handlePendingFlag:pendingFlag];
}

//Mark: -- Set up state for state machine


- (void)triggerBackupForPeers:(NSArray<NSString*>*)backupPeers
{
    NSMutableSet *pending = [NSMutableSet set];
    if (backupPeers) {
        [pending addObjectsFromArray:backupPeers];
    }

    WEAKIFY(self);
    dispatch_async(self.stateMachineQueue, ^{
        STRONGIFY(self);

        SOSAccountConfiguration *storage = self.accountConfiguration.storage;

        if (storage.pendingBackupPeers) {
            [pending addObjectsFromArray:storage.pendingBackupPeers];
        }
        storage.pendingBackupPeers = [[pending allObjects] mutableCopy];
        [self.accountConfiguration setStorage:storage];
        [self.performBackups trigger];
        secnotice("sos-sm", "trigger backup for peers: %@ at %@",
                  backupPeers, self.performBackups.nextFireTime);
    });
}

- (void)triggerRingUpdate
{

    WEAKIFY(self);
    dispatch_async(self.stateMachineQueue, ^{
        STRONGIFY(self);
        SOSAccountConfiguration *storage = self.accountConfiguration.storage;
        storage.ringUpdateFlag = YES;
        [self.accountConfiguration setStorage:storage];
        [self.performRingUpdates trigger];
        secnotice("sos-sm", "trigger ring update at %@",
                  self.performRingUpdates.nextFireTime);
    });
}

//Mark: -- State machine and opertions

- (OctagonStateTransitionOperation *)performBackup {

    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"perform-backup-state"
                                        intending:SOSStateReady
                                       errorState:SOSStateError
                              withBlockTakingSelf:^void(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);
        SOSAccountConfiguration *storage = self.accountConfiguration.storage;

        secnotice("sos-sm", "performing backup for %@", storage.pendingBackupPeers);

        if (storage.pendingBackupPeers.count) {
            SOSCCRequestSyncWithBackupPeerList((__bridge CFArrayRef)storage.pendingBackupPeers);
            [storage clearPendingBackupPeers];
        }
        [self.accountConfiguration setStorage:storage];

        op.nextState = SOSStateReady;
    }];
}

- (OctagonStateTransitionOperation *)performRingUpdate {

    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"perform-ring-update"
                                        intending:SOSStateReady
                                       errorState:SOSStateError
                              withBlockTakingSelf:^void(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);

        SOSAccountConfiguration *storage = self.accountConfiguration.storage;
        storage.ringUpdateFlag = NO;
        [self.accountConfiguration setStorage:storage];

        [self performTransaction:^(SOSAccountTransaction * _Nonnull txn) {

            [self _onQueueRecordRetiredPeersInCircle];

            SOSAccountEnsureRecoveryRing(self);
            [self _onQueueEnsureInBackupRings];

            CFErrorRef localError = NULL;
            if(![self.circle_transport flushChanges:&localError]){
                secerror("flush circle failed %@", localError);
            }
            CFReleaseNull(localError);

            if(!SecCKKSTestDisableSOS()) {
                SOSAccountNotifyEngines(self);
            }
        }];

        op.nextState = SOSStateReady;
    }];

}


- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextStateMachineTransition:(OctagonState*)currentState
                                                                                                        flags:(nonnull OctagonFlags *)flags
                                                                                                 pendingFlags:(nonnull id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler
{
    dispatch_assert_queue(self.stateMachineQueue);

    secnotice("sos-sm", "Entering state: %@ [flags: %@]", currentState, flags);

    if ([currentState isEqualToString:SOSStateReady]) {
        if([flags _onqueueContains:SOSFlagTriggerBackup]) {
            [flags _onqueueRemoveFlag:SOSFlagTriggerBackup];
            return [OctagonStateTransitionOperation named:@"perform-backup-flag"
                                                 entering:SOSStatePerformBackup];
        }

        if ([flags _onqueueContains:SOSFlagTriggerRingUpdate]) {
            [flags _onqueueRemoveFlag:SOSFlagTriggerRingUpdate];
            return [OctagonStateTransitionOperation named:@"perform-ring-update-flag"
                                                 entering:SOSStatePerformRingUpdate];
        }
        return nil;

    } else if ([currentState isEqualToString:SOSStateError]) {
        return nil;
    } else if ([currentState isEqualToString:SOSStatePerformRingUpdate]) {
        return [self performRingUpdate];

    } else if ([currentState isEqualToString:SOSStatePerformBackup]) {
        return [self performBackup];
    }

    return nil;
}
#endif

@end


