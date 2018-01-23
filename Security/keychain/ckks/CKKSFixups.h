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

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSResultOperation.h"

// Sometimes things go wrong.
// Sometimes you have to clean up after your past self.
// This contains the fixes.

typedef NS_ENUM(NSUInteger, CKKSFixup) {
    CKKSFixupNever,
    CKKSFixupRefetchCurrentItemPointers,
    CKKSFixupFetchTLKShares,
    CKKSFixupLocalReload,
};
#define CKKSCurrentFixupNumber (CKKSFixupLocalReload)

@interface CKKSFixups : NSObject
+(CKKSGroupOperation*)fixup:(CKKSFixup)lastfixup for:(CKKSKeychainView*)keychainView;
@end

// Fixup declarations. You probably don't need to look at these
@interface CKKSFixupRefetchAllCurrentItemPointers : CKKSGroupOperation
@property (weak) CKKSKeychainView* ckks;
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView ckoperationGroup:(CKOperationGroup*)ckoperationGroup;
@end

@interface CKKSFixupFetchAllTLKShares : CKKSGroupOperation
@property (weak) CKKSKeychainView* ckks;
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView ckoperationGroup:(CKOperationGroup*)ckoperationGroup;
@end

@interface CKKSFixupLocalReloadOperation : CKKSGroupOperation
@property (weak) CKKSKeychainView* ckks;
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView
                        ckoperationGroup:(CKOperationGroup*)ckoperationGroup;
@end

#endif  // OCTAGON
