//
//  KNPersistantState.h
//  Security
//
//  Created by J Osborne on 7/28/13.
//
//

#import <Foundation/Foundation.h>
#include "SecureObjectSync/SOSCloudCircle.h"
#include "SecureObjectSync/SOSPeerInfo.h"

@interface KNPersistantState : NSObject
+(instancetype)loadFromStorage;
-(void)writeToStorage;

@property SOSCCStatus lastCircleStatus;
@property NSNumber *debugLeftReason;
@property NSNumber *pendingApplicationReminderInterval;
@property NSDate *pendingApplicationReminder;
@property NSDate *applcationDate;
@property NSDate *lastWritten;
@end
