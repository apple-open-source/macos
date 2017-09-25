/*
 * Copyright (c) 2017 Apple Inc.  All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <objc/runtime.h>
#import <utilities/debugging.h>

#include <ipc/securityd_client.h>

@implementation SecuritydXPCClient
@synthesize connection = _connection;

- (instancetype) initWithEndpoint:(xpc_endpoint_t)endpoint
{
    if ((self = [super init])) {
        NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SecuritydXPCProtocol)];
        NSXPCListenerEndpoint *listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];

        [listenerEndpoint _setEndpoint:endpoint];

        self.connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];
        if (self.connection == NULL) {
            return NULL;
        }

        self.connection.remoteObjectInterface = interface;
        [SecuritydXPCClient configureSecuritydXPCProtocol: self.connection.remoteObjectInterface];
    }

    return self;
}

+(void)configureSecuritydXPCProtocol: (NSXPCInterface*) interface {
    NSXPCInterface *rpcCallbackInterface = [NSXPCInterface interfaceWithProtocol: @protocol(SecuritydXPCCallbackProtocol)];
    [interface setInterface:rpcCallbackInterface
                forSelector:@selector(SecItemAddAndNotifyOnSync:syncCallback:complete:)
              argumentIndex:1
                    ofReply:0];

#if OCTAGON
    static NSMutableSet *errClasses;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // By finding classes by strings at runtime, we'll only get the CloudKit helpers if you link CloudKit
        // Plus, we don't have to weak-link cloudkit from Security.framework

        errClasses = [[NSMutableSet alloc] init];
        char *classes[] = {
            "NSError",
            "NSArray",
            "NSString",
            "NSNumber",
            "NSData",
            "NSDate",
            "CKReference",
            "CKAsset",
            "CLLocation",
            "CKPackage",
            "CKArchivedAnchoredPackage",
            "CKPrettyError",
            "CKRecordID",
            "NSURL",
        };

        for (unsigned n = 0; n < sizeof(classes)/sizeof(classes[0]); n++) {
            Class cls = objc_getClass(classes[n]);
            if (cls) {
                [errClasses addObject:cls];
            }
        }
    });

    @try {
        [rpcCallbackInterface setClasses:errClasses forSelector:@selector(callCallback:error:) argumentIndex:1 ofReply:NO];

        [interface setClasses:errClasses forSelector:@selector(SecItemAddAndNotifyOnSync:
                                                               syncCallback:
                                                               complete:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemFetchCurrentItemAcrossAllDevices:
                                                               identifier:
                                                               viewHint:
                                                               fetchCloudValue:
                                                               complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemDigest:
                                                               accessGroup:
                                                               complete:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(secItemSetCurrentItemAcrossAllDevices:
                                                               newCurrentItemHash:
                                                               accessGroup:
                                                               identifier:
                                                               viewHint:
                                                               oldCurrentItemReference:
                                                               oldCurrentItemHash:
                                                               complete:) argumentIndex:0 ofReply:YES];
    }
    @catch(NSException* e) {
        secerror("Could not configure SecuritydXPCProtocol: %@", e);
#if DEBUG
        @throw e;
#endif // DEBUG
    }
#endif // OCTAGON
}
@end

@implementation SecuritydXPCCallback
@synthesize callback = _callback;

-(instancetype)initWithCallback: (SecBoolNSErrorCallback) callback {
    if((self = [super init])) {
        _callback = callback;
    }
    return self;
}

- (void)callCallback: (bool) result error:(NSError*) error {
    self.callback(result, error);
}
@end

id<SecuritydXPCProtocol> SecuritydXPCProxyObject(void (^rpcErrorHandler)(NSError *))
{
    if (gSecurityd && gSecurityd->secd_xpc_server) {
        return (__bridge id<SecuritydXPCProtocol>)gSecurityd->secd_xpc_server;
    }

    static SecuritydXPCClient* rpc;
    static dispatch_once_t onceToken;
    static CFErrorRef cferror = NULL;
    static dispatch_queue_t queue;
    __block SecuritydXPCClient *result = nil;

    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("SecuritydXPCProxyObject", DISPATCH_QUEUE_SERIAL);
    });

    dispatch_sync(queue, ^{
        if (rpc) {
            result = rpc;
            return;
        }

        xpc_endpoint_t endpoint = _SecSecuritydCopyEndpoint(kSecXPCOpSecuritydXPCServerEndpoint, &cferror);
        if (endpoint == NULL) {
            return;
        }
        rpc = [[SecuritydXPCClient alloc] initWithEndpoint:endpoint];
        rpc.connection.invalidationHandler = ^{
            dispatch_sync(queue, ^{
                rpc = nil;
            });
        };
        [rpc.connection resume];

        result = rpc;
    });

    if (result == NULL) {
        rpcErrorHandler((__bridge NSError *)cferror);
        return NULL;
    } else {
        return [result.connection remoteObjectProxyWithErrorHandler: rpcErrorHandler];
    }
}

