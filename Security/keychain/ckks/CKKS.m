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

#include <utilities/debugging.h>
#include <securityd/SecItemServer.h>
#include <Security/SecItemPriv.h>

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKey.h"

const SecCKKSItemEncryptionVersion currentCKKSItemEncryptionVersion = CKKSItemEncryptionVersion2;

NSString* const SecCKKSActionAdd = @"add";
NSString* const SecCKKSActionDelete = @"delete";
NSString* const SecCKKSActionModify = @"modify";

CKKSItemState* const SecCKKSStateNew = (CKKSItemState*) @"new";
CKKSItemState* const SecCKKSStateUnauthenticated = (CKKSItemState*) @"unauthenticated";
CKKSItemState* const SecCKKSStateInFlight = (CKKSItemState*) @"inflight";
CKKSItemState* const SecCKKSStateReencrypt = (CKKSItemState*) @"reencrypt";
CKKSItemState* const SecCKKSStateError = (CKKSItemState*) @"error";
CKKSItemState* const SecCKKSStateDeleted = (CKKSItemState*) @"deleted";

CKKSProcessedState* const SecCKKSProcessedStateLocal = (CKKSProcessedState*) @"local";
CKKSProcessedState* const SecCKKSProcessedStateRemote = (CKKSProcessedState*) @"remote";

CKKSKeyClass* const SecCKKSKeyClassTLK = (CKKSKeyClass*) @"tlk";
CKKSKeyClass* const SecCKKSKeyClassA = (CKKSKeyClass*) @"classA";
CKKSKeyClass* const SecCKKSKeyClassC = (CKKSKeyClass*) @"classC";

NSString* const SecCKKSContainerName = @"com.apple.security.keychain";
bool SecCKKSContainerUsePCS = false;

NSString* const SecCKKSSubscriptionID = @"keychain-changes";
NSString* const SecCKKSAPSNamedPort = @"com.apple.securityd.aps";

NSString* const SecCKRecordItemType = @"item";
NSString* const SecCKRecordVersionKey = @"uploadver";
NSString* const SecCKRecordEncryptionVersionKey = @"encver";
NSString* const SecCKRecordDataKey = @"data";
NSString* const SecCKRecordParentKeyRefKey = @"parentkeyref";
NSString* const SecCKRecordWrappedKeyKey = @"wrappedkey";
NSString* const SecCKRecordGenerationCountKey = @"gen";

NSString* const SecCKRecordPCSServiceIdentifier = @"pcsservice";
NSString* const SecCKRecordPCSPublicKey = @"pcspublickey";
NSString* const SecCKRecordPCSPublicIdentity = @"pcspublicidentity";
NSString* const SecCKRecordServerWasCurrent = @"server_wascurrent";

NSString* const SecCKRecordIntermediateKeyType = @"synckey";
NSString* const SecCKRecordKeyClassKey = @"class";

NSString* const SecCKRecordCurrentKeyType = @"currentkey";

NSString* const SecCKRecordCurrentItemType = @"currentitem";
NSString* const SecCKRecordItemRefKey = @"item";

NSString* const SecCKRecordDeviceStateType = @"devicestate";
NSString* const SecCKRecordCirclePeerID = @"peerid";
NSString* const SecCKRecordCircleStatus = @"circle";
NSString* const SecCKRecordKeyState = @"keystate";
NSString* const SecCKRecordCurrentTLK = @"currentTLK";
NSString* const SecCKRecordCurrentClassA = @"currentClassA";
NSString* const SecCKRecordCurrentClassC = @"currentClassC";

NSString* const SecCKRecordManifestType = @"manifest";
NSString* const SecCKRecordManifestDigestValueKey = @"digest_value";
NSString* const SecCKRecordManifestGenerationCountKey = @"generation_count";
NSString* const SecCKRecordManifestLeafRecordIDsKey = @"leaf_records";
NSString* const SecCKRecordManifestPeerManifestRecordIDsKey = @"peer_manifests";
NSString* const SecCKRecordManifestCurrentItemsKey = @"current_items";
NSString* const SecCKRecordManifestSignaturesKey = @"signatures";
NSString* const SecCKRecordManifestSignerIDKey = @"signer_id";
NSString* const SecCKRecordManifestSchemaKey = @"schema";

NSString* const SecCKRecordManifestLeafType = @"manifest_leaf";
NSString* const SecCKRecordManifestLeafDERKey = @"der";
NSString* const SecCKRecordManifestLeafDigestKey = @"digest";

CKKSZoneKeyState* const SecCKKSZoneKeyStateReady = (CKKSZoneKeyState*) @"ready";
CKKSZoneKeyState* const SecCKKSZoneKeyStateError = (CKKSZoneKeyState*) @"error";
CKKSZoneKeyState* const SecCKKSZoneKeyStateCancelled = (CKKSZoneKeyState*) @"cancelled";

CKKSZoneKeyState* const SecCKKSZoneKeyStateInitializing = (CKKSZoneKeyState*) @"initializing";
CKKSZoneKeyState* const SecCKKSZoneKeyStateInitialized = (CKKSZoneKeyState*) @"initialized";
CKKSZoneKeyState* const SecCKKSZoneKeyStateFetchComplete = (CKKSZoneKeyState*) @"fetchcomplete";
CKKSZoneKeyState* const SecCKKSZoneKeyStateNeedFullRefetch = (CKKSZoneKeyState*) @"needrefetch";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLK = (CKKSZoneKeyState*) @"waitfortlk";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForUnlock = (CKKSZoneKeyState*) @"waitforunlock";
CKKSZoneKeyState* const SecCKKSZoneKeyStateUnhealthy = (CKKSZoneKeyState*) @"unhealthy";
CKKSZoneKeyState* const SecCKKSZoneKeyStateBadCurrentPointers = (CKKSZoneKeyState*) @"badcurrentpointers";
CKKSZoneKeyState* const SecCKKSZoneKeyStateNewTLKsFailed = (CKKSZoneKeyState*) @"newtlksfailed";

NSDictionary<CKKSZoneKeyState*, NSNumber*>* CKKSZoneKeyStateMap(void) {
    static NSDictionary<CKKSZoneKeyState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
          SecCKKSZoneKeyStateReady:              [NSNumber numberWithUnsignedInt: 0],
          SecCKKSZoneKeyStateError:              [NSNumber numberWithUnsignedInt: 1],
          SecCKKSZoneKeyStateCancelled:          [NSNumber numberWithUnsignedInt: 2],

          SecCKKSZoneKeyStateInitializing:       [NSNumber numberWithUnsignedInt: 3],
          SecCKKSZoneKeyStateInitialized:        [NSNumber numberWithUnsignedInt: 4],
          SecCKKSZoneKeyStateFetchComplete:      [NSNumber numberWithUnsignedInt: 5],
          SecCKKSZoneKeyStateWaitForTLK:         [NSNumber numberWithUnsignedInt: 6],
          SecCKKSZoneKeyStateWaitForUnlock:      [NSNumber numberWithUnsignedInt: 7],
          SecCKKSZoneKeyStateUnhealthy:          [NSNumber numberWithUnsignedInt: 8],
          SecCKKSZoneKeyStateBadCurrentPointers: [NSNumber numberWithUnsignedInt: 9],
          SecCKKSZoneKeyStateNewTLKsFailed:      [NSNumber numberWithUnsignedInt:10],
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

const NSUInteger SecCKKSItemPaddingBlockSize = 20;

NSString* const SecCKKSAggdPropagationDelay   = @"com.apple.security.ckks.propagationdelay";
NSString* const SecCKKSAggdPrimaryKeyConflict = @"com.apple.security.ckks.pkconflict";
NSString* const SecCKKSAggdViewKeyCount = @"com.apple.security.ckks.keycount";
NSString* const SecCKKSAggdItemReencryption = @"com.apple.security.ckks.reencrypt";

NSString* const SecCKKSUserDefaultsSuite = @"com.apple.security.ckks";

#if OCTAGON
static bool enableCKKS = true;
static bool testCKKS = false;

bool SecCKKSIsEnabled(void) {
    if([CKDatabase class] == nil) {
        // CloudKit is not linked. We cannot bring CKKS up; disable it with prejudice.
        secerror("CKKS: CloudKit.framework appears to not be linked. Cannot enable CKKS (on pain of crash).");
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
    [CKKSViewManager resetManager: true setTo: nil];
    return SecCKKSIsEnabled();
}

bool SecCKKSTestsEnabled(void) {
    return testCKKS;
}

bool SecCKKSTestsEnable(void) {
    if([CKDatabase class] == nil) {
        // CloudKit is not linked. We cannot bring CKKS up; disable it with prejudice.
        secerror("CKKS: CloudKit.framework appears to not be linked. Cannot enable CKKS testing.");
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

void SecCKKSTestResetFlags(void) {
    SecCKKSTestSetDisableAutomaticUUID(false);
    SecCKKSTestSetDisableSOS(false);
    SecCKKSTestSetDisableKeyNotifications(false);
}

XPC_RETURNS_RETAINED xpc_endpoint_t
SecServerCreateCKKSEndpoint(void)
{
    if (SecCKKSIsEnabled()) {
        return [[CKKSViewManager manager] xpcControlEndpoint];
    } else {
        return NULL;
    }
}

#else /* NO OCTAGON */

bool SecCKKSIsEnabled(void) {
    secerror("CKKS was disabled at compile time.");
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

XPC_RETURNS_RETAINED xpc_endpoint_t
SecServerCreateCKKSEndpoint(void)
{
    return NULL;
}
#endif /* OCTAGON */



void SecCKKSInitialize(SecDbRef db) {
#if OCTAGON
    CKKSViewManager* manager = [CKKSViewManager manager];
    [manager initializeZones];

    SecDbAddNotifyPhaseBlock(db, ^(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes) {
        SecCKKSNotifyBlock(dbconn, phase, source, changes);
    });

    [manager.completedSecCKKSInitialize fulfill];
#endif
}

void SecCKKSNotifyBlock(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes) {
#if OCTAGON
    if(phase == kSecDbTransactionDidRollback) {
        return;
    }

    // Ignore our own changes, otherwise we'd infinite-loop.
    if(source == kSecDbCKKSTransaction) {
        secinfo("ckks", "Ignoring kSecDbCKKSTransaction notification");
        return;
    }

    CFArrayForEach(changes, ^(CFTypeRef r) {
        SecDbItemRef deleted = NULL;
        SecDbItemRef added = NULL;

        SecDbEventTranslateComponents(r, (CFTypeRef*) &deleted, (CFTypeRef*) &added);

        if(!added && !deleted) {
            secerror("CKKS: SecDbEvent gave us garbage: %@", r);
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
