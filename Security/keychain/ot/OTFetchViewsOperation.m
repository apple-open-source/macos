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

#import "keychain/ot/OTFetchViewsOperation.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ckks/CKKSAnalytics.h"

@interface OTFetchViewsOperation ()
@property OTOperationDependencies* deps;
@end

@implementation OTFetchViewsOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if ((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "fetching views");

    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper fetchCurrentPolicyWithContainer:self.deps.containerName
                                                            context:self.deps.contextID
                                                    modelIDOverride:nil
                                                              reply:^(TPSyncingPolicy* _Nullable syncingPolicy,
                                                                      TPPBPeerStableInfo_UserControllableViewStatus userControllableViewStatusOfPeers,
                                                                      NSError* _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventFetchViews hardFailure:true result:error];

        if (error) {
            secerror("octagon: failed to retrieve policy+views: %@", error);
            self.error = error;
            return;
        }

        secnotice("octagon-ckks", "Received syncing policy %@ with view list: %@", syncingPolicy, syncingPolicy.viewList);
        // Write them down before continuing

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
