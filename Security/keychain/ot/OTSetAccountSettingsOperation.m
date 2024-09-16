/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#import <utilities/debugging.h>

#import "keychain/ot/OTSetAccountSettingsOperation.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@interface OTSetAccountSettingsOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishOp;
@end

@implementation OTSetAccountSettingsOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                            settings:(OTAccountSettings* _Nullable)settings
{
    if((self = [super init])) {
        _deps = dependencies;
        _settings = settings;
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];
    
    if (self.settings == nil) {
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    TPPBPeerStableInfoSetting *walrus = nil;
    if (_settings.hasWalrus && _settings.walrus != nil) {
        walrus = [[TPPBPeerStableInfoSetting alloc]init];
        walrus.value = self.settings.walrus.enabled;
    }
    
    TPPBPeerStableInfoSetting *webAccess = nil;
    if (_settings.hasWebAccess && _settings.webAccess != nil) {
        webAccess = [[TPPBPeerStableInfoSetting alloc]init];
        webAccess.value = self.settings.webAccess.enabled;
    }
    
    WEAKIFY(self);
    
    [self.deps.cuttlefishXPCWrapper updateWithSpecificUser:self.deps.activeAccount
                                              forceRefetch:NO
                                                deviceName:nil
                                              serialNumber:nil
                                                 osVersion:nil
                                             policyVersion:nil
                                             policySecrets:nil
                                 syncUserControllableViews:nil
                                     secureElementIdentity:nil
                                             walrusSetting:walrus
                                                 webAccess:webAccess
                                                     reply:^(TrustedPeersHelperPeerState* peerState, TPSyncingPolicy* syncingPolicy, NSError* error) {
        STRONGIFY(self);
        TPPBPeerStableInfoSetting *walrus = peerState.walrus;
        TPPBPeerStableInfoSetting *webAccess = peerState.webAccess;
        NSError *walrusError = nil;
        NSError *webAccessError = nil;


        
        if (self.settings.walrus != nil && (walrus == nil || walrus.value != self.settings.walrus.enabled)) {
            secerror("octagon: error setting walrus: Intended value: %@, final value: %@, error: %@",
                     self.settings.walrus.enabled ? @"ON": @"OFF",
                     walrus == nil ? @"none" : walrus.value ? @"ON": @"OFF",
                     error);
            walrusError = [NSError errorWithDomain:OctagonErrorDomain
                                              code:OctagonErrorFailedToSetWalrus
                                       description:@"Failed to set walrus setting"
                                        underlying:error];
        }
        if (self.settings.webAccess != nil && (webAccess == nil || webAccess.value != self.settings.webAccess.enabled)) {
            secerror("octagon: Error setting web access: Intended value: %@, final value: %@, error: %@",
                     self.settings.webAccess.enabled ? @"ON": @"OFF",
                     webAccess.value ? @"ON": @"OFF",
                     error);
            webAccessError = [NSError errorWithDomain:OctagonErrorDomain
                                                 code:OctagonErrorFailedToSetWebAccess
                                          description:@"Failed to set web access setting"
                                           underlying:error];
        }
        if (walrusError && webAccessError) { //nest em
            walrusError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorFailedToSetWalrus description:@"Failed to set walrus setting" underlying:webAccessError];
            self.error = walrusError;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        } else if (walrusError) {
            self.error = walrusError;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        } else if (webAccessError) {
            self.error = webAccessError;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishOp];
    }];
}

@end

#endif // OCTAGON
