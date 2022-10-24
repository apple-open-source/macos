/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/NSXPCConnection_Private.h>
#import <xpc/xpc.h>

#import <AppleFeatures/AppleFeatures.h>

#import <Security/SecItemPriv.h>
#import <Security/SecXPCHelper.h>

#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/proto/generated_source/OTCurrentSecureElementIdentities.h"
#import "OctagonTrust/OctagonTrust.h"

#include <security_utilities/debugging.h>

#if OCTAGON
#import <SecurityFoundation/SFKey.h>
#endif

@implementation OTControlArguments
- (instancetype)init
{
    return [self initWithContainerName:OTCKContainerName
                             contextID:OTDefaultContext
                               altDSID:nil];
}

- (instancetype)initWithConfiguration:(OTConfigurationContext*)configuration
{
    return [self initWithContainerName:configuration.containerName ?: OTCKContainerName
                             contextID:configuration.context ?: OTDefaultContext
                               altDSID:configuration.altDSID];
}

- (instancetype)initWithAltDSID:(NSString* _Nullable)altDSID
{
    return [self initWithContainerName:OTCKContainerName
                             contextID:OTDefaultContext
                               altDSID:altDSID];
}

- (instancetype)initWithContainerName:(NSString* _Nullable)containerName
                            contextID:(NSString*)contextID
                              altDSID:(NSString* _Nullable)altDSID
{
    if((self = [super init])) {
        _containerName = containerName ?: OTCKContainerName;
        _contextID = contextID;
        _altDSID = altDSID;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<OTControlArguments: container:%@, context:%@, altDSID:%@>",
            self.containerName,
            self.contextID,
            self.altDSID];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    [coder encodeObject:self.contextID forKey:@"contextID"];
    [coder encodeObject:self.containerName forKey:@"containerName"];
    [coder encodeObject:self.altDSID forKey:@"altDSID"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder *)coder {
    if((self = [super init])) {
        _contextID = [coder decodeObjectOfClass:[NSString class] forKey:@"contextID"];
        _containerName = [coder decodeObjectOfClass:[NSString class] forKey:@"containerName"];
        _altDSID = [coder decodeObjectOfClass:[NSString class] forKey:@"altDSID"];
    }
    return self;
}

- (OTConfigurationContext*)makeConfigurationContext
{
    OTConfigurationContext* context = [[OTConfigurationContext alloc] init];
    context.containerName = self.containerName;
    context.context = self.contextID;
    context.altDSID = self.altDSID;
    return context;
}
@end


@interface OTControl ()
@property NSXPCConnection *connection;
@property bool sync;
@end

@implementation OTControl

- (instancetype)initWithConnection:(NSXPCConnection*)connection sync:(bool)sync {
    if(self = [super init]) {
        _connection = connection;
        _sync = sync;
    }
    return self;
}

- (void)dealloc {
    [self.connection invalidate];
}

- (NSXPCConnection<OTControlProtocol>*)getConnection:(void (^)(NSError *error))handler
{
    if(self.sync) {
        return [self.connection synchronousRemoteObjectProxyWithErrorHandler: handler];
    } else {
        return [self.connection remoteObjectProxyWithErrorHandler: handler];
    }
}

- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID
          reply:(void (^)(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error))reply
{
    reply(nil,
          nil,
          [NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

-(void)reset:(void (^)(BOOL result, NSError* _Nullable error))reply
{
    reply(NO,
          [NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)signingKey:(void (^)(NSData* result, NSError* _Nullable error))reply
{
    [self octagonSigningPublicKey:reply];
}

- (void)octagonSigningPublicKey:(nonnull void (^)(NSData * _Nullable, NSError * _Nullable))reply {
    reply(nil,
          [NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)encryptionKey:(void (^)(NSData* result, NSError* _Nullable error))reply
{
    [self octagonEncryptionPublicKey:reply];
}

- (void)octagonEncryptionPublicKey:(nonnull void (^)(NSData * _Nullable, NSError * _Nullable))reply
{
    reply(nil,
          [NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)listOfRecords:(void (^)(NSArray* list, NSError* _Nullable error))reply
{
    [self listOfEligibleBottledPeerRecords:reply];
}

- (void)listOfEligibleBottledPeerRecords:(nonnull void (^)(NSArray * _Nullable, NSError * _Nullable))reply
{
    reply(@[],
          [NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)appleAccountSignedIn:(OTControlArguments*)arguments reply:(void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] appleAccountSignedIn:arguments reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)appleAccountSignedOut:(OTControlArguments*)arguments reply:(void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] appleAccountSignedOut:arguments reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)notifyIDMSTrustLevelChangeForAltDSID:(OTControlArguments*)arguments reply:(void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] notifyIDMSTrustLevelChangeForAltDSID:arguments reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)rpcEpochWithArguments:(OTControlArguments*)arguments
                    configuration:(OTJoiningConfiguration*)config
                            reply:(void (^)(uint64_t epoch,
                                            NSError * _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(0, error);
    }] rpcEpochWithArguments:arguments configuration:config reply:^(uint64_t epoch, NSError * _Nullable error) {
        reply(epoch, error);
    }];
#else
    reply(0, NULL);
#endif
}

- (void)rpcPrepareIdentityAsApplicantWithArguments:(OTControlArguments*)arguments
                                     configuration:(OTJoiningConfiguration*)config
                                             reply:(void (^)(NSString * _Nullable peerID,
                                                             NSData * _Nullable permanentInfo,
                                                             NSData * _Nullable permanentInfoSig,
                                                             NSData * _Nullable stableInfo,
                                                             NSData * _Nullable stableInfoSig,
                                                             NSError * _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(nil, nil, nil, nil, nil, error);
    }] rpcPrepareIdentityAsApplicantWithArguments:arguments configuration:config reply:^(NSString* pID, NSData* pI, NSData* piSig, NSData* si, NSData* siSig, NSError* e) {
        reply(pID, pI, piSig, si, siSig, e);
    }];
#else
    reply(NULL, NULL, NULL, NULL, NULL, NULL);
#endif
}

- (void)rpcVoucherWithArguments:(OTControlArguments*)arguments
                  configuration:(OTJoiningConfiguration*)config
                         peerID:(NSString*)peerID
                  permanentInfo:(NSData *)permanentInfo
               permanentInfoSig:(NSData *)permanentInfoSig
                     stableInfo:(NSData *)stableInfo
                  stableInfoSig:(NSData *)stableInfoSig
                          reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(nil, nil, error);
    }] rpcVoucherWithArguments:arguments configuration:config peerID:peerID permanentInfo:permanentInfo permanentInfoSig:permanentInfoSig stableInfo:stableInfo stableInfoSig:stableInfoSig reply:^(NSData* voucher, NSData* voucherSig, NSError * _Nullable error) {
        reply(voucher, voucherSig, error);
    }];
#else
    reply(NULL, NULL, NULL);
#endif
}

- (void)rpcJoinWithArguments:(OTControlArguments*)arguments
               configuration:(OTJoiningConfiguration*)config
                   vouchData:(NSData*)vouchData
                    vouchSig:(NSData*)vouchSig
                       reply:(void (^)(NSError * _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] rpcJoinWithArguments:arguments configuration:config vouchData:vouchData vouchSig:vouchSig reply:^(NSError* e) {
        reply(e);
    }];
#else
    reply(NULL);
#endif
}

- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply
{
    reply(nil,
          nil,
          nil,
          [NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable))reply
{
    secnotice("octagon", "launchBottledPeer");
    reply([NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable))reply
{
    reply([NSError errorWithDomain:NSOSStatusErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)status:(NSString* _Nullable)container
       context:(NSString*)context
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    OTControlArguments *oca = [[OTControlArguments alloc] initWithContainerName:container contextID:context altDSID:nil];
    [self status:oca reply:reply];
}

- (void)status:(OTControlArguments*)arguments
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] status:arguments reply:reply];
}

- (void)fetchEgoPeerID:(OTControlArguments*)arguments
                 reply:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] fetchEgoPeerID:arguments reply:reply];
}

- (void)fetchCliqueStatus:(OTControlArguments*)arguments
            configuration:(OTOperationConfiguration*)configuration
                    reply:(void (^)(CliqueStatus cliqueStatus, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(CliqueStatusError, error);
    }] fetchCliqueStatus:arguments configuration:configuration reply:reply];
}

- (void)fetchTrustStatus:(OTControlArguments*)arguments
           configuration:(OTOperationConfiguration *)configuration
                   reply:(void (^)(CliqueStatus status, NSString* peerID, NSNumber * _Nullable numberOfOctagonPeers, BOOL isExcluded, NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(CliqueStatusError, nil, nil, false, error);
    }] fetchTrustStatus:arguments configuration:configuration reply:reply];
}

- (void)startOctagonStateMachine:(OTControlArguments*)arguments
                           reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] startOctagonStateMachine:arguments reply:reply];
}


- (void)resetAndEstablish:(OTControlArguments*)arguments
              resetReason:(CuttlefishResetReason)resetReason
                    reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] resetAndEstablish:arguments resetReason:resetReason reply:reply];
}

- (void)establish:(OTControlArguments*)arguments
            reply:(void (^)(NSError * _Nullable))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] establish:arguments reply:reply];
}

- (void)leaveClique:(OTControlArguments*)arguments
              reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] leaveClique:arguments reply:reply];
}

- (void)removeFriendsInClique:(OTControlArguments*)arguments
                      peerIDs:(NSArray<NSString*>*)peerIDs
                        reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] removeFriendsInClique:arguments peerIDs:peerIDs reply:reply];
}

- (void)peerDeviceNamesByPeerID:(OTControlArguments*)arguments
                          reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] peerDeviceNamesByPeerID:arguments reply:reply];
}

- (void)fetchAllViableBottles:(OTControlArguments*)arguments
                        reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*> * _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, nil, error);
    }] fetchAllViableBottles:arguments reply:reply];
}

- (void)restoreFromBottle:(OTControlArguments*)arguments
                  entropy:(NSData *)entropy
                 bottleID:(NSString *)bottleID
                    reply:(void (^)(NSError * _Nullable))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] restoreFromBottle:arguments entropy:entropy bottleID:bottleID reply:reply];
}

- (void)fetchEscrowContents:(OTControlArguments*)arguments
                      reply:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, nil, nil, error);
    }] fetchEscrowContents:arguments reply:reply];
}

- (void)createRecoveryKey:(OTControlArguments*)arguments
              recoveryKey:(NSString *)recoveryKey
                    reply:(void (^)( NSError * error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] createRecoveryKey:arguments recoveryKey:recoveryKey reply:reply];
}

- (void) joinWithRecoveryKey:(OTControlArguments*)arguments
                 recoveryKey:(NSString*)recoveryKey
                       reply:(void (^)(NSError * _Nullable))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] joinWithRecoveryKey:arguments recoveryKey:recoveryKey reply:reply];
}

- (void) createCustodianRecoveryKey:(OTControlArguments*)arguments
                               uuid:(NSUUID *_Nullable)uuid
                              reply:(void (^)(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, error);
    }] createCustodianRecoveryKey:arguments uuid:uuid reply:reply];
}

- (void) joinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                 custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                reply:(void(^)(NSError* _Nullable error)) reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] joinWithCustodianRecoveryKey:arguments custodianRecoveryKey:crk reply:reply];
}

- (void) preflightJoinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                          custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                         reply:(void(^)(NSError* _Nullable error)) reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] preflightJoinWithCustodianRecoveryKey:arguments custodianRecoveryKey:crk reply:reply];
}

- (void) removeCustodianRecoveryKey:(OTControlArguments*)arguments
                               uuid:(NSUUID *)uuid
                              reply:(void (^)(NSError *_Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] removeCustodianRecoveryKey:arguments uuid:uuid reply:reply];
}

- (void) createInheritanceKey:(OTControlArguments*)arguments
                         uuid:(NSUUID *_Nullable)uuid
                        reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, error);
    }] createInheritanceKey:arguments uuid:uuid reply:reply];
}

- (void) generateInheritanceKey:(OTControlArguments*)arguments
                           uuid:(NSUUID *_Nullable)uuid
                          reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, error);
    }] generateInheritanceKey:arguments uuid:uuid reply:reply];
}

- (void) storeInheritanceKey:(OTControlArguments*)arguments
                          ik:(OTInheritanceKey *)ik
                       reply:(void (^)(NSError *_Nullable error)) reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] storeInheritanceKey:arguments ik:ik reply:reply];
}

- (void) joinWithInheritanceKey:(OTControlArguments*)arguments
                 inheritanceKey:(OTInheritanceKey *)ik
                          reply:(void(^)(NSError* _Nullable error)) reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] joinWithInheritanceKey:arguments inheritanceKey:ik reply:reply];
}

- (void) preflightJoinWithInheritanceKey:(OTControlArguments*)arguments
                          inheritanceKey:(OTInheritanceKey *)ik
                                   reply:(void(^)(NSError* _Nullable error)) reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] preflightJoinWithInheritanceKey:arguments inheritanceKey:ik reply:reply];
}

- (void) removeInheritanceKey:(OTControlArguments*)arguments
                         uuid:(NSUUID *)uuid
                        reply:(void (^)(NSError *_Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] removeInheritanceKey:arguments uuid:uuid reply:reply];
}

- (void)healthCheck:(OTControlArguments*)arguments
skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
              reply:(void (^)(NSError *_Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] healthCheck:arguments skipRateLimitingCheck:skipRateLimitingCheck reply:reply];
}

- (void)waitForOctagonUpgrade:(OTControlArguments*)arguments
                        reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] waitForOctagonUpgrade:arguments reply:reply];
}

- (void)postCDPFollowupResult:(OTControlArguments*)arguments
                      success:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                        reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] postCDPFollowupResult:arguments success:success type:type error:[SecXPCHelper cleanseErrorForXPC:error] reply:reply];
}

- (void)tapToRadar:(NSString *)action
       description:(NSString *)description
             radar:(NSString *)radar
             reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] tapToRadar:action description:description radar:radar reply:reply];
}

- (void)refetchCKKSPolicy:(OTControlArguments*)arguments
                    reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] refetchCKKSPolicy:arguments reply:reply];
}

- (void)setCDPEnabled:(OTControlArguments*)arguments
                reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] setCDPEnabled:arguments reply:reply];
}

- (void)getCDPStatus:(OTControlArguments*)arguments
               reply:(void (^)(OTCDPStatus status, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(OTCDPStatusUnknown, connectionError);
    }] getCDPStatus:arguments reply:reply];
}

- (void)fetchEscrowRecords:(OTControlArguments*)arguments
                forceFetch:(BOOL)forceFetch
                     reply:(void (^)(NSArray<NSData*>* _Nullable records,
                                     NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(nil, connectionError);
    }] fetchEscrowRecords:arguments forceFetch:forceFetch reply:reply];
}

- (void)setUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                   enabled:(BOOL)enabled
                                     reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(NO, connectionError);
    }] setUserControllableViewsSyncStatus:arguments enabled:enabled reply:reply];

}

- (void)fetchUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                       reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(NO, connectionError);
    }] fetchUserControllableViewsSyncStatus:arguments reply:reply];
}

- (void)invalidateEscrowCache:(OTControlArguments*)arguments
                        reply:(nonnull void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] invalidateEscrowCache:arguments reply:reply];
}

- (void)resetAccountCDPContents:(OTControlArguments*)arguments
                          reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] resetAccountCDPContents:arguments reply:reply];
}

- (void)setLocalSecureElementIdentity:(OTControlArguments*)arguments
                secureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                reply:(void (^)(NSError* _Nullable))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(connectionError);
    }] setLocalSecureElementIdentity:arguments secureElementIdentity:secureElementIdentity reply:reply];

}

- (void)removeLocalSecureElementIdentityPeerID:(OTControlArguments*)arguments
                   secureElementIdentityPeerID:(NSData*)sePeerID
                                         reply:(void (^)(NSError* _Nullable))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(connectionError);
    }] removeLocalSecureElementIdentityPeerID:arguments secureElementIdentityPeerID:sePeerID reply:reply];
}

- (void)fetchTrustedSecureElementIdentities:(OTControlArguments*)arguments
                                      reply:(void (^)(OTCurrentSecureElementIdentities* currentSet,
                                                      NSError* replyError))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(nil, connectionError);
    }] fetchTrustedSecureElementIdentities:arguments reply:reply];
}


- (void)waitForPriorityViewKeychainDataRecovery:(OTControlArguments*)arguments
                                          reply:(void (^)(NSError* _Nullable replyError))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(connectionError);
    }] waitForPriorityViewKeychainDataRecovery:arguments reply:reply];
}

- (void)tlkRecoverabilityForEscrowRecordData:(OTControlArguments*)arguments
                                  recordData:(NSData*)recordData
                                       reply:(void (^)(NSArray<NSString*>* _Nullable views, NSError* _Nullable error))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(nil, connectionError);
    }] tlkRecoverabilityForEscrowRecordData:arguments recordData:recordData reply:reply];
}

- (void)deliverAKDeviceListDelta:(NSDictionary*)notificationDictionary
                           reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(connectionError);
    }] deliverAKDeviceListDelta:notificationDictionary reply:reply];
}

- (void)setMachineIDOverride:(OTControlArguments*)arguments
                   machineID:(NSString*)machineID
                       reply:(void (^)(NSError* _Nullable replyError))reply
{
    [[self getConnection:^(NSError *connectionError) {
        reply(connectionError);
    }] setMachineIDOverride:arguments machineID:machineID reply:reply];
}

+ (OTControl*)controlObject:(NSError* __autoreleasing *)error {
    return [OTControl controlObject:false error:error];
}

+ (OTControl*)controlObject:(bool)sync error:(NSError**)error
{
    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydOctagonServiceName) options:0];
    
    if (connection == nil) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"Couldn't create connection (no reason given)"}];
        }
        return nil;
    }
    
    NSXPCInterface *interface = OTSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(OTControlProtocol)]);
    connection.remoteObjectInterface = interface;
    [connection resume];
    
    OTControl* c = [[OTControl alloc] initWithConnection:connection sync:sync];
    return c;
}

@end

#endif // __OBJC2__
