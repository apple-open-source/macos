//
//  AppleKeyboardStateManager.h
//  AppleKeyboardFilterPlugin
//
//  Created by Daniel Kim on 3/22/18.
//  Copyright © 2018 Apple. All rights reserved.
//

#pragma once

@interface AppleKeyboardStateManager : NSObject

+ (instancetype)sharedManager;

- (BOOL)isCapsLockEnabled:(NSNumber *)locationID;
- (void)setCapsLockEnabled:(BOOL)enable
                locationID:(NSNumber *)locationID;

@end
