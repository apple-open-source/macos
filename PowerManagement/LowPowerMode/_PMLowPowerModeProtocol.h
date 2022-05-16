//
//  _PMLowPowerModeProtocol.h
//
//  Created by Andrei Dorofeev on 1/14/15.
//  Copyright © 2015,2020 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, PMPowerMode) {
    PMNormalPowerMode = 0,
    PMLowPowerMode = 1
};

typedef void (^PMSetPowerModeCompletionHandler)(BOOL success, NSError *error);

@protocol _PMLowPowerModeProtocol

- (void)setPowerMode:(PMPowerMode)mode
          fromSource:(NSString *)source
      withCompletion:(PMSetPowerModeCompletionHandler)handler;

@end
