//
//  EncryptionTest.h
//  CommonCrypto
//
//  Created by Jim Murphy on 1/13/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "TestToolProtocol.h"
#include "CommonCryptor.h"



@interface CCEncryptionTest : NSObject <TestToolProtocol>
{
	CCAlgorithm						_encrAlg;
	uint32							_blockSize;
	uint32							_minKeySizeInBytes;
	uint32							_maxKeySizeInBytes;
	size_t							_ctxSize;
	NSString*						_algName;
	id<TestToolProtocol>			_testObject; // The owning test object NOT retained	
	BOOL							_testPassed;
}

@property (readonly) CCAlgorithm encrAlg;
@property (readonly) uint32 blockSize;
@property (readonly) uint32 minKeySizeInBytes;
@property (readonly) uint32 maxKeySizeInBytes;
@property (readonly) size_t ctxSize;
@property (readonly) NSString* algName;
@property (readonly) id<TestToolProtocol> testObject;
@property (readonly) BOOL testPassed;


+ (NSArray *)setupEncryptionTests:(id<TestToolProtocol>)testObject;

- (id)initWithEncryptionName:(NSString *)name 
		withEncryptionAlgo:(CCAlgorithm)encrAlg
		withBlockSize:(uint32)blockSize 
		withMinKeySizeInBytes:(uint32)minKeySizeInBytes
		withMaxKeySizeInBytes:(uint32)maxKeySizeInBytes
		withContextSize:(size_t)ctxSize
		withUnitTest:(id<TestToolProtocol>)testObject;

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;

- (void)runTest;

@end
