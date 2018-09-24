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
#if OCTAGON

#import <Foundation/Foundation.h>
#import <Foundation/NSKeyedArchiver_Private.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/OTCloudStore.h"
#import "keychain/ot/OTCloudStoreState.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import <utilities/debugging.h>


NS_ASSUME_NONNULL_BEGIN

/* Octagon Trust Local Context Record Constants  */
static NSString* OTCKRecordContextID = @"contextID";
static NSString* OTCKRecordDSID = @"accountDSID";
static NSString* OTCKRecordContextName = @"contextName";
static NSString* OTCKRecordZoneCreated = @"zoneCreated";
static NSString* OTCKRecordSubscribedToChanges = @"subscribedToChanges";
static NSString* OTCKRecordChangeToken = @"changeToken";
static NSString* OTCKRecordEgoPeerID = @"egoPeerID";
static NSString* OTCKRecordEgoPeerCreationDate = @"egoPeerCreationDate";
static NSString* OTCKRecordRecoverySigningSPKI = @"recoverySigningSPKI";
static NSString* OTCKRecordRecoveryEncryptionSPKI = @"recoveryEncryptionSPKI";
static NSString* OTCKRecordBottledPeerTableEntry = @"bottledPeer";

/* Octagon Trust Local Peer Record  */
static NSString* OTCKRecordPeerID = @"peerID";
static NSString* OTCKRecordPermanentInfo = @"permanentInfo";
static NSString* OTCKRecordStableInfo = @"stableInfo";
static NSString* OTCKRecordDynamicInfo = @"dynamicInfo";
static NSString* OTCKRecordRecoveryVoucher = @"recoveryVoucher";
static NSString* OTCKRecordIsEgoPeer = @"isEgoPeer";

/* Octagon Trust BottledPeerSchema  */
static NSString* OTCKRecordEscrowRecordID = @"escrowRecordID";
static NSString* OTCKRecordBottle = @"bottle";
static NSString* OTCKRecordSPID = @"spID";
static NSString* OTCKRecordEscrowSigningSPKI = @"escrowSigningSPKI";
static NSString* OTCKRecordPeerSigningSPKI = @"peerSigningSPKI";
static NSString* OTCKRecordSignatureFromEscrow = @"signatureUsingEscrow";
static NSString* OTCKRecordSignatureFromPeerKey = @"signatureUsingPeerKey";
static NSString* OTCKRecordEncodedRecord = @"encodedRecord";

/* Octagon Table Names */
static NSString* const contextTable = @"context";
static NSString* const peerTable = @"peer";
static NSString* const bottledPeerTable = @"bp";

/* Octagon Trust Schemas */
static NSString* const octagonZoneName = @"OctagonTrustZone";

/* Octagon Cloud Kit defines */
static NSString* OTCKContainerName = @"com.apple.security.keychain";
static NSString* OTCKZoneName = @"OctagonTrust";
static NSString* OTCKRecordName = @"bp-";
static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";

@interface OTCloudStore ()
@property (nonatomic, strong) NSString* dsid;
@property (nonatomic, strong) NSString* containerName;
@property (nonatomic, strong) CKModifyRecordsOperation* modifyRecordsOperation;
@property (nonatomic, strong) CKDatabaseOperation<CKKSFetchRecordZoneChangesOperation>* fetchRecordZoneChangesOperation;
@property (nonatomic, strong) NSOperationQueue *operationQueue;
@property (nonatomic, strong) OTLocalStore* localStore;
@property (nonatomic, strong) CKKSResultOperation* viewSetupOperation;
@property (nonatomic, strong) NSError* error;
@end

@class CKKSAPSReceiver;

@interface OTCloudStore()

@property CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneCreationOperation;
@property CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneDeletionOperation;
@property CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation;

@property NSOperation* accountLoggedInDependency;

@property NSHashTable<NSOperation*>* accountOperations;
@end

@implementation OTCloudStore

- (instancetype) initWithContainer:(CKContainer*) container
                          zoneName:(NSString*)zoneName
                    accountTracker:(nullable CKKSCKAccountStateTracker*)accountTracker
               reachabilityTracker:(nullable CKKSReachabilityTracker*)reachabilityTracker
                        localStore:(OTLocalStore*)localStore
                         contextID:(NSString*)contextID
                              dsid:(NSString*)dsid
fetchRecordZoneChangesOperationClass:(Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
        fetchRecordsOperationClass:(Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
               queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
 modifySubscriptionsOperationClass:(Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
   modifyRecordZonesOperationClass:(Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                apsConnectionClass:(Class<CKKSAPSConnection>) apsConnectionClass
                    operationQueue:(nullable NSOperationQueue *)operationQueue
{
    
    self = [super initWithContainer:container
                           zoneName:zoneName
                     accountTracker:accountTracker
                reachabilityTracker:reachabilityTracker
fetchRecordZoneChangesOperationClass:fetchRecordZoneChangesOperationClass
         fetchRecordsOperationClass:fetchRecordsOperationClass
                queryOperationClass:queryOperationClass
  modifySubscriptionsOperationClass:modifySubscriptionsOperationClass
    modifyRecordZonesOperationClass:modifyRecordZonesOperationClass
                 apsConnectionClass:apsConnectionClass];
    
    if(self){
        if (!operationQueue) {
            operationQueue = [[NSOperationQueue alloc] init];
        }
        _contextID = [contextID copy];
        _localStore = localStore;
        _containerName = OTCKContainerName;
        _dsid = [dsid copy];
        _operationQueue = operationQueue;
        self.queue = dispatch_queue_create([[NSString stringWithFormat:@"OctagonTrustQueue.%@.zone.%@", container.containerIdentifier, zoneName] UTF8String], DISPATCH_QUEUE_SERIAL);
        [self initializeZone];
    }
    return self;
    
}

-(CKKSResultOperation*) otFetchAndProcessUpdates
{
    CKKSResultOperation* fetchOp = [CKKSResultOperation named:@"fetch-and-process-updates-watcher" withBlock:^{}];

    __weak __typeof(self) weakSelf = self;

    [self dispatchSync: ^bool{

        OTCloudStoreState* state = [OTCloudStoreState state: self.zoneName];

        CKFetchRecordZoneChangesConfiguration* options = [[CKFetchRecordZoneChangesConfiguration alloc] init];
        options.previousServerChangeToken = state.changeToken;

        self.fetchRecordZoneChangesOperation = [[[self.fetchRecordZoneChangesOperationClass class] alloc] initWithRecordZoneIDs:@[self.zoneID] configurationsByRecordZoneID:@{self.zoneID : options}];

        self.fetchRecordZoneChangesOperation.recordChangedBlock = ^(CKRecord *record) {
            secinfo("octagon", "CloudKit notification: record changed(%@): %@", [record recordType], record);
            __strong __typeof(weakSelf) strongSelf = weakSelf;

            if(!strongSelf) {
                secnotice("octagon", "received callback for released object");
                fetchOp.error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTCloudStore userInfo:@{NSLocalizedDescriptionKey: @"received callback for released object"}];

                fetchOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerFetchRecords;

                return;
            }
            if ([record.recordType isEqualToString:OTCKRecordBottledPeerType]) {
                NSError* localError = nil;

                //write to localStore
                OTBottledPeerRecord *rec = [[OTBottledPeerRecord alloc] init];
                rec.bottle = record[OTCKRecordBottle];
                rec.spID = record[OTCKRecordSPID];
                rec.escrowRecordID = record[OTCKRecordEscrowRecordID];
                rec.escrowedSigningSPKI = record[OTCKRecordEscrowSigningSPKI];
                rec.peerSigningSPKI = record[OTCKRecordPeerSigningSPKI];
                rec.signatureUsingEscrowKey = record[OTCKRecordSignatureFromEscrow];
                rec.signatureUsingPeerKey = record[OTCKRecordSignatureFromPeerKey];
                rec.encodedRecord = [strongSelf recordToData:record];
                rec.launched = @"YES";
                BOOL result = [strongSelf.localStore insertBottledPeerRecord:rec escrowRecordID:record[OTCKRecordEscrowRecordID] error:&localError];
                if(!result || localError){
                    secerror("Could not write bottled peer record:%@ to database: %@", record.recordID.recordName, localError);
                    fetchOp.error = localError;
                    fetchOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerFetchRecords;

                }
                secnotice("octagon", "fetched changes: %@", record);
            }
        };

        self.fetchRecordZoneChangesOperation.recordWithIDWasDeletedBlock = ^(CKRecordID *RecordID, NSString *recordType) {
            secinfo("octagon", "CloudKit notification: deleted record(%@): %@", recordType, RecordID);
        };

        self.fetchRecordZoneChangesOperation.recordZoneChangeTokensUpdatedBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            NSError* error = nil;
            OTCloudStoreState* state = [OTCloudStoreState state: strongSelf.zoneName];
            secdebug("octagon", "Received a new server change token: %@ %@", serverChangeToken, clientChangeTokenData);
            state.changeToken = serverChangeToken;

            if(error) {
                secerror("octagon: Couldn't save new server change token: %@", error);
                fetchOp.error = error;
                fetchOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerFetchRecords;
            }
        };

        // Completion blocks don't count for dependencies. Use this intermediate operation hack instead.
        NSBlockOperation* recordZoneChangesCompletedOperation = [[NSBlockOperation alloc] init];
        self.fetchRecordZoneChangesOperation.recordZoneFetchCompletionBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData, BOOL moreComing, NSError * recordZoneError) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secnotice("octagon", "received callback for released object");
                return;
            }
            if(recordZoneError) {
                secerror("octagon: FetchRecordZoneChanges(%@) error: %@", strongSelf.zoneName, recordZoneError);
                fetchOp.error = recordZoneError;
                fetchOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerFetchRecords;
            }

            // TODO: fetch state here
            if(serverChangeToken) {
                NSError* error = nil;
                secdebug("octagon", "Zone change fetch complete: received a new server change token: %@ %@", serverChangeToken, clientChangeTokenData);
                state.changeToken = serverChangeToken;
                if(error) {
                    secerror("octagon: Couldn't save new server change token: %@", error);
                    fetchOp.error = error;
                    fetchOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerFetchRecords;
                }
            }
            secdebug("octagon", "Record zone fetch complete: changeToken=%@ error=%@", serverChangeToken, recordZoneError);

            [strongSelf.operationQueue addOperation: recordZoneChangesCompletedOperation];
            [strongSelf.operationQueue addOperation: fetchOp];

        };
        self.fetchRecordZoneChangesOperation.fetchRecordZoneChangesCompletionBlock = ^(NSError * _Nullable operationError) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secnotice("octagon", "received callback for released object");
                fetchOp.error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTCloudStore userInfo:@{NSLocalizedDescriptionKey: @"received callback for released object"}];
                fetchOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerFetchRecords;
                return;
            }
            secnotice("octagon", "Record zone changes fetch complete: error=%@", operationError);
        };
        return true;
    }];
    [self.database addOperation: self.fetchRecordZoneChangesOperation];

    return fetchOp;
}


- (void)notifyZoneChange:(CKRecordZoneNotification* _Nullable)notification
{
    secnotice("octagon", "received notify zone change.  notification: %@", notification);

    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-fetch-and-process-changes" withBlock:^{}];

    [op addSuccessDependency: [self otFetchAndProcessUpdates]];

    [op timeout:(SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : 120*NSEC_PER_SEC)];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error != nil) {
        secerror("octagon: failed to fetch changes error:%@", op.error);
    }
    else{
        secnotice("octagon", "downloaded bottled peer records");
    }
}

-(BOOL) downloadBottledPeerRecord:(NSError**)error
{
    secnotice("octagon", "downloadBottledPeerRecord");
    BOOL result = NO;
    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-fetch-and-process-changes" withBlock:^{}];

    [op addSuccessDependency: [self otFetchAndProcessUpdates]];

    [op timeout:(SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : 120*NSEC_PER_SEC)];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error != nil) {
        secerror("octagon: failed to fetch changes error:%@", op.error);
        if(error){
            *error = op.error;
        }
    }
    else{
        result = YES;
        secnotice("octagon", "downloaded bottled peer records");
    }
    return result;
}

- (nullable NSArray*) retrieveListOfEligibleEscrowRecordIDs:(NSError**)error
{
    NSError* localError = nil;

    NSMutableArray* recordIDs = [NSMutableArray array];
    
    //fetch any recent changes first before gathering escrow record ids
    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-fetch-and-process-changes" withBlock:^{}];
    
    secnotice("octagon", "Beginning CloudKit fetch");
    [op addSuccessDependency: [self otFetchAndProcessUpdates]];
    
    [op timeout:(SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : 120*NSEC_PER_SEC)];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error != nil) {
        secnotice("octagon", "failed to fetch changes error:%@", op.error);
    }

    secnotice("octagon", "checking local store for bottles");

    //check localstore for bottles
    NSArray* localStoreBottledPeerRecords = [self.localStore readAllLocalBottledPeerRecords:&localError];
    if(!localStoreBottledPeerRecords)
    {
        secerror("octagon: local store contains no bottled peer entries: %@", localError);
        if(error){
            *error = localError;
        }
        return nil;
    }
    for(OTBottledPeerRecord* entry in localStoreBottledPeerRecords){
        NSString* escrowID = entry.escrowRecordID;
        if(escrowID && ![recordIDs containsObject:escrowID]){
            [recordIDs addObject:escrowID];
        }
    }

    return recordIDs;
}

-(CKRecord*) dataToRecord:(NSData*)encodedRecord
{
    NSKeyedUnarchiver *coder = [[NSKeyedUnarchiver alloc] initForReadingFromData:encodedRecord error:nil];
    CKRecord* record = [[CKRecord alloc] initWithCoder:coder];
    [coder finishDecoding];
    return record;
}

-(NSData*) recordToData:(CKRecord*)record
{
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [record encodeWithCoder:archiver];
    [archiver finishEncoding];

    return archiver.encodedData;
}

-( CKRecord* _Nullable ) CKRecordFromMirror:(CKRecordID*)recordID bpRecord:(OTBottledPeerRecord*)bprecord escrowRecordID:(NSString*)escrowRecordID error:(NSError**)error
{
    CKRecord* record = nil;

    OTBottledPeerRecord* recordFromDB = [self.localStore readLocalBottledPeerRecordWithRecordID:recordID.recordName error:error];
    if(recordFromDB && recordFromDB.encodedRecord != nil){
        record = [self dataToRecord:recordFromDB.encodedRecord];
    }
    else{
        record = [[CKRecord alloc] initWithRecordType:OTCKRecordBottledPeerType recordID:recordID];
    }

    if(record == nil){
        secerror("octagon: failed to create cloud kit record");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTCloudStore userInfo:@{NSLocalizedDescriptionKey: @"failed to create cloud kit record"}];
        }
        return nil;
    }
    record[OTCKRecordPeerID] = bprecord.peerID;
    record[OTCKRecordSPID] = bprecord.spID;
    record[OTCKRecordEscrowSigningSPKI] = bprecord.escrowedSigningSPKI;
    record[OTCKRecordPeerSigningSPKI] = bprecord.peerSigningSPKI;
    record[OTCKRecordEscrowRecordID] = escrowRecordID;
    record[OTCKRecordBottle] = bprecord.bottle;
    record[OTCKRecordSignatureFromEscrow] = bprecord.signatureUsingEscrowKey;
    record[OTCKRecordSignatureFromPeerKey] = bprecord.signatureUsingPeerKey;
    
    return record;
}

-(CKKSResultOperation*) modifyRecords:(NSArray<CKRecord *>*) recordsToSave deleteRecordIDs:(NSArray<CKRecordID*>*) recordIDsToDelete
{
    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* modifyOp = [CKKSResultOperation named:@"modify-records-watcher" withBlock:^{}];

    [self dispatchSync: ^bool{
        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave recordIDsToDelete:recordIDsToDelete];

        self.modifyRecordsOperation.atomic = YES;
        self.modifyRecordsOperation.longLived = NO; // The keys are only in memory; mark this explicitly not long-lived

        // Currently done during buddy. User is waiting.
        self.modifyRecordsOperation.configuration.automaticallyRetryNetworkFailures = NO;
        self.modifyRecordsOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;

        self.modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;

        self.modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            // These should all fail or succeed as one. Do the hard work in the records completion block.
            if(!error) {
                secnotice("octagon", "Successfully completed upload for %@", record.recordID.recordName);
                
            } else {
                secerror("octagon: error on row: %@ %@", record.recordID.recordName, error);
                modifyOp.error = error;
                modifyOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerModifyRecords;
                [weakSelf.operationQueue addOperation:modifyOp];
            }
        };
        self.modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
            secnotice("octagon", "Completed trust update");
            __strong __typeof(weakSelf) strongSelf = weakSelf;

            if(error){
                modifyOp.error = error;
                modifyOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerModifyRecords;
                secerror("octagon: received error from cloudkit: %@", error);
                if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorPartialFailure)) {
                    NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = error.userInfo[CKPartialErrorsByItemIDKey];
                    ckksnotice("octagon", strongSelf, "failed records %@", failedRecords);
                }
                return;
            }
            if(!strongSelf) {
                secerror("octagon: received callback for released object");
                modifyOp.error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorOTCloudStore userInfo:@{NSLocalizedDescriptionKey: @"received callback for released object"}];
                modifyOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerModifyRecords;
                [strongSelf.operationQueue addOperation:modifyOp];
                return;
            }
            
            if(savedRecords && [savedRecords count] > 0){
                for(CKRecord* record in savedRecords){
                    NSError* localError = nil;
                    secnotice("octagon", "saving recordID: %@ changeToken:%@", record.recordID.recordName, record.recordChangeTag);
                    
                    //write to localStore
                    OTBottledPeerRecord *rec = [[OTBottledPeerRecord alloc] init];
                    rec.bottle = record[OTCKRecordBottle];
                    rec.spID = record[OTCKRecordSPID];
                    rec.escrowRecordID = record[OTCKRecordEscrowRecordID];
                    rec.signatureUsingEscrowKey = record[OTCKRecordSignatureFromEscrow];
                    rec.signatureUsingPeerKey = record[OTCKRecordSignatureFromPeerKey];
                    rec.encodedRecord = [strongSelf recordToData:record];
                    rec.launched = @"YES";
                    rec.escrowedSigningSPKI = record[OTCKRecordEscrowSigningSPKI];
                    rec.peerSigningSPKI = record[OTCKRecordPeerSigningSPKI];
                    
                    BOOL result = [strongSelf.localStore insertBottledPeerRecord:rec escrowRecordID:record[OTCKRecordEscrowRecordID] error:&localError];

                    if(!result || localError){
                        secerror("Could not write bottled peer record:%@ to database: %@", record.recordID.recordName, localError);
                    }
                    
                    if(localError){
                        secerror("octagon: could not save to database: %@", localError);
                        modifyOp.error = localError;
                        modifyOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerModifyRecords;
                    }
                }
            }
            else if(deletedRecordIDs && [deletedRecordIDs count] >0){
                for(CKRecordID* recordID in deletedRecordIDs){
                    secnotice("octagon", "removed recordID: %@", recordID);
                    NSError* localError = nil;
                    BOOL result = [strongSelf.localStore deleteBottledPeer:recordID.recordName error:&localError];
                    if(!result){
                        secerror("octagon: could not remove record id: %@, error:%@", recordID, localError);
                        modifyOp.error = localError;
                        modifyOp.descriptionErrorCode = CKKSResultDescriptionPendingBottledPeerModifyRecords;
                    }
                }
            }
            [strongSelf.operationQueue addOperation:modifyOp];
        };
        return true;
    }];
    
    [self.database addOperation: self.modifyRecordsOperation];
    return modifyOp;
}

- (BOOL) uploadBottledPeerRecord:(OTBottledPeerRecord *)bprecord
                  escrowRecordID:(NSString *)escrowRecordID
                           error:(NSError**)error
{
    secnotice("octagon", "sending bottled peer to cloudkit");
    BOOL result = YES;
    
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:bprecord.recordName zoneID:self.zoneID];
    CKRecord *record = [self CKRecordFromMirror:recordID bpRecord:bprecord escrowRecordID:escrowRecordID error:error];

    if(!record){
        return NO;
    }
    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-modify-changes" withBlock:^{}];

    secnotice("octagon", "Beginning CloudKit ModifyRecords");
    [op addSuccessDependency: [self modifyRecords:@[ record ] deleteRecordIDs:@[]]];

    [op timeout:(SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : 120*NSEC_PER_SEC)];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error != nil) {
        secerror("octagon: failed to commit record changes error:%@", op.error);
        if(error){
            *error = op.error;
        }
        return NO;
    }
    secnotice("octagon", "successfully uploaded record: %@", bprecord.recordName);
    return result;
}

-(BOOL) removeBottledPeerRecordID:(CKRecordID*)recordID error:(NSError**)error
{
    secnotice("octagon", "removing bottled peer from cloudkit");
    BOOL result = YES;
    
    NSMutableArray<CKRecordID*>* recordIDsToRemove = [[NSMutableArray alloc] init];
    [recordIDsToRemove addObject:recordID];
    
    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-modify-changes" withBlock:^{}];

    secnotice("octagon", "Beginning CloudKit ModifyRecords");
    [op addSuccessDependency: [self modifyRecords:[NSMutableArray array] deleteRecordIDs:recordIDsToRemove]];

    [op timeout:(SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : 120*NSEC_PER_SEC)];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error != nil) {
        secerror("octagon: ailed to commit record changes error:%@", op.error);
        if(error){
            *error = op.error;
        }
        return NO;
    }

    return result;
}

- (void)_onqueueHandleCKLogin {
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckks", self, "Skipping CloudKit initialization due to disabled CKKS");
        return;
    }

    dispatch_assert_queue(self.queue);

    __weak __typeof(self) weakSelf = self;

    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: self.zoneName];
    [self handleCKLogin:ckse.ckzonecreated zoneSubscribed:ckse.ckzonesubscribed];

    self.viewSetupOperation = [CKKSResultOperation operationWithBlock: ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return;
        }

        __block bool quit = false;

        [strongSelf dispatchSync: ^bool {
            ckksnotice("octagon", strongSelf, "Zone setup progress: %@ %d %@ %d %@",
                       [CKKSCKAccountStateTracker stringFromAccountStatus:strongSelf.accountStatus],
                       strongSelf.zoneCreated, strongSelf.zoneCreatedError, strongSelf.zoneSubscribed, strongSelf.zoneSubscribedError);

            NSError* error = nil;
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: strongSelf.zoneName];
            ckse.ckzonecreated = strongSelf.zoneCreated;
            ckse.ckzonesubscribed = strongSelf.zoneSubscribed;

            // Although, if the zone subscribed error says there's no zone, mark down that there's no zone
            if(strongSelf.zoneSubscribedError &&
               [strongSelf.zoneSubscribedError.domain isEqualToString:CKErrorDomain] && strongSelf.zoneSubscribedError.code == CKErrorPartialFailure) {
                NSError* subscriptionError = strongSelf.zoneSubscribedError.userInfo[CKPartialErrorsByItemIDKey][strongSelf.zoneID];
                if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorZoneNotFound) {

                    ckkserror("octagon", strongSelf, "zone subscription error appears to say the zone doesn't exist, fixing status: %@", strongSelf.zoneSubscribedError);
                    ckse.ckzonecreated = false;
                }
            }

            [ckse saveToDatabase: &error];
            if(error) {
                ckkserror("octagon", strongSelf, "couldn't save zone creation status for %@: %@", strongSelf.zoneName, error);
            }

            if(!strongSelf.zoneCreated || !strongSelf.zoneSubscribed || strongSelf.accountStatus != CKAccountStatusAvailable) {
                // Something has gone very wrong. Error out and maybe retry.
                quit = true;

                // Note that CKKSZone has probably called [handleLogout]; which means we have a key hierarchy reset queued up. Error here anyway.
                NSError* realReason = strongSelf.zoneCreatedError ? strongSelf.zoneCreatedError : strongSelf.zoneSubscribedError;
                strongSelf.viewSetupOperation.error = realReason;


                return true;
            }

            return true;
        }];

        if(quit) {
            ckkserror("octagon", strongSelf, "Quitting setup.");
            return;
        }
    }];
    self.viewSetupOperation.name = @"zone-setup";

    [self.viewSetupOperation addNullableDependency: self.zoneSetupOperation];
    [self scheduleAccountStatusOperation: self.viewSetupOperation];
}

- (void)handleCKLogin
{
    ckksinfo("octagon", self, "received a notification of CK login");

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* login = [CKKSResultOperation named:@"octagon-login" withBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;

        [strongSelf dispatchSync:^bool{
            strongSelf.accountStatus = CKKSAccountStatusAvailable;
            [strongSelf _onqueueHandleCKLogin];
            return true;
        }];
    }];

    [self scheduleAccountStatusOperation:login];
}

- (bool)_onqueueResetLocalData: (NSError * __autoreleasing *) error {
    dispatch_assert_queue(self.queue);
    
    NSError* localerror = nil;
    bool setError = false;
    
    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: self.zoneName];
    ckse.ckzonecreated = false;
    ckse.ckzonesubscribed = false;
    ckse.changeToken = NULL;
    [ckse saveToDatabase: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't reset zone status for %@: %@", self.zoneName, localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    BOOL result = [_localStore removeAllBottledPeerRecords:&localerror];
    if(!result){
        *error = localerror;
        secerror("octagon: failed to move all bottled peer entries for context: %@ error: %@", self.contextID, localerror);
    }
    return (localerror == nil && !setError);
}

-(CKKSResultOperation*) resetOctagonTrustZone:(NSError**)error
{
    // On a reset, we should cancel all existing operations
    [self cancelAllOperations];
    CKKSResultOperation* reset = [super deleteCloudKitZoneOperation:nil];
    [self scheduleOperationWithoutDependencies:reset];
    
    __weak __typeof(self) weakSelf = self;
    CKKSGroupOperation* resetFollowUp = [[CKKSGroupOperation alloc] init];
    resetFollowUp.name = @"cloudkit-reset-follow-up-group";
    
    [resetFollowUp runBeforeGroupFinished: [CKKSResultOperation named:@"cloudkit-reset-follow-up" withBlock: ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("octagon", strongSelf, "received callback for released object");
            return;
        }
        
        if(!reset.error) {
            ckksnotice("octagon", strongSelf, "Successfully deleted zone %@", strongSelf.zoneName);
            __block NSError* error = nil;
            
            [strongSelf dispatchSync: ^bool{
                [strongSelf _onqueueResetLocalData: &error];
                return true;
            }];
        } else {
            // Shouldn't ever happen, since reset is a successDependency
            ckkserror("ckks", strongSelf, "Couldn't reset zone %@: %@", strongSelf.zoneName, reset.error);
        }
    }]];
    
    [resetFollowUp addSuccessDependency:reset];
    [self scheduleOperationWithoutDependencies:resetFollowUp];
    
    return reset;
}

-(BOOL) performReset:(NSError**)error
{
    BOOL result = NO;
    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-reset-zones-waiter" withBlock:^{}];
   
    secnotice("octagon", "Beginning CloudKit reset for Octagon Trust");
    [op addSuccessDependency:[self resetOctagonTrustZone:error]];
    
    [op timeout:(SecCKKSTestsEnabled() ? 2*NSEC_PER_SEC : 120*NSEC_PER_SEC)];
    [self.operationQueue addOperation: op];
    
    [op waitUntilFinished];
    if(!op.error) {
        secnotice("octagon", "Completed rpcResetCloudKit");
        __weak __typeof(self) weakSelf = self;
        CKKSResultOperation* login = [CKKSResultOperation named:@"octagon-login" withBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;

            [strongSelf dispatchSync:^bool{
                strongSelf.accountStatus = CKKSAccountStatusAvailable;
                [strongSelf handleCKLogin:false zoneSubscribed:false];
                return true;
            }];
        }];

        [self.operationQueue addOperation:login];
        result = YES;
    } else {
        secnotice("octagon", "Completed rpcResetCloudKit with error: %@", op.error);
        if(error){
            *error = op.error;
        }
    }
    
    return result;
}

@end

NS_ASSUME_NONNULL_END
#endif

