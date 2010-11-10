//
//  unit_tests.h
//  openssl
//
//  Created by J. Osborne on 1/25/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#include <dispatch/dispatch.h>

@interface unit_tests : SenTestCase {
}

// these run ONCE, not one per test unit
+(void)tearDown;

-(void)testTrustedKeyExists;
-(void)testTrustedKeyDoesntOveride;

@end
