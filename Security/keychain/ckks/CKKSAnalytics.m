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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <os/log.h>

#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#include <utilities/SecFileLocations.h>
#include <sys/stat.h>

NSString* const CKKSAnalyticsInCircle = @"inCircle";
NSString* const CKKSAnalyticsHasTLKs = @"TLKs";
NSString* const CKKSAnalyticsSyncedClassARecently = @"inSyncA";
NSString* const CKKSAnalyticsSyncedClassCRecently = @"inSyncC";
NSString* const CKKSAnalyticsIncomingQueueIsErrorFree = @"IQNOE";
NSString* const CKKSAnalyticsOutgoingQueueIsErrorFree = @"OQNOE";
NSString* const CKKSAnalyticsInSync = @"inSync";
NSString* const CKKSAnalyticsValidCredentials = @"validCredentials";
NSString* const CKKSAnalyticsLastUnlock = @"lastUnlock";
NSString* const CKKSAnalyticsLastKeystateReady = @"lastKSR";
NSString* const CKKSAnalyticsLastInCircle = @"lastInCircle";

NSString* const CKKSAnalyticsNumberOfSyncItems = @"numItems";
NSString* const CKKSAnalyticsNumberOfTLKShares = @"numTLKShares";
NSString* const CKKSAnalyticsNumberOfSyncKeys = @"numSyncKeys";

NSString* const OctagonAnalyticsStateMachineState = @"OASMState";
NSString* const OctagonAnalyticIcloudAccountState = @"OAiC";
NSString* const OctagonAnalyticCDPBitStatus = @"OACDPStatus";

NSString* const OctagonAnalyticsTrustState = @"OATrust";
NSString* const OctagonAnalyticsAttemptedJoin = @"OAAttemptedJoin";
NSString* const OctagonAnalyticsUserControllableViewsSyncing = @"OAUserViewsSyncing";
NSString* const OctagonAnalyticsLastHealthCheck = @"OAHealthCheck";
NSString* const OctagonAnalyticsSOSStatus = @"OASOSStatus";
NSString* const OctagonAnalyticsDateOfLastPreflightPreapprovedJoin = @"OALastPPJ";
NSString* const OctagonAnalyticsLastKeystateReady = @"OALastKSR";
NSString* const OctagonAnalyticsLastCoreFollowup = @"OALastCFU";
//NSString* const OctagonAnalyticsCoreFollowupStatus = @"OACFUStatus";
NSString* const OctagonAnalyticsCoreFollowupFailureCount = @"OACFUTFailureCount";
NSString* const OctagonAnalyticsCoreFollowupLastFailureTime = @"OACFULastFailureTime";
NSString* const OctagonAnalyticsPrerecordPending = @"OAPrerecordPending";
NSString* const OctagonAnalyticsCDPStateRun = @"OACDPStateRun";

NSString* const OctagonAnalyticsBottledUniqueTLKsRecovered = @"OABottledUniqueTLKsRecoveredCount";
NSString* const OctagonAnalyticsBottledTotalTLKShares = @"OABottledTotalTLKSharesCount";
NSString* const OctagonAnalyticsBottledTotalTLKSharesRecovered = @"OABottledTotalTLKSharesRecoveredCount";
NSString* const OctagonAnalyticsBottledUniqueTLKsWithSharesCount = @"OABottledUniqueTLKsWithSharesCount";
NSString* const OctagonAnalyticsBottledTLKUniqueViewCount = @"OABottledTLKUniqueViewCount";

NSString* const OctagonAnalyticsRKUniqueTLKsRecovered = @"OARKUniqueTLKsRecoveredCount";
NSString* const OctagonAnalyticsRKTotalTLKShares = @"OARKTotalTLKSharesCount";
NSString* const OctagonAnalyticsRKTotalTLKSharesRecovered = @"OARKTotalTLKSharesRecoveredCount";
NSString* const OctagonAnalyticsRKUniqueTLKsWithSharesCount = @"OARKUniqueTLKsWithSharesCount";
NSString* const OctagonAnalyticsRKTLKUniqueViewCount = @"OARKTLKUniqueViewCount";

NSString* const OctagonAnalyticsCustodianUniqueTLKsRecovered = @"OACustodianUniqueTLKsRecoveredCount";
NSString* const OctagonAnalyticsCustodianTotalTLKShares = @"OACustodianTotalTLKSharesCount";
NSString* const OctagonAnalyticsCustodianTotalTLKSharesRecovered = @"OACustodianTotalTLKSharesRecoveredCount";
NSString* const OctagonAnalyticsCustodianUniqueTLKsWithSharesCount = @"OACustodianUniqueTLKsWithSharesCount";
NSString* const OctagonAnalyticsCustodianTLKUniqueViewCount = @"OACustodianTLKUniqueViewCount";

NSString* const OctagonAnalyticsInheritanceUniqueTLKsRecovered = @"OAInheritanceUniqueTLKsRecoveredCount";
NSString* const OctagonAnalyticsInheritanceTotalTLKShares = @"OAInheritanceTotalTLKSharesCount";
NSString* const OctagonAnalyticsInheritanceTotalTLKSharesRecovered = @"OAInheritanceTotalTLKSharesRecoveredCount";
NSString* const OctagonAnalyticsInheritanceUniqueTLKsWithSharesCount = @"OAInheritanceUniqueTLKsWithSharesCount";
NSString* const OctagonAnalyticsInheritanceTLKUniqueViewCount = @"OAInheritanceTLKUniqueViewCount";

NSString* const OctagonAnalyticsHaveMachineID = @"OAMIDPresent";
NSString* const OctagonAnalyticsMIDOnMemoizedList = @"OAMIDOnList";
NSString* const OctagonAnalyticsPeersWithMID = @"OAPeersWithMID";
NSString* const OctagonAnalyticsEgoMIDMatchesCurrentMID = @"OAMIDMatchesCurrentMID";

NSString* const OctagonAnalyticsTotalPeers = @"OAnPeers";
NSString* const OctagonAnalyticsTotalViablePeers = @"OAnViablePeers";

NSString* const CKKSAnalyticsLastCKKSPush = @"lastCKKSPush";
NSString* const CKKSAnalyticsLastOctagonPush = @"lastOctagonPush";

NSString* const OctagonAnalyticsKVSProvisioned = @"OADCKVSProvisioned";
NSString* const OctagonAnalyticsKVSEnabled = @"OADCKVSEnabled";
NSString* const OctagonAnalyticsKeychainSyncProvisioned = @"OADCKCSProvisioned";
NSString* const OctagonAnalyticsKeychainSyncEnabled = @"OADCKCSEnabled";
NSString* const OctagonAnalyticsCloudKitProvisioned = @"OADCCKProvisioned";
NSString* const OctagonAnalyticsCloudKitEnabled = @"OADCCKEnabled";
NSString* const OctagonAnalyticsSecureBackupTermsAccepted = @"OASecureBackupTermsAccepted";

static NSString* const CKKSAnalyticsAttributeRecoverableError = @"recoverableError";
static NSString* const CKKSAnalyticsAttributeZoneName = @"zone";
static NSString* const CKKSAnalyticsAttributeErrorDomain = @"errorDomain";
static NSString* const CKKSAnalyticsAttributeErrorCode = @"errorCode";
static NSString* const CKKSAnalyticsAttributeErrorChain = @"errorChain";

CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassA";
CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessIncomingQueueClassC";
CKKSAnalyticsFailableEvent* const CKKSEventProcessOutgoingQueue = (CKKSAnalyticsFailableEvent*)@"CKKSEventProcessOutgoingQueue";
CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges = (CKKSAnalyticsFailableEvent*)@"CKKSEventUploadChanges";
CKKSAnalyticsFailableEvent* const CKKSEventStateError = (CKKSAnalyticsFailableEvent*)@"CKKSEventStateError";
CKKSAnalyticsFailableEvent* const CKKSEventProcessHealKeyHierarchy = (CKKSAnalyticsFailableEvent *)@"CKKSEventProcessHealKeyHierarchy";
CKKSAnalyticsFailableEvent* const CKKSEventProcessReencryption = (CKKSAnalyticsFailableEvent *)@"CKKSEventProcessReencryption";

NSString* const OctagonEventFailureReason = @"FailureReason";

CKKSAnalyticsFailableEvent* const OctagonEventPreflightBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightBottle";
CKKSAnalyticsFailableEvent* const OctagonEventLaunchBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventLaunchBottle";
CKKSAnalyticsFailableEvent* const OctagonEventRestoreBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventRestoreBottle";
CKKSAnalyticsFailableEvent* const OctagonEventScrubBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventScrubBottle";
CKKSAnalyticsFailableEvent* const OctagonEventSignIn = (CKKSAnalyticsFailableEvent *)@"OctagonEventSignIn";
CKKSAnalyticsFailableEvent* const OctagonEventSignOut = (CKKSAnalyticsFailableEvent *)@"OctagonEventSignOut";
CKKSAnalyticsFailableEvent* const OctagonEventRamp = (CKKSAnalyticsFailableEvent *)@"OctagonEventRamp";
CKKSAnalyticsFailableEvent* const OctagonEventBottleCheck = (CKKSAnalyticsFailableEvent *)@"OctagonEventBottleCheck";
CKKSAnalyticsFailableEvent* const OctagonEventCoreFollowUp = (CKKSAnalyticsFailableEvent *)@"OctagonEventCoreFollowUp";
CKKSAnalyticsFailableEvent* const OctagonEventUpdateBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpdateBottle";

CKKSAnalyticsFailableEvent* const OctagonEventCheckTrustState = (CKKSAnalyticsFailableEvent *)@"OctagonEventCheckTrustState";

CKKSAnalyticsFailableEvent* const OctagonEventBottledPeerRestore = (CKKSAnalyticsFailableEvent*)@"OctagonEventBottledPeerRestore";
CKKSAnalyticsFailableEvent* const OctagonEventRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventRecoveryKey";

CKKSAnalyticsFailableEvent* const OctagonEventCreateCustodianRecoveryKey2 = (CKKSAnalyticsFailableEvent*)@"OctagonEventCreateCustodianRecoveryKey2";
CKKSAnalyticsFailableEvent* const OctagonEventJoinCustodianRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinCustodianRecoveryKey";
CKKSAnalyticsFailableEvent* const OctagonEventPreflightCustodianRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightCustodianRecoveryKey";
CKKSAnalyticsFailableEvent* const OctagonEventRemoveCustodianRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventRemoveCustodianRecoveryKey";
CKKSAnalyticsFailableEvent* const OctagonEventCheckCustodianRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventCheckCustodianRecoveryKey";

CKKSAnalyticsFailableEvent* const OctagonEventCreateInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventCreateInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventGenerateInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventGenerateInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventStoreInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventStoreInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventJoinInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventPreflightInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventRemoveInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventRemoveInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventCheckInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventCheckInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventRecreateInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventRecreateInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventCreateInheritanceKeyWithClaimTokenAndWrappingKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventCreateInheritanceKeyWithClaimTokenAndWrappingKey";
CKKSAnalyticsFailableEvent* const OctagonEventReroll = (CKKSAnalyticsFailableEvent*)@"OctagonEventReroll";

CKKSAnalyticsFailableEvent* const OctagonEventFetchAllBottles = (CKKSAnalyticsFailableEvent*)@"OctagonEventFetchAllBottles";
CKKSAnalyticsFailableEvent* const OctagonEventFetchEscrowContents = (CKKSAnalyticsFailableEvent*)@"OctagonEventFetchEscrowContents";
CKKSAnalyticsFailableEvent* const OctagonEventResetAndEstablish = (CKKSAnalyticsFailableEvent*)@"OctagonEventResetAndEstablish";
CKKSAnalyticsFailableEvent* const OctagonEventEstablish = (CKKSAnalyticsFailableEvent*)@"OctagonEventEstablish";
CKKSAnalyticsFailableEvent* const OctagonEventLeaveClique = (CKKSAnalyticsFailableEvent*)@"OctagonEventLeaveClique";
CKKSAnalyticsFailableEvent* const OctagonEventRemoveFriendsInClique = (CKKSAnalyticsFailableEvent*)@"OctagonEventRemoveFriendsInClique";

CKKSAnalyticsFailableEvent* const OctagonEventUpgradeFetchDeviceIDs = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradeFetchDeviceIDs";
CKKSAnalyticsFailableEvent* const OctagonEventUpgradeSetAllowList = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradeSetAllowList";
CKKSAnalyticsFailableEvent* const OctagonEventUpgradeSilentEscrow = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradeSilentEscrow";
CKKSAnalyticsFailableEvent* const OctagonEventUpgradePreapprovedJoin = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradePreapprovedJoin";
CKKSAnalyticsFailableEvent* const OctagonEventUpgradePreflightPreapprovedJoin = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradePreflightPreapprovedJoin";
CKKSAnalyticsFailableEvent* const OctagonEventUpgradePreapprovedJoinAfterPairing = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradePreapprovedJoinAfterPairing";
CKKSAnalyticsFailableEvent* const OctagonEventUpgradePrepare = (CKKSAnalyticsFailableEvent*)@"OctagonEventUpgradePrepare";

CKKSAnalyticsFailableEvent* const OctagonEventJoinWithVoucher = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinWithVoucher";

CKKSAnalyticsFailableEvent* const OctagonEventPreflightVouchWithBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightVouchWithBottle";
CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithBottle = (CKKSAnalyticsFailableEvent*)@"OctagonEventVoucherWithBottle";

CKKSAnalyticsFailableEvent* const OctagonEventPreflightVouchWithRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightVouchWithRecoveryKey";
CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventVoucherWithRecoveryKey";

CKKSAnalyticsFailableEvent* const OctagonEventPreflightVouchWithCustodianRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightVouchWithCustodianRecoveryKey";
CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithCustodianRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventVoucherWithCustodianRecoveryKey";

CKKSAnalyticsFailableEvent* const OctagonEventPreflightVouchWithInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventPreflightVouchWithInheritanceKey";
CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithInheritanceKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventVoucherWithInheritanceKey";

CKKSAnalyticsFailableEvent* const OctagonEventVoucherWithReroll = (CKKSAnalyticsFailableEvent*)@"OctagonEventVoucherWithReroll";

CKKSAnalyticsFailableEvent* const OctagonEventSetRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventSetRecoveryKey";

CKKSAnalyticsFailableEvent* const OctagonEventSetRecoveryKeyValidationFailed = (CKKSAnalyticsFailableEvent*)@"OctagonEventSetRecoveryKeyValidationFailed";
CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyValidationFailed = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinRecoveryKeyValidationFailed";
CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyCircleReset = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinRecoveryKeyCircleReset";
CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyCircleResetFailed = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinRecoveryKeyCircleResetFailed";
CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyEnrollFailed = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinRecoveryKeyEnrollFailed";
CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKeyFailed = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinRecoveryKeyFailed";
CKKSAnalyticsFailableEvent* const OctagonEventJoinRecoveryKey = (CKKSAnalyticsFailableEvent*)@"OctagonEventJoinRecoveryKey";

CKKSAnalyticsFailableEvent* const OctagonEventCreateCustodianRecoveryKeyTPH = (CKKSAnalyticsFailableEvent*)@"OctagonEventCreateCustodianRecoveryKeyTPH";
CKKSAnalyticsFailableEvent* const OctagonEventRemoveCustodianRecoveryKeyTPH = (CKKSAnalyticsFailableEvent*)@"OctagonEventRemoveCustodianRecoveryKeyTPH";
CKKSAnalyticsFailableEvent* const OctagonEventCheckCustodianRecoveryKeyTPH = (CKKSAnalyticsFailableEvent*)@"OctagonEventCheckCustodianRecoveryKeyTPH";

CKKSAnalyticsFailableEvent* const OctagonEventReset = (CKKSAnalyticsFailableEvent*)@"OctagonEventReset";

CKKSAnalyticsFailableEvent* const OctagonEventPrepareIdentity = (CKKSAnalyticsFailableEvent*)@"OctagonEventPrepareIdentity";

CKKSAnalyticsFailableEvent* const OctagonEventEstablishIdentity = (CKKSAnalyticsFailableEvent*)@"OctagonEventEstablishIdentity";
CKKSAnalyticsFailableEvent* const OctagonEventFetchViews = (CKKSAnalyticsFailableEvent*)@"OctagonEventFetchViews";

CKKSAnalyticsFailableEvent* const OctagonEventStateTransition = (CKKSAnalyticsFailableEvent*)@"OctagonEventStateTransition";

CKKSAnalyticsFailableEvent* const OctagonEventCompanionPairing = (CKKSAnalyticsFailableEvent*)@"OctagonEventCompanionPairing";

CKKSAnalyticsFailableEvent* const OctagonEventCheckTrustForCFU = (CKKSAnalyticsFailableEvent*)@"OctagonEventCheckTrustForCFU";

CKKSAnalyticsSignpostEvent* const CKKSEventPushNotificationReceived = (CKKSAnalyticsSignpostEvent*)@"CKKSEventPushNotificationReceived";
CKKSAnalyticsSignpostEvent* const CKKSEventItemAddedToOutgoingQueue = (CKKSAnalyticsSignpostEvent*)@"CKKSEventItemAddedToOutgoingQueue";
CKKSAnalyticsSignpostEvent* const CKKSEventMissingLocalItemsFound = (CKKSAnalyticsSignpostEvent*)@"CKKSEventMissingLocalItemsFound";
CKKSAnalyticsSignpostEvent* const CKKSEventReachabilityTimerExpired = (CKKSAnalyticsSignpostEvent *)@"CKKSEventReachabilityTimerExpired";

CKKSAnalyticsFailableEvent* const OctagonEventTPHHealthCheckStatus = (CKKSAnalyticsFailableEvent*)@"OctagonEventTPHHealthCheckStatus";

CKKSAnalyticsFailableEvent* const OctagonEventAuthKitDeviceList = (CKKSAnalyticsFailableEvent *)@"OctagonEventAuthKitDeviceList";

CKKSAnalyticsFailableEvent* const OctagonEventEscrowMoveTriggered = (CKKSAnalyticsFailableEvent *)@"OctagonEventEscrowMoveTriggered";
CKKSAnalyticsFailableEvent* const OctagonEventEscrowMoveRateLimited = (CKKSAnalyticsFailableEvent *)@"OctagonEventEscrowMoveRateLimited";
CKKSAnalyticsFailableEvent* const OctagonEventEscrowMoveTermsNeeded = (CKKSAnalyticsFailableEvent *)@"OctagonEventEscrowMoveTermsNeeded";

CKKSAnalyticsActivity* const CKKSActivityOTFetchRampState = (CKKSAnalyticsActivity *)@"CKKSActivityOTFetchRampState";
CKKSAnalyticsActivity* const CKKSActivityOctagonPreflightBottle = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonPreflightBottle";
CKKSAnalyticsActivity* const CKKSActivityOctagonLaunchBottle = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonLaunchBottle";
CKKSAnalyticsActivity* const CKKSActivityOctagonRestore = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonRestore";
CKKSAnalyticsActivity* const CKKSActivityScrubBottle = (CKKSAnalyticsActivity *)@"CKKSActivityScrubBottle";
CKKSAnalyticsActivity* const CKKSActivityBottleCheck = (CKKSAnalyticsActivity *)@"CKKSActivityBottleCheck";
CKKSAnalyticsActivity* const CKKSActivityOctagonUpdateBottle = (CKKSAnalyticsActivity *)@"CKKSActivityOctagonUpdateBottle";

CKKSAnalyticsActivity* const OctagonActivityAccountAvailable = (CKKSAnalyticsActivity *)@"OctagonActivityAccountAvailable";
CKKSAnalyticsActivity* const OctagonActivityAccountNotAvailable = (CKKSAnalyticsActivity *)@"OctagonActivityAccountNotAvailable";
CKKSAnalyticsActivity* const OctagonActivityResetAndEstablish = (CKKSAnalyticsActivity *)@"OctagonActivityResetAndEstablish";
CKKSAnalyticsActivity* const OctagonActivityEstablish = (CKKSAnalyticsActivity *)@"OctagonActivityEstablish";
CKKSAnalyticsActivity* const OctagonSOSAdapterUpdateKeys = (CKKSAnalyticsActivity*)@"OctagonSOSAdapterUpdateKeys";

CKKSAnalyticsActivity* const OctagonActivityFetchAllViableBottles = (CKKSAnalyticsActivity *)@"OctagonActivityFetchAllViableBottles";
CKKSAnalyticsActivity* const OctagonActivityFetchEscrowContents = (CKKSAnalyticsActivity *)@"OctagonActivityFetchEscrowContents";
CKKSAnalyticsActivity* const OctagonActivityBottledPeerRestore = (CKKSAnalyticsActivity *)@"OctagonActivityBottledPeerRestore";
CKKSAnalyticsActivity* const OctagonActivitySetRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivitySetRecoveryKey";
CKKSAnalyticsActivity* const OctagonActivityCreateCustodianRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivityCreateCustodianRecoveryKey";
CKKSAnalyticsActivity* const OctagonActivityJoinWithRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivityJoinWithRecoveryKey";
CKKSAnalyticsActivity* const OctagonActivityJoinWithCustodianRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivityJoinWithCustodianRecoveryKey";
CKKSAnalyticsActivity* const OctagonActivityPreflightJoinWithCustodianRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivityPreflightJoinWithCustodianRecoveryKey";
CKKSAnalyticsActivity* const OctagonActivityRemoveCustodianRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivityRemoveCustodianRecoveryKey";
CKKSAnalyticsActivity* const OctagonActivityCheckCustodianRecoveryKey = (CKKSAnalyticsActivity *)@"OctagonActivityCheckCustodianRecoveryKey";

CKKSAnalyticsActivity* const OctagonActivityCreateInheritanceKey = (CKKSAnalyticsActivity *)@"OctagonActivityCreateInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityGenerateInheritanceKey = (CKKSAnalyticsActivity *)@"OctagonActivityGenerateInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityStoreInheritanceKey = (CKKSAnalyticsActivity *)@"OctagonActivityStoreInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityJoinWithInheritanceKey = (CKKSAnalyticsActivity*)@"OctagonActivityJoinWithInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityPreflightJoinWithInheritanceKey = (CKKSAnalyticsActivity*)@"OctagonActivityPreflightJoinWithInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityRemoveInheritanceKey = (CKKSAnalyticsActivity *)@"OctagonActivityRemoveInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityCheckInheritanceKey = (CKKSAnalyticsActivity *)@"OctagonActivityCheckInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityRecreateInheritanceKey = (CKKSAnalyticsActivity *)@"OctagonActivityRecreateInheritanceKey";
CKKSAnalyticsActivity* const OctagonActivityReroll = (CKKSAnalyticsActivity*)@"OctagonActivityReroll";

CKKSAnalyticsActivity* const OctagonActivityLeaveClique = (CKKSAnalyticsActivity *)@"OctagonActivityLeaveClique";
CKKSAnalyticsActivity* const OctagonActivityRemoveFriendsInClique = (CKKSAnalyticsActivity *)@"OctagonActivityRemoveFriendsInClique";


// SOS Deferral

CKKSAnalyticsActivity* const SOSDeferralActivitySetCompatibilityMode = (CKKSAnalyticsActivity *)@"SOSDeferralActivitySetCompatibilityMode";
CKKSAnalyticsFailableEvent* const SOSDeferralEventCompatibilityModeEnabled = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventCompatibilityModeEnabled";
CKKSAnalyticsFailableEvent* const SOSDeferralEventCompatibilityModeDisabled = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventCompatibilityModeDisabled";

CKKSAnalyticsFailableEvent* const SOSDeferralEventJoinCircleAfterRestore = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventJoinCircleAfterRestore";
CKKSAnalyticsFailableEvent* const SOSDeferralEventResetToOffering = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventResetToOffering";
CKKSAnalyticsFailableEvent* const SOSDeferralEventReset = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventReset";
CKKSAnalyticsFailableEvent* const SOSDeferralEventPiggybacking = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventPiggybacking";
CKKSAnalyticsFailableEvent* const SOSDeferralEventPairing = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventPairing";
CKKSAnalyticsFailableEvent* const SOSDeferralEventFetchCompatibilityMode = (CKKSAnalyticsFailableEvent *)@"SOSDeferralEventFetchCompatibilityMode";

NSString* const SOSDeferralAnalyticsSOSEnabled = @"SOSEnabled";
NSString* const SOSDeferralAnalyticsJoiningSOSResult = @"JoiningSOSResult";
NSString* const SOSDeferralAnalyticsCircleContainsLegacy = @"CircleContainsLegacy";
NSString* const SOSDeferralAnalyticsSOSJoinMethod = @"SOSJoinMethod";

NSString* const SOSDeferralAnalyticsReset = @"Reset";
NSString* const SOSDeferralAnalyticsPiggybacking = @"Piggybacking";
NSString* const SOSDeferralAnalyticsPairing = @"Pairing";
NSString* const SOSDeferralAnalyticsRestore = @"Restore";
NSString* const SOSDeferralAnalyticsResetToOffering = @"ResetToOffering";

@implementation CKKSAnalytics

+ (NSString*)databasePath
{
    // This block exists because we moved database locations in 11.3 for easier sandboxing of securityuploadd, so we're cleaning up.
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        WithPathInKeychainDirectory(CFSTR("ckks_analytics_v2.db"), ^(const char *filename) {
            remove(filename);
        });
        WithPathInKeychainDirectory(CFSTR("ckks_analytics_v2.db-wal"), ^(const char *filename) {
            remove(filename);
        });
        WithPathInKeychainDirectory(CFSTR("ckks_analytics_v2.db-shm"), ^(const char *filename) {
            remove(filename);
        });
    });
#if TARGET_OS_OSX
    return [CKKSAnalytics defaultProtectedAnalyticsDatabasePath:@"ckks_analytics"];
#else
    return [CKKSAnalytics defaultAnalyticsDatabasePath:@"ckks_analytics"];
#endif
}

+ (instancetype)logger
{
    // just here because I want it in the header for discoverability
    return [super logger];
}

- (void)logSuccessForEvent:(CKKSAnalyticsFailableEvent*)event zoneName:(NSString*)zoneName
{
    [self logSuccessForEventNamed:[NSString stringWithFormat:@"%@-%@", zoneName, event]];
    [self setDateProperty:[NSDate date] forKey:[NSString stringWithFormat:@"last_success_%@-%@", zoneName, event]];
}

- (bool)isCKPartialError:(NSError *)error
{
    return [error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorPartialFailure;
}

- (void)addCKPartialError:(NSMutableDictionary *)errorDictionary error:(NSError *)error depth:(NSUInteger)depth
{
    // capture one random underlaying error
    if ([self isCKPartialError:error]) {
        NSDictionary<NSString *,NSError *> *partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];
        if ([partialErrors isKindOfClass:[NSDictionary class]]) {
            for (NSString *key in partialErrors) {
                NSError* ckError = partialErrors[key];
                if (![ckError isKindOfClass:[NSError class]])
                    continue;
                if ([ckError.domain isEqualToString:CKErrorDomain] && ckError.code == CKErrorBatchRequestFailed) {
                    continue;
                }
                NSDictionary *res =  [self errorChain:ckError depth:(depth + 1)];
                if (res) {
                    errorDictionary[@"oneCloudKitPartialFailure"] = res;
                    break;
                }
            }
        }
    }
}

// if we have underlying errors, capture the chain below the top-most error
- (NSDictionary *)errorChain:(NSError *)error depth:(NSUInteger)depth
{
    NSMutableDictionary *errorDictionary = nil;

    if (depth > 5 || ![error isKindOfClass:[NSError class]])
        return nil;

    errorDictionary = [@{
       @"domain" : error.domain,
       @"code" : @(error.code),
    } mutableCopy];

    errorDictionary[@"child"] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:(depth + 1)];
    [self addCKPartialError:errorDictionary error:error depth:(depth + 1)];

    return errorDictionary;
}

- (NSDictionary *)createErrorAttributes:(NSError *)error
                                  depth:(NSUInteger)depth
                             attributes:(NSDictionary *)attributes
{
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      CKKSAnalyticsAttributeRecoverableError : @(YES),
                                                      CKKSAnalyticsAttributeErrorDomain : error.domain,
                                                      CKKSAnalyticsAttributeErrorCode : @(error.code)
                                                      }];

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    return eventAttributes;
}

- (void)logRecoverableError:(NSError*)error
                   forEvent:(CKKSAnalyticsFailableEvent*)event
                   zoneName:(NSString*)zoneName
             withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];
    
    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }
    
    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      CKKSAnalyticsAttributeRecoverableError : @(YES),
                                                      CKKSAnalyticsAttributeZoneName : zoneName,
                                                      CKKSAnalyticsAttributeErrorDomain : error.domain,
                                                      CKKSAnalyticsAttributeErrorCode : @(error.code)
                                                      }];
    
    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];
    
    [super logSoftFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)logRecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      CKKSAnalyticsAttributeRecoverableError : @(YES),
                                                      CKKSAnalyticsAttributeErrorDomain : error.domain,
                                                      CKKSAnalyticsAttributeErrorCode : @(error.code)
                                                      }];

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    [super logSoftFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)logUnrecoverableError:(NSError*)error
                     forEvent:(CKKSAnalyticsFailableEvent*)event
                     zoneName:(NSString*)zoneName
               withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    [eventAttributes setValuesForKeysWithDictionary:@{
        CKKSAnalyticsAttributeRecoverableError : @(NO),
        CKKSAnalyticsAttributeZoneName : zoneName,
        CKKSAnalyticsAttributeErrorDomain : error.domain,
        CKKSAnalyticsAttributeErrorCode : @(error.code)
    }];

    [self logHardFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)logUnrecoverableError:(NSError*)error
                     forEvent:(CKKSAnalyticsFailableEvent*)event
               withAttributes:(NSDictionary *)attributes
{
    if (error == nil){
        return;
    }
    NSMutableDictionary* eventAttributes = [NSMutableDictionary dictionary];

    /* Don't allow caller to overwrite our attributes, lets merge them first */
    if (attributes) {
        [eventAttributes setValuesForKeysWithDictionary:attributes];
    }

    eventAttributes[CKKSAnalyticsAttributeErrorChain] = [self errorChain:error.userInfo[NSUnderlyingErrorKey] depth:0];
    [self addCKPartialError:eventAttributes error:error depth:0];

    [eventAttributes setValuesForKeysWithDictionary:@{
                                                      CKKSAnalyticsAttributeRecoverableError : @(NO),
                                                      CKKSAnalyticsAttributeZoneName : OctagonEventAttributeZoneName,
                                                      CKKSAnalyticsAttributeErrorDomain : error.domain,
                                                      CKKSAnalyticsAttributeErrorCode : @(error.code)
                                                      }];

    [self logHardFailureForEventNamed:event withAttributes:eventAttributes];
}

- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event
{
    [self noteEventNamed:event];
}
- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event zoneName:(NSString*)zoneName
{
    [self noteEventNamed:[NSString stringWithFormat:@"%@-%@", zoneName, event]];
}

- (NSDate*)dateOfLastSuccessForEvent:(CKKSAnalyticsFailableEvent*)event zoneName:(NSString*)zoneName
{
    return [self datePropertyForKey:[NSString stringWithFormat:@"last_success_%@-%@", zoneName, event]];
}

- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key zoneName:(NSString*)zoneName
{
    [self setDateProperty:date forKey:[NSString stringWithFormat:@"%@-%@", key, zoneName]];
}
- (NSDate *)datePropertyForKey:(NSString *)key zoneName:(NSString*)zoneName
{
    return [self datePropertyForKey:[NSString stringWithFormat:@"%@-%@", key, zoneName]];
}

#pragma mark - Helper methods for clients

- (void)noteMetric:(NSString*)metric count:(int64_t)count
{
    NSString* metricName = [NSString stringWithFormat:@"%@%lld", metric, count];

    [self logResultForEvent:metricName
                hardFailure:NO
                     result:nil];

    [self setDateProperty:[NSDate date] forKey:metricName];
    [self setNumberProperty:[[NSNumber alloc]initWithLong:(long)count] forKey:metric];
}

- (void)recordRecoveredTLKMetrics:(NSArray<CKKSTLKShare*>*)possibleTLKShares
               tlkRecoveryResults:(TrustedPeersHelperTLKRecoveryResult*)tlkRecoveryResults
         uniqueTLKsRecoveredEvent:(NSString*)uniqueTLKsRecoveredEvent
        totalSharesRecoveredEvent:(NSString*)totalSharesRecoveredEvent
   totalRecoverableTLKSharesEvent:(NSString*)totalRecoverableTLKSharesEvent
        totalRecoverableTLKsEvent:(NSString*)totalRecoverableTLKsEvent
        totalViewsWithSharesEvent:(NSString*)totalViewsWithSharesEvent
{
    [self noteMetric:uniqueTLKsRecoveredEvent count:tlkRecoveryResults.successfulKeysRecovered.count];
    [self noteMetric:totalSharesRecoveredEvent count:tlkRecoveryResults.totalTLKSharesRecovered];

    [self noteMetric:totalRecoverableTLKsEvent count:possibleTLKShares.count];

    NSMutableSet<NSString*>* uniqueTLKsWithShares = [NSMutableSet set];
    for (CKKSTLKShare* share in possibleTLKShares) {
        [uniqueTLKsWithShares addObject:share.tlkUUID];
    }

    [self noteMetric:totalRecoverableTLKsEvent count:uniqueTLKsWithShares.count];

    NSMutableDictionary *views = [NSMutableDictionary dictionary];
    for (CKKSTLKShare *share in possibleTLKShares) {
        views[share.zoneID] = share.zoneID;
    }
    [self noteMetric:totalViewsWithSharesEvent count:views.count];
}

@end

#endif // OCTAGON
