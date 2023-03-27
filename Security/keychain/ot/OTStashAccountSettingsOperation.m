/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTStashAccountSettingsOperation.h"

#import "utilities/debugging.h"
#import "keychain/ot/CuttlefishXPCWrapper.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/proto/generated_source/OTAccountSettings.h"
#import "keychain/ot/proto/generated_source/OTWalrus.h"
#import "keychain/ot/proto/generated_source/OTWebAccess.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"

@interface OTStashAccountSettingsOperation ()
@property OTOperationDependencies* deps;
@property NSOperation* finishedOp;
@property id<OTAccountSettingsContainer> accountSettings;
@property bool accountWide;
@property bool forceFetch;
@end

@implementation OTStashAccountSettingsOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                     accountSettings:(id<OTAccountSettingsContainer>)accountSettings
                         accountWide:(bool)accountWide
                          forceFetch:(bool)forceFetch
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;

        _accountSettings = accountSettings;
        _accountWide = accountWide;
        _forceFetch = forceFetch;
    }
    return self;
}

+ (void)performWithAccountWide:(bool)accountWide
                    forceFetch:(bool)forceFetch
          cuttlefishXPCWrapper:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
                 activeAccount:(TPSpecificUser* _Nullable)activeAccount
                 containerName:(NSString*)containerName
                     contextID:(NSString*)contextID
                         reply:(void (^)(OTAccountSettings* _Nullable settings, NSError* _Nullable error))reply
{
    if (accountWide) {
        [cuttlefishXPCWrapper fetchAccountSettingsWithSpecificUser:activeAccount
                                                        forceFetch:forceFetch
                                                             reply:^(NSDictionary<NSString*, TPPBPeerStableInfoSetting *> * _Nullable retSettings,
                                                                     NSError * _Nullable operror) {
                if(operror) {
                    secnotice("octagon", "Unable to fetch account settings for (%@,%@): %@", containerName, contextID, operror);
                    reply(nil, operror);
                } else {
                    if (retSettings && [retSettings count]) {
                        OTAccountSettings* settings = [[OTAccountSettings alloc] init];
                        OTWalrus* walrus = [[OTWalrus alloc]init];
                        if (retSettings[@"walrus"] != nil) {
                            TPPBPeerStableInfoSetting *walrusSetting = retSettings[@"walrus"];
                            walrus.enabled = walrusSetting.value;
                        }
                        settings.walrus = walrus;
                        OTWebAccess* webAccess = [[OTWebAccess alloc]init];
                        if (retSettings[@"webAccess"] != nil) {
                            TPPBPeerStableInfoSetting *webAccessSetting = retSettings[@"webAccess"];
                            webAccess.enabled = webAccessSetting.value;
                        }
                        settings.webAccess = webAccess;
                        reply(settings, nil);
                    } else {
                        reply(nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoAccountSettingsSet userInfo: @{ NSLocalizedDescriptionKey : @"No account settings have been set"}]);
                    }
                }
            }];
    } else {
        [cuttlefishXPCWrapper fetchTrustStateWithSpecificUser:activeAccount
                                                        reply:^(TrustedPeersHelperPeerState * _Nullable selfPeerState,
                                                                NSArray<TrustedPeersHelperPeer *> * _Nullable trustedPeers,
                                                                NSError * _Nullable operror) {
                if(operror) {
                    secnotice("octagon", "Unable to fetch account settings for (%@,%@): %@", containerName, contextID, operror);
                    reply(nil, operror);
                } else {
                    OTAccountSettings* settings = [[OTAccountSettings alloc]init];
                    OTWalrus* walrus = [[OTWalrus alloc]init];
                    walrus.enabled = selfPeerState.walrus.value ? selfPeerState.walrus.value : false;
                    settings.walrus = walrus;
                    OTWebAccess* webAccess = [[OTWebAccess alloc]init];
                    webAccess.enabled = selfPeerState.webAccess.value ? selfPeerState.webAccess.value : false;
                    settings.webAccess = webAccess;
                    reply(settings, nil);
                }
            }];
    }
}

- (void)groupStart
{
    secnotice("octagon", "stashing account settings");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    WEAKIFY(self);
    [OTStashAccountSettingsOperation performWithAccountWide:self.accountWide
                                                 forceFetch:self.forceFetch
                                       cuttlefishXPCWrapper:self.deps.cuttlefishXPCWrapper
                                              activeAccount:self.deps.activeAccount
                                              containerName:self.deps.containerName
                                                  contextID:self.deps.contextID
                                                      reply:^(OTAccountSettings* _Nullable settings, NSError* _Nullable error) {
            STRONGIFY(self);
            if (error != nil) {
                self.error = error;
                [self.accountSettings setAccountSettings:nil];
            } else {
                self.nextState = self.intendedState;
                [self.accountSettings setAccountSettings:settings];
            }
            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
