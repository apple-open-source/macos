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
#ifndef OTCloudStore_h
#define OTCloudStore_h

#if OCTAGON
#import "keychain/ot/OTLocalStore.h"

#import <CloudKit/CloudKit.h>
#import <CloudKit/CKContainer_Private.h>

#import "keychain/ckks/CKKSZone.h"
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"
#import "keychain/ckks/CKKSRecordHolder.h"
#import "keychain/ckks/CKKSZoneModifier.h"
#import "OTBottledPeerRecord.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTCloudStore : CKKSZone <CKKSZoneUpdateReceiver>

@property (nonatomic, readonly) NSString* contextID;
@property (nonatomic, readonly) NSString* dsid;
@property (nonatomic, readonly) NSString* containerName;
@property (nonatomic, readonly) CKRecordID* recordID;
@property (nonatomic, readonly) CKKSResultOperation* viewSetupOperation;
@property CKKSCondition* loggedIn;
@property CKKSCondition* loggedOut;


- (instancetype) initWithContainer:(CKContainer*) container
                          zoneName:(NSString*)zoneName
                    accountTracker:(nullable CKKSAccountStateTracker*)accountTracker
               reachabilityTracker:(nullable CKKSReachabilityTracker*)reachabilityTracker
                        localStore:(OTLocalStore*)localStore
                         contextID:(NSString*)contextID
                              dsid:(NSString*)dsid
                      zoneModifier:(CKKSZoneModifier*)zoneModifier
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
                    operationQueue:(nullable NSOperationQueue *)operationQueue;


- (BOOL) uploadBottledPeerRecord:(OTBottledPeerRecord *)bprecord
                  escrowRecordID:(NSString *)escrowRecordID
                           error:(NSError**)error;
- (BOOL) downloadBottledPeerRecord:(NSError**)error;
- (BOOL) removeBottledPeerRecordID:(CKRecordID*)recordID error:(NSError**)error;
- (nullable NSArray*) retrieveListOfEligibleEscrowRecordIDs:(NSError**)error;

- (void)notifyZoneChange:(CKRecordZoneNotification* _Nullable)notification;
- (void)handleCKLogin;
- (BOOL) performReset:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
#endif
#endif /* OTCloudStore_h */
