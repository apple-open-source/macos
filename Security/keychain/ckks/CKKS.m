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

#include <dispatch/dispatch.h>
#import <Foundation/Foundation.h>
#if OCTAGON
#import <CloudKit/CloudKit.h>
#endif

#include "keychain/securityd/SecItemServer.h"
#include <Security/SecItemPriv.h>

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKey.h"

#import "keychain/ot/OTManager.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

NSDictionary<CKKSZoneKeyState*, NSNumber*>* CKKSZoneKeyStateMap(void) {
    static NSDictionary<CKKSZoneKeyState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
          SecCKKSZoneKeyStateReady:              @0U,
          SecCKKSZoneKeyStateError:              @1U,
          //SecCKKSZoneKeyStateCancelled:          @2U,

          SecCKKSZoneKeyStateInitializing:       @3U,
          SecCKKSZoneKeyStateInitialized:        @4U,
          SecCKKSZoneKeyStateFetchComplete:      @5U,
          SecCKKSZoneKeyStateWaitForTLK:         @6U,
          SecCKKSZoneKeyStateWaitForUnlock:      @7U,
          SecCKKSZoneKeyStateUnhealthy:          @8U,
          SecCKKSZoneKeyStateBadCurrentPointers: @9U,
          SecCKKSZoneKeyStateNewTLKsFailed:      @10U,
          SecCKKSZoneKeyStateNeedFullRefetch:    @11U,
          SecCKKSZoneKeyStateHealTLKShares:      @12U,
          SecCKKSZoneKeyStateHealTLKSharesFailed:@13U,
          SecCKKSZoneKeyStateWaitForFixupOperation:@14U,
          SecCKKSZoneKeyStateReadyPendingUnlock: @15U,
          SecCKKSZoneKeyStateFetch:              @16U,
          SecCKKSZoneKeyStateResettingZone:      @17U,
          SecCKKSZoneKeyStateResettingLocalData: @18U,
          SecCKKSZoneKeyStateLoggedOut:          @19U,
          SecCKKSZoneKeyStateZoneCreationFailed: @20U,
          SecCKKSZoneKeyStateWaitForTrust:       @21U,
          SecCKKSZoneKeyStateWaitForTLKUpload:   @22U,
          SecCKKSZoneKeyStateWaitForTLKCreation: @23U,
          SecCKKSZoneKeyStateProcess:            @24U,
          SecCKKSZoneKeyStateBecomeReady:        @25U,
          SecCKKSZoneKeyStateLoseTrust:          @26U,
          SecCKKSZoneKeyStateTLKMissing:         @27U,
          SecCKKSZoneKeyStateWaitForCloudKitAccountStatus:@28U,
          SecCKKSZoneKeyStateBeginFetch:         @29U,
        };
    });
    return map;
}

NSDictionary<NSNumber*, CKKSZoneKeyState*>* CKKSZoneKeyStateInverseMap(void) {
    static NSDictionary<NSNumber*, CKKSZoneKeyState*>* backwardMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDictionary<CKKSZoneKeyState*, NSNumber*>* forwardMap = CKKSZoneKeyStateMap();
        backwardMap = [NSDictionary dictionaryWithObjects:[forwardMap allKeys] forKeys:[forwardMap allValues]];
    });
    return backwardMap;
}

NSNumber* CKKSZoneKeyToNumber(CKKSZoneKeyState* state) {
    if(!state) {
        return CKKSZoneKeyStateMap()[SecCKKSZoneKeyStateError];
    }
    NSNumber* result = CKKSZoneKeyStateMap()[state];
    if(result) {
        return result;
    }
    return CKKSZoneKeyStateMap()[SecCKKSZoneKeyStateError];
}
CKKSZoneKeyState* CKKSZoneKeyRecover(NSNumber* stateNumber) {
    if(!stateNumber) {
        return SecCKKSZoneKeyStateError;
    }
    CKKSZoneKeyState* result = CKKSZoneKeyStateInverseMap()[stateNumber];
    if(result) {
        return result;
    }
    return SecCKKSZoneKeyStateError;
}

NSSet<CKKSZoneKeyState*>* CKKSKeyStateNonTransientStates()
{
    static NSSet<CKKSZoneKeyState*>* states = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        states = [NSSet setWithArray:@[
             SecCKKSZoneKeyStateReady,
             SecCKKSZoneKeyStateReadyPendingUnlock,
             SecCKKSZoneKeyStateWaitForTrust,
             SecCKKSZoneKeyStateWaitForTLK,
             SecCKKSZoneKeyStateWaitForTLKCreation,
             SecCKKSZoneKeyStateWaitForTLKUpload,
             SecCKKSZoneKeyStateWaitForUnlock,
             SecCKKSZoneKeyStateError,
             SecCKKSZoneKeyStateLoggedOut,
#if OCTAGON
             OctagonStateMachineHalted,
#endif
        ]];
    });
    return states;
}

#if OCTAGON
// If you want CKKS to run in your daemon/tests, you must call SecCKKSEnable before bringing up the keychain db
static bool enableCKKS = false;
static bool testCKKS = false;

bool SecCKKSIsEnabled(void) {
    if([CKDatabase class] == nil) {
        // CloudKit is not linked. We cannot bring CKKS up; disable it with prejudice.
        ckkserror_global("ckks", "CloudKit.framework appears to not be linked. Cannot enable CKKS (on pain of crash).");
        return false;
    }

    return enableCKKS;
}

bool SecCKKSEnable() {
    enableCKKS = true;
    return enableCKKS;
}

bool SecCKKSDisable() {
    enableCKKS = false;
    return enableCKKS;
}

bool SecCKKSResetSyncing(void) {
    // The function name is a bit of a lie, but it does the thing.
    [OTManager resetManager:true to:nil];
    return SecCKKSIsEnabled();
}

bool SecCKKSTestsEnabled(void) {
    return testCKKS;
}

bool SecCKKSTestsEnable(void) {
    if([CKDatabase class] == nil) {
        // CloudKit is not linked. We cannot bring CKKS up; disable it with prejudice.
        ckkserror_global("ckks", "CloudKit.framework appears to not be linked. Cannot enable CKKS testing.");
        testCKKS = false;
        return false;
    }

    testCKKS = true;
    return testCKKS;
}

bool SecCKKSTestsDisable(void) {
    testCKKS = false;
    return testCKKS;
}

// Feature flags to twiddle behavior
static bool CKKSSyncManifests = false;
bool SecCKKSSyncManifests(void) {
    return CKKSSyncManifests;
}
bool SecCKKSEnableSyncManifests() {
    CKKSSyncManifests = true;
    return CKKSSyncManifests;
}
bool SecCKKSSetSyncManifests(bool value) {
    CKKSSyncManifests = value;
    return CKKSSyncManifests;
}

static bool CKKSEnforceManifests = false;
bool SecCKKSEnforceManifests(void) {
    return CKKSEnforceManifests;
}
bool SecCKKSEnableEnforceManifests() {
    CKKSEnforceManifests = true;
    return CKKSEnforceManifests;
}
bool SecCKKSSetEnforceManifests(bool value) {
    CKKSEnforceManifests = value;
    return CKKSEnforceManifests;
}

// defaults write com.apple.security.ckks reduce-rate-limiting YES
static bool CKKSReduceRateLimiting = false;
bool SecCKKSReduceRateLimiting(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Use the default value as above, or apply the preferences value if it exists
        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:SecCKKSUserDefaultsSuite];
        NSString* key = @"reduce-rate-limiting";
        [defaults registerDefaults: @{key: CKKSReduceRateLimiting ? @YES : @NO}];

        CKKSReduceRateLimiting = !![defaults boolForKey:@"reduce-rate-limiting"];
        ckksnotice_global("ratelimit", "reduce-rate-limiting is %@", CKKSReduceRateLimiting ? @"on" : @"off");
    });

    return CKKSReduceRateLimiting;
}

bool SecCKKSSetReduceRateLimiting(bool value) {
    (void) SecCKKSReduceRateLimiting(); // Call this once to read the defaults write
    CKKSReduceRateLimiting = value;
    ckksnotice_global("ratelimit", "reduce-rate-limiting is now %@", CKKSReduceRateLimiting ? @"on" : @"off");
    return CKKSReduceRateLimiting;
}

// Here's a mechanism for CKKS feature flags with default values from NSUserDefaults:
/*static bool CKKSShareTLKs = true;
bool SecCKKSShareTLKs(void) {
    return true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Use the default value as above, or apply the preferences value if it exists
        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:SecCKKSUserDefaultsSuite];
        [defaults registerDefaults: @{@"tlksharing": CKKSShareTLKs ? @YES : @NO}];

        CKKSShareTLKs = !![defaults boolForKey:@"tlksharing"];
        ckksnotice_global("ckksshare", "TLK sharing is %@", CKKSShareTLKs ? @"on" : @"off");
    });

    return CKKSShareTLKs;
}*/

// Feature flags to twiddle behavior for tests
static bool CKKSDisableAutomaticUUID = false;
bool SecCKKSTestDisableAutomaticUUID(void) {
#if DEBUG
    return CKKSDisableAutomaticUUID;
#else
    return false;
#endif
}
void SecCKKSTestSetDisableAutomaticUUID(bool set) {
    CKKSDisableAutomaticUUID = set;
}

static bool CKKSDisableSOS = false;
bool SecCKKSTestDisableSOS(void) {
#if DEBUG
    return CKKSDisableSOS;
#else
    return false;
#endif
}
void SecCKKSTestSetDisableSOS(bool set) {
    CKKSDisableSOS = set;
}


static bool CKKSDisableKeyNotifications = false;
bool SecCKKSTestDisableKeyNotifications(void) {
#if DEBUG
    return CKKSDisableKeyNotifications;
#else
    return false;
#endif
}
void SecCKKSTestSetDisableKeyNotifications(bool set) {
    CKKSDisableKeyNotifications = set;
}

static bool CKKSSkipScan = false;
bool SecCKKSTestSkipScan(void) {
    return CKKSSkipScan;
}
bool SecCKKSSetTestSkipScan(bool value) {
    CKKSSkipScan = value;
    return CKKSSkipScan;
}

void SecCKKSTestResetFlags(void) {
    SecCKKSTestSetDisableAutomaticUUID(false);
    SecCKKSTestSetDisableSOS(false);
    SecCKKSTestSetDisableKeyNotifications(false);
    SecCKKSSetTestSkipScan(false);
}

#else /* NO OCTAGON */

bool SecCKKSIsEnabled(void) {
    ckkserror_global("ckks", "CKKS was disabled at compile time.");
    return false;
}

bool SecCKKSEnable() {
    return false;
}

bool SecCKKSDisable() {
    return false;
}

bool SecCKKSResetSyncing(void) {
    return SecCKKSIsEnabled();
}

#endif /* OCTAGON */



void SecCKKSInitialize(SecDbRef db) {
#if OCTAGON
    @autoreleasepool {
        CKKSViewManager* manager = [CKKSViewManager manager];
        [manager createViews];
        [manager setupAnalytics];

        SecDbAddNotifyPhaseBlock(db, ^(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes) {
            SecCKKSNotifyBlock(dbconn, phase, source, changes);
        });

        [manager.completedSecCKKSInitialize fulfill];

        if(!SecCKKSTestsEnabled()) {
            static dispatch_once_t onceToken;
            dispatch_once(&onceToken, ^{
                [[OctagonAPSReceiver receiverForNamedDelegatePort:SecCKKSAPSNamedPort apsConnectionClass:[APSConnection class]] registerForEnvironment:APSEnvironmentProduction];
            });
        }
    }
#endif
}

void SecCKKSNotifyBlock(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes) {
#if OCTAGON
    if(phase == kSecDbTransactionDidRollback) {
        return;
    }

    // Ignore our own changes, otherwise we'd infinite-loop.
    if(source == kSecDbCKKSTransaction) {
        ckksinfo_global("ckks", "Ignoring kSecDbCKKSTransaction notification");
        return;
    }

    CFArrayForEach(changes, ^(CFTypeRef r) {
        SecDbItemRef deleted = NULL;
        SecDbItemRef added = NULL;

        SecDbEventTranslateComponents(r, (CFTypeRef*) &deleted, (CFTypeRef*) &added);

        if(!added && !deleted) {
            ckkserror_global("ckks", "SecDbEvent gave us garbage: %@", r);
            return;
        }

        [[CKKSViewManager manager] handleKeychainEventDbConnection: dbconn source:source added: added deleted: deleted];
    });
#endif
}

void SecCKKS24hrNotification() {
#if OCTAGON
    @autoreleasepool {
        [[CKKSViewManager manager] xpc24HrNotification];
    }
#endif
}

void CKKSRegisterSyncStatusCallback(CFStringRef cfuuid, SecBoolCFErrorCallback cfcallback) {
#if OCTAGON
    // Keep plumbing, but transition to NS.
    SecBoolNSErrorCallback nscallback = ^(bool result, NSError* err) {
        cfcallback(result, (__bridge CFErrorRef) err);
    };

    [[CKKSViewManager manager] registerSyncStatusCallback: (__bridge NSString*) cfuuid callback:nscallback];
#endif
}

void SecCKKSPerformLocalResync() {
#if OCTAGON
    if(SecCKKSIsEnabled()) {
        ckksnotice_global("reset", "Local keychain was reset; performing local resync");
        [[CKKSViewManager manager] rpcResyncLocal:nil reply:^(NSError *result) {
            if(result) {
                ckksnotice_global("reset", "Local keychain reset resync finished with an error: %@", result);
            } else {
                ckksnotice_global("reset", "Local keychain reset resync finished successfully");
            }
        }];
    }
#endif
}
