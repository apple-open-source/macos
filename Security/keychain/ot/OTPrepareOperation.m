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

#if OCTAGON

#import "utilities/debugging.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>

#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTPrepareOperation.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassCAccountSettings.h"

#import <AppleAccount/ACAccount+AppleAccount.h>
#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

@interface OTPrepareOperation ()
@property OTOperationDependencies* deps;
@property NSOperation* finishedOp;
@end

@implementation OTPrepareOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                          deviceInfo:(OTDeviceInformation*)deviceInfo
                      policyOverride:(TPPolicyVersion* _Nullable)policyOverride
                     accountSettings:(OTAccountSettings* _Nullable)accountSettings
                               epoch:(uint64_t)epoch
{
    if((self = [super init])) {
        _deps = dependencies;

        _deviceInfo = deviceInfo;
        _epoch = epoch;

        _intendedState = intendedState;
        _nextState = errorState;

        _policyOverride = policyOverride;
        _accountSettings = accountSettings;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "preparing an identity");
    
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:self.deps.activeAccount.altDSID
                                                                                                  flowID:self.deps.flowID 
                                                                                         deviceSessionID:self.deps.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNamePrepareIdentityInTPH
                                                                                         testsAreEnabled:SecCKKSTestsEnabled()
                                                                                          canSendMetrics:self.deps.permittedToSendMetrics
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSString* altDSID = self.deps.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("authkit", "No configured altDSID: %@", self.deps.activeAccount);
        self.error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoAppleAccount
                                  description:@"No altDSID configured"];
        [self runBeforeGroupFinished:self.finishedOp];
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
        return;
    }

    NSString* bottleSalt = altDSID;
    WEAKIFY(self);

    // But, if this device is SOS-enabled and SOS is present, use the SOS octagon keys (if present)
    NSData* signingKeyPersistRef = nil;
    NSData* encryptionKeyPersistRef = nil;
    if(self.deps.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "Investigating use of Octagon keys from SOS identity");

        NSError* error = nil;
        id<CKKSSelfPeer> sosSelf = [self.deps.sosAdapter currentSOSSelf:&error];

        if(!sosSelf || error) {
            secnotice("octagon-sos", "Failed to get the current SOS self: %@", error);
        } else {
            // Fetch the persistent references for our signing and encryption keys
            OSStatus status = errSecSuccess;
            CFDataRef cfSigningKeyPersistRef = NULL;
            status = SecKeyCopyPersistentRef(sosSelf.signingKey.secKey, &cfSigningKeyPersistRef);
            if(status != errSecSuccess || !cfSigningKeyPersistRef) {
                secnotice("octagon-sos", "Failed to get the persistent ref for our SOS signing key: %d", (int)status);
            } else {
                CFDataRef cfEncryptionKeyPersistRef = NULL;
                status = SecKeyCopyPersistentRef(sosSelf.encryptionKey.secKey, &cfEncryptionKeyPersistRef);
                if(status != errSecSuccess || !cfEncryptionKeyPersistRef) {
                    secnotice("octagon-sos", "Failed to get the persistent ref for our SOS encryption key: %d", (int)status);
                    CFReleaseNull(cfSigningKeyPersistRef);
                    CFReleaseNull(cfEncryptionKeyPersistRef);
                } else {
                    // We only want to use these keys if we successfully have both
                    signingKeyPersistRef = CFBridgingRelease(cfSigningKeyPersistRef);
                    encryptionKeyPersistRef = CFBridgingRelease(cfEncryptionKeyPersistRef);
                }
            }
        }
    }

    __block TPPBSecureElementIdentity* existingSecureElementIdentity = nil;
    
    NSError* persistError = nil;

    BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
        existingSecureElementIdentity = [metadata parsedSecureElementIdentity];
        metadata.attemptedJoin = OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED;

        return metadata;
    } error:&persistError];

    if(!persisted || persistError) {
        secerror("octagon: failed to save 'attempted join' state: %@", persistError);
    }

    secnotice("octagon", "preparing identity with %@", self.accountSettings);

    // Note: we pass syncUserControllableViews as FOLLOWING here, with the intention that
    // it will be set later, when this peer decides who it trusts and accepts their value.

    [self.deps.cuttlefishXPCWrapper prepareWithSpecificUser:self.deps.activeAccount
                                                      epoch:self.epoch
                                                  machineID:self.deviceInfo.machineID
                                                 bottleSalt:bottleSalt
                                                   bottleID:[NSUUID UUID].UUIDString
                                                    modelID:self.deviceInfo.modelID
                                                 deviceName:self.deviceInfo.deviceName
                                               serialNumber:self.deviceInfo.serialNumber
                                                  osVersion:self.deviceInfo.osVersion
                                              policyVersion:self.policyOverride
                                              policySecrets:nil
                                  syncUserControllableViews:TPPBPeerStableInfoUserControllableViewStatus_FOLLOWING
                                      secureElementIdentity:existingSecureElementIdentity
                                                    setting:self.accountSettings
                                signingPrivKeyPersistentRef:signingKeyPersistRef
                                    encPrivKeyPersistentRef:encryptionKeyPersistRef
                                                      reply:^(NSString * _Nullable peerID,
                                                              NSData * _Nullable permanentInfo,
                                                              NSData * _Nullable permanentInfoSig,
                                                              NSData * _Nullable stableInfo,
                                                              NSData * _Nullable stableInfoSig,
                                                              TPSyncingPolicy* _Nullable syncingPolicy,
                                                              NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventPrepareIdentity hardFailure:true result:error];
        if(error) {
            secerror("octagon: Error preparing identity: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishedOp];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
        } else {
            secnotice("octagon", "Prepared: %@ %@ %@", peerID, permanentInfo, permanentInfoSig);
            self.peerID = peerID;
            self.permanentInfo = permanentInfo;
            self.permanentInfoSig = permanentInfoSig;
            self.stableInfo = stableInfo;
            self.stableInfoSig = stableInfoSig;

            NSError* localError = nil;

            secnotice("octagon-ckks", "New syncing policy: %@ views: %@", syncingPolicy, syncingPolicy.viewList);

            BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.peerID = peerID;
                [metadata setTPSyncingPolicy:syncingPolicy];
                return metadata;
            } error:&localError];

            if(!persisted || localError) {
                secnotice("octagon", "Couldn't persist metadata: %@", localError);
                self.error = localError;
                [self runBeforeGroupFinished:self.finishedOp];
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
                return;
            }

            // Let CKKS know of the new policy, so it can spin up
            [self.deps.ckks setCurrentSyncingPolicy:syncingPolicy];

            self.nextState = self.intendedState;
            [self runBeforeGroupFinished:self.finishedOp];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        }
    }];
}

@end

#endif // OCTAGON
