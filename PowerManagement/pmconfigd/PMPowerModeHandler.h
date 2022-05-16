//
//  PMPowerModeHandler.h
//  PowerManagement
//
//  Created by Saurabh Shah on 4/14/21.
//

#ifndef PMPowerModeHandler_h
#define PMPowerModeHandler_h

#import <Foundation/Foundation.h>

@interface PMPowerModeHandler : NSObject
+ (instancetype)sharedInstance;
- (void)handleEventWakeAfterHibernate;
@end

#endif /* PMPowerModeHandler_h */
