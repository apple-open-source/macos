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
#include <ipc/securityd_client.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <xpc/xpc.h>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
#else
CF_ASSUME_NONNULL_BEGIN
#endif

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

/* Useful CloudKit configuration */
extern NSString* const SecCKKSContainerName;
extern bool SecCKKSContainerUsePCS;
extern NSString* const SecCKKSSubscriptionID;
extern NSString* const SecCKKSAPSNamedPort;

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
@protocol SecCKKSZoneKeyState <NSObject>
@end
typedef NSString<SecCKKSZoneKeyState> CKKSZoneKeyState;

// CKKS is currently logged out
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateLoggedOut;

// Class has just been created.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateInitializing;
// CKKSZone has just informed us that its setup is done (and completed successfully).
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateInitialized;
// CKKSZone has informed us that zone setup did not work. Try again soon!
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateZoneCreationFailed;
// Everything is ready and waiting for input.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateReady;
// We're presumably ready, but we'd like to do one or two more checks after we unlock.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateReadyPendingUnlock;

// We're currently refetching the zone
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateFetch;
// A Fetch has just been completed which includes some new keys to process
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateFetchComplete;
// We'd really like a full refetch.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateNeedFullRefetch;
// We've received a wrapped TLK, but we don't have its contents yet. Wait until they arrive.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLK;
// We've received a wrapped TLK, but we can't process it until the keybag unlocks. Wait until then.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForUnlock;
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
// The key hierarchy state machine needs to wait for the fixup operation to complete
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForFixupOperation;

// CKKS is resetting the remote zone, due to key hierarchy reasons. Will not proceed until the local reset occurs.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingZone;
// CKKS is resetting the local data, likely to do a cloudkit reset or a rpc.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingLocalData;

// Fatal error. Will not proceed unless fixed from outside class.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateError;
// This CKKS instance has been cancelled.
extern CKKSZoneKeyState* const SecCKKSZoneKeyStateCancelled;

// If you absolutely need to numberify one of the above constants, here's your maps.
NSDictionary<CKKSZoneKeyState*, NSNumber*>* CKKSZoneKeyStateMap(void);
NSDictionary<NSNumber*, CKKSZoneKeyState*>* CKKSZoneKeyStateInverseMap(void);
NSNumber* CKKSZoneKeyToNumber(CKKSZoneKeyState* state);
CKKSZoneKeyState* CKKSZoneKeyRecover(NSNumber* stateNumber);

// Use this to determine if CKKS believes the current state is "transient": that is, should resolve itself with further local processing
// or 'nontransient': further local processing won't progress. Either we're ready, or waiting for the user to unlock, or a remote device to do something.
bool CKKSKeyStateTransient(CKKSZoneKeyState* state);

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
#define SecCKKSIncomingQueueItemsAtOnce 10

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

bool SecCKKSSyncManifests(void);
bool SecCKKSEnableSyncManifests(void);
bool SecCKKSSetSyncManifests(bool value);

bool SecCKKSEnforceManifests(void);
bool SecCKKSEnableEnforceManifests(void);
bool SecCKKSSetEnforceManifests(bool value);

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
    CKKSResultDescriptionPendingZoneInitializeScheduling = 1002,
    CKKSResultDescriptionPendingOutgoingQueueScheduling = 1003,
    CKKSResultDescriptionPendingKeyHierachyPokeScheduling = 1004,
};

// These errors are returned by the CKKS server extension.
// Commented out codes here indicate that we don't currently handle them on the client side.
typedef CF_ENUM(CFIndex, CKKSServerExtensionErrorCode) {
    // Generic Errors
    //CKKSServerMissingField = 1,
    //CKKSServerMissingRecord = 2,
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

#define SecTranslateError(nserrorptr, cferror)             \
    if(nserrorptr) {                                       \
        *nserrorptr = (__bridge_transfer NSError*)cferror; \
    } else {                                               \
        CFReleaseNull(cferror);                            \
    }

// Very similar to the secerror, secnotice, and secinfo macros in debugging.h, but add zoneNames
#define ckkserrorwithzonename(scope, zoneName, format, ...)                                                             \
    {                                                                                                                   \
        os_log(secLogObjForScope("SecError"), scope "-%@: " format, (zoneName ? zoneName : @"unknown"), ##__VA_ARGS__); \
    }
#define ckksnoticewithzonename(scope, zoneName, format, ...)                                                                         \
    {                                                                                                                                \
        os_log(secLogObjForCFScope((__bridge CFStringRef)[@(scope "-") stringByAppendingString:(zoneName ? zoneName : @"unknown")]), \
               format,                                                                                                               \
               ##__VA_ARGS__);                                                                                                       \
    }
#define ckksinfowithzonename(scope, zoneName, format, ...)                                                                                 \
    {                                                                                                                                      \
        os_log_debug(secLogObjForCFScope((__bridge CFStringRef)[@(scope "-") stringByAppendingString:(zoneName ? zoneName : @"unknown")]), \
                     format,                                                                                                               \
                     ##__VA_ARGS__);                                                                                                       \
    }

#define ckkserror(scope, zoneNameHaver, format, ...)             \
    {                                                            \
        NSString* znh = zoneNameHaver.zoneName;                  \
        ckkserrorwithzonename(scope, znh, format, ##__VA_ARGS__) \
    }
#define ckksnotice(scope, zoneNameHaver, format, ...)             \
    {                                                             \
        NSString* znh = zoneNameHaver.zoneName;                   \
        ckksnoticewithzonename(scope, znh, format, ##__VA_ARGS__) \
    }
#define ckksinfo(scope, zoneNameHaver, format, ...)             \
    {                                                           \
        NSString* znh = zoneNameHaver.zoneName;                 \
        ckksinfowithzonename(scope, znh, format, ##__VA_ARGS__) \
    }

#undef ckksdebug
#if !defined(NDEBUG)
#define ckksdebugwithzonename(scope, zoneName, format, ...)                                                                                \
    {                                                                                                                                      \
        os_log_debug(secLogObjForCFScope((__bridge CFStringRef)[@(scope "-") stringByAppendingString:(zoneName ? zoneName : @"unknown")]), \
                     format,                                                                                                               \
                     ##__VA_ARGS__);                                                                                                       \
    }
#define ckksdebug(scope, zoneNameHaver, format, ...)             \
    {                                                            \
        NSString* znh = zoneNameHaver.zoneName;                  \
        ckksdebugwithzonename(scope, znh, format, ##__VA_ARGS__) \
    }
#else
#define ckksdebug(scope, ...) /* nothing */
#endif

#ifdef __OBJC__
NS_ASSUME_NONNULL_END
#else
CF_ASSUME_NONNULL_END
#endif

#endif /* CKKS_h */

