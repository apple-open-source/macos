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
 *
 */

#import "KeychainCheck.h"
#import "SFKeychainControl.h"
#import "builtin_commands.h"
#import "SOSControlHelper.h"
#import "SOSTypes.h"
#import "CKKSControlProtocol.h"
#import <Security/SecItemPriv.h>
#import <Foundation/NSXPCConnection_Private.h>

@interface KeychainCheck ()

- (void)checkKeychain;
- (void)cleanKeychain;

@end

@implementation KeychainCheck {
    NSXPCConnection* _connection;
}

- (instancetype)initWithEndpoint:(xpc_endpoint_t)endpoint
{
    if (self = [super init]) {
        NSXPCListenerEndpoint* listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];
        [listenerEndpoint _setEndpoint:endpoint];
        _connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];
        if (!_connection) {
            return  nil;
        }

        NSXPCInterface* interface = [NSXPCInterface interfaceWithProtocol:@protocol(SFKeychainControl)];
        _connection.remoteObjectInterface = interface;
        [_connection resume];
    }

    return self;
}

- (void)checkKeychain
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        NSLog(@"failed to communicate with server with error: %@", error);
        dispatch_semaphore_signal(semaphore);
    }] rpcFindCorruptedItemsWithReply:^(NSArray* corruptedItems, NSError* error) {
        if (error) {
            NSLog(@"error searching keychain: %@", error.localizedDescription);
        }

        if (corruptedItems.count > 0) {
            NSLog(@"found %d corrupted items", (int)corruptedItems.count);
        }
        else {
            NSLog(@"no corrupted items found");
        }

        dispatch_semaphore_signal(semaphore);
    }];

    if (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER)) {
        NSLog(@"timed out trying to communicate with server");
    }
}

- (void)cleanKeychain
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        NSLog(@"failed to communicate with server with error: %@", error);
        dispatch_semaphore_signal(semaphore);
    }] rpcDeleteCorruptedItemsWithReply:^(bool success, NSError* error) {
        if (success) {
            NSLog(@"successfully cleaned keychain");
        }
        else {
            NSLog(@"error attempting to clean keychain: %@", error);
        }

        dispatch_semaphore_signal(semaphore);
    }];

    if (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER)) {
        NSLog(@"timed out trying to communicate with server");
    }
}

@end

int command_keychain_check(int argc, char* const* argv)
{
    KeychainCheck* keychainCheck = [[KeychainCheck alloc] initWithEndpoint:_SecSecuritydCopyKeychainControlEndpoint(NULL)];
    [keychainCheck checkKeychain];
    return 0;
}

int command_keychain_cleanup(int argc, char* const* argv)
{
    KeychainCheck* keychainCheck = [[KeychainCheck alloc] initWithEndpoint:_SecSecuritydCopyKeychainControlEndpoint(NULL)];
    [keychainCheck cleanKeychain];
    return 0;
}
