#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>

#import "SecEntitlements.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSControlProtocol.h"
#import "keychain/ckks/CKKSControlServer.h"
#import "keychain/ckks/CKKSViewManager.h"

@interface CKKSControlServer : NSObject <NSXPCListenerDelegate>
@end

@implementation CKKSControlServer

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
#if OCTAGON
    NSNumber *num = [newConnection valueForEntitlement:(__bridge NSString *)kSecEntitlementPrivateCKKS];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secerror("ckks: Client pid: %d doesn't have entitlement: %@",
                [newConnection processIdentifier], kSecEntitlementPrivateCKKS);
        return NO;
    }

    // In the future, we should consider vending a proxy object that can return a nicer error.
    if (!SecCKKSIsEnabled()) {
        secerror("ckks: Client pid: %d attempted to use CKKS, but CKKS is not enabled.",
                newConnection.processIdentifier);
        return NO;
    }

    newConnection.exportedInterface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    newConnection.exportedObject = [CKKSViewManager manager];

    [newConnection resume];

    return YES;
#else
    return NO;
#endif /* OCTAGON */
}

@end

void
CKKSControlServerInitialize(void)
{
    static dispatch_once_t once;
    static CKKSControlServer *server;
    static NSXPCListener *listener;

    dispatch_once(&once, ^{
        @autoreleasepool {
            server = [CKKSControlServer new];

            listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSecuritydCKKSServiceName)];
            listener.delegate = server;
            [listener resume];
        }
    });
}
