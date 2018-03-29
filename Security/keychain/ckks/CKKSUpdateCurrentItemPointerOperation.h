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

#if OCTAGON

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSKeychainView.h"
NS_ASSUME_NONNULL_BEGIN


@interface CKKSUpdateCurrentItemPointerOperation : CKKSGroupOperation
@property (weak,nullable) CKKSKeychainView* ckks;

@property NSString* currentPointerIdentifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView* _Nonnull)ckks
                                 newItem:(NSData* _Nonnull)newItemPersistentRef
                                    hash:(NSData* _Nonnull)newItemSHA1
                             accessGroup:(NSString* _Nonnull)accessGroup
                              identifier:(NSString* _Nonnull)identifier
                               replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                                    hash:(NSData* _Nullable)oldItemSHA1
                        ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup;
@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
