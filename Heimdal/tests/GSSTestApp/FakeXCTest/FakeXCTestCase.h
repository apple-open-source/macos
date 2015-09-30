//
//  FakeXCTest
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface XCTestCase : NSObject

+ (NSArray *)defaultPerformanceMetrics;
- (void)measureBlock:(void (^)(void))block;
- (void)measureMetrics:(NSArray *)metrics automaticallyStartMeasuring:(BOOL)automaticallyStartMeasuring forBlock:(void (^)(void))block;

- (void)setUp;
- (void)tearDown;

@end
