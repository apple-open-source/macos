//
//  PersistantState.h
//  Security
//
//  Created by J Osborne on 7/11/13.
//
//

#import <Foundation/Foundation.h>
#include "SecureObjectSync/SOSCloudCircle.h"
#include "SecureObjectSync/SOSPeerInfo.h"

@interface PersistantState : NSObject
+(instancetype)loadFromStorage;
-(unsigned int)defaultPendingApplicationReminderAlertInterval;
-(void)writeToStorage;

@property SOSCCStatus lastCircleStatus;
@property NSDate *lastWritten;
@property NSDate *pendingApplicationReminder;
@property unsigned int pendingApplicationReminderAlertInterval;
@property NSDate *applcationDate;
@property NSNumber *debugShowLeftReason;
@property BOOL absentCircleWithNoReason;
@end
