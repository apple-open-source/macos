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

#ifdef __OBJC__

#import <Foundation/Foundation.h>

@class CKKSExternalKey;
@class CKKSExternalTLKShare;

NS_ASSUME_NONNULL_BEGIN;

#define CKKSControlStatusDefaultNonTransientStateTimeout (1*NSEC_PER_SEC)

@protocol CKKSControlProtocol <NSObject>
- (void)performanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> * _Nullable))reply;
- (void)rpcResetLocal: (NSString* _Nullable)viewName reply: (void(^)(NSError* _Nullable result)) reply;

/**
 * Reset CloudKit zone with a caller provided reason, the reason will be logged in the operation group
 * name so that the reason for reset can be summarized server side.
 */
- (void)rpcResetCloudKit:(NSString* _Nullable)viewName reason:(NSString *)reason reply: (void(^)(NSError* _Nullable result)) reply;
- (void)rpcResync:(NSString* _Nullable)viewName reply: (void(^)(NSError* _Nullable result)) reply;
- (void)rpcResyncLocal:(NSString* _Nullable)viewName reply:(void(^)(NSError* _Nullable result))reply;

/**
 * Fetch status for the CKKS zones. If NULL is passed in a viewname, all zones are fetched.
 * If `fast` is `YES`, this call will avoid expensive operations (and thus not
 * report them), and also omit the global status.
 */
- (void)rpcStatus:(NSString* _Nullable)viewName fast:(BOOL)fast waitForNonTransientState:(dispatch_time_t)waitForTransientTimeout reply: (void(^)(NSArray<NSDictionary*>* _Nullable result, NSError* _Nullable error)) reply;

- (void)rpcFetchAndProcessChanges:(NSString* _Nullable)viewName classA:(bool)classAError onlyIfNoRecentFetch:(bool)onlyIfNoRecentFetch reply:(void(^)(NSError* _Nullable result))reply;
- (void)rpcPushOutgoingChanges:(NSString* _Nullable)viewName reply: (void(^)(NSError* _Nullable result)) reply;
- (void)rpcGetCKDeviceIDWithReply:(void (^)(NSString* _Nullable ckdeviceID))reply;
- (void)rpcCKMetric:(NSString *)eventName attributes:(NSDictionary *)attributes reply:(void(^)(NSError* _Nullable result)) reply;

- (void)proposeTLKForSEView:(NSString*)seViewName
                proposedTLK:(CKKSExternalKey *)proposedTLK
              wrappedOldTLK:(CKKSExternalKey * _Nullable)wrappedOldTLK
                  tlkShares:(NSArray<CKKSExternalTLKShare*>*)shares
                      reply:(void(^)(NSError* _Nullable error))reply;

- (void)fetchSEViewKeyHierarchy:(NSString*)seViewName
                     forceFetch:(BOOL)forceFetch
                          reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                          NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                          NSArray<CKKSExternalTLKShare*>* _Nullable curentTLKShares,
                                          NSError* _Nullable error))reply;

- (void)modifyTLKSharesForSEView:(NSString*)seViewName
                          adding:(NSArray<CKKSExternalTLKShare*>*)sharesToAdd
                        deleting:(NSArray<CKKSExternalTLKShare*>*)sharesToDelete
                           reply:(void (^)(NSError* _Nullable error))reply;

- (void)deleteSEView:(NSString*)seViewName
               reply:(void (^)(NSError* _Nullable error))reply;

- (void)toggleHavoc:(void (^)(BOOL havoc, NSError* _Nullable error))reply;

- (void)pcsMirrorKeysForServices:(NSDictionary<NSNumber*,NSArray<NSData*>*>*)services reply:(void (^)(NSDictionary<NSNumber*,NSArray<NSData*>*>* _Nullable result, NSError* _Nullable error))reply;

@end

NSXPCInterface* CKKSSetupControlProtocol(NSXPCInterface* interface);

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */
