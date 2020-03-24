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

#import <Security/SecItemPriv.h>
#import <Security/SecXPCHelper.h>

#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OctagonControlServer.h"

#include <security_utilities/debugging.h>

#if OCTAGON
#import <SecurityFoundation/SFKey.h>
#endif

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
    [[self getConnection: ^(NSError* error) {
        reply(nil, nil, error);
    }] restore:contextID dsid:dsid secret:secret escrowRecordID:escrowRecordID reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError *error) {
        reply(signingKeyData, encryptionKeyData, error);
    }];
}

-(void)reset:(void (^)(BOOL result, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(NO, error);
    }] reset:^(BOOL result, NSError * _Nullable error) {
        reply(result, error);
    }];
}

- (void)signingKey:(void (^)(NSData* result, NSError* _Nullable error))reply
{
    [self octagonSigningPublicKey:reply];
}

- (void)octagonSigningPublicKey:(nonnull void (^)(NSData * _Nullable, NSError * _Nullable))reply {
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] octagonSigningPublicKey:^(NSData *signingKey, NSError * _Nullable error) {
        reply(signingKey, error);
    }];

}

- (void)encryptionKey:(void (^)(NSData* result, NSError* _Nullable error))reply
{
    [self octagonEncryptionPublicKey:reply];
}

- (void)octagonEncryptionPublicKey:(nonnull void (^)(NSData * _Nullable, NSError * _Nullable))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] octagonEncryptionPublicKey:^(NSData *encryptionKey, NSError * _Nullable error) {
        reply(encryptionKey, error);
    }];
}

- (void)listOfRecords:(void (^)(NSArray* list, NSError* _Nullable error))reply
{
    [self listOfEligibleBottledPeerRecords:reply];
}

- (void)listOfEligibleBottledPeerRecords:(nonnull void (^)(NSArray * _Nullable, NSError * _Nullable))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] listOfEligibleBottledPeerRecords:^(NSArray *list, NSError * _Nullable error) {
        reply(list, error);
    }];
    
}

- (void)signIn:(NSString*)altDSID container:(NSString* _Nullable)container context:(NSString*)contextID reply:(void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] signIn:altDSID container:container context:contextID reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)signOut:(NSString* _Nullable)container context:(NSString*)contextID reply:(void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] signOut:container context:contextID reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)notifyIDMSTrustLevelChangeForContainer:(NSString* _Nullable)container context:(NSString*)contextID reply:(void (^)(NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] notifyIDMSTrustLevelChangeForContainer:container context:contextID reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)handleIdentityChangeForSigningKey:(SFECKeyPair* _Nonnull)peerSigningKey
                         ForEncryptionKey:(SFECKeyPair* _Nonnull)encryptionKey
                                ForPeerID:(NSString*)peerID
                                    reply:(void (^)(BOOL result,
                                                    NSError* _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(NO, error);
    }] handleIdentityChangeForSigningKey:peerSigningKey ForEncryptionKey:encryptionKey ForPeerID:peerID reply:^(BOOL result, NSError* _Nullable error) {
        reply(result, error);
     }];
#else
    reply(NO, NULL);
#endif
}

- (void)rpcEpochWithConfiguration:(OTJoiningConfiguration*)config
                            reply:(void (^)(uint64_t epoch,
                                            NSError * _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(0, error);
    }] rpcEpochWithConfiguration:config reply:^(uint64_t epoch,
                                                NSError * _Nullable error) {
        reply(epoch, error);
    }];
#else
    reply(0, NULL);
#endif
}

- (void)rpcPrepareIdentityAsApplicantWithConfiguration:(OTJoiningConfiguration*)config
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
    }] rpcPrepareIdentityAsApplicantWithConfiguration:config reply:^(NSString* pID, NSData* pI, NSData* piSig, NSData* si, NSData* siSig, NSError* e) {
        reply(pID, pI, piSig, si, siSig, e);
    }];
#else
    reply(NULL, NULL, NULL, NULL, NULL, NULL);
#endif
}

- (void)rpcVoucherWithConfiguration:(OTJoiningConfiguration*)config
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
    }] rpcVoucherWithConfiguration:config peerID:peerID permanentInfo:permanentInfo permanentInfoSig:permanentInfoSig stableInfo:stableInfo stableInfoSig:stableInfoSig reply:^(NSData* voucher, NSData* voucherSig, NSError * _Nullable error) {
        reply(voucher, voucherSig, error);
    }];
#else
    reply(NULL, NULL, NULL);
#endif
}

- (void)rpcJoinWithConfiguration:(OTJoiningConfiguration*)config
                       vouchData:(NSData*)vouchData
                        vouchSig:(NSData*)vouchSig
                           reply:(void (^)(NSError * _Nullable error))reply
{
#if OCTAGON
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] rpcJoinWithConfiguration:config vouchData:vouchData vouchSig:vouchSig reply:^(NSError* e) {
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
    [[self getConnection: ^(NSError* error) {
        reply(nil, nil, nil, error);
    }] preflightBottledPeer:contextID dsid:dsid reply:^(NSData* _Nullable entropy,
                                                        NSString* _Nullable bottleID,
                                                        NSData* _Nullable signingPublicKey,
                                                        NSError* _Nullable error) {
        reply(entropy, bottleID, signingPublicKey, error);
    }];
}

- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] launchBottledPeer:contextID bottleID:bottleID reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] scrubBottledPeer:contextID bottleID:bottleID reply:reply];
}

- (void)status:(NSString* _Nullable)container
       context:(NSString*)context
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] status:container context:context reply:reply];
}

- (void)fetchEgoPeerID:(NSString* _Nullable)container
               context:(NSString*)context
                 reply:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] fetchEgoPeerID:container context:context reply:reply];
}

- (void)fetchCliqueStatus:(NSString* _Nullable)container
                  context:(NSString*)context
            configuration:(OTOperationConfiguration*)configuration
                    reply:(void (^)(CliqueStatus cliqueStatus, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(CliqueStatusError, error);
    }] fetchCliqueStatus:container context:context configuration:configuration reply:reply];
}

- (void)fetchTrustStatus:(NSString* _Nullable)container
                 context:(NSString*)context
           configuration:(OTOperationConfiguration *)configuration
                   reply:(void (^)(CliqueStatus status, NSString* peerID, NSNumber * _Nullable numberOfOctagonPeers, BOOL isExcluded, NSError * _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(CliqueStatusError, false, NULL, false, error);
    }] fetchTrustStatus:container context:context configuration:configuration reply:reply];
}

- (void)startOctagonStateMachine:(NSString* _Nullable)container
                         context:(NSString*)context
                           reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] startOctagonStateMachine:container context:context reply:reply];
}

- (void)resetAndEstablish:(NSString* _Nullable)container
                  context:(NSString*)context
                  altDSID:(NSString*)altDSID
              resetReason:(CuttlefishResetReason)resetReason
                    reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] resetAndEstablish:container context:context altDSID:altDSID resetReason:resetReason reply:reply];
}

- (void)establish:(NSString* _Nullable)container
          context:(NSString*)context
          altDSID:(NSString*)altDSID
            reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] establish:container context:context altDSID:altDSID reply:reply];
}

- (void)leaveClique:(NSString* _Nullable)container
            context:(NSString*)context
              reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] leaveClique:container context:context reply:reply];
}

- (void)removeFriendsInClique:(NSString* _Nullable)container
                      context:(NSString*)context
                      peerIDs:(NSArray<NSString*>*)peerIDs
                        reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] removeFriendsInClique:container context:context peerIDs:peerIDs reply:reply];
}

- (void)peerDeviceNamesByPeerID:(NSString* _Nullable)container
                        context:(NSString*)context
                          reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(nil, error);
    }] peerDeviceNamesByPeerID:container context:context reply:reply];
}

- (void)fetchAllViableBottles:(NSString* _Nullable)container
                      context:(NSString*)context
                        reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*> * _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, nil, error);
    }] fetchAllViableBottles:container context:context reply:reply];
}

-(void)restore:(NSString* _Nullable)containerName
     contextID:(NSString *)contextID
    bottleSalt:(NSString *)bottleSalt
       entropy:(NSData *)entropy
bottleID:(NSString *)bottleID
         reply:(void (^)(NSError * _Nullable))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] restore:containerName contextID:contextID bottleSalt:bottleSalt entropy:entropy bottleID:bottleID reply:reply];
}

- (void)fetchEscrowContents:(NSString* _Nullable)containerName
                  contextID:(NSString *)contextID
                      reply:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(nil, nil, nil, error);
    }] fetchEscrowContents:containerName contextID:contextID reply:reply];
}

- (void) createRecoveryKey:(NSString* _Nullable)containerName
                 contextID:(NSString *)contextID
               recoveryKey:(NSString *)recoveryKey
                     reply:(void (^)( NSError * error))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] createRecoveryKey:containerName contextID:contextID recoveryKey:recoveryKey reply:reply];
}

- (void) joinWithRecoveryKey:(NSString* _Nullable)containerName
                   contextID:(NSString *)contextID
                 recoveryKey:(NSString*)recoveryKey
                       reply:(void (^)(NSError * _Nullable))reply
{
    [[self getConnection:^(NSError *error) {
        reply(error);
    }] joinWithRecoveryKey:containerName contextID:contextID recoveryKey:recoveryKey reply:reply];
}

- (void)healthCheck:(NSString *)container
            context:(NSString *)context
skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
              reply:(void (^)(NSError *_Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] healthCheck:container context:context skipRateLimitingCheck:skipRateLimitingCheck reply:reply];
}

- (void)attemptSosUpgrade:(NSString* _Nullable)container
                  context:(NSString*)context
                    reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] attemptSosUpgrade:container context:context reply:reply];
}

- (void)waitForOctagonUpgrade:(NSString* _Nullable)container
                      context:(NSString*)context
                        reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] waitForOctagonUpgrade:container context:context reply:reply];
}

- (void)postCDPFollowupResult:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                containerName:(NSString* _Nullable)containerName
                  contextName:(NSString *)contextName
                        reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] postCDPFollowupResult:success type:type error:[SecXPCHelper cleanseErrorForXPC:error] containerName:containerName contextName:contextName reply:reply];
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

- (void)refetchCKKSPolicy:(NSString* _Nullable)container
                contextID:(NSString*)contextID
                    reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* error) {
        reply(error);
    }] refetchCKKSPolicy:container contextID:contextID reply:reply];
}

- (void)setCDPEnabled:(NSString* _Nullable)containerName
            contextID:(NSString*)contextID
                reply:(void (^)(NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(connectionError);
    }] setCDPEnabled:containerName contextID:contextID reply:reply];
}

- (void)getCDPStatus:(NSString* _Nullable)containerName
           contextID:(NSString*)contextID
               reply:(void (^)(OTCDPStatus status, NSError* _Nullable error))reply
{
    [[self getConnection: ^(NSError* connectionError) {
        reply(OTCDPStatusUnknown, connectionError);
    }] getCDPStatus:containerName contextID:contextID reply:reply];
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
