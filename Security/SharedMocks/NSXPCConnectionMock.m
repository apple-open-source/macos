//
//  NSXPCConnectionMock.m
//  Security_ios
//
//  Created by Love Hörnquist Åstrand on 12/16/18.
//

#import "NSXPCConnectionMock.h"

@interface NSXPCConnectionMock ()
@property id reality;
@end

@implementation NSXPCConnectionMock
- (instancetype) initWithRealObject:(id)reality
{
    self = [super init];
    if (self) {
        _reality = reality;
    }
    return self;
}
- (id)remoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler
{
    (void)failureHandler;
    return _reality;
}
- (id)synchronousRemoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler
{
    (void)failureHandler;
    return _reality;
}

@end
