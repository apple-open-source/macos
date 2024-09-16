//
//  _PMPowerModeSession.m
//
//  Created by Prateek Malhotra on 6/24/24.
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#import "_PMPowerModeSession.h"

#if TARGET_OS_OSX

@implementation _PMPowerModeSession

- (instancetype)init
{
    self = [super init];
    if (self) {
        _mode = PMNormalPowerMode;
        _state = PMPowerModeStateOn;
        _expirationEventType = PMPowerModeExpirationEventTypeNone;
        _startedAt = [NSDate distantPast];
        _expiresAt = nil;
    }
    return self;
}

@end

#endif
