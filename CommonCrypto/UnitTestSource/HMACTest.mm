//
//  HMACTest.mm
//  CommonCrypto
//
//  Created by Jim Murphy on 1/14/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "HMACTest.h"
#import "RandomNumberService.h"
#import "CommonHMAC.h"
#include <stdio.h>

// completely arbitrary
#define kMIN_KEY_LENGTH 0
#define kMAX_KEY_LENGTH 512

#define kMIN_DATA_LENGTH 16
#define kMAC_DATA_LENGTH 0x8000

@implementation CCHMACTestObject

@synthesize nameHMAC = _nameHMAC;
@synthesize algoHMAC = _algoHMAC;
@synthesize keyMaterial = _keyMaterial;
@synthesize stagedResult = _stagedResult;
@synthesize oneShotResult = _oneShotResult;
@synthesize dataHMAC = _dataHMAC;
@synthesize testObject = _testObject;
@synthesize digestBuffer = _digestBuffer;
@synthesize testPassed = _testPassed;


/* --------------------------------------------------------------------------
	method: 		setupHMACTests
	returns: 		NSArray *												
	decription: 	This method allows for creating digest specific tests for
					all of the digest supported by the CommonCrypto library.
					It creates an instance of the CCDigestTestObject for
					each digest to be tested and places that object into
					an NSArray.
   -------------------------------------------------------------------------- */
+ (NSArray *)setupHMACTests:(id<TestToolProtocol>)testObject;
{
	
	NSMutableArray* result = [NSMutableArray array]; // autoreleased
	
	// ======================= SHA1 HMAC ==========================
	
	CCHMACTestObject* sha1HMACTest = [[[CCHMACTestObject alloc] initWithHMACName:@"Sha1HMAC" 
 		withCCHmacAlgorithm:kCCHmacAlgSHA1 withDigestSize:CC_SHA1_DIGEST_LENGTH 
		withTestObject:testObject] autorelease];

	[result addObject:sha1HMACTest];
	
	// ======================= MD5 HMAC ==========================
	
	CCHMACTestObject* md5HMACTest = [[[CCHMACTestObject alloc] initWithHMACName:@"md5HMAC" 
 		withCCHmacAlgorithm:kCCHmacAlgMD5 withDigestSize:CC_MD5_DIGEST_LENGTH 
		withTestObject:testObject] autorelease];

	[result addObject:md5HMACTest];
	
	// ====================== SHA256 HMAC =========================
	
	CCHMACTestObject* sha256HMACTest = [[[CCHMACTestObject alloc] initWithHMACName:@"Sha256HMAC" 
 		withCCHmacAlgorithm:kCCHmacAlgSHA256 withDigestSize:CC_SHA256_DIGEST_LENGTH
		withTestObject:testObject] autorelease];

	[result addObject:sha256HMACTest];
	
	// ====================== SHA384 HMAC =========================
	
	CCHMACTestObject* sha384HMACTest = [[[CCHMACTestObject alloc] initWithHMACName:@"Sha384HMAC" 
 		withCCHmacAlgorithm:kCCHmacAlgSHA384 withDigestSize:CC_SHA384_DIGEST_LENGTH
		withTestObject:testObject] autorelease];

	[result addObject:sha384HMACTest];
	
	// ====================== SHA512 HMAC =========================
	
	CCHMACTestObject* sha512HMACTest = [[[CCHMACTestObject alloc] initWithHMACName:@"Sha512HMAC" 
 		withCCHmacAlgorithm:kCCHmacAlgSHA512 withDigestSize:CC_SHA512_DIGEST_LENGTH
		withTestObject:testObject] autorelease];

	[result addObject:sha512HMACTest];
	
	// ====================== SHA224 HMAC =========================
	
	CCHMACTestObject* sha224HMACTest = [[[CCHMACTestObject alloc] initWithHMACName:@"Sha224HMAC" 
 		withCCHmacAlgorithm:kCCHmacAlgSHA224 withDigestSize:CC_SHA224_DIGEST_LENGTH
		withTestObject:testObject] autorelease];

	[result addObject:sha224HMACTest];

	
	return result;
}


- (id)initWithHMACName:(NSString *)name 
 	withCCHmacAlgorithm:(CCHmacAlgorithm)algo
	withDigestSize:(unsigned int)digestSize
	withTestObject:(id<TestToolProtocol>)testObject
	
{	
	if ((self = [super init]))
	{
		_testPassed = YES;

		_nameHMAC = [name copy];
		_algoHMAC = algo;
		
		CCRandomNumberService* randService = [CCRandomNumberService defaultRandomNumberService];
		
		unsigned int bufferSize = [randService generateRandomNumberInRange:kMIN_KEY_LENGTH toMax:kMAX_KEY_LENGTH];
		_keyMaterial = [[randService generateRandomDataOfSize:bufferSize] copy];
		
		_stagedResult = nil;
		_oneShotResult = nil;
		
		bufferSize = [randService generateRandomNumberInRange:kMIN_DATA_LENGTH toMax:kMAC_DATA_LENGTH];
		_dataHMAC = [[randService generateRandomDataOfSize:bufferSize] copy];
		
		_digestBufferSize = digestSize;
		_digestBuffer = malloc(_digestBufferSize);
		
		_testObject = [testObject retain];
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
	[_keyMaterial release];
	[_stagedResult release];
	[_oneShotResult release];
	[_dataHMAC release];
	[_testObject release];
	if (NULL != _digestBuffer)
	{
		free(_digestBuffer);
		_digestBuffer = NULL;
	}
	[super dealloc];
}

- (CCHmacContext *)context
{
	return &_context;
}

- (void)clearContext
{
	memset(&_context, 0, sizeof(_context));
	memset(_digestBuffer, 0, _digestBufferSize);
}

/* --------------------------------------------------------------------------
	method: 		doStaged
	returns: 		void												
	decription: 	Do the staged digest creation for this test placing the
					result into the _stagedResult member
   -------------------------------------------------------------------------- */
- (void)doStaged
{
	[self clearContext];
	
	CCHmacInit([self context], self.algoHMAC, [self.keyMaterial bytes], 
		[self.keyMaterial length]);
		
	unsigned int dataLength = [self.dataHMAC length];
	unsigned int thisMove;
	
	const unsigned char* raw_bytes = (const unsigned char*)[self.dataHMAC bytes];
	
	CCRandomNumberService* randNumService = [CCRandomNumberService defaultRandomNumberService];
		
	while (dataLength)
	{
		thisMove = [randNumService generateRandomNumberInRange:1 toMax:dataLength];
		
		CCHmacUpdate([self context], raw_bytes, thisMove);
		
		raw_bytes += thisMove;
		
		dataLength -= thisMove;
	}
	
	CCHmacFinal([self context], _digestBuffer);
	[_stagedResult release];
	_stagedResult = [[NSData alloc] initWithBytes:_digestBuffer length:_digestBufferSize];
}


/* --------------------------------------------------------------------------
	method: 		doOneShot
	returns: 		void												
	decription: 	Do the 'one shot' digest creation for this test placing the
					result into the _oneShotResult member
   -------------------------------------------------------------------------- */
- (void)doOneShot
{
	[self clearContext];
	CCHmac(self.algoHMAC, [self.keyMaterial bytes], [self.keyMaterial length], 
		[self.dataHMAC bytes], [self.dataHMAC length], _digestBuffer);

	[_oneShotResult release];
	_oneShotResult = [[NSData alloc] initWithBytes:_digestBuffer length:_digestBufferSize];
	
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
		NSString stringWithFormat:@"Staged Result is not equal to the one shot result for digest type %@", self.nameHMAC]];
	
	if (nil == _testObject)
	{
		printf("HMACTest: %s\n", (self.testPassed) ? "Passed" : "Failed");
	}
	
}

@end

