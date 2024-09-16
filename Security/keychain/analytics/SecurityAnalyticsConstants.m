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

#import "SecurityAnalyticsConstants.h"

NSNumber *const kSecurityRTCClientType = @(38);
NSString *const kSecurityRTCClientName = @"com.apple.aaa";
NSString *const kSecurityRTCClientNameDNU = @"com.apple.aaa.dnu";
NSNumber *const kSecurityRTCEventCategoryAccountDataAccessRecovery = @(10000);
NSString *const kSecurityRTCClientBundleIdentifier = @"com.apple.securityd";

// MARK: RTC Event Names
// NOTE: Adding new metrics encapsulating cuttlefish calls must match their function name.  Ex: self.cuttlefish.updateTrust
// should use "com.apple.security.updateTrust".  self.cuttlefish.joinWithVoucher should use com.apple.security.joinWithVoucher
// ContainerMap.swift will track metric events based on the functionName to make things simplier at that layer

NSString *const kSecurityRTCEventNamePrimaryAccountAdded = @"com.apple.security.primaryAccountAdded";
NSString *const kSecurityRTCEventNameIdMSSecurityLevel = @"com.apple.security.idMSSecurityLevel";
NSString *const kSecurityRTCEventNameCloudKitAccountAvailability = @"com.apple.security.cloudKitAccountAvailability";
NSString *const kSecurityRTCEventNameInitiatorCreatesPacket1 = @"com.apple.security.initiatorCreatesPacket1";
NSString *const kSecurityRTCEventNameAcceptorCreatesPacket2 = @"com.apple.security.acceptorCreatesPacket2";
NSString *const kSecurityRTCEventNameKVSSyncAndWait = @"com.apple.security.kVSSyncAndWait";
NSString *const kSecurityRTCEventNameFlush = @"com.apple.security.flush";
NSString *const kSecurityRTCEventNameValidatedStashedAccountCredential = @"com.apple.security.validatedStashedAccountCredential";
NSString *const kSecurityRTCEventNameInitiatorCreatesPacket3 = @"com.apple.security.initiatorCreatesPacket3";
NSString *const kSecurityRTCEventNameFetchMachineID = @"com.apple.security.fetchMachineID";
NSString *const kSecurityRTCEventNamePrepareIdentityInTPH = @"com.apple.security.prepareIdentityInTPH";
NSString *const kSecurityRTCEventNameCreatesSOSApplication = @"com.apple.security.createSOSApplication";
NSString *const kSecurityRTCEventNameAcceptorCreatesPacket4 = @"com.apple.security.acceptorCreatesPacket4";
NSString *const kSecurityRTCEventNameVerifySOSApplication = @"com.apple.security.verifySOSApplication";
NSString *const kSecurityRTCEventNameCreateSOSCircleBlob = @"com.apple.security.createSOSCircleBlob";
NSString *const kSecurityRTCEventNameTrustedDeviceListRefresh = @"com.apple.security.trustedDeviceListRefresh";
NSString *const kSecurityRTCEventNameCKKSTlkFetch = @"com.apple.security.cKKSTLKFetch";
NSString *const kSecurityRTCEventNameUpdateTrust = @"com.apple.security.updateTrust";
NSString *const kSecurityRTCEventNameInitiatorJoinsTrustSystems = @"com.apple.security.initiatorJoinsTrustSystems";
NSString *const kSecurityRTCEventNameInitiatorJoinsSOS = @"com.apple.security.initiatorJoinSOS";
NSString *const kSecurityRTCEventNameUpdateTDL = @"com.apple.security.updateTDL";
NSString *const kSecurityRTCEventNameFetchAndPersistChanges = @"com.apple.security.fetchAndPersistChanges";
NSString *const kSecurityRTCEventNameFetchPolicyDocument = @"com.apple.security.fetchPolicyDocument";
NSString *const kSecurityRTCEventNameJoin = @"com.apple.security.joinWithVoucher";
NSString *const kSecurityRTCEventNameInitiatorWaitsForUpgrade = @"com.apple.security.initiatorWaitsForUpgrade";
NSString *const kSecurityRTCEventNamePreApprovedJoin = @"com.apple.security.preApprovedJoin";
NSString *const kSecurityRTCEventNameAcceptorCreatesPacket5 = @"com.apple.security.acceptorCreatesPacket5";
NSString *const kSecurityRTCEventNameInitiatorImportsInitialSyncData = @"com.apple.security.initiatorImportsInitialSyncData";
NSString *const kSecurityRTCEventNameAcceptorCreatesVoucher = @"com.apple.security.acceptorCreatesVoucher";
NSString *const kSecurityRTCEventNameAcceptorFetchesInitialSyncData = @"com.apple.security.acceptorFetchesInitialSyncData";
NSString *const kSecurityRTCEventNameNumberOfTrustedOctagonPeers = @"com.apple.security.numberOfTrustedOctagonPeers";
NSString *const kSecurityRTCEventNameCliqueMemberIdentifier = @"com.apple.security.cliqueMemberIdentifier";
NSString *const kSecurityRTCEventNameDuplicateMachineID = @"com.apple.security.duplicateMachineID";
NSString *const kSecurityRTCEventNameMIDVanishedFromTDL = @"com.apple.security.midVanishedFromTDL";
NSString *const kSecurityRTCEventNameTDLProcessingSuccess = @"com.apple.security.tdlProcessingSuccess";
NSString *const kSecurityRTCEventNameAllowedMIDHashMismatch = @"com.apple.security.allowedMIDHashMismatch";
NSString *const kSecurityRTCEventNameDeletedMIDHashMismatch = @"com.apple.security.deletedMIDHashMismatch";
NSString *const kSecurityRTCEventNameTrustedDeviceListFailure = @"com.apple.security.trustedDeviceListFailure";
NSString *const kSecurityRTCEventNamePairingDidNotReceivePCSData = @"com.apple.security.pairingDidNotReceivePCSData";
NSString *const kSecurityRTCEventNamePairingFailedToAddItemToKeychain = @"com.apple.security.pairingFailedToAddItemToKeychain";
NSString *const kSecurityRTCEventNamePairingFailedToUpdateItemInKeychain = @"com.apple.security.pairingFailedToUpdateItemInKeychain";
NSString *const kSecurityRTCEventNamePairingImportKeychainResults = @"com.apple.security.pairingImportKeychainResults";
NSString *const kSecurityRTCEventNamePairingFailedFetchPCSItems = @"com.apple.security.pairingFailedFetchPCSItems";
NSString *const kSecurityRTCEventNamePairingEmptyOctagonPayload = @"com.apple.security.pairingEmptyOctagonPayload";
NSString *const kSecurityRTCEventNamePairingEmptyAckPayload = @"com.apple.security.pairingEmptyAckPayload";
NSString *const kSecurityRTCEventNameRPDDeleteAllRecords = @"com.apple.security.rpdDeleteAllRecords";


// MARK: RTC Fields

NSString *const kSecurityRTCFieldSupportedTrustSystem = @"supportedTrustSystem";
NSString *const kSecurityRTCFieldEventName = @"eventName";
NSString *const kSecurityRTCFieldDidSucceed = @"didSucceed";
NSString *const kSecurityRTCFieldNumberOfTLKsFetched = @"numberOfTLKsFetched";
NSString *const kSecurityRTCFieldNumberOfPCSItemsFetched = @"numberOfPCSItemsFetched";
NSString *const kSecurityRTCFieldNumberOfBluetoothMigrationItemsFetched = @"numberOfBluetoothMigrationItemsFetched";
NSString *const kSecurityRTCFieldOctagonSignInResult = @"octagonSignInResult";
NSString *const kSecurityRTCFieldNumberOfKeychainItemsCollected = @"numberOfKeychainItemsCollected";
NSString *const kSecurityRTCFieldNumberOfKeychainItemsAdded = @"numberOfKeychainItemsAdded";
NSString *const kSecurityRTCFieldNumberOfTrustedPeers = @"numberOfTrustedPeers";
NSString *const kSecurityRTCFieldSecurityLevel = @"securityLevel";
NSString *const kSecurityRTCFieldRetryAttemptCount = @"retryAttemptCount";
NSString *const kSecurityRTCFieldTotalRetryDuration = @"totalRetryDuration";
NSString *const kSecurityRTCFieldEgoMachineIDVanishedFromTDL = @"egoMachineIDVanishedFromTDL";
NSString *const kSecurityRTCFieldPairingSuccessfulImportCount = @"pairingSuccessfulImportCount";
NSString *const kSecurityRTCFieldPairingFailedImportCount = @"pairingFailedImportCount";

// MARK: CKKS Launch RTC Event Names

NSString *const kSecurityRTCEventNameLaunchStart = @"com.apple.security.ckks.launchStart";
NSString *const kSecurityRTCEventNameSyncingPolicySet = @"com.apple.security.ckks.syncingPolicySet";
NSString *const kSecurityRTCEventNameCKAccountLogin = @"com.apple.security.ckks.CKAccountLogin";
NSString *const kSecurityRTCEventNameZoneChangeFetch = @"com.apple.security.ckks.zoneChangeFetch";
NSString *const kSecurityRTCEventNameZoneCreation = @"com.apple.security.ckks.zoneCreation";
NSString *const kSecurityRTCEventNameTrustGain = @"com.apple.security.ckks.trustGain";
NSString *const kSecurityRTCEventNameTrustLoss = @"com.apple.security.ckks.trustLoss";
NSString *const kSecurityRTCEventNameHealKeyHierarchy = @"com.apple.security.ckks.healKeyHierarchy";
NSString *const kSecurityRTCEventNameHealBrokenRecords = @"com.apple.security.ckks.healKeyHierarchy.healBrokenRecords";
NSString *const kSecurityRTCEventNameUploadHealedTLKShares = @"com.apple.security.ckks.healKeyHierarchy.uploadHealedTLKShares";
NSString *const kSecurityRTCEventNameHealTLKShares = @"com.apple.security.ckks.healTLKShares";
NSString *const kSecurityRTCEventNameCreateMissingTLKShares = @"com.apple.security.ckks.healTLKShares.createMissingTLKShares";
NSString *const kSecurityRTCEventNameUploadMissingTLKShares = @"com.apple.security.ckks.healTLKShares.uploadMissingTLKShares";
NSString *const kSecurityRTCEventNameProcessIncomingQueue = @"com.apple.security.ckks.processIncomingQueue";
NSString *const kSecurityRTCEventNameLoadAndProcessIQEs = @"com.apple.security.ckks.processIncomingQueue.loadAndProcessIQEs";
NSString *const kSecurityRTCEventNameFixMismatchedViewItems = @"com.apple.security.ckks.processIncomingQueue.fixMismatchedViewItems";
NSString *const kSecurityRTCEventNameProcessReceivedKeys = @"com.apple.security.ckks.processReceivedKeys";
NSString *const kSecurityRTCEventNameScanLocalItems = @"com.apple.security.ckks.scanLocalItems";
NSString *const kSecurityRTCEventNameQuerySyncableItems = @"com.apple.security.ckks.scanLocalItems.querySyncableItems";
NSString *const kSecurityRTCEventNameOnboardMissingItems = @"com.apple.security.ckks.scanLocalItems.onboardMissingItems";
NSString *const kSecurityRTCEventNameProcessOutgoingQueue = @"com.apple.security.ckks.processOutgoingQueue";
NSString *const kSecurityRTCEventNameUploadOQEsToCK = @"com.apple.security.ckks.processOutgoingQueue.uploadOQEstoCK";
NSString *const kSecurityRTCEventNameSaveCKMirrorEntries = @"com.apple.security.ckks.processOutgoingQueue.saveCKMirrorEntries";
NSString *const kSecurityRTCEventNameFirstManateeKeyFetch = @"com.apple.security.ckks.firstManateeKeyFetch";
NSString *const kSecurityRTCEventNameLocalSyncFinish = @"com.apple.security.ckks.localSyncFinish";
NSString *const kSecurityRTCEventNameContentSyncFinish = @"com.apple.security.ckks.contentSyncFinish";
NSString *const kSecurityRTCEventNameDeviceLocked = @"com.apple.security.ckks.deviceLocked";
NSString *const kSecurityRTCEventNameDeviceUnlocked = @"com.apple.security.ckks.deviceUnlocked";
NSString *const kSecurityRTCEventNameLocalReset = @"com.apple.security.ckks.localReset";

// MARK: CKKS Launch RTC Fields

NSString *const kSecurityRTCFieldNumViews = @"numViews";
NSString *const kSecurityRTCFieldTrustStatus = @"trustStatus";
NSString *const kSecurityRTCFieldSyncingPolicy = @"syncingPolicy";
NSString *const kSecurityRTCFieldPolicyFreshness = @"policyFreshness";
NSString *const kSecurityRTCFieldItemsScanned = @"itemsScanned";
NSString *const kSecurityRTCFieldNewItemsScanned = @"newItemsScanned";
NSString *const kSecurityRTCFieldFetchReasons = @"fetchReasons";
NSString *const kSecurityRTCFieldFullFetch = @"fullFetch";
NSString *const kSecurityRTCFieldAvgRemoteKeys = @"avgRemoteKeys";
NSString *const kSecurityRTCFieldTotalRemoteKeys = @"totalRemoteKeys";
NSString *const kSecurityRTCFieldNewTLKShares = @"newTLKShares";
NSString *const kSecurityRTCFieldIsPrioritized = @"isPrioritized";
NSString *const kSecurityRTCFieldFullRefetchNeeded = @"fullRefetchNeeded";
NSString *const kSecurityRTCFieldIsLocked = @"isLocked";
NSString *const kSecurityRTCFieldMissingKey = @"missingKey";
NSString *const kSecurityRTCFieldPendingClassA = @"pendingClassAEntries";
NSString *const kSecurityRTCFieldSuccessfulItemsProcessed = @"successfulItemsProcessed";
NSString *const kSecurityRTCFieldErrorItemsProcessed = @"errorItemsProcessed";
NSString *const kSecurityRTCFieldAvgSuccessfulItemsProcessed = @"avgSuccessfulItemsProcessed";
NSString *const kSecurityRTCFieldAvgErrorItemsProcessed = @"avgErrorItemsProcessed";
NSString *const kSecurityRTCFieldIsFullUpload = @"isFullUpload";
NSString *const kSecurityRTCFieldPartialFailure = @"partialFailure";
NSString *const kSecurityRTCFieldItemsToAdd = @"itemsToAdd";
NSString *const kSecurityRTCFieldItemsToModify = @"itemsToModify";
NSString *const kSecurityRTCFieldItemsToDelete = @"itemsToDelete";
NSString *const kSecurityRTCFieldNumMismatchedItems = @"numMismatchedItems";
NSString *const kSecurityRTCFieldNumViewsWithNewEntries = @"numViewsWithNewEntries";
NSString *const kSecurityRTCFieldNeedsReencryption = @"needsReencryption";
NSString *const kSecurityRTCFieldNumLocalRecords = @"numLocalRecords";
NSString *const kSecurityRTCFieldNumKeychainItems = @"numKeychainItems";
NSString *const kSecurityRTCFieldTotalCKRecords = @"totalCKRecords";
NSString *const kSecurityRTCFieldAvgCKRecords = @"avgCKRecords";
