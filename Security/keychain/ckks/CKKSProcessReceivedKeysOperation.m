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

#if OCTAGON

#import <AssertMacros.h>

#import "CKKSKeychainView.h"
#import "CKKSCurrentKeyPointer.h"
#import "CKKSKey.h"
#import "CKKSProcessReceivedKeysOperation.h"
#import "keychain/ckks/CKKSOperationDependencies.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@implementation CKKSProcessReceivedKeysOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)main {
    NSArray<CKKSPeerProviderState*>* currentTrustStates = self.deps.currentTrustStates;

    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult {
        bool ok = [self _onqueueMain:currentTrustStates];

        return ok ? CKKSDatabaseTransactionCommit : CKKSDatabaseTransactionRollback;
    }];
}

- (bool)_onqueueMain:(NSArray<CKKSPeerProviderState*>*)currentTrustStates
{
    NSError* error = nil;
    CKKSKey* tlk = nil;
    CKKSKey* topKey = nil;

    // The synckeys table contains everything that's in CloudKit, if looked at correctly.
    // Updates from CloudKit are marked 'remote'; everything else is 'local'.

    // Step 1. Find all remote keys.
    NSArray<CKKSKey*>* remoteKeys = [CKKSKey remoteKeys:self.deps.zoneID error:&error];
    if(!remoteKeys) {
        ckkserror("ckkskey", self.deps.zoneID, "couldn't fetch list of remote keys: %@", error);
        self.error = error;
        self.nextState = SecCKKSZoneKeyStateError;
        return false;
    }

    if([remoteKeys count] == 0u) {
        ckksnotice("ckkskey", self.deps.zoneID, "No remote keys? Quitting.");
        // Not a ready state, more of a quizzical one? The key state machine will know what to do.
        self.error = error;
        self.nextState = SecCKKSZoneKeyStateBecomeReady;
        return false;
    }

    ckksinfo("ckkskey", self.deps.zoneID, "remote keys: %@", remoteKeys);

    // current TLK record:
    CKKSCurrentKeyPointer* currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:self.deps.zoneID error:&error];
    CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:self.deps.zoneID error:&error];
    CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:self.deps.zoneID error:&error];

    // Do these pointers point at anything?
    NSError* localerror = nil;
    CKKSKey* suggestedTLK    = currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabaseAnyState:currentTLKPointer.currentKeyUUID    zoneID:self.deps.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassA = currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabaseAnyState:currentClassAPointer.currentKeyUUID zoneID:self.deps.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassC = currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabaseAnyState:currentClassCPointer.currentKeyUUID zoneID:self.deps.zoneID error:&localerror] : nil;

    if(!currentTLKPointer || !currentClassAPointer || !currentClassCPointer ||
       !currentTLKPointer.currentKeyUUID || !currentClassAPointer.currentKeyUUID || !currentClassCPointer.currentKeyUUID ||
       !suggestedTLK || !suggestedClassA || !suggestedClassC) {
        ckkserror("ckkskey", self.deps.zoneID, "no current pointer for some keyclass: tlk:%@ a:%@ c:%@ %@ %@",
                 currentTLKPointer, currentClassAPointer, currentClassCPointer, error, localerror);
        self.error = error;
        self.nextState = SecCKKSZoneKeyStateBadCurrentPointers;
        return true;
    }

    for(CKKSKey* key in remoteKeys) {
        // Find the active TLK.
        if([key.uuid isEqualToString: currentTLKPointer.currentKeyUUID]) {
            if([key wrapsSelf]) {
                tlk = key;
            } else {
                NSError *newError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSKeyNotSelfWrapped description:[NSString stringWithFormat: @"current TLK doesn't wrap itself: %@ %@", key, key.parentKeyUUID] underlying:error];
                ckkserror("ckkskey", self.deps.zoneID, "%@", error);
                self.error = newError;
                self.nextState = SecCKKSZoneKeyStateUnhealthy;
                return true;
            }
        }
    }

    if(!tlk) {
        ckkserror("ckkskey", self.deps.zoneID, "couldn't find active TLK: %@", currentTLKPointer);
        self.error = error;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return true;
    }

    if(![tlk validTLK:&error]) {
        // Something has gone horribly wrong. Enter error state.
        ckkserror("ckkskey", self.deps.zoneID, "CKKS claims %@ is not a valid TLK: %@", tlk, error);
        self.error  = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"invalid TLK from CloudKit" underlying:error];
        self.nextState = SecCKKSZoneKeyStateError;
        return true;
    }

    // This key is our proposed TLK.
    if(![tlk tlkMaterialPresentOrRecoverableViaTLKShare:currentTrustStates
                                                  error:&error]) {
        // TLK is valid, but not present locally
        if(error && [self.deps.lockStateTracker isLockedError:error]) {
            ckksnotice("ckkskey", self.deps.zoneID, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", tlk);
            self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
        } else {
            ckksnotice("ckkskey", self.deps.zoneID, "Received a TLK(%@) which we don't have in the local keychain: %@", tlk, error);
            self.error = error;
            self.nextState = SecCKKSZoneKeyStateTLKMissing;
        }
        return true;
    }

    // Ensure that new keys wrap to the TLK.
    for(CKKSKey* key in remoteKeys) {
        if(key == tlk) {
            continue;
        }

        topKey = [key topKeyInAnyState:&error];

        if(error != nil || ![topKey.uuid isEqual: tlk.uuid]) {
            ckkserror("ckkskey", self.deps.zoneID, "new key %@ is orphaned (%@)", key, error);
            // TODO: possibly re-fetch. Maybe not an actual error state.
            self.error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSOrphanedKey
                                      description:[NSString stringWithFormat:@"orphaned key(%@) in hierarchy", topKey]
                                       underlying:error];
            self.nextState = SecCKKSZoneKeyStateError;
            return true;

        }

        // Okay, it wraps to the TLK. Can we unwrap it?
        if(![key unwrapViaKeyHierarchy:&error] || error != nil) {
            if(error && [self.deps.lockStateTracker isLockedError:error]) {
                ckksnotice("ckkskey", self.deps.zoneID, "Couldn't unwrap new key (%@), but keybag appears to be locked. Entering waitforunlock.", key);
                self.error = error;
                self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
                return true;
            } else {
                ckkserror("ckkskey", self.deps.zoneID, "new key %@ claims to wrap to TLK, but we can't unwrap it: %@", topKey, error);
                self.error = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSOrphanedKey
                                          description:[NSString stringWithFormat:@"unwrappable key(%@) in hierarchy: %@", topKey, error]
                                           underlying:error];
                self.nextState = SecCKKSZoneKeyStateError;
                return true;
            }
        }

        ckksnotice("ckkskey", self.deps.zoneID, "New key %@ wraps to tlk %@", key, tlk);
    }


    // We're happy with this key hierarchy. Save it.
    for(CKKSKey* key in remoteKeys) {
        key.state = SecCKKSProcessedStateLocal;

        if([key.uuid isEqualToString: currentClassAPointer.currentKeyUUID] ||
           [key.uuid isEqualToString: currentClassCPointer.currentKeyUUID]) {
            [key saveToDatabaseAsOnlyCurrentKeyForClassAndState: &error];
        } else {
            [key saveToDatabase: &error];
        }

        [key saveKeyMaterialToKeychain: &error];


        if(error) {
            if([self.deps.lockStateTracker isLockedError:error]) {
                ckksnotice("ckkskey", self.deps.zoneID, "Couldn't save newly local key %@ keychain, due to lock state. Entering a waiting state; %@", key, error);
                self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
            } else {
                ckkserror("ckkskey", self.deps.zoneID, "couldn't save newly local key %@ to database: %@", key, error);
                self.nextState = SecCKKSZoneKeyStateError;
            }
            self.error = error;
            return false;
        }
    }

    // New key hierarchy? Get it backed up!
    // TLKs are now saved in the local keychain; fire off a backup
    CKKSNearFutureScheduler* tlkNotifier = self.deps.savedTLKNotifier;
    ckksnotice("ckkstlk", self.deps.zoneID, "triggering new TLK notification: %@", tlkNotifier);
    [tlkNotifier trigger];

    if(!error) {
        ckksnotice("ckkskey", self.deps.zoneID, "Accepted new key hierarchy");
        self.nextState = self.intendedState;
    } else {
        ckkserror("ckkskey", self.deps.zoneID, "error accepting new key hierarchy: %@", error);
        self.error = error;
        self.nextState = SecCKKSZoneKeyStateError;
    }
    return true;
}

@end;

#endif
