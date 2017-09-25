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
#import <dispatch/dispatch.h>

/*
 * The CKKSNearFutureScheduler is intended to rate-limit an operation. When
 * triggered, it will schedule the operation to take place in the future.
 * Further triggers during the delay period will not cause the operation to
 * occur again, but they may cause the delay period to extend.
 *
 * Triggers after the delay period will start another delay period.
 */

@interface CKKSNearFutureScheduler : NSObject

@property (readonly) NSDate* nextFireTime;
@property void (^futureOperation)(void);

-(instancetype)initWithName:(NSString*)name
                      delay:(dispatch_time_t)ns
           keepProcessAlive:(bool)keepProcessAlive
                      block:(void (^)(void))futureOperation;

-(instancetype)initWithName:(NSString*)name
               initialDelay:(dispatch_time_t)initialDelay
            continuingDelay:(dispatch_time_t)continuingDelay
           keepProcessAlive:(bool)keepProcessAlive
                      block:(void (^)(void))futureOperation;

-(void)trigger;

-(void)cancel;

// Don't trigger again until at least this much time has passed.
-(void)waitUntil:(uint64_t)delay;

@end
