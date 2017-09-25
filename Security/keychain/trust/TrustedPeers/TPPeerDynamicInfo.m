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

#import "TPPeerDynamicInfo.h"
#import "TPUtils.h"

static const NSString *kCircleID = @"circleID";
static const NSString *kClique = @"clique";
static const NSString *kRemovals = @"removals";
static const NSString *kClock = @"clock";


@interface TPPeerDynamicInfo ()

@property (nonatomic, strong) NSString *circleID;
@property (nonatomic, strong) NSString *clique;
@property (nonatomic, assign) TPCounter removals;
@property (nonatomic, assign) TPCounter clock;
@property (nonatomic, strong) NSData *dynamicInfoPList;
@property (nonatomic, strong) NSData *dynamicInfoSig;

@end


@implementation TPPeerDynamicInfo

+ (instancetype)dynamicInfoWithCircleID:(NSString *)circleID
                                 clique:(NSString *)clique
                               removals:(TPCounter)removals
                                  clock:(TPCounter)clock
                        trustSigningKey:(id<TPSigningKey>)trustSigningKey
                                  error:(NSError **)error
{
    NSDictionary *dict = @{
                           kCircleID: circleID,
                           kClique: clique,
                           kRemovals: @(removals),
                           kClock: @(clock)
                           };
    NSData *data = [TPUtils serializedPListWithDictionary:dict];
    NSData *sig = [trustSigningKey signatureForData:data withError:error];
    if (nil == sig) {
        return nil;
    }
    TPPeerDynamicInfo* info = [self dynamicInfoWithPListData:data dynamicInfoSig:sig];
    assert(info);
    return info;
}

+ (instancetype)dynamicInfoWithPListData:(NSData *)dynamicInfoPList
                          dynamicInfoSig:(NSData *)dynamicInfoSig
{
    id dict = [NSPropertyListSerialization propertyListWithData:dynamicInfoPList
                                                        options:NSPropertyListImmutable
                                                         format:nil
                                                          error:NULL];
    if (![dict isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    
    TPPeerDynamicInfo* info = [[TPPeerDynamicInfo alloc] init];
    
    if (![dict[kCircleID] isKindOfClass:[NSString class]]) {
        return nil;
    }
    info.circleID = dict[kCircleID];

    if (![dict[kClique] isKindOfClass:[NSString class]]) {
        return nil;
    }
    info.clique = dict[kClique];

    if (![dict[kRemovals] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    info.removals = [dict[kRemovals] unsignedLongLongValue];

    if (![dict[kClock] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    info.clock = [dict[kClock] unsignedLongLongValue];
    
    info.dynamicInfoPList = [dynamicInfoPList copy];
    info.dynamicInfoSig = [dynamicInfoSig copy];
    
    return info;
}

- (BOOL)isEqualToPeerDynamicInfo:(TPPeerDynamicInfo *)other
{
    if (other == self) {
        return YES;
    }
    return [self.dynamicInfoPList isEqualToData:other.dynamicInfoPList]
        && [self.dynamicInfoSig isEqualToData:other.dynamicInfoSig];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[TPPeerDynamicInfo class]]) {
        return NO;
    }
    return [self isEqualToPeerDynamicInfo:object];
}

@end
