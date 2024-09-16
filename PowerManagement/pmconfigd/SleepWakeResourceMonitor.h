//
//  SleepWakeResourceMonitor.h
//  PowerManagement
//
//  Created by Archana Venkatesh on 11/1/23.
//

#ifndef SleepWakeResourceMonitor_h
#define SleepWakeResourceMonitor_h
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <PowerExperience/ResourceHint.h>

void initSleepWakeResourceMonitoring(void);
void startMonitoring(void);
void handleWillSleep(void);
void handleWillNotSleep(void);
void handleWillWake(void);
void handleDidWake(void);
static dispatch_queue_t sleep_wake_resource_queue;
static io_connect_t sleep_wake_port;

#endif /* SleepWakeResourceMonitor_h */
