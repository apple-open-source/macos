/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
#if OCTAGON

#import "keychain/ckks/CKKSOperationDependencies.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKeychainView;
@class CKKSItem;

@interface CKKSIncomingQueueOperation : CKKSResultOperation <OctagonStateTransitionOperationProtocol>
@property CKKSOperationDependencies* deps;
@property (weak) CKKSKeychainView* ckks;

// Set this to true if this instance of CKKSIncomingQueueOperation
// should error if it can't process class A items due to the keychain being locked.
@property bool errorOnClassAFailure;

// Set this to true if you're pretty sure that the policy set on the CKKS object
// should be considered authoritative, and items that do not match this policy should
// be moved.
@property bool handleMismatchedViewItems;

@property size_t successfulItemsProcessed;
@property size_t errorItemsProcessed;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                                ckks:(CKKSKeychainView*)ckks
                           intending:(OctagonState*)intending
                          errorState:(OctagonState*)errorState
                errorOnClassAFailure:(bool)errorOnClassAFailure
               handleMismatchedViewItems:(bool)handleMismatchedViewItems;

// Use this to turn a CKKS item into a keychain dictionary suitable for keychain insertion
+ (NSDictionary* _Nullable)decryptCKKSItemToAttributes:(CKKSItem*)item error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
