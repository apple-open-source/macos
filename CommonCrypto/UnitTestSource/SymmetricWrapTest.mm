//
//  SymmetricWrapTest.mm
//  CommonCrypto
//
//  Created by Richard Murphy on 2/3/10.
//  Copyright 2010 McKenzie-Murphy. All rights reserved.
//

#import "SymmetricWrapTest.h"
#include <stdio.h>


@implementation CCSymmetricalWrapTest

@synthesize testObject = _testObject;
@synthesize testPassed = _testPassed;

+ (NSArray *)setupSymmWrapTests:(id<TestToolProtocol>)testObject
{
	
	NSMutableArray* result = [NSMutableArray array]; // autoreleased
	
	CCSymmetricalWrapTest* wrapTest = [[[CCSymmetricalWrapTest alloc] initWithTestObject:testObject] autorelease];
	[result addObject:wrapTest];
	return result;
}

- (id)initWithTestObject:(id<TestToolProtocol>)testObject
{
	
	if ((self = [super init]))
	{
		_testPassed = YES;
		_testObject = testObject;
		
	}
	return self;
}

- (void)dealloc
{
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
	

- (void)runTest
{	
	
	uint8_t kek[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
	};
	
	uint8_t key[] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	};
	
	uint8_t wrapped_key[] = {
		0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47, 
		0xAE, 0xF3, 0x4B, 0xD8, 0xFB, 0x5A, 0x7B, 0x82, 
		0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5
	};
	
	uint8_t wrapped[(128+64)/8];
	size_t wrapped_size = sizeof(wrapped);
	uint8_t unwrapped[128/8];
	size_t unwrapped_size = sizeof(unwrapped);
	
	BOOL self_test = NO; // guilty until proven
	
	//rfc3394_wrap(kek, sizeof(kek), rfc3394_iv, key, sizeof(key), wrapped, &wrapped_size);
	const uint8_t *iv =  CCrfc3394_iv;
	const size_t ivLen = CCrfc3394_ivLen;
	
	CCSymmetricKeyWrap(kCCWRAPAES, iv , ivLen, kek, sizeof(kek), key, sizeof(key), wrapped, &wrapped_size);
					   
	self_test = (0 == memcmp(wrapped, wrapped_key, wrapped_size));
	[self doAssertTest:self_test errorString:@"Wrapped key does not match"];
								   
	//rfc3394_unwrap(kek, sizeof(kek), rfc3394_iv, wrapped, wrapped_size, unwrapped, &unwrapped_size);
	CCSymmetricKeyUnwrap(kCCWRAPAES, iv, ivLen, kek, sizeof(kek), wrapped, wrapped_size, unwrapped, &unwrapped_size);
	self_test = (0 == memcmp(unwrapped, key, sizeof(key)));
	[self doAssertTest:self_test errorString:@"Unwrapped key does not match"];
	
	if (nil == _testObject)
	{
		printf("SymmetricalWrapTestest: %s\n", (self.testPassed) ? "Passed" : "Failed");
	}
																  
	/*
	 #if !KERNEL || AES256_KEK
	 {
	 uint8_t kek[] = {
	 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
	 };
	 uint8_t key[] = {
	 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
	 };
	 uint8_t wrapped_key[] = {
	 0x28, 0xC9, 0xF4, 0x04, 0xC4, 0xB8, 0x10, 0xF4,
	 0xCB, 0xCC, 0xB3, 0x5C, 0xFB, 0x87, 0xF8, 0x26,
	 0x3F, 0x57, 0x86, 0xE2, 0xD8, 0x0E, 0xD3, 0x26,
	 0xCB, 0xC7, 0xF0, 0xE7, 0x1A, 0x99, 0xF4, 0x3B,
	 0xFB, 0x98, 0x8B, 0x9B, 0x7A, 0x02, 0xDD, 0x21
	 };
	 uint8_t wrapped[(256+64)/8];
	 size_t wrapped_size = sizeof(wrapped);
	 uint8_t unwrapped[256/8];
	 size_t unwrapped_size = sizeof(unwrapped);
	 bool self_test;
	 
	 rfc3394_wrap(kek, sizeof(kek), rfc3394_iv, key, sizeof(key), wrapped, &wrapped_size);
	 self_test = (0 == memcmp(wrapped, wrapped_key, wrapped_size));
	 require(self_test, out);
	 printf("\nSELF-TEST %s\n\n", self_test ? "OK" : "FAIL");
	 rfc3394_unwrap(kek, sizeof(kek), rfc3394_iv, wrapped, wrapped_size, unwrapped, &unwrapped_size);
	 self_test = (0 == memcmp(unwrapped, key, sizeof(key)));
	 require(self_test, out);
	 printf("\nSELF-TEST %s\n\n", self_test ? "OK" : "FAIL");
	 }
	 #endif
	 */
	
								   
}

@end

