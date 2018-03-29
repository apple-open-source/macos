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
#import <keychain/ckks/CKKSResultOperation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The CKKSNearFutureScheduler is intended to rate-limit an operation. When
 * triggered, it will schedule the operation to take place in the future.
 * Further triggers during the delay period will not cause the operation to
 * occur again, but they may cause the delay period to extend.
 *
 * Triggers after the delay period will start another delay period.
 */

@interface CKKSNearFutureScheduler : NSObject

@property (nullable, readonly) NSDate* nextFireTime;
@property void (^futureBlock)(void);

// Will execute every time futureBlock is called, just after the future block.
// Operations added in the futureBlock will receive the next operationDependency, so they won't run again until futureBlock occurs again.
@property (readonly) CKKSResultOperation* operationDependency;


// dependencyDescriptionCode will be integrated into the operationDependency as per the rules in CKKSResultOperation.h
- (instancetype)initWithName:(NSString*)name
                       delay:(dispatch_time_t)ns
            keepProcessAlive:(bool)keepProcessAlive
   dependencyDescriptionCode:(NSInteger)code
                       block:(void (^_Nonnull)(void))futureOperation;

- (instancetype)initWithName:(NSString*)name
                initialDelay:(dispatch_time_t)initialDelay
             continuingDelay:(dispatch_time_t)continuingDelay
            keepProcessAlive:(bool)keepProcessAlive
   dependencyDescriptionCode:(NSInteger)code
                       block:(void (^_Nonnull)(void))futureBlock;

- (void)trigger;

- (void)cancel;

// Don't trigger again until at least this much time has passed.
- (void)waitUntil:(uint64_t)delay;

@end

NS_ASSUME_NONNULL_END
#endif // OCTAGON
