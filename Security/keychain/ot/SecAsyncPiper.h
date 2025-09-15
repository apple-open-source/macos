/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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

#ifndef SecAsyncPiper_h
#define SecAsyncPiper_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SecAsyncPiper : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

-(instancetype)initWithError:(NSError** _Nullable)error NS_DESIGNATED_INITIALIZER;
-(xpc_object_t)xpcFd;
-(void)waitAndReleaseFd_ForTestingOnly;
-(NSDictionary*)dictWithError:(NSError**)errorOut;

+(void)forTestingOnlySetFailPipe:(BOOL)yorn;
+(void)forTestingOnlySetFailXpcFdWrapping:(BOOL)yorn;

@end

NS_ASSUME_NONNULL_END

#endif /* SecAsyncPiper_h */

#endif // ___OBJC2__
