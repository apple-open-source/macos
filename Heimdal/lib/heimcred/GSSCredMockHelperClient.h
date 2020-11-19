//
//  GSSCredMockHelperClient.h
//  GSSCredTests
//
//  Created by Matt Chanda on 6/16/20.
//

#import <Foundation/Foundation.h>
#import "GSSCredHelperClient.h"
#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface GSSCredMockHelperClient : NSObject<GSSCredHelperClient>

typedef krb5_error_code (^clientBlock)(HeimCredRef, time_t *);

@property (nonatomic, class, copy, nullable) clientBlock expireBlock;
@property (nonatomic, class, copy, nullable) clientBlock renewBlock;

@property (nonatomic, nullable)  NSMutableDictionary<NSString *, XCTestExpectation *> *expireExpectations;
@property (nonatomic, nullable)  NSMutableDictionary<NSString *, XCTestExpectation *> *renewExpectations;
@property (nonatomic, nullable)  XCTestExpectation *finalExpectation;

+ (krb5_error_code)acquireForCred:(nonnull HeimCredRef)cred expireTime:(nonnull time_t *)expire;
+ (krb5_error_code)refreshForCred:(nonnull HeimCredRef)cred expireTime:(nonnull time_t *)expire;

+ (void)setExpireBlock:(clientBlock) block;
+ (void)setRenewBlock:(clientBlock) block;

@end

NS_ASSUME_NONNULL_END
