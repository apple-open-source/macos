//
//  TestHarness.h
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-06-07.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol TestHarnessProtocol <NSObject>

- (void)THPTestStart:(NSString *)name;
- (void)THPTestOutput:(NSString *)name;
- (void)THPTestComplete:(NSString *)name status:(bool)status duration:(float)durataion;
- (void)THPSuiteComplete:(bool)status;


@end

@interface TestHarness : NSObject

@property (weak) id<TestHarnessProtocol> delegate;

+ (void)TestHarnessOutput:(NSString *)output;

- (bool)runTests;


@end
