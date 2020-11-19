/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import "utilities/debugging.h"

#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDetermineCDPBitStatusOperation.h"
#import "keychain/ot/OTStates.h"

@interface OTDetermineCDPBitStatusOperation ()
@property OTOperationDependencies* deps;
@end

@implementation OTDetermineCDPBitStatusOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon-cdp-status", "Checking CDP status");

    NSError* localError = nil;
    OTAccountMetadataClassC* account = [self.deps.stateHolder loadOrCreateAccountMetadata:&localError];
    if(localError && [self.deps.lockStateTracker isLockedError:localError]) {
        secnotice("octagon-cdp-status", "Device is locked! restarting on unlock");
        self.nextState = OctagonStateWaitForClassCUnlock;
        return;
    }

    if(localError) {
        secnotice("octagon-cdp-status", "Failed to load account metadata: %@", localError);
        self.error = localError;
        return;
    }

    secnotice("octagon-cdp-status", "CDP is %@", OTAccountMetadataClassC_CDPStateAsString(account.cdpState));

    if(account.cdpState == OTAccountMetadataClassC_CDPState_ENABLED) {
        self.nextState = self.intendedState;
    } else {
        // If the CDP status is unknown or disabled, double-check with TPH.
        // If there are any peers, the CDP status really should be ENABLED.
        __block OTAccountMetadataClassC_CDPState newState = OTAccountMetadataClassC_CDPState_UNKNOWN;

        WEAKIFY(self);
        [self.deps.cuttlefishXPCWrapper trustStatusWithContainer:self.deps.containerName
                                                         context:self.deps.contextID
                                                           reply:^(TrustedPeersHelperEgoPeerStatus *egoStatus,
                                                                   NSError *xpcError) {
            STRONGIFY(self);
            if(xpcError) {
                secnotice("octagon-cdp-status", "Unable to talk with TPH; leaving CDP status as 'unknown': %@", xpcError);
                return;
            }

            secnotice("octagon-cdp-status", "Octagon reports %d peers", (int)egoStatus.numberOfPeersInOctagon);
            if(egoStatus.numberOfPeersInOctagon > 0) {
                newState = OTAccountMetadataClassC_CDPState_ENABLED;
            } else {
                // As a last gasp, check in with SOS (if enabled). If there's a circle (in or out), CDP is on
                if(self.deps.sosAdapter.sosEnabled) {
                    secnotice("octagon-cdp-status", "Requesting SOS status...");

                    NSError* circleError = nil;
                    SOSCCStatus circleStatus = [self.deps.sosAdapter circleStatus:&circleError];

                    if(circleError || circleStatus == kSOSCCError) {
                        secnotice("octagon-cdp-status", "Error fetching circle status. Leaving CDP status as 'unknown': %@", circleError);
                    } else if(circleStatus == kSOSCCCircleAbsent) {
                        secnotice("octagon-cdp-status", "SOS reports circle absent. Setting CDP to 'disabled'");
                        newState = OTAccountMetadataClassC_CDPState_DISABLED;
                    } else {
                        secnotice("octagon-cdp-status", "SOS reports some existing circle (%d). Setting CDP to 'enabled'", (int)circleStatus);
                        newState = OTAccountMetadataClassC_CDPState_ENABLED;
                    }
                } else {
                    // No SOS? no CDP.
                    secnotice("octagon-cdp-status", "No SOS. CDP bit is off.");
                    newState = OTAccountMetadataClassC_CDPState_DISABLED;
                }
            }
        }];

        if(account.cdpState != newState) {
            NSError* stateError = nil;
            [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                if(metadata.cdpState == OTAccountMetadataClassC_CDPState_ENABLED) {
                    secnotice("octagon-cdp-status", "CDP bit is enabled on-disk, not modifying (would have been %@)", OTAccountMetadataClassC_CDPStateAsString(newState));

                    // Set this here to perform the right state choice later
                    newState = OTAccountMetadataClassC_CDPState_ENABLED;
                    return nil;
                } else {
                    secnotice("octagon-cdp-status", "Writing CDP bit as %@", OTAccountMetadataClassC_CDPStateAsString(newState));
                    metadata.cdpState = newState;
                    return metadata;
                }
            } error:&stateError];

            if(stateError) {
                secnotice("octagon-cdp-status", "Failed to load account metadata: %@", stateError);
            }
        }

        if(newState == OTAccountMetadataClassC_CDPState_ENABLED) {
            self.nextState = self.intendedState;
        } else {
            self.nextState = OctagonStateWaitForCDP;
        }
    }
}

@end

#endif // OCTAGON
