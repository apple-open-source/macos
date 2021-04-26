/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#if OCTAGON
#import "Analytics/SFAnalytics.h"

extern NSString* const CKKSAnalyticsInCircle;
extern NSString* const CKKSAnalyticsHasTLKs;
extern NSString* const CKKSAnalyticsSyncedClassARecently;
extern NSString* const CKKSAnalyticsSyncedClassCRecently;
extern NSString* const CKKSAnalyticsIncomingQueueIsErrorFree;
extern NSString* const CKKSAnalyticsOutgoingQueueIsErrorFree;
extern NSString* const CKKSAnalyticsInSync;
extern NSString* const CKKSAnalyticsValidCredentials;
extern NSString* const CKKSAnalyticsLastUnlock;
extern NSString* const CKKSAnalyticsLastKeystateReady;
extern NSString* const CKKSAnalyticsLastInCircle;

extern NSString* const CKKSAnalyticsNumberOfSyncItems;
extern NSString* const CKKSAnalyticsNumberOfTLKShares;
extern NSString* const CKKSAnalyticsNumberOfSyncKeys;

extern NSString* const OctagonAnalyticsStateMachineState;
extern NSString* const OctagonAnalyticIcloudAccountState;
extern NSString* const OctagonAnalyticCDPBitStatus;
extern NSString* const OctagonAnalyticsTrustState;
extern NSString* const OctagonAnalyticsAttemptedJoin;
extern NSString* const OctagonAnalyticsUserControllableViewsSyncing;
extern NSString* const OctagonAnalyticsLastHealthCheck;
extern NSString* const OctagonAnalyticsSOSStatus;
extern NSString* const OctagonAnalyticsDateOfLastPreflightPreapprovedJoin;
extern NSString* const OctagonAnalyticsLastKeystateReady;
extern NSString* const OctagonAnalyticsLastCoreFollowup;
extern NSString* const OctagonAnalyticsCoreFollowupFailureCount;
extern NSString* const OctagonAnalyticsCoreFollowupLastFailureTime;
extern NSString* const OctagonAnalyticsPrerecordPending;
extern NSString* const OctagonAnalyticsCDPStateRun;

extern NSString* const OctagonAnalyticsHaveMachineID;
extern NSString* const OctagonAnalyticsMIDOnMemoizedList;
extern NSString* const OctagonAnalyticsPeersWithMID;
extern NSString* const OctagonAnalyticsEgoMIDMatchesCurrentMID;

extern NSString* const OctagonAnalyticsTotalPeers;
extern NSString* const OctagonAnalyticsTotalViablePeers;

extern NSString* const CKKSAnalyticsLastCKKSPush;
extern NSString* const CKKSAnalyticsLastOctagonPush;

extern NSString* const OctagonAnalyticsKVSProvisioned;
extern NSString* const OctagonAnalyticsKVSEnabled;
extern NSString* const OctagonAnalyticsKeychainSyncProvisioned;
extern NSString* const OctagonAnalyticsKeychainSyncEnabled;
extern NSString* const OctagonAnalyticsCloudKitProvisioned;
extern NSString* const OctagonAnalyticsCloudKitEnabled;
extern NSString* const OctagonAnalyticsSecureBackupTermsAccepted;

extern NSString* const OctagonAnalyticsBottledUniqueTLKsRecovered;
extern NSString* const OctagonAnalyticsBottledTotalTLKShares;
extern NSString* const OctagonAnalyticsBottledTotalTLKSharesRecovered;
extern NSString* const OctagonAnalyticsBottledUniqueTLKsWithSharesCount;
extern NSString* const OctagonAnalyticsBottledTLKUniqueViewCount;

@protocol CKKSAnalyticsFailableEvent <NSObject>
@end
typedef NSString<CKKSAnalyticsFailableEvent> CKKSAnalyticsFailableEvent;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessOutgoingQueue;
extern CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges;
extern CKKSAnalyticsFailableEvent* const CKKSEventStateError;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessHealKeyHierarchy;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessReencryption;

extern CKKSAnalyticsFailableEvent* const OctagonEventPreflightBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventLaunchBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventScrubBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventSignIn;
extern CKKSAnalyticsFailableEvent* const OctagonEventSignOut;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoreBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventRamp;
extern CKKSAnalyticsFailableEvent* const OctagonEventBottleCheck;
extern CKKSAnalyticsFailableEvent* const OctagonEventCoreFollowUp;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpdateBottle;

extern CKKSAnalyticsFailableEvent* const OctagonEventRecoverBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventFetchAllBottles;
extern CKKSAnalyticsFailableEvent* const OctagonEventFetchEscrowContents;

extern CKKSAnalyticsFailableEvent* const OctagonEventRestoredSignedBottlePeer;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoredOctagonPeerEncryptionKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoredOctagonPeerSigningKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoreComplete;

/* Initial health check */
extern CKKSAnalyticsFailableEvent* const OctagonEventCheckTrustState;

/* Outer calls as seen by the client */
extern CKKSAnalyticsFailableEvent* const OctagonEventBottledPeerRestore;
extern CKKSAnalyticsFailableEvent* const OctagonEventFetchEscrowContents;
extern CKKSAnalyticsFailableEvent* const OctagonEventResetAndEstablish;
extern CKKSAnalyticsFailableEvent* const OctagonEventEstablish;
extern CKKSAnalyticsFailableEvent* const OctagonEventLeaveClique;
extern CKKSAnalyticsFailableEvent* const OctagonEventRemoveFriendsInClique;
extern CKKSAnalyticsFailableEvent* const OctagonEventRecoveryKey;

/* Inner calls as seen by TPH and securityd */
/* inner: Upgrade */
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradeFetchDeviceIDs;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradeSetAllowList;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradeSilentEscrow;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradePreapprovedJoin;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradePreflightPreapprovedJoin;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradePrepare;
extern CKKSAnalyticsFailableEvent* const OctagonEventUpgradePreapprovedJoinAfterPairing;

/* inner: join with voucher */
extern CKKSAnalyticsFailableEvent* const OctagonEventJoinWithVoucher;

/* inner: join with bottle */
extern CKKSAnalyticsFailableEvent* const OctagonEventPreflightVouchWithBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithBottle;

/* inner: join with recovery key */
extern CKKSAnalyticsFailableEvent* const OctagonEventPreflightVouchWithRecoveryKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithRecoveryKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyValidationFailed;
extern CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyFailed;
extern CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyCircleReset;
extern CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyCircleResetFailed;
extern CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyEnrollFailed;

/* inner: set recovery key */
extern CKKSAnalyticsFailableEvent* const OctagonEventSetRecoveryKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventSetRecoveryKeyValidationFailed;

/* inner: reset */
extern CKKSAnalyticsFailableEvent* const OctagonEventReset;

/* inner: prepare */
extern CKKSAnalyticsFailableEvent* const OctagonEventPrepareIdentity;

/* inner: establish */
extern CKKSAnalyticsFailableEvent* const OctagonEventEstablishIdentity;

/* inner: fetchviews */
extern CKKSAnalyticsFailableEvent* const OctagonEventFetchViews;

/* state machine */
extern CKKSAnalyticsFailableEvent* const OctagonEventStateTransition;

/* become untrusted */
extern CKKSAnalyticsFailableEvent* const OctagonEventCheckTrustForCFU;

/* watchOS only: pairing with companion */
extern CKKSAnalyticsFailableEvent* const OctagonEventCompanionPairing;

/* trust state from trusted peers helper*/
extern CKKSAnalyticsFailableEvent* const OctagonEventTPHHealthCheckStatus;

extern CKKSAnalyticsFailableEvent* const OctagonEventAuthKitDeviceList;

@protocol CKKSAnalyticsSignpostEvent <NSObject>
@end
typedef NSString<CKKSAnalyticsSignpostEvent> CKKSAnalyticsSignpostEvent;
extern CKKSAnalyticsSignpostEvent* const CKKSEventPushNotificationReceived;
extern CKKSAnalyticsSignpostEvent* const CKKSEventItemAddedToOutgoingQueue;
extern CKKSAnalyticsSignpostEvent* const CKKSEventReachabilityTimerExpired;
extern CKKSAnalyticsSignpostEvent* const CKKSEventMissingLocalItemsFound;

@protocol CKKSAnalyticsActivity <NSObject>
@end
typedef NSString<CKKSAnalyticsActivity> CKKSAnalyticsActivity;
extern CKKSAnalyticsActivity* const CKKSActivityOTFetchRampState;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonPreflightBottle;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonLaunchBottle;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonRestore;
extern CKKSAnalyticsActivity* const CKKSActivityScrubBottle;
extern CKKSAnalyticsActivity* const CKKSActivityBottleCheck;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonUpdateBottle;

extern CKKSAnalyticsActivity* const OctagonActivityAccountAvailable;
extern CKKSAnalyticsActivity* const OctagonActivityAccountNotAvailable;
extern CKKSAnalyticsActivity* const OctagonActivityResetAndEstablish;
extern CKKSAnalyticsActivity* const OctagonActivityEstablish;
extern CKKSAnalyticsActivity* const OctagonActivityFetchAllViableBottles;
extern CKKSAnalyticsActivity* const OctagonActivityFetchEscrowContents;
extern CKKSAnalyticsActivity* const OctagonActivityBottledPeerRestore;
extern CKKSAnalyticsActivity* const OctagonActivitySetRecoveryKey;
extern CKKSAnalyticsActivity* const OctagonActivityLeaveClique;
extern CKKSAnalyticsActivity* const OctagonActivityRemoveFriendsInClique;
extern CKKSAnalyticsActivity* const OctagonActivityJoinWithRecoveryKey;
extern CKKSAnalyticsActivity* const OctagonSOSAdapterUpdateKeys;


@interface CKKSAnalytics : SFAnalytics

+ (instancetype)logger;

- (void)logSuccessForEvent:(CKKSAnalyticsFailableEvent*)event zoneName:(NSString*)viewName;
- (void)logRecoverableError:(NSError*)error
                   forEvent:(CKKSAnalyticsFailableEvent*)event
                   zoneName:(NSString*)viewName
             withAttributes:(NSDictionary*)attributes;

- (void)logRecoverableError:(NSError*)error
                   forEvent:(CKKSAnalyticsFailableEvent*)event
             withAttributes:(NSDictionary *)attributes;

- (void)logUnrecoverableError:(NSError*)error
                     forEvent:(CKKSAnalyticsFailableEvent*)event
               withAttributes:(NSDictionary *)attributes;

- (void)logUnrecoverableError:(NSError*)error
                     forEvent:(CKKSAnalyticsFailableEvent*)event
                     zoneName:(NSString*)viewName
               withAttributes:(NSDictionary*)attributes;

- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event;
- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event zoneName:(NSString*)zoneName;

- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key zoneName:(NSString*)zoneName;
- (NSDate *)datePropertyForKey:(NSString *)key zoneName:(NSString*)zoneName;

@end

@interface CKKSAnalytics (UnitTesting)

- (NSDate*)dateOfLastSuccessForEvent:(CKKSAnalyticsFailableEvent*)event
                            zoneName:(NSString*)zoneName;
- (NSDictionary *)errorChain:(NSError *)error
                       depth:(NSUInteger)depth;
- (NSDictionary *)createErrorAttributes:(NSError *)error
                                  depth:(NSUInteger)depth
                             attributes:(NSDictionary *)attributes;

@end

#endif


