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
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import "keychain/ot/OTEscrowKeys.h"
#import "keychain/ckks/CKKS.h"
#import "OTTestsBase.h"

static NSString* const testDSID = @"123456789";

static const uint8_t signingKey_384[] = {
    0x04, 0xe4, 0x1b, 0x3e, 0x88, 0x81, 0x9f, 0x3b, 0x80, 0xd0, 0x28, 0x1c,
    0xd9, 0x07, 0xa0, 0x8c, 0xa1, 0x89, 0xa8, 0x3b, 0x69, 0x91, 0x17, 0xa7,
    0x1f, 0x00, 0x31, 0x91, 0x82, 0x89, 0x1f, 0x5c, 0x44, 0x2d, 0xd6, 0xa8,
    0x22, 0x1f, 0x22, 0x7d, 0x27, 0x21, 0xf2, 0xc9, 0x75, 0xf2, 0xda, 0x41,
    0x61, 0x55, 0x29, 0x11, 0xf7, 0x71, 0xcf, 0x66, 0x52, 0x2a, 0x27, 0xfe,
    0x77, 0x1e, 0xd4, 0x3d, 0xfb, 0xbc, 0x59, 0xe4, 0xed, 0xa4, 0x79, 0x2a,
    0x9b, 0x73, 0x3e, 0xf4, 0xf4, 0xe3, 0xaf, 0xf2, 0x8d, 0x34, 0x90, 0x92,
    0x47, 0x53, 0xd0, 0x34, 0x1e, 0x49, 0x87, 0xeb, 0x11, 0x89, 0x0f, 0x9c,
    0xa4, 0x99, 0xe8, 0x4f, 0x39, 0xbe, 0x21, 0x94, 0x88, 0xba, 0x4c, 0xa5,
    0x6a, 0x60, 0x1c, 0x2f, 0x77, 0x80, 0xd2, 0x73, 0x14, 0x33, 0x46, 0x5c,
    0xda, 0xee, 0x13, 0x8a, 0x3a, 0xdb, 0x4e, 0x05, 0x4d, 0x0f, 0x6d, 0x96,
    0xcd, 0x28, 0xab, 0x52, 0x4c, 0x12, 0x2b, 0x79, 0x80, 0xfe, 0x9a, 0xe4,
    0xf4
};

static const uint8_t encryptionKey_384[] = {
    0x04, 0x99, 0xf9, 0x9a, 0x9b, 0x48, 0xe2, 0xf8, 0x69, 0xd3, 0xf9, 0x60,
    0xa0, 0xf4, 0x86, 0xda, 0xb3, 0x35, 0x3d, 0x97, 0x7d, 0xc3, 0xf4, 0x13,
    0x24, 0x78, 0x06, 0x10, 0xd5, 0x46, 0x55, 0x7a, 0x8a, 0x4d, 0x80, 0x0d,
    0x71, 0x19, 0x46, 0x4b, 0x15, 0x93, 0x36, 0xb0, 0xf4, 0x6e, 0x41, 0x30,
    0x09, 0x55, 0x25, 0x3b, 0x06, 0xdd, 0xf8, 0x85, 0xdc, 0xf2, 0x0b, 0xc7,
    0x33, 0x21, 0x99, 0x3c, 0x79, 0xa6, 0xb1, 0x0f, 0xf0, 0x55, 0xfa, 0xe8,
    0x6d, 0x3f, 0x0d, 0x57, 0x21, 0x08, 0xd2, 0x7e, 0x73, 0x4a, 0xe7, 0x4a,
    0xb3, 0xdf, 0xed, 0x86, 0x06, 0xa6, 0xf2, 0x03, 0xe6, 0x20, 0xd4, 0x82,
    0x39, 0x29, 0xcf, 0x6d, 0x76, 0x3e, 0x9a, 0xaa, 0x29, 0x4f, 0x33, 0x84,
    0x5a, 0x38, 0x50, 0x35, 0xca, 0x3f, 0x69, 0x92, 0xb1, 0xb3, 0x8b, 0x26,
    0x2b, 0xb5, 0xd6, 0x25, 0xcf, 0x2d, 0x18, 0xc4, 0x5e, 0x24, 0x34, 0xc5,
    0xcc, 0x83, 0x2f, 0xff, 0x08, 0x85, 0x0f, 0x89, 0xb5, 0xb1, 0xc1, 0x17,
    0x2a
};

static const uint8_t symmetricKey_384[] = {
    0x31, 0xf1, 0xe3, 0x7b, 0x76, 0x3f, 0x99, 0x65, 0x74, 0xab, 0xe8, 0x2b,
    0x8f, 0x06, 0x78, 0x57, 0x1b, 0xaa, 0x07, 0xb3, 0xab, 0x79, 0x81, 0xcb,
    0xc5, 0x89, 0x1e, 0x78, 0x28, 0x8d, 0x8e, 0x36
};

@interface UnitTestEscrowKeys : OTTestsBase

@end

@implementation UnitTestEscrowKeys

- (void)setUp
{
    [super setUp];
    NSError *error = nil;

    self.continueAfterFailure = NO;
    NSString* secretString = @"I'm a secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret";
    
    self.secret = [[NSData alloc]initWithBytes:[secretString UTF8String] length:[secretString length]];
    self.escrowKeys = [[OTEscrowKeys alloc]initWithSecret:self.secret dsid:testDSID error:&error];
    
    XCTAssertNil(error, @"error should be initialized");
    XCTAssertNotNil(self.escrowKeys, @"escrow keys should be initialized");
}

- (void)tearDown
{
    [super tearDown];
}

-(void) testEscrowKeyAllocations
{
    XCTAssertNotNil(self.escrowKeys.symmetricKey, @"escrowed symmetric key pair should not be nil");
    XCTAssertNotNil(self.escrowKeys.secret, @"escrowed secret should not be nil");
    XCTAssertNotNil(self.escrowKeys.dsid, @"account dsid should not be nil");
    XCTAssertNotNil(self.escrowKeys.signingKey, @"escrowed signing key should not be nil");
    XCTAssertNotNil(self.escrowKeys.encryptionKey, @"escrowed encryption key should not be nil");
}
-(void) testEscrowKeyTestVectors
{
    NSError* error = nil;

    //test vectors
    NSData* testv1 = [OTEscrowKeys generateEscrowKey:kOTEscrowKeySigning masterSecret:self.secret dsid:testDSID error:&error];
    NSData* signingFromBytes = [[NSData alloc] initWithBytes:signingKey_384 length:sizeof(signingKey_384)];
    XCTAssertTrue([testv1 isEqualToData:signingFromBytes], @"signing keys should match");
    
    NSData* testv2 = [OTEscrowKeys generateEscrowKey:kOTEscrowKeyEncryption masterSecret:self.secret dsid:testDSID error:&error];
    NSData* encryptionFromBytes = [[NSData alloc] initWithBytes:encryptionKey_384 length:sizeof(encryptionKey_384)];
    XCTAssertTrue([testv2 isEqualToData:encryptionFromBytes], @"encryption keys should match");
    
    NSData* testv3 = [OTEscrowKeys generateEscrowKey:kOTEscrowKeySymmetric masterSecret:self.secret dsid:testDSID error:&error];
    NSData* symmetricKeyFromBytes = [[NSData alloc]initWithBytes:symmetricKey_384 length:sizeof(symmetricKey_384)];
    XCTAssertTrue([testv3 isEqualToData:symmetricKeyFromBytes], @"symmetric keys should match");
    
    NSString* newSecretString = @"I'm f secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret";
    NSData* newSecret = [[NSData alloc]initWithBytes:[newSecretString UTF8String] length:[newSecretString length]];
    
    NSData* testv4 = [OTEscrowKeys generateEscrowKey:kOTEscrowKeySigning masterSecret:newSecret dsid:testDSID error:&error];
    XCTAssertFalse([testv4 isEqualToData:signingFromBytes], @"signing keys should not match");
    
    NSData* testv5 = [OTEscrowKeys generateEscrowKey:kOTEscrowKeyEncryption masterSecret:newSecret dsid:testDSID error:&error];
    XCTAssertFalse([testv5 isEqualToData:encryptionFromBytes], @"encryption keys should not match");
    
    NSData* testv6 = [OTEscrowKeys generateEscrowKey:kOTEscrowKeySymmetric masterSecret:newSecret dsid:testDSID error:&error];
    XCTAssertFalse([testv6 isEqualToData:symmetricKeyFromBytes], @"symmetric keys should not match");
}

-(void) testEmptyArguments
{
    NSError* error = nil;
    OTEscrowKeys* newSet = [[OTEscrowKeys alloc] initWithSecret:[NSData data] dsid:testDSID error:&error];
    XCTAssertNotNil(error, @"error should be initialized");
    XCTAssertNil(newSet, @"escrow keys should not be initialized");
    
    newSet = [[OTEscrowKeys alloc] initWithSecret:self.secret dsid:[NSString string] error:&error];
    XCTAssertNotNil(error, @"error should be initialized");
    XCTAssertNil(newSet, @"escrow keys should not be initialized");
}


@end

#endif /* OCTAGON */

