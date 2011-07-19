//
//  CommonCryptoUnitTests.h
//  CommonCrypto
//
//  Created by Jim Murphy on 1/11/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <SenTestingKit/SenTestingKit.h>
#import "TestToolProtocol.h"


@interface CommonCryptoUnitTests : SenTestCase  <TestToolProtocol>
{
	// No member variables
}

// This allows an object that is NOT subclassed from SenTestCase to issue 
// an assert
- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr;

@end
