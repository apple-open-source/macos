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
#import "keychain/ckks/CKKSExternalTLKClient.h"
#import "utilities/debugging.h"

@interface CKKSControl ()
@property (readwrite,assign) BOOL synchronous;
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

- (void)dealloc {
    [self.connection invalidate];
}

- (id<CKKSControlProtocol>)objectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler
{
    if (self.synchronous) {
        return [self.connection synchronousRemoteObjectProxyWithErrorHandler:failureHandler];
    } else {
        return [self.connection remoteObjectProxyWithErrorHandler:failureHandler];
    }
}

- (void)rpcStatus:(NSString*)viewName reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error)) reply {
    [self rpcStatus:viewName fast:NO waitForNonTransientState:CKKSControlStatusDefaultNonTransientStateTimeout reply:reply];
}

- (void)rpcFastStatus:(NSString*)viewName reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error)) reply {
    [self rpcStatus:viewName fast:YES waitForNonTransientState:CKKSControlStatusDefaultNonTransientStateTimeout reply:reply];
}

- (void)rpcStatus:(NSString* _Nullable)viewName
        fast:(BOOL)fast
        waitForNonTransientState:(dispatch_time_t)nonTransientStateTimeout
        reply:(void(^)(NSArray<NSDictionary*>* _Nullable result, NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);

    }] rpcStatus:viewName fast:fast waitForNonTransientState:nonTransientStateTimeout reply:^(NSArray<NSDictionary*>* result, NSError* error){
        reply(result, error);
    }];
}

- (void)rpcResetLocal:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    secnotice("ckkscontrol", "Requesting a local reset for view %@", viewName);
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResetLocal:viewName reply:^(NSError* error){
        if(error) {
            secnotice("ckkscontrol", "Local reset finished with error: %@", error);
        } else {
            secnotice("ckkscontrol", "Local reset finished successfully");
        }
        reply(error);
    }];
}

- (void)rpcResetCloudKit:(NSString*)viewName reason:(NSString *)reason reply:(void(^)(NSError* error))reply {
    secnotice("ckkscontrol", "Requesting a CloudKit reset for view %@", viewName);
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResetCloudKit:viewName reason:reason reply:^(NSError* error){
        if(error) {
            secnotice("ckkscontrol", "CloudKit reset finished with error: %@", error);
        } else {
            secnotice("ckkscontrol", "CloudKit reset finished successfully");
        }
        reply(error);
    }];
}

- (void)rpcResyncLocal:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply
{
    secnotice("ckkscontrol", "Requesting a local resync for view %@", viewName);
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResyncLocal:viewName reply:^(NSError* error){
        if(error) {
            secnotice("ckkscontrol", "Local resync finished with error: %@", error);
        } else {
            secnotice("ckkscontrol", "Local resync finished successfully");
        }
        reply(error);
    }];
}
- (void)rpcResync:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    secnotice("ckkscontrol", "Requesting a resync for view %@", viewName);
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcResync:viewName reply:^(NSError* error){
        if(error) {
            secnotice("ckkscontrol", "Resync finished with error: %@", error);
        } else {
            secnotice("ckkscontrol", "Resync finished successfully");
        }
        reply(error);
    }];
}

- (void)rpcFetchAndProcessChanges:(NSString*)viewName classA:(bool)classAError onlyIfNoRecentFetch:(bool)onlyIfNoRecentFetch reply:(void(^)(NSError* error))reply
{
    secnotice("ckkscontrol", "Requesting a fetch for view %@", viewName);
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcFetchAndProcessChanges:viewName classA:classAError onlyIfNoRecentFetch:onlyIfNoRecentFetch reply:^(NSError* error){
        if(error) {
            secnotice("ckkscontrol", "Fetch finished with error: %@", error);
        } else {
            secnotice("ckkscontrol", "Fetch finished successfully");
        }
        reply(error);
    }];
}
- (void)rpcFetchAndProcessChanges:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [self rpcFetchAndProcessChanges:viewName classA:false onlyIfNoRecentFetch:false reply:reply];
}
- (void)rpcFetchAndProcessChangesIfNoRecentFetch:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [self rpcFetchAndProcessChanges:viewName classA:false onlyIfNoRecentFetch:true reply:reply];
}
- (void)rpcFetchAndProcessClassAChanges:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    [self rpcFetchAndProcessChanges:viewName classA:true onlyIfNoRecentFetch:false reply:reply];
}

- (void)rpcPushOutgoingChanges:(NSString*)viewName reply:(void(^)(NSError* error))reply {
    secnotice("ckkscontrol", "Requesting a push for view %@", viewName);
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcPushOutgoingChanges:viewName reply:^(NSError* error){
        if(error) {
            secnotice("ckkscontrol", "Push finished with error: %@", error);
        } else {
            secnotice("ckkscontrol", "Push finished successfully");
        }
        reply(error);
    }];
}

- (void)rpcCKMetric:(NSString *)eventName attributes:(NSDictionary *)attributes reply:(void(^)(NSError* error))reply {
    [[self objectProxyWithErrorHandler:^(NSError* error) {
        reply(error);
    }] rpcCKMetric:eventName attributes:attributes reply:^(NSError* error){
        reply(error);
    }];
}

- (void)rpcPerformanceCounters:(void(^)(NSDictionary <NSString *,NSNumber *> *,NSError*))reply {
    [[self objectProxyWithErrorHandler: ^(NSError* error) {
        reply(nil, error);
    }] performanceCounters:^(NSDictionary <NSString *, NSNumber *> *counters){
        reply(counters, nil);
    }];
}

- (void)rpcGetCKDeviceIDWithReply:(void (^)(NSString *))reply {
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(nil);
    }] rpcGetCKDeviceIDWithReply:^(NSString *ckdeviceID) {
        reply(ckdeviceID);
    }];
}

- (void)rpcTLKMissing:(NSString*)viewName reply:(void(^)(bool missing))reply {
    [self rpcFastStatus:viewName reply:^(NSArray<NSDictionary*>* results, NSError* blockError) {
        bool missing = false;

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
    [self rpcFastStatus:viewName reply:^(NSArray<NSDictionary*>* results, NSError* blockError) {
        bool tlkMissing = false;
        bool waitForUnlock = false;
        bool waitForOctagon = false;
        bool noAccount = false;

        CKKSKnownBadState response = CKKSKnownStatePossiblyGood;

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

            if([keystate isEqualToString:@"waitfortlkcreation"] ||
               [keystate isEqualToString:@"waitfortlkupload"] ||
               [keystate isEqualToString:@"waitfortrust"]) {
                waitForOctagon = true;
            }

            if([keystate isEqualToString:@"loggedout"]) {
                noAccount = true;
            }
        }

        response = (noAccount ? CKKSKnownStateNoCloudKitAccount :
                    (tlkMissing ? CKKSKnownStateTLKsMissing :
                     (waitForUnlock ? CKKSKnownStateWaitForUnlock :
                      (waitForOctagon ? CKKSKnownStateWaitForOctagon :
                       CKKSKnownStatePossiblyGood))));

        reply(response);
    }];
}

- (void)proposeTLKForSEView:(NSString*)seViewName
                proposedTLK:(CKKSExternalKey *)proposedTLK
              wrappedOldTLK:(CKKSExternalKey * _Nullable)wrappedOldTLK
                  tlkShares:(NSArray<CKKSExternalTLKShare*>*)shares
                      reply:(void(^)(NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(error);
    }] proposeTLKForSEView:seViewName
               proposedTLK:proposedTLK
             wrappedOldTLK:wrappedOldTLK
                 tlkShares:shares
                     reply:reply];
}

- (void)fetchSEViewKeyHierarchy:(NSString*)seViewName
                          reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                          NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                          NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                          NSError* _Nullable error))reply
{
    [self fetchSEViewKeyHierarchy:seViewName
                       forceFetch:YES
                            reply:reply];
}

- (void)fetchSEViewKeyHierarchy:(NSString*)seViewName
                     forceFetch:(BOOL)forceFetch
                          reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                          NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                          NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                          NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(nil, nil, nil, error);
    }] fetchSEViewKeyHierarchy:seViewName forceFetch:forceFetch reply:reply];
}

- (void)modifyTLKSharesForSEView:(NSString*)seViewName
                          adding:(NSArray<CKKSExternalTLKShare*>*)sharesToAdd
                        deleting:(NSArray<CKKSExternalTLKShare*>*)sharesToDelete
                          reply:(void (^)(NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(error);
    }] modifyTLKSharesForSEView:seViewName adding:sharesToAdd deleting:sharesToDelete reply:reply];
}

- (void)deleteSEView:(NSString*)seViewName
              reply:(void (^)(NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(error);
    }] deleteSEView:seViewName reply:reply];
}

- (void)toggleHavoc:(void (^)(BOOL havoc, NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(NO, error);
    }] toggleHavoc:reply];
}

- (void)pcsMirrorKeysForServices:(NSDictionary<NSNumber*,NSArray<NSData*>*>*)services reply:(void (^)(NSDictionary<NSNumber*,NSArray<NSData*>*>* _Nullable result, NSError* _Nullable error))reply
{
    [[self objectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        reply(nil, error);
    }] pcsMirrorKeysForServices:services reply:reply];
}

+ (CKKSControl*)controlObject:(NSError* __autoreleasing *)error {
    return [CKKSControl CKKSControlObject:NO error:error];
}

+ (CKKSControl*)CKKSControlObject:(BOOL)synchronous error:(NSError* __autoreleasing *)error {

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
    c.synchronous = synchronous;
    return c;
}

@end

#endif // __OBJC2__
