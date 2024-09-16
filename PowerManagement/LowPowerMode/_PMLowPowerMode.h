//
//  _PMLowPowerMode.h
//
//  Created by Andrei Dorofeev on 1/14/15.
//  Copyright © 2015,2020 Apple Inc. All rights reserved.
//

#import <TargetConditionals.h>
#import <AppleFeatures/AppleFeatures.h>
#import <LowPowerMode/_PMLowPowerModeProtocol.h>
#import <LowPowerMode/_PMPowerModeState.h>

extern NSString *const kPMLowPowerModeServiceName;

extern NSString *const kPMLPMSourceSpringBoardAlert;
extern NSString *const kPMLPMSourceReenableBulletin;
extern NSString *const kPMLPMSourceControlCenter;
extern NSString *const kPMLPMSourceSettings;
extern NSString *const kPMLPMSourceSiri;
extern NSString *const kPMLPMSourceLostMode;
extern NSString *const kPMLPMSourceSystemDisable;
extern NSString *const kPMLPMSourceWorkouts;


@interface _PMLowPowerMode : NSObject <_PMLowPowerModeProtocol>

+ (instancetype)sharedInstance;

// Synchronous flavor. The one from Protocol is async.
- (BOOL)setPowerMode:(PMPowerMode)mode fromSource:(NSString *)source;
- (BOOL)setPowerMode:(PMPowerMode)mode fromSource:(NSString *)source withParams:(NSDictionary *)params;
- (PMPowerMode)getPowerMode;

@end
