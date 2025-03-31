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

#ifndef AyncPiper_h
#define AsyncPiper_h

NS_ASSUME_NONNULL_BEGIN

@interface AsyncPiper : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

-(instancetype)initWithError:(NSError** _Nullable)error NS_DESIGNATED_INITIALIZER;
-(xpc_object_t)xpcFd;
-(void)waitAndReleaseFd_ForTestingOnly;
-(NSDictionary*)dictWithError:(NSError**)errorOut;

@end

// Helper for later macros
#define AsyncPiperForTestingFailHelper(X) _AsyncPiperForTestingFail##X * failWrapping __attribute__((objc_precise_lifetime)) = [[_AsyncPiperForTestingFail##X alloc] init]

// Use this macro to force `pipe` in [AsyncPiper init] to fail for the scope in which the macro is invoked
#define AsyncPiperForTestingFailPipe AsyncPiperForTestingFailHelper(Pipe)

@interface _AsyncPiperForTestingFailPipe : NSObject

-(instancetype)init;

@end

// Use this macro to force `xpc_fd_create` in [AsyncPiper init] to fail for the scope in which the macro is invoked
#define AsyncPiperForTestingFailXpcFdWrapping AsyncPiperForTestingFailHelper(XpcFdWrapping)

@interface _AsyncPiperForTestingFailXpcFdWrapping : NSObject

-(instancetype)init;

@end

#endif /* AsyncPiper_h */

NS_ASSUME_NONNULL_END

#endif // ___OBJC2__
