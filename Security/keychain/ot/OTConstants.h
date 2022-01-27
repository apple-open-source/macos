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

#if __OBJC__

#import <Foundation/Foundation.h>

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
    OctagonErrorNoAccountSettingsSet                            = 53,
    OctagonErrorBadUUID                                         = 54,
    OctagonErrorUserControllableViewsUnavailable                = 55,
    OctagonErrorICloudAccountStateUnknown                       = 56,
    OctagonErrorClassCLocked                                    = 57,
    OctagonErrorRecordNotViable                                 = 58,
};

/* used for defaults writes */
extern NSString* OTDefaultsDomain;

extern NSString* OTProtocolPairing;
extern NSString* OTProtocolPiggybacking;

extern const char * OTTrustStatusChangeNotification;
extern NSString* OTEscrowRecordPrefix;


bool OctagonPlatformSupportsSOS(void);

// Used for testing.
void OctagonSetPlatformSupportsSOS(bool value);

bool OctagonIsSOSFeatureEnabled(void);
void OctagonSetSOSFeatureEnabled(bool value);

bool SecKVSOnCloudKitIsEnabled(void);
void SecKVSOnCloudKitSetOverrideIsEnabled(bool value);

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

#endif // __OBJC__

#endif /* OTConstants_h */
