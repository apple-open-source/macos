#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <unistd.h>
#import <xpc/private.h>
#import <sandbox.h>

#import <Security/SecEntitlements.h>
#import <utilities/debugging.h>
#import <utilities/SecFileLocations.h>

#import "KeychainStasher.h"

NSString* const KeychainStasherMachServiceName = @"com.apple.security.KeychainStasher";

@interface ServiceDelegate : NSObject <NSXPCListenerDelegate>
@end

@implementation ServiceDelegate

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    // We should encounter no more than 1 transaction per boot in normal conditions, so get out of everyone's way ASAP
    xpc_transaction_exit_clean();

    NSNumber* value = [newConnection valueForEntitlement:kSecEntitlementPrivateStashService];
    if (value == nil || ![value boolValue]) {
        secerror("KeychainStasher: client not entitled, rejecting connection");
        [newConnection invalidate];
        return NO;
    }

    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(KeychainStasherProtocol)];
    newConnection.exportedObject = [KeychainStasher new];
    [newConnection resume];
    return YES;
}

@end

int main(int argc, const char *argv[])
{
    if (geteuid() == 0) {
        secerror("KeychainStasher invoked as root, do not want.");
        return 1;
    } else {
        secnotice("KeychainStasher", "Invoked with uid %d", geteuid());
    }

    NSString* analyticsdir = [[(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(nil) URLByAppendingPathComponent:@"Analytics/"] path];
    if (analyticsdir) {
        const char* sandbox_parameters[] = {"ANALYTICSDIR", analyticsdir.UTF8String, NULL};
        char* sandbox_error = NULL;
        if (0 != sandbox_init_with_parameters("com.apple.security.KeychainStasher", SANDBOX_NAMED, sandbox_parameters, &sandbox_error)) {
            secerror("unable to enter sandbox with parameter: %s", sandbox_error);
            sandbox_free_error(sandbox_error);
            abort();
        }
    } else {    // If this fails somehow we will go ahead without analytics
        char* sandbox_error = NULL;
        if (0 != sandbox_init("com.apple.security.KeychainStasher", SANDBOX_NAMED, &sandbox_error)) {
            secerror("unable to enter sandbox: %s", sandbox_error);
            sandbox_free_error(sandbox_error);
            abort();
        }
    }

    ServiceDelegate *delegate = [ServiceDelegate new];
    NSXPCListener *listener = [[NSXPCListener alloc] initWithMachServiceName:KeychainStasherMachServiceName];
    listener.delegate = delegate;
    [listener resume];
    [[NSRunLoop currentRunLoop] run];
    return 0;
}
