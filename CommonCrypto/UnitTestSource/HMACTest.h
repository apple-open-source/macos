//
//  HMACTest.h
//  CommonCrypto
//
//  Created by Jim Murphy on 1/14/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "CommonHMAC.h"
#import "TestToolProtocol.h"

/* --------------------------------------------------------------------------
	Class: 			CCHMACTestObject
	Description: 	This class provides for testing a HMAC type
   -------------------------------------------------------------------------- */

@interface CCHMACTestObject : NSObject
{
	NSString*		_nameHMAC;				    // The name of the HMAC type
	CCHmacAlgorithm	_algoHMAC;					// The HMAC algorithm
	NSData*			_keyMaterial;				// The key for the HMAC
	NSData*			_stagedResult;				// Result of the staged HMAC
	NSData*			_oneShotResult;				// Result of the 'one shot' HMAC
	NSData*			_dataHMAC;				    // Data to be HMACed
	unsigned int	_digestBufferSize;			// The size of the output buffer
	void*			_digestBuffer;				// The output buffer for the digest
	CCHmacContext	_context;				    // Working HMAC buffer
	id<TestToolProtocol>
					_testObject;				// The owning test object NOT retained	
	BOOL			_testPassed;
}

@property (readonly) NSString* nameHMAC;
@property (readonly) CCHmacAlgorithm algoHMAC;
@property (readonly) NSData* keyMaterial;
@property (readonly) NSData* stagedResult;
@property (readonly) NSData* oneShotResult;
@property (readonly) NSData* dataHMAC;
@property (readonly) void* digestBuffer;
@property (readonly) id<TestToolProtocol> testObject;
@property (readonly) BOOL testPassed;


+ (NSArray *)setupHMACTests:(id<TestToolProtocol>)testObject;

- (id)initWithHMACName:(NSString *)name 
 	withCCHmacAlgorithm:(CCHmacAlgorithm)algo
	withDigestSize:(unsigned int)digestSize
	withTestObject:(id<TestToolProtocol>)testObject;

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;
	
- (void)runTest;

@end
