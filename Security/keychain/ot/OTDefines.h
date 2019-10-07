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
#include <utilities/debugging.h>
NS_ASSUME_NONNULL_BEGIN

extern NSString* const OctagonEventAttributeZoneName;
extern NSString* const OctagonEventAttributeFailureReason;
extern NSString* const OctagonEventAttributeTimeSinceLastPostedFollowUp;

extern NSString* OTCKContainerName;
extern NSString* const CuttlefishTrustZone;
extern NSString* const CuttlefishErrorDomain;
extern NSString* const TrustedPeersHelperErrorDomain;

/* Octagon Errors */
typedef enum {
    OTErrorNoColumn                         = 1,
    OTErrorKeyGeneration                    = 2,
    OTErrorEmptySecret                      = 3,
    OTErrorEmptyDSID                        = 4,
    OTErrorNoIdentity                       = 5,
    OTErrorRestoreFailed                    = 6,
    OTErrorRestoredPeerEncryptionKeyFailure = 7,
    OTErrorRestoredPeerSigningKeyFailure    = 8,
    OTErrorEntropyCreationFailure           = 9,
    OTErrorDeserializationFailure           = 10,
    OTErrorDecryptFailure                   = 11,
    OTErrorPrivateKeyFailure                = 12,
    OTErrorEscrowSigningSPKI                = 13,
    OTErrorBottleID                         = 14,
    OTErrorOTLocalStore                     = 15,
    OTErrorOTCloudStore                     = 16,
    OTErrorEmptyEscrowRecordID              = 17,
    OTErrorNoBottlePeerRecords              = 18,
    OTErrorCoreFollowUp                     = 19,
    OTErrorFeatureNotEnabled                = 20,
    OTErrorCKCallback                       = 21,
    OTErrorRampInit                         = 22,
    OTErrorCKTimeOut                        = 23,
    OTErrorNoNetwork                        = 24,
    OTErrorNotSignedIn                      = 25,
    OTErrorRecordNotFound                   = 26,
    OTErrorNoEscrowKeys                     = 27,
    OTErrorBottleUpdate                     = 28,
    OTErrorNotSupported                     = 29,
    OTErrorUnexpectedStateTransition        = 30,
    OTErrorNoSuchContext                    = 31,
    OTErrorTimeout                          = 32,
    OTErrorMasterKey                        = 33,
    OTErrorNotTrusted                       = 34,
    OTErrorLimitedPeer                      = 35,
    OTErrorNoOctagonKeysInSOS               = 36,
    OTErrorNeedsAuthentication              = 37,
    OTErrorOctagonAdapter                   = 38,
    OTErrorSOSAdapter                       = 39,
    OctagonErrorNoAccount                   = 40,
} OctagonErrorCode;

#define OTMasterSecretLength 72

typedef enum {
    OctagonSigningKey =         1,
    OctagonEncryptionKey =      2
} OctagonKeyType;

typedef enum {
    UNCLEAR =                   0,
    BOTTLE =                    1,
    NOBOTTLE =                  2
} OctagonBottleCheckState;

typedef NS_ENUM(NSInteger, TrustedPeersHelperErrorCode) {
    TrustedPeersHelperErrorNoPreparedIdentity = 1,
    TrustedPeersHelperErrorNoPeersPreapprovePreparedIdentity = 14,
    TrustedPeersHelperErrorCodeNotEnrolled   = 34,
};

typedef NS_ENUM(NSInteger, CuttlefishErrorCode) {
    CuttlefishErrorResultGraphNotFullyReachable = 1007,
    CuttlefishErrorTransactionalFailure = 1019,
    CuttlefishErrorKeyHierarchyAlreadyExists = 1033,
};

NS_ASSUME_NONNULL_END
#endif
#endif /* OTDefines_h */
