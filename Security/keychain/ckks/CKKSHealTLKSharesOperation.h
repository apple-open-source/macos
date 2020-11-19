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
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ot/OctagonStateMachine.h"

#if OCTAGON

NS_ASSUME_NONNULL_BEGIN

@class CKKSKeychainView;
@class CKKSOperationDependencies;

@interface CKKSHealTLKSharesOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>
@property CKKSOperationDependencies* deps;
@property (weak) CKKSKeychainView* ckks;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                                         ckks:(CKKSKeychainView*)ckks;


// For this keyset, who doesn't yet have a CKKSTLKShare for its TLK, shared to their current Octagon keys?
// Note that we really want a record sharing the TLK to ourselves, so this function might return
// a non-empty set even if all peers have the TLK: it wants us to make a record for ourself.
// If you pass in a non-empty set in afterUploading, those records will be included in the calculation.
+ (NSSet<CKKSTLKShareRecord*>* _Nullable)createMissingKeyShares:(CKKSCurrentKeySet*)keyset
                                                    trustStates:(NSArray<CKKSPeerProviderState*>*)trustStates
                                                          error:(NSError* __autoreleasing*)errore;
@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
