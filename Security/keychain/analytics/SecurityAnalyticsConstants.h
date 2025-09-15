/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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

#ifndef SecurityAnalyticsConstants_h
#define SecurityAnalyticsConstants_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSNumber *const kSecurityRTCClientType;
extern NSString *const kSecurityRTCClientName;
extern NSString *const kSecurityRTCClientNameDNU;
extern NSNumber *const kSecurityRTCEventCategoryAccountDataAccessRecovery;
extern NSString *const kSecurityRTCClientBundleIdentifier;

// MARK: RTC Event Names

extern NSString *const kSecurityRTCEventNamePrimaryAccountAdded;
extern NSString *const kSecurityRTCEventNameIdMSSecurityLevel;
extern NSString *const kSecurityRTCEventNameCloudKitAccountAvailability;
extern NSString *const kSecurityRTCEventNameInitiatorCreatesPacket1; // sends which trust system it supports
extern NSString *const kSecurityRTCEventNameAcceptorCreatesPacket2; // sends epoch
extern NSString *const kSecurityRTCEventNameKVSSyncAndWait;
extern NSString *const kSecurityRTCEventNameFlush;
extern NSString *const kSecurityRTCEventNameValidatedStashedAccountCredential;
extern NSString *const kSecurityRTCEventNameInitiatorCreatesPacket3; // sends new identity
extern NSString *const kSecurityRTCEventNameFetchMachineID;
extern NSString *const kSecurityRTCEventNamePrepareIdentityInTPH;
extern NSString *const kSecurityRTCEventNameCreatesSOSApplication;
extern NSString *const kSecurityRTCEventNameAcceptorCreatesPacket4; // sends voucher
extern NSString *const kSecurityRTCEventNameVerifySOSApplication;
extern NSString *const kSecurityRTCEventNameCreateSOSCircleBlob;
extern NSString *const kSecurityRTCEventNameCKKSTlkFetch;
extern NSString *const kSecurityRTCEventNameUpdateTrust;
extern NSString *const kSecurityRTCEventNameInitiatorJoinsTrustSystems;
extern NSString *const kSecurityRTCEventNameInitiatorJoinsSOS;
extern NSString *const kSecurityRTCEventNameUpdateTDL;
extern NSString *const kSecurityRTCEventNameFetchAndPersistChanges;
extern NSString *const kSecurityRTCEventNameFetchPolicyDocument;
extern NSString *const kSecurityRTCEventNameJoinWithVoucherOperation;
extern NSString *const kSecurityRTCEventNameJoinWithVoucherInTPH;
extern NSString *const kSecurityRTCEventNameInitiatorWaitsForUpgrade;
extern NSString *const kSecurityRTCEventNamePreApprovedJoin;
extern NSString *const kSecurityRTCEventNameAcceptorCreatesPacket5; // sends initial sync data
extern NSString *const kSecurityRTCEventNameInitiatorImportsInitialSyncData;
extern NSString *const kSecurityRTCEventNameAcceptorCreatesVoucher;
extern NSString *const kSecurityRTCEventNameAcceptorFetchesInitialSyncData;
extern NSString *const kSecurityRTCEventNameNumberOfTrustedOctagonPeers;
extern NSString *const kSecurityRTCEventNameCliqueMemberIdentifier;
extern NSString *const kSecurityRTCEventNameDuplicateMachineID;
extern NSString *const kSecurityRTCEventNameMIDVanishedFromTDL;
extern NSString *const kSecurityRTCEventNameTDLProcessingSuccess;
extern NSString *const kSecurityRTCEventNameAllowedMIDHashMismatch;
extern NSString *const kSecurityRTCEventNameDeletedMIDHashMismatch;
extern NSString *const kSecurityRTCEventNameTrustedDeviceListFailure;
extern NSString *const kSecurityRTCEventNamePairingDidNotReceivePCSData;
extern NSString *const kSecurityRTCEventNamePairingFailedToAddItemToKeychain;
extern NSString *const kSecurityRTCEventNamePairingImportKeychainResults;
extern NSString *const kSecurityRTCEventNamePairingFailedToUpdateItemInKeychain;
extern NSString *const kSecurityRTCEventNamePairingFailedFetchPCSItems;
extern NSString *const kSecurityRTCEventNamePairingEmptyOctagonPayload;
extern NSString *const kSecurityRTCEventNamePairingEmptyAckPayload;
extern NSString *const kSecurityRTCEventNameRPDDeleteAllRecords;
extern NSString *const kSecurityRTCEventNamePairingFailedToFetchItemForPersistentRef;
extern NSString *const kSecurityRTCEventNamePiggybackingAcceptorInitialMessage;
extern NSString *const kSecurityRTCEventNamePiggybackingAcceptorProcessMessage;
extern NSString *const kSecurityRTCEventNamePiggybackingAcceptorProcessApplication;
extern NSString *const kSecurityRTCEventNamePiggybackingSessionInitiatorInitialMessage;
extern NSString *const kSecurityRTCEventNamePiggybackingSessionInitiatorHandleChallenge;
extern NSString *const kSecurityRTCEventNamePiggybackingSessionInitiatorHandleVerification;
extern NSString *const kSecurityRTCEventNamePiggybackingCircleInitiatorInitialMessage;
extern NSString *const kSecurityRTCEventNamePiggybackingCircleInitiatorHandleCircleBlobMessage;
extern NSString *const kSecurityRTCEventNamePiggybackingInitiatorPreVersion3Change;
extern NSString *const kSecurityRTCEventNamePiggybackingAcceptorPreVersion3Change;
extern NSString *const kSecurityRTCEventNamePiggybackingInitiatorChannelSecured;
extern NSString *const kSecurityRTCEventNamePiggybackingAcceptorChannelSecured;

// MARK: Escrow Recovery Events
extern NSString *const kSecurityRTCEventNamePerformSilentEscrowRecovery;
extern NSString *const kSecurityRTCEventNameRecoverSilentWithCDPContext;
extern NSString *const kSecurityRTCEventNameRecoverWithInfo;
extern NSString *const kSecurityRTCEventNameHandleRecoveryResults;
extern NSString *const kSecurityRTCEventNameRestoreFromBottleEvent;
extern NSString *const kSecurityRTCEventNameHandleRecoveryResultsResetAndEstablish;
extern NSString *const kSecurityRTCEventNameRestoreKeychainAsyncWithPassword;
extern NSString *const kSecurityRTCEventNameVouchWithBottle;
extern NSString *const kSecurityRTCEventNamePreflightVouchWithBottle;
extern NSString *const kSecurityRTCEventNameFetchRecoverableTLKShares;
extern NSString *const kSecurityRTCEventNameVouchWithBottleTPH;
extern NSString *const kSecurityRTCEventNamePerformEscrowRecovery;
extern NSString *const kSecurityRTCEventNameRecoverWithCDPContext;

// MARK: Account Creation
extern NSString *const kSecurityRTCEventNameEstablish;
extern NSString *const kSecurityRTCEventNameEstablishOperation;
extern NSString *const kSecurityRTCEventNameOnqueueEstablishTPH;
extern NSString *const kSecurityRTCEventNameFetchAfterEstablish;

// MARK: Reset Protected Data
extern NSString *const kSecurityRTCEventNameResetProtectedData;
extern NSString *const kSecurityRTCEventNameOTResetOperation;
extern NSString *const kSecurityRTCEventNameFetchAccountWideSettings;
extern NSString *const kSecurityRTCEventNameFetchAccountWideSettingsTPH;
extern NSString *const kSecurityRTCEventNameResetSOS;
extern NSString *const kSecurityRTCEventNameResetTPH;

// MARK: Establish Path
extern NSString *const kSecurityRTCEventNameResetCKKSZonesLackingTLKsOperation;
extern NSString *const kSecurityRTCEventNameOTSetCDPBitOperation;
extern NSString *const kSecurityRTCEventNameOTTriggerEscrowUpdateOperation;
extern NSString *const kSecurityRTCEventNameOTBecomeReadyOperation;
extern NSString *const kSecurityRTCEventNameOTLocalCKKSResetOperation;

// MARK: iCSC reliability
extern NSString *const kSecurityRTCEventNameEscrowPasscodeEnableCacheFlow;
extern NSString *const kSecurityRTCEventNameEscrowPasscodeCacheAvailable;
extern NSString *const kSecurityRTCEventNameEscrowRepairOperation;

// MARK: RPD
extern NSString *const kSecurityRTCEventNamePerformCKServerUnreadableDataRemoval;
extern NSString *const kSecurityRTCEventNamePerformCKServerUnreadableDataRemovalTPH;

// MARK: 2FAFA RPD
extern NSString *const kSecurityRTCEventNameClearCliqueFromAccount;

extern NSString *const kSecurityRTCErrorDomain;

typedef NS_ERROR_ENUM(kSecurityRTCErrorDomain, OctagonTrustDepartureReasonErrorCode) {
    OctagonTrustDepartureReasonErrorCodeUnknown,
    OctagonTrustDepartureReasonErrorCodeTDLUserInitiated,
    OctagonTrustDepartureReasonErrorCodeTDLEvicted,
    OctagonTrustDepartureReasonErrorCodeTDLUnknown,
    OctagonTrustDepartureReasonErrorCodeTDLGhost,
    OctagonTrustDepartureReasonErrorCodeTDLMidRoll,
    OctagonTrustDepartureReasonErrorCodeLeaveClique,
    OctagonTrustDepartureReasonErrorCodeCLI,
    OctagonTrustDepartureReasonErrorCodeRPD,
    OctagonTrustDepartureReasonErrorCodeResetRecipient,
    OctagonTrustDepartureReasonErrorCodePeerDistrust,
    OctagonTrustDepartureReasonErrorCodeCFDeletedEgo,
};

extern NSString *const kSecurityRTCEventNameOctagonTrustLost;

// MARK: RTC Fields

extern NSString *const kSecurityRTCFieldSupportedTrustSystem;
extern NSString *const kSecurityRTCFieldEventName;
extern NSString *const kSecurityRTCFieldDidSucceed;
extern NSString *const kSecurityRTCFieldNumberOfTLKsFetched;
extern NSString *const kSecurityRTCFieldNumberOfPCSItemsFetched;
extern NSString *const kSecurityRTCFieldNumberOfBluetoothMigrationItemsFetched;
extern NSString *const kSecurityRTCFieldOctagonSignInResult;
extern NSString *const kSecurityRTCFieldNumberOfKeychainItemsCollected;
extern NSString *const kSecurityRTCFieldNumberOfKeychainItemsAdded;
extern NSString *const kSecurityRTCFieldNumberOfTrustedPeers;
extern NSString *const kSecurityRTCFieldSecurityLevel;
extern NSString *const kSecurityRTCFieldRetryAttemptCount;
extern NSString *const kSecurityRTCFieldTotalRetryDuration;
extern NSString *const kSecurityRTCFieldPairingSuccessfulImportCount;
extern NSString *const kSecurityRTCFieldPairingFailedImportCount;

extern NSString *const kSecurityRTCEventNameLaunchStart;
extern NSString *const kSecurityRTCEventNameSyncingPolicySet;
extern NSString *const kSecurityRTCEventNameCKAccountLogin;
extern NSString *const kSecurityRTCEventNameZoneChangeFetch;
extern NSString *const kSecurityRTCEventNameZoneCreation;
extern NSString *const kSecurityRTCEventNameTrustGain;
extern NSString *const kSecurityRTCEventNameTrustLoss;
extern NSString *const kSecurityRTCEventNameHealKeyHierarchy;
extern NSString *const kSecurityRTCEventNameHealBrokenRecords;
extern NSString *const kSecurityRTCEventNameUploadHealedTLKShares;
extern NSString *const kSecurityRTCEventNameHealTLKShares;
extern NSString *const kSecurityRTCEventNameCreateMissingTLKShares;
extern NSString *const kSecurityRTCEventNameUploadMissingTLKShares;
extern NSString *const kSecurityRTCEventNameProcessIncomingQueue;
extern NSString *const kSecurityRTCEventNameLoadAndProcessIQEs;
extern NSString *const kSecurityRTCEventNameFixMismatchedViewItems;
extern NSString *const kSecurityRTCEventNameProcessReceivedKeys;
extern NSString *const kSecurityRTCEventNameScanLocalItems;
extern NSString *const kSecurityRTCEventNameQuerySyncableItems;
extern NSString *const kSecurityRTCEventNameOnboardMissingItems;
extern NSString *const kSecurityRTCEventNameProcessOutgoingQueue;
extern NSString *const kSecurityRTCEventNameUploadOQEsToCK;
extern NSString *const kSecurityRTCEventNameSaveCKMirrorEntries;
extern NSString *const kSecurityRTCEventNameFirstManateeKeyFetch;
extern NSString *const kSecurityRTCEventNameLocalSyncFinish;
extern NSString *const kSecurityRTCEventNameContentSyncFinish;
extern NSString *const kSecurityRTCEventNameDeviceLocked;
extern NSString *const kSecurityRTCEventNameDeviceUnlocked;
extern NSString *const kSecurityRTCEventNameLocalReset;
extern NSString *const kSecurityRTCEventNameEvaluateTLKShares;

/* CKKS Initial Launch Fields */
extern NSString *const kSecurityRTCFieldNumViews;
extern NSString *const kSecurityRTCFieldTrustStatus;
extern NSString *const kSecurityRTCFieldSyncingPolicy;
extern NSString *const kSecurityRTCFieldPolicyFreshness;
extern NSString *const kSecurityRTCFieldItemsScanned;
extern NSString *const kSecurityRTCFieldNewItemsScanned;
extern NSString *const kSecurityRTCFieldFetchReasons;
extern NSString *const kSecurityRTCFieldFullFetch;
extern NSString *const kSecurityRTCFieldAvgRemoteKeys;
extern NSString *const kSecurityRTCFieldTotalRemoteKeys;
extern NSString *const kSecurityRTCFieldNewTLKShares;
extern NSString *const kSecurityRTCFieldIsPrioritized;
extern NSString *const kSecurityRTCFieldFullRefetchNeeded;
extern NSString *const kSecurityRTCFieldIsLocked;
extern NSString *const kSecurityRTCFieldMissingKey;
extern NSString *const kSecurityRTCFieldPendingClassA;
extern NSString *const kSecurityRTCFieldSuccessfulItemsProcessed;
extern NSString *const kSecurityRTCFieldErrorItemsProcessed;
extern NSString *const kSecurityRTCFieldAvgSuccessfulItemsProcessed;
extern NSString *const kSecurityRTCFieldAvgErrorItemsProcessed;
extern NSString *const kSecurityRTCFieldIsFullUpload;
extern NSString *const kSecurityRTCFieldPartialFailure;
extern NSString *const kSecurityRTCFieldItemsToAdd;
extern NSString *const kSecurityRTCFieldItemsToModify;
extern NSString *const kSecurityRTCFieldItemsToDelete;
extern NSString *const kSecurityRTCFieldNumMismatchedItems;
extern NSString *const kSecurityRTCFieldNumViewsWithNewEntries;
extern NSString *const kSecurityRTCFieldNeedsReencryption;
extern NSString *const kSecurityRTCFieldNumLocalRecords;
extern NSString *const kSecurityRTCFieldNumKeychainItems;
extern NSString *const kSecurityRTCFieldTotalCKRecords;
extern NSString *const kSecurityRTCFieldAvgCKRecords;
extern NSString *const kSecurityRTCFieldNumModificationsFetched;
extern NSString *const kSecurityRTCFieldNumDeletionsFetched;
extern NSString *const kSecurityRTCFieldNumZonesReverseSyncing;
extern NSString *const kSecurityRTCFieldPeersEvaluatedForTLKShares;
extern NSString *const kSecurityRTCFieldNumPeersMissingShares;
extern NSString *const kSecurityRTCFieldNumTLKSharesEvaluated;
extern NSString *const kSecurityRTCFieldIsCurrentDevice;

extern NSString *const kSecurityRTCFieldCKKSFetchBecauseAPNS;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseAPIFetchRequest;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseSEAPIFetchRequest;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseKeySetFetchRequest;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseCurrentItemFetchRequest;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseInitialStart;
extern NSString *const kSecurityRTCFieldCKKSFetchBecausePreviousFetchFailed;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseNetwork;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseKeyHierarchy;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseTesting;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseResync;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseMoreComing;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseResolvingConflict;
extern NSString *const kSecurityRTCFieldCKKSFetchBecausePeriodicRefetch;
extern NSString *const kSecurityRTCFieldCKKSFetchBecauseOctagonPairingComplete;

/* Escrow Recovery Fields */
extern NSString *const kSecurityRTCFieldRecordDataMissing;
extern NSString *const kSecurityRTCFieldMissingDigest;
extern NSString *const kSecurityRTCFieldMissingPassword;

/* Join metrics */
extern NSString *const kSecurityRTCFieldTotalNumberOfPeers;
extern NSString *const kSecurityRTCFieldTotalNumberOfRecoveryKeys;
extern NSString *const kSecurityRTCFieldTotalNumberOfDistrustedRecoveryKeys;
extern NSString *const kSecurityRTCFieldTotalNumberOfCustodians;
extern NSString *const kSecurityRTCFieldTotalNumberOfTrustedRecoveryKeys;
extern NSString *const kSecurityRTCFieldTotalNumberOfTrustedCustodians;
extern NSString *const kSecurityRTCFieldTotalNumberOfPreapprovals;

/* Reset Protected Data Fields */
extern NSString *const kSecurityRTCFieldAccountIsW;
extern NSString *const kSecurityRTCFieldAccountIsG;

NS_ASSUME_NONNULL_END


#endif /* SecurityAnalyticsConstants_h */
