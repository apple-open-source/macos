
#if OCTAGON

#import <AppleFeatures/AppleFeatures.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/ot/ObjCImprovements.h"

#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTUpdateTPHOperation.h"
#import "keychain/ot/ErrorUtils.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#import "keychain/categories/NSError+UsefulConstructors.h"

@interface OTUpdateTPHOperation ()
@property OTOperationDependencies* deps;
@property BOOL forceRefetch;

@property (nullable) OctagonState* peerUnknownState;
@property (nullable) OctagonState* determineCDPState;
@property (nonatomic) OTDeviceInformation* deviceInfo;
@property NSOperation* finishedOp;

@property (nullable) OctagonFlag* retryFlag;
@end

@implementation OTUpdateTPHOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                          deviceInfo:(OTDeviceInformation*)deviceInfo
                       intendedState:(OctagonState*)intendedState
                    peerUnknownState:(OctagonState*)peerUnknownState
                   determineCDPState:(OctagonState*)determineCDPState
                          errorState:(OctagonState*)errorState
                        forceRefetch:(BOOL)forceRefetch
                           retryFlag:(OctagonFlag* _Nullable)retryFlag
{
    if((self = [super init])) {
        _deps = dependencies;

        _deviceInfo = deviceInfo;

        _intendedState = intendedState;
        _nextState = errorState;
        _peerUnknownState = peerUnknownState;
        _determineCDPState = determineCDPState;
        
        _forceRefetch = forceRefetch;

        _retryFlag = retryFlag;
    }
    return self;
}

- (void)sendMetric:(NSError*)error
{
    AAFAnalyticsEventSecurity* trustLossEvent = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                         altDSID:self.deps.activeAccount.altDSID
                                                                                                          flowID:self.deps.flowID
                                                                                                 deviceSessionID:self.deps.deviceSessionID
                                                                                                       eventName:kSecurityRTCEventNameOctagonTrustLost
                                                                                                 testsAreEnabled:self.deps.permittedToSendMetrics
                                                                                                  canSendMetrics:YES
                                                                                                        category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    [trustLossEvent sendMetricWithResult:YES error:error];
}

- (void)sendResetRecipientMetric
{
    NSError* error = [NSError errorWithDomain:kSecurityRTCErrorDomain code:OctagonTrustDepartureReasonErrorCodeResetRecipient description:@"Peer fell out of trust due to an Octagon Reset"];
    [self sendMetric:error];
}


- (void)sendPeerDistrustWhileStillTDLAllowedMetric
{
    NSError* error = [NSError errorWithDomain:kSecurityRTCErrorDomain code:OctagonTrustDepartureReasonErrorCodePeerDistrust description:@"Peer fell out of trust due to another peer distrusting ego but we're still in the TDL allowed list"];
    [self sendMetric:error];
}

- (BOOL)isOurMachineIDAllowed
{
    __block BOOL stillAllowed = NO;

    [self.deps.authKitAdapter fetchCurrentDeviceListByAltDSID:self.deps.activeAccount.altDSID
                                                       flowID:self.deps.flowID
                                              deviceSessionID:self.deps.deviceSessionID
                                                        reply:^(NSSet<NSString *> * _Nullable machineIDs,
                                                                NSSet<NSString *> * _Nullable userInitiatedRemovals,
                                                                NSSet<NSString *> * _Nullable evictedRemovals,
                                                                NSSet<NSString *> * _Nullable unknownReasonRemovals,
                                                                NSString * _Nullable version, NSString * _Nullable trustedDeviceHash,
                                                                NSString * _Nullable deletedDeviceHash,
                                                                NSNumber * _Nullable trustedDevicesUpdateTimestamp,
                                                                NSError * _Nullable error) {
        if (error) {
            secerror("octagon: failed to fetch current trusted device list");
            return;
        }
        if ([machineIDs containsObject:self.deviceInfo.machineID]) {
            secnotice("octagon", "Our machineID is still allowed");
            stillAllowed = YES;
        } else {
            secnotice("octagon", "Our machineID is NOT allowed");
        }
    }];

    return stillAllowed;
}

- (void)groupStart
{
    WEAKIFY(self);
    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        // If we errored in some unknown way, ask to try again!
        STRONGIFY(self);

        if(self.error) {
            if(self.retryFlag == nil) {
                secerror("octagon: Received an error updating TPH, but no retry flag present.");
                return;
            }

            // Is this a very scary error?
            bool fatal = true;

            OctagonPendingFlag* pendingFlag = nil;

            if([self.deps.lockStateTracker isLockedError:self.error]) {
                secnotice("octagon", "Updating trust state failed because locked, retry once unlocked: %@", self.error);
                self.nextState = OctagonStateWaitForUnlock;
                pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:self.retryFlag
                                                            conditions:OctagonPendingConditionsDeviceUnlocked];
                fatal = false;
            } else {
                // more CloudKit errors should trigger a retry here
                secnotice("octagon", "Error is currently unknown, aborting: %@", self.error);
            }

            if(!fatal) {
                if(!pendingFlag) {
                    NSTimeInterval delay = [self.error retryInterval];

                    pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:self.retryFlag
                                                            delayInSeconds:delay];
                }
                secnotice("octagon", "Updating trust state not fatal: requesting retry: %@",
                          pendingFlag);
                [self.deps.flagHandler handlePendingFlag:pendingFlag];
            }
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSError* stateError = nil;
    BOOL everAppliedToOctagon = NO;
    OTAccountMetadataClassC* currentMetadata = [self.deps.stateHolder loadOrCreateAccountMetadata:&stateError];

    // If there is no secureElementIdentity, we need to positively assert that across the XPC boundary
    TrustedPeersHelperIntendedTPPBSecureElementIdentity* secureElementIdentity = nil;
    if (!currentMetadata || stateError) {
        secerror("octagon: Unable to load current metadata: %@", stateError);
        // fall through; this isn't fatal
    } else {
        secureElementIdentity = [[TrustedPeersHelperIntendedTPPBSecureElementIdentity alloc] initWithSecureElementIdentity:currentMetadata.parsedSecureElementIdentity];
    }
    
    if (currentMetadata.hasAttemptedJoin) {
        switch (currentMetadata.attemptedJoin) {
            case OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED:
                everAppliedToOctagon = YES;
                break;
            case OTAccountMetadataClassC_AttemptedAJoinState_NOTATTEMPTED:
                everAppliedToOctagon = NO;
                break;
            case OTAccountMetadataClassC_AttemptedAJoinState_UNKNOWN:
                everAppliedToOctagon = NO;
                break;
            default:
                break;
        }
    }

    if(self.forceRefetch) {
        secnotice("octagon", "Forcing a full refetch");
    }

    [self.deps.cuttlefishXPCWrapper updateWithSpecificUser:self.deps.activeAccount
                                              forceRefetch:self.forceRefetch
                                                deviceName:self.deps.deviceInformationAdapter.deviceName
                                              serialNumber:self.deps.deviceInformationAdapter.serialNumber
                                                 osVersion:self.deps.deviceInformationAdapter.osVersion
                                             policyVersion:nil
                                             policySecrets:nil
                                 syncUserControllableViews:nil
                                     secureElementIdentity:secureElementIdentity
                                             walrusSetting:nil
                                                 webAccess:nil
                                                     reply:^(TrustedPeersHelperPeerState* peerState, TPSyncingPolicy* syncingPolicy, NSError* error) {
        STRONGIFY(self);
        if(error || !peerState) {
            secerror("octagon: update errored: %@", error);
            self.error = error;
            
            if ([error isCuttlefishError:CuttlefishErrorUpdateTrustPeerNotFound]) {
                secnotice("octagon-ckks", "Cuttlefish reports we no longer exist.");
                if (self.determineCDPState) {
                    self.nextState = self.determineCDPState;
                } else if (everAppliedToOctagon && self.peerUnknownState) {
                    [self sendResetRecipientMetric];
                    self.nextState = self.peerUnknownState;
                } else {
                    self.nextState = OctagonStateBecomeUntrusted;
                }
            } else {
                // On an error, don't set nextState.
            }
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        secnotice("octagon", "update complete: %@, %@", peerState, syncingPolicy);

        NSError* localError = nil;
        BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
            [metadata setTPSyncingPolicy:syncingPolicy];

            return metadata;
        } error:&localError];
        if(!persisted || localError) {
            secerror("octagon: Unable to save new syncing state: %@", localError);

        } else {
            // After an update(), we're sure that we have a fresh policy
            BOOL viewSetChanged = [self.deps.ckks setCurrentSyncingPolicy:syncingPolicy policyIsFresh:YES];
            if(viewSetChanged) {
                [self.deps.flagHandler handleFlag:OctagonFlagCKKSViewSetChanged];
            }
        }

        if(peerState.identityIsPreapproved) {
            secnotice("octagon-sos", "Self peer is now preapproved!");
            [self.deps.flagHandler handleFlag:OctagonFlagEgoPeerPreapproved];
        }
        if (peerState.memberChanges) {
            secnotice("octagon", "Member list changed");
            [self.deps.octagonAdapter sendTrustedPeerSetChangedUpdate];
        }

        if (peerState.unknownMachineIDsPresent) {
            secnotice("octagon-authkit", "Unknown machine IDs are present; requesting fetch");
            [self.deps.flagHandler handleFlag:OctagonFlagFetchAuthKitMachineIDList];
        }

        if (peerState.peerStatus & TPPeerStatusExcluded) {
            secnotice("octagon", "Self peer (%@) is excluded; moving to untrusted", peerState.peerID);

            NSError* accountError = nil;
            OTAccountMetadataClassC* accountState = [self.deps.stateHolder loadOrCreateAccountMetadata:&accountError];
            if (accountError || accountState == nil) {
                secerror("octagon: failed to get account metadata: %@", accountError);
            } else if (accountState.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
                if ([self isOurMachineIDAllowed]) {
                    [self sendPeerDistrustWhileStillTDLAllowedMetric];
                }
            }
            self.nextState = OctagonStateBecomeUntrusted;
        } else if(peerState.peerStatus & TPPeerStatusUnknown) {
            if (peerState.identityIsPreapproved) {
                secnotice("octagon", "Self peer (%@) is excluded but is preapproved, moving to sosuprade", peerState.peerID);
                self.nextState = OctagonStateAttemptSOSUpgrade;
            } else {
                if (self.determineCDPState) {
                    secnotice("octagon", "Self peer (%@) is unknown, but still figuring out cdp state; moving to '%@''", peerState.peerID, self.determineCDPState);
                    self.nextState = self.determineCDPState;
                } else if (everAppliedToOctagon && self.peerUnknownState) {
                    // if we ever attempted to join, then move to peer unknown state
                    secnotice("octagon", "Self peer (%@) is unknown and has attempted a join; moving to '%@''", peerState.peerID, self.peerUnknownState);
                    [self sendResetRecipientMetric];
                    self.nextState = self.peerUnknownState;
                } else {
                    secnotice("octagon", "Self peer (%@) is unknown and never attempted a join; moving to '%@''", peerState.peerID, OctagonStateBecomeUntrusted);
                    self.nextState = OctagonStateBecomeUntrusted;
                }
            }
        } else {
            self.nextState = self.intendedState;
        }

        [self runBeforeGroupFinished:self.finishedOp];
    }];
}

@end

#endif // OCTAGON
