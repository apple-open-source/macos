/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
#import <objc/runtime.h>
#import <Foundation/NSXPCConnection_Private.h>

#import "utilities/debugging.h"
#import <Security/SecEntitlements.h>
#import "keychain/escrowrequest/EscrowRequestServer.h"
#import "keychain/escrowrequest/EscrowRequestXPCServer.h"
#import "keychain/escrowrequest/EscrowRequestXPCProtocol.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@interface EscrowRequestXPCServer : NSObject <NSXPCListenerDelegate>
@end

@implementation EscrowRequestXPCServer

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    NSNumber *num = [newConnection valueForEntitlement:kSecEntitlementPrivateEscrowRequest];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secerror("escrow-update: Client pid: %d doesn't have entitlement: %@",
                 [newConnection processIdentifier], kSecEntitlementPrivateEscrowRequest);
        return NO;
    }

    secnotice("escrowrequest", "received connection from client pid %d", [newConnection processIdentifier]);
    newConnection.exportedInterface = SecEscrowRequestSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(EscrowRequestXPCProtocol)]);

    newConnection.exportedObject = [EscrowRequestServer server];

    [newConnection resume];

    return YES;
}
@end

void
EscrowRequestXPCServerInitialize(void)
{
    static dispatch_once_t once;
    static EscrowRequestXPCServer *server;
    static NSXPCListener *listener;

    dispatch_once(&once, ^{
        @autoreleasepool {
            server = [EscrowRequestXPCServer new];

            listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSecuritydEscrowRequestServiceName)];
            listener.delegate = server;
            [listener resume];
        }
    });
}
