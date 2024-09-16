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

#ifndef CKKS_h
#define CKKS_h

#include <dispatch/dispatch.h>
#include "ipc/securityd_client.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/SecDb.h"
#include <xpc/xpc.h>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import "keychain/ot/OctagonStateMachine.h"
#endif /* __OBJC__ */

CF_ASSUME_NONNULL_BEGIN

#ifdef __OBJC__

typedef NS_ENUM(NSUInteger, SecCKKSItemEncryptionVersion) {
    CKKSItemEncryptionVersionNone = 0,  // No encryption present
    CKKSItemEncryptionVersion1 = 1,     // Current version, AES-SIV 512, not all fields authenticated
    CKKSItemEncryptionVersion2 = 2,     // Seed3 version, AES-SIV 512, all fields (including unknown fields) authenticated
};

extern const SecCKKSItemEncryptionVersion currentCKKSItemEncryptionVersion;

/* Queue Actions */
extern NSString* const SecCKKSActionAdd;
extern NSString* const SecCKKSActionDelete;
extern NSString* const SecCKKSActionModify;

/* Queue States */
@protocol SecCKKSItemState <NSObject>
@end
typedef NSString<SecCKKSItemState> CKKSItemState;
extern CKKSItemState* const SecCKKSStateNew;
extern CKKSItemState* const SecCKKSStateUnauthenticated;
extern CKKSItemState* const SecCKKSStateInFlight;
extern CKKSItemState* const SecCKKSStateReencrypt;
extern CKKSItemState* const SecCKKSStateError;
extern CKKSItemState* const SecCKKSStateDeleted;  // meta-state: please delete this item!
extern CKKSItemState* const SecCKKSStateMismatchedView; // This item was for a different view at processing time. Held pending a policy refresh.

/* Processed States */
@protocol SecCKKSProcessedState <NSObject>
@end
typedef NSString<SecCKKSProcessedState> CKKSProcessedState;
extern CKKSProcessedState* const SecCKKSProcessedStateLocal;
extern CKKSProcessedState* const SecCKKSProcessedStateRemote;

/* Key Classes */
@protocol SecCKKSKeyClass <NSObject>
@end
typedef NSString<SecCKKSKeyClass> CKKSKeyClass;
extern CKKSKeyClass* const SecCKKSKeyClassTLK;
extern CKKSKeyClass* const SecCKKSKeyClassA;
extern CKKSKeyClass* const SecCKKSKeyClassC;

@interface CKKSCurrentItemData: NSData
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithUUID:(NSString *)uuid;
@property (strong) NSString* uuid;
@property (strong, nullable) NSDate *modificationDate;
@end

/* Useful CloudKit configuration */
extern NSString* SecCKKSContainerName;
extern NSString* const SecCKKSSubscriptionID;
extern NSString* const SecCKKSAPSNamedPort;

/* This is the default CKKS context ID, which maps to what the DB upgrade will insert on existing dbs */
extern NSString* CKKSDefaultContextID;

/* Item CKRecords */
extern NSString* const SecCKRecordItemType;
extern NSString* const SecCKRecordHostOSVersionKey;
extern NSString* const SecCKRecordEncryptionVersionKey;
extern NSString* const SecCKRecordParentKeyRefKey;
extern NSString* const SecCKRecordDataKey;
extern NSString* const SecCKRecordWrappedKeyKey;
extern NSString* const SecCKRecordGenerationCountKey;
extern NSString* const SecCKRecordPCSServiceIdentifier;
extern NSString* const SecCKRecordPCSPublicKey;
extern NSString* const SecCKRecordPCSPublicIdentity;
extern NSString* const SecCKRecordServerWasCurrent;

/* Intermediate Key CKRecord Keys */
extern NSString* const SecCKRecordIntermediateKeyType;
extern NSString* const SecCKRecordKeyClassKey;
//extern NSString*  const SecCKRecordWrappedKeyKey;
//extern NSString*  const SecCKRecordParentKeyRefKey;

/* TLK Share CKRecord Keys */
// These are a bit special; they can't use the record ID as information without parsing.
extern NSString* const SecCKRecordTLKShareType;
extern NSString* const SecCKRecordSenderPeerID;
extern NSString* const SecCKRecordReceiverPeerID;
extern NSString* const SecCKRecordReceiverPublicEncryptionKey;
extern NSString* const SecCKRecordCurve;
extern NSString* const SecCKRecordEpoch;
extern NSString* const SecCKRecordPoisoned;
extern NSString* const SecCKRecordSignature;
extern NSString* const SecCKRecordVersion;
//extern NSString*  const SecCKRecordParentKeyRefKey; // reference to the key contained by this record
//extern NSString*  const SecCKRecordWrappedKeyKey;   // key material

/* Current Key CKRecord Keys */
extern NSString* const SecCKRecordCurrentKeyType;
// The key class will be the record name.
//extern NSString*  const SecCKRecordParentKeyRefKey; <-- represent the current key for this key class

/* Current Item CKRecord Keys */
extern NSString* const SecCKRecordCurrentItemType;
extern NSString* const SecCKRecordItemRefKey;


/* Device State CKRecord Keys */
extern NSString* const SecCKRecordDeviceStateType;
extern NSString* const SecCKRecordCirclePeerID;
extern NSString* const SecCKRecordOctagonPeerID;
extern NSString* const SecCKRecordOctagonStatus;
extern NSString* const SecCKRecordCircleStatus;
extern NSString* const SecCKRecordKeyState;
extern NSString* const SecCKRecordCurrentTLK;
extern NSString* const SecCKRecordCurrentClassA;
extern NSString* const SecCKRecordCurrentClassC;
extern NSString* const SecCKSRecordLastUnlockTime;
extern NSString* const SecCKSRecordOSVersionKey; // Similar to SecCKRecordHostOSVersionKey, but better named

/* Manifest master CKRecord Keys */
extern NSString* const SecCKRecordManifestType;
extern NSString* const SecCKRecordManifestDigestValueKey;
extern NSString* const SecCKRecordManifestGenerationCountKey;
extern NSString* const SecCKRecordManifestLeafRecordIDsKey;
extern NSString* const SecCKRecordManifestPeerManifestRecordIDsKey;
extern NSString* const SecCKRecordManifestCurrentItemsKey;
extern NSString* const SecCKRecordManifestSignaturesKey;
extern NSString* const SecCKRecordManifestSignerIDKey;
extern NSString* const SecCKRecordManifestSchemaKey;

/* Manifest leaf CKRecord Keys */
extern NSString* const SecCKRecordManifestLeafType;
extern NSString* const SecCKRecordManifestLeafDERKey;
extern NSString* const SecCKRecordManifestLeafDigestKey;

/* Zone Key Hierarchy States */
#if OCTAGON
typedef OctagonState CKKSZoneKeyState;
#else
// This is here to allow for building with Octagon off
@protocol SecCKKSZoneKeyState <NSObject>
@end
typedef NSString<SecCKKSZoneKeyState> CKKSZoneKeyState;
#endif

extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForCloudKitAccountStatus;

// CKKS is currently logged out
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateLoggedOut;

// Class has just been created.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateInitializing;
// CKKSZone has just informed us that its setup is done (and completed successfully).
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateInitialized;
// CKKSZone has informed us that zone setup did not work. Try again soon!
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateZoneCreationFailed;

// Everything is likely ready. Double-check.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateBecomeReady;
// Everything is ready and waiting for input.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateReady;

// No longer used in the local state machine, but might be relevant for other devices.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateReadyPendingUnlock;

// A key hierarchy fetch will now begin
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateBeginFetch;

// We're currently refetching the zone
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateFetch;
// A Fetch has just been completed which includes some new keys to process
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateFetchComplete;
// We'd really like a full refetch.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateNeedFullRefetch;

// The TLK doesn't appear to be present. Determine what to to next!
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateTLKMissing;

// We've received a wrapped TLK, but we don't have its contents yet. Wait until they arrive.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLK;

// No keys exist for this zone yet, and we're waiting to make some. Please call the "make TLKs" API.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLKCreation;
// No keys exist for this zone yet, but we've made some. Octagon should upload them.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLKUpload;

// We've received a wrapped TLK, but we can't process it until the keybag unlocks. Wait until then.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForUnlock;
// Some operation has noticed that trust is lost. Will enter WaitForTrust.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateLoseTrust;
// We've done some CK ops, but are waiting for the trust system to tell us to continue
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTrust;

// Things are unhealthy, but we're not sure entirely why.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateUnhealthy;
// Something has gone horribly wrong with the current key pointers.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateBadCurrentPointers;
// Something has gone wrong creating new TLKs.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateNewTLKsFailed;
// Something isn't quite right with the TLK shares.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateHealTLKShares;
// Something has gone wrong fixing TLK shares.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateHealTLKSharesFailed;
// The key hierarchy state machine is responding to a key state reprocess request
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateProcess;

extern CKKSZoneKeyState* const CKKSZoneKeyStateCheckTLKShares;

// CKKS is resetting the remote zone, due to key hierarchy reasons. Will not proceed until the local reset occurs.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingZone;
// CKKS is resetting the local data, likely to do a cloudkit reset or a rpc.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingLocalData;

// States to fix up data on-disk that's in a bad state
extern CKKSZoneKeyState* const CKKSZoneKeyStateFixupRefetchCurrentItemPointers;
extern CKKSZoneKeyState* const CKKSZoneKeyStateFixupFetchTLKShares;
extern CKKSZoneKeyState* const CKKSZoneKeyStateFixupLocalReload;
extern CKKSZoneKeyState* const CKKSZoneKeyStateFixupResaveDeviceStateEntries;
extern CKKSZoneKeyState* const CKKSZoneKeyStateFixupDeleteAllCKKSTombstones;

// Fatal error. Will not proceed unless fixed from outside class.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateError;

// If you absolutely need to numberify one of the above constants, here's your maps.
NSDictionary<CKKSZoneKeyState*, NSNumber*>* CKKSZoneKeyStateMap(void);
NSDictionary<NSNumber*, CKKSZoneKeyState*>* CKKSZoneKeyStateInverseMap(void);
NSNumber* CKKSZoneKeyToNumber(CKKSZoneKeyState* state);
CKKSZoneKeyState* CKKSZoneKeyRecover(NSNumber* stateNumber);

// We no longer use all of the above states, but we keep them around to fill out the State Map above.
// This is the set of valid states.
NSSet<CKKSZoneKeyState*>* CKKSKeyStateValidStates(void);

// Use this to determine if CKKS believes the current state is "transient": that is, should resolve itself with further local processing
// or 'nontransient': further local processing won't progress. Either we're ready, or waiting for the user to unlock, or a remote device to do something.
NSSet<CKKSZoneKeyState*>* CKKSKeyStateNonTransientStates(void);

/* Hide Item Length */
extern const NSUInteger SecCKKSItemPaddingBlockSize;

/* Aggd Keys */
extern NSString* const SecCKKSAggdPropagationDelay;
extern NSString* const SecCKKSAggdPrimaryKeyConflict;
extern NSString* const SecCKKSAggdViewKeyCount;
extern NSString* const SecCKKSAggdItemReencryption;

extern NSString* const SecCKKSUserDefaultsSuite;

extern NSString* const CKKSErrorDomain;
extern NSString* const CKKSServerExtensionErrorDomain;

/* Queue limits: these should likely be configurable via plist */
#define SecCKKSOutgoingQueueItemsAtOnce 100
#define SecCKKSIncomingQueueItemsAtOnce 50

// Utility functions
NSString* SecCKKSHostOSVersion(void);

#endif  // OBJ-C

/* C functions to interact with CKKS */
void SecCKKSInitialize(SecDbRef db);
void SecCKKSNotifyBlock(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes);

// Called by XPC approximately every 3 days
void SecCKKS24hrNotification(void);

// Register this callback to receive a call when the item with this UUID next successfully (or unsuccessfully) exits the outgoing queue.
void CKKSRegisterSyncStatusCallback(CFStringRef cfuuid, SecBoolCFErrorCallback callback);

// Tells CKKS that the local keychain was reset, and that it should respond accordingly
void SecCKKSPerformLocalResync(void);

// Returns true if CloudKit keychain syncing should occur
bool SecCKKSIsEnabled(void);

bool SecCKKSEnable(void);
bool SecCKKSDisable(void);

bool SecCKKSResetSyncing(void);

bool SecCKKSReduceRateLimiting(void);
bool SecCKKSSetReduceRateLimiting(bool value);

// Testing support
bool SecCKKSTestsEnabled(void);
bool SecCKKSTestsEnable(void);
bool SecCKKSTestsDisable(void);

void SecCKKSTestResetFlags(void);
bool SecCKKSTestDisableAutomaticUUID(void);
void SecCKKSTestSetDisableAutomaticUUID(bool set);

bool SecCKKSTestDisableSOS(void);
void SecCKKSTestSetDisableSOS(bool set);

bool SecCKKSTestDisableKeyNotifications(void);
void SecCKKSTestSetDisableKeyNotifications(bool set);

bool SecCKKSTestSkipScan(void);
bool SecCKKSSetTestSkipScan(bool value);

bool SecCKKSTestSkipTLKHealing(void);
bool SecCKKSSetTestSkipTLKShareHealing(bool value);

// TODO: handle errors better
typedef CF_ENUM(CFIndex, CKKSErrorCode) {
    CKKSNotInitialized = 9,
    CKKSNotLoggedIn = 10,
    CKKSNoSuchView = 11,

    CKKSRemoteItemChangePending = 12,
    CKKSLocalItemChangePending = 13,
    CKKSItemChanged = 14,
    CKKSNoUUIDOnItem = 15,
    CKKSItemCreationFailure = 16,
    CKKSInvalidKeyClass = 17,
    CKKSKeyNotSelfWrapped = 18,
    CKKSNoTrustedPeer = 19,
    CKKSDataMismatch = 20,
    CKKSProtobufFailure = 21,
    CKKSNoSuchRecord = 22,
    CKKSMissingTLKShare = 23,
    CKKSNoPeersAvailable = 24,

    CKKSSplitKeyHierarchy = 32,
    CKKSOrphanedKey = 33,
    CKKSInvalidTLK = 34,
    CKKSNoTrustedTLKShares = 35,
    CKKSKeyUnknownFormat = 36,
    CKKSNoSigningKey = 37,
    CKKSNoEncryptionKey = 38,

    CKKSNotHSA2 = 40,
    CKKSiCloudGreyMode = 41,

    CKKSNoFetchesRequested = 50,

    CKKSNoMetric = 51,

    CKKSLackingTrust = 52,
    CKKSKeysMissing = 53,

    CKKSCircularKeyReference = 54,

    CKKSErrorViewIsPaused = 55,
    CKKSErrorPolicyNotLoaded = 56,

    CKKSErrorUnexpectedNil = 57,
    CKKSErrorGenerationCountMismatch = 58,

    CKKSNoCloudKitDeviceID = 59,
    CKKSErrorRateLimited = 60,
    CKKSErrorTLKMismatch = 61,
    CKKSErrorViewNotExternallyManaged = 62,
    CKKSErrorViewIsExternallyManaged = 63,

    CKKSErrorAccountStatusUnknown = 64,
    CKKSErrorNotSupported = 65,

    CKKSErrorFetchNotCompleted = 66,
    CKKSErrorOutOfBandFetchingDisallowed = 67,
};

typedef CF_ENUM(CFIndex, CKKSResultDescriptionErrorCode) {
    CKKSResultDescriptionNone = 0,
    CKKSResultDescriptionPendingKeyReady = 1,
    CKKSResultDescriptionPendingSuccessfulFetch = 2,
    CKKSResultDescriptionPendingAccountLoggedIn = 3,
    CKKSResultDescriptionPendingUnlock = 4,
    CKKSResultDescriptionPendingBottledPeerModifyRecords = 5,
    CKKSResultDescriptionPendingBottledPeerFetchRecords = 6,

    CKKSResultDescriptionPendingZoneChangeFetchScheduling = 1000,
    CKKSResultDescriptionPendingViewChangedScheduling = 1001,
    CKKSResultDescriptionPendingZoneInitializeScheduling = 1002,  // No longer used
    CKKSResultDescriptionPendingOutgoingQueueScheduling = 1003,
    CKKSResultDescriptionPendingKeyHierachyPokeScheduling = 1004,
    CKKSResultDescriptionPendingCloudKitRetryAfter = 1005,
    CKKSResultDescriptionPendingFlag = 1006,

    CKKSResultDescriptionErrorPendingMachineRequestStart = 2000,
};

// These errors are returned by the CKKS server extension.
// Commented out codes here indicate that we don't currently handle them on the client side.
typedef CF_ENUM(CFIndex, CKKSServerExtensionErrorCode) {
    // Generic Errors
    //CKKSServerMissingField = 1,
    CKKSServerMissingRecord = 2,
    //CKKSServerUnexpectedFieldType = 3,
    //CKKSServerUnexpectedRecordType = 4,
    //CKKSServerUnepxectedRecordID = 5,

    // Chain errors:
    //CKKSServerMissingCurrentKeyPointer = 6,
    //CKKSServerMissingCurrentKey = 7,
    //CKKSServerUnexpectedSyncKeyClassInChain = 8,
    CKKSServerUnexpectedSyncKeyInChain = 9,

    // Item/Currentitem record errors:
    //CKKSServerKeyrollingNotAllowed = 10,
    //CKKSServerInvalidPublicIdentity = 11,
    //CKKSServerPublicKeyMismatch = 12,
    //CKKSServerServiceNumberMismatch = 13,
    //CKKSServerUnknownServiceNumber = 14,
    //CKKSServerEncverLessThanMinVal = 15,
    //CKKSServerCannotModifyWasCurrent = 16,
    //CKKSServerInvalidCurrentItem = 17,
};

#if __OBJC__

#define SecTranslateError(nserrorptr, cferror)             \
    if(nserrorptr) {                                       \
        *nserrorptr = (__bridge_transfer NSError*)cferror; \
    } else {                                               \
        CFReleaseNull(cferror);                            \
    }

extern os_log_t CKKSLogObject(NSString* scope, NSString* _Nullable zoneName);

// Very similar to the secerror, secnotice, and secinfo macros in debugging.h, but add zoneNames
#define ckkserrorwithzonename(scope, zoneName, format, ...) \
{ \
    os_log_with_type(CKKSLogObject(@scope, zoneName),       \
                    OS_LOG_TYPE_ERROR,                      \
                    format,                                 \
                    ##__VA_ARGS__);                         \
}

#define ckkserror(scope, zoneNameBearer, format, ...) \
    ckkserrorwithzonename(scope, zoneNameBearer.zoneName, format, ##__VA_ARGS__)

#define ckkserror_global(scope, format, ...) \
    ckkserrorwithzonename(scope, nil, format, ##__VA_ARGS__)

#define ckksnotice(scope, zoneNameBearer, format, ...) \
    os_log(CKKSLogObject(@scope, zoneNameBearer.zoneName), format, ##__VA_ARGS__)

#define ckksnotice_global(scope, format, ...) \
    os_log(CKKSLogObject(@scope, nil), format, ##__VA_ARGS__)

#define ckksinfo(scope, zoneNameBearer, format, ...) \
    os_log_debug(CKKSLogObject(@scope, zoneNameBearer.zoneName), format, ##__VA_ARGS__)

#define ckksinfo_global(scope, format, ...) \
    os_log_debug(CKKSLogObject(@scope, nil), format, ##__VA_ARGS__)

#endif // __OBJC__

CF_ASSUME_NONNULL_END

#endif /* CKKS_h */

