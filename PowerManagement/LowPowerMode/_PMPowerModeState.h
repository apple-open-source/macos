//
//  _PMPowerModeState.h
//
//  Created by Prateek Malhotra on 6/25/24.
//  Copyright © 2024 Apple Inc. All rights reserved.
//

typedef NS_ENUM(NSInteger, PMPowerMode) {
    PMNormalPowerMode = 0,
    PMLowPowerMode = 1,
#if TARGET_OS_OSX
    PMHighPowerMode = 2,
#endif
};

typedef NS_ENUM(NSInteger, PMPowerModeState) {
    PMPowerModeStateOff = 0,
    PMPowerModeStateOn = 255
};
