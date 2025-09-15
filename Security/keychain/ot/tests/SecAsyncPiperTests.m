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

#import <XCTest/XCTest.h>
#import "keychain/ot/SecAsyncPiper.h"

// Helper for later macros
#define SecAsyncPiperForTestingFailHelper(X) _SecAsyncPiperForTestingFail##X * failWrapping __attribute__((objc_precise_lifetime)) = [[_SecAsyncPiperForTestingFail##X alloc] init]

// Use this macro to force `pipe` in [SecAsyncPiper init] to fail for the scope in which the macro is invoked
#define SecAsyncPiperForTestingFailPipe SecAsyncPiperForTestingFailHelper(Pipe)

@interface _SecAsyncPiperForTestingFailPipe : NSObject

-(instancetype)init;

@end

// Use this macro to force `xpc_fd_create` in [SecAsyncPiper init] to fail for the scope in which the macro is invoked
#define SecAsyncPiperForTestingFailXpcFdWrapping SecAsyncPiperForTestingFailHelper(XpcFdWrapping)

@interface _SecAsyncPiperForTestingFailXpcFdWrapping : NSObject

-(instancetype)init;

@end

@implementation _SecAsyncPiperForTestingFailPipe

-(instancetype)init {
    [SecAsyncPiper forTestingOnlySetFailPipe:YES];
    self = [super init];
    return self;
}

-(void)dealloc {
    [SecAsyncPiper forTestingOnlySetFailPipe:NO];
}

@end

@implementation _SecAsyncPiperForTestingFailXpcFdWrapping

-(instancetype)init {
    [SecAsyncPiper forTestingOnlySetFailXpcFdWrapping:YES];
    self = [super init];
    return self;
}

-(void)dealloc {
    [SecAsyncPiper forTestingOnlySetFailXpcFdWrapping:NO];
}

@end


@interface SecAsyncPiperTests : XCTestCase

@end

@implementation SecAsyncPiperTests

- (void)testFailToCreatePipe {
    SecAsyncPiperForTestingFailPipe;
    NSError* error = nil;
    SecAsyncPiper* piper = [[SecAsyncPiper alloc] initWithError:&error];
    XCTAssertNil(piper);
    XCTAssertNotNil(error);
    XCTAssert(error.domain == NSPOSIXErrorDomain);
    XCTAssert(error.code == EPERM);
}

- (void)testFailToWrapFd {
    SecAsyncPiperForTestingFailXpcFdWrapping;
    NSError* error = nil;
    SecAsyncPiper* piper = [[SecAsyncPiper alloc] initWithError:&error];
    XCTAssertNil(piper);
    XCTAssertNotNil(error);
    XCTAssert(error.domain == NSPOSIXErrorDomain);
    XCTAssert(error.code == EPERM);
}

- (void)testNoData {
    NSError* error = nil;
    SecAsyncPiper* piper = [[SecAsyncPiper alloc] initWithError:&error];
    XCTAssertNotNil(piper);
    XCTAssertNil(error);
    [piper waitAndReleaseFd_ForTestingOnly];
}

@end
