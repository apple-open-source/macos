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
#import "TPHash.h"

NS_ASSUME_NONNULL_BEGIN

/*!
 This class is a value type -- its members are immutable and
 instances with identical contents are interchangeable.
 */
@interface TPPeerPermanentInfo : NSObject

/*!
 Can return nil with error if [trustSigningKey signatureForData:error:] errors.
 */
+ (nullable instancetype)permanentInfoWithMachineID:(NSString *)machineID
                                            modelID:(NSString *)modelID
                                              epoch:(TPCounter)epoch
                                    trustSigningKey:(id<TPSigningKey>)trustSigningKey
                                     peerIDHashAlgo:(TPHashAlgo)peerIDHashAlgo
                                              error:(NSError **)error;

// Returns nil if:
// - permanentInfoPList cannot be deserialized to a dictionary
// - that dictionary does not contain the expected keys and value types
// - permanentInfoSig does not match permanentInfoPList signed with the trustSigningKey from the dictionary
// - peerID does not match the hash of (permanentInfoPList + permanentInfoSig)
+ (nullable instancetype)permanentInfoWithPeerID:(NSString *)peerID
                              permanentInfoPList:(NSData *)permanentInfoPList
                                permanentInfoSig:(NSData *)permanentInfoSig
                                      keyFactory:(id<TPSigningKeyFactory>)keyFactory;

@property (nonatomic, readonly) NSString* machineID;
@property (nonatomic, readonly) NSString* modelID;
@property (nonatomic, readonly) TPCounter epoch;
@property (nonatomic, readonly) id<TPSigningKey> trustSigningKey;
@property (nonatomic, readonly) NSData *permanentInfoPList;
@property (nonatomic, readonly) NSData *permanentInfoSig;
@property (nonatomic, readonly) NSString *peerID;

@end

NS_ASSUME_NONNULL_END
