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

#import "TPPolicy.h"
#import "TPEncrypter.h"
#import "TPDecrypter.h"
#import "TPHash.h"
#import "TPTypes.h"

NS_ASSUME_NONNULL_BEGIN

/*!
 This class is a value type -- its members are immutable and
 instances with identical contents are interchangeable.
 */
@interface TPPolicyDocument : NSObject

@property (nonatomic, readonly) TPCounter policyVersion;
@property (nonatomic, readonly) NSString *policyHash;
@property (nonatomic, readonly) NSData *pList;

+ (nullable instancetype)policyDocWithHash:(NSString *)policyHash
                                     pList:(NSData *)pList;

+ (instancetype)policyDocWithVersion:(TPCounter)policyVersion
                     modelToCategory:(NSArray<NSDictionary*> *)modelToCategory
                    categoriesByView:(NSDictionary<NSString*,NSArray<NSString*>*> *)categoriesByView
               introducersByCategory:(NSDictionary<NSString*,NSArray<NSString*>*> *)introducersByCategory
                          redactions:(NSDictionary<NSString*,NSData*> *)redactions
                            hashAlgo:(TPHashAlgo)hashAlgo;

+ (nullable NSData *)redactionWithEncrypter:(id<TPEncrypter>)encrypter
                            modelToCategory:(nullable NSArray<NSDictionary*> *)modelToCategory
                           categoriesByView:(nullable NSDictionary<NSString*,NSArray<NSString*>*> *)categoriesByView
                      introducersByCategory:(nullable NSDictionary<NSString*,NSArray<NSString*>*> *)introducersByCategory
                                      error:(NSError **)error;

- (nullable id<TPPolicy>)policyWithSecrets:(NSDictionary<NSString*,NSData*> *)secrets
                                 decrypter:(id<TPDecrypter>)decrypter
                                     error:(NSError **)error;

- (BOOL)isEqualToPolicyDocument:(TPPolicyDocument *)other;

@end

NS_ASSUME_NONNULL_END
