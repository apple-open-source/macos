
#if OCTAGON

#import <TargetConditionals.h>
#import <CloudKit/CloudKit_Private.h>
#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>

#include "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTFetchViewsOperation.h"
#import "keychain/ot/OTSOSUpgradeOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTUpdateTrustedDeviceListOperation.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CloudKitCategories.h"
#import <AuthKit/AKError.h>
#import <os/feature_private.h>

@interface OTSOSUpgradeOperation ()
@property OTOperationDependencies* deps;
@property OTDeviceInformation* deviceInfo;

@property OctagonState* ckksConflictState;

// Since we're making callback based async calls, use this operation trick to hold off the ending of this operation
@property NSOperation* finishedOp;

@property OTUpdateTrustedDeviceListOperation* updateOp;
@end

@implementation OTSOSUpgradeOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                   ckksConflictState:(OctagonState*)ckksConflictState
                          errorState:(OctagonState*)errorState
                          deviceInfo:(OTDeviceInformation*)deviceInfo
                      policyOverride:(TPPolicyVersion* _Nullable)policyOverride
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
        _ckksConflictState = ckksConflictState;

        _deviceInfo = deviceInfo;
        _policyOverride = policyOverride;
    }
    return self;
}

- (NSData *)persistentKeyRef:(SecKeyRef)secKey error:(NSError **)error
{
    CFDataRef cfEncryptionKeyPersistRef = NULL;
    OSStatus status;

    status = SecKeyCopyPersistentRef(secKey, &cfEncryptionKeyPersistRef);
    if(status) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        }
    } else if (cfEncryptionKeyPersistRef) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecItemNotFound userInfo:nil];
        }
    }

    return CFBridgingRelease(cfEncryptionKeyPersistRef);
}

- (void)groupStart
{
    WEAKIFY(self);

    if(!self.deps.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "SOS not enabled on this platform?");
        self.nextState = OctagonStateBecomeUntrusted;
        return;
    }

    secnotice("octagon-sos", "Attempting SOS upgrade");

    NSError* error = nil;
    SOSCCStatus sosCircleStatus = [self.deps.sosAdapter circleStatus:&error];
    if(error || sosCircleStatus == kSOSCCError) {
        secnotice("octagon-sos", "Error fetching circle status: %@", error);
        self.nextState = OctagonStateBecomeUntrusted;
        return;
    }

    // Now that we have some non-error SOS status, write down that we attempted an SOS Upgrade (and make sure the CDP bit is on)
    NSError* persistError = nil;
    BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
        metadata.attemptedJoin = OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED;
        metadata.cdpState = OTAccountMetadataClassC_CDPState_ENABLED;
        return metadata;
    } error:&persistError];
    if(!persisted || persistError) {
        secerror("octagon: failed to save 'attempted join' state: %@", persistError);
    }

    if(sosCircleStatus != kSOSCCInCircle) {
        secnotice("octagon-sos", "Device is not in SOS circle (state: %@), quitting SOS upgrade", SOSAccountGetSOSCCStatusString(sosCircleStatus));
        self.nextState = OctagonStateBecomeUntrusted;
        return;
    }

    id<CKKSSelfPeer> sosSelf = [self.deps.sosAdapter currentSOSSelf:&error];
    if(!sosSelf || error) {
        secnotice("octagon-sos", "Failed to get the current SOS self: %@", error);
        [self handlePrepareErrors:error nextExpectedState:OctagonStateBecomeUntrusted];
        return;
    }

    // Fetch the persistent references for our signing and encryption keys
    NSData* signingKeyPersistRef = [self persistentKeyRef:sosSelf.signingKey.secKey error:&error];
    if (signingKeyPersistRef == NULL) {
        secnotice("octagon-sos", "Failed to get the persistent ref for our SOS signing key: %@", error);
        [self handlePrepareErrors:error nextExpectedState:OctagonStateBecomeUntrusted];
        return;
    }

    NSData* encryptionKeyPersistRef = [self persistentKeyRef:sosSelf.encryptionKey.secKey error:&error];
    if (encryptionKeyPersistRef == NULL) {
        secnotice("octagon-sos", "Failed to get the persistent ref for our SOS encryption key: %@", error);
        [self handlePrepareErrors:error nextExpectedState:OctagonStateBecomeUntrusted];
        return;
    }

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        // If we errored in some unknown way, ask to try again!
        STRONGIFY(self);

        if(self.error) {
            // Is this a very scary error?
            bool fatal = false;

            NSTimeInterval ckDelay = CKRetryAfterSecondsForError(self.error);
            NSTimeInterval cuttlefishDelay = [self.error cuttlefishRetryAfter];
            NSTimeInterval delay = MAX(ckDelay, cuttlefishDelay);
            if (delay == 0) {
                delay = 30;
            }

            if([self.error isCuttlefishError:CuttlefishErrorResultGraphNotFullyReachable]) {
                secnotice("octagon-sos", "SOS upgrade error is 'result graph not reachable'; retrying is useless: %@", self.error);
                fatal = true;
            }

            if([self.error.domain isEqualToString:TrustedPeersHelperErrorDomain] && self.error.code == TrustedPeersHelperErrorNoPeersPreapprovePreparedIdentity) {
                secnotice("octagon-sos", "SOS upgrade error is 'no peers preapprove us'; retrying immediately is useless: %@", self.error);
                fatal = true;
            }

            if(!fatal) {
                secnotice("octagon-sos", "SOS upgrade error is not fatal: requesting retry in %0.2fs: %@", delay, self.error);
                [self.deps.flagHandler handlePendingFlag:[[OctagonPendingFlag alloc] initWithFlag:OctagonFlagAttemptSOSUpgrade
                                                                                   delayInSeconds:delay]];
            }
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSString* bottleSalt = nil;
    NSError *authKitError = nil;

    NSString *altDSID = [self.deps.authKitAdapter primaryiCloudAccountAltDSID:&authKitError];
    if(altDSID){
        bottleSalt = altDSID;
    }
    else {
        NSError* accountError = nil;
        OTAccountMetadataClassC* account = [self.deps.stateHolder loadOrCreateAccountMetadata:&accountError];

        if(account && !accountError) {
            secnotice("octagon", "retrieved account, altdsid is: %@", account.altDSID);
            bottleSalt = account.altDSID;
        }
        if(accountError || !account){
            secerror("failed to rerieve account object: %@", accountError);
        }
    }

    [self.deps.cuttlefishXPCWrapper prepareWithContainer:self.deps.containerName
                                                 context:self.deps.contextID
                                                   epoch:self.deviceInfo.epoch
                                               machineID:self.deviceInfo.machineID
                                              bottleSalt:bottleSalt
                                                bottleID:[NSUUID UUID].UUIDString
                                                 modelID:self.deviceInfo.modelID
                                              deviceName:self.deviceInfo.deviceName
                                            serialNumber:self.self.deviceInfo.serialNumber
                                               osVersion:self.deviceInfo.osVersion
                                           policyVersion:self.policyOverride
                                           policySecrets:nil
                             signingPrivKeyPersistentRef:signingKeyPersistRef
                                 encPrivKeyPersistentRef:encryptionKeyPersistRef
                                                   reply:^(NSString * _Nullable peerID,
                                                           NSData * _Nullable permanentInfo,
                                                           NSData * _Nullable permanentInfoSig,
                                                           NSData * _Nullable stableInfo,
                                                           NSData * _Nullable stableInfoSig,
                                                           NSSet<NSString*>* syncingViews,
                                                           TPPolicy* _Nullable syncingPolicy,
                                                           NSError * _Nullable error) {
            STRONGIFY(self);

            [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradePrepare hardFailure:true result:error];

            if(error) {
                secerror("octagon-sos: Error preparing identity: %@", error);
                self.error = error;
                [self handlePrepareErrors:error nextExpectedState:OctagonStateBecomeUntrusted];

                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            secnotice("octagon-sos", "Prepared: %@ %@ %@", peerID, permanentInfo, permanentInfoSig);

            NSError* localError = nil;
            BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.syncingViews = [syncingViews mutableCopy];
                [metadata setTPPolicy:syncingPolicy];
                return metadata;
            } error:&localError];

            if(!persisted || localError) {
                secerror("octagon-ckks: Error persisting new views and policy: %@", localError);
                self.error = localError;
                [self handlePrepareErrors:error nextExpectedState:OctagonStateBecomeUntrusted];
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            [self.deps.viewManager setSyncingViews:syncingViews sortingPolicy:syncingPolicy];

            [self afterPrepare];
        }];
}

- (void)afterPrepare
{
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper preflightPreapprovedJoinWithContainer:self.deps.containerName
                                                                  context:self.deps.contextID
                                                                    reply:^(BOOL launchOkay, NSError * _Nullable error) {
            STRONGIFY(self);

            // Preflight preapprovedjoin should return the policy used by the peers we eventually will trust
            // <rdar://problem/57768490> Octagon: ensure we use appropriate CKKS policy when joining octagon with future policy

            [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradePreflightPreapprovedJoin hardFailure:true result:error];
            if(error) {
                secerror("octagon-sos: preflightPreapprovedJoin failed: %@", error);

                self.error = error;
                self.nextState = OctagonStateBecomeUntrusted;
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            if(!launchOkay) {
                secnotice("octagon-sos", "TPH believes a preapprovedJoin will fail; aborting.");
                self.nextState = OctagonStateBecomeUntrusted;
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            secnotice("octagon-sos", "TPH believes a preapprovedJoin might succeed; continuing.");
            [self afterPreflight];
        }];
}

- (void)afterPreflight
{
    WEAKIFY(self);
    self.updateOp = [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.deps
                                                                     intendedState:OctagonStateReady
                                                                  listUpdatesState:OctagonStateReady
                                                                        errorState:OctagonStateError
                                                                         retryFlag:nil];
    self.updateOp.logForUpgrade = YES;
    [self runBeforeGroupFinished:self.updateOp];

    CKKSResultOperation* afterUpdate = [CKKSResultOperation named:@"after-update"
                                                      withBlock:^{
        STRONGIFY(self);
        [self afterUpdate];
    }];
    [afterUpdate addDependency:self.updateOp];
    [self runBeforeGroupFinished:afterUpdate];
}

- (void)handlePrepareErrors:(NSError *)error nextExpectedState:(OctagonState*)nextState
{
    secnotice("octagon-sos", "handling prepare error: %@", error);

    if ([self.deps.lockStateTracker isLockedError:error]) {
        self.nextState = OctagonStateWaitForUnlock;
    } else {
        self.nextState = nextState;
    }
    self.error = error;
}

- (void)afterUpdate
{
    if (self.updateOp.error) {
        [self handlePrepareErrors:self.updateOp.error nextExpectedState:self.nextState];
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }
    secnotice("octagon-sos", "Successfully saved machineID allow-list");
    [self afterSuccessfulAllowList];
}

- (void)requestSilentEscrowUpdate
{
    NSError* error = nil;
    id<SecEscrowRequestable> request = [self.deps.escrowRequestClass request:&error];
    if(!request || error) {
        secnotice("octagon-sos", "Unable to acquire a EscrowRequest object: %@", error);
        return;
    }

    [request triggerEscrowUpdate:@"octagon-sos" error:&error];
    [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradeSilentEscrow hardFailure:true result:error];

    if(error) {
        secnotice("octagon-sos", "Unable to request silent escrow update: %@", error);
    } else{
        secnotice("octagon-sos", "Requested silent escrow update");
    }
}

- (void)afterSuccessfulAllowList
{
    WEAKIFY(self);

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps];
    [self runBeforeGroupFinished:fetchKeysOp];
    
    secnotice("octagon-sos", "Fetching keys from CKKS");
    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"sos-upgrade-with-keys"
                                                                withBlock:^{
            STRONGIFY(self);
            [self proceedWithKeys:fetchKeysOp.viewKeySets pendingTLKShares:fetchKeysOp.pendingTLKShares];
        }];
    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
{
    WEAKIFY(self);

    secnotice("octagon-sos", "Fetching trusted peers from SOS");

    NSArray<NSData*>* publicSigningSPKIs = nil;
    if(self.deps.sosAdapter.sosEnabled) {
        NSError* sosPreapprovalError = nil;
        publicSigningSPKIs = [OTSOSAdapterHelpers peerPublicSigningKeySPKIsForCircle:self.deps.sosAdapter error:&sosPreapprovalError];

        if(publicSigningSPKIs) {
            secnotice("octagon-sos", "SOS preapproved keys are %@", publicSigningSPKIs);
        } else {
            secnotice("octagon-sos", "Unable to fetch SOS preapproved keys: %@", sosPreapprovalError);
        }

    } else {
        secnotice("octagon-sos", "SOS not enabled; no preapproved keys");
    }

    secnotice("octagon-sos", "Beginning SOS upgrade with %d key sets and %d SOS peers", (int)viewKeySets.count, (int)publicSigningSPKIs.count);

    [self.deps.cuttlefishXPCWrapper attemptPreapprovedJoinWithContainer:self.deps.containerName
                                                                context:self.deps.contextID
                                                               ckksKeys:viewKeySets
                                                              tlkShares:pendingTLKShares
                                                        preapprovedKeys:publicSigningSPKIs
                                                                  reply:^(NSString * _Nullable peerID,
                                                                          NSArray<CKRecord*>* keyHierarchyRecords,
                                                                          NSSet<NSString*>* _Nullable syncingViewList,
                                                                          TPPolicy* _Nullable syncingPolicy,
                                                                          NSError * _Nullable error) {
        STRONGIFY(self);

        [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradePreapprovedJoin hardFailure:true result:error];
        if(error) {
            secerror("octagon-sos: attemptPreapprovedJoin failed: %@", error);

            if ([error isCuttlefishError:CuttlefishErrorKeyHierarchyAlreadyExists]) {
                secnotice("octagon-ckks", "A CKKS key hierarchy is out of date; requesting reset");
                self.nextState = self.ckksConflictState;
            } else {
                self.error = error;
                self.nextState = OctagonStateBecomeUntrusted;
            }
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        [self requestSilentEscrowUpdate];

        secerror("octagon-sos: attemptPreapprovedJoin succeded");
        [self.deps.viewManager setSyncingViews:syncingViewList sortingPolicy:syncingPolicy];

        NSError* localError = nil;
        BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC *  _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
            metadata.trustState = OTAccountMetadataClassC_TrustState_TRUSTED;
            metadata.peerID = peerID;
            metadata.syncingViews = [syncingViewList mutableCopy];
            [metadata setTPPolicy:syncingPolicy];

            return metadata;
        } error:&localError];

        if(!persisted || localError) {
            secnotice("octagon-sos", "Couldn't persist results: %@", localError);
            self.error = localError;
            self.nextState = OctagonStateError;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        self.nextState = self.intendedState;

        // Tell CKKS about our shiny new records!
        for (id key in self.deps.viewManager.views) {
            CKKSKeychainView* view = self.deps.viewManager.views[key];
            secnotice("octagon-ckks", "Providing ck records (from sos upgrade) to %@", view);
            [view receiveTLKUploadRecords: keyHierarchyRecords];
        }

        [self runBeforeGroupFinished:self.finishedOp];
    }];
}

@end

#endif // OCTAGON
