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

NS_ASSUME_NONNULL_BEGIN

/*!
 A "circle" identifies a set of peers that should be included and
 a set of peers that should be excluded from trust membership.
 
 This class is a value type -- its members are immutable and
 instances with identical contents are interchangeable.
 It overrides isEqual and hash, so that two instances with
 identical contents will compare as equal.
 */
@interface TPCircle : NSObject

@property (nonatomic, readonly) NSString *circleID;
@property (nonatomic, readonly) NSSet<NSString*>* includedPeerIDs;
@property (nonatomic, readonly) NSSet<NSString*>* excludedPeerIDs;

/*!
 A convenience for allocating and initializing an instance from array literals.
 */
+ (instancetype)circleWithIncludedPeerIDs:(nullable NSArray<NSString*> *)includedPeerIDs
                          excludedPeerIDs:(nullable NSArray<NSString*> *)excludedPeerIDs;

/*!
 A convenience for checking a provided circleID. Returns nil if it does not match.
 */
+ (nullable instancetype)circleWithID:(NSString *)circleID
                      includedPeerIDs:(nullable NSArray<NSString*> *)includedPeerIDs
                      excludedPeerIDs:(nullable NSArray<NSString*> *)excludedPeerIDs;

/*!
 Construct a circle that includes a set of peer IDs and excludes a set of peer IDs.
 */
- (instancetype)initWithIncludedPeerIDs:(NSSet<NSString*> *)includedPeerIDs
                        excludedPeerIDs:(NSSet<NSString*> *)excludedPeerIDs;

- (BOOL)isEqualToCircle:(TPCircle *)other;

@end

NS_ASSUME_NONNULL_END
