//
//  _PMPowerModes.h
//
//  Created by Prateek Malhotra on 6/24/24.
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#import "_PMPowerModesProtocol.h"

#if TARGET_OS_OSX

@interface _PMPowerModes : NSObject <_PMPowerModesProtocol>

+ (instancetype)sharedInstance;

/**
 * @abstract            Fetch information about the currently active power mode session
 * @return              The session info object
 * */
- (_PMPowerModeSession *)currentPowerModeSession;

/**
 * @abstract            The currently active power mode
 * */
- (PMPowerMode)currentPowerMode;

/**
 Whether power modes can be controlled through the UI.
 */
/**
 * @abstract            Whether power modes can be controlled through the UI.
 * @discussion          On non-Battery Macs, modes may not be available for selection through ControlCenter but only be available through Settings.
 * @return              YES, if mode selection is supported through the UI (Control Center and/or MenuExtra)
 * */

- (BOOL)supportsPowerModeSelectionUI;

@end

#endif
