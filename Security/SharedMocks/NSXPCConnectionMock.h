//
//  NSXPCConnectionMock.h
//  Security_ios
//
//  Created by Love Hörnquist Åstrand on 12/16/18.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSXPCConnectionMock : NSObject
- (instancetype) initWithRealObject:(id)reality;
- (id)remoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler;
- (id)synchronousRemoteObjectProxyWithErrorHandler:(void(^)(NSError * _Nonnull error))failureHandler;
@end

NS_ASSUME_NONNULL_END
