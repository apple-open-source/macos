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

#include <TargetConditionals.h>

#if TARGET_OS_SIMULATOR

int main(int argc, char** argv)
{
    return 0;
}

#else

#import <Foundation/Foundation.h>
#import "supd.h"
#include "debugging.h"
#import <Foundation/NSXPCConnection_Private.h>
#include <xpc/private.h>

@interface ServiceDelegate : NSObject <NSXPCListenerDelegate>
@end

@implementation ServiceDelegate

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    NSNumber *num = [newConnection valueForEntitlement:@"com.apple.private.securityuploadd"];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secerror("xpc: Client (pid: %d) doesn't have entitlement", [newConnection processIdentifier]);
        return NO;
    } else {
        secinfo("xpc", "Client (pid: %d) properly entitled, let's go", [newConnection processIdentifier]);
    }

    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(supdProtocol)];
    supd *exportedObject = [supd instance];
    newConnection.exportedObject = exportedObject;
    [newConnection resume];
    return YES;
}

@end

int main(int argc, const char *argv[])
{
    secnotice("lifecycle", "supd lives!");
    ServiceDelegate *delegate = [ServiceDelegate new];

    // kick the singleton so it can register its xpc activity handler
    [supd instantiate];
    
    NSXPCListener *listener = [[NSXPCListener alloc] initWithMachServiceName:@"com.apple.securityuploadd"];
    listener.delegate = delegate;

    // We're always launched in response to client activity and don't want to sit around idle.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5ull * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        secnotice("lifecycle", "will exit when clean");
        xpc_transaction_exit_clean();
    });

    [listener resume];
    [[NSRunLoop currentRunLoop] run];
    return 0;
}

#endif  // !TARGET_OS_SIMULATOR
