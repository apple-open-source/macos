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
    }] rpcResetCloudKit:viewName reply:^(NSError* error){
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

- (void)rpcGetAnalyticsSysdiagnoseWithReply:(void (^)(NSString* sysdiagnose, NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(nil, error);
    }] rpcGetAnalyticsSysdiagnoseWithReply:^(NSString* sysdiagnose, NSError* error){
        reply(sysdiagnose, error);
    }];
}

- (void)rpcGetAnalyticsJSONWithReply:(void (^)(NSData* json, NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(nil, error);
    }] rpcGetAnalyticsJSONWithReply:^(NSData* json, NSError* error){
        reply(json, error);
    }];
}

- (void)rpcForceUploadAnalyticsWithReply:   (void (^)(BOOL success, NSError* error))reply {
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        reply(false, error);
    }] rpcForceUploadAnalyticsWithReply:^(BOOL success, NSError* error){
        reply(success, error);
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

+ (CKKSControl*)controlObject:(NSError* __autoreleasing *)error {

    CFErrorRef cferror = NULL;
    xpc_endpoint_t endpoint = _SecSecuritydCopyCKKSEndpoint(&cferror);
    if (endpoint == NULL) {
        NSString* errorstr = NULL;

        if(cferror) {
            errorstr = CFBridgingRelease(CFErrorCopyDescription(cferror));
        }

        NSString* errorDescription = [NSString stringWithFormat:@"no CKKSControl endpoint available: %@", errorstr ? errorstr : @"unknown error"];
        if(error) {
            *error = [NSError errorWithDomain:@"securityd" code:-1 userInfo:@{NSLocalizedDescriptionKey: errorDescription}];
        }
        return nil;
    }

    NSXPCListenerEndpoint *listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];
    [listenerEndpoint _setEndpoint:endpoint];
    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];

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
