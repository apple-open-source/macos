//
//  PMSmartPowerNapPredictor.h
//  PMSmartPowerNapPredictor
//
//  Created by Archana on 10/19/21.
//



#import <Foundation/Foundation.h>

@interface PMSmartPowerNapPredictor : NSObject 

+ (instancetype)sharedInstance;

- (void)updateUserActivityLevel:(BOOL)useractive;
- (void)evaluateSmartPowerNap:(BOOL)useractive;
- (void)queryModelAndEngage;
- (void)enterSmartPowerNap;
- (void)exitSmartPowerNapWithReason:(NSString *)reason;
- (void)logEndOfSessionWithReason:(NSString *)reason;

@end
