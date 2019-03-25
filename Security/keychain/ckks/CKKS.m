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
#include <sys/sysctl.h>
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

#import "keychain/ot/OTManager.h"
const SecCKKSItemEncryptionVersion currentCKKSItemEncryptionVersion = CKKSItemEncryptionVersion2;

NSString* const SecCKKSActionAdd = @"add";
NSString* const SecCKKSActionDelete = @"delete";
NSString* const SecCKKSActionModify = @"modify";

CKKSItemState* const SecCKKSStateNew = (CKKSItemState*) @"new";
CKKSItemState* const SecCKKSStateUnauthenticated = (CKKSItemState*) @"unauthenticated";
CKKSItemState* const SecCKKSStateInFlight = (CKKSItemState*) @"inflight";
CKKSItemState* const SecCKKSStateReencrypt = (CKKSItemState*) @"reencrypt";
CKKSItemState* const SecCKKSStateError = (CKKSItemState*) @"error";
CKKSItemState* const SecCKKSStateZoneMismatch = (CKKSItemState*) @"zone_mismatch";
CKKSItemState* const SecCKKSStateDeleted = (CKKSItemState*) @"deleted";

CKKSProcessedState* const SecCKKSProcessedStateLocal = (CKKSProcessedState*) @"local";
CKKSProcessedState* const SecCKKSProcessedStateRemote = (CKKSProcessedState*) @"remote";

CKKSKeyClass* const SecCKKSKeyClassTLK = (CKKSKeyClass*) @"tlk";
CKKSKeyClass* const SecCKKSKeyClassA = (CKKSKeyClass*) @"classA";
CKKSKeyClass* const SecCKKSKeyClassC = (CKKSKeyClass*) @"classC";

NSString* SecCKKSContainerName = @"com.apple.security.keychain";
bool SecCKKSContainerUsePCS = false;

NSString* const SecCKKSSubscriptionID = @"keychain-changes";
NSString* const SecCKKSAPSNamedPort = @"com.apple.securityd.aps";

NSString* const SecCKRecordItemType = @"item";
NSString* const SecCKRecordHostOSVersionKey = @"uploadver";
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

NSString* const SecCKRecordTLKShareType = @"tlkshare";
NSString* const SecCKRecordSenderPeerID = @"sender";
NSString* const SecCKRecordReceiverPeerID = @"receiver";
NSString* const SecCKRecordReceiverPublicEncryptionKey = @"receiverPublicEncryptionKey";
NSString* const SecCKRecordCurve = @"curve";
NSString* const SecCKRecordEpoch = @"epoch";
NSString* const SecCKRecordPoisoned = @"poisoned";
NSString* const SecCKRecordSignature = @"signature";
NSString* const SecCKRecordVersion = @"version";

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
NSString* const SecCKSRecordLastUnlockTime = @"lastunlock";
NSString* const SecCKSRecordOSVersionKey = @"osver";

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
CKKSZoneKeyState* const SecCKKSZoneKeyStateReadyPendingUnlock = (CKKSZoneKeyState*) @"readypendingunlock";
CKKSZoneKeyState* const SecCKKSZoneKeyStateError = (CKKSZoneKeyState*) @"error";
CKKSZoneKeyState* const SecCKKSZoneKeyStateCancelled = (CKKSZoneKeyState*) @"cancelled";

CKKSZoneKeyState* const SecCKKSZoneKeyStateInitializing = (CKKSZoneKeyState*) @"initializing";
CKKSZoneKeyState* const SecCKKSZoneKeyStateInitialized = (CKKSZoneKeyState*) @"initialized";
CKKSZoneKeyState* const SecCKKSZoneKeyStateFetch = (CKKSZoneKeyState*) @"fetching";
CKKSZoneKeyState* const SecCKKSZoneKeyStateFetchComplete = (CKKSZoneKeyState*) @"fetchcomplete";
CKKSZoneKeyState* const SecCKKSZoneKeyStateNeedFullRefetch = (CKKSZoneKeyState*) @"needrefetch";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLK = (CKKSZoneKeyState*) @"waitfortlk";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForUnlock = (CKKSZoneKeyState*) @"waitforunlock";
CKKSZoneKeyState* const SecCKKSZoneKeyStateUnhealthy = (CKKSZoneKeyState*) @"unhealthy";
CKKSZoneKeyState* const SecCKKSZoneKeyStateBadCurrentPointers = (CKKSZoneKeyState*) @"badcurrentpointers";
CKKSZoneKeyState* const SecCKKSZoneKeyStateNewTLKsFailed = (CKKSZoneKeyState*) @"newtlksfailed";
CKKSZoneKeyState* const SecCKKSZoneKeyStateHealTLKShares = (CKKSZoneKeyState*) @"healtlkshares";
CKKSZoneKeyState* const SecCKKSZoneKeyStateHealTLKSharesFailed = (CKKSZoneKeyState*) @"healtlksharesfailed";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForFixupOperation = (CKKSZoneKeyState*) @"waitforfixupoperation";
CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingZone = (CKKSZoneKeyState*) @"resetzone";
CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingLocalData = (CKKSZoneKeyState*) @"resetlocal";
CKKSZoneKeyState* const SecCKKSZoneKeyStateLoggedOut = (CKKSZoneKeyState*) @"loggedout";
CKKSZoneKeyState* const SecCKKSZoneKeyStateZoneCreationFailed = (CKKSZoneKeyState*) @"zonecreationfailed";
CKKSZoneKeyState* const SecCKKSZoneKeyStateProcess = (CKKSZoneKeyState*) @"process";

NSDictionary<CKKSZoneKeyState*, NSNumber*>* CKKSZoneKeyStateMap(void) {
    static NSDictionary<CKKSZoneKeyState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
          SecCKKSZoneKeyStateReady:              @0U,
          SecCKKSZoneKeyStateError:              @1U,
          SecCKKSZoneKeyStateCancelled:          @2U,

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

bool CKKSKeyStateTransient(CKKSZoneKeyState* state) {
    // Easier to compare against a blacklist of end states
    bool nontransient = [state isEqualToString:SecCKKSZoneKeyStateReady] ||
        [state isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock] ||
        [state isEqualToString:SecCKKSZoneKeyStateWaitForTLK] ||
        [state isEqualToString:SecCKKSZoneKeyStateWaitForUnlock] ||
        [state isEqualToString:SecCKKSZoneKeyStateError] ||
        [state isEqualToString:SecCKKSZoneKeyStateCancelled];
    return !nontransient;
}

const NSUInteger SecCKKSItemPaddingBlockSize = 20;

NSString* const SecCKKSAggdPropagationDelay   = @"com.apple.security.ckks.propagationdelay";
NSString* const SecCKKSAggdPrimaryKeyConflict = @"com.apple.security.ckks.pkconflict";
NSString* const SecCKKSAggdViewKeyCount = @"com.apple.security.ckks.keycount";
NSString* const SecCKKSAggdItemReencryption = @"com.apple.security.ckks.reencrypt";

NSString* const SecCKKSUserDefaultsSuite = @"com.apple.security.ckks";

NSString* const CKKSErrorDomain = @"CKKSErrorDomain";
NSString* const CKKSServerExtensionErrorDomain = @"CKKSServerExtensionErrorDomain";

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
        secnotice("ckks", "reduce-rate-limiting is %@", CKKSReduceRateLimiting ? @"on" : @"off");
    });

    return CKKSReduceRateLimiting;
}

bool SecCKKSSetReduceRateLimiting(bool value) {
    (void) SecCKKSReduceRateLimiting(); // Call this once to read the defaults write
    CKKSReduceRateLimiting = value;
    secnotice("ckks", "reduce-rate-limiting is now %@", CKKSReduceRateLimiting ? @"on" : @"off");
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
        secnotice("ckksshare", "TLK sharing is %@", CKKSShareTLKs ? @"on" : @"off");
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

void SecCKKSTestResetFlags(void) {
    SecCKKSTestSetDisableAutomaticUUID(false);
    SecCKKSTestSetDisableSOS(false);
    SecCKKSTestSetDisableKeyNotifications(false);
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

#endif /* OCTAGON */



void SecCKKSInitialize(SecDbRef db) {
#if OCTAGON
    @autoreleasepool {
        CKKSViewManager* manager = [CKKSViewManager manager];
        [manager initializeZones];

        SecDbAddNotifyPhaseBlock(db, ^(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes) {
            SecCKKSNotifyBlock(dbconn, phase, source, changes);
        });

        [manager.completedSecCKKSInitialize fulfill];
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

void SecCKKSPerformLocalResync() {
#if OCTAGON
    secnotice("ckks", "Local keychain was reset; performing local resync");
    [[CKKSViewManager manager] rpcResyncLocal:nil reply:^(NSError *result) {
        if(result) {
            secnotice("ckks", "Local keychain reset resync finished with an error: %@", result);
        } else {
            secnotice("ckks", "Local keychain reset resync finished successfully");
        }
    }];
#endif
}

NSString* SecCKKSHostOSVersion()
{
#ifdef PLATFORM
    // Use complicated macro magic to get the string value passed in as preprocessor define PLATFORM.
#define PLATFORM_VALUE(f) #f
#define PLATFORM_OBJCSTR(f) @PLATFORM_VALUE(f)
    NSString* platform = (PLATFORM_OBJCSTR(PLATFORM));
#undef PLATFORM_OBJCSTR
#undef PLATFORM_VALUE
#else
    NSString* platform = "unknown";
#warning No PLATFORM defined; why?
#endif

    NSString* osversion = nil;

    // If we can get the build information from sysctl, use it.
    char release[256];
    size_t releasesize = sizeof(release);
    bool haveSysctlInfo = true;
    haveSysctlInfo &= (0 == sysctlbyname("kern.osrelease", release, &releasesize, NULL, 0));

    char version[256];
    size_t versionsize = sizeof(version);
    haveSysctlInfo &= (0 == sysctlbyname("kern.osversion", version, &versionsize, NULL, 0));

    if(haveSysctlInfo) {
        // Null-terminate for extra safety
        release[sizeof(release)-1] = '\0';
        version[sizeof(version)-1] = '\0';
        osversion = [NSString stringWithFormat:@"%s (%s)", release, version];
    }

    if(!osversion) {
        //  Otherwise, use the not-really-supported fallback.
        osversion = [[NSProcessInfo processInfo] operatingSystemVersionString];

        // subtly improve osversion (but it's okay if that does nothing)
        osversion = [osversion stringByReplacingOccurrencesOfString:@"Version" withString:@""];
    }

    return [NSString stringWithFormat:@"%@ %@", platform, osversion];
}
