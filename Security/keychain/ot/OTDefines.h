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

#ifndef OTDefines_h
#define OTDefines_h
#if OCTAGON
#include <Foundation/Foundation.h>
#include "utilities/debugging.h"
#import "keychain/ot/OTConstants.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString* const OctagonEventAttributeZoneName;
extern NSString* const OctagonEventAttributeFailureReason;
extern NSString* const OctagonEventAttributeTimeSinceLastPostedFollowUp;

extern NSString* OTCKContainerName;
extern NSString* const CuttlefishTrustZone;
extern NSString* const TrustedPeersHelperErrorDomain;


#define OTMasterSecretLength 72

typedef NS_ENUM(NSInteger, TrustedPeersHelperErrorCode) {
    TrustedPeersHelperErrorNoPreparedIdentity = 1,
    TrustedPeersHelperErrorNoPeersPreapprovePreparedIdentity = 14,
    TrustedPeersHelperErrorCodeUntrustedRecoveryKeys    = 32,
    TrustedPeersHelperErrorCodeNotEnrolled   = 34,
    TrustedPeersHelperErrorUnknownCloudKitError   = 36,
    TrustedPeersHelperErrorNoPeersPreapprovedBySelf = 47,
};

// See cuttlefish/CuttlefishService/Sources/CuttlefishService/CuttlefishError.swift
typedef NS_ENUM(NSInteger, CuttlefishErrorCode) {
    CuttlefishErrorEstablishFailed = 1001,
    CuttlefishErrorJoinFailed = 1002,
    CuttlefishErrorUpdateTrustFailed = 1004,
    CuttlefishErrorInvalidChangeToken = 1005,
    CuttlefishErrorMalformedRecord = 1006,
    CuttlefishErrorResultGraphNotFullyReachable = 1007,
    CuttlefishErrorResultGraphHasNoPotentiallyTrustedPeers = 1008,
    CuttlefishErrorResultGraphHasSplitKnowledge = 1009,
    CuttlefishErrorResultGraphHasPeerWithNoSelf = 1010,
    CuttlefishErrorInvalidEscrowProxyOperation = 1011,
    CuttlefishErrorRecordWrongType = 1012,
    CuttlefishErrorMissingMandatoryField = 1013,
    CuttlefishErrorMalformedViewKeyHierarchy = 1014,
    CuttlefishErrorUnknownView = 1015,
    CuttlefishErrorEstablishPeerFailed = 1016,
    CuttlefishErrorEstablishBottleFailed = 1017,
    CuttlefishErrorChangeTokenExpired = 1018,
    CuttlefishErrorTransactionalFailure = 1019,
    CuttlefishErrorSetRecoveryKeyFailed = 1020,
    CuttlefishErrorRetryableServerFailure = 1021,
    CuttlefishErrorPreflightGraphValidationError = 1022,
    CuttlefishErrorKeyHierarchyAlreadyExists = 1033,
    CuttlefishErrorDuplicatePeerIdUnderConsideration = 1034,
    CuttlefishErrorIneligibleExclusionDenied = 1035,
    CuttlefishErrorMultiplePreapprovedJoinDenied = 1036,
    CuttlefishErrorUpdateTrustPeerNotFound = 1037,
    CuttlefishErrorEscrowProxyFailure = 1038,
    CuttlefishErrorResetFailed = 1039,
    CuttlefishErrorViewZoneDeletionFailed = 1040,
    CuttlefishErrorAddCustodianRecoveryKeyFailed = 1041,

    // For testing error handling. Never returned from actual cuttlefish.
    // Should not be retried.
    CuttlefishErrorTestGeneratedFailure = 9999,
};

NS_ASSUME_NONNULL_END
#endif
#endif /* OTDefines_h */
