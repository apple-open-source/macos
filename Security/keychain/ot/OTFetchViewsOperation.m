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
#import "keychain/ckks/CKKSAnalytics.h"

@interface OTFetchViewsOperation ()
@property OTOperationDependencies* deps;
@property NSOperation* finishedOp;
@property CKKSViewManager* ckm;
@end

@implementation OTFetchViewsOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
{
    if ((self = [super init])) {
        _deps = dependencies;
        _ckm = dependencies.viewManager;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "fetching views");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSSet<NSString*>* sosViewList = [self.ckm viewList];
    self.policy = nil;
    self.viewList = sosViewList;

    if ([self.ckm useCKKSViewsFromPolicy]) {
        WEAKIFY(self);
        
        [self.deps.cuttlefishXPCWrapper fetchPolicyWithContainer:self.deps.containerName context:self.deps.contextID reply:^(TPPolicy* _Nullable policy, NSError* _Nullable error) {
                STRONGIFY(self);
                if (error) {
                    secerror("octagon: failed to retrieve policy: %@", error);
                    [[CKKSAnalytics logger] logResultForEvent:OctagonEventFetchViews hardFailure:true result:error];
                    self.error = error;
                    [self runBeforeGroupFinished:self.finishedOp];
                } else {
                    if (policy == nil) {
                        secerror("octagon: no policy returned");
                    }
                    self.policy = policy;
                    NSArray<NSString*>* sosViews = [sosViewList allObjects];
                    [self.deps.cuttlefishXPCWrapper getViewsWithContainer:self.deps.containerName context:self.deps.contextID inViews:sosViews reply:^(NSArray<NSString*>* _Nullable outViews, NSError* _Nullable error) {
                            STRONGIFY(self);
                            if (error) {
                                secerror("octagon: failed to retrieve list of views: %@", error);
                                [[CKKSAnalytics logger] logResultForEvent:OctagonEventFetchViews hardFailure:true result:error];
                                self.error = error;
                                [self runBeforeGroupFinished:self.finishedOp];
                            } else {
                                if (outViews == nil) {
                                    secerror("octagon: bad results from getviews");
                                } else {
                                    self.viewList = [NSSet setWithArray:outViews];
                                }
                                [self complete];
                            }
                        }];
                }
            }];
    } else {
        [self complete];
    }
}

- (void)complete {
    secnotice("octagon", "viewList: %@", self.viewList);
    self.ckm.policy = self.policy;
    self.ckm.viewList = self.viewList;

    [self.ckm createViews];
    [self.ckm beginCloudKitOperationOfAllViews];
    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON
