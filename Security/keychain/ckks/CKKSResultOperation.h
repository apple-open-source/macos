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
#import <dispatch/dispatch.h>
#import "keychain/ckks/NSOperationCategories.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSCondition;

#define CKKSResultErrorDomain @"CKKSResultOperationError"
enum {
    CKKSResultSubresultError = 1,
    CKKSResultSubresultCancelled = 2,
    CKKSResultTimedOut = 3,
};

#define CKKSResultDescriptionErrorDomain @"CKKSResultOperationDescriptionError"

@interface CKKSResultOperation : NSBlockOperation
@property (nullable) NSError* error;
@property (nullable) NSDate* finishDate;
@property CKKSCondition* completionHandlerDidRunCondition;

@property NSInteger descriptionErrorCode; // Set to non-0 for inclusion of this operation in NSError chains. Code is application-dependent.

// If you subclass CKKSResultOperation, this is the method corresponding to descriptionErrorCode. Fill it in to your heart's content.
- (NSError* _Nullable)descriptionError;

// Very similar to addDependency, but:
//   if the dependent operation has an error or is canceled, cancel this operation
- (void)addSuccessDependency:(CKKSResultOperation*)operation;
- (void)addNullableSuccessDependency:(CKKSResultOperation*)operation;

// Call to check if you should run.
// Note: all subclasses must call this if they'd like to comply with addSuccessDependency
// Also sets your .error property to encapsulate the upstream error
- (bool)allDependentsSuccessful;

// Allows you to time out CKKSResultOperations: if they haven't started by now, they'll cancel themselves
// and set their error to indicate the timeout
- (instancetype)timeout:(dispatch_time_t)timeout;

// Convenience constructor.
+ (instancetype)operationWithBlock:(void (^)(void))block;
+ (instancetype)named:(NSString*)name withBlock:(void (^)(void))block;
+ (instancetype)named:(NSString*)name withBlockTakingSelf:(void(^)(CKKSResultOperation* op))block;

// Determine if all these operations were successful, and set this operation's result if not.
- (bool)allSuccessful:(NSArray<CKKSResultOperation*>*)operations;

// Call this to prevent the timeout on this operation from occuring.
// Upon return, either this operation is cancelled, or the timeout will never fire.
- (void)invalidateTimeout;

// Reports the state of this operation. Used for making up description strings.
- (NSString*)operationStateString;
@end

NS_ASSUME_NONNULL_END
#endif // OCTAGON

