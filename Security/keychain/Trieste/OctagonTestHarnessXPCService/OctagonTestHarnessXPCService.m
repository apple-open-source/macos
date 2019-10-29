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

    [self.remoteDevice otReset:altDSID complete:^(bool success, NSError * _Nullable error) {
        complete([NSNumber numberWithBool:success], error);
    }];
}

- (void)octagonPeerID:(NSString *)altDSID complete:(void (^)(NSString *, NSError *))complete {

    [self.remoteDevice otPeerID:altDSID complete:^(NSString *peerID, NSError * _Nullable error) {
        complete(peerID, error);
    }];
}

- (void)octagonInCircle:(NSString *)altDSID complete:(void (^)(NSNumber *,  NSError *_Nullable error))complete
{
    [self.remoteDevice otInCircle:altDSID complete:^(bool inCircle, NSError * _Nullable error) {
        complete(@(inCircle), error);
    }];
}




@end

#pragma clang diagnostic pop
