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

static NSString* const octagonErrorDomain = @"com.apple.security.octagon";
static NSString* const OctagonEventAttributeZoneName = @"OTBottledPeer";
static NSString* const OctagonEventAttributeFailureReason = @"OTFailureReason";
static NSString* const OctagonEventAttributeTimeSinceLastPostedFollowUp = @"TimeSinceLastPostedFollowUp";


/* Octagon Errors */
#define OTErrorNoColumn                         1
#define OTErrorKeyGeneration                    2
#define OTErrorEmptySecret                      3
#define OTErrorEmptyDSID                        4
#define OTErrorNoIdentity                       5
#define OTErrorRestoreFailed                    6
#define OTErrorRestoredPeerEncryptionKeyFailure 7
#define OTErrorRestoredPeerSigningKeyFailure    8
#define OTErrorEntropyCreationFailure           9
#define OTErrorDeserializationFailure           10
#define OTErrorDecryptFailure                   11
#define OTErrorPrivateKeyFailure                12
#define OTErrorEscrowSigningSPKI                13
#define OTErrorBottleID                         14
#define OTErrorOTLocalStore                     15
#define OTErrorOTCloudStore                     16
#define OTErrorEmptyEscrowRecordID              17
#define OTErrorNoBottlePeerRecords              18
#define OTErrorCoreFollowUp                     19
#define OTErrorFeatureNotEnabled                20
#define OTErrorCKCallback                       21
#define OTErrorRampInit                         22
#define OTErrorCKTimeOut                        23
#define OTErrorNoNetwork                        24
#define OTErrorNotSignedIn                      25
#define OTErrorRecordNotFound                   26

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

NS_ASSUME_NONNULL_END
#endif
#endif /* OTDefines_h */
