/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#ifndef OTEscrowCheckCallResult_h
#define OTEscrowCheckCallResult_h
#if __OBJC2__

#import <Foundation/Foundation.h>
#import <OctagonTrust/OTEscrowMoveRequestContext.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, OTEscrowCheckRepairReason) {
    OTEscrowCheckRepairReasonUnknown = 0,
    OTEscrowCheckRepairReasonRecordOK = 1,
    OTEscrowCheckRepairReasonNoRecordMatchingPeer = 2,
    OTEscrowCheckRepairReasonNoRecordMatchingPasscodeGeneration = 3,
    OTEscrowCheckRepairReasonNoRecordMatchingRecoverable = 4,
    OTEscrowCheckRepairReasonRecordNeedsMigration = 5,
};

typedef NS_ENUM(NSInteger, OTEscrowCheckRateLimitState) {
    OTEscrowCheckRateLimitStateUnknown = 0,
    OTEscrowCheckRateLimitStateNotRateLimited = 1,
    OTEscrowCheckRateLimitStateRateLimited = 2,
    OTEscrowCheckRateLimitStateValidCacheNotRateLimited = 3,
    OTEscrowCheckRateLimitStateValidCacheRateLimited = 4,
};

typedef NS_ENUM(NSInteger, OctagonTrustStatus) {
    OctagonTrustStatusUnknown = 0,
    OctagonTrustStatusNotTrustedLocally = 1,
    OctagonTrustStatusNotTrustedCuttlefish = 2,
    OctagonTrustStatusGraphNeedsRepair = 3,
    OctagonTrustStatusTrusted = 4,
};

@interface OTEscrowCheckCallResult: NSObject<NSSecureCoding>
@property bool needsReenroll;
@property NSInteger octagonTrusted;
@property bool secureTermsNeeded;
@property NSInteger repairReason;
@property (nullable, retain) OTEscrowMoveRequestContext* moveRequest;
@property bool repairDisabled;
@property NSInteger daysLeftOnRateLimit;
@property NSInteger rateLimitState;
- (NSDictionary*)dictionaryRepresentation;
@end

NS_ASSUME_NONNULL_END

#endif // __OBJC2__

#endif /* OTEscrowCheckCallResult_h */
