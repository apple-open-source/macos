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

#ifndef OTConstants_h
#define OTConstants_h

#include <stdbool.h>

bool SecErrorIsNestedErrorCappingEnabled(void);
bool SecKeychainIsStaticPersistentRefsEnabled(void);
void SecKeychainSetOverrideStaticPersistentRefsIsEnabled(bool value);
bool OctagonIsSOSFeatureEnabled(void);
bool OctagonPlatformSupportsSOS(void);
void OctagonSetSOSFeatureEnabled(bool value);
bool SOSCompatibilityModeEnabled(void);
void SetSOSCompatibilityMode(bool value);
void ClearSOSCompatibilityModeOverride(void);

#if __OBJC__

#import <Foundation/Foundation.h>
#import <AppleFeatures/AppleFeatures.h>

extern NSString* OTDefaultContext;

extern NSErrorDomain const OctagonErrorDomain;

typedef NS_ERROR_ENUM(OctagonErrorDomain, OctagonError) {
    OctagonErrorNoIdentity                                      = 5,
    OctagonErrorDeserializationFailure                          = 10,
    OctagonErrorFeatureNotEnabled                               = 20,
    OctagonErrorCKCallback                                      = 21,
    OctagonErrorCKTimeOut                                       = 23,
    OctagonErrorNoNetwork                                       = 24,
    OctagonErrorNotSignedIn                                     = 25,
    OctagonErrorRecordNotFound                                  = 26,
    OctagonErrorNotSupported                                    = 29,
    OctagonErrorUnexpectedStateTransition                       = 30,
    OctagonErrorNoSuchContext                                   = 31,
    OctagonErrorOperationUnavailableOnLimitedPeer               = 35,
    OctagonErrorOctagonAdapter                                  = 38,
    OctagonErrorSOSAdapter                                      = 39,
    OctagonErrorRecoveryKeyMalformed                            = 41,
    OctagonErrorAuthKitAKDeviceListRequestContextClass          = 43,
    OctagonErrorAuthKitNoPrimaryAccount                         = 44,
    OctagonErrorAuthKitNoAuthenticationController               = 45,
    OctagonErrorAuthKitMachineIDMissing                         = 46,
    OctagonErrorAuthKitPrimaryAccountHaveNoDSID                 = 47,
    OctagonErrorFailedToLeaveClique                             = 48,
    OctagonErrorSyncPolicyMissing                               = 49,
    OctagonErrorRequiredLibrariesNotPresent                     = 50,
    OctagonErrorFailedToSetWalrus                               = 51,
    OctagonErrorFailedToSetWebAccess                            = 52,
    OctagonErrorNoAccountSettingsSet                            = 53,
    OctagonErrorBadUUID                                         = 54,
    OctagonErrorUserControllableViewsUnavailable                = 55,
    OctagonErrorICloudAccountStateUnknown                       = 56,
    OctagonErrorClassCLocked                                    = 57,
    OctagonErrorRecordNotViable                                 = 58,
    OctagonErrorNoAppleAccount                                  = 59,
    OctagonErrorInvalidPersona                                  = 60,
    OctagonErrorNoSuchCKKS                                      = 61,
    OctagonErrorUnsupportedInEDUMode                            = 62,
    OctagonErrorAltDSIDPersonaMismatch                          = 63,
    OctagonErrorNoRecoveryKeyRegistered                         = 64,
    OctagonErrorRecoverWithRecoveryKeyNotSupported              = 65,
    OctagonErrorSecureBackupRestoreUsingRecoveryKeyFailed       = 66,
    OctagonErrorRecoveryKeyIncorrect                            = 67,
    OctagonErrorBadAuthKitResponse                              = 68,
    OctagonErrorUnsupportedAccount                              = 69,
    OctagonErrorSOSDisabled                                     = 70,
    OctagonErrorNotInSOS                                        = 71,
    OctagonErrorInjectedError                                   = 72,
    OctagonErrorCannotSetAccountSettings                        = 73,
};

/* used for defaults writes */
extern NSString* OTDefaultsDomain;

extern NSString* OTProtocolPairing;
extern NSString* OTProtocolPiggybacking;

extern const char * OTTrustStatusChangeNotification;
extern NSString* OTEscrowRecordPrefix;

// Used for testing.

bool OctagonSupportsPersonaMultiuser(void);
void OctagonSetSupportsPersonaMultiuser(bool value);
void OctagonClearSupportsPersonaMultiuserOverride(void);

void SecErrorSetOverrideNestedErrorCappingIsEnabled(bool value);


typedef NS_ENUM(NSInteger, CuttlefishResetReason) {
    CuttlefishResetReasonUnknown = 0,
    CuttlefishResetReasonUserInitiatedReset = 1,
    CuttlefishResetReasonHealthCheck = 2,
    CuttlefishResetReasonNoBottleDuringEscrowRecovery = 3,
    CuttlefishResetReasonLegacyJoinCircle = 4,
    CuttlefishResetReasonRecoveryKey = 5,
    CuttlefishResetReasonTestGenerated = 6,
};

extern NSString* const CuttlefishErrorDomain;
extern NSString* const CuttlefishErrorRetryAfterKey;

typedef NS_ENUM(NSInteger, OTEscrowRecordFetchSource) {
    /// Default is equivalent to cache or cuttlefish, depending on recency of cache update.
    OTEscrowRecordFetchSourceDefault = 0,
    
    /// Forces the escrow record fetch to only use local on-disk cache, even if stale.
    OTEscrowRecordFetchSourceCache = 1,
    
    /// Forces the escrow record fetch to only use cuttlefish, even if cache is recent.
    OTEscrowRecordFetchSourceCuttlefish = 2,
};

#endif // __OBJC__

#endif /* OTConstants_h */
