/*
* Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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
//
//  PMCoreSmartPowerNapPredictor.h
//  PowerManagement
//
//  Created by Prateek Malhotra on 12/7/22.
//

#ifndef PMCoreSmartPowerNapPredictor_h
#define PMCoreSmartPowerNapPredictor_h

#import <Foundation/Foundation.h>
#import <xpc/private.h>
#import <IOKit/pwr_mgt/powermanagement_mig.h>

#if !XCTEST && !TARGET_OS_BRIDGE
#import <CoreMotion/CMMotionAlarmManager.h>
#import <CoreMotion/CMMotionAlarmDelegateProtocol.h>
@interface PMCoreSmartPowerNapPredictor : NSObject <CMMotionAlarmDelegateProtocol>
#else
@interface PMCoreSmartPowerNapPredictor : NSObject
#endif

+ (instancetype)sharedInstance;

- (instancetype)initWithQueue:(dispatch_queue_t)queue;
- (void)evaluateCoreSmartPowerNap:(BOOL)useractive;
- (void)queryModelAndEngage;
- (void)enterCoreSmartPowerNap;
- (void)exitCoreSmartPowerNapWithReason:(NSString *)reason;
- (void)logEndOfSessionWithReason:(NSString *)reason;
- (void)handleRemoteDeviceIsNear;
- (void)updateCoreSmartPowerNapState:(BOOL)active;
- (void)updateLockState:(uint64_t)state;
- (void)updateMotionState:(BOOL)state;

/*
 Update parameters through pmtool
 */
- (void)updateRequeryDelta:(uint32_t)seconds;
- (void)updateMotionAlarmThreshold:(uint32_t)seconds;
- (void)updateMotionAlarmStartThreshold:(uint32_t)seconds;
/*
 saving defaults
 */
- (void)restoreState;
- (void)saveState:(BOOL)enabled withEndTime:(NSDate *)endTime;
- (BOOL)readStateFromDefaults;
- (NSDate *)readEndTimeFromDefaults;
@end


@interface PMCoreSmartPowerNapInterruption : NSObject
@property (retain) NSDate *start;
@property (retain) NSDate *end;
@property BOOL is_transient;

-(instancetype)initWithStart:(NSDate *)date;
@end

void setCSPNRequeryDelta(xpc_object_t remoteConnection, xpc_object_t msg);


#endif /* PMCoreSmartPowerNapPredictor_h */
