//
//  DigestTest.mm
//  CommonCrypto
//
//  Created by Jim Murphy on 1/12/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "DigestTest.h"
#import "RandomNumberService.h"
#include "CommonDigest.h"
#include "CommonDigestSPI.h"
#include <stdio.h>


@implementation CCDigestTestObject

@synthesize digestName = _digestName;
@synthesize digestSize = _digestSize;
@synthesize stagedResult = _stagedResult;
@synthesize oneShotResult = _oneShotResult;
@synthesize digestData = _digestData;
@synthesize testObject = _testObject;
@synthesize testPassed = _testPassed;


/* --------------------------------------------------------------------------
	method: 		setupDigestTests
	returns: 		NSArray *												
	decription: 	This method allows for creating digest specific tests for
					all of the digest supported by the CommonCrypto library.
					It creates an instance of the CCDigestTestObject for
					each digest to be tested and places that object into
					an NSArray.
   -------------------------------------------------------------------------- */
+ (NSArray *)setupDigestTests:(id<TestToolProtocol>)testObject;
{
	initBlock anInitBlock;
	updateBlock anUpdateBlock;
	finalBlock aFinalBlock;
	oneShotBlock anOneShotBlock;
	
	NSMutableArray* result = [NSMutableArray array]; // autoreleased
	
	// --------------------- MD2 Digest ----------------------
	anInitBlock = ^(void *ctx)
	{
		return CC_MD2_Init((CC_MD2_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_MD2_Update((CC_MD2_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_MD2_Final(md, (CC_MD2_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_MD2(data, len, md);
	};
	
	CCDigestTestObject* md2DigestTest = [[[CCDigestTestObject alloc] 
		initWithDigestName:@"MD2" 
		withDigestSize:CC_MD2_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	md2DigestTest.testObject = testObject;
		
	[result addObject:md2DigestTest];
	
	// --------------------- MD4 Digest ----------------------
	anInitBlock = ^(void *ctx)
	{
		return CC_MD4_Init((CC_MD4_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_MD4_Update((CC_MD4_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_MD4_Final(md, (CC_MD4_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_MD4(data, len, md);
	};
	
	CCDigestTestObject* md4DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"MD4" 
		withDigestSize:CC_MD4_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	md4DigestTest.testObject = testObject;
		
	[result addObject:md4DigestTest];
	
	// --------------------- MD5 Digest ----------------------
	
	anInitBlock = ^(void *ctx)
	{
		return CC_MD5_Init((CC_MD5_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_MD5_Update((CC_MD5_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_MD5_Final(md, (CC_MD5_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_MD5(data, len, md);
	};
	
	CCDigestTestObject* md5DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"MD5" 
		withDigestSize:CC_MD5_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	md5DigestTest.testObject = testObject;
		
	[result addObject:md5DigestTest];
	
	// --------------------- SHA1 Digest ----------------------
		
	anInitBlock = ^(void *ctx)
	{
		return CC_SHA1_Init((CC_SHA1_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_SHA1_Update((CC_SHA1_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_SHA1_Final(md, (CC_SHA1_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_SHA1(data, len, md);
	};
	
	CCDigestTestObject* sha1DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"SHA1" 
		withDigestSize:CC_SHA1_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	sha1DigestTest.testObject = testObject;
		
	[result addObject:sha1DigestTest];
	
	// --------------------- SHA224 Digest ----------------------
	
	anInitBlock = ^(void *ctx)
	{
		return CC_SHA224_Init((CC_SHA256_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_SHA224_Update((CC_SHA256_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_SHA224_Final(md, (CC_SHA256_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_SHA224(data, len, md);
	};
	
	CCDigestTestObject* sha224DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"SHA224" 
		withDigestSize:CC_SHA224_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	sha224DigestTest.testObject = testObject;
		
	[result addObject:sha224DigestTest];
	
	// --------------------- SHA256 Digest ----------------------
	
	anInitBlock = ^(void *ctx)
	{
		return CC_SHA256_Init((CC_SHA256_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_SHA256_Update((CC_SHA256_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_SHA256_Final(md, (CC_SHA256_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_SHA256(data, len, md);
	};
	
	CCDigestTestObject* sha256DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"SHA256" 
		withDigestSize:CC_SHA256_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	sha224DigestTest.testObject = testObject;
		
	[result addObject:sha256DigestTest];
	
	// --------------------- SHA384 Digest ----------------------
	
	anInitBlock = ^(void *ctx)
	{
		return CC_SHA384_Init((CC_SHA512_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_SHA384_Update((CC_SHA512_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_SHA384_Final(md, (CC_SHA512_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_SHA384(data, len, md);
	};
	
	CCDigestTestObject* sha384DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"SHA384" 
		withDigestSize:CC_SHA384_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	sha384DigestTest.testObject = testObject;
		
	[result addObject:sha384DigestTest];
	
	// --------------------- SHA512 Digest ----------------------
	
	anInitBlock = ^(void *ctx)
	{
		return CC_SHA512_Init((CC_SHA512_CTX *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CC_SHA512_Update((CC_SHA512_CTX *)ctx, data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CC_SHA512_Final(md, (CC_SHA512_CTX *)ctx);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return CC_SHA512(data, len, md);
	};
	
	CCDigestTestObject* sha512DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"SHA512" 
		withDigestSize:CC_SHA512_DIGEST_LENGTH 
		withInitBlock:anInitBlock
		withUpdateBlock:anUpdateBlock
		withFinalBlock:aFinalBlock
		withOneShotBlock:anOneShotBlock] autorelease];
		
	sha512DigestTest.testObject = testObject;
		
	[result addObject:sha512DigestTest];
	
	// --------------------- Skein512 Digest ----------------------

		
	anInitBlock = ^(void *ctx)
	{
		return CCDigestInit(kCCDigestSkein512, (CCDigestCtx *)ctx);
	};
	
	anUpdateBlock = ^(void *ctx, const void *data, CC_LONG len)
	{
		return CCDigestUpdate((CCDigestCtx *)ctx, (const uint8_t *) data, len);
	};
	
	aFinalBlock = ^(unsigned char *md, void *ctx)
	{
		return CCDigestFinal((CCDigestCtx *)ctx, md);
	};
	
	anOneShotBlock = ^(const void *data, CC_LONG len, unsigned char *md)
	{
		return (unsigned char *) CCDigest(kCCDigestSkein512, (const uint8_t *)data, len, (uint8_t *)md);
	};
	
	CCDigestTestObject* skein512DigestTest = [[[CCDigestTestObject alloc] initWithDigestName:@"Skein512 (CommonHash)" 
																			withDigestSize:CC_SHA512_DIGEST_LENGTH 
																			 withInitBlock:anInitBlock
																		   withUpdateBlock:anUpdateBlock
																			withFinalBlock:aFinalBlock
																		  withOneShotBlock:anOneShotBlock] autorelease];
	
	skein512DigestTest.testObject = testObject;
	
	[result addObject:skein512DigestTest];
	

	
	return result;
}


/* --------------------------------------------------------------------------
	method: 		initWithDigestName:withDigestSize:withInitBlock:withUpdateBlock:withFinalBlock:withOneShotBlock:
	returns: 		new CCDigestTestObject object
	parameters:
					name:
						Then name of the digest type i.e. SHA1
					size:
						The size in bytes of the digest for the specified type
					initDigest:
						A block to initialize a staged digest
					updateDigest:
						A block to update a staged digest
					completeDigest:
						A block to finalize a staged digest
					oneShotDigest:
						A block to do a 'one shot' digest
						
												
	decription: 	Initalize a new Digest testing object
   -------------------------------------------------------------------------- */
- (id)initWithDigestName:(NSString *)name withDigestSize:(size_t)size 
	withInitBlock:(initBlock)initDigest
	withUpdateBlock:(updateBlock)updateDigest
	withFinalBlock:(finalBlock)completeDigest
	withOneShotBlock:(oneShotBlock)oneShotDigest
{
	if ((self = [super init]))
	{
		_testPassed = YES;
		[self doAssertTest:(NULL != name) errorString:@"CCDigestTestObject.init received a nil Name"];
		[self doAssertTest:(size > 0) errorString:@"CCDigestTestObject.init got a 0 buffer size"];
		[self doAssertTest:(0 != initDigest) errorString:@"CCDigestTestObject.init received a NULL InitBlock"];
		[self doAssertTest:(0 != updateDigest) errorString:@"CCDigestTestObject.init received a NULL UpdateBlock"];
		[self doAssertTest:(0 != completeDigest) errorString:@"CCDigestTestObject.init received a NULL CompleteDigestBlock"];
		[self doAssertTest:(0 != oneShotDigest) errorString:@"CCDigestTestObject.init received a NULL OneShotBlock"];
		
		_digestName = [name copy];
		_digestSize = size;
		_initBlock = [initDigest copy];
		_updateBlock = [updateDigest copy];
		_finalBlock = [completeDigest copy];
		_oneShotBlock = [oneShotDigest copy];
		
		// Create the data that will be digested by this test.
		CCRandomNumberService* randNumService = [CCRandomNumberService defaultRandomNumberService];
		unsigned int randomDataLength = [randNumService generateRandomNumberInRange:MIN_DATA_SIZE toMax:MAX_DATA_SIZE];
		_digestData = [[randNumService generateRandomDataOfSize:randomDataLength] retain];
		memset(_context, 0, MAX_CONTEXT_SIZE);
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
	[_digestName release];
	[_stagedResult release];
	[_oneShotResult release];
	[_digestData release];
	[_initBlock release];
	[_updateBlock release];
	[_finalBlock release];
	[_oneShotBlock release];
	[super dealloc];
}

/* --------------------------------------------------------------------------
	method: 		doStaged
	returns: 		void												
	decription: 	Do the staged digest creation for this test placing the
					result into the _stagedResult member
   -------------------------------------------------------------------------- */
- (void)doStaged
{
	
	unsigned int thisMove;
	memset(_context, 0, MAX_CONTEXT_SIZE);
	
	_initBlock(_context);
	
	unsigned int dataLength = [self.digestData length];
	const unsigned char* raw_bytes = (const unsigned char*)[self.digestData bytes];
	
	unsigned char	mdBuffer[MAX_DIGEST_SIZE];
	memset(mdBuffer, 0, MAX_DIGEST_SIZE);
		
	
	CCRandomNumberService* randNumService = [CCRandomNumberService defaultRandomNumberService];
		
	while (dataLength)
	{
		thisMove = [randNumService generateRandomNumberInRange:1 toMax:dataLength];
		_updateBlock(_context, raw_bytes, thisMove);
		
		raw_bytes += thisMove;
		
		dataLength -= thisMove;
	}
	
	(void)_finalBlock(mdBuffer, _context);
	[_stagedResult release];
	_stagedResult = [[NSData alloc] initWithBytes:mdBuffer length:_digestSize];
	
}

/* --------------------------------------------------------------------------
	method: 		doOneShot
	returns: 		void												
	decription: 	Do the 'one shot' digest creation for this test placing the
					result into the _oneShotResult member
   -------------------------------------------------------------------------- */
- (void)doOneShot
{
	unsigned char mdBuffer[MAX_DIGEST_SIZE];
	memset(mdBuffer, 0, MAX_DIGEST_SIZE);
	_oneShotBlock([self.digestData bytes], [self.digestData length], (unsigned char *)mdBuffer);
	[_oneShotResult release];
	_oneShotResult = [[NSData alloc] initWithBytes:mdBuffer length:_digestSize];
	
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
	method: 		runTest
	returns: 		void												
	decription: 	Do the testing of the digest by creating both a staged 
					and one shot digest from the same data and ensuring 
					that the two digests match
   -------------------------------------------------------------------------- */
- (void)runTest
{
	[self doOneShot];
	[self doStaged];
	
	BOOL testResult = [self.stagedResult isEqualToData:self.oneShotResult];
	
	[self doAssertTest:testResult errorString:[
		NSString stringWithFormat:@"Staged Result is not equal to the one shot result for digest type %@", self.digestName]];
	
	if (nil == _testObject)
	{
		printf("DigestTest: %s\n", (self.testPassed) ? "Passed" : "Failed");
	}
}

@end

