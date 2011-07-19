//
//  DigestTest.h
//  CommonCrypto
//
//  Created by Jim Murphy on 1/12/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "CommonDigest.h"
#include "CommonDigestSPI.h"
#import "TestToolProtocol.h"

/* ==========================================================================
	Type defined by this file
   ========================================================================== */

// Block definition for initializing a staged digest 
typedef int (^initBlock)(void *ctx);

// Block definition for updating a staged digest
typedef int (^updateBlock)(void *ctx, const void *data, CC_LONG len);

// Block definition for finalizing a staged digest
typedef int (^finalBlock)(unsigned char *md, void *ctx);

// Block definition for a 'one shot' digest
typedef unsigned char* (^oneShotBlock)(const void *data, CC_LONG len, unsigned char *md);

/* ==========================================================================
	Defines used by this file
   ========================================================================== */

#define MIN_DATA_SIZE	1
#define MAX_DATA_SIZE	10000			/* bytes */
#define MAX_DIGEST_SIZE		64
#define MAX_CONTEXT_SIZE	CC_DIGEST_SIZE


/* --------------------------------------------------------------------------
	Class: 			CCDigestTestObject
	Description: 	This class provides for testing a digest type
   -------------------------------------------------------------------------- */

@interface CCDigestTestObject : NSObject
{
	NSString*		_digestName;				// The name of the digest type
	size_t			_digestSize;				// The size of the digest
	initBlock		_initBlock;					// Block that initialize a staged digest
	updateBlock		_updateBlock;				// Block that updates a staged digest
	finalBlock		_finalBlock;				// Block that finalizes a staged digest
	oneShotBlock	_oneShotBlock;				// Block that does a 'one shot' digest
	NSData*			_stagedResult;				// Result of the staged digest
	NSData*			_oneShotResult;				// Result of the 'one shot' digest
	NSData*			_digestData;				// Data to be digested
	unsigned char	_context[MAX_CONTEXT_SIZE];	// Working digest buffer
	id<TestToolProtocol>
					_testObject;				// The owning test object NOT retained	
	BOOL			_testPassed;
}

@property (readonly) NSString* digestName;
@property (readonly) size_t digestSize;
@property (readonly) NSData* stagedResult;
@property (readonly) NSData* oneShotResult;
@property (readonly) NSData* digestData;
@property (readwrite, assign) id<TestToolProtocol> testObject;
@property (readonly) BOOL testPassed;


+ (NSArray *)setupDigestTests:(id<TestToolProtocol>)testObject;


- (id)initWithDigestName:(NSString *)name withDigestSize:(size_t)size 
	withInitBlock:(initBlock)initDigest
	withUpdateBlock:(updateBlock)updateDigest
	withFinalBlock:(finalBlock)completeDigest
	withOneShotBlock:(oneShotBlock)oneShotDigest;

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;

- (void)runTest;

@end
