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
#import <SystemConfiguration/SystemConfiguration.h>

@class CKKSResultOperation;

@interface CKKSReachabilityTracker : NSObject
@property CKKSResultOperation* reachabilityDependency;
@property (readonly) bool currentReachability; // get current reachability value w/o recheck

- (instancetype)init;
- (void)recheck;
- (bool)isNetworkError:(NSError *)error;

// only for testing override, the method will be call sync on an internal queue,
// so take that into consideration when you mock this class method.
+ (SCNetworkReachabilityFlags)getReachabilityFlags:(SCNetworkReachabilityRef)target;

@end

#endif  // OCTAGON

