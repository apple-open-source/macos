/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#if OCTAGON
@class CKKSKeychainView;
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CloudKitDependencies.h"

NS_ASSUME_NONNULL_BEGIN

/* Fetch Reasons */
@protocol CKKSFetchBecauseProtocol <NSObject>
@end
typedef NSString<CKKSFetchBecauseProtocol> CKKSFetchBecause;
extern CKKSFetchBecause* const CKKSFetchBecauseAPNS;
extern CKKSFetchBecause* const CKKSFetchBecauseAPIFetchRequest;
extern CKKSFetchBecause* const CKKSFetchBecauseCurrentItemFetchRequest;
extern CKKSFetchBecause* const CKKSFetchBecauseInitialStart;
extern CKKSFetchBecause* const CKKSFetchBecauseSecuritydRestart;
extern CKKSFetchBecause* const CKKSFetchBecausePreviousFetchFailed;
extern CKKSFetchBecause* const CKKSFetchBecauseKeyHierarchy;
extern CKKSFetchBecause* const CKKSFetchBecauseTesting;
extern CKKSFetchBecause* const CKKSFetchBecauseResync;

/* Clients that register to use fetches */
@interface CKKSCloudKitFetchRequest : NSObject
@property bool participateInFetch;
@property (nullable) CKServerChangeToken* changeToken;
@end

@class CKKSCloudKitDeletion;
@protocol CKKSChangeFetcherClient <NSObject>
- (CKRecordZoneID*)zoneID;
- (CKKSCloudKitFetchRequest*)participateInFetch;
- (bool)notifyFetchError:(NSError*)error;
- (void)changesFetched:(NSArray<CKRecord*>*)changedRecords
      deletedRecordIDs:(NSArray<CKKSCloudKitDeletion*>*)deleted
        oldChangeToken:(CKServerChangeToken* _Nullable)oldChangeToken
        newChangeToken:(CKServerChangeToken*)changeToken;
@end

// I don't understand why recordType isn't part of record ID, but deletions come in as both things
@interface CKKSCloudKitDeletion : NSObject
@property CKRecordID* recordID;
@property NSString* recordType;
- (instancetype)initWithRecordID:(CKRecordID*)recordID recordType:(NSString*)recordType;
@end


@interface CKKSFetchAllRecordZoneChangesOperation : CKKSGroupOperation
@property (readonly) Class<CKKSFetchRecordZoneChangesOperation> fetchRecordZoneChangesOperationClass;
@property (readonly) CKContainer* container;

// Set this to true before starting this operation if you'd like resync behavior:
//  Fetching everything currently in CloudKit and comparing to local copy
@property bool resync;

@property NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clientMap;
@property (nullable) NSMutableArray<CKRecordZoneID*>* fetchedZoneIDs;

@property NSSet<CKKSFetchBecause*>* fetchReasons;
@property NSSet<CKRecordZoneNotification*>* apnsPushes;

@property NSMutableDictionary<CKRecordID*, CKRecord*>* modifications;
@property NSMutableDictionary<CKRecordID*, CKKSCloudKitDeletion*>* deletions;
@property NSMutableDictionary<CKRecordID*, CKServerChangeToken*>* changeTokens;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContainer:(CKContainer*)container
                       fetchClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
                          clients:(NSArray<id<CKKSChangeFetcherClient>>*)clients
                     fetchReasons:(NSSet<CKKSFetchBecause*>*)fetchReasons
                       apnsPushes:(NSSet<CKRecordZoneNotification*>* _Nullable)apnsPushes
                      forceResync:(bool)forceResync
                 ckoperationGroup:(CKOperationGroup*)ckoperationGroup;

@end

NS_ASSUME_NONNULL_END

#endif
