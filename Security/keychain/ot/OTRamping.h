/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#ifndef OTRamping_h
#define OTRamping_h
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTRamp : NSObject

@property (nonatomic, readonly) NSString* featureName;
@property (nonatomic, readonly) CKKSAccountStateTracker *accountTracker;
@property (nonatomic, readonly) CKKSLockStateTracker      *lockStateTracker;
@property (nonatomic, readonly) CKKSReachabilityTracker   *reachabilityTracker;

-(instancetype)initWithRecordName:(NSString *) recordName
                 localSettingName:(NSString*) featureName
                         container:(CKContainer*) container
                          database:(CKDatabase*) database
                            zoneID:(CKRecordZoneID*) zoneID
                    accountTracker:(CKKSAccountStateTracker*) accountTracker
                  lockStateTracker:(CKKSLockStateTracker*) lockStateTracker
               reachabilityTracker:(CKKSReachabilityTracker*) reachabilityTracker
  fetchRecordRecordsOperationClass:(Class<CKKSFetchRecordsOperation>) fetchRecordRecordsOperationClass;

-(void)fetchRampRecord:(CKOperationDiscretionaryNetworkBehavior)networkBehavior
                  reply:(void (^)(BOOL featureAllowed, BOOL featurePromoted, BOOL featureVisible, NSInteger retryAfter, NSError *rampStateFetchError))recordRampStateFetchCompletionBlock;
-(BOOL)checkRampStateWithError:(NSError**)error;
@end
NS_ASSUME_NONNULL_END
#endif /* OCTAGON */
#endif /* OTRamping_h */
