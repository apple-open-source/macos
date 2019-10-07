//  Copyright (c) 2018 Apple Inc. All rights reserved.

#import "OctagonTestHarnessXPCService.h"

#import <objc/runtime.h>
#import <Security/CKKSControlProtocol.h>
#import <Security/SecAccessControlPriv.h>
#import "SecDbKeychainItem.h"
#import "SecRemoteDevice.h"
#import "OTControl.h"

@interface OctagonTestHarnessXPCService ()
@property (strong) SecRemoteDevice *remoteDevice;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"

@implementation OctagonTestHarnessXPCService

- (instancetype)init {
    if ((self = [super init]) != NULL) {
        self.remoteDevice = [SecRemoteDevice new];
        if (self.remoteDevice == nil) {
            return nil;
        }
    }
    return self;
}

- (void)octagonReset:(NSString *)altDSID complete:(void (^)(NSNumber *, NSError *))complete {
    NSError *error = nil;

    OTControl* rpc = [OTControl controlObject:true error:&error];
    if (rpc == nil) {
        complete(@NO, error);

        return;
    }

    [rpc resetAndEstablish:NULL context:OTDefaultContext altDSID:altDSID reply:^(NSError * _Nullable e) {
        complete([NSNumber numberWithBool:e != NULL], e);
    }];
}

/* Oh, ObjC, you are my friend */
- (void)forwardInvocation:(NSInvocation *)invocation {
    struct objc_method_description desc = protocol_getMethodDescription(@protocol(SecRemoteDeviceProtocol), [invocation selector], true, true);
    if (desc.name == NULL) {
        [super forwardInvocation:invocation];
    } else {
        [invocation invokeWithTarget:self.remoteDevice];
    }
}

@end

#pragma clang diagnostic pop
