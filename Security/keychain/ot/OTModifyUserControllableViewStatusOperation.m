/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <TrustedPeers/TrustedPeers.h>
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/OTModifyUserControllableViewStatusOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

@interface OTModifyUserControllableViewStatusOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* peerMissingState;

@property TPPBPeerStableInfo_UserControllableViewStatus intendedViewStatus;
@end

@implementation OTModifyUserControllableViewStatusOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                  intendedViewStatus:(TPPBPeerStableInfo_UserControllableViewStatus)intendedViewStatus
                       intendedState:(OctagonState*)intendedState
                    peerMissingState:(OctagonState*)peerMissingState
                          errorState:(OctagonState*)errorState
{
    if ((self = [super init])) {
        _deps = dependencies;

        _intendedViewStatus = intendedViewStatus;

        _intendedState = intendedState;
        _peerMissingState = peerMissingState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{

    if(self.intendedViewStatus == TPPBPeerStableInfo_UserControllableViewStatus_FOLLOWING) {
#if TARGET_OS_WATCH || TARGET_OS_TV
        // Watches and TVs want to be able to set the FOLLOWING state
        [self performWithStatus:self.intendedViewStatus];
#else
        // For other platforms, we want to determine the actual state by asking
        WEAKIFY(self);

        // Should we ask SOS? Or Octagon?
        if(self.deps.sosAdapter.sosEnabled) {
            NSError* error = nil;
            BOOL safariViewEnabled = [self.deps.sosAdapter safariViewSyncingEnabled:&error];

            if(error) {
                secerror("octagon-ckks: Unable to fetch SOS Safari view status: %@", error);
                self.error = error;
                return;
            }

            secnotice("octagon-ckks", "Currently SOS believes the safari view is '%@'", safariViewEnabled ? @"enabled" : @"disabled");

            TPPBPeerStableInfo_UserControllableViewStatus status = safariViewEnabled ?
                TPPBPeerStableInfo_UserControllableViewStatus_ENABLED :
                TPPBPeerStableInfo_UserControllableViewStatus_DISABLED;

            [self performWithStatus:status];
            return;
        }

        secnotice("octagon-ckks", "Determining peers' user-controllable views policy");

        [self.deps.cuttlefishXPCWrapper fetchCurrentPolicyWithContainer:self.deps.containerName
                                                                context:self.deps.contextID
                                                        modelIDOverride:nil
                                                                  reply:^(TPSyncingPolicy* _Nullable syncingPolicy,
                                                                          TPPBPeerStableInfo_UserControllableViewStatus userControllableViewStatusOfPeers,
                                                                          NSError* _Nullable error) {
            STRONGIFY(self);

            if(error) {
                secnotice("octagon-ckks", "Determining peers' user-controllable views policy failed: %@", error);
                self.error = error;
                return;
            }

            secnotice("octagon-ckks", "Retrieved peers' user-controllable views policy as: %@",
                      TPPBPeerStableInfo_UserControllableViewStatusAsString(userControllableViewStatusOfPeers));

            [self performWithStatus:userControllableViewStatusOfPeers];
            return;
        }];
#endif

    } else {
        [self performWithStatus:self.intendedViewStatus];
    }
}

- (void)performWithStatus:(TPPBPeerStableInfo_UserControllableViewStatus)intendedViewStatus
{
    WEAKIFY(self);

    secnotice("octagon-ckks", "Setting user-controllable views to %@", TPPBPeerStableInfo_UserControllableViewStatusAsString(self.intendedViewStatus));

    [self.deps.cuttlefishXPCWrapper updateWithContainer:self.deps.containerName
                                                context:self.deps.contextID
                                             deviceName:nil
                                           serialNumber:nil
                                              osVersion:nil
                                          policyVersion:nil
                                          policySecrets:nil
                              syncUserControllableViews:[NSNumber numberWithInt:intendedViewStatus]
                                                  reply:^(TrustedPeersHelperPeerState* peerState, TPSyncingPolicy* syncingPolicy, NSError* error) {
        STRONGIFY(self);
        if(error || !syncingPolicy) {
            secerror("octagon-ckks: setting user-controllable views status errored: %@", error);
            self.error = error;

            if([self.deps.lockStateTracker isLockedError:self.error]) {
                secnotice("octagon-ckks", "Updating user-controllable view status failed because of lock state, will retry once unlocked: %@", self.error);
                OctagonPendingFlag* pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagAttemptUserControllableViewStatusUpgrade
                                                                                conditions:OctagonPendingConditionsDeviceUnlocked];

                [self.deps.flagHandler handlePendingFlag:pendingFlag];
            }
            if(peerState.peerStatus & (TPPeerStatusExcluded | TPPeerStatusUnknown)) {
                secnotice("octagon-ckks", "Updating user-controllable view status failed because our self peer is excluded or missing");
                self.nextState = self.peerMissingState;
            }
            return;
        }

        secnotice("octagon-ckks", "Received syncing policy %@ with view list: %@", syncingPolicy, syncingPolicy.viewList);

        NSError* stateError = nil;
        [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
            [metadata setTPSyncingPolicy:syncingPolicy];
            return metadata;
        } error:&stateError];

        if(stateError) {
            secerror("octagon: failed to save policy+views: %@", stateError);
            self.error = stateError;
            return;
        }

        [self.deps.viewManager setCurrentSyncingPolicy:syncingPolicy];

        self.nextState = self.intendedState;
    }];
}

@end

#endif // OCTAGON
