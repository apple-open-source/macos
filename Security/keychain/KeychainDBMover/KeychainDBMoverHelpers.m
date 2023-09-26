//
//  KeychainDBMoverHelpers.m
//  Security
//

#import <Foundation/Foundation.h>
#import "keychain/KeychainDBMover/KeychainDBMoverHelpers.h"
#import "keychain/KeychainDBMover/KeychainDBMover.h"
#import "debugging.h"

OSStatus SecKeychainMoveUserDb(void) {
    __block OSStatus status = errSecServiceNotAvailable;

    NSXPCConnection* moverCxn = [[NSXPCConnection alloc] initWithServiceName:@"com.apple.security.KeychainDBMover"];
    secnotice("SecKeychainMoveUserDb", "moverCxn: %@", moverCxn);
    moverCxn.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(KeychainDBMoverProtocol)];
    [moverCxn resume];
    secdebug("SecKeychainMoveUserDb", "moverCxn resumed");

    [[moverCxn synchronousRemoteObjectProxyWithErrorHandler:^(NSError *err) {
        secerror("SecKeychainMoveUserDb: remote object failed with error: %@", err);
        status = (int)[err code];
    }] moveUserDbWithReply:^(NSError *err) {
        if (err) {
            secerror("SecKeychainMoveUserDb: replied with error: %@", err);
            status = (int)[err code];
        } else {
            status = errSecSuccess;
        }
    }];

    secdebug("SecKeychainMoveUserDb", "invalidating");
    [moverCxn invalidate];

    secnotice("SecKeychainMoveUserDb", "returning %d", (int)status);
    return status;
}
