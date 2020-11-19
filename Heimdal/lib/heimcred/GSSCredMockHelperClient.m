//
//  GSSCredMockHelperClient.m
//  GSSCredTests
//
//  Created by Matt Chanda on 6/16/20.
//

#import "GSSCredMockHelperClient.h"

@implementation GSSCredMockHelperClient

@synthesize expireExpectations = _expireExpectations;
@synthesize renewExpectations = _renewExpectations;
@synthesize finalExpectation = _finalExpectation;

static clientBlock _expireBlock;
static clientBlock _renewBlock;

- (instancetype)init
{
    self = [super init];
    if (self) {
	_expireExpectations = [@{} mutableCopy];
	_renewExpectations = [@{} mutableCopy];
    }
    return self;
}

+ (krb5_error_code)acquireForCred:(nonnull HeimCredRef)cred expireTime:(nonnull time_t *)expire
{
    return _expireBlock(cred, expire);
}

+ (krb5_error_code)refreshForCred:(nonnull HeimCredRef)cred expireTime:(nonnull time_t *)expire
{
    return _renewBlock(cred, expire);
}

+ (clientBlock)expireBlock
{
    return _expireBlock;
}

+ (clientBlock)renewBlock {
    return _renewBlock;
}

+ (void)setExpireBlock:(clientBlock) block
{
    _expireBlock = block;
}

+ (void)setRenewBlock:(clientBlock) block
{
    _renewBlock = block;
}


@end
