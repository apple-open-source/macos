/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "SFBehavior.h"
#import <dispatch/dispatch.h>

#if __OBJC2__

@interface SFBehavior ()
@property NSString *family;
@property NSXPCConnection *connection;
- (instancetype)initBehaviorFamily:(NSString *)family connection:(NSXPCConnection *)connection;
@end

@protocol SFBehaviorProtocol <NSObject>
- (void)ramping:(NSString *)feature family:(NSString*)family complete:(void (^)(SFBehaviorRamping))complete;
- (void)feature:(NSString *)feature family:(NSString*)family defaultValue:(bool)defaultValue complete:(void (^)(bool))complete;

- (void)configNumber:(NSString *)configuration family:(NSString*)family complete:(void (^)(NSNumber *))complete;
- (void)configString:(NSString *)configuration family:(NSString*)family complete:(void (^)(NSString *))complete;
@end


@implementation SFBehavior

+ (SFBehavior *)behaviorFamily:(NSString *)family
{
    static dispatch_once_t onceToken = 0;
    static NSMutableDictionary<NSString *, SFBehavior *> *behaviors;
    static NSXPCConnection *connection = NULL;
    dispatch_once(&onceToken, ^{
        behaviors = [NSMutableDictionary dictionary];
        connection = [[NSXPCConnection alloc] initWithMachServiceName:@"com.apple.security.behavior" options:NSXPCConnectionPrivileged];

        connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(SFBehaviorProtocol)];
        [connection resume];
    });

    SFBehavior *behavior = nil;
    @synchronized (behaviors) {
        behavior = behaviors[family];
        if (behavior == NULL) {
            behavior = [[SFBehavior alloc] initBehaviorFamily:family connection:connection];
            behaviors[family] = behavior;
        }
    }

    return behavior;
}

- (instancetype)initBehaviorFamily:(NSString *)family connection:(NSXPCConnection *)connection
{
    self = [super init];
    if (self) {
        _family = family;
        _connection = connection;
    }
    return self;
}

- (SFBehaviorRamping)ramping:(NSString *)feature force:(bool)force
{
    __block SFBehaviorRamping _ramping = SFBehaviorRampingDisabled;
    [[_connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
    }] ramping:feature family:_family complete:^(SFBehaviorRamping ramping) {
        _ramping = ramping;
    }];
    return _ramping;
}

- (bool)feature:(NSString *)feature defaultValue:(bool)defaultValue
{
    __block bool enabled = defaultValue;

    [[_connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
    }] feature:feature family:_family defaultValue:defaultValue complete:^(bool returnFeature) {
        enabled = returnFeature;
    }];
    return enabled;

}

- (bool)featureEnabled:(NSString *)feature
{
    return [self feature:feature defaultValue:true];
}

- (bool)featureDisabled:(NSString *)feature
{
    return ![self feature:feature defaultValue:false];
}

- (NSNumber *)configurationNumber:(NSString *)configuration defaultValue:(NSNumber *)defaultValue
{
    __block NSNumber *_number = defaultValue;

    [[_connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
    }] configNumber:configuration family:_family complete:^(NSNumber *number) {
        if (number)
            _number = number;
    }];
    return _number;
}

- (NSString *)configurationString:(NSString *)configuration defaultValue:(NSString *)defaultValue
{
    __block NSString *_string = defaultValue;

    [[_connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
    }] configString:configuration family:_family complete:^(NSString *string) {
        if (string)
            _string = string;
    }];
    return _string;
}

@end

#endif /* __OBJC2__ */

