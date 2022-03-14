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
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSOperationDependencies.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@interface CKKSProcessReceivedKeysOperation ()
@property BOOL allowFullRefetchResult;
@end

@implementation CKKSProcessReceivedKeysOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
              allowFullRefetchResult:(BOOL)allowFullRefetchResult
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;
        _allowFullRefetchResult = allowFullRefetchResult;
        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)main {
    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    NSArray<CKKSPeerProviderState*>* currentTrustStates = self.deps.currentTrustStates;

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        __block CKKSZoneKeyState* newZoneState = nil;
        __block NSError* remoteError = nil;

        __block CKKSCurrentKeySet* set = nil;

        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* loadError = nil;

            // The synckeys table contains every key that's in CloudKit, if looked at correctly.
            // Updates from CloudKit are marked 'remote'; everything else is 'local'.
            // If that's not the case (as in, we have some item for which we don't have a synckey), then we need to
            // request a refetch/reset.

            NSArray<CKKSKey*>* remoteKeys = [CKKSKey remoteKeys:viewState.zoneID error:&loadError];
            if(!remoteKeys || loadError) {
                ckkserror("ckkskey", viewState.zoneID, "couldn't fetch list of remote keys: %@", loadError);
                self.error = loadError;
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                return CKKSDatabaseTransactionRollback;
            }

            if(remoteKeys.count > 0) {
                newZoneState = [self processRemoteKeys:remoteKeys
                                             viewState:viewState
                                    currentTrustStates:currentTrustStates
                                                 error:&remoteError];

                ckksnotice("ckkskey", viewState.zoneID, "After remote key processing, Key hierarchy is '%@' (error: %@)", newZoneState, remoteError);

                // If there's any issue processing the remote keys, bail.
                if(![newZoneState isEqualToString:SecCKKSZoneKeyStateReady]) {
                    return CKKSDatabaseTransactionCommit;
                }
            }

            // Check for the existence of a key which we don't have
            NSError* parentKeyUUIDsError = nil;
            BOOL allIQEsHaveKeys = [CKKSIncomingQueueEntry allIQEsHaveValidUnwrappingKeys:viewState.zoneID error:&parentKeyUUIDsError];

            if(parentKeyUUIDsError != nil) {
                ckkserror("ckkskey", viewState.zoneID, "Unable to determine if all IQEs have parent keys: %@", parentKeyUUIDsError);
            } else if(!allIQEsHaveKeys) {
                if(self.allowFullRefetchResult) {
                    ckksnotice("ckkskey", viewState.zoneID, "We have some item that encrypts to a non-existent key. This is exceptional; requesting full refetch");
                    newZoneState = SecCKKSZoneKeyStateNeedFullRefetch;
                } else {
                    ckksnotice("ckkskey", viewState.zoneID, "We have some item that encrypts to a non-existent key, but we cannot request a refetch! Possible inifinite-loop ahead");
                }
            }

            // Now that we have some state, load the existing keyset
            set = [CKKSCurrentKeySet loadForZone:viewState.zoneID];

            if([newZoneState isEqualToString:SecCKKSZoneKeyStateError]) {
                return CKKSDatabaseTransactionRollback;

            } else {
                return CKKSDatabaseTransactionCommit;
            }
        }];

        // If processRemoteKeys didn't return a state, or if it thinks everything is okay, double-check!
        if(newZoneState == nil || [newZoneState isEqualToString:SecCKKSZoneKeyStateReady]) {
            NSError* localProcessingError = nil;
            ckksnotice("ckkskey", viewState.zoneID, "Checking consistency of key hierarchy");
            newZoneState = [self checkExistingKeyHierarchy:set
                                                    zoneID:viewState.zoneID
                                        currentTrustStates:currentTrustStates
                                                     error:&localProcessingError];

            ckksnotice("ckkskey", viewState.zoneID, "Key hierachy is '%@' (error: %@)", newZoneState, localProcessingError);
        }

        viewState.viewKeyHierarchyState = newZoneState;
    }

    self.nextState = self.intendedState;
}

- (CKKSZoneKeyState*)processRemoteKeys:(NSArray<CKKSKey*>*)remoteKeys
                             viewState:(CKKSKeychainViewState*)view
                    currentTrustStates:(NSArray<CKKSPeerProviderState*>*)currentTrustStates
                                 error:(NSError**)outError
{
    NSError* localerror = nil;
    CKKSKey* tlk = nil;
    CKKSKey* topKey = nil;

    ckksinfo("ckkskey", view.zoneID, "remote keys: %@", remoteKeys);

    // current TLK record:
    CKKSCurrentKeyPointer* currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:view.zoneID error:&localerror];
    CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:view.zoneID error:&localerror];
    CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:view.zoneID error:&localerror];

    // Do these pointers point at anything?
    CKKSKey* suggestedTLK    = currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabaseAnyState:currentTLKPointer.currentKeyUUID    zoneID:view.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassA = currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabaseAnyState:currentClassAPointer.currentKeyUUID zoneID:view.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassC = currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabaseAnyState:currentClassCPointer.currentKeyUUID zoneID:view.zoneID error:&localerror] : nil;

    if(!currentTLKPointer || !currentClassAPointer || !currentClassCPointer ||
       !currentTLKPointer.currentKeyUUID || !currentClassAPointer.currentKeyUUID || !currentClassCPointer.currentKeyUUID ||
       !suggestedTLK || !suggestedClassA || !suggestedClassC) {
        ckkserror("ckkskey", view.zoneID, "no current pointer for some keyclass: tlk:%@ a:%@ c:%@ %@ %@",
                  currentTLKPointer, currentClassAPointer, currentClassCPointer, localerror, localerror);

        if(outError) {
            *outError = localerror;
        }

        return SecCKKSZoneKeyStateUnhealthy;
    }

    for(CKKSKey* key in remoteKeys) {
        // Find the active TLK.
        if([key.uuid isEqualToString: currentTLKPointer.currentKeyUUID]) {
            if([key wrapsSelf]) {
                tlk = key;
            } else {
                NSError *newError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSKeyNotSelfWrapped description:[NSString stringWithFormat: @"current TLK doesn't wrap itself: %@ %@", key, key.parentKeyUUID] underlying:localerror];
                ckkserror("ckkskey", view.zoneID, "Proposed TLK doesn't wrap self: %@", newError);

                if(outError) {
                    *outError = newError;
                }

                return SecCKKSZoneKeyStateUnhealthy;
            }
        }
    }

    if(!tlk) {
        ckkserror("ckkskey", view.zoneID, "couldn't find active TLK: %@", currentTLKPointer);
        if(outError) {
            *outError = localerror;
        }

        return SecCKKSZoneKeyStateUnhealthy;
    }

    if(![tlk validTLK:&localerror]) {
        // Something has gone horribly wrong. Enter error state.
        ckkserror("ckkskey", view.zoneID, "CKKS claims %@ is not a valid TLK: %@", tlk, localerror);
        NSError* invalidError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"invalid TLK from CloudKit" underlying:localerror];
        if(outError) {
            *outError = invalidError;
        }
        return SecCKKSZoneKeyStateError;
    }

    // This key is our proposed TLK.
    NSError* tlkRecoveryError = nil;
    if(![tlk tlkMaterialPresentOrRecoverableViaTLKShare:currentTrustStates
                                                  error:&tlkRecoveryError]) {
        // TLK is valid, but not present locally
        if([self.deps.lockStateTracker isLockedError:tlkRecoveryError]) {
            ckksnotice("ckkskey", view.zoneID, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", tlk);
            if(outError) {
                *outError = tlkRecoveryError;
            }
            return SecCKKSZoneKeyStateWaitForUnlock;

        } else {
            ckksnotice("ckkskey", view.zoneID, "Received a TLK(%@) which we don't have in the local keychain: %@", tlk, localerror);

            NSError* trustStateError = nil;
            BOOL haveTrust = [self.deps considerSelfTrusted:currentTrustStates error:&trustStateError];

            if(!haveTrust) {
                ckksnotice("ckkskey", view.zoneID, "Not proceeding due to trust system failure: %@", trustStateError);

                if(outError) {
                    *outError = trustStateError ?: [NSError errorWithDomain:CKKSErrorDomain
                                                                       code:CKKSLackingTrust
                                                                description:@"No trust states available"
                    ];
                }
                return SecCKKSZoneKeyStateWaitForTrust;
            }

            if(outError) {
                *outError = tlkRecoveryError;
            }
            return SecCKKSZoneKeyStateTLKMissing;
        }
    }

    // Ensure that new keys wrap to the TLK.
    for(CKKSKey* key in remoteKeys) {
        if(key == tlk) {
            continue;
        }

        NSError* topKeyError = nil;
        topKey = [key topKeyInAnyState:&topKeyError];

        if(topKeyError != nil || ![topKey.uuid isEqual:tlk.uuid]) {
            ckkserror("ckkskey", view.zoneID, "new key %@ is orphaned (%@)", key, localerror);
            // TODO: possibly re-fetch. Maybe not an actual error state.
            if(outError) {
                *outError = [NSError errorWithDomain:CKKSErrorDomain
                                                code:CKKSOrphanedKey
                                         description:[NSString stringWithFormat:@"orphaned key(%@) in hierarchy", topKey]
                                          underlying:topKeyError];
            }
            return SecCKKSZoneKeyStateError;
        }

        // Okay, it wraps to the TLK. Can we unwrap it?
        NSError* unwrapError = nil;
        if(![key unwrapViaKeyHierarchy:&unwrapError] || unwrapError != nil) {
            if(unwrapError && [self.deps.lockStateTracker isLockedError:unwrapError]) {
                ckksnotice("ckkskey", view.zoneID, "Couldn't unwrap new key (%@), but keybag appears to be locked. Entering waitforunlock.", key);
                if(outError) {
                    *outError = unwrapError;
                }
                return SecCKKSZoneKeyStateWaitForUnlock;

            } else {
                ckkserror("ckkskey", view.zoneID, "new key %@ claims to wrap to TLK, but we can't unwrap it: %@", topKey, unwrapError);
                if(outError) {
                 *outError = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSOrphanedKey
                                          description:[NSString stringWithFormat:@"unwrappable key(%@) in hierarchy: %@", topKey, unwrapError]
                                           underlying:localerror];
                }
                return SecCKKSZoneKeyStateError;
            }
        }

        ckksnotice("ckkskey", view.zoneID, "New key %@ wraps to tlk %@", key, tlk);
    }

    // We're happy with this key hierarchy. Save it.
    for(CKKSKey* key in remoteKeys) {
        key.state = SecCKKSProcessedStateLocal;

        if([key.uuid isEqualToString: currentClassAPointer.currentKeyUUID] ||
           [key.uuid isEqualToString: currentClassCPointer.currentKeyUUID]) {
            [key saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];
        } else {
            [key saveToDatabase: &localerror];
        }

        [key saveKeyMaterialToKeychain: &localerror];

        if(localerror) {
            if([self.deps.lockStateTracker isLockedError:localerror]) {
                ckksnotice("ckkskey", view.zoneID, "Couldn't save newly local key %@ keychain, due to lock state. Entering a waiting state; %@", key, localerror);
                if(outError) {
                    *outError = localerror;
                }
                return SecCKKSZoneKeyStateWaitForUnlock;

            } else {
                ckkserror("ckkskey", view.zoneID, "couldn't save newly local key %@ to database: %@", key, localerror);
                if(outError) {
                    *outError = localerror;
                }
                return SecCKKSZoneKeyStateError;
            }
        }
    }

    // New key hierarchy? Get it backed up!
    // TLKs are now saved in the local keychain; fire off a backup
    CKKSNearFutureScheduler* tlkNotifier = self.deps.savedTLKNotifier;
    ckksnotice("ckkstlk", view.zoneID, "triggering new TLK notification: %@", tlkNotifier);
    [tlkNotifier trigger];

    ckksnotice("ckkskey", view.zoneID, "Accepted new key hierarchy");
    return SecCKKSZoneKeyStateReady;
}

- (CKKSZoneKeyState*)checkExistingKeyHierarchy:(CKKSCurrentKeySet*)set
                                        zoneID:(CKRecordZoneID*)zoneID
                            currentTrustStates:(NSArray<CKKSPeerProviderState*>*)currentTrustStates
                                         error:(NSError**)outError
{
    // Drop off the sql queue: we can do the rest of this function with what we've already loaded

    if(set.error && !([set.error.domain isEqual: @"securityd"] && set.error.code == errSecItemNotFound)) {
        ckkserror("ckkskey", zoneID, "Error examining existing key hierarchy: %@", set.error);
    }

    // "There are no TLKs" takes precedence over "there's something here, but we're untrusted"
    if(!set.currentTLKPointer && !set.currentClassAPointer && !set.currentClassCPointer) {
        ckkserror("ckkskey", zoneID, "No existing key hierarchy (missing all CKPs): %@", set);
        return SecCKKSZoneKeyStateWaitForTLKCreation;
    }

    NSError* trustStateError = nil;
    BOOL haveTrust = [self.deps considerSelfTrusted:currentTrustStates error:&trustStateError];

    // Being locked is not a failure worthy of wait-for-trust here
    if(!haveTrust && trustStateError && [self.deps.lockStateTracker isLockedError:trustStateError]) {
        ckksnotice("ckkskey", zoneID, "Trust system reports device locked: %@", trustStateError);
        haveTrust = YES;
        trustStateError = nil;
    }

    if(!haveTrust) {
        ckksnotice("ckkskey", zoneID, "Not proceeding due to trust system failure: %@", trustStateError);
        if(outError) {
            *outError = trustStateError ?: [NSError errorWithDomain:CKKSErrorDomain
                                                               code:CKKSLackingTrust
                                                        description:@"No trust states available"];
        }
        return SecCKKSZoneKeyStateWaitForTrust;
    }

    // Check keyset
    if(!set.tlk || !set.classA || !set.classC) {
        ckkserror("ckkskey", zoneID, "Error examining existing key hierarchy (missing at least one key): %@", set);

        if(outError) {
            *outError = set.error;
        }

        return SecCKKSZoneKeyStateUnhealthy;
    }

    // keychain being locked is not a fatal error here
    NSError* tlkLoadError = nil;

    if(![set.tlk loadKeyMaterialFromKeychain:&tlkLoadError]) {
        if(tlkLoadError && ![self.deps.lockStateTracker isLockedError:tlkLoadError]) {
            ckkserror("ckkskey", zoneID, "Error loading TLK(%@): %@", set.tlk, tlkLoadError);
            if(outError) {
                *outError = tlkLoadError;
            }
            return SecCKKSZoneKeyStateUnhealthy;

        } else {
            ckkserror("ckkskey", zoneID, "Soft error loading TLK(%@), maybe locked: %@", set.tlk, tlkLoadError);
        }
    }

    // keychain being locked is not a fatal error here
    NSError* classALoadError = nil;
    if(![set.classA loadKeyMaterialFromKeychain:&classALoadError]) {
        if(classALoadError && ![self.deps.lockStateTracker isLockedError:classALoadError]) {
            ckkserror("ckkskey", zoneID, "Error loading classA key(%@): %@", set.classA, classALoadError);
            if(outError) {
                *outError = classALoadError;
            }
            return SecCKKSZoneKeyStateUnhealthy;

        } else {
            ckkserror("ckkskey", zoneID, "Soft error loading classA key(%@), maybe locked: %@", set.classA, classALoadError);
        }
    }

    // keychain being locked is a fatal error here, since this is class C
    NSError* classCLoadError = nil;
    if(![set.classC loadKeyMaterialFromKeychain:&classCLoadError]) {
        ckkserror("ckkskey", zoneID, "Error loading classC(%@): %@", set.classC, classCLoadError);
        if(outError) {
            *outError = classCLoadError;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }

    // Check that the classA and classC keys point to the current TLK
    if(![set.classA.parentKeyUUID isEqualToString:set.tlk.uuid]) {
        NSError* localerror = [NSError errorWithDomain:CKKSServerExtensionErrorDomain
                                                  code:CKKSServerUnexpectedSyncKeyInChain
                                              userInfo:@{
            NSLocalizedDescriptionKey: @"Current class A key does not wrap to current TLK",
        }];
        ckkserror("ckkskey", zoneID, "Key hierarchy unhealthy: %@", localerror);

        if(outError) {
            *outError = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }
    if(![set.classC.parentKeyUUID isEqualToString:set.tlk.uuid]) {
        NSError* localerror = [NSError errorWithDomain:CKKSServerExtensionErrorDomain
                                                  code:CKKSServerUnexpectedSyncKeyInChain
                                              userInfo:@{
            NSLocalizedDescriptionKey: @"Current class C key does not wrap to current TLK",
        }];
        ckkserror("ckkskey", zoneID, "Key hierarchy unhealthy: %@", localerror);
        if(outError) {
            *outError = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }

    return SecCKKSZoneKeyStateReady;
}

@end;

#endif
