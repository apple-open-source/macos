//
//  PBKDFTest.h
//  CommonCrypto
//
//  Created by Richard Murphy on 2/3/10.
//  Copyright 2010 McKenzie-Murphy. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "CommonKeyDerivation.h"
#import "TestToolProtocol.h"

/* --------------------------------------------------------------------------
 Class: 			CCDerviationTestObject
 Description: 	This class provides for testing a PBKDF type
 -------------------------------------------------------------------------- */

@interface CCDerviationTestObject : NSObject <TestToolProtocol>
{
	NSString*		_namePBKDF;				    // The name of the PBKDF type
	CCPBKDFAlgorithm	_algoPBKDF;				// The PBKDF algorithm
	NSData*			_password;					// The password for the PBKDF
	NSData*			_salt;						// The salt for the PBKDF
	CCPseudoRandomAlgorithm _prf;				// The PRF to use with the PBKDF
	uint			_rounds;					// The number of rounds for the PBKDF
	uint8_t			_derivedKey[1024];			// Max buffer for derived key from function under test
	NSData*			_derivedResult;				// Result of the staged PBKDF
	NSData*			_expectedResult;			// The expected result
	id<TestToolProtocol> _testObject;			// The owning test object NOT retained	
	BOOL					_testPassed;

}

@property (readonly) NSString* namePBKDF;
@property (readonly) CCPBKDFAlgorithm algoPBKDF;
@property (readonly) NSData* password;
@property (readonly) NSData* salt;
@property (readonly) CCPseudoRandomAlgorithm prf;
@property (readonly) uint rounds;
@property (readonly) NSData* derivedResult;
@property (readonly) NSData* expectedResult;
@property (readonly) id<TestToolProtocol> testObject;
@property (readonly) BOOL testPassed;


+ (NSArray *)setupPBKDFTests:(id<TestToolProtocol>)testObject;

- (id)initWithPBKDFName:(NSString *)name 
		withCCPBKDFAlgorithm:(CCPBKDFAlgorithm) algo
		withPassword:(NSData *) password
		withSalt:(NSData *) salt
		withCCPseudoRandomAlgorithm:(CCPseudoRandomAlgorithm) prf
		withRounds:(uint)rounds
		withExpectedResult:(NSData *)expectedResult
		withTestObject:(id<TestToolProtocol>)testObject;

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;

- (void)runTest;


struct test_vector {
	u_int rounds;
	const char *pass;
	const char *salt;
	const char expected[32];
};
@end
