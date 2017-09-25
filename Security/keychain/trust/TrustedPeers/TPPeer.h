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

#import "TPHash.h"
#import "TPSigningKey.h"
#import "TPTypes.h"

@class TPCircle;
@class TPVoucher;
@class TPPeerPermanentInfo;
@class TPPeerStableInfo;
@class TPPeerDynamicInfo;

NS_ASSUME_NONNULL_BEGIN

@interface TPPeer : NSObject

@property (nonatomic, readonly) NSString* peerID;

@property (nonatomic, readonly) TPPeerPermanentInfo* permanentInfo;
@property (nonatomic, readonly, nullable) TPPeerStableInfo* stableInfo;
@property (nonatomic, readonly, nullable) TPPeerDynamicInfo* dynamicInfo;
@property (nonatomic, strong) NSData* wrappedPrivateKeys;

// setCircle asserts that circle.circleID == dynamicInfo.circleID
@property (nonatomic, strong, nullable) TPCircle* circle;

@property (nonatomic, readonly) NSSet<NSString*>* trustedPeerIDs;

- (instancetype)initWithPermanentInfo:(TPPeerPermanentInfo *)permanentInfo;

- (TPResult)updateStableInfo:(TPPeerStableInfo *)stableInfo;

// Returns YES on success, or NO if:
// - the data or signature is invalid
// - this update makes a change without advancing dynamicInfo.clock
//
// An "update" with unchanged data is considered success.
//
// This call also sets self.circle to nil.
// The caller should subsequently call updateCircle to update it.
- (TPResult)updateDynamicInfo:(TPPeerDynamicInfo *)dynamicInfo;

@end

NS_ASSUME_NONNULL_END
