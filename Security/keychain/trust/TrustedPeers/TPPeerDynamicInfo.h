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

#import "TPTypes.h"
#import "TPSigningKey.h"

NS_ASSUME_NONNULL_BEGIN

/*!
 Having an instance of this class does *not* mean that
 its signature has been checked. Checking the signature
 is up to whoever consumes it.
 
 This class is a value type -- its members are immutable and
 instances with identical contents are interchangeable.
 */
@interface TPPeerDynamicInfo : NSObject

/*!
 Can return nil with error if [trustSigningKey signatureForData:error:] errors.
 */
+ (nullable instancetype)dynamicInfoWithCircleID:(NSString *)circleID
                                          clique:(NSString *)clique
                                        removals:(TPCounter)removals
                                           clock:(TPCounter)clock
                                 trustSigningKey:(id<TPSigningKey>)trustSigningKey
                                           error:(NSError **)error;

// Returns nil if data cannot be deserialized to a dictionary
// or that dictionary does not contain the expected keys and value types.
+ (nullable instancetype)dynamicInfoWithPListData:(NSData *)dynamicInfoPList
                                   dynamicInfoSig:(NSData *)dynamicInfoSig;

- (BOOL)isEqualToPeerDynamicInfo:(TPPeerDynamicInfo *)other;

@property (nonatomic, readonly) NSString *circleID;
@property (nonatomic, readonly) NSString *clique;
@property (nonatomic, readonly) TPCounter removals;
@property (nonatomic, readonly) TPCounter clock;
@property (nonatomic, readonly) NSData *dynamicInfoPList;
@property (nonatomic, readonly) NSData *dynamicInfoSig;

@end

NS_ASSUME_NONNULL_END
