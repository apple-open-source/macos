//
//  CommonCryptoUnitTests.mm
//  CommonCrypto
//
//  Created by Jim Murphy on 1/11/10.
//  Copyright 2010 Apple Inc. All rights reserved.
//

#import "CommonCryptoUnitTests.h"
#import "DigestTest.h"
#include "CommonDigest.h"
#include "CommonCryptor.h"
#include "CommonHMAC.h"
#include "RandomNumberService.h"
#import "EncryptionTest.h"
#import "HMACTest.h"
#import "PBKDFTest.h"
#import "SymmetricWrapTest.h"
#import "CommonRandomSPI.h"


// - (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;
/* --------------------------------------------------------------------------
	class: 			CommonCryptoUnitTests
	description: 	Implementation of the unit tests for the CommonCrypto
					library	
   -------------------------------------------------------------------------- */
@implementation CommonCryptoUnitTests

/* --------------------------------------------------------------------------
	method: 		-(void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr
	decription: 	Provide a way for class that is NOT subclassed from 
					SenTestCase have an assert
   -------------------------------------------------------------------------- */
-(void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr	
{
	STAssertTrue(result, errorStr);
}

/* --------------------------------------------------------------------------
	method: 		- (void)testDigests
	decription: 	Test all of the digest algorithms in the CommonCrypto
					library
   -------------------------------------------------------------------------- */
- (void)testDigests	
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
	
	NSLog(@"%@", @"In the testDigest method");
	
	NSArray* digestTests = [CCDigestTestObject setupDigestTests:self];
	
	for (CCDigestTestObject* testObject in digestTests)
	{
		NSLog(@"Running test for %@", testObject.digestName);
		[testObject runTest];
	}
	
	[pool drain];
}

/* --------------------------------------------------------------------------
	method: 		- (void)testEncryption
	decription: 	Test all of the encryption algorithms in the CommonCrypto
					library
   -------------------------------------------------------------------------- */
- (void)testEncryption	
{
	
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
	
	NSLog(@"%@", @"In the testEncryption method");
	NSArray* encryptionTests = [CCEncryptionTest setupEncryptionTests:self];
								
	for (CCEncryptionTest* aTest in encryptionTests)
	{
		NSLog(@"Running test for %@", aTest.algName);
		[aTest runTest];
	}
	
		
	[pool drain];
	
}

/* --------------------------------------------------------------------------
	method: 		- (void)testHMAC
	decription: 	Test all of the HMAC algorithms in the CommonCrypto library
   -------------------------------------------------------------------------- */
- (void)testHMAC
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
	
	NSLog(@"%@", @"In the testHMAC method");
	NSArray* hmacTests = [CCHMACTestObject setupHMACTests:self];
	
	for (CCHMACTestObject* aTest in hmacTests)
	{
		NSLog(@"Running test for %@", aTest.nameHMAC);
		[aTest runTest];
	}
	
	
	[pool drain];
}

/* --------------------------------------------------------------------------
 method: 		- (void)testPBKDF
 decription: 	Test all of the PBKDF algorithms in the CommonCrypto library
 -------------------------------------------------------------------------- */
- (void)testPBKDF
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
	
	NSLog(@"%@", @"In the testPBKDF method");
	printf("Starting\n");
	NSArray* pbkdfTests = [CCDerviationTestObject setupPBKDFTests:self];
	
	for (CCDerviationTestObject* aTest in pbkdfTests)
	{
		NSLog(@"Running test for %@", aTest.namePBKDF);
		[aTest runTest];
	}
	
	
	[pool drain];
}


/* --------------------------------------------------------------------------
 method: 		- (void)testSymmetricWrap
 decription: 	Test all of the SymmetricWrap algorithms in the CommonCrypto library
 -------------------------------------------------------------------------- */
- (void)testSymmetricWrap
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
	
	NSLog(@"%@", @"In the testSymmetricWrap method");
	NSArray* SymmetricWrapTests = [CCSymmetricalWrapTest setupSymmWrapTests:self];
	
	for (CCSymmetricalWrapTest* aTest in SymmetricWrapTests)
	{
		NSLog(@"%@", @"About to call the unit test");
		[aTest runTest];
	}
	
	[pool drain];
}

/* --------------------------------------------------------------------------
 method: 		- (void)testRandomCopyBytes
 decription: 	Test the main PRNG for non-repeatability
 -------------------------------------------------------------------------- */
- (void)testRandomCopyBytes
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
    uint8_t bytes[1024];
    uint8_t previous[1024];
    int i;
    
	NSLog(@"%@", @"In the testRandomCopyBytes method");
    bzero(previous, 1024);
    for(i=0; i<1024; i++) {
        int retval =  CCRandomCopyBytes(kCCRandomDefault, bytes, 1024);
        if(retval) {
            NSLog(@"%@", @"Failed call to CCRandomCopyBytes");
        }
        
        if(memcmp(previous, bytes, 1024) == 0) {
            NSLog(@"%@", @"Failed - random bytes match (1024 bytes)");
        }
        
        memcpy(previous, bytes, 1024);
    }

	
	[pool drain];
}


@end


