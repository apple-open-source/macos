//
//  _PMLowPowerMode.m
//
//  Created by Andrei Dorofeev on 1/14/15.
//  Copyright © 2015,2020 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Foundation/NSPrivate.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <dispatch/dispatch.h>

#import "_PMLowPowerMode.h"
#import "_PMLowPowerModeProtocol.h"

#define _PM_XPC_TIMEOUT (15 * NSEC_PER_SEC)

// Service Name
NSString *const kPMLowPowerModeServiceName = @"com.apple.powerd.lowpowermode";

NSString *const kPMLPMSourceSpringBoardAlert = @"SpringBoard";
NSString *const kPMLPMSourceReenableBulletin = @"Reenable";
NSString *const kPMLPMSourceControlCenter = @"ControlCenter";
NSString *const kPMLPMSourceSettings = @"Settings";
NSString *const kPMLPMSourceSiri = @"Siri";
NSString *const kPMLPMSourceLostMode = @"LostMode";
NSString *const kPMLPMSourceSystemDisable = @"SystemDisable";

@interface _PMLowPowerMode () {
    NSXPCConnection *_connection;
}

@end

@implementation _PMLowPowerMode

+ (instancetype)sharedInstance
{
    static dispatch_once_t onceToken;
    static _PMLowPowerMode *saver = nil;
    dispatch_once(&onceToken, ^{
        saver = [[_PMLowPowerMode alloc] init];
    });
    return saver;
}

- (instancetype)init
{
    self = [super init];

    if (self) {
        _connection = [[NSXPCConnection alloc] initWithMachServiceName:kPMLowPowerModeServiceName
                                                               options:NSXPCConnectionPrivileged];
        _connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(_PMLowPowerModeProtocol)];
        [_connection resume];
    }

    return self;
}

- (void)dealloc
{
    [_connection invalidate];
}

- (void)setPowerMode:(PMPowerMode)mode
          fromSource:(NSString *)source
      withCompletion:(PMSetPowerModeCompletionHandler)handler
{
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
        handler(NO, error);
    }] setPowerMode:mode
            fromSource:source
        withCompletion:handler];
}

- (BOOL)setPowerMode:(PMPowerMode)mode fromSource:(NSString *)source
{
    __block BOOL ret = YES;
    [[_connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull __unused error) {
        ret = NO;
        NSLog(@"synchronous connection failed: %@\n", error);
    }] setPowerMode:mode
            fromSource:source
        withCompletion:^(BOOL success, NSError *error) {
            if (!success || error) {
                ret = NO;
                NSLog(@"setPowerMode failed: %@\n", error);
            }
            return;
        }];
    return ret;
}

- (PMPowerMode)getPowerMode
{
    BOOL enabled = [[NSProcessInfo processInfo] isLowPowerModeEnabled];
    return (enabled ? PMLowPowerMode : PMNormalPowerMode);
}

@end
