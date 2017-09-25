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

#import <XCTest/XCTest.h>
#import <TrustedPeers/TrustedPeers.h>

@interface TPHashTests : XCTestCase

@property (nonatomic, strong) NSData *hello;

@end

@implementation TPHashTests

- (void)setUp
{
    self.hello = [@"hello" dataUsingEncoding:NSUTF8StringEncoding];
}

- (void)testSHA224
{
    NSString *hash = [TPHashBuilder hashWithAlgo:kTPHashAlgoSHA224 ofData:self.hello];
    XCTAssertEqualObjects(hash, @"SHA224:6gmunMZ2jFD87pA+0FRVblv8g0eQfxJZiqJBkw==");
    TPHashAlgo algo = [TPHashBuilder algoOfHash:hash];
    XCTAssertEqual(kTPHashAlgoSHA224, algo);
}

- (void)testSHA256
{
    NSString *hash = [TPHashBuilder hashWithAlgo:kTPHashAlgoSHA256 ofData:self.hello];
    XCTAssertEqualObjects(hash, @"SHA256:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=");
    TPHashAlgo algo = [TPHashBuilder algoOfHash:hash];
    XCTAssertEqual(kTPHashAlgoSHA256, algo);
}

- (void)testSHA384
{
    NSString *hash = [TPHashBuilder hashWithAlgo:kTPHashAlgoSHA384 ofData:self.hello];
    XCTAssertEqualObjects(hash, @"SHA384:WeF0h3dEjGnea4ANejO7+5/xtGPkQ1TDVTvNucZm+pASWjx5+QOXvfX2oT3oKGhP");
    TPHashAlgo algo = [TPHashBuilder algoOfHash:hash];
    XCTAssertEqual(kTPHashAlgoSHA384, algo);
}

- (void)testSHA512
{
    NSString *hash = [TPHashBuilder hashWithAlgo:kTPHashAlgoSHA512 ofData:self.hello];
    XCTAssertEqualObjects(hash, @"SHA512:m3HSJL1i83hdltRq0+o9czGb+8KJDKra4t/3JRlnPKcjI8PZm6XBHXx6zG4UuMXaDEZjR1wuXDre9G9zvN7AQw==");
    TPHashAlgo algo = [TPHashBuilder algoOfHash:hash];
    XCTAssertEqual(kTPHashAlgoSHA512, algo);
}

- (void)testBadAlgo
{
    XCTAssertEqual(kTPHashAlgoUnknown, [TPHashBuilder algoOfHash:@""]);
    XCTAssertEqual(kTPHashAlgoUnknown, [TPHashBuilder algoOfHash:@"foo"]);
    XCTAssertEqual(kTPHashAlgoUnknown, [TPHashBuilder algoOfHash:@"foo:"]);
    XCTAssertEqual(kTPHashAlgoUnknown, [TPHashBuilder algoOfHash:@":"]);
    XCTAssertEqual(kTPHashAlgoUnknown, [TPHashBuilder algoOfHash:@"foo:bar"]);
    XCTAssertEqual(kTPHashAlgoUnknown, [TPHashBuilder algoOfHash:@"SHA256"]);
    
    XCTAssertThrows([TPHashBuilder hashWithAlgo:kTPHashAlgoUnknown ofData:self.hello]);
}

- (void)testBadReuse
{
    TPHashBuilder *builder = [[TPHashBuilder alloc] initWithAlgo:kTPHashAlgoSHA256];
    [builder finalHash];
    XCTAssertThrows([builder updateWithData:self.hello]);
    XCTAssertThrows([builder finalHash]);
}

@end
