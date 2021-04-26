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
#import <Foundation/NSError_Private.h>
#import <dirhelper_priv.h>

#if TARGET_OS_OSX
#include <sandbox.h>
#include <notify.h>
#include <pwd.h>
#endif

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
    /* Client must either have the supd entitlement or the trustd file helping entitlement.
     * Each method of the protocol will additionally check for the entitlement it needs. */
    NSNumber *supdEntitlement = [newConnection valueForEntitlement:@"com.apple.private.securityuploadd"];
    BOOL hasSupdEntitlement = [supdEntitlement isKindOfClass:[NSNumber class]] && [supdEntitlement boolValue];
    NSNumber *trustdHelperEntitlement = [newConnection valueForEntitlement:@"com.apple.private.trustd.FileHelp"];
    BOOL hasTrustdHelperEntitlement = [trustdHelperEntitlement isKindOfClass:[NSNumber class]] && [trustdHelperEntitlement boolValue];

    /* expose the protocol based the client's entitlement (a client can't do both) */
    if (hasSupdEntitlement) {
        secinfo("xpc", "Client (pid: %d) properly entitled for supd interface, let's go", [newConnection processIdentifier]);
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(supdProtocol)];
    } else if (hasTrustdHelperEntitlement) {
        secinfo("xpc", "Client (pid: %d) properly entitled for trustd file helper interface, let's go", [newConnection processIdentifier]);
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(TrustdFileHelper_protocol)];
    } else {
        secerror("xpc: Client (pid: %d) doesn't have entitlement", [newConnection processIdentifier]);
        return NO;
    }

    supd *exportedObject = [[supd alloc] initWithConnection:newConnection];
    newConnection.exportedObject = exportedObject;
    [newConnection resume];
    return YES;
}

@end

static void securityuploadd_sandbox(void)
{
#if TARGET_OS_OSX
    // Enter the sandbox on macOS
    char homeDir[PATH_MAX] = {};
    struct passwd* pwd = getpwuid(getuid());
    if (pwd == NULL) {
        secerror("Failed to get home directory for user: %d", errno);
        exit(EXIT_FAILURE);
    }

    if (realpath(pwd->pw_dir, homeDir) == NULL) {
        strlcpy(homeDir, pwd->pw_dir, sizeof(homeDir));
    }

    const char *sandbox_params[] = {
        "HOME", homeDir,
        NULL
    };

    char *sberror = NULL;
    secerror("initializing securityuploadd sandbox with HOME=%s", homeDir);
    if (sandbox_init_with_parameters("com.apple.securityuploadd", SANDBOX_NAMED, sandbox_params, &sberror) != 0) {
        secerror("Failed to enter securityuploadd sandbox: %{public}s", sberror);
        exit(EXIT_FAILURE);
    }
#endif
}

int main(int argc, const char *argv[])
{
    secnotice("lifecycle", "supd lives!");
    [NSError _setFileNameLocalizationEnabled:NO];
    securityuploadd_sandbox();

    ServiceDelegate *delegate = [ServiceDelegate new];

    // Always create a supd instance to register for the background activity that doesn't check entitlements
    static supd *activity_supd = nil;
    activity_supd = [[supd alloc] initWithConnection:nil];
    
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
