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


#ifndef CKKSGroupOperation_h
#define CKKSGroupOperation_h

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

@class CKKSCondition;

@interface NSOperation (CKKSUsefulPrintingOperation)
- (NSString*)description;
- (BOOL)isPending;

// If op is nonnull, op becomes a dependency of this operation
- (void)addNullableDependency: (NSOperation*) op;

// Add all operations in this collection as dependencies, then add yourself to the collection
-(void)linearDependencies:(NSHashTable*)collection;
@end

@interface NSBlockOperation (CKKSUsefulConstructorOperation)
+(instancetype)named: (NSString*)name withBlock: (void(^)(void)) block;
@end


#define CKKSResultErrorDomain @"CKKSResultOperationError"
enum {
    CKKSResultSubresultError = 1,
    CKKSResultSubresultCancelled = 2,
    CKKSResultTimedOut = 3,
};

@interface CKKSResultOperation : NSBlockOperation
@property NSError* error;
@property NSDate* finishDate;
@property CKKSCondition* completionHandlerDidRunCondition;

// Very similar to addDependency, but:
//   if the dependent operation has an error or is canceled, cancel this operation
- (void)addSuccessDependency: (CKKSResultOperation*) operation;

// Call to check if you should run.
// Note: all subclasses must call this if they'd like to comply with addSuccessDependency
// Also sets your .error property to encapsulate the upstream error
- (bool)allDependentsSuccessful;

// Allows you to time out CKKSResultOperations: if they haven't started by now, they'll cancel themselves
// and set their error to indicate the timeout
- (instancetype)timeout:(dispatch_time_t)timeout;

// Convenience constructor.
+(instancetype)operationWithBlock:(void (^)(void))block;
+(instancetype)named: (NSString*)name withBlock: (void(^)(void)) block;
@end

@interface CKKSGroupOperation : CKKSResultOperation {
    BOOL executing;
    BOOL finished;
}

@property NSOperationQueue* operationQueue;

- (instancetype)init;

// For subclasses: override this to execute at Group operation start time
- (void)groupStart;

- (void)runBeforeGroupFinished: (NSOperation*) suboperation;
- (void)dependOnBeforeGroupFinished: (NSOperation*) suboperation;
@end

#endif // CKKSGroupOperation_h
