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

#import "TPPeer.h"
#import "TPPeerPermanentInfo.h"
#import "TPPeerStableInfo.h"
#import "TPPeerDynamicInfo.h"
#import "TPCircle.h"
#import "TPVoucher.h"

@interface TPPeer ()

@property (nonatomic, strong) TPPeerPermanentInfo* permanentInfo;
@property (nonatomic, strong) TPPeerStableInfo* stableInfo;
@property (nonatomic, strong) TPPeerDynamicInfo* dynamicInfo;

@end


@implementation TPPeer

- (NSString *)peerID
{
    return self.permanentInfo.peerID;
}

- (instancetype)initWithPermanentInfo:(TPPeerPermanentInfo *)permanentInfo
{
    self = [super init];
    if (self) {
        _permanentInfo = permanentInfo;
    }
    return self;
}

- (TPResult)updateStableInfo:(TPPeerStableInfo *)stableInfo
{
    if (![self.permanentInfo.trustSigningKey checkSignature:stableInfo.stableInfoSig
                                                matchesData:stableInfo.stableInfoPList]) {
        return TPResultSignatureMismatch;
    }
    if ([self.stableInfo isEqualToPeerStableInfo:stableInfo]) {
        return TPResultOk;
    }
    if (self.stableInfo != nil && stableInfo.clock <= self.stableInfo.clock) {
        return TPResultClockViolation;
    }
    self.stableInfo = stableInfo;
    return TPResultOk;
}

- (TPResult)updateDynamicInfo:(TPPeerDynamicInfo *)dynamicInfo
{
    if (![self.permanentInfo.trustSigningKey checkSignature:dynamicInfo.dynamicInfoSig
                                                matchesData:dynamicInfo.dynamicInfoPList]) {
        return TPResultSignatureMismatch;
    }
    if ([self.dynamicInfo isEqualToPeerDynamicInfo:dynamicInfo]) {
        return TPResultOk;
    }
    if (self.dynamicInfo != nil && dynamicInfo.clock <= self.dynamicInfo.clock) {
        return TPResultClockViolation;
    }
    self.dynamicInfo = dynamicInfo;
    self.circle = nil;
    return TPResultOk;
}

- (void)setCircle:(TPCircle *)circle
{
    if (nil != circle) {
        NSAssert([circle.circleID isEqualToString:self.dynamicInfo.circleID],
                 @"circle property must match dynamicInfo.circleID");
    }
    _circle = circle;
}

- (NSSet<NSString*> *)trustedPeerIDs
{
    if (self.dynamicInfo) {
        NSAssert(self.circle, @"dynamicInfo needs corresponding circle");
        return self.circle.includedPeerIDs;
    } else {
        return [NSSet setWithObject:self.peerID];
    }
}

@end
