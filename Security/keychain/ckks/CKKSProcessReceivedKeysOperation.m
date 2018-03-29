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

#import <AssertMacros.h>

#import "CKKSKeychainView.h"
#import "CKKSCurrentKeyPointer.h"
#import "CKKSKey.h"
#import "CKKSProcessReceivedKeysOperation.h"
#import "keychain/ckks/CloudKitCategories.h"

#if OCTAGON

@implementation CKKSProcessReceivedKeysOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks {
    if(self = [super init]) {
        _ckks = ckks;
    }
    return self;
}

- (void) main {
    // Synchronous, on some thread. Get back on the CKKS queue for SQL thread-safety.

    // Take a strong reference.
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        secerror("ckkskeys: No CKKS object");
        return;
    }

    if(self.cancelled) {
        ckksinfo("ckkskey", ckks, "CKKSProcessReceivedKeysOperation cancelled, quitting");
        return;
    }

    [ckks dispatchSyncWithAccountKeys: ^bool{
        if(self.cancelled) {
            ckksinfo("ckkskey", ckks, "CKKSProcessReceivedKeysOperation cancelled, quitting");
            return false;
        }

        ckks.lastProcessReceivedKeysOperation = self;

        NSError* error = nil;
        CKKSKey* tlk = nil;
        CKKSKey* topKey = nil;

        // The synckeys table contains everything that's in CloudKit, if looked at correctly.
        // Updates from CloudKit are marked 'remote'; everything else is 'local'.

        // Step 1. Find all remote keys.
        NSArray<CKKSKey*>* remoteKeys = [CKKSKey remoteKeys:ckks.zoneID error:&error];
        if(!remoteKeys) {
            ckkserror("ckkskey", ckks, "couldn't fetch list of remote keys: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:error];
            return false;
        }

        if([remoteKeys count] == 0u) {
            ckksnotice("ckkskey", ckks, "No remote keys? Quitting.");
            // Not a ready state, more of a quizzical one? The key state machine will know what to do.
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateReady withError:error];
            return false;
        }

        ckksinfo("ckkskey", ckks, "remote keys: %@", remoteKeys);

        // current TLK record:
        CKKSCurrentKeyPointer* currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:ckks.zoneID error:&error];
        CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:ckks.zoneID error:&error];
        CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:ckks.zoneID error:&error];

        // Do these pointers point at anything?
        NSError* localerror = nil;
        CKKSKey* suggestedTLK    = currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabaseAnyState:currentTLKPointer.currentKeyUUID    zoneID:ckks.zoneID error:&localerror] : nil;
        CKKSKey* suggestedClassA = currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabaseAnyState:currentClassAPointer.currentKeyUUID zoneID:ckks.zoneID error:&localerror] : nil;
        CKKSKey* suggestedClassC = currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabaseAnyState:currentClassCPointer.currentKeyUUID zoneID:ckks.zoneID error:&localerror] : nil;

        if(!currentTLKPointer || !currentClassAPointer || !currentClassCPointer ||
           !currentTLKPointer.currentKeyUUID || !currentClassAPointer.currentKeyUUID || !currentClassCPointer.currentKeyUUID ||
           !suggestedTLK || !suggestedClassA || !suggestedClassC) {
            ckkserror("ckkskey", ckks, "no current pointer for some keyclass: tlk:%@ a:%@ c:%@ %@ %@",
                     currentTLKPointer, currentClassAPointer, currentClassCPointer, error, localerror);
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateBadCurrentPointers withError:error];
            return true;
        }

        for(CKKSKey* key in remoteKeys) {
            // Find the active TLK.
            if([key.uuid isEqualToString: currentTLKPointer.currentKeyUUID]) {
                if([key wrapsSelf]) {
                    tlk = key;
                } else {
                    ckkserror("ckkskey", ckks, "current TLK doesn't wrap itself: %@ %@", key, key.parentKeyUUID);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateUnhealthy withError:error];
                    return true;
                }
            }
        }

        if(!tlk) {
            ckkserror("ckkskey", ckks, "couldn't find active TLK: %@", currentTLKPointer);
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateUnhealthy withError:error];
            return true;
        }

        // This key is our proposed TLK. Check with the CKKS object.
        if(![ckks _onqueueWithAccountKeysCheckTLK: tlk error: &error]) {
            // Was this error "I've never seen that TLK before in my life"? If so, enter the "wait for TLK sync" state.
            if(error && [error.domain isEqualToString: @"securityd"] && error.code == errSecItemNotFound) {
                ckksnotice("ckkskey", ckks, "Received a TLK which we don't have in the local keychain(%@). Entering waitfortlk.", tlk);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForTLK withError:nil];
                return true;
            } else if(error && [ckks.lockStateTracker isLockedError:error]) {
                // TODO: _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError should handle this. But, we don't have tests, so, leave this in until 33204154
                ckksnotice("ckkskey", ckks, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", tlk);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:nil];
                return true;
            } else {
                // Otherwise, something has gone horribly wrong. enter error state.
                ckkserror("ckkskey", ckks, "CKKS claims %@ is not a valid TLK: %@", tlk, error);
                NSError* newError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"invalid TLK from CloudKit" underlying:error];
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:newError];
                return true;
            }
        }

        // Ensure that new keys wrap to the TLK.
        for(CKKSKey* key in remoteKeys) {
            if(key == tlk) {
                continue;
            }

            topKey = [key topKeyInAnyState:&error];

            if(error != nil || ![topKey.uuid isEqual: tlk.uuid]) {
                ckkserror("ckkskey", ckks, "new key %@ is orphaned (%@)", key, error);
                // TODO: possibly re-fetch. Maybe not an actual error state.
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError
                                                  withError:[NSError errorWithDomain:CKKSErrorDomain
                                                                                code:CKKSOrphanedKey
                                                                         description:[NSString stringWithFormat:@"orphaned key(%@) in hierarchy", topKey]
                                                                          underlying:error]];
                return true;

            }

            // Okay, it wraps to the TLK. Can we unwrap it?
            if(![key unwrapViaKeyHierarchy:&error] || error != nil) {
                if(error && [ckks.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckkskey", ckks, "Couldn't unwrap new key (%@), but keybag appears to be locked. Entering waitforunlock.", key);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:error];
                    return true;
                } else {
                    ckkserror("ckkskey", ckks, "new key %@ claims to wrap to TLK, but we can't unwrap it: %@", topKey, error);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError
                                                      withError:[NSError errorWithDomain:CKKSErrorDomain
                                                                                    code:CKKSOrphanedKey
                                                                             description:[NSString stringWithFormat:@"unwrappable key(%@) in hierarchy: %@", topKey, error]
                                                                              underlying:error]];
                    return true;
                }
            }

            ckksnotice("ckkskey", ckks, "New key %@ wraps to tlk %@", key, tlk);
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
                ckkserror("ckkskey", ckks, "couldn't save newly local key %@ to database: %@", key, error);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError: error];
                return false;
            }
        }

        if(!error) {
            ckksnotice("ckkskey", ckks, "Accepted new key hierarchy");
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateReady withError: nil];
        } else {
            ckkserror("ckkskey", ckks, "error accepting new key hierarchy: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
        }
        return true;
    }];
}

@end;

#endif
