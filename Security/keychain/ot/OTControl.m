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

#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OctagonControlServer.h"

#include <security_utilities/debugging.h>

#if OCTAGON
#import <SecurityFoundation/SFKey.h>
#endif

@interface OTControl ()
@property NSXPCConnection *connection;
@end

@implementation OTControl

- (instancetype)initWithConnection:(NSXPCConnection*)connection {
    if(self = [super init]) {
        _connection = connection;
    }
    return self;
}

- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID
         reply:(void (^)(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, nil, error);
    }] restore:contextID dsid:dsid secret:secret escrowRecordID:escrowRecordID reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError *error) {
        reply(signingKeyData, encryptionKeyData, error);
    }];

}

-(void)reset:(void (^)(BOOL result, NSError* _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(NO, error);
    }] reset:^(BOOL result, NSError * _Nullable error) {
        reply(result, error);
    }];
}

- (void)signingKey:(void (^)(NSData* result, NSError* _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);
    }] octagonSigningPublicKey:^(NSData *signingKey, NSError * _Nullable error) {
        reply(signingKey, error);
    }];

}

- (void)encryptionKey:(void (^)(NSData* result, NSError* _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);
    }] octagonEncryptionPublicKey:^(NSData *encryptionKey, NSError * _Nullable error) {
        reply(encryptionKey, error);
    }];

}

- (void)listOfRecords:(void (^)(NSArray* list, NSError* _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);
    }] listOfEligibleBottledPeerRecords:^(NSArray *list, NSError * _Nullable error) {
        reply(list, error);
    }];
    
}

- (void)signIn:(NSString*)dsid reply:(void (^)(BOOL result, NSError * _Nullable error))reply{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(NO, error);
    }] signIn:dsid reply:^(BOOL result, NSError * _Nullable error) {
        reply(result, error);
    }];
}

- (void)signOut:(void (^)(BOOL result, NSError * _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(NO, error);
    }] signOut:^(BOOL result, NSError * _Nullable error) {
        reply(result, error);
    }];
    
}


- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
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
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(error);
    }] launchBottledPeer:contextID bottleID:bottleID reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable))reply
{
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(error);
    }] scrubBottledPeer:contextID bottleID:bottleID reply:reply];
}

+ (OTControl*)controlObject:(NSError* __autoreleasing *)error {

    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydOctagonServiceName) options:0];
    
    if (connection == nil) {
        if(error) {
            *error =  [NSError errorWithDomain:@"securityd" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Couldn't create connection (no reason given)"}];
        }
        return nil;
    }
    
    NSXPCInterface *interface = OTSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(OTControlProtocol)]);
    connection.remoteObjectInterface = interface;
    [connection resume];
    
    OTControl* c = [[OTControl alloc] initWithConnection:connection];
    return c;
}

@end

#endif // __OBJC2__
