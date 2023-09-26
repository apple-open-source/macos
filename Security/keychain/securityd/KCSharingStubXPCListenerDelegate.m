// server.c undefs KCSHARING on darwinOS / system securityd, we want to match those conditions here.
#if !KCSHARING || (defined(TARGET_DARWINOS) && TARGET_DARWINOS) || (defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM)

#import "KCSharingStubXPCListenerDelegate.h"

void KCSharingStubXPCServerInitialize(void) {
    [KCSharingStubXPCListenerDelegate sharedInstance];
}

@implementation KCSharingStubXPCListenerDelegate {
    NSXPCListener* _listener;
}

+ (instancetype)sharedInstance {
    static dispatch_once_t once;
    static KCSharingStubXPCListenerDelegate *delegate;

    dispatch_once(&once, ^{
        @autoreleasepool {
            delegate = [[KCSharingStubXPCListenerDelegate alloc] init];
        }
    });

    return delegate;
}

- (instancetype)init {
    if (self = [super init]) {
        _listener = [[NSXPCListener alloc] initWithMachServiceName:@"com.apple.security.kcsharing"];
        _listener.delegate = self;
        [_listener activate];
    }
    return self;
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    return NO;
}

@end

#endif /* !KCSHARING || (defined(TARGET_DARWINOS) && TARGET_DARWINOS) || (defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM) */
