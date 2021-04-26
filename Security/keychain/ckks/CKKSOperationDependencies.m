
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSOperationDependencies.h"

#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSManifestLeafRecord.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"

@implementation CKKSOperationDependencies

- (instancetype)initWithViewState:(CKKSKeychainViewState*)viewState
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
                       ckdatabase:(CKDatabase*)ckdatabase
                 ckoperationGroup:(CKOperationGroup* _Nullable)operationGroup
                      flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                   launchSequence:(CKKSLaunchSequence*)launchSequence
              accountStateTracker:(CKKSAccountStateTracker*)accountStateTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                    peerProviders:(NSArray<id<CKKSPeerProvider>>*)peerProviders
                 databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
       notifyViewChangedScheduler:(CKKSNearFutureScheduler*)notifyViewChangedScheduler
                 savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
{
    if((self = [super init])) {
        _zones = [NSSet setWithObject:viewState];
        _zoneID = viewState.zoneID;

        _zoneModifier = zoneModifier;
        _ckdatabase = ckdatabase;
        _ckoperationGroup = operationGroup;
        _flagHandler = flagHandler;
        _launch = launchSequence;
        _accountStateTracker = accountStateTracker;
        _lockStateTracker = lockStateTracker;
        _reachabilityTracker = reachabilityTracker;
        _peerProviders = peerProviders;
        _databaseProvider = databaseProvider;
        _notifyViewChangedScheduler = notifyViewChangedScheduler;
        _savedTLKNotifier = savedTLKNotifier;

        _currentOutgoingQueueOperationGroup = nil;
        _requestPolicyCheck = nil;

        _keysetProviderOperations = [NSHashTable weakObjectsHashTable];
    }
    return self;
}

- (NSArray<CKKSPeerProviderState*>*)currentTrustStates
{
    NSArray<id<CKKSPeerProvider>>* peerProviders = self.peerProviders;
    NSMutableArray<CKKSPeerProviderState*>* trustStates = [NSMutableArray array];

#if DEBUG
    NSAssert(![self.databaseProvider insideSQLTransaction], @"Cannot fetch current trust states from inside a SQL transaction, on pain of deadlocK");
#endif

    for(id<CKKSPeerProvider> provider in peerProviders) {
        ckksnotice("ckks", self.zoneID, "Fetching account keys for provider %@", provider);
        [trustStates addObject:provider.currentState];
    }

    return trustStates;
}

- (void)provideKeySet:(CKKSCurrentKeySet*)keyset
{
    if(!keyset || !keyset.currentTLKPointer.currentKeyUUID) {
        ckksnotice("ckkskey", self.zoneID, "No valid keyset provided: %@", keyset);
        return;
    }
    ckksnotice("ckkskey", self.zoneID, "Providing keyset (%@) to listeners", keyset);

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in self.keysetProviderOperations) {
        [op provideKeySet:keyset];
    }
}

- (bool)intransactionCKRecordChanged:(CKRecord*)record resync:(bool)resync
{
    @autoreleasepool {
        ckksnotice("ckksfetch", self.zoneID, "Processing record modification(%@): %@", record.recordType, record);

        NSError* localerror = nil;

        if([[record recordType] isEqual: SecCKRecordItemType]) {
            [CKKSItem intransactionRecordChanged:record resync:resync error:&localerror];

        } else if([[record recordType] isEqual: SecCKRecordCurrentItemType]) {
            [CKKSCurrentItemPointer intransactionRecordChanged:record resync:resync error:&localerror];

        } else if([[record recordType] isEqual: SecCKRecordIntermediateKeyType]) {
            [CKKSKey intransactionRecordChanged:record resync:resync flagHandler:self.flagHandler error:&localerror];
            [self.flagHandler _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];

        } else if ([[record recordType] isEqual: SecCKRecordTLKShareType]) {
            [CKKSTLKShareRecord intransactionRecordChanged:record resync:resync error:&localerror];
            [self.flagHandler _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];

        } else if([[record recordType] isEqualToString: SecCKRecordCurrentKeyType]) {
            [CKKSCurrentKeyPointer intransactionRecordChanged:record resync:resync flagHandler:self.flagHandler error:&localerror];

        } else if ([[record recordType] isEqualToString:SecCKRecordManifestType]) {
            [CKKSPendingManifest intransactionRecordChanged:record resync:resync error:&localerror];

        } else if ([[record recordType] isEqualToString:SecCKRecordManifestLeafType]) {
            [CKKSManifestPendingLeafRecord intransactionRecordChanged:record resync:resync error:&localerror];

        } else if ([[record recordType] isEqualToString:SecCKRecordDeviceStateType]) {
            [CKKSDeviceStateEntry intransactionRecordChanged:record resync:resync error:&localerror];

        } else {
            ckkserror("ckksfetch", self.zoneID, "unknown record type: %@ %@", [record recordType], record);
            return false;
        }

        if(localerror) {
            ckksnotice("ckksfetch", self.zoneID, "Record modification(%@) failed:: %@", record.recordType, localerror);
            return false;
        }
        return true;
    }
}

- (bool)intransactionCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync
{
    // TODO: resync doesn't really mean much here; what does it mean for a record to be 'deleted' if you're fetching from scratch?
    ckksnotice("ckksfetch", self.zoneID, "Processing record deletion(%@): %@", recordType, recordID.recordName);

    NSError* error = nil;

    if([recordType isEqual: SecCKRecordItemType]) {
        [CKKSItem intransactionRecordDeleted:recordID resync:resync error:&error];

    } else if([recordType isEqual: SecCKRecordCurrentItemType]) {
        [CKKSCurrentItemPointer intransactionRecordDeleted:recordID resync:resync error:&error];

    } else if([recordType isEqual: SecCKRecordIntermediateKeyType]) {
        // TODO: handle in some interesting way
        return true;

    } else if([recordType isEqual: SecCKRecordTLKShareType]) {
        [CKKSTLKShareRecord intransactionRecordDeleted:recordID resync:resync error:&error];

    } else if([recordType isEqualToString: SecCKRecordCurrentKeyType]) {
        // Ignore these as well
        return true;

    } else if([recordType isEqual: SecCKRecordDeviceStateType]) {
        [CKKSDeviceStateEntry intransactionRecordDeleted:recordID resync:resync error:&error];

    } else if ([recordType isEqualToString:SecCKRecordManifestType]) {
        [CKKSManifest intransactionRecordDeleted:recordID resync:resync error:&error];

    } else {
        ckkserror("ckksfetch", self.zoneID, "unknown record type: %@ %@", recordType, recordID);
        return false;
    }

    if(error) {
        ckksnotice("ckksfetch", self.zoneID, "Record deletion(%@) failed:: %@", recordID, error);
        return false;
    }

    return true;
}

// Lets the view know about a failed CloudKit write. If the error is "already have one of these records", it will
// store the new records and kick off the new processing
//
// Note that you need to tell this function the records you wanted to save, so it can determine what needs deletion
- (bool)intransactionCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords
{
    NSDictionary<CKRecordID*,NSError*>* partialErrors = ckerror.userInfo[CKPartialErrorsByItemIDKey];
    if([ckerror.domain isEqual:CKErrorDomain] && ckerror.code == CKErrorPartialFailure && partialErrors) {
        // Check if this error was "you're out of date"
        bool recordChanged = true;

        for(NSError* error in partialErrors.allValues) {
            if((![error.domain isEqual:CKErrorDomain]) || (error.code != CKErrorBatchRequestFailed && error.code != CKErrorServerRecordChanged && error.code != CKErrorUnknownItem)) {
                // There's an error in there that isn't CKErrorServerRecordChanged, CKErrorBatchRequestFailed, or CKErrorUnknownItem. Don't handle nicely...
                recordChanged = false;
            }
        }

        if(recordChanged) {
            ckksnotice("ckks", self.zoneID, "Received a ServerRecordChanged error, attempting to update new records and delete unknown ones");

            bool updatedRecord = false;

            for(CKRecordID* recordID in partialErrors.allKeys) {
                NSError* error = partialErrors[recordID];
                if([error.domain isEqual:CKErrorDomain] && error.code == CKErrorServerRecordChanged) {
                    CKRecord* newRecord = error.userInfo[CKRecordChangedErrorServerRecordKey];
                    ckksnotice("ckks", self.zoneID, "On error: updating our idea of: %@", newRecord);

                    updatedRecord |= [self intransactionCKRecordChanged:newRecord resync:true];
                } else if([error.domain isEqual:CKErrorDomain] && error.code == CKErrorUnknownItem) {
                    CKRecord* record = savedRecords[recordID];
                    ckksnotice("ckks", self.zoneID, "On error: handling an unexpected delete of: %@ %@", recordID, record);

                    updatedRecord |= [self intransactionCKRecordDeleted:recordID recordType:record.recordType resync:true];
                }
            }

            if(updatedRecord) {
                [self.flagHandler _onqueueHandleFlag:CKKSFlagProcessIncomingQueue];
                return true;
            }
        }

        // Check if this error was the CKKS server extension rejecting the write
        for(CKRecordID* recordID in partialErrors.allKeys) {
            NSError* error = partialErrors[recordID];

            NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];
            NSError* thirdLevelError = underlyingError.userInfo[NSUnderlyingErrorKey];
            ckksnotice("ckks", self.zoneID, "Examining 'write failed' error: %@ %@ %@", error, underlyingError, thirdLevelError);

            if([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorServerRejectedRequest &&
               underlyingError && [underlyingError.domain isEqualToString:CKInternalErrorDomain] && underlyingError.code == CKErrorInternalPluginError &&
               thirdLevelError && [thirdLevelError.domain isEqualToString:@"CloudkitKeychainService"]) {

                if(thirdLevelError.code == CKKSServerUnexpectedSyncKeyInChain) {
                    // The server thinks the classA/C synckeys don't wrap directly the to top TLK, but we don't (otherwise, we would have fixed it).
                    // Issue a key hierarchy fetch and see what's what.
                    ckkserror("ckks", self.zoneID, "CKKS Server extension has told us about %@ for record %@; requesting refetch and reprocess of key hierarchy", thirdLevelError, recordID);
                    [self.flagHandler _onqueueHandleFlag:CKKSFlagFetchRequested];

                } else if(thirdLevelError.code == CKKSServerMissingRecord) {
                    // The server is concerned that there's a missing record somewhere.
                    // Issue a key hierarchy fetch and see what's happening
                    ckkserror("ckks", self.zoneID, "CKKS Server extension has told us about %@ for record %@; requesting refetch and reprocess of key hierarchy", thirdLevelError, recordID);
                    [self.flagHandler _onqueueHandleFlag:CKKSFlagFetchRequested];

                } else {
                    ckkserror("ckks", self.zoneID, "CKKS Server extension has told us about %@ for record %@, but we don't currently handle this error", thirdLevelError, recordID);
                }
            }
        }
    }

    return false;
}

@end

#endif // OCTAGON
