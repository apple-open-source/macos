//
//  PMLowPowerModeService.h
//
//  Created by Andrei Dorofeev on 1/13/15.
//  Copyright © 2015,2020 Apple Inc. All rights reserved.
//

#import "_PMLowPowerModeProtocol.h"
#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>

@interface PMLowPowerModeService : NSXPCListener <NSXPCListenerDelegate, _PMLowPowerModeProtocol>

+ (instancetype)sharedInstance;

- (BOOL)inLowPowerMode;

@end
