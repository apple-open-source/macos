//
//  _PMPowerModes.m
//
//  Created by Prateek Malhotra on 6/24/24.
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#import "_PMPowerModesProtocol.h"
#import "_PMPowerModes.h"

#if TARGET_OS_OSX

@implementation _PMPowerModes

+ (instancetype)sharedInstance
{
    static dispatch_once_t onceToken;
    static _PMPowerModes *saver = nil;
    dispatch_once(&onceToken, ^{
        saver = [[_PMPowerModes alloc] init];
    });
    return saver;
}

- (IOReturn)registerForUpdatesOfPowerMode:(PMPowerMode)mode 
                           withIdentifier:(NSString *)clientIdentifier
                             withCallback:(PMPowerModeUpdateHandler)handler
{
    return kIOReturnUnsupported;
}

- (IOReturn)registerForUpdatesWithIdentifier:(NSString *)clientIdentifier 
                                withCallback:(PMPowerModeUpdateHandler)handler
{
    return kIOReturnUnsupported;
}

- (IOReturn)setStateTo:(PMPowerModeState)newState
          forPowerMode:(PMPowerMode)mode
            fromSource:(NSString *)source
   withExpirationEvent:(PMPowerModeExpirationEventType)expirationEvent
             andParams:(PMPowerModeExpirationEventParams)expirationParams
          withCallback:(PMPowerModeUpdateHandler)handler
{
    return kIOReturnUnsupported;
}

- (BOOL)supportsPowerMode:(PMPowerMode)mode
{
    if (mode == PMNormalPowerMode) {
        return YES;
    }
    return NO;
}

- (BOOL)supportsPowerModeSelectionUI
{
    return NO;
}

- (PMPowerMode)currentPowerMode {
    return PMNormalPowerMode;
}

- (_PMPowerModeSession *)currentPowerModeSession {
    return [[_PMPowerModeSession alloc] init];
}

@end

#endif
