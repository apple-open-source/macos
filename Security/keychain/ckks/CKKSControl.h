/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// You must be 64-bit to use this class.
#if __OBJC2__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN


typedef NS_ENUM(NSUInteger, CKKSKnownBadState) {
    CKKSKnownStatePossiblyGood = 0,  // State might be good: give your operation a shot!
    CKKSKnownStateTLKsMissing = 1,   // CKKS doesn't have the TLKs: your operation will likely not succeed
    CKKSKnownStateWaitForUnlock = 2, // CKKS has some important things to do, but the device is locked. Your operation will likely not succeed
};

@interface CKKSControl : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithConnection:(NSXPCConnection*)connection;

- (void)rpcStatus:(NSString* _Nullable)viewName
            reply:(void (^)(NSArray<NSDictionary*>* _Nullable result, NSError* _Nullable error))reply;
- (void)rpcResetLocal:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply;
- (void)rpcResetCloudKit:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply;
- (void)rpcResync:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply;
- (void)rpcFetchAndProcessChanges:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply;
- (void)rpcFetchAndProcessClassAChanges:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply;
- (void)rpcPushOutgoingChanges:(NSString* _Nullable)viewName reply:(void (^)(NSError* _Nullable error))reply;

- (void)rpcPerformanceCounters:(void(^)(NSDictionary <NSString *,NSNumber *> *,NSError*))reply;
- (void)rpcGetCKDeviceIDWithReply:(void (^)(NSString* ckdeviceID))reply;

// convenience wrapper for rpcStatus:reply:
- (void)rpcTLKMissing:(NSString* _Nullable)viewName reply:(void (^)(bool missing))reply;
- (void)rpcKnownBadState:(NSString* _Nullable)viewName reply:(void (^)(CKKSKnownBadState))reply;

+ (CKKSControl* _Nullable)controlObject:(NSError* _Nullable __autoreleasing* _Nullable)error;

@end

NS_ASSUME_NONNULL_END
#endif  // __OBJC__
