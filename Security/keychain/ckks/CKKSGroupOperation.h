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
#include <dispatch/dispatch.h>
#import "keychain/ckks/CKKSResultOperation.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSGroupOperation : CKKSResultOperation
{
    BOOL executing;
    BOOL finished;
}

+ (instancetype)operationWithBlock:(void (^)(void))block;
+ (instancetype)named:(NSString*)name withBlock:(void (^)(void))block;
+ (instancetype)named:(NSString*)name withBlockTakingSelf:(void(^)(CKKSGroupOperation* strongOp))block;

@property NSOperationQueue* operationQueue;

- (instancetype)init;

// For subclasses: override this to execute at Group operation start time
- (void)groupStart;

- (void)runBeforeGroupFinished:(NSOperation*)suboperation;
- (void)dependOnBeforeGroupFinished:(NSOperation*)suboperation;
@end

NS_ASSUME_NONNULL_END

#endif  // OCTAGON
