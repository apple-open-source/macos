//
//  SymmetricWrapTest.h
//  CommonCrypto
//
//  Created by Richard Murphy on 2/3/10.
//  Copyright 2010 McKenzie-Murphy. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "CommonSymmetricKeywrap.h"
#import "TestToolProtocol.h"


@interface CCSymmetricalWrapTest : NSObject <TestToolProtocol>
{
	id<TestToolProtocol>	_testObject;			// The owning test object NOT retained	
	BOOL					_testPassed;
	
}

@property (readonly) id<TestToolProtocol> testObject;
@property (readonly) BOOL testPassed;

+ (NSArray *)setupSymmWrapTests:(id<TestToolProtocol>)testObject;

- (id)initWithTestObject:(id<TestToolProtocol>)testObject;

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;

- (void)runTest;

@end

