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

#import "keychain/ckks/CKKSControl.h"
#import "keychain/ckks/CKKSControlProtocol.h"
#import "keychain/ckks/CKKSControlServer.h"

#include <security_utilities/debugging.h>

@interface CKKSControl ()
@property xpc_endpoint_t endpoint;
@property NSXPCConnection *connection;
@end

@implementation CKKSControl

- (instancetype)initWithConnection:(NSXPCConnection*)connection {
    if(self = [super init]) {
        _connection = connection;
    }
    return self;
}

- (void)rpcStatus:(NSString*)viewName reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error)) reply {
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);

    }] rpcStatus:viewName reply:^(NSArray<NSDictionary*>* result, NSError* error){
        reply(result, error);
    }];
}

- (void)rpcResetLocal:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResetLocal:viewName reply:^(NSError* error){
        reply(error);
    }];
}

- (void)rpcResetCloudKit:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResetCloudKit:viewName reason:[NSString stringWithFormat:@"%s", getprogname()] reply:^(NSError* error){
        reply(error);
    }];
}

- (void)rpcResetCloudKit:(NSString*)viewName reason:(NSString *)reason reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResetCloudKit:viewName reason:reason reply:^(NSError* error){
        reply(error);
    }];
}


- (void)rpcResync:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResync:viewName reply:^(NSError* error){
        reply(error);
    }];
}
- (void)rpcFetchAndProcessChanges:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcFetchAndProcessChanges:viewName reply:^(NSError* error){
        reply(error);
    }];
}
- (void)rpcFetchAndProcessClassAChanges:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcFetchAndProcessClassAChanges:viewName reply:^(NSError* error){
        reply(error);
    }];
}
- (void)rpcPushOutgoingChanges:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcPushOutgoingChanges:viewName reply:^(NSError* error){
        reply(error);
    }];
}

- (void)rpcPerformanceCounters:(void(^)(NSDictionary <NSString *,NSNumber *> *,NSError*))reply {
    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);
    }] performanceCounters:^(NSDictionary <NSString *, NSNumber *> *counters){
        reply(counters, nil);
    }];
}

- (void)rpcGetCKDeviceIDWithReply:(void (^)(NSString *))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(nil);
    }] rpcGetCKDeviceIDWithReply:^(NSString *ckdeviceID) {
        reply(ckdeviceID);
    }];
}

- (void)rpcTLKMissing:(NSString*)viewName reply:(void(^)(bool missing))reply {
    [self rpcStatus:viewName reply:^(NSArray<NSDictionary*>* results, NSError* blockError) {
        bool missing = false;

        // Until PCS fixes [<rdar://problem/35103941> PCS: Remove PCS's use of CKKSControlProtocol], we can't add things to the protocol
        // Use this hack
        for(NSDictionary* result in results) {
            NSString* name = result[@"view"];
            NSString* keystate = result[@"keystate"];

            if([name isEqualToString:@"global"]) {
                // this is global status; no view implicated
                continue;
            }

            if ([keystate isEqualToString:@"waitfortlk"] || [keystate isEqualToString:@"error"]) {
                missing = true;
            }
        }

        reply(missing);
    }];
}

- (void)rpcKnownBadState:(NSString* _Nullable)viewName reply:(void (^)(CKKSKnownBadState))reply {
    [self rpcStatus:viewName reply:^(NSArray<NSDictionary*>* results, NSError* blockError) {
        bool tlkMissing = false;
        bool waitForUnlock = false;

        CKKSKnownBadState response = CKKSKnownStatePossiblyGood;

        // We can now change this hack, but this change needs to be addition-only: <rdar://problem/36356681> CKKS: remove "global" hack from rpcStatus
        // Use this hack
        for(NSDictionary* result in results) {
            NSString* name = result[@"view"];
            NSString* keystate = result[@"keystate"];

            if([name isEqualToString:@"global"]) {
                // this is global status; no view implicated
                continue;
            }

            if ([keystate isEqualToString:@"waitfortlk"] || [keystate isEqualToString:@"error"]) {
                tlkMissing = true;
            }
            if ([keystate isEqualToString:@"waitforunlock"]) {
                waitForUnlock = true;
            }
        }

        response = (tlkMissing ? CKKSKnownStateTLKsMissing :
                   (waitForUnlock ? CKKSKnownStateWaitForUnlock :
                    CKKSKnownStatePossiblyGood));

        reply(response);
    }];
}

+ (CKKSControl*)controlObject:(NSError* __autoreleasing *)error {

    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:@(kSecuritydCKKSServiceName) options:0];

    if (connection == nil) {
        if(error) {
            *error =  [NSError errorWithDomain:@"securityd" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Couldn't create connection (no reason given)"}];
        }
        return nil;
    }

    NSXPCInterface *interface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    connection.remoteObjectInterface = interface;
    [connection resume];

    CKKSControl* c = [[CKKSControl alloc] initWithConnection:connection];
    return c;
}

@end

#endif // __OBJC2__
