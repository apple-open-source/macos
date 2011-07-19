//
//  PBKDFTest.mm
//  CommonCrypto
//
//  Created by Richard Murphy on 2/3/10.
//  Copyright 2010 McKenzie-Murphy. All rights reserved.
//

#import "PBKDFTest.h"
#include <stdio.h>


@implementation CCDerviationTestObject

@synthesize namePBKDF = _namePBKDF;
@synthesize algoPBKDF = _algoPBKDF;
@synthesize password = _password;
@synthesize salt = _salt;
@synthesize prf = _prf;
@synthesize rounds = _rounds;
@synthesize derivedResult = _derivedResult;
@synthesize expectedResult = _expectedResult;
@synthesize testObject = _testObject;
@synthesize testPassed = _testPassed;


/* --------------------------------------------------------------------------
 method: 		setupPBKDFTests
 returns: 		NSArray *												
 decription: 	This method allows for creating digest specific tests for
				all of the digest supported by the CommonCrypto library.
				It creates an instance of the CCDigestTestObject for
				each digest to be tested and places that object into
				an NSArray.
 -------------------------------------------------------------------------- */
+ (NSArray *)setupPBKDFTests:(id<TestToolProtocol>)testObject;
{
	
	NSMutableArray* result = [NSMutableArray array]; // autoreleased
	
	/*
	 * Test vectors from RFC 3962
	 */
	
	struct test_vector test_vectors[] = {
		{ 
			1, "password", "ATHENA.MIT.EDUraeburn", { 
				0xcd, 0xed, 0xb5, 0x28, 0x1b, 0xb2, 0xf8, 0x01, 0x56, 0x5a, 0x11, 0x22, 0xb2, 0x56, 0x35, 0x15,
				0x0a, 0xd1, 0xf7, 0xa0, 0x4b, 0xb9, 0xf3, 0xa3, 0x33, 0xec, 0xc0, 0xe2, 0xe1, 0xf7, 0x08, 0x37 },
		}, {
			2, "password", "ATHENA.MIT.EDUraeburn", { 
				0x01, 0xdb, 0xee, 0x7f, 0x4a, 0x9e, 0x24, 0x3e,  0x98, 0x8b, 0x62, 0xc7, 0x3c, 0xda, 0x93, 0x5d,
				0xa0, 0x53, 0x78, 0xb9, 0x32, 0x44, 0xec, 0x8f, 0x48, 0xa9, 0x9e, 0x61, 0xad, 0x79, 0x9d, 0x86 },
		}, {
			1200, "password", "ATHENA.MIT.EDUraeburn", { 
				0x5c, 0x08, 0xeb, 0x61, 0xfd, 0xf7, 0x1e, 0x4e, 0x4e, 0xc3, 0xcf, 0x6b, 0xa1, 0xf5, 0x51, 0x2b,
				0xa7, 0xe5, 0x2d, 0xdb, 0xc5, 0xe5, 0x14, 0x2f, 0x70, 0x8a, 0x31, 0xe2, 0xe6, 0x2b, 0x1e, 0x13 },
		}, {
			5, "password", "\0224VxxV4\022", /* 0x1234567878563412 */ { 
				0xd1, 0xda, 0xa7, 0x86, 0x15, 0xf2, 0x87, 0xe6, 0xa1, 0xc8, 0xb1, 0x20, 0xd7, 0x06, 0x2a, 0x49,
				0x3f, 0x98, 0xd2, 0x03, 0xe6, 0xbe, 0x49, 0xa6, 0xad, 0xf4, 0xfa, 0x57, 0x4b, 0x6e, 0x64, 0xee },
		}, { 1200, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "pass phrase equals block size", {
				0x13, 0x9c, 0x30, 0xc0, 0x96, 0x6b, 0xc3, 0x2b, 0xa5, 0x5f, 0xdb, 0xf2, 0x12, 0x53, 0x0a, 0xc9,
				0xc5, 0xec, 0x59, 0xf1, 0xa4, 0x52, 0xf5, 0xcc, 0x9a, 0xd9, 0x40, 0xfe, 0xa0, 0x59, 0x8e, 0xd1 },
		}, { 1200, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "pass phrase exceeds block size", {
				0x9c, 0xca, 0xd6, 0xd4, 0x68, 0x77, 0x0c, 0xd5, 0x1b, 0x10, 0xe6, 0xa6, 0x87, 0x21, 0xbe, 0x61,
				0x1a, 0x8b, 0x4d, 0x28, 0x26, 0x01, 0xdb, 0x3b, 0x36, 0xbe, 0x92, 0x46, 0x91, 0x5e, 0xc8, 0x2a },
		}, { 50, "\360\235\204\236", /* g-clef (0xf09d849e) */ "EXAMPLE.COMpianist", {
				0x6b, 0x9c, 0xf2, 0x6d, 0x45, 0x45, 0x5a, 0x43, 0xa5, 0xb8, 0xbb, 0x27, 0x6a, 0x40, 0x3b, 0x39,
				0xe7, 0xfe, 0x37, 0xa0, 0xc4, 0x1e, 0x02, 0xc2, 0x81, 0xff, 0x30, 0x69, 0xe1, 0xe9, 0x4f, 0x52 },
		}, { 1, "password", "salt", { 
				0x0c, 0x60, 0xc8, 0x0f, 0x96, 0x1f, 0x0e, 0x71, 0xf3, 0xa9, 0xb5, 0x24, 0xaf, 0x60, 0x12, 0x06, 
                0x2f, 0xe0, 0x37, 0xa6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		}
	};
	int nvecs = (sizeof(test_vectors) / sizeof(*test_vectors));
	int i;

	for(i=0; i< nvecs-1; i++) {
		CCDerviationTestObject* pbkdfHMACSha1Test = [[[CCDerviationTestObject alloc] 
		   initWithPBKDFName:@"pbkdfHMACSha1Test1" 
		   withCCPBKDFAlgorithm: kCCPBKDF2
		   withPassword: [[NSData alloc] initWithBytes:test_vectors[i].pass length:strlen(test_vectors[i].pass)]
		   withSalt: [[NSData alloc] initWithBytes:test_vectors[i].salt length:strlen(test_vectors[i].salt)]
		   withCCPseudoRandomAlgorithm: kCCPRFHmacAlgSHA1
		   withRounds:(uint)test_vectors[i].rounds
		   withExpectedResult:[[NSData alloc] initWithBytes:test_vectors[i].expected length:32]
		   withTestObject:testObject] autorelease];
		[result addObject:pbkdfHMACSha1Test];
	}
    
    CCDerviationTestObject* pbkdfHMACSha1Test = [[[CCDerviationTestObject alloc] 
                                                  initWithPBKDFName:@"pbkdfHMACSha1Test1" 
                                                  withCCPBKDFAlgorithm: kCCPBKDF2
                                                  withPassword: [[NSData alloc] initWithBytes:test_vectors[i].pass length:strlen(test_vectors[i].pass)]
                                                  withSalt: [[NSData alloc] initWithBytes:test_vectors[i].salt length:strlen(test_vectors[i].salt)]
                                                  withCCPseudoRandomAlgorithm: kCCPRFHmacAlgSHA1
                                                  withRounds:(uint)test_vectors[i].rounds
                                                  withExpectedResult:[[NSData alloc] initWithBytes:test_vectors[i].expected length:20]
                                                  withTestObject:testObject] autorelease];
    [result addObject:pbkdfHMACSha1Test];
    
	
	return result;
}


- (id)initWithPBKDFName:(NSString *)namePBKDF 
   withCCPBKDFAlgorithm:(CCPBKDFAlgorithm) algoPBKDF
		   withPassword:(NSData *) password
			   withSalt:(NSData *) salt
withCCPseudoRandomAlgorithm:(CCPseudoRandomAlgorithm) prf
			 withRounds:(uint)rounds
	 withExpectedResult:(NSData *)expectedResult
		 withTestObject:(id<TestToolProtocol>)testObject;

{	
	if ((self = [super init]))
	{
		_namePBKDF = [namePBKDF copy];
		_algoPBKDF = algoPBKDF;
		_password = [password copy];
		_salt = [salt copy];
		_prf = prf;
		_rounds = rounds;
		_expectedResult = [expectedResult copy];
		_testObject = testObject; 
		_testPassed = YES;
	}
	return self;
	
}


/* --------------------------------------------------------------------------
 method: 		dealloc
 returns: 		void												
 decription: 	Alway put away your toys when you are done playing with
				them.
 -------------------------------------------------------------------------- */
- (void)dealloc
{
	[_namePBKDF release];
	[_password release];
	[_salt release];
	[_expectedResult release];
	[_derivedResult release];
	//[_testObject release];
	[super dealloc];
}

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr
{
	if (nil != self.testObject)
	{
		[self.testObject doAssertTest:result errorString:errorStr];
		return;
	}
	
	if (_testPassed)
	{
		_testPassed = result;
	}
}

/* --------------------------------------------------------------------------
 method: 		doTest
 returns: 		void												
 decription: 	Do the 'one shot' digest creation for this test placing the
				result into the _oneShotResult member
 -------------------------------------------------------------------------- */
- (void)doVectorTest
{
	// [self clearContext];
	(void) CCKeyDerivationPBKDF(self.algoPBKDF, (const char *)[self.password bytes], [self.password length], 
						 (const uint8_t *)[self.salt bytes], [self.salt length], 
						 _prf, _rounds,
						 _derivedKey, [self.expectedResult length]);
	
	//[_derivedResult release];
	_derivedResult = [[NSData alloc] initWithBytes:_derivedKey length:[self.expectedResult length]];
	
}

/* --------------------------------------------------------------------------
 method: 		runTest
 returns: 		void												
 decription: 	Do the testing of the digest by creating both a staged 
				and one shot digest from the same data and ensuring 
				that the two digests match
 -------------------------------------------------------------------------- */
- (void)runTest
{
	[self doVectorTest];
	
	BOOL testResult = [self.expectedResult isEqualToData:self.derivedResult];
	
	[self doAssertTest:testResult errorString:[
			NSString stringWithFormat:@"Expected Result is not equal to the derived result in %@", self.namePBKDF]];
	
	if (nil == _testObject)
	{
		printf("PBKDFTest: %s\n", (self.testPassed) ? "Passed" : "Failed");
	}
}

@end

