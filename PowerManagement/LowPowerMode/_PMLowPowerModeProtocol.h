//
//  _PMLowPowerModeProtocol.h
//
//  Created by Andrei Dorofeev on 1/14/15.
//  Copyright © 2015,2020 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <LowPowerMode/_PMPowerModeState.h>

typedef void (^PMSetPowerModeCompletionHandler)(BOOL success, NSError *error);

@protocol _PMLowPowerModeProtocol

@optional
- (void)setPowerMode:(PMPowerMode)mode
          fromSource:(NSString *)source
      withCompletion:(PMSetPowerModeCompletionHandler)handler;

@required
- (void)setPowerMode:(PMPowerMode)mode
          fromSource:(NSString *)source
          withParams:(NSDictionary *)params // Session params applicable only while LPM is ON
      withCompletion:(PMSetPowerModeCompletionHandler)handler;

@end
