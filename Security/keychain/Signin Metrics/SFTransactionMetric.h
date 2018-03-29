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

#if __OBJC2__
#ifndef SFTransactionMetric_h
#define SFTransactionMetric_h

#import <Foundation/Foundation.h>

@interface SFTransactionMetric : NSObject <NSSecureCoding>

/*
   abstract: creates a new SFTransactionMetric object
   uuid: iCloud sign in transaction UUID
   category: name of client subsystem.  This will be used as the category name when logging
*/
- (instancetype)initWithUUID:(NSString *)uuid category:(NSString *)category;

/*
   abstract: log when a particular event occurs ex. piggybacking, restore from backup
   eventName: name of the event that occured
   arguments: dictionary containing a set of event attributes a subsystem wishes to log.
*/
- (void)logEvent:(NSString*)eventName eventAttributes:(NSDictionary<NSString*, id>*)attributes;

/*
   abstract: call to time tasks that take a while (backup, wait for initial sync)
   eventName: name of the event that occured
   blockToTime: the block of code you wish to have timed
*/
- (void)timeEvent:(NSString*)eventName blockToTime:(void(^)(void))blockToTime;

/*
   abstract: call to log when a error occurs during sign in
   error: error that occured during iCloud Sign in
*/
- (void)logError:(NSError*)error;

/*
   abstract: call to signal iCloud sign in has finished.
 */
- (void)signInCompleted;

@end


#endif /* SFTransactionMetric_h */
#endif
