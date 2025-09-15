//
//  PMPowerMitigations.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#ifndef PMPowerMitigations_h
#define PMPowerMitigations_h

#import "PMPowerMitigationsObservable.h"
#import "PMPowerMitigationsServiceCallbackProtocol.h"


extern NSString *const kPowerMitigationsManagerService;
extern NSString *const kPowerMitigationsManagerServiceStartupNotify;

@interface PMPowerMitigations : NSObject <PMPowerMitigationsObservable, PMPowerMitigationsServiceCallbackProtocol>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)sharedInstance;

@end


#endif /* PMPowerMitigations_h */
