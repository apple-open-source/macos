/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#if __OBJC2__

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>

#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/SigninMetrics/OctagonSignPosts.h"

#import <utilities/SecCFWrappers.h>
#import <utilities/debugging.h>

#import "keychain/SecureObjectSync/SOSCloudCircle.h"
#import "KeychainCircle/PairingChannel.h"
#import <Security/SecBase.h>

const NSString* kSecEntitlementPrivateOctagonEscrow = @"com.apple.private.octagon.escrow-content";

#if OCTAGON
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#import <SoftLinking/SoftLinking.h>
#import <CloudServices/SecureBackup.h>
#import <CloudServices/SecureBackupConstants.h>
#import "keychain/ot/OTControl.h"
#import "keychain/ot/categories/OctagonEscrowRecoverer.h"

SOFT_LINK_FRAMEWORK(PrivateFrameworks, KeychainCircle);
SOFT_LINK_FRAMEWORK(PrivateFrameworks, CloudServices);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
SOFT_LINK_CLASS(KeychainCircle, KCPairingChannel);
SOFT_LINK_CLASS(KeychainCircle, OTPairingChannel);
SOFT_LINK_CLASS(CloudServices, SecureBackup);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupErrorDomain, NSErrorDomain);

#pragma clang diagnostic pop
#endif

OTCliqueCDPContextType OTCliqueCDPContextTypeNone = @"cdpContextTypeNone";
OTCliqueCDPContextType OTCliqueCDPContextTypeSignIn = @"cdpContextTypeSignIn";
OTCliqueCDPContextType OTCliqueCDPContextTypeRepair = @"cdpContextTypeRepair";
OTCliqueCDPContextType OTCliqueCDPContextTypeFinishPasscodeChange = @"cdpContextTypeFinishPasscodeChange";
OTCliqueCDPContextType OTCliqueCDPContextTypeRecoveryKeyGenerate = @"cdpContextTypeRecoveryKeyGenerate";
OTCliqueCDPContextType OTCliqueCDPContextTypeRecoveryKeyNew = @"cdpContextTypeRecoveryKeyNew";
OTCliqueCDPContextType OTCliqueCDPContextTypeUpdatePasscode = @"cdpContextTypeUpdatePasscode";

NSString* OTCliqueStatusToString(CliqueStatus status)
{
    switch(status) {
        case CliqueStatusIn:
            return @"CliqueStatusIn";
        case CliqueStatusNotIn:
            return @"CliqueStatusNotIn";
        case CliqueStatusPending:
            return @"CliqueStatusPending";
        case CliqueStatusAbsent:
            return @"CliqueStatusAbsent";
        case CliqueStatusNoCloudKitAccount:
            return @"CliqueStatusNoCloudKitAccount";
        case CliqueStatusError:
            return @"CliqueStatusError";
    };
}
CliqueStatus OTCliqueStatusFromString(NSString* str)
{
    if([str isEqualToString: @"CliqueStatusIn"]) {
        return CliqueStatusIn;
    } else if([str isEqualToString: @"CliqueStatusNotIn"]) {
        return CliqueStatusNotIn;
    } else if([str isEqualToString: @"CliqueStatusPending"]) {
        return CliqueStatusPending;
    } else if([str isEqualToString: @"CliqueStatusAbsent"]) {
        return CliqueStatusAbsent;
    } else if([str isEqualToString: @"CliqueStatusNoCloudKitAccount"]) {
        return CliqueStatusNoCloudKitAccount;
    } else if([str isEqualToString: @"CliqueStatusError"]) {
        return CliqueStatusError;
    }

    return CliqueStatusError;
}


@implementation OTConfigurationContext
- (OTControl* _Nullable)makeOTControl:(NSError**)error
{
#if OCTAGON
    if (self.otControl) {
        return self.otControl;
    }
    return [OTControl controlObject:true error:error];
#else
    return nil;
#endif
}
@end

@implementation OTBottleIDs
@end

@implementation OTOperationConfiguration

- (instancetype)init {
    if ((self = [super init]) == nil) {
        return nil;
    }
    _timeoutWaitForCKAccount = 10 * NSEC_PER_SEC;
    _qualityOfService = NSQualityOfServiceDefault;
    _discretionaryNetwork = NO;
    _useCachedAccountStatus = NO;
    return self;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    [coder encodeObject:@(_timeoutWaitForCKAccount) forKey:@"timeoutWaitForCKAccount"];
    [coder encodeObject:@(_qualityOfService) forKey:@"qualityOfService"];
    [coder encodeObject:@(_discretionaryNetwork) forKey:@"discretionaryNetwork"];
    [coder encodeObject:@(_useCachedAccountStatus) forKey:@"useCachedAccountStatus"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder *)coder {
    _timeoutWaitForCKAccount = [[coder decodeObjectOfClass:[NSNumber class] forKey:@"timeoutWaitForCKAccount"] unsignedLongLongValue];
    _qualityOfService = [[coder decodeObjectOfClass:[NSNumber class] forKey:@"qualityOfService"] integerValue];
    _discretionaryNetwork = [[coder decodeObjectOfClass:[NSNumber class] forKey:@"discretionaryNetwork"] boolValue];
    _useCachedAccountStatus = [[coder decodeObjectOfClass:[NSNumber class] forKey:@"useCachedAccountStatus"] boolValue];
    return self;
}

@end


@interface OTClique ()
@property (nonatomic, copy) NSString* cliqueMemberIdentifier;
@property (nonatomic, strong) OTConfigurationContext *ctx;
@property (nonatomic, strong) NSMutableDictionary *defaults;
@end

@implementation OTClique

+ (BOOL)platformSupportsSOS
{
    return (OctagonPlatformSupportsSOS() && OctagonIsSOSFeatureEnabled());
}

// defaults write com.apple.security.octagon enable -bool YES
-(BOOL)isOctagonPairingEnabled {
    BOOL nsDefaults = self.defaults[OTDefaultsOctagonEnable] ? [self.defaults[OTDefaultsOctagonEnable] boolValue] : OctagonIsEnabled();
    secnotice("octagon", "pairing is %@", nsDefaults ? @"on" : @"off");
    return nsDefaults;
}

- (void)setPairingDefault:(BOOL)defaults
{
    self.defaults[OTDefaultsOctagonEnable] = @(defaults);
}

- (void)removePairingDefault
{
    [self.defaults removeObjectForKey:OTDefaultsOctagonEnable];
}

- (instancetype)initWithContextData:(OTConfigurationContext *)ctx error:(NSError * __autoreleasing *)error
{
#if OCTAGON
    self = [super init];
    if(self){
        _ctx = [[OTConfigurationContext alloc]init];
        _ctx.context = ctx.context ?: OTDefaultContext;
        _ctx.dsid = [ctx.dsid copy];
        _ctx.altDSID = [ctx.altDSID copy];
        _ctx.analytics = ctx.analytics;
        _ctx.otControl = ctx.otControl;

        self.defaults = [NSMutableDictionary dictionary];
    }
    return self;
#else
    NSAssert(false, @"OTClique is not implemented on this platform");
    return nil;
#endif // OCTAGON
}

- (NSString* _Nullable)cliqueMemberIdentifier
{
#if OCTAGON
    __block NSString* retPeerID = nil;
    __block bool subTaskSuccess = false;

    OctagonSignpost fetchEgoPeerSignPost = OctagonSignpostBegin(OctagonSignpostNameFetchEgoPeer);
    if(OctagonIsEnabled()) {
        NSError* localError = nil;
        OTControl* control = [self makeOTControl:&localError];
        if(!control) {
            secerror("octagon: Failed to create OTControl: %@", localError);
            OctagonSignpostEnd(fetchEgoPeerSignPost, OctagonSignpostNameFetchEgoPeer, OctagonSignpostNumber1(OctagonSignpostNameFetchEgoPeer), (int)subTaskSuccess);
            return nil;
        }

        [control fetchEgoPeerID:nil
                        context:self.ctx.context
                          reply:^(NSString* peerID, NSError* error) {
                              if(error) {
                                  secerror("octagon: Failed to fetch octagon peer ID: %@", error);
                              }
                              retPeerID = peerID;
                          }];
        secnotice("clique", "cliqueMemberIdentifier(octagon) received %@", retPeerID);
    }

    if([OTClique platformSupportsSOS]) {
        CFErrorRef error = NULL;
        SOSPeerInfoRef me = SOSCCCopyMyPeerInfo(&error);
        retPeerID =  (NSString*)CFBridgingRelease(CFRetainSafe(SOSPeerInfoGetPeerID(me)));
        CFReleaseNull(me);
    }

    secnotice("clique", "cliqueMemberIdentifier complete: %@", retPeerID);
    subTaskSuccess = retPeerID ? true : false;
    OctagonSignpostEnd(fetchEgoPeerSignPost, OctagonSignpostNameFetchEgoPeer, OctagonSignpostNumber1(OctagonSignpostNameFetchEgoPeer), (int)subTaskSuccess);
    return retPeerID;
#else
    return nil;
#endif
}

#if OCTAGON
- (OTControl* _Nullable)makeOTControl:(NSError**)error
{
    return [self.ctx makeOTControl:error];
}

- (BOOL)establish:(NSError**)error
{
    secnotice("clique-establish", "establish started");
    OctagonSignpost establishSignPost = OctagonSignpostBegin(OctagonSignpostNameEstablish);
    bool subTaskSuccess = false;
    OTControl* control = [self makeOTControl:error];
    if(!control) {
        OctagonSignpostEnd(establishSignPost, OctagonSignpostNameEstablish, OctagonSignpostNumber1(OctagonSignpostNameEstablish), (int)subTaskSuccess);
        return false;
    }
    
    __block BOOL success = NO;
    __block NSError* localError = nil;

    //only establish
    [control establish:nil context:self.ctx.context altDSID:self.ctx.altDSID reply:^(NSError * _Nullable operationError) {
        if(operationError) {
            secnotice("clique-establish", "establish returned an error: %@", operationError);
        }
        success = !!operationError;
        localError = operationError;
    }];
    
    if(localError && error) {
        *error = localError;
    }
    secnotice("clique-establish", "establish complete: %@", success ? @"YES" : @"NO");
    subTaskSuccess = success ? true : false;
    OctagonSignpostEnd(establishSignPost, OctagonSignpostNameEstablish, OctagonSignpostNumber1(OctagonSignpostNameEstablish), (int)subTaskSuccess);

    return success;
}

- (BOOL)resetAndEstablish:(CuttlefishResetReason)resetReason error:(NSError**)error
{
    secnotice("clique-resetandestablish", "resetAndEstablish started");
    bool subTaskSuccess = false;
    OctagonSignpost resetAndEstablishSignPost = OctagonSignpostBegin(OctagonSignpostNameResetAndEstablish);

    OTControl* control = [self makeOTControl:error];

    if(!control) {
        OctagonSignpostEnd(resetAndEstablishSignPost, OctagonSignpostNameResetAndEstablish, OctagonSignpostNumber1(OctagonSignpostNameResetAndEstablish), (int)subTaskSuccess);
        return NO;
    }

    __block BOOL success = NO;
    __block NSError* localError = nil;
    [control resetAndEstablish:nil context:self.ctx.context altDSID:self.ctx.altDSID resetReason:resetReason reply:^(NSError * _Nullable operationError) {

        if(operationError) {
            secnotice("clique-resetandestablish", "resetAndEstablish returned an error: %@", operationError);
        }
        success = !!operationError;
        localError = operationError;
    }];

    if(localError && error) {
        *error = localError;
    }

    secnotice("clique-resetandestablish", "establish complete: %@", success ? @"YES" : @"NO");
    subTaskSuccess = success ? true : false;
    OctagonSignpostEnd(resetAndEstablishSignPost, OctagonSignpostNameResetAndEstablish, OctagonSignpostNumber1(OctagonSignpostNameResetAndEstablish), (int)subTaskSuccess);

    return success;
}
#endif // OCTAGON

+ (OTClique*)newFriendsWithContextData:(OTConfigurationContext*)data error:(NSError * __autoreleasing *)error
{
    return [OTClique newFriendsWithContextData:data resetReason:CuttlefishResetReasonUserInitiatedReset error:error];
}

+ (OTClique*)newFriendsWithContextData:(OTConfigurationContext*)data resetReason:(CuttlefishResetReason)resetReason error:(NSError * __autoreleasing *)error
{
#if OCTAGON
    secnotice("clique-newfriends", "makeNewFriends invoked using context: %@, dsid: %@", data.context, data.dsid);
    bool result = false;
    bool subTaskSuccess = false;
    OctagonSignpost performEscrowRecoverySignpost = OctagonSignpostBegin(OctagonSignpostNameMakeNewFriends);

    OTClique* clique = [[OTClique alloc] initWithContextData:data error:error];

    if(OctagonIsEnabled()) {
        NSError* localError = nil;
        [clique resetAndEstablish:resetReason error:&localError];

        if(localError) {
            secnotice("clique-newfriends", "account reset failed: %@", localError);
            if(error) {
                *error = localError;
            }
            OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNameMakeNewFriends, OctagonSignpostNumber1(OctagonSignpostNameMakeNewFriends), (int)subTaskSuccess);
            return nil;
        } else {
            secnotice("clique-newfriends", "Octagon account reset succeeded");
        }
    }

    if([OTClique platformSupportsSOS]) {
        CFErrorRef resetError = NULL;
        NSData* analyticsData = nil;
        if(data.analytics) {
            NSError* encodingError = nil;
            analyticsData = [NSKeyedArchiver archivedDataWithRootObject:data.analytics requiringSecureCoding:YES error:&encodingError];

            if(encodingError) {
                secnotice("clique-newfriends", "newFriendsWithContextData: unable to serialize analytics: %@", encodingError);
            }
        }

        result = SOSCCResetToOffering(&resetError);

        if(!result || resetError){
            secnotice("clique-newfriends", "newFriendsWithContextData: resetToOffering failed: %@", resetError);
            if(error) {
                *error = CFBridgingRelease(resetError);
            }
            OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNameMakeNewFriends, OctagonSignpostNumber1(OctagonSignpostNameMakeNewFriends), (int)subTaskSuccess);
            return nil;
        }
        secnotice("clique-newfriends", "newFriendsWithContextData: reset the SOS circle");
    } else {
        secnotice("clique-newfriends", "newFriendsWithContextData: SOS disabled on this platform");
    }
    secnotice("clique-newfriends", "makeNewFriends complete");

    subTaskSuccess = true;
    OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNameMakeNewFriends, OctagonSignpostNumber1(OctagonSignpostNameMakeNewFriends), (int)subTaskSuccess);

    return clique;

#else // !OCTAGON
    if (error)
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    return NULL;
#endif
}

+ (OTClique* _Nullable)performEscrowRecoveryWithContextData:(OTConfigurationContext*)data
                                            escrowArguments:(NSDictionary*)sbdRecoveryArguments
                                                      error:(NSError**)error
{
#if OCTAGON
    OctagonSignpost performEscrowRecoverySignpost = OctagonSignpostBegin(OctagonSignpostNamePerformEscrowRecovery);
    bool subTaskSuccess = false;
    NSError* localError = nil;
    OTClique* clique = [[OTClique alloc] initWithContextData:data
                                                       error:&localError];

    if(!clique || localError) {
        secnotice("clique-recovery", "unable to create otclique: %@", localError);
        if(error) {
            *error = localError;
        }
        OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);
        return nil;
    }

    // Attempt the recovery from sbd
    secnotice("clique-recovery", "attempting an escrow recovery for context:%@, altdsid:%@", data.context, data.altDSID);
    id<OctagonEscrowRecovererPrococol> sb = data.sbd ?: [[getSecureBackupClass() alloc] init];
    NSDictionary* recoveredInformation = nil;

    OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNamePerformRecoveryFromSBD);
    NSError* recoverError = [sb recoverWithInfo:sbdRecoveryArguments results:&recoveredInformation];
    subTaskSuccess = (recoverError == nil) ? true : false;
    OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNamePerformRecoveryFromSBD, OctagonSignpostNumber1(OctagonSignpostNamePerformRecoveryFromSBD), (int)subTaskSuccess);

    if(recoverError) {
        secnotice("clique-recovery", "sbd escrow recovery failed: %@", recoverError);
        if(recoverError.code == 17 /* kSecureBackupRestoringLegacyBackupKeychainError */ && [recoverError.domain isEqualToString:getkSecureBackupErrorDomain()]) { /* XXX */
            if([OTClique platformSupportsSOS]) {
                secnotice("clique-recovery", "Can't restore legacy backup with no keybag. Resetting SOS to offering");
                CFErrorRef blowItAwayError = NULL;
                bool successfulReset = SOSCCResetToOffering(&blowItAwayError);
                if(!successfulReset || blowItAwayError) {
                    secerror("clique-recovery: failed to reset to offering:%@", blowItAwayError);
                } else {
                    secnotice("clique-recovery", "resetting SOS circle successful");
                }
            } else {
                secnotice("clique-recovery", "Legacy restore failed on a non-SOS platform");
            }
        } else {
            if(error) {
                *error = recoverError;
            }
            subTaskSuccess = false;
            OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);
            return nil;
        }
    } else {
        if(OctagonPlatformSupportsSOS()) { // Join if the legacy restore is complete now.
            secnotice("clique-recovery", "attempting joinAfterRestore");
            [clique joinAfterRestore:&localError];
            secnotice("clique-recovery", "joinAfterRestore: %@", localError);
        }
    }

    // look for OT Bottles
    OTControl* control = [clique makeOTControl:&localError];
    if (!control) {
        secnotice("clique-recovery", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        subTaskSuccess = false;
        OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);
        return nil;
    }

    NSString *bottleID = recoveredInformation[@"bottleID"];
    NSString *isValid = recoveredInformation[@"bottleValid"];
    NSData *bottledPeerEntropy = recoveredInformation[@"EscrowServiceEscrowData"][@"BottledPeerEntropy"];
    bool shouldResetOctagon = false;
    
    if(bottledPeerEntropy && bottleID &&  [isValid isEqualToString:@"valid"]){
        secnotice("clique-recovery", "recovering from bottle: %@", bottleID);
        __block NSError* restoreBottleError = nil;

        OctagonSignpost bottleRecoverySignPost = OctagonSignpostBegin(OctagonSignpostNamePerformBottleRecovery);
        //restore bottle!
        [control restore:OTCKContainerName
               contextID:data.context
              bottleSalt:data.altDSID
                 entropy:bottledPeerEntropy
                bottleID:bottleID
                   reply:^(NSError * _Nullable restoreError) {
                if(restoreError) {
                    secnotice("clique-recovery", "restore bottle errored: %@", restoreError);
                } else {
                    secnotice("clique-recovery", "restoring bottle succeeded");
                }
                restoreBottleError = restoreError;
            }];

        subTaskSuccess = (restoreBottleError == nil) ? true : false;
        OctagonSignpostEnd(bottleRecoverySignPost, OctagonSignpostNamePerformBottleRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformBottleRecovery), (int)subTaskSuccess);

        if(restoreBottleError) {
            if(error){
                *error = restoreBottleError;
            }
            subTaskSuccess = false;
            OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);
            return nil;
        }
    } else {
        shouldResetOctagon = true;
    }

    if(shouldResetOctagon) {
        secnotice("clique-recovery", "bottle %@ is not valid, resetting octagon", bottleID);
        NSError* resetError = nil;

        OctagonSignpost resetSignPost = OctagonSignpostBegin(OctagonSignpostNamePerformResetAndEstablishAfterFailedBottle);
        [clique resetAndEstablish:CuttlefishResetReasonNoBottleDuringEscrowRecovery error:&resetError];
        subTaskSuccess = (resetError == nil) ? true : false;
        OctagonSignpostEnd(resetSignPost, OctagonSignpostNamePerformResetAndEstablishAfterFailedBottle, OctagonSignpostNumber1(OctagonSignpostNamePerformResetAndEstablishAfterFailedBottle), (int)subTaskSuccess);

        if(resetError) {
            secnotice("clique-recovery", "failed to reset octagon: %@", resetError);
        } else{
            secnotice("clique-recovery", "reset octagon succeeded");
        }
    }

    secnotice("clique-recovery", "recovery complete: %@", clique);

    subTaskSuccess = clique ? true : false;
    OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);

    return clique;
#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NULL;
#endif
}


- (KCPairingChannel *)setupPairingChannelAsInitiator:(KCPairingChannelContext *)ctx
{
#if OCTAGON
    return [getKCPairingChannelClass() pairingChannelInitiator:ctx];
#else
    return NULL;
#endif
}

- (KCPairingChannel * _Nullable)setupPairingChannelAsInitator:(KCPairingChannelContext *)ctx error:(NSError * __autoreleasing *)error
{
    if (error) {
        *error = nil;
    }
    return [self setupPairingChannelAsInitiator:ctx];
}

- (KCPairingChannel *)setupPairingChannelAsAcceptor:(KCPairingChannelContext *)ctx
{
#if OCTAGON
    return [getKCPairingChannelClass() pairingChannelAcceptor:ctx];
#else
    return NULL;
#endif
}

- (KCPairingChannel * _Nullable)setupPairingChannelAsAcceptor:(KCPairingChannelContext *)ctx error:(NSError * __autoreleasing *)error
{
    if (error) {
        *error = nil;
    }
    
    return [self setupPairingChannelAsAcceptor:ctx];
}


- (CliqueStatus)_fetchCliqueStatus:(OTOperationConfiguration *)configuration error:(NSError * __autoreleasing *)error
{
#if OCTAGON
    __block CliqueStatus sosStatus = CliqueStatusError;
    __block CliqueStatus octagonStatus = CliqueStatusError;
    bool subTaskSuccess = false;

    OctagonSignpost fetchCliqueStatusSignPost = OctagonSignpostBegin(OctagonSignpostNameFetchCliqueStatus);

    // Octagon is supreme.

    if(OctagonIsEnabled()) {
        OTControl* control = [self makeOTControl:error];
        if(!control) {
            secnotice("clique-status", "cliqueStatus noOTControl");
            OctagonSignpostEnd(fetchCliqueStatusSignPost, OctagonSignpostNameFetchCliqueStatus, OctagonSignpostNumber1(OctagonSignpostNameFetchCliqueStatus), (int)subTaskSuccess);

            return CliqueStatusError;
        }

        __block NSError* localError = nil;
        [control fetchCliqueStatus:nil context:self.ctx.context configuration:configuration reply:^(CliqueStatus cliqueStatus, NSError * _Nullable fetchError) {
            if(fetchError){
                octagonStatus = CliqueStatusError;
                localError = fetchError;
                secnotice("clique-status", "octagon clique status errored: %@", fetchError);
            } else {
                octagonStatus = cliqueStatus;
            }
        }];

        if(OctagonAuthoritativeTrustIsEnabled() || !OctagonPlatformSupportsSOS()) {
            secnotice("clique-status", "cliqueStatus(%{public}scached)(context:%@, altDSID:%@) returning %@ (error: %@)",
                      configuration.useCachedAccountStatus ? "" : "non-",
                      self.ctx.context, self.ctx.altDSID,
                      OTCliqueStatusToString(octagonStatus), localError);
            if (localError && error) {
                *error = localError;
                subTaskSuccess = false;
            } else {
                subTaskSuccess = true;
            }
            OctagonSignpostEnd(fetchCliqueStatusSignPost, OctagonSignpostNameFetchCliqueStatus, OctagonSignpostNumber1(OctagonSignpostNameFetchCliqueStatus), (int)subTaskSuccess);
            return octagonStatus;
        }
    }

    if([OTClique platformSupportsSOS]) {
        CFErrorRef circleStatusError = NULL;
        sosStatus = kSOSCCError;
        if(configuration.useCachedAccountStatus){
            sosStatus = SOSCCThisDeviceIsInCircle(&circleStatusError);
        } else {
            sosStatus = SOSCCThisDeviceIsInCircleNonCached(&circleStatusError);
        }
        secnotice("clique-status", "sos clique status is %d (%@)", (int)sosStatus, circleStatusError);

        if (error) {
            *error = (NSError*)CFBridgingRelease(circleStatusError);
        } else {
            CFBridgingRelease(circleStatusError);
        }
    }
    secnotice("clique-status", "cliqueStatus(%{public}scached)(context:%@, altDSID:%@) complete: %@",
              configuration.useCachedAccountStatus ? "" : "non-",
              self.ctx.context, self.ctx.altDSID,
              OTCliqueStatusToString(octagonStatus));

    subTaskSuccess = true;
    OctagonSignpostEnd(fetchCliqueStatusSignPost, OctagonSignpostNameFetchCliqueStatus, OctagonSignpostNumber1(OctagonSignpostNameFetchCliqueStatus), (int)subTaskSuccess);

    return octagonStatus;
#else // !OCTAGON
    if(error){
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return (CliqueStatus)kSOSCCError;
#endif
}

// Don't change rules for CoreCDP, and preserve legacy behavior for now
// preserve old behavior until CoreCDP can move to -fetchCliqueStatus:error:
#define LEGACY_WAITING_BEHAVIOR (TARGET_OS_OSX || TARGET_OS_IOS)

- (CliqueStatus)fetchCliqueStatus:(OTOperationConfiguration *)configuration error:(NSError * __autoreleasing * _Nonnull)error
{
    return [self _fetchCliqueStatus:configuration error:error];
}

- (CliqueStatus)fetchCliqueStatus:(NSError * __autoreleasing *)error
{
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];
#if LEGACY_WAITING_BEHAVIOR
    configuration.timeoutWaitForCKAccount = 0;
#endif
    return [self _fetchCliqueStatus:configuration error:error];
}

- (CliqueStatus)cachedCliqueStatus:(BOOL)usedCached error:(NSError * __autoreleasing *)error
{
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];
#if LEGACY_WAITING_BEHAVIOR
    configuration.timeoutWaitForCKAccount = 0;
#endif
    if (usedCached) {
        configuration.useCachedAccountStatus = YES;
    }
    return [self _fetchCliqueStatus:configuration error:error];
}


- (BOOL)removeFriendsInClique:(NSArray<NSString*>*)friendIdentifiers error:(NSError * __autoreleasing *)error
{
#if OCTAGON
    secnotice("clique-removefriends", "removeFriendsInClique invoked using context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost removeFriendsSignPost = OctagonSignpostBegin(OctagonSignpostNameRemoveFriendsInClique);
    bool subTaskSuccess = false;

    // Annoying: we must sort friendIdentifiers into octagon/sos lists.
    NSMutableArray<NSString*>* octagonIdentifiers = [NSMutableArray array];
    NSMutableArray<NSString*>* sosIdentifiers = [NSMutableArray array];

    for(NSString* friendIdentifier in friendIdentifiers) {
        if([friendIdentifier hasPrefix:@"SHA256:"]) {
            [octagonIdentifiers addObject: friendIdentifier];
        } else {
            [sosIdentifiers addObject: friendIdentifier];
        }
    }

    // Ensure that we don't have any peers on the wrong platform
    if(!OctagonIsEnabled() && octagonIdentifiers.count > 0) {
        NSError *localError = [NSError errorWithDomain:NSOSStatusErrorDomain
                                                  code:errSecUnimplemented
                                              userInfo:@{NSLocalizedDescriptionKey: @"Octagon is disabled; can't distrust any Octagon peers"}];
        secnotice("clique-removefriends", "removeFriendsInClique failed:%@", localError);
        if(error) {
            *error = localError;
        }
        OctagonSignpostEnd(removeFriendsSignPost, OctagonSignpostNameRemoveFriendsInClique, OctagonSignpostNumber1(OctagonSignpostNameRemoveFriendsInClique), (int)subTaskSuccess);
        return NO;
    }

    if(!OctagonPlatformSupportsSOS() && sosIdentifiers.count > 0) {
        NSError *localError = [NSError errorWithDomain:NSOSStatusErrorDomain
                                                  code:errSecUnimplemented
                                              userInfo:@{NSLocalizedDescriptionKey: @"SOS is not available on this platform; can't distrust any SOS peers"}];
        secnotice("clique-removefriends", "removeFriendsInClique failed:%@", localError);
        if(error) {
            *error = localError;
        }
        OctagonSignpostEnd(removeFriendsSignPost, OctagonSignpostNameRemoveFriendsInClique, OctagonSignpostNumber1(OctagonSignpostNameRemoveFriendsInClique), (int)subTaskSuccess);
        return NO;
    }


    __block NSError* localError = nil;
    bool result = true;

    if(OctagonIsEnabled() && octagonIdentifiers.count > 0) {
        OTControl* control = [self makeOTControl:error];
        if(!control) {
            OctagonSignpostEnd(removeFriendsSignPost, OctagonSignpostNameRemoveFriendsInClique, OctagonSignpostNumber1(OctagonSignpostNameRemoveFriendsInClique), (int)subTaskSuccess);
            return NO;
        }

        secnotice("clique-removefriends", "octagon: removing octagon friends: %@", octagonIdentifiers);
        [control removeFriendsInClique:nil
                               context:self.ctx.context
                               peerIDs:octagonIdentifiers
                                 reply:^(NSError* replyError) {
            if(replyError) {
                secnotice("clique-removefriends", "removeFriendsInClique failed: unable to remove friends: %@", replyError);
                localError = replyError;
            } else {
                secnotice("clique-removefriends", "octagon: friends removed: %@", octagonIdentifiers);
            }
        }];
    }

    if([OTClique platformSupportsSOS] && sosIdentifiers.count >0) {
        CFErrorRef removeFriendError = NULL;
        NSData* analyticsData = nil;

        secnotice("clique-removefriends", "removing sos friends: %@", sosIdentifiers);

        if(self.ctx.analytics){
            NSError* encodingError = nil;
            analyticsData = [NSKeyedArchiver archivedDataWithRootObject:self.ctx.analytics requiringSecureCoding:YES error:&encodingError];
        }

        if(analyticsData) {
            result = SOSCCRemovePeersFromCircleWithAnalytics((__bridge CFArrayRef)friendIdentifiers, (__bridge CFDataRef)analyticsData, &removeFriendError);
        } else {
            result = SOSCCRemovePeersFromCircle((__bridge CFArrayRef)friendIdentifiers, &removeFriendError);
        }

        if(removeFriendError) {
            secnotice("clique-removefriends", "removeFriendsInClique failed: unable to remove friends: %@", removeFriendError);
            localError = CFBridgingRelease(removeFriendError);
        }
    }

    if(error && localError) {
        *error = localError;
    }
    secnotice("clique-removefriends", "removeFriendsInClique complete: %d", result);

    subTaskSuccess = result;
    OctagonSignpostEnd(removeFriendsSignPost, OctagonSignpostNameRemoveFriendsInClique, OctagonSignpostNumber1(OctagonSignpostNameRemoveFriendsInClique), (int)subTaskSuccess);

    return result && localError == nil;
#else // !OCTAGON
    return NO;
#endif
}

- (BOOL)leaveClique:(NSError * __autoreleasing *)error
{
#if OCTAGON
    secnotice("clique-leaveClique", "leaveClique invoked using context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    CFErrorRef removeThisDeviceError = NULL;
    bool result = false;
    bool subTaskSuccess = false;

    OctagonSignpost leaveCliqueSignPost = OctagonSignpostBegin(OctagonSignpostNameLeaveClique);

    if(OctagonIsEnabled()) {
        OTControl* control = [self makeOTControl:error];
        if(!control) {
            OctagonSignpostEnd(leaveCliqueSignPost, OctagonSignpostNameLeaveClique, OctagonSignpostNumber1(OctagonSignpostNameLeaveClique), (int)subTaskSuccess);
            return NO;
        }

        // We only want to issue a "leave" command if we're actively in a clique
        __block NSError* localError = nil;
        CliqueStatus currentStatus = [self fetchCliqueStatus:[[OTOperationConfiguration alloc] init]
                                                       error:&localError];

        if(localError) {
            secnotice("clique-leaveClique", "fetching current status errored: %@", localError);
            if(error) {
                *error = localError;
            }
            OctagonSignpostEnd(leaveCliqueSignPost, OctagonSignpostNameLeaveClique, OctagonSignpostNumber1(OctagonSignpostNameLeaveClique), (int)subTaskSuccess);
            return NO;
        }

        if(currentStatus == CliqueStatusNotIn) {
            secnotice("clique-leaveClique", "current status is Not In; no need to leave");
            subTaskSuccess = true;
            OctagonSignpostEnd(leaveCliqueSignPost, OctagonSignpostNameLeaveClique, OctagonSignpostNumber1(OctagonSignpostNameLeaveClique), (int)subTaskSuccess);
            return YES;
        }
        [control leaveClique:nil context:self.ctx.context reply:^(NSError * _Nullable leaveError) {
            if(leaveError) {
                secnotice("clique-leaveClique", "leaveClique errored: %@", leaveError);
                localError = leaveError;
            } else {
                secnotice("clique-leaveClique", "leaveClique success.");
            }
        }];

        if(error) {
            *error = localError;
        }
        result = !localError;
    }

    if([OTClique platformSupportsSOS]) {
        NSData* analyticsData = nil;

        if(self.ctx.analytics) {
            NSError* encodingError = nil;
            analyticsData = [NSKeyedArchiver archivedDataWithRootObject:self.ctx.analytics requiringSecureCoding:YES error:&encodingError];
            if(!analyticsData){
                secnotice("clique-leaveClique", "leaveClique unable to archive analytics object: %@", encodingError);
            }
        }

        if(analyticsData) {
            result &= SOSCCRemoveThisDeviceFromCircleWithAnalytics((__bridge CFDataRef)analyticsData, &removeThisDeviceError);
        } else {
            result &= SOSCCRemoveThisDeviceFromCircle(&removeThisDeviceError);
        }
        if (error) {
            *error = (NSError*)CFBridgingRelease(removeThisDeviceError);
        } else {
            CFBridgingRelease(removeThisDeviceError);
        }
    }
    secnotice("clique-leaveClique", "leaveClique complete: %d", result);

    subTaskSuccess = result;
    OctagonSignpostEnd(leaveCliqueSignPost, OctagonSignpostNameLeaveClique, OctagonSignpostNumber1(OctagonSignpostNameLeaveClique), (int)subTaskSuccess);

    return result ? YES : NO;
#else // !OCTAGON
    return NO;
#endif
}

- (NSDictionary<NSString*,NSString*>* _Nullable)peerDeviceNamesByPeerID:(NSError * __autoreleasing *)error
{
#if OCTAGON
    secnotice("clique", "peerDeviceNamesByPeerID invoked using context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost peerNamesSignPost = OctagonSignpostBegin(OctagonSignpostNamePeerDeviceNamesByPeerID);
    __block bool subTaskSuccess = false;
    NSMutableDictionary<NSString*, NSString*>* retPeers = [NSMutableDictionary dictionary];

    if(OctagonIsEnabled()) {
        OTControl* control = [self makeOTControl:error];
        if(!control) {
            OctagonSignpostEnd(peerNamesSignPost, OctagonSignpostNamePeerDeviceNamesByPeerID, OctagonSignpostNumber1(OctagonSignpostNamePeerDeviceNamesByPeerID), (int)subTaskSuccess);
            return nil;
        }

        __block NSError* localError = nil;
        __block NSDictionary<NSString*, NSString*>* localPeers = nil;

        [control peerDeviceNamesByPeerID:nil context:OTDefaultContext reply:^(NSDictionary<NSString*,NSString*>* peers, NSError* controlError) {
            if(controlError) {
                secnotice("clique", "peerDeviceNamesByPeerID errored: %@", controlError);
            } else {
                secnotice("clique", "peerDeviceNamesByPeerID succeeded: %@", peers);
            }
            localError = controlError;
            localPeers = peers;
        }];

        if(error && localError) {
            *error = localError;
        }
        if(localError) {
            OctagonSignpostEnd(peerNamesSignPost, OctagonSignpostNamePeerDeviceNamesByPeerID, OctagonSignpostNumber1(OctagonSignpostNamePeerDeviceNamesByPeerID), (int)subTaskSuccess);
            return nil;
        }
        [retPeers addEntriesFromDictionary:localPeers];
        secnotice("clique", "Received %lu Octagon peers", (unsigned long)localPeers.count);
    }

    if([OTClique platformSupportsSOS]) {
        CFErrorRef peerErrorRef = NULL;
        NSMutableDictionary<NSString*,NSString*>* peerMapping = [NSMutableDictionary dictionary];
        NSArray* arrayOfPeerRefs = CFBridgingRelease(SOSCCCopyPeerPeerInfo(&peerErrorRef));
        if(arrayOfPeerRefs){
            [arrayOfPeerRefs enumerateObjectsUsingBlock:^(id peerRef, NSUInteger idx, BOOL * stop) {
                SOSPeerInfoRef peer = (__bridge SOSPeerInfoRef)peerRef;
                if(peer){
                    [peerMapping setObject:(__bridge NSString*)SOSPeerInfoGetPeerName(peer) forKey:(__bridge NSString*)SOSPeerInfoGetPeerID(peer)];
                }
            }];
        }
        subTaskSuccess = (peerErrorRef == NULL || [retPeers count] == 0) ? true : false;

        if (error) {
            *error = (NSError*)CFBridgingRelease(peerErrorRef);
        } else {
            CFBridgingRelease(peerErrorRef);
        }
        [retPeers addEntriesFromDictionary:peerMapping];
        secnotice("clique", "Received %lu SOS peers", (unsigned long)peerMapping.count);
    }

    OctagonSignpostEnd(peerNamesSignPost, OctagonSignpostNamePeerDeviceNamesByPeerID, OctagonSignpostNumber1(OctagonSignpostNamePeerDeviceNamesByPeerID), (int)subTaskSuccess);
    return retPeers;
#else // !OCTAGON
    return NULL;
#endif
}

- (BOOL)joinAfterRestore:(NSError * __autoreleasing *)error
{
    secnotice("clique-recovery", "joinAfterRestore for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost joinAfterRestoreSignPost = OctagonSignpostBegin(OctagonSignpostNameJoinAfterRestore);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef restoreError = NULL;
        bool res = SOSCCRequestToJoinCircleAfterRestore(&restoreError);
        if (error) {
            *error = (NSError*)CFBridgingRelease(restoreError);
        } else {
            CFBridgingRelease(restoreError);
        }
        secnotice("clique-recovery", "joinAfterRestore complete: %d %@", res, error ? *error : @"no error pointer provided");

        subTaskSuccess = res;
        OctagonSignpostEnd(joinAfterRestoreSignPost, OctagonSignpostNameJoinAfterRestore, OctagonSignpostNumber1(OctagonSignpostNameJoinAfterRestore), (int)subTaskSuccess);

        return res ? YES : NO;
    } else {
        secnotice("clique-recovery", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"join after restore unimplemented"}];
        }
        OctagonSignpostEnd(joinAfterRestoreSignPost, OctagonSignpostNameJoinAfterRestore, OctagonSignpostNumber1(OctagonSignpostNameJoinAfterRestore), (int)subTaskSuccess);
        return NO;
    }
}

- (BOOL)safariPasswordSyncingEnabled:(NSError **)error
{
    secnotice("clique-safari", "safariPasswordSyncingEnabled for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost safariSyncingEnabledSignPost = OctagonSignpostBegin(OctagonSignpostNameSafariPasswordSyncingEnabled);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef viewErrorRef = NULL;

        SOSViewResultCode result = SOSCCView(kSOSViewAutofillPasswords, kSOSCCViewQuery, &viewErrorRef);
        subTaskSuccess = (viewErrorRef == NULL) ? true : false;

        BOOL viewMember = result == kSOSCCViewMember;
        if (error) {
            *error = (NSError*)CFBridgingRelease(viewErrorRef);
        } else {
            CFBridgingRelease(viewErrorRef);
        }
        OctagonSignpostEnd(safariSyncingEnabledSignPost, OctagonSignpostNameSafariPasswordSyncingEnabled, OctagonSignpostNumber1(OctagonSignpostNameSafariPasswordSyncingEnabled), (int)subTaskSuccess);

        secnotice("clique-safari", "safariPasswordSyncingEnabled complete: %@", viewMember ? @"YES" : @"NO");
        return viewMember;
    } else {
        secnotice("clique-safari", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"safari password syncing enabled unimplemented"}];
        }
        OctagonSignpostEnd(safariSyncingEnabledSignPost, OctagonSignpostNameSafariPasswordSyncingEnabled, OctagonSignpostNumber1(OctagonSignpostNameSafariPasswordSyncingEnabled), (int)subTaskSuccess);
        return NO;
    }
}

- (BOOL)isLastFriend:(NSError **)error
{
    secnotice("clique-isLastFriend", "is last friend");
    return NO;
}

- (BOOL)waitForInitialSync:(NSError *__autoreleasing*)error
{
    secnotice("clique-legacy", "waitForInitialSync for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);

    OctagonSignpost waitForInitialSyncSignPost = OctagonSignpostBegin(OctagonSignpostNameWaitForInitialSync);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef initialSyncErrorRef = NULL;
        bool result = false;
        if(self.ctx.analytics){
            NSError* encodingError = nil;
            NSData* analyticsData = [NSKeyedArchiver archivedDataWithRootObject:self.ctx.analytics requiringSecureCoding:YES error:&encodingError];
            if(!encodingError && analyticsData){
                result = SOSCCWaitForInitialSyncWithAnalytics((__bridge CFDataRef)analyticsData, &initialSyncErrorRef);
            }else{
                result = SOSCCWaitForInitialSync(&initialSyncErrorRef);
            }
        }else{
            result = SOSCCWaitForInitialSync(&initialSyncErrorRef);
        }

        BOOL initialSyncResult = result ? YES : NO;
        if (error) {
            *error = (NSError*)CFBridgingRelease(initialSyncErrorRef);
        } else {
            CFBridgingRelease(initialSyncErrorRef);
        }
        secnotice("clique-legacy", "waitForInitialSync waited: %d %@", initialSyncResult, error ? *error : @"no error pointer provided");

        subTaskSuccess = initialSyncResult ? true : false;
        OctagonSignpostEnd(waitForInitialSyncSignPost, OctagonSignpostNameWaitForInitialSync, OctagonSignpostNumber1(OctagonSignpostNameWaitForInitialSync), (int)subTaskSuccess);

        return initialSyncResult;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"wait for initial sync unimplemented"}];
        }
        OctagonSignpostEnd(waitForInitialSyncSignPost, OctagonSignpostNameWaitForInitialSync, OctagonSignpostNumber1(OctagonSignpostNameWaitForInitialSync), (int)subTaskSuccess);
        return NO;
    }
}

- (NSArray* _Nullable)copyViewUnawarePeerInfo:(NSError *__autoreleasing*)error
{
    secnotice("clique-legacy", "copyViewUnawarePeerInfo for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);

    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameCopyViewUnawarePeerInfo);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef copyViewUnawarePeerInfoErrorRef = NULL;
        CFArrayRef peerListRef = SOSCCCopyViewUnawarePeerInfo(&copyViewUnawarePeerInfoErrorRef);

        NSArray* peerList = (peerListRef ? (NSArray*)(CFBridgingRelease(peerListRef)) : nil);
        if (error) {
            *error = (NSError*)CFBridgingRelease(copyViewUnawarePeerInfoErrorRef);
        } else {
            CFBridgingRelease(copyViewUnawarePeerInfoErrorRef);
        }
        subTaskSuccess = (peerList != nil) ? true : false;
        OctagonSignpostEnd(signPost, OctagonSignpostNameCopyViewUnawarePeerInfo, OctagonSignpostNumber1(OctagonSignpostNameCopyViewUnawarePeerInfo), (int)subTaskSuccess);

        return peerList;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NULL");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"copy view unaware peer info unimplemented"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameCopyViewUnawarePeerInfo, OctagonSignpostNumber1(OctagonSignpostNameCopyViewUnawarePeerInfo), (int)subTaskSuccess);
        return nil;
    }
}

- (BOOL)viewSet:(NSSet*)enabledViews disabledViews:(NSSet*)disabledViews
{
    secnotice("clique-legacy", "viewSet for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameViewSet);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        bool result = false;
        if(self.ctx.analytics){
            NSError* encodingError = nil;
            NSData* analyticsData = [NSKeyedArchiver archivedDataWithRootObject:self.ctx.analytics requiringSecureCoding:YES error:&encodingError];
            if(!encodingError && analyticsData){
                result = SOSCCViewSetWithAnalytics((__bridge CFSetRef)enabledViews, (__bridge CFSetRef)disabledViews, (__bridge CFDataRef)analyticsData);
            }else{
                result = SOSCCViewSet((__bridge CFSetRef)enabledViews, (__bridge CFSetRef)disabledViews);
            }
        }else{
            result = SOSCCViewSet((__bridge CFSetRef)enabledViews, (__bridge CFSetRef)disabledViews);
        }

        BOOL viewSetResult = result ? YES : NO;
        subTaskSuccess = result;
        OctagonSignpostEnd(signPost, OctagonSignpostNameViewSet, OctagonSignpostNumber1(OctagonSignpostNameViewSet), (int)subTaskSuccess);
        return viewSetResult;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        OctagonSignpostEnd(signPost, OctagonSignpostNameViewSet, OctagonSignpostNumber1(OctagonSignpostNameViewSet), (int)subTaskSuccess);
        return NO;
    }
}

- (BOOL)setUserCredentialsAndDSID:(NSString*)userLabel
                         password:(NSData*)userPassword
                            error:(NSError *__autoreleasing*)error
{
    secnotice("clique-legacy", "setUserCredentialsAndDSID for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameSetUserCredentialsAndDSID);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef setCredentialsErrorRef = NULL;
        bool result = false;
        if(self.ctx.analytics){
            NSError* encodingError = nil;
            NSData* analyticsData = [NSKeyedArchiver archivedDataWithRootObject:self.ctx.analytics requiringSecureCoding:YES error:&encodingError];
            if(!encodingError && analyticsData){
                result = SOSCCSetUserCredentialsAndDSIDWithAnalytics((__bridge CFStringRef)userLabel,
                                                                     (__bridge CFDataRef)userPassword,
                                                                     (__bridge CFStringRef)self.ctx.dsid,
                                                                     (__bridge CFDataRef)analyticsData,
                                                                     &setCredentialsErrorRef);
            }else{
                result = SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef)userLabel,
                                                        (__bridge CFDataRef)userPassword,
                                                        (__bridge CFStringRef)self.ctx.dsid,
                                                        &setCredentialsErrorRef);
            }
        }else{
            result = SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef)userLabel,
                                                    (__bridge CFDataRef)userPassword,
                                                    (__bridge CFStringRef)self.ctx.dsid,
                                                    &setCredentialsErrorRef);
        }

        BOOL setCredentialsResult = result ? YES : NO;
        if (error) {
            *error = (NSError*)CFBridgingRelease(setCredentialsErrorRef);
        } else {
            CFBridgingRelease(setCredentialsErrorRef);
        }
        secnotice("clique-legacy", "setUserCredentialsAndDSID results: %d %@", setCredentialsResult, setCredentialsErrorRef);
        subTaskSuccess = result;
        OctagonSignpostEnd(signPost, OctagonSignpostNameSetUserCredentialsAndDSID, OctagonSignpostNumber1(OctagonSignpostNameSetUserCredentialsAndDSID), (int)subTaskSuccess);

        return setCredentialsResult;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"set user credentials unimplemented"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameSetUserCredentialsAndDSID, OctagonSignpostNumber1(OctagonSignpostNameSetUserCredentialsAndDSID), (int)subTaskSuccess);
        return NO;
    }
}

- (BOOL)tryUserCredentialsAndDSID:(NSString*)userLabel
                         password:(NSData*)userPassword
                            error:(NSError *__autoreleasing*)error
{
    secnotice("clique-legacy", "tryUserCredentialsAndDSID for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameTryUserCredentialsAndDSID);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef tryCredentialsErrorRef = NULL;
        bool result = SOSCCTryUserCredentialsAndDSID((__bridge CFStringRef)userLabel,
                                                     (__bridge CFDataRef)userPassword,
                                                     (__bridge CFStringRef)self.ctx.dsid,
                                                     &tryCredentialsErrorRef);

        BOOL tryCredentialsResult = result ? YES : NO;
        if (error) {
            *error = (NSError*)CFBridgingRelease(tryCredentialsErrorRef);
        } else {
            CFBridgingRelease(tryCredentialsErrorRef);
        }
        secnotice("clique-legacy", "tryUserCredentialsAndDSID results: %d %@", tryCredentialsResult, tryCredentialsErrorRef);
        subTaskSuccess = result;
        OctagonSignpostEnd(signPost, OctagonSignpostNameTryUserCredentialsAndDSID, OctagonSignpostNumber1(OctagonSignpostNameTryUserCredentialsAndDSID), (int)subTaskSuccess);
        return tryCredentialsResult;

    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"try user credentials unimplemented"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameTryUserCredentialsAndDSID, OctagonSignpostNumber1(OctagonSignpostNameTryUserCredentialsAndDSID), (int)subTaskSuccess);
        return NO;
    }
}

- (NSArray* _Nullable)copyPeerPeerInfo:(NSError *__autoreleasing*)error
{
    secnotice("clique-legacy", "copyPeerPeerInfo for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameCopyPeerPeerInfo);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef copyPeerErrorRef = NULL;
        CFArrayRef result = SOSCCCopyPeerPeerInfo(&copyPeerErrorRef);

        NSArray* peerList = (result ? (NSArray*)(CFBridgingRelease(result)) : nil);

        if (error) {
            *error = (NSError*)CFBridgingRelease(copyPeerErrorRef);
        } else {
            CFBridgingRelease(copyPeerErrorRef);
        }
        secnotice("clique-legacy", "copyPeerPeerInfo results: %@", peerList);
        subTaskSuccess = (peerList != nil) ? true : false;
        OctagonSignpostEnd(signPost, OctagonSignpostNameCopyPeerPeerInfo, OctagonSignpostNumber1(OctagonSignpostNameCopyPeerPeerInfo), (int)subTaskSuccess);
        return peerList;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"copy peer peer info unimplemented"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameCopyPeerPeerInfo, OctagonSignpostNumber1(OctagonSignpostNameCopyPeerPeerInfo), (int)subTaskSuccess);
        return nil;
    }
}

- (BOOL)peersHaveViewsEnabled:(NSArray<NSString*>*)viewNames error:(NSError *__autoreleasing*)error
{
    secnotice("clique-legacy", "peersHaveViewsEnabled for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNamePeersHaveViewsEnabled);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        CFErrorRef viewsEnabledErrorRef = NULL;
        BOOL viewsEnabledResult = NO;

        CFBooleanRef result = SOSCCPeersHaveViewsEnabled((__bridge CFArrayRef)viewNames, &viewsEnabledErrorRef);
        if(result){
            viewsEnabledResult = CFBooleanGetValue(result) ? YES : NO;
        }
        if (error) {
            *error = (NSError*)CFBridgingRelease(viewsEnabledErrorRef);
        } else {
            CFBridgingRelease(viewsEnabledErrorRef);
        }
        secnotice("clique-legacy", "peersHaveViewsEnabled results: %@", viewsEnabledResult ? @"YES" : @"NO");
        subTaskSuccess = viewsEnabledResult ? true : false;
        OctagonSignpostEnd(signPost, OctagonSignpostNamePeersHaveViewsEnabled, OctagonSignpostNumber1(OctagonSignpostNamePeersHaveViewsEnabled), (int)subTaskSuccess);
        return viewsEnabledResult;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        if(error){
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"peers have views enabled unimplemented"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNamePeersHaveViewsEnabled, OctagonSignpostNumber1(OctagonSignpostNamePeersHaveViewsEnabled), (int)subTaskSuccess);
        return NO;
    }
}

- (BOOL)requestToJoinCircle:(NSError *__autoreleasing*)error
{
    bool result = false;
    CFErrorRef joinErrorRef = NULL;
    bool subTaskSuccess = false;
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameRequestToJoinCircle);

#if OCTAGON
    secnotice("clique-legacy", "requestToJoinCircle for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);

    if(OctagonIsEnabled()) {
        // Sometimes, CoreCDP calls this to cause a circle creation to occur.
        // So, for Octagon, we might want to request a establish, but not a reset.

        // Fetch the current trust status, so we know if we should fire off the establish.
        NSError* localError = nil;
        CliqueStatus status = [self fetchCliqueStatus: &localError];

        if(localError) {
            secnotice("clique-legacy", "fetching clique status failed: %@", localError);
            if(error) {
                *error = localError;
            }
            OctagonSignpostEnd(signPost, OctagonSignpostNameRequestToJoinCircle, OctagonSignpostNumber1(OctagonSignpostNameRequestToJoinCircle), (int)subTaskSuccess);
            return NO;
        }

        if(status == CliqueStatusAbsent) {
            secnotice("clique-legacy", "clique status is %@; beginning an establish", OTCliqueStatusToString(status));
            [self establish:&localError];

            if(localError) {
                if(error) {
                    *error = localError;
                }
                OctagonSignpostEnd(signPost, OctagonSignpostNameRequestToJoinCircle, OctagonSignpostNumber1(OctagonSignpostNameRequestToJoinCircle), (int)subTaskSuccess);
                return NO;
            } else {
                secnotice("clique-legacy", "establish succeeded");
            }
        } else {
            secnotice("clique-legacy", "clique status is %@; performing no Octagon actions", OTCliqueStatusToString(status));
        }

        // If we didn't early-exit, and we aren't going to invoke SOS below, we succeeded.
        if(!OctagonPlatformSupportsSOS()) {
            secnotice("clique-legacy", "requestToJoinCircle results: %d %@", result, joinErrorRef);
            subTaskSuccess = true;
            OctagonSignpostEnd(signPost, OctagonSignpostNameRequestToJoinCircle, OctagonSignpostNumber1(OctagonSignpostNameRequestToJoinCircle), (int)subTaskSuccess);
            return YES;
        }
    }
#endif // OCTAGON

    if([OTClique platformSupportsSOS]) {
        NSData* analyticsData = nil;
        if(self.ctx.analytics){
            NSError* encodingError = nil;
            analyticsData = [NSKeyedArchiver archivedDataWithRootObject:self.ctx.analytics requiringSecureCoding:YES error:&encodingError];
        }

        if(analyticsData){
            result = SOSCCRequestToJoinCircleWithAnalytics((__bridge CFDataRef)analyticsData, &joinErrorRef);
        } else {
            result = SOSCCRequestToJoinCircle(&joinErrorRef);
        }

        secnotice("clique-legacy", "sos requestToJoinCircle complete: %d %@", result, joinErrorRef);
    }

    if (error) {
        *error = (NSError*)CFBridgingRelease(joinErrorRef);
    } else {
        CFBridgingRelease(joinErrorRef);
    }
    subTaskSuccess = result;
    OctagonSignpostEnd(signPost, OctagonSignpostNameRequestToJoinCircle, OctagonSignpostNumber1(OctagonSignpostNameRequestToJoinCircle), (int)subTaskSuccess);

    return result ? YES : NO;
}

- (BOOL)accountUserKeyAvailable
{
    secnotice("clique-legacy", "accountUserKeyAvailable for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameAccountUserKeyAvailable);
    bool subTaskSuccess = false;

    if([OTClique platformSupportsSOS]) {
        BOOL canAuthenticate = SOSCCCanAuthenticate(NULL) ? YES : NO;
        if (canAuthenticate == NO) {
            secnotice("clique-legacy", "Security requires credentials...");
        }
        subTaskSuccess = canAuthenticate ? true : false;
        OctagonSignpostEnd(signPost, OctagonSignpostNameAccountUserKeyAvailable, OctagonSignpostNumber1(OctagonSignpostNameAccountUserKeyAvailable), (int)subTaskSuccess);
        return canAuthenticate;
    } else {
        secnotice("clique-legacy", "SOS disabled for this platform, returning NO");
        OctagonSignpostEnd(signPost, OctagonSignpostNameAccountUserKeyAvailable, OctagonSignpostNumber1(OctagonSignpostNameAccountUserKeyAvailable), (int)subTaskSuccess);
        return NO;
    }
}

// MARK: SBD interfaces
+ (OTBottleIDs* _Nullable)findOptimalBottleIDsWithContextData:(OTConfigurationContext*)data
                                                        error:(NSError**)error
{
#if OCTAGON
    secnotice("clique-findbottle", "finding optimal bottles for context:%@, altdsid:%@", data.context, data.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameFindOptimalBottleIDsWithContextData);
    bool subTaskSuccess = false;

    if(OctagonIsEnabled()) {
        __block NSError* localError = nil;
        __block NSArray<NSString*>* localViableBottleIDs = nil;
        __block NSArray<NSString*>* localPartiallyViableBottleIDs = nil;

        OTControl *control = [data makeOTControl:&localError];
        if (!control) {
            secnotice("clique-findbottle", "unable to create otcontrol: %@", localError);
            if (error) {
                *error = localError;
            }
            OctagonSignpostEnd(signPost, OctagonSignpostNameFindOptimalBottleIDsWithContextData, OctagonSignpostNumber1(OctagonSignpostNameFindOptimalBottleIDsWithContextData), (int)subTaskSuccess);
            return nil;
        }
        [control fetchAllViableBottles:OTCKContainerName
                               context:data.context
                                 reply:^(NSArray<NSString *> * _Nullable sortedBottleIDs,
                                         NSArray<NSString*> * _Nullable sortedPartialBottleIDs,
                                         NSError * _Nullable fetchError) {
            if(fetchError) {
                secnotice("clique-findbottle", "findOptimalBottleIDsWithContextData errored: %@", fetchError);
            } else {
                secnotice("clique-findbottle", "findOptimalBottleIDsWithContextData succeeded: %@, %@", sortedBottleIDs, sortedPartialBottleIDs);
            }
            localError = fetchError;
            localViableBottleIDs = sortedBottleIDs;
            localPartiallyViableBottleIDs = sortedPartialBottleIDs;
        }];

        if(error && localError) {
            *error = localError;
        }
        OTBottleIDs* bottleIDs = [[OTBottleIDs alloc] init];
        bottleIDs.preferredBottleIDs = localViableBottleIDs;
        bottleIDs.partialRecoveryBottleIDs = localPartiallyViableBottleIDs;

        secnotice("clique-findbottle", "findOptimalBottleIDsWithContextData complete");

        subTaskSuccess = (localError == nil) ? true : false;
        OctagonSignpostEnd(signPost, OctagonSignpostNameFindOptimalBottleIDsWithContextData, OctagonSignpostNumber1(OctagonSignpostNameFindOptimalBottleIDsWithContextData), (int)subTaskSuccess);
        return bottleIDs;
    } else {
        // With octagon off, fail with 'unimplemented'
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecUnimplemented
                                     userInfo:@{NSLocalizedDescriptionKey: @"optimal bottle IDs unimplemented"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameFindOptimalBottleIDsWithContextData, OctagonSignpostNumber1(OctagonSignpostNameFindOptimalBottleIDsWithContextData), (int)subTaskSuccess);
        return nil;
    }
#else // !OCTAGON
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NULL;
#endif
}

+ (OTClique* _Nullable)recoverWithContextData:(OTConfigurationContext*)data
                                     bottleID:(NSString*)bottleID
                              escrowedEntropy:(NSData*)entropy
                                        error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon", "replaced by performEscrowRecoveryWithContextData:escrowArguments:error: remove call");
    return nil;
#else // !OCTAGON
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NULL;
#endif
}

// used by sbd to fill in the escrow record
// TODO: what extra entitlement do you need to call this?
- (void)fetchEscrowContents:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{
#if OCTAGON
    secnotice("clique-fetchescrow", "fetching entropy for bottling for context:%@, altdsid:%@", self.ctx.context, self.ctx.altDSID);
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameFetchEscrowContents);
    __block bool subTaskSuccess = false;
    if(OctagonIsEnabled()) {
        NSError* controlError = nil;
        OTControl* control = [self makeOTControl:&controlError];
        if (!control) {
            OctagonSignpostEnd(signPost, OctagonSignpostNameFetchEscrowContents, OctagonSignpostNumber1(OctagonSignpostNameFetchEscrowContents), (int)subTaskSuccess);
            reply(nil, nil, nil, controlError);
            return;
        }
        [control fetchEscrowContents:OTCKContainerName
                           contextID:self.ctx.context
                               reply:^(NSData * _Nullable entropy,
                                       NSString * _Nullable bottleID,
                                       NSData * _Nullable signingPublicKey,
                                       NSError * _Nullable error) {
            if(error){
                secnotice("clique-fetchescrow", "fetchEscrowContents errored: %@", error);
            } else{
                secnotice("clique-fetchescrow","fetchEscrowContents succeeded");
            }
            subTaskSuccess = (error == nil) ? true : false;
            OctagonSignpostEnd(signPost, OctagonSignpostNameFetchEscrowContents, OctagonSignpostNumber1(OctagonSignpostNameFetchEscrowContents), (int)subTaskSuccess);
            reply (entropy, bottleID, signingPublicKey, error);
        }];
    } else {
        // With octagon off, fail with 'unimplemented'
        OctagonSignpostEnd(signPost, OctagonSignpostNameFetchEscrowContents, OctagonSignpostNumber1(OctagonSignpostNameFetchEscrowContents), (int)subTaskSuccess);
        reply(nil, nil, nil, [NSError errorWithDomain:NSOSStatusErrorDomain
                                                 code:errSecUnimplemented
                                             userInfo:@{NSLocalizedDescriptionKey: @"fetchEscrowRecordContents unimplemented"}]);
    }
#else // !OCTAGON
    reply(nil, nil, nil, [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil]);
#endif
}

+ (void)setNewRecoveryKeyWithData:(OTConfigurationContext *)ctx
                      recoveryKey:(NSString*)recoveryKey reply:(nonnull void (^)(SecRecoveryKey *rk, NSError *error))reply
{
#if OCTAGON
    secnotice("octagon-setrecoverykey", "setNewRecoveryKeyWithData invoked for context: %@", ctx.context);
    //set the recovery key for SOS
    NSError* createRecoveryKeyError = nil;
    NSMutableDictionary *userInfo = [NSMutableDictionary new];
    NSError* retError = nil;
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameSetNewRecoveryKeyWithData);
    __block bool subTaskSuccess = false;

    SecRecoveryKey *rk = SecRKCreateRecoveryKeyWithError(recoveryKey, &createRecoveryKeyError);
    if (rk == nil) {
        secerror("octagon-setrecoverykey, SecRKCreateRecoveryKeyWithError() failed: %@", createRecoveryKeyError);
        userInfo[NSLocalizedDescriptionKey] = @"SecRKCreateRecoveryKeyWithError() failed";
        userInfo[NSUnderlyingErrorKey] = createRecoveryKeyError;
        retError = [NSError errorWithDomain:getkSecureBackupErrorDomain() code:kSecureBackupInternalError userInfo:userInfo];
        OctagonSignpostEnd(signPost, OctagonSignpostNameSetNewRecoveryKeyWithData, OctagonSignpostNumber1(OctagonSignpostNameSetNewRecoveryKeyWithData), (int)subTaskSuccess);
        reply(nil, retError);
        return;
    }
    if([OTClique platformSupportsSOS]) {
        CFErrorRef registerError = nil;
        if (!SecRKRegisterBackupPublicKey(rk, &registerError)) {
            secerror("octagon-setrecoverykey, SecRKRegisterBackupPublicKey() failed: %@", registerError);
            NSError *underlyingError = CFBridgingRelease(registerError);
            userInfo[NSLocalizedDescriptionKey] = @"SecRKRegisterBackupPublicKey() failed";
            userInfo[NSUnderlyingErrorKey] = underlyingError;
            retError = [NSError errorWithDomain:getkSecureBackupErrorDomain() code:kSecureBackupInternalError userInfo:userInfo];
            OctagonSignpostEnd(signPost, OctagonSignpostNameSetNewRecoveryKeyWithData, OctagonSignpostNumber1(OctagonSignpostNameSetNewRecoveryKeyWithData), (int)subTaskSuccess);
            reply(nil,retError);
            return;
        } else {
            secnotice("octagon-setrecoverykey", "successfully registered recovery key for SOS");
        }
    }

    //set the recovery key for Octagon
    if(OctagonRecoveryKeyIsEnabled()) {
        NSError* controlError = nil;
        OTControl* control = [ctx makeOTControl:&controlError];
        if(!control) {
            secnotice("octagon-setrecoverykey", "failed to fetch OTControl object: %@", controlError);
            OctagonSignpostEnd(signPost, OctagonSignpostNameSetNewRecoveryKeyWithData, OctagonSignpostNumber1(OctagonSignpostNameSetNewRecoveryKeyWithData), (int)subTaskSuccess);
            reply(nil, controlError);
            return;
        }
        [control createRecoveryKey:OTCKContainerName contextID:ctx.context recoveryKey:recoveryKey reply:^(NSError * createError) {
            if(createError){
                secerror("octagon-setrecoverykey, failed to create octagon recovery key");
                OctagonSignpostEnd(signPost, OctagonSignpostNameSetNewRecoveryKeyWithData, OctagonSignpostNumber1(OctagonSignpostNameSetNewRecoveryKeyWithData), (int)subTaskSuccess);
                reply(nil, createError);
                return;
            } else {
                secnotice("octagon-setrecoverykey", "successfully set octagon recovery key");
                subTaskSuccess = true;
                OctagonSignpostEnd(signPost, OctagonSignpostNameSetNewRecoveryKeyWithData, OctagonSignpostNumber1(OctagonSignpostNameSetNewRecoveryKeyWithData), (int)subTaskSuccess);
                reply(rk, nil);
                return;
            }
        }];
    }
#else // !OCTAGON
    reply(nil, [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil]);
#endif
}

+ (void)recoverOctagonUsingData:(OTConfigurationContext *)ctx
                    recoveryKey:(NSString*)recoveryKey
                          reply:(void(^)(NSError* _Nullable error))reply
{
#if OCTAGON
    OctagonSignpost signpost = OctagonSignpostBegin(OctagonSignpostNameRecoverOctagonUsingData);
    __block bool subTaskSuccess = false;

    if(OctagonRecoveryKeyIsEnabled()) {
        NSError* controlError = nil;
        OTControl* control = [ctx makeOTControl:&controlError];

        secnotice("clique-recoverykey", "join using recovery key");

        if(!control) {
            secnotice("clique-recoverykey", "failed to fetch OTControl object: %@", controlError);
            OctagonSignpostEnd(signpost, OctagonSignpostNameRecoverOctagonUsingData, OctagonSignpostNumber1(OctagonSignpostNameRecoverOctagonUsingData), (int)subTaskSuccess);
            reply(controlError);
            return;
        }
        [control joinWithRecoveryKey:OTCKContainerName contextID:ctx.context recoveryKey:recoveryKey reply:^(NSError *joinError) {
            if(joinError){
                secnotice("clique-recoverykey", "failed to join using recovery key: %@", joinError);
                OctagonSignpostEnd(signpost, OctagonSignpostNameRecoverOctagonUsingData, OctagonSignpostNumber1(OctagonSignpostNameRecoverOctagonUsingData), (int)subTaskSuccess);
                reply(joinError);
                return;
            }
            secnotice("clique-recoverykey", "successfully joined using recovery key");
            subTaskSuccess = true;
            OctagonSignpostEnd(signpost, OctagonSignpostNameRecoverOctagonUsingData, OctagonSignpostNumber1(OctagonSignpostNameRecoverOctagonUsingData), (int)subTaskSuccess);
            reply(nil);
        }];
    }

#else // !OCTAGON
    reply([NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil]);
#endif
}

- (void)performedCDPStateMachineRun:(OTCliqueCDPContextType)type
                            success:(BOOL)success
                              error:(NSError * _Nullable)error
                              reply:(void(^)(NSError* _Nullable error))reply
{
#if OCTAGON
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNamePerformedCDPStateMachineRun);
    NSError* controlError = nil;
    __block bool subTaskSuccess = false;

    OTControl* control = [self makeOTControl:&controlError];
    if(!control) {
        secnotice("clique-cdp-sm", "octagon, failed to fetch OTControl object: %@", controlError);
        OctagonSignpostEnd(signPost, OctagonSignpostNamePerformedCDPStateMachineRun, OctagonSignpostNumber1(OctagonSignpostNamePerformedCDPStateMachineRun), (int)subTaskSuccess);
        reply(controlError);
        return;
    }

    [control postCDPFollowupResult:success type:type error:error containerName:OTCKContainerName contextName:OTDefaultContext reply:^(NSError *postError) {
        if(postError){
            secnotice("clique-cdp-sm", "failed to post %@ result: %@ ", type, postError);
            OctagonSignpostEnd(signPost, OctagonSignpostNamePerformedCDPStateMachineRun, OctagonSignpostNumber1(OctagonSignpostNamePerformedCDPStateMachineRun), (int)subTaskSuccess);
            reply(postError);
            return;
        }
        if (success) {
            secnotice("clique-cdp-sm", "posted success: %@", type);
        } else {
            secnotice("clique-cdp-sm", "posted error: %@:  %@", type, error);
        }
        subTaskSuccess = success ? true : false;
        OctagonSignpostEnd(signPost, OctagonSignpostNamePerformedCDPStateMachineRun, OctagonSignpostNumber1(OctagonSignpostNamePerformedCDPStateMachineRun), (int)subTaskSuccess);
        reply(nil);
    }];
#else // !OCTAGON
    reply([NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil]);
#endif
}

- (BOOL)waitForOctagonUpgrade:(NSError** _Nullable)error
{
#if OCTAGON
    OTControl* control = nil;
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameWaitForOctagonUpgrade);
    __block bool subTaskSuccess = false;

    if (!OctagonIsEnabled()) {
        secnotice("clique-waitforoctagonupgrade", "cannot upgrade, octagon is not enabled");
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:@{NSLocalizedDescriptionKey: @"Octagon is not enabled"}];
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameWaitForOctagonUpgrade, OctagonSignpostNumber1(OctagonSignpostNameWaitForOctagonUpgrade), (int)subTaskSuccess);

        return NO;
    }

    NSError *controlError = nil;
    control = [self makeOTControl:&controlError];
    if (!control) {
        secnotice("clique-waitforoctagonupgrade", "octagon, failed to fetch OTControl object: %@", controlError);
        if (error) {
            *error = controlError;
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameWaitForOctagonUpgrade, OctagonSignpostNumber1(OctagonSignpostNameWaitForOctagonUpgrade), (int)subTaskSuccess);
        return NO;
    }

    __block BOOL ret = NO;
    __block NSError* blockError = nil;

    [control waitForOctagonUpgrade:OTCKContainerName context:OTDefaultContext reply:^(NSError *postError) {
        if(postError){
            secnotice("clique-waitforoctagonupgrade", "error from control: %@", postError);
            blockError = postError;
            ret = NO;
        } else {
            secnotice("clique-waitforoctagonupgrade", "successfully upgraded to octagon");
            ret = YES;
        }
    }];

    if (blockError && error) {
        *error = blockError;
    }
    subTaskSuccess = ret ? true : false;
    OctagonSignpostEnd(signPost, OctagonSignpostNameWaitForOctagonUpgrade, OctagonSignpostNumber1(OctagonSignpostNameWaitForOctagonUpgrade), (int)subTaskSuccess);
    return ret;
#else // !OCTAGON
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

- (void)performedFailureCDPStateMachineRun:(OTCliqueCDPContextType)type
                                     error:(NSError * _Nullable)error
                                     reply:(void(^)(NSError* _Nullable error))reply
{
    [self performedCDPStateMachineRun:type success:NO error:error reply:reply];
}

- (void)performedSuccessfulCDPStateMachineRun:(OTCliqueCDPContextType)type
                                        reply:(void(^)(NSError* _Nullable error))reply
{
    [self performedCDPStateMachineRun:type success:YES error:nil reply:reply];
}

@end

#endif /* OBJC2 */
