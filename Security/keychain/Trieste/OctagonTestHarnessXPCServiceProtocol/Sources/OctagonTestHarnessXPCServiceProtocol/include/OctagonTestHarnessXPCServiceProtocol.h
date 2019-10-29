// Copyright (C) 2018 Apple Inc. All Rights Reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OctagonTestHarnessXPCServiceProtocol<NSObject>

// Trieste-compliant Octagon reset
- (void)octagonReset:(NSString *)altDSID complete:(void (^)(NSNumber *, NSError * _Nullable))complete;
- (void)octagonPeerID:(NSString *)altDSID complete:(void (^)(NSString *, NSError *_Nullable))complete;
- (void)octagonInCircle:(NSString *)altDSID complete:(void (^)(NSNumber *,  NSError * _Nullable))complete;

@end

NS_ASSUME_NONNULL_END
