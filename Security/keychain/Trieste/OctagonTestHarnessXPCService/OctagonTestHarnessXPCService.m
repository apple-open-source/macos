//  Copyright (c) 2018 Apple Inc. All rights reserved.

#import "OctagonTestHarnessXPCService.h"

#import <objc/runtime.h>
#import <Security/CKKSControlProtocol.h>
#import <Security/SecAccessControlPriv.h>
#import <CloudServices/CloudServices.h>

#import "SecDbKeychainItem.h"
#import "SecRemoteDevice.h"
#import "OTControl.h"

@interface OctagonTestHarnessXPCService ()
@property (strong) SecRemoteDevice *remoteDevice;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"

@interface NSError (OctagonTestHarnessXPCService)
- (NSDictionary*)errorAsDictionary;
@end


@implementation NSError (OctagonTestHarnessXPCService)

- (NSDictionary*)errorAsDictionary {
    return @{
        @"domain": self.domain,
        @"code": @(self.code),
    };
}
@end


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


//MARK: Keychain


- (void)secItemAdd:(NSDictionary *)input complete:(void (^)(NSNumber *, NSDictionary * _Nullable))reply
{
    NSMutableDictionary *attributes = [input mutableCopy];
    CFTypeRef data = NULL;

    attributes[(__bridge NSString *)kSecReturnAttributes] = @YES;
    attributes[(__bridge NSString *)kSecReturnPersistentRef] = @YES;
    attributes[(__bridge NSString *)kSecReturnData] = @YES;

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, &data);
    NSDictionary *returnData = CFBridgingRelease(data);

    reply(@(status), returnData);

}
- (void)secItemCopyMatching:(NSDictionary *)input complete:(void (^)(NSNumber *, NSArray<NSDictionary *>* _Nullable))reply
{
    NSMutableDictionary *attributes = [input mutableCopy];
    CFTypeRef data = NULL;

    attributes[(__bridge NSString *)kSecReturnAttributes] = @YES;
    attributes[(__bridge NSString *)kSecReturnData] = @YES;
    attributes[(__bridge NSString *)kSecReturnPersistentRef] = @YES;
    attributes[(__bridge NSString *)kSecMatchLimit] = (__bridge id)kSecMatchLimitAll;

    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)attributes, &data);
    NSArray<NSDictionary *>* array = CFBridgingRelease(data);
    NSMutableArray *result = [NSMutableArray array];
    for (NSDictionary *d in array) {
        NSMutableDictionary *r = [d mutableCopy];
        r[@"accc"] = nil;
        [result addObject:r];
    }

    reply(@(status), result);

}
- (void)secItemDelete:(NSDictionary *)input complete:(void (^)(NSNumber *))reply
{
    NSMutableDictionary *attributes = [input mutableCopy];

    attributes[(__bridge NSString *)kSecReturnAttributes] = @YES;
    attributes[(__bridge NSString *)kSecReturnPersistentRef] = @YES;
    attributes[(__bridge NSString *)kSecReturnData] = @YES;

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)attributes);

    reply(@(status));
}

//MARK: CloudServices

- (void)csAccountInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply
{
    SecureBackup *sb = [[SecureBackup alloc] init];

    [sb getAccountInfoWithInfo:info completionBlock:^(NSDictionary *results, NSError *error) {
        reply(results, [error errorAsDictionary]);
     }];
}

- (void)csEnableInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply
{
    SecureBackup *sb = [[SecureBackup alloc] init];

    [sb enableWithInfo:info completionBlock:^(NSError *error) {
        reply(@{}, [error errorAsDictionary]);
    }];
}
- (void)csUpdateInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply
{
    SecureBackup *sb = [[SecureBackup alloc] init];
    [sb updateMetadataWithInfo:info completionBlock:^(NSError *error) {
        reply(@{}, [error errorAsDictionary]);
    }];
}
- (void)csDisableInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply
{
    SecureBackup *sb = [[SecureBackup alloc] init];
    [sb disableWithInfo:info completionBlock:^(NSError *error) {
        reply(@{}, [error errorAsDictionary]);
    }];
}

- (void)csRecoverInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply
{
    SecureBackup *sb = [[SecureBackup alloc] init];
    [sb recoverWithInfo:info completionBlock:^(NSDictionary *results, NSError *error) {
        reply(results, [error errorAsDictionary]);
    }];
}

@end

#pragma clang diagnostic pop
