// Copyright (C) 2018 Apple Inc. All Rights Reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OctagonTestHarnessXPCServiceProtocol<NSObject>

// Trieste-compliant Octagon reset
- (void)octagonReset:(NSString *)altDSID complete:(void (^)(NSNumber *, NSError * _Nullable))complete;
- (void)octagonPeerID:(NSString *)altDSID complete:(void (^)(NSString *, NSError *_Nullable))complete;
- (void)octagonInCircle:(NSString *)altDSID complete:(void (^)(NSNumber *,  NSError * _Nullable))complete;

// Local Keychain
- (void)secItemAdd:(NSDictionary *)input complete:(void (^)(NSNumber *, NSDictionary * _Nullable))reply;
- (void)secItemCopyMatching:(NSDictionary *)input complete:(void (^)(NSNumber *, NSArray<NSDictionary *>* _Nullable))reply;
- (void)secItemDelete:(NSDictionary *)input complete:(void (^)(NSNumber *))reply;


// CloudServices
- (void)csAccountInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply;
- (void)csEnableInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply;
- (void)csUpdateInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply;
- (void)csDisableInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply;
- (void)csRecoverInfo:(NSDictionary *)info complete:(void (^)(NSDictionary * _Nullable, NSDictionary * _Nullable))reply;

@end

NS_ASSUME_NONNULL_END
