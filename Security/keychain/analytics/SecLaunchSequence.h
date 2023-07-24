/*
 * Copyright (c) 2019, 2023 Apple Inc. All Rights Reserved.
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
#if __OBJC__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 *  Takes a sequence of events and report their time relative from the starting point
 *  Duplicate events are counted.
 */

@interface SecLaunchSequence : NSObject
@property (readonly) bool launched;
@property (assign) bool firstLaunch;
@property (readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;

// name should be dns reverse notation, com.apple.label
- (instancetype)initWithRocketName:(NSString *)name;

// value must be a valid JSON compatible type
- (void)addAttribute:(NSString *)key value:(id)value;
- (void)addEvent:(NSString *)eventname;

- (void)launch;

- (void)addDependantLaunch:(NSString *)name child:(SecLaunchSequence *)child;

- (NSArray *) eventsRelativeTime;
- (NSDictionary<NSString*,id>* _Nullable) metricsReport;

// For including in human readable diagnostics
- (NSArray<NSString *> *)eventsByTime;
@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */
