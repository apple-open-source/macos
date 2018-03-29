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

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>

#import <utilities/debugging.h>
#import "SecEntitlements.h"
#import "keychain/ot/OctagonControlServer.h"
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OT.h"

@interface OctagonControlServer : NSObject <NSXPCListenerDelegate>
@end

@implementation OctagonControlServer

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
#if OCTAGON
    NSNumber *num = [newConnection valueForEntitlement:kSecEntitlementPrivateOctagon];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secerror("octagon: Client pid: %d doesn't have entitlement: %@",
                 [newConnection processIdentifier], kSecEntitlementPrivateOctagon);
        return NO;
    }
    // In the future, we should consider vending a proxy object that can return a nicer error.
    if (!SecOTIsEnabled()) {
        secerror("Octagon: Client pid: %d attempted to use Octagon, but Octagon is not enabled.",
                 newConnection.processIdentifier);
        return NO;
    }
    
    secnotice("octagon", "received connection from client pid %d", [newConnection processIdentifier]);
    newConnection.exportedInterface = OTSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(OTControlProtocol)]);
    newConnection.exportedObject = [OTManager manager];

    [newConnection resume];

    return YES;
#else // OCTAGON
    secerror("octagon does not exist on this platform");
    return NO;
#endif // OCTAGON
}
@end

void
OctagonControlServerInitialize(void)
{
    static dispatch_once_t once;
    static OctagonControlServer *server;
    static NSXPCListener *listener;

    dispatch_once(&once, ^{
        @autoreleasepool {
            server = [OctagonControlServer new];

            listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSecuritydOctagonServiceName)];
            listener.delegate = server;
            [listener resume];
        }
    });
}
