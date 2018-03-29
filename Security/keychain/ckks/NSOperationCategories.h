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

#import "keychain/ckks/NSOperationCategories.h"

@interface NSOperation (CKKSUsefulPrintingOperation)
- (NSString*)description;
- (BOOL)isPending;

// Use our .name field if it's there, otherwise, just a generic name
- (NSString*)selfname;

// If op is nonnull, op becomes a dependency of this operation
- (void)addNullableDependency:(NSOperation*)op;

// Add all operations in this collection as dependencies, then add yourself to the collection
- (void)linearDependencies:(NSHashTable*)collection;

// Insert yourself as high up the linearized list of dependencies as possible
- (void)linearDependenciesWithSelfFirst:(NSHashTable*)collection;

// Set completionBlock to remove all dependencies - break strong references.
- (void)removeDependenciesUponCompletion;

// Return a stringified representation of this operation's live dependencies.
- (NSString*)pendingDependenciesString:(NSString*)prefix;
@end

@interface NSBlockOperation (CKKSUsefulConstructorOperation)
+ (instancetype)named:(NSString*)name withBlock:(void (^)(void))block;
@end
