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
#ifndef SignInAnalytics_h
#define SignInAnalytics_h

#import <Foundation/Foundation.h>
#import <Security/SFAnalytics.h>

NS_ASSUME_NONNULL_BEGIN

@interface SFSignInAnalytics : NSObject <NSSecureCoding>

@property (readonly) NSString* eventName;
@property (readonly) NSString* category;
@property (readonly) BOOL stopped;

/*
 abstract: creates a new SignInAnalytics object, automatically starts a timer for this task.
 uuid: iCloud sign in transaction UUID
 category: name of client subsystem.  This will be used as the category name when logging
 eventName: name of the event we are measuring
 */
- (instancetype _Nullable)initWithSignInUUID:(NSString *)uuid category:(NSString *)category eventName:(NSString*)eventName;
- (instancetype)init NS_UNAVAILABLE;

/*
 abstract: creates a new SignInAnalytics that starts a timer for the subtask.
           ideal for fine grained timing of sub events and automatically creates a dependency chain.
 eventNmae: name of the event being timed
 */
- (SFSignInAnalytics* _Nullable)newSubTaskForEvent:(NSString*)eventName;

/*
 abstract: call to log when a recoverable error occurs during sign in
 error: error that occured during iCloud Sign in
 */
- (void)logRecoverableError:(NSError*)error;

/*
 abstract: call to log when a unrecoverable error occurs during sign in
 error: error that occured during iCloud Sign in
 */
- (void)logUnrecoverableError:(NSError*)error;

/*
 abstract: call to cancel the timer object.
 */
- (void)cancel;

/*
 abstract: call to stop a timer and log the time spent.
 eventName: subsystem name
 attributes: a dictionary containing event attributes
 */
- (void)stopWithAttributes:(NSDictionary<NSString*, id>* _Nullable)attributes;

/*
 abstract: call to signal iCloud sign in has finished.
 */
- (void)signInCompleted;

@end
NS_ASSUME_NONNULL_END
#endif /* SignInAnalytics_h */
#endif
