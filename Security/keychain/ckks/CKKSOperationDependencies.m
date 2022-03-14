
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSOperationDependencies.h"

#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSManifestLeafRecord.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"

@interface CKKSOperationDependencies ()
@property (nullable) NSSet<CKKSKeychainViewState*>* viewsOverride;

// Make writable
@property NSSet<CKKSKeychainViewState*>* allViews;
@property NSSet<CKKSKeychainViewState*>* allPriorityViews;
@property (nullable) TPSyncingPolicy* syncingPolicy;

@property BOOL limitOperationToPriorityViewsSet;

@end

@implementation CKKSOperationDependencies

- (instancetype)initWithViewStates:(NSSet<CKKSKeychainViewState*>*)viewStates
                      zoneModifier:(CKKSZoneModifier*)zoneModifier
                        ckdatabase:(CKDatabase*)ckdatabase
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
                  ckoperationGroup:(CKOperationGroup* _Nullable)operationGroup
                       flagHandler:(id<OctagonStateFlagHandler>)flagHandler
               accountStateTracker:(CKKSAccountStateTracker*)accountStateTracker
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
               reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                     peerProviders:(NSArray<id<CKKSPeerProvider>>*)peerProviders
                  databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
                  savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
{
    if((self = [super init])) {
        _allViews = viewStates;

        _zoneModifier = zoneModifier;
        _ckdatabase = ckdatabase;
        _cloudKitClassDependencies = cloudKitClassDependencies;
        _ckoperationGroup = operationGroup;
        _flagHandler = flagHandler;
        _accountStateTracker = accountStateTracker;
        _lockStateTracker = lockStateTracker;
        _reachabilityTracker = reachabilityTracker;
        _peerProviders = peerProviders;
        _databaseProvider = databaseProvider;
        _savedTLKNotifier = savedTLKNotifier;

        _currentOutgoingQueueOperationGroup = nil;
        _requestPolicyCheck = nil;

        _keysetProviderOperations = [NSHashTable weakObjectsHashTable];
        _currentFetchReasons = [NSMutableSet set];

        _limitOperationToPriorityViewsSet = NO;
    }
    return self;
}

- (NSSet<CKKSKeychainViewState*>*)views
{
    if(self.viewsOverride != nil) {
        return self.viewsOverride;
    }

    if(self.limitOperationToPriorityViewsSet) {
        return self.allPriorityViews;
    } else {
        return self.allViews;
    }
}

- (NSSet<CKKSKeychainViewState*>*)activeManagedViews
{
    NSMutableSet<CKKSKeychainViewState*>* result = [NSMutableSet set];
    for(CKKSKeychainViewState* vs in self.views) {
        if(vs.ckksManagedView) {
            [result addObject: vs];
        }
    }
    return result;
}

- (NSSet<CKKSKeychainViewState*>*)allCKKSManagedViews
{
    NSMutableSet<CKKSKeychainViewState*>* result = [NSMutableSet set];
    for(CKKSKeychainViewState* vs in self.allViews) {
        if(vs.ckksManagedView) {
            [result addObject: vs];
        }
    }
    return result;
}

- (NSSet<CKKSKeychainViewState*>*)allExternalManagedViews
{
    NSMutableSet<CKKSKeychainViewState*>* result = [NSMutableSet set];
    for(CKKSKeychainViewState* vs in self.allViews) {
        if(!vs.ckksManagedView) {
            [result addObject: vs];
        }
    }
    return result;
}

- (void)setStateForActiveZones:(CKKSZoneKeyState*)newZoneKeyState
{
    for(CKKSKeychainViewState* viewState in self.views) {
        viewState.viewKeyHierarchyState = newZoneKeyState;
    }
}

- (void)setStateForActiveCKKSManagedViews:(CKKSZoneKeyState*)newZoneKeyState
{
    for(CKKSKeychainViewState* viewState in self.views) {
        if(!viewState.ckksManagedView) {
            continue;
        }
        viewState.viewKeyHierarchyState = newZoneKeyState;
    }
}

- (void)setStateForActiveExternallyManagedViews:(CKKSZoneKeyState*)newZoneKeyState
{
    for(CKKSKeychainViewState* viewState in self.views) {
        if(viewState.ckksManagedView) {
            continue;
        }
        viewState.viewKeyHierarchyState = newZoneKeyState;
    }
}

- (void)setStateForAllViews:(CKKSZoneKeyState*)newZoneKeyState
{
    for(CKKSKeychainViewState* viewState in self.allViews) {
        viewState.viewKeyHierarchyState = newZoneKeyState;
    }
}

- (void)operateOnSelectViews:(NSSet<CKKSKeychainViewState*>*)views
{
    if(SecCKKSTestsEnabled()) {
        NSMutableSet<NSString*>* viewNames = [NSMutableSet set];
        for(CKKSKeychainViewState* viewState in views) {
            [viewNames addObject:viewState.zoneName];
        }

        NSMutableSet<NSString*>* allViewNames = [NSMutableSet set];
        for(CKKSKeychainViewState* viewState in self.allViews) {
            [allViewNames addObject:viewState.zoneName];
        }

        NSAssert([viewNames isSubsetOfSet:allViewNames], @"Can only operate on views previously known");
    }
    self.viewsOverride = views;

    ckksnotice_global("ckksviews", "Limited view operation to %@", self.views);
}

- (void)operateOnAllViews
{
    self.viewsOverride = nil;
    self.limitOperationToPriorityViewsSet = NO;

    ckksnotice_global("ckksviews", "Expanded view operation to %@", self.views);
}

- (void)limitOperationToPriorityViews
{
    self.limitOperationToPriorityViewsSet = YES;

    ckksnotice_global("ckksviews", "Limited view operation to priority views %@", self.views);
}

- (NSSet<CKKSKeychainViewState*>*)viewsInState:(CKKSZoneKeyState*)state
{
    NSMutableSet<CKKSKeychainViewState*>* set = [NSMutableSet set];

    for(CKKSKeychainViewState* viewState in self.views) {
        if([viewState.viewKeyHierarchyState isEqualToString:state]) {
            [set addObject:viewState];
        }
    }

    return set;
}

- (NSSet<CKKSKeychainViewState*>*)viewStatesByNames:(NSSet<NSString*>*)names
{
    NSMutableSet<CKKSKeychainViewState*>* set = [NSMutableSet set];

    for(CKKSKeychainViewState* viewState in self.views) {
        if([names containsObject:viewState.zoneID.zoneName]) {
            [set addObject:viewState];
        }
    }

    return set;
}

- (CKKSKeychainViewState* _Nullable)viewStateForName:(NSString*)name
{
    for(CKKSKeychainViewState* viewState in self.allViews) {
        if([viewState.zoneID.zoneName isEqualToString:name]) {
            return viewState;
        }
    }

    return nil;
}

- (NSSet<CKKSKeychainViewState*>*)readyAndSyncingViews
{
    NSMutableSet<CKKSKeychainViewState*>* set = [NSMutableSet set];

    for(CKKSKeychainViewState* viewState in self.views) {
        if(viewState.ckksManagedView &&
           [self.syncingPolicy isSyncingEnabledForView:viewState.zoneID.zoneName] &&
           [viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady]) {
            [set addObject:viewState];
        }
    }

    return set;
}

- (void)applyNewSyncingPolicy:(TPSyncingPolicy*)policy
                   viewStates:(NSSet<CKKSKeychainViewState*>*)viewStates
{
    self.syncingPolicy = policy;

    NSMutableSet<CKKSKeychainViewState*>* priorityViews = [NSMutableSet set];
    for(CKKSKeychainViewState* viewState in viewStates) {
        if([policy.priorityViews containsObject:viewState.zoneName]) {
            [priorityViews addObject:viewState];
        }
    }

    self.allPriorityViews = priorityViews;
    self.allViews = viewStates;

    self.viewsOverride = nil;
}

- (NSArray<CKKSPeerProviderState*>*)currentTrustStates
{
    NSArray<id<CKKSPeerProvider>>* peerProviders = self.peerProviders;
    NSMutableArray<CKKSPeerProviderState*>* trustStates = [NSMutableArray array];

#if DEBUG
    NSAssert(![self.databaseProvider insideSQLTransaction], @"Cannot fetch current trust states from inside a SQL transaction, on pain of deadlocK");
#endif

    for(id<CKKSPeerProvider> provider in peerProviders) {
        ckksnotice_global("ckks", "Fetching account keys for provider %@", provider);
        [trustStates addObject:provider.currentState];
    }

    return trustStates;
}

- (BOOL)considerSelfTrusted:(NSArray<CKKSPeerProviderState*>*)currentTrustStates error:(NSError**)error
{
    NSError* possibleTrustStateError = nil;

    // Are the essential trust systems actually telling us everything is broken?
    for(CKKSPeerProviderState* providerState in currentTrustStates) {
        if(providerState.essential) {
            NSError* trustStateError = providerState.currentSelfPeersError ?: providerState.currentTrustedPeersError;

            if(providerState.essential && providerState.currentSelfPeersError == nil && providerState.currentTrustedPeersError == nil) {
                return YES;
            } else {
                possibleTrustStateError = trustStateError;
            }
        }
    }

    if(error && possibleTrustStateError) {
        *error = possibleTrustStateError;
    }

    return NO;
}

- (void)provideKeySets:(NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>*)keysets
{
    for(CKRecordZoneID* zoneID in [keysets allKeys]) {
        CKKSCurrentKeySet* keyset = keysets[zoneID];
        ckksnotice("ckkskey", zoneID, "Providing keyset (%@) to listeners", keyset);
    }

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in self.keysetProviderOperations) {
        [op provideKeySets:keysets];
    }
}

- (bool)intransactionCKRecordChanged:(CKRecord*)record resync:(bool)resync
{
    @autoreleasepool {
        ckksnotice("ckksfetch", record.recordID.zoneID, "Processing record modification(%@): %@", record.recordType, record);

        NSError* localerror = nil;

        if([[record recordType] isEqual: SecCKRecordItemType]) {
            [CKKSItem intransactionRecordChanged:record resync:resync error:&localerror];

        } else if([[record recordType] isEqual: SecCKRecordCurrentItemType]) {
            [CKKSCurrentItemPointer intransactionRecordChanged:record resync:resync error:&localerror];

        } else if([[record recordType] isEqual: SecCKRecordIntermediateKeyType]) {
            [CKKSKey intransactionRecordChanged:record resync:resync flagHandler:self.flagHandler error:&localerror];

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
            ckkserror("ckksfetch", record.recordID.zoneID, "unknown record type: %@ %@", [record recordType], record);
            return false;
        }

        if(localerror) {
            ckksnotice("ckksfetch", record.recordID.zoneID, "Record modification(%@) failed:: %@", record.recordType, localerror);
            return false;
        }
        return true;
    }
}

- (bool)intransactionCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync
{
    // TODO: resync doesn't really mean much here; what does it mean for a record to be 'deleted' if you're fetching from scratch?
    ckksnotice("ckksfetch", recordID.zoneID, "Processing record deletion(%@): %@", recordType, recordID.recordName);

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
        ckkserror("ckksfetch", recordID.zoneID, "unknown record type: %@ %@", recordType, recordID);
        return false;
    }

    if(error) {
        ckksnotice("ckksfetch", recordID.zoneID, "Record deletion(%@) failed:: %@", recordID, error);
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
            ckksnotice_global("ckks", "Received a ServerRecordChanged error, attempting to update new records and delete unknown ones");

            bool updatedRecord = false;

            for(CKRecordID* recordID in partialErrors.allKeys) {
                NSError* error = partialErrors[recordID];
                if([error.domain isEqual:CKErrorDomain] && error.code == CKErrorServerRecordChanged) {
                    CKRecord* newRecord = error.userInfo[CKRecordChangedErrorServerRecordKey];
                    ckksnotice("ckks", recordID.zoneID, "On error: updating our idea of: %@", newRecord);

                    updatedRecord |= [self intransactionCKRecordChanged:newRecord resync:true];
                } else if([error.domain isEqual:CKErrorDomain] && error.code == CKErrorUnknownItem) {
                    CKRecord* record = savedRecords[recordID];
                    ckksnotice("ckks", recordID.zoneID, "On error: handling an unexpected delete of: %@ %@", recordID, record);

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
            ckksnotice("ckks", recordID.zoneID, "Examining 'write failed' error: %@ %@ %@", error, underlyingError, thirdLevelError);

            if([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorServerRejectedRequest &&
               underlyingError && [underlyingError.domain isEqualToString:CKInternalErrorDomain] && underlyingError.code == CKErrorInternalPluginError &&
               thirdLevelError && [thirdLevelError.domain isEqualToString:@"CloudkitKeychainService"]) {

                if(thirdLevelError.code == CKKSServerUnexpectedSyncKeyInChain) {
                    // The server thinks the classA/C synckeys don't wrap directly the to top TLK, but we don't (otherwise, we would have fixed it).
                    // Issue a key hierarchy fetch and see what's what.
                    ckkserror("ckks", recordID.zoneID, "CKKS Server extension has told us about %@ for record %@; requesting refetch and reprocess of key hierarchy", thirdLevelError, recordID);
                    [self.currentFetchReasons addObject:CKKSFetchBecauseKeyHierarchy];
                    [self.currentFetchReasons addObject:CKKSFetchBecauseResolvingConflict];
                    [self.flagHandler _onqueueHandleFlag:CKKSFlagFetchRequested];

                } else if(thirdLevelError.code == CKKSServerMissingRecord) {
                    // The server is concerned that there's a missing record somewhere.
                    // Issue a key hierarchy fetch and see what's happening
                    ckkserror("ckks", recordID.zoneID, "CKKS Server extension has told us about %@ for record %@; requesting refetch and reprocess of key hierarchy", thirdLevelError, recordID);
                    [self.currentFetchReasons addObject:CKKSFetchBecauseKeyHierarchy];
                    [self.currentFetchReasons addObject:CKKSFetchBecauseResolvingConflict];
                    [self.flagHandler _onqueueHandleFlag:CKKSFlagFetchRequested];

                } else {
                    ckkserror("ckks", recordID.zoneID, "CKKS Server extension has told us about %@ for record %@, but we don't currently handle this error", thirdLevelError, recordID);
                }
            }
        }
    }

    return false;
}

- (NSString* _Nullable)viewNameForItem:(SecDbItemRef)item
{
    CFErrorRef cferror = NULL;
    NSMutableDictionary *dict = (__bridge_transfer NSMutableDictionary*)SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &cferror);

    if(cferror) {
        ckkserror_global("ckks", "Couldn't fetch attributes from item: %@", cferror);
        CFReleaseNull(cferror);
        return nil;
    }

    // Ensure that we've added the class name, because SecDbItemCopyPListWithMask doesn't do that for some reason.
    dict[(__bridge NSString*)kSecClass] = (__bridge NSString*)item->class->name;

    NSString* view = [self.syncingPolicy mapDictionaryToView:dict];
    if (view == nil) {
        ckkserror_global("ckks", "No view returned from policy (%@): %@", self.syncingPolicy, item);
        return nil;
    }

    return view;
}

@end

#endif // OCTAGON
