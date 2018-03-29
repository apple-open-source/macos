/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#import "SecEntitlements.h"
#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>


#if OCTAGON
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTContext.h"
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTRamping.h"
#import "keychain/ot/SFPublicKey+SPKI.h"
#import "keychain/ot/OT.h"
#import "keychain/ot/OTConstants.h"

#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSLockStateTracker.h"

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#import <Accounts/ACAccountType_Private.h>
#import <Accounts/ACAccountStore.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#else
#import <Accounts/Accounts.h>
#import <AOSAccounts/MobileMePrefsCoreAEPrivate.h>
#import <AOSAccounts/MobileMePrefsCore.h>
#import <AOSAccounts/ACAccountStore+iCloudAccount.h>
#import <AOSAccounts/ACAccount+iCloudAccount.h>
#import <AOSAccounts/iCloudAccount.h>
#endif

#import <Security/SecureObjectSync/SOSAccountTransaction.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import <Security/SecureObjectSync/SOSAccount.h>
#pragma clang diagnostic pop

static NSString* const kOTRampForEnrollmentRecordName = @"metadata_rampstate_enroll";
static NSString* const kOTRampForRestoreRecordName = @"metadata_rampstate_restore";
static NSString* const kOTRampForCFURecordName = @"metadata_rampstate_cfu";
static NSString* const kOTRampZoneName = @"metadata_zone";
#define NUM_NSECS_IN_24_HRS (86400 * NSEC_PER_SEC)

@interface OTManager () <NSXPCListenerDelegate, OTContextIdentityProvider>
@property NSXPCListener *listener;
@property (nonatomic, strong) OTContext* context;
@property (nonatomic, strong) OTLocalStore *localStore;
@property (nonatomic, strong) OTRamp *enrollRamp;
@property (nonatomic, strong) OTRamp *restoreRamp;
@property (nonatomic, strong) OTRamp *cfuRamp;
@property (nonatomic, strong) CKKSNearFutureScheduler *cfuScheduler;
@property (nonatomic, strong) NSDate *lastPostedCoreFollowUp;
@end

@implementation OTManager

-(instancetype)init
{
    OTLocalStore* localStore = nil;
    OTContext* context = nil;

    NSString* dsid = [self askAccountsForDSID];
    if(dsid){
        localStore = [[OTLocalStore alloc]initWithContextID:OTDefaultContext dsid:dsid path:nil error:nil];
        context = [[OTContext alloc]initWithContextID:OTDefaultContext dsid:dsid localStore:self.localStore cloudStore:nil identityProvider:self error:nil];
    }
    //initialize our scheduler
    CKKSNearFutureScheduler *cfuScheduler = [[CKKSNearFutureScheduler alloc] initWithName:@"scheduling-cfu" initialDelay:NUM_NSECS_IN_24_HRS  continuingDelay:NUM_NSECS_IN_24_HRS keepProcessAlive:true dependencyDescriptionCode:CKKSResultDescriptionNone block:^{
        secnotice("octagon", "running scheduled cfu block");
        NSError* error = nil;
        [self scheduledCloudKitRampCheck:&error];
    }];
    
    //initialize our ramp objects
    [self initRamps];
    
    return [self initWithContext:context
                      localStore:localStore
                          enroll:self.enrollRamp
                         restore:self.restoreRamp
                             cfu:self.cfuRamp
                    cfuScheduler:cfuScheduler];
}

-(instancetype) initWithContext:(OTContext*)context
                     localStore:(OTLocalStore*)localStore
                         enroll:(OTRamp*)enroll
                        restore:(OTRamp*)restore
                            cfu:(OTRamp*)cfu
                   cfuScheduler:(CKKSNearFutureScheduler*)cfuScheduler
{
    self = [super init];
    if(self){
        self.context = context;
        self.localStore = localStore;
        self.cfuRamp = cfu;
        self.enrollRamp = enroll;
        self.restoreRamp = restore;
        self.cfuScheduler = cfuScheduler;
        
        secnotice("octagon", "otmanager init");
    }
    return self;
}

-(NSString*) askAccountsForDSID
{
    NSString *dsid = nil;
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
    ACAccount *account = [accountStore aa_primaryAppleAccount];
    dsid = [account aa_personID];
#else
    ACAccount *primaryiCloudAccount = nil;
    if ([accountStore respondsToSelector:@selector(icaPrimaryAppleAccount)]){
        primaryiCloudAccount = [accountStore icaPrimaryAppleAccount];
    }
    dsid =  [primaryiCloudAccount icaPersonID];
#endif
    return dsid;
}

+ (instancetype _Nullable)manager {
    static OTManager* manager = nil;

    if(!SecOTIsEnabled()) {
        secerror("octagon: Attempt to fetch a manager while Octagon is disabled");
        return nil;
    }
    
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        manager = [[OTManager alloc]init];
    });

    return manager;
}


-(BOOL) initRamps
{
    BOOL initResult = NO;

    CKContainer* container = [CKKSViewManager manager].container;
    CKDatabase* database = [container privateCloudDatabase];
    CKRecordZoneID* zoneID = [[CKRecordZoneID alloc] initWithZoneName:kOTRampZoneName ownerName:CKCurrentUserDefaultName];

    CKKSCKAccountStateTracker *accountTracker = [CKKSViewManager manager].accountTracker;
    CKKSReachabilityTracker *reachabilityTracker = [CKKSViewManager manager].reachabilityTracker;
    CKKSLockStateTracker *lockStateTracker = [CKKSViewManager manager].lockStateTracker;

    self.cfuRamp = [[OTRamp alloc]initWithRecordName:kOTRampForCFURecordName
                                         featureName:@"cfu"
                                           container:container
                                            database:database
                                              zoneID:zoneID
                                      accountTracker:accountTracker
                                    lockStateTracker:lockStateTracker
                                 reachabilityTracker:reachabilityTracker
                    fetchRecordRecordsOperationClass:[CKFetchRecordsOperation class]];

    self.enrollRamp = [[OTRamp alloc]initWithRecordName:kOTRampForEnrollmentRecordName
                                            featureName:@"enroll"
                                              container:container
                                               database:database
                                                 zoneID:zoneID
                                         accountTracker:accountTracker
                                       lockStateTracker:lockStateTracker
                                    reachabilityTracker:reachabilityTracker
                       fetchRecordRecordsOperationClass:[CKFetchRecordsOperation class]];


    self.restoreRamp = [[OTRamp alloc]initWithRecordName:kOTRampForRestoreRecordName
                                             featureName:@"restore"
                                               container:container
                                                database:database
                                                  zoneID:zoneID
                                          accountTracker:accountTracker
                                        lockStateTracker:lockStateTracker
                                     reachabilityTracker:reachabilityTracker
                        fetchRecordRecordsOperationClass:[CKFetchRecordsOperation class]];

    if(self.cfuRamp && self.enrollRamp && self.restoreRamp){
        initResult = YES;
    }
    return initResult;
}

-(BOOL) initializeManagerPropertiesForContext:(NSString*)dsid error:(NSError**)error
{
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    NSError *localError = nil;
    BOOL initialized = YES;

    if(dsid == nil){
        dsid = [self askAccountsForDSID];
    }

    //create local store
    self.localStore = [[OTLocalStore alloc] initWithContextID:OTDefaultContext dsid:dsid path:nil error:&localError];
    if(!self.localStore){
        secerror("octagon: could not create localStore: %@", localError);
        [logger logUnrecoverableError:localError forEvent:OctagonEventSignIn withAttributes:@{
                                                                                         OctagonEventAttributeFailureReason : @"creating local store",
                                                                                         }];
        initialized = NO;
    }
    
    //create context
    self.context = [[OTContext alloc]initWithContextID:OTDefaultContext dsid:dsid localStore:self.localStore cloudStore:nil identityProvider:self error:&localError];
    if(!self.context){
        secerror("octagon: could not create context: %@", localError);
        [logger logUnrecoverableError:localError forEvent:OctagonEventSignIn withAttributes:@{
                                                                                         OctagonEventAttributeFailureReason : @"creating context",
                                                                                         }];
        self.localStore = nil;
        initialized = NO;
    }

    //just in case, init the ramp objects
    [self initRamps];

    if(localError && error){
        *error = localError;
    }
    return initialized;
}

/*
 *       SPI routines
 */

- (void)signIn:(NSString*)dsid reply:(void (^)(BOOL result, NSError * _Nullable signedInError))reply
{
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityOctagonSignIn withAction:nil];
    [tracker start];

    NSError *error = nil;
    if(![self initializeManagerPropertiesForContext:dsid error:&error]){
        [tracker cancel];
        reply(NO, error);
        return;
    }

    [tracker stop];
    [logger logSuccessForEventNamed:OctagonEventSignIn];
    
    secnotice("octagon","created context and local store on manager for:%@", dsid);
    
    reply(YES, error);
}

- (void)signOut:(void (^)(BOOL result, NSError * _Nullable signedOutError))reply
{
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    
    NSError* error = nil;
    NSError *bottledPeerError = nil;
    NSError *localContextError = nil;
    
    secnotice("octagon", "signing out of octagon trust: dsid: %@ contextID: %@",
              self.context.dsid,
              self.context.contextID);
    
    NSString* contextAndDSID = [NSString stringWithFormat:@"%@-%@", self.context.contextID, self.context.dsid];
    
    //remove all locally stored context
    BOOL result1 = [self.localStore deleteLocalContext:contextAndDSID error:&localContextError];
    if(!result1){
        secerror("octagon: could not delete local context: %@: %@", self.context.contextID, localContextError);
        [logger logUnrecoverableError:localContextError forEvent:OctagonEventSignOut withAttributes:@{
                                                                                                     OctagonEventAttributeFailureReason : @"deleting local context",
                                                                                                     }];
        error = localContextError;
    }
    
    BOOL result2 = [self.localStore deleteBottledPeersForContextAndDSID:contextAndDSID error:&bottledPeerError];
    if(!result2){
        secerror("octagon: could not delete bottle peer records: %@: %@", self.context.contextID, bottledPeerError);
        [logger logUnrecoverableError:bottledPeerError forEvent:OctagonEventSignOut withAttributes:@{
                                                                                                    OctagonEventAttributeFailureReason : @"deleting local bottled peers",
                                                                                                    }];
        error = bottledPeerError;
    }
    
    //free context & local store
    self.context = nil;
    self.localStore = nil;
    
    BOOL result = (result1 && result2);
    if (result) {
        [logger logSuccessForEventNamed:OctagonEventSignOut];
    }
    
    reply(result, error);
}
- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply
{
    secnotice("octagon", "preflightBottledPeer: %@ %@", contextID, dsid);
    NSError* error = nil;
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityOctagonPreflightBottle withAction:nil];

    [tracker start];

    if(!self.context || !self.localStore){
        if(![self initializeManagerPropertiesForContext:dsid error:&error]){
            secerror("octagon:  could not init manager obejcts: %@", error);
            reply(nil,nil,nil,error);
            [tracker cancel];
            return;
        }
    }

    NSInteger retryDelayInSeconds = 0;
    BOOL isFeatureOn = [self.enrollRamp checkRampState:&retryDelayInSeconds qos:NSQualityOfServiceUserInitiated error:&error];

    //got an error from ramp check, we should log it
    if(error){
        [logger logRecoverableError:error
                           forEvent:OctagonEventRamp
                           zoneName:kOTRampZoneName
                     withAttributes:@{
                                      OctagonEventAttributeFailureReason : @"ramp check for preflight bottle"
                                      }];
    }

    if(!isFeatureOn){           //cloud kit has not asked us to come back and the feature is off for this device
        secnotice("octagon", "bottled peers is not on");
        if(!error){
            error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorFeatureNotEnabled userInfo:@{NSLocalizedDescriptionKey: @"Feature not enabled"}];
        }
        reply(nil, nil, nil, error);
        return;
    }
    
    NSData* entropy = [self.context makeMeSomeEntropy:OTMasterSecretLength];
    if(!entropy){
        secerror("octagon: entropy creation failed: %@", error);
        error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEntropyCreationFailure userInfo:@{NSLocalizedDescriptionKey: @"Failed to create entropy"}];
        [logger logUnrecoverableError:error forEvent:OctagonEventPreflightBottle withAttributes:@{
                                                                                                  OctagonEventAttributeFailureReason : @"preflight bottle, entropy failure"}
         ];
        [tracker stop];
        reply(nil, nil, nil, error);
        return;
    }
    
    OTPreflightInfo* result = [self.context preflightBottledPeer:contextID entropy:entropy error:&error];
    if(!result || error){
        secerror("octagon: preflight failed: %@", error);
        [logger logUnrecoverableError:error forEvent:OctagonEventPreflightBottle withAttributes:@{ OctagonEventAttributeFailureReason : @"preflight bottle"}];
        reply(nil, nil, nil, error);
        [tracker stop];
        return;
    }
    
    [tracker stop];
    [logger logSuccessForEventNamed:OctagonEventPreflightBottle];
    
    secnotice("octagon", "preflightBottledPeer completed, created: %@", result.bottleID);
    
    reply(entropy, result.bottleID, result.escrowedSigningSPKI, error);
}

- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable error))reply
{
    secnotice("octagon", "launchBottledPeer");
    NSError* error = nil;
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityOctagonLaunchBottle withAction:nil];

    [tracker start];
    
    if(!self.context || !self.localStore){
        if(![self initializeManagerPropertiesForContext:nil error:&error]){
            [tracker cancel];
            reply(error);
            return;
        }
    }
    
    NSInteger retryDelayInSeconds = 0;
    BOOL isFeatureOn = [self.enrollRamp checkRampState:&retryDelayInSeconds qos:NSQualityOfServiceUserInitiated error:&error];

    //got an error from ramp check, we should log it
    if(error){
        [logger logRecoverableError:error
                           forEvent:OctagonEventRamp
                           zoneName:kOTRampZoneName
                     withAttributes:@{
                                      OctagonEventAttributeFailureReason : @"ramp state check for launch bottle"
                                      }];
    }

    if(!isFeatureOn){
        secnotice("octagon", "bottled peers is not on");
        if(!error){
            error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorFeatureNotEnabled userInfo:@{NSLocalizedDescriptionKey: @"Feature not enabled"}];
        }
        reply(error);
        return;
    }

    OTBottledPeerRecord* bprecord = [self.localStore readLocalBottledPeerRecordWithRecordID:bottleID error:&error];
    if(!bprecord || error){
        secerror("octagon: could not retrieve record for: %@, error: %@", bottleID, error);
        [logger logUnrecoverableError:error forEvent:OctagonEventLaunchBottle withAttributes:@{
                                                                                               OctagonEventAttributeFailureReason : @"reading bottle from local store"
                                                                                               }];
        [tracker stop];
        reply(error);
        return;
    }
    BOOL result = [self.context.cloudStore uploadBottledPeerRecord:bprecord escrowRecordID:bprecord.escrowRecordID error:&error];
    if(!result || error){
        secerror("octagon: could not upload record for bottleID %@, error: %@", bottleID, error);
        [logger logUnrecoverableError:error forEvent:OctagonEventLaunchBottle withAttributes:@{
                                                                                               OctagonEventAttributeFailureReason : @"upload bottle to cloud kit"
                                                                                               }];
        [tracker stop];
        reply(error);
        return;
    }
    
    [tracker stop];
    [logger logSuccessForEventNamed:OctagonEventLaunchBottle];
    
    secnotice("octagon", "successfully launched: %@", bprecord.recordName);
    
    reply(error);
}

- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID reply:(void (^)(NSData* signingKeyData, NSData* encryptionKeyData, NSError *))reply
{
    //check if configuration zone allows restore
    NSError* error = nil;
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityOctagonRestore withAction:nil];

    [tracker start];

    if(!self.context || !self.localStore){
        if(![self initializeManagerPropertiesForContext:dsid error:&error]){
            secerror("octagon:  could not init manager obejcts: %@", error);
            reply(nil,nil,error);
            [tracker cancel];
            return;
        }
    }

    NSInteger retryDelayInSeconds = 0;
    BOOL isFeatureOn = [self.restoreRamp checkRampState:&retryDelayInSeconds qos:NSQualityOfServiceUserInitiated error:&error];

    //got an error from ramp check, we should log it
    if(error){
        [logger logRecoverableError:error
                           forEvent:OctagonEventRamp
                           zoneName:kOTRampZoneName
                     withAttributes:@{
                                      OctagonEventAttributeFailureReason : @"checking ramp state for restore"
                                      }];
    }

    if(!isFeatureOn){
        secnotice("octagon", "bottled peers is not on");
        if(!error){
            error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorFeatureNotEnabled userInfo:@{NSLocalizedDescriptionKey: @"Feature not enabled"}];
        }
        [tracker stop];
        reply(nil, nil, error);
        return;
    }

    if(!escrowRecordID || [escrowRecordID length] == 0){
        secerror("octagon: missing escrowRecordID");
        error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEmptyEscrowRecordID userInfo:@{NSLocalizedDescriptionKey: @"Escrow Record ID is empty or missing"}];
        
        [logger logUnrecoverableError:error forEvent:OctagonEventRestoreBottle withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"escrow record id missing",
                                                                                                }];
        
        [tracker stop];
        reply(nil, nil, error);
        return;
    }
    if(!dsid || [dsid length] == 0){
        secerror("octagon: missing dsid");
        error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEmptyDSID userInfo:@{NSLocalizedDescriptionKey: @"DSID is empty or missing"}];
        
        [logger logUnrecoverableError:error forEvent:OctagonEventRestoreBottle withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"dsid missing",
                                                                                                }];
        [tracker stop];
        reply(nil, nil, error);
        return;
    }
    if(!secret || [secret length] == 0){
        secerror("octagon: missing secret");
        error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEmptySecret userInfo:@{NSLocalizedDescriptionKey: @"Secret is empty or missing"}];
        
        
        [logger logUnrecoverableError:error forEvent:OctagonEventRestoreBottle withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"secret missing",
                                                                                                }];
        
        [tracker stop];
        reply(nil, nil, error);
        return;
    }

    OTBottledPeerSigned *bps = [_context restoreFromEscrowRecordID:escrowRecordID secret:secret error:&error];
    if(!bps || error != nil){
        secerror("octagon: failed to restore bottled peer: %@", error);
        
        [logger logUnrecoverableError:error forEvent:OctagonEventRestoreBottle withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"restore failed",
                                                                                                }];
        [tracker stop];
        reply(nil, nil, error);
        return;
    }
    
    NSData *encryptionKeyData = bps.bp.peerEncryptionKey.publicKey.keyData;  // FIXME
    if(!encryptionKeyData){
        secerror("octagon: restored octagon encryption key is nil: %@", error);
        error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorRestoredPeerEncryptionKeyFailure userInfo:@{NSLocalizedDescriptionKey: @"Failed to retrieve restored Octagon Peer Encryption Key"}];
        
        [logger logUnrecoverableError:error forEvent:OctagonEventRestoreBottle withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"restored octagon encryption key"
                                                                                                }];
        [tracker stop];
        reply(nil,nil,error);
        return;
    }
    
    NSData *signingKeyData = bps.bp.peerSigningKey.publicKey.keyData;  // FIXME
    if(!signingKeyData){
        secerror("octagon: restored octagon signing key is nil: %@", error);
        error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorRestoredPeerSigningKeyFailure userInfo:@{NSLocalizedDescriptionKey: @"Failed to retrieve restored Octagon Peer Signing Key"}];
        
        [logger logUnrecoverableError:error forEvent:OctagonEventRestoreBottle withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"restored octagon signing key"
                                                                                                }];
        [tracker stop];
        reply(nil,nil,error);
        return;
    }
    [tracker stop];
    
    [logger logSuccessForEventNamed:OctagonEventRestoreBottle];
    
    secnotice("octagon", "restored bottled peer: %@", escrowRecordID);
    
    reply(signingKeyData, encryptionKeyData, error);
}

- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable error))reply
{
    NSError* error = nil;

    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityScrubBottle withAction:nil];
  
    if(!self.context || !self.localStore){
        if(![self initializeManagerPropertiesForContext:nil error:&error]){
            [tracker cancel];
            reply(error);
            return;
        }
    }
    [tracker start];

    NSInteger retryDelayInSeconds = 0;
    BOOL isFeatureOn = [self.enrollRamp checkRampState:&retryDelayInSeconds qos:NSQualityOfServiceUserInitiated error:&error];

    //got an error from ramp check, we should log it
    if(error){
        [logger logRecoverableError:error
                           forEvent:OctagonEventRamp
                           zoneName:kOTRampZoneName
                     withAttributes:@{
                                      OctagonEventAttributeFailureReason : @"ramp check for scrubbing bottled peer"
                                      }];
    }

    if(!isFeatureOn){
        secnotice("octagon", "bottled peers is not on");
        if(!error){
            error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorFeatureNotEnabled userInfo:@{NSLocalizedDescriptionKey: @"Feature not enabled"}];
        }
        [tracker stop];
        reply(error);
        return;
    }
    
    BOOL result = [self.context scrubBottledPeer:contextID bottleID:bottleID error:&error];
    if(!result || error){
        secerror("octagon: could not scrub record for bottleID %@, error: %@", bottleID, error);
        [logger logUnrecoverableError:error forEvent:OctagonEventScrubBottle withAttributes:@{
                                                                                              OctagonEventAttributeFailureReason : @"could not scrub bottle",
                                                                                              }];
        [tracker stop];
        reply(error);
        return;
    }
    [logger logSuccessForEventNamed:OctagonEventScrubBottle];
    
    secnotice("octagon", "scrubbed bottled peer: %@", bottleID);
    
    reply(error);
}

/*
 *  OTCTL tool routines
 */

-(void) reset:(void (^)(BOOL result, NSError *))reply
{
    NSError* error = nil;

    if(self.context.lockStateTracker.isLocked){
        secnotice("octagon","device is locked! can't check ramp state");
        error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                                    code:errSecInteractionNotAllowed
                                userInfo:@{NSLocalizedDescriptionKey: @"device is locked"}];

        reply(NO,error);
        return;
    }
    if(self.context.accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable){
        secnotice("octagon","not signed in! can't check ramp state");
        error = [NSError errorWithDomain:octagonErrorDomain
                                    code:OTErrorNotSignedIn
                                userInfo:@{NSLocalizedDescriptionKey: @"not signed in"}];
        reply(NO,error);
        return;

    }
    if(!self.context.reachabilityTracker.currentReachability){
        secnotice("octagon","no network! can't check ramp state");
        error = [NSError errorWithDomain:octagonErrorDomain
                                    code:OTErrorNoNetwork
                                userInfo:@{NSLocalizedDescriptionKey: @"no network"}];
        reply(NO,error);
        return;
    }

    NSError* bottledPeerError = nil;

    BOOL result = [_context.cloudStore performReset:&bottledPeerError];
    if(!result || bottledPeerError != nil){
        secerror("octagon: resetting octagon trust zone failed: %@", bottledPeerError);
    }
    
    NSString* contextAndDSID = [NSString stringWithFormat:@"%@-%@", self.context.contextID, self.context.dsid];

    result = [self.localStore deleteBottledPeersForContextAndDSID:contextAndDSID error:&bottledPeerError];
    if(!result){
        secerror("octagon: could not delete bottle peer records: %@: %@", self.context.contextID, bottledPeerError);
    }
    
    reply(result, bottledPeerError);
}

- (void)listOfEligibleBottledPeerRecords:(void (^)(NSArray* listOfRecords, NSError *))reply
{
    NSError* error = nil;

    if(self.context.lockStateTracker.isLocked){
        secnotice("octagon","device is locked! can't check ramp state");
        error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                                         code:errSecInteractionNotAllowed
                                     userInfo:@{NSLocalizedDescriptionKey: @"device is locked"}];

        reply(nil,error);
        return;
    }
    if(self.context.accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable){
        secnotice("octagon","not signed in! can't check ramp state");
        error = [NSError errorWithDomain:octagonErrorDomain
                                         code:OTErrorNotSignedIn
                                     userInfo:@{NSLocalizedDescriptionKey: @"not signed in"}];
        reply(nil,error);
        return;
    }
    if(!self.context.reachabilityTracker.currentReachability){
        secnotice("octagon","no network! can't check ramp state");
        error = [NSError errorWithDomain:octagonErrorDomain
                                         code:OTErrorNoNetwork
                                     userInfo:@{NSLocalizedDescriptionKey: @"no network"}];
        reply(nil,error);
        return;
    }

    NSArray* list = [_context.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error];
    if(!list || error !=nil){
        secerror("octagon: there are not eligible bottle peer records: %@", error);
        reply(nil,error);
        return;
    }
    reply(list, error);
}

- (void)octagonEncryptionPublicKey:(void (^)(NSData* encryptionKey, NSError *))reply
{
    __block NSData *encryptionKey = NULL;
    __block NSError* localError = nil;

    SOSCCPerformWithOctagonEncryptionPublicKey(^(SecKeyRef octagonPrivKey, CFErrorRef error) {
        CFDataRef key;
        SecKeyCopyPublicBytes(octagonPrivKey, &key);
        encryptionKey = CFBridgingRelease(key);
        if(error){
            localError = (__bridge NSError*)error;
        }
    });
    if(!encryptionKey || localError != nil){
        reply(nil, localError);
        secerror("octagon: retrieving the octagon encryption public key failed: %@", localError);
        return;
    }
    reply(encryptionKey, localError);
}

-(void)octagonSigningPublicKey:(void (^)(NSData* encryptionKey, NSError *))reply
{
    __block NSData *signingKey = NULL;
    __block NSError* localError = nil;

    SOSCCPerformWithOctagonSigningPublicKey(^(SecKeyRef octagonPrivKey, CFErrorRef error) {
        CFDataRef key;
        SecKeyCopyPublicBytes(octagonPrivKey, &key);
        signingKey = CFBridgingRelease(key);
        if(error){
            localError = (__bridge NSError*)error;
        }
    });
    if(!signingKey || localError != nil){
        reply(nil, localError);
        secerror("octagon: retrieving the octagon signing public key failed: %@", localError);
        return;
    }
    reply(signingKey, localError);
}

/*
 *  OT Helpers
 */

-(BOOL)scheduledCloudKitRampCheck:(NSError**)error
{
    secnotice("octagon", "scheduling a CloudKit ramping check");
    NSInteger retryAfterInSeconds = 0;
    NSError* localError = nil;
    BOOL cancelScheduler = YES;

    CKKSAnalytics* logger = [CKKSAnalytics logger];

    if(self.cfuRamp){
        BOOL canCFU = [self.cfuRamp checkRampState:&retryAfterInSeconds qos:NSQualityOfServiceUserInitiated error:&localError];

        if(localError){
            secerror("octagon: checking ramp state for CFU error'd: %@", localError);
            [logger logUnrecoverableError:localError forEvent:OctagonEventRamp withAttributes:@{
                                                                                                OctagonEventAttributeFailureReason : @"ramp check failed",
                                                                                                }];
        }

        if(canCFU){
            secnotice("octagon", "CFU is enabled, checking if this device has a bottle");
            OctagonBottleCheckState bottleStatus = [self.context doesThisDeviceHaveABottle:&localError];

            if(bottleStatus == NOBOTTLE){
                //time to post a follow up!
                secnotice("octagon", "device does not have a bottle, posting a follow up");
                if(!SecCKKSTestsEnabled()){
                    [self.context postFollowUp];
                }
                NSInteger timeDiff = -1;

                NSDate *currentDate = [NSDate date];
                if(self.lastPostedCoreFollowUp){
                    timeDiff = [currentDate timeIntervalSinceDate:self.lastPostedCoreFollowUp];
                }

                //log how long we last posted a followup, if any
                [logger logRecoverableError:localError
                                   forEvent:OctagonEventCoreFollowUp
                                   zoneName:kOTRampZoneName
                             withAttributes:@{
                                              OctagonEventAttributeFailureReason : @"No bottle for peer",
                                              OctagonEventAttributeTimeSinceLastPostedFollowUp: [NSNumber numberWithInteger:timeDiff],
                                              }];

                self.lastPostedCoreFollowUp = currentDate;
                //if the followup failed or succeeded, we should continue the scheduler until we have a bottle.
                cancelScheduler = NO;
            }else if(bottleStatus == BOTTLE){
                secnotice("octagon", "device has a bottle");
                [logger logSuccessForEventNamed:OctagonEventBottleCheck];
            }

            if(localError){
                [logger logRecoverableError:localError
                                   forEvent:OctagonEventBottleCheck
                                   zoneName:kOTRampZoneName
                             withAttributes:@{
                                              OctagonEventAttributeFailureReason : @"bottle check",
                                              }];
            }
        }
    }
    if(cancelScheduler == NO){
        secnotice("octagon", "requesting bottle check again");
        [self.cfuScheduler trigger];
    }

    if(error && localError){
        *error = localError;
    }
    return cancelScheduler;
}

-(void)scheduleCFUForFuture
{
    secnotice("octagon", "scheduling a query to cloudkit to see if this device can post a core follow up");
    
    [self.cfuScheduler trigger];
}

- (nullable OTIdentity *) currentIdentity:(NSError**)error
{
    return [OTIdentity currentIdentityFromSOS:error];
}

@end

#endif
