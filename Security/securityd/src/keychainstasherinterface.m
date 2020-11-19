#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Foundation/NSData_Private.h>

#include "utilities/debugging.h"

#import "KeychainStasherProtocol.h"
#import "keychainstasherinterface.h"

NSString* const KeychainStasherMachServiceName = @"com.apple.security.KeychainStasher";

OSStatus stashKeyWithStashAgent(uid_t client, void const* keybytes, size_t keylen) {
    if (!keybytes || keylen == 0) {
        secerror("KeychainStasherInterface: No or truncated key, won't stash");
        return errSecParam;
    }

    secnotice("KeychainStasherInterface", "Reaching out to agent to stash key");
    __block OSStatus result = errSecInternalError;
    @autoreleasepool {
        NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:KeychainStasherMachServiceName options:0];
        [connection _setTargetUserIdentifier: client];
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(KeychainStasherProtocol)];
        [connection resume];

        id<KeychainStasherProtocol> proxy = [connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            secerror("KeychainStasherInterface: errorhandler for agent called: %@", error);
            result = errSecIO;
        }];

        NSData* key = [NSData _newZeroingDataWithBytes:keybytes length:keylen];
        [proxy stashKey:key withReply:^(NSError* error) {
            if (error) {
                secerror("KeychainStasherInterface: agent failed to stash key: %@", error);
                result = (int)error.code;
            } else {
                result = errSecSuccess;
            }
        }];

        [connection invalidate];
    }

    if (result == errSecSuccess) {
        secnotice("KeychainStasherInterface", "Successfully stashed key");
    }
    return result;
}

OSStatus loadKeyFromStashAgent(uid_t client, void** keybytes, size_t* keylen) {
    if (!keybytes || !keylen) {
        secerror("KeychainStasherInterface: No outparams for key, won't load");
        return errSecParam;
    }

    secnotice("KeychainStasherInterface", "Reaching out to agent to retrieve key");
    __block OSStatus result = errSecInternalError;
    @autoreleasepool {
        NSXPCConnection* connection = [[NSXPCConnection alloc] initWithMachServiceName:KeychainStasherMachServiceName options:0];
        [connection _setTargetUserIdentifier: client];
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(KeychainStasherProtocol)];
        [connection resume];

        id<KeychainStasherProtocol> proxy = [connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            secerror("KeychainStasherInterface: errorhandler for agent called: %@", error);
            result = errSecIO;
        }];

        [proxy loadKeyWithReply:^(NSData *key, NSError *error) {
            if (!key) {
                secerror("KeychainStasherInterface: agent failed to load key: %@", error);
                result = (int)error.code;
                return;
            }
            *keybytes = calloc(1, key.length);
            memcpy(*keybytes, key.bytes, key.length);
            *keylen = key.length;
            result = errSecSuccess;
        }];

        [connection invalidate];
    }

    if (result == errSecSuccess) {
        secnotice("KeychainStasherInterface", "Successfully loaded key");
    }
    return result;
}
