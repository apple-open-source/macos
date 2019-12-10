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
#ifndef SFSignInAnalytics_Internal_h
#define SFSignInAnalytics_Internal_h

#import "SFSignInAnalytics.h"

@interface SFSignInAnalytics (Internal)
@property (readonly) NSString *signin_uuid;
@property (readonly) NSString *my_uuid;
-(instancetype) initChildWithSignInUUID:(NSString*)signin_uuid andCategory:(NSString*)category andEventName:(NSString*)eventName;
@end

@interface SFSIALoggerObject : SFAnalytics
+ (instancetype)logger;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif /* SFSignInAnalytics+Internal_h */
#endif
