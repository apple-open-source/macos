/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#import <Foundation/NSArray.h>
#import <Foundation/Foundation.h>

#import <Security/SecBasePriv.h>
#import <Security/SecItemPriv.h>
#import <utilities/debugging.h>
#import <notify.h>

#include <Security/CKBridge/SOSCloudKeychainConstants.h>
#include <Security/SecureObjectSync/SOSARCDefines.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>

#import <IDS/IDS.h>
#import <os/activity.h>

#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#import "IDSPersistentState.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"
#import "KeychainSyncingOverIDSProxy+Throttle.h"
#import <utilities/SecADWrapper.h>


static NSString *kExportUnhandledMessages = @"UnhandledMessages";
static NSString *kMonitorState = @"MonitorState";

static NSString *kMonitorPenaltyBoxKey = @"Penalty";
static NSString *kMonitorMessageKey = @"Message";
static NSString *kMonitorConsecutiveWrites = @"ConsecutiveWrites";
static NSString *kMonitorLastWriteTimestamp = @"LastWriteTimestamp";
static NSString *kMonitorMessageQueue = @"MessageQueue";
static NSString *kMonitorPenaltyTimer = @"PenaltyTimer";
static NSString *kMonitorDidWriteDuringPenalty = @"DidWriteDuringPenalty";

static NSString *kMonitorTimeTable = @"TimeTable";
static NSString *kMonitorFirstMinute = @"AFirstMinute";
static NSString *kMonitorSecondMinute = @"BSecondMinute";
static NSString *kMonitorThirdMinute = @"CThirdMinute";
static NSString *kMonitorFourthMinute = @"DFourthMinute";
static NSString *kMonitorFifthMinute = @"EFifthMinute";
static NSString *kMonitorWroteInTimeSlice = @"TimeSlice";

static int max_penalty_timeout = 32;
static int seconds_per_minute = 60;
static int queue_depth = 1;

#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
CFStringRef const IDSPAggdIncreaseThrottlingKey = CFSTR("com.apple.security.idsproxy.increasethrottle");
CFStringRef const IDSPAggdDecreaseThrottlingKey = CFSTR("com.apple.security.idsproxy.decreasethrottle");
#endif

static const int64_t kRetryTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.

@implementation KeychainSyncingOverIDSProxy (Throttle)

-(dispatch_source_t)setNewTimer:(int)timeout key:(NSString*)key deviceName:(NSString*)deviceName peerID:(NSString*)peerID
{

    __block dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC * seconds_per_minute), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
    dispatch_source_set_event_handler(timer, ^{
        [self penaltyTimerFired:key deviceName:deviceName peerID:peerID];
    });
    dispatch_resume(timer);
    return timer;
}

-(void) increasePenalty:(NSNumber*)currentPenalty key:(NSString*)key keyEntry:(NSMutableDictionary**)keyEntry deviceName:(NSString*)deviceName peerID:(NSString*)peerID
{
#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
    SecADAddValueForScalarKey((IDSPAggdIncreaseThrottlingKey), 1);
#endif

    secnotice("backoff", "increasing penalty!");
    int newPenalty = 0;
    
    if ([currentPenalty intValue] <= 0)
        newPenalty = 1;
    else
        newPenalty = fmin([currentPenalty intValue]*2, max_penalty_timeout);
    
    secnotice("backoff", "key %@, waiting %d minutes long to send next messages", key, newPenalty);
    
    NSNumber* penalty_timeout = [[NSNumber alloc]initWithInt:newPenalty];
    dispatch_source_t existingTimer = [*keyEntry objectForKey:kMonitorPenaltyTimer];
    
    if(existingTimer != nil){
        [*keyEntry removeObjectForKey:kMonitorPenaltyTimer];
        dispatch_suspend(existingTimer);
        dispatch_source_set_timer(existingTimer,dispatch_time(DISPATCH_TIME_NOW, newPenalty * NSEC_PER_SEC * seconds_per_minute), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
        dispatch_resume(existingTimer);
        [*keyEntry setObject:existingTimer forKey:kMonitorPenaltyTimer];
    }
    else{
        dispatch_source_t timer = [self setNewTimer:newPenalty key:key deviceName:deviceName peerID:peerID];
        [*keyEntry setObject:timer forKey:kMonitorPenaltyTimer];
    }
    
    [*keyEntry setObject:penalty_timeout forKey:kMonitorPenaltyBoxKey];
    [[KeychainSyncingOverIDSProxy idsProxy].monitor setObject:*keyEntry forKey:key];
}

-(void) decreasePenalty:(NSNumber*)currentPenalty key:(NSString*)key keyEntry:(NSMutableDictionary**)keyEntry deviceName:(NSString*)deviceName peerID:(NSString*)peerID
{
#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
    SecADAddValueForScalarKey((IDSPAggdDecreaseThrottlingKey), 1);
#endif
    int newPenalty = 0;
    secnotice("backoff","decreasing penalty!");
    if([currentPenalty intValue] == 0 || [currentPenalty intValue] == 1)
        newPenalty = 0;
    else
        newPenalty = [currentPenalty intValue]/2;
    
    secnotice("backoff","key %@, waiting %d minutes long to send next messages", key, newPenalty);
    
    NSNumber* penalty_timeout = [[NSNumber alloc]initWithInt:newPenalty];
    
    dispatch_source_t existingTimer = [*keyEntry objectForKey:kMonitorPenaltyTimer];
    if(existingTimer != nil){
        [*keyEntry removeObjectForKey:kMonitorPenaltyTimer];
        dispatch_suspend(existingTimer);
        if(newPenalty != 0){
            dispatch_source_set_timer(existingTimer,dispatch_time(DISPATCH_TIME_NOW, newPenalty * NSEC_PER_SEC * seconds_per_minute), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
            dispatch_resume(existingTimer);
            [*keyEntry setObject:existingTimer forKey:kMonitorPenaltyTimer];
        }
        else{
            dispatch_resume(existingTimer);
            dispatch_source_cancel(existingTimer);
        }
    }
    else{
        if(newPenalty != 0){
            dispatch_source_t timer = [self setNewTimer:newPenalty key:key deviceName:deviceName peerID:peerID];
            [*keyEntry setObject:timer forKey:kMonitorPenaltyTimer];
        }
    }
    
    [*keyEntry setObject:penalty_timeout forKey:kMonitorPenaltyBoxKey];
    [[KeychainSyncingOverIDSProxy idsProxy].monitor setObject:*keyEntry forKey:key];
    
}

- (void)penaltyTimerFired:(NSString*)key deviceName:(NSString*)deviceName peerID:(NSString*)peerID
{
    secnotice("backoff", "key: %@, !!!!!!!!!!!!!!!!penalty timeout is up!!!!!!!!!!!!", key);
    NSMutableDictionary *keyEntry = [[KeychainSyncingOverIDSProxy idsProxy].monitor objectForKey:key];
    if(!keyEntry){
        [self initializeKeyEntry:key];
        keyEntry = [[KeychainSyncingOverIDSProxy idsProxy].monitor objectForKey:key];
    }
    NSMutableArray *queuedMessages = [[KeychainSyncingOverIDSProxy idsProxy].monitor objectForKey:kMonitorMessageQueue];
    secnotice("backoff","key: %@, queuedMessages: %@", key, queuedMessages);
    if(queuedMessages && [queuedMessages count] != 0){
        secnotice("backoff","key: %@, message queue not empty, writing to IDS!", key);
        [queuedMessages enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            NSError* error = nil;
            NSDictionary* message = (NSDictionary*) obj;
            [self sendFragmentedIDSMessages:message name:deviceName peer:peerID error:&error];
        }];
       
        [[KeychainSyncingOverIDSProxy idsProxy].monitor  setObject:[NSMutableArray array] forKey:kMonitorMessageQueue];
    }
    //decrease timeout since we successfully wrote messages out
    NSNumber *penalty_timeout = [keyEntry objectForKey:kMonitorPenaltyBoxKey];
    secnotice("backoff", "key: %@, current penalty timeout: %@", key, penalty_timeout);

    NSString* didWriteDuringTimeout = [keyEntry objectForKey:kMonitorDidWriteDuringPenalty];
    if( didWriteDuringTimeout && [didWriteDuringTimeout isEqualToString:@"YES"] )
    {
        //increase timeout since we wrote during out penalty timeout
        [self increasePenalty:penalty_timeout key:key keyEntry:&keyEntry deviceName:deviceName peerID:peerID];
    }
    else{
        //decrease timeout since we successfully wrote messages out
        [self decreasePenalty:penalty_timeout key:key keyEntry:&keyEntry deviceName:deviceName peerID:peerID];
    }
    
    //resetting the check
    [keyEntry setObject: @"NO" forKey:kMonitorDidWriteDuringPenalty];
    
    //recompute the timetable and number of consecutive writes to IDS
    NSMutableDictionary *timetableForKey = [keyEntry objectForKey:kMonitorTimeTable];
    if(timetableForKey == nil){
        timetableForKey = [self initializeTimeTable:key];
    }
    NSNumber *consecutiveWrites = [keyEntry objectForKey:kMonitorConsecutiveWrites];
    if(consecutiveWrites == nil){
        consecutiveWrites = [[NSNumber alloc] initWithInt:0];
    }
    [self recordTimestampForAppropriateInterval:&timetableForKey key:key consecutiveWrites:&consecutiveWrites];
    
    [keyEntry setObject:consecutiveWrites forKey:kMonitorConsecutiveWrites];
    [keyEntry setObject:timetableForKey forKey:kMonitorTimeTable];
    [[KeychainSyncingOverIDSProxy idsProxy].monitor setObject:keyEntry forKey:key];
    
}

-(NSMutableDictionary*)initializeTimeTable:(NSString*)key
{
    NSDate *currentTime = [NSDate date];
    NSMutableDictionary *firstMinute = [NSMutableDictionary dictionaryWithObjectsAndKeys:[currentTime dateByAddingTimeInterval: seconds_per_minute], kMonitorFirstMinute, @"YES", kMonitorWroteInTimeSlice, nil];
    NSMutableDictionary *secondMinute = [NSMutableDictionary dictionaryWithObjectsAndKeys:[currentTime dateByAddingTimeInterval: seconds_per_minute * 2],kMonitorSecondMinute, @"NO", kMonitorWroteInTimeSlice, nil];
    NSMutableDictionary *thirdMinute = [NSMutableDictionary dictionaryWithObjectsAndKeys:[currentTime dateByAddingTimeInterval: seconds_per_minute * 3], kMonitorThirdMinute, @"NO",kMonitorWroteInTimeSlice, nil];
    NSMutableDictionary *fourthMinute = [NSMutableDictionary dictionaryWithObjectsAndKeys:[currentTime dateByAddingTimeInterval: seconds_per_minute * 4],kMonitorFourthMinute, @"NO", kMonitorWroteInTimeSlice, nil];
    NSMutableDictionary *fifthMinute = [NSMutableDictionary dictionaryWithObjectsAndKeys:[currentTime dateByAddingTimeInterval: seconds_per_minute * 5], kMonitorFifthMinute, @"NO", kMonitorWroteInTimeSlice, nil];
    
    NSMutableDictionary *timeTable = [NSMutableDictionary dictionaryWithObjectsAndKeys: firstMinute, kMonitorFirstMinute,
                                      secondMinute, kMonitorSecondMinute,
                                      thirdMinute, kMonitorThirdMinute,
                                      fourthMinute, kMonitorFourthMinute,
                                      fifthMinute, kMonitorFifthMinute, nil];
    return timeTable;
}

- (void)initializeKeyEntry:(NSString*)key
{
    NSMutableDictionary *timeTable = [[KeychainSyncingOverIDSProxy idsProxy] initializeTimeTable:key];
    NSDate *currentTime = [NSDate date];
    
    NSMutableDictionary *keyEntry = [NSMutableDictionary dictionaryWithObjectsAndKeys: key, kMonitorMessageKey, @0, kMonitorConsecutiveWrites, currentTime, kMonitorLastWriteTimestamp, @0, kMonitorPenaltyBoxKey, timeTable, kMonitorTimeTable,[NSMutableArray array], kMonitorMessageQueue, nil];
    
    [[KeychainSyncingOverIDSProxy idsProxy].monitor setObject:keyEntry forKey:key];
    
}

- (void)recordTimestampForAppropriateInterval:(NSMutableDictionary**)timeTable key:(NSString*)key consecutiveWrites:(NSNumber**)consecutiveWrites
{
    NSDate *currentTime = [NSDate date];
    __block int cWrites = [*consecutiveWrites intValue];
    __block BOOL foundTimeSlot = NO;
    __block NSMutableDictionary *previousTable = nil;
    
    NSArray *sortedTimestampKeys = [[*timeTable allKeys] sortedArrayUsingSelector:@selector(compare:)];
    [sortedTimestampKeys enumerateObjectsUsingBlock:^(id arrayObject, NSUInteger idx, BOOL *stop)
     {
         if(foundTimeSlot == YES)
             return;
         
         NSString *sortedKey = (NSString*)arrayObject;
         
         //grab the dictionary containing write information
         //(date, boolean to check if a write occured in the timeslice,
         NSMutableDictionary *minutesTable = [*timeTable objectForKey: sortedKey];
         if(minutesTable == nil)
             minutesTable = [[KeychainSyncingOverIDSProxy idsProxy] initializeTimeTable:key];
         
         NSString *minuteKey = (NSString*)sortedKey;
         NSDate *timeStampForSlice = [minutesTable objectForKey:minuteKey];
         
         if(timeStampForSlice && [timeStampForSlice compare:currentTime] == NSOrderedDescending){
             foundTimeSlot = YES;
             NSString* written = [minutesTable objectForKey:kMonitorWroteInTimeSlice];
             //figure out if we have previously recorded a write in this time slice
             if([written isEqualToString:@"NO"]){
                 [minutesTable setObject:@"YES" forKey:kMonitorWroteInTimeSlice];
                 if(previousTable != nil){
                     //if we wrote in the previous time slice count the current time as in the consecutive write count
                     written = [previousTable objectForKey:kMonitorWroteInTimeSlice];
                     if([written isEqualToString:@"YES"]){
                         cWrites++;
                     }
                     else if ([written isEqualToString:@"NO"]){
                         cWrites = 0;
                     }
                 }
             }
             return;
         }
         previousTable = minutesTable;
     }];
    
    if(foundTimeSlot == NO){
        //reset the time table
        secnotice("backoff","didn't find a time slot, resetting the table");
        
        //record if a write occured between the last time slice of
        //the time table entries and now.
        NSMutableDictionary *lastTable = [*timeTable objectForKey:kMonitorFifthMinute];
        NSDate *lastDate = [lastTable objectForKey:kMonitorFifthMinute];
        
        if(lastDate && ((double)[currentTime timeIntervalSinceDate: lastDate] >= seconds_per_minute)){
            *consecutiveWrites = [[NSNumber alloc]initWithInt:0];
        }
        else{
            NSString* written = [lastTable objectForKey:kMonitorWroteInTimeSlice];
            if(written && [written isEqualToString:@"YES"]){
                cWrites++;
                *consecutiveWrites = [[NSNumber alloc]initWithInt:cWrites];
            }
            else{
                *consecutiveWrites = [[NSNumber alloc]initWithInt:0];
            }
        }
        
        *timeTable  = [[KeychainSyncingOverIDSProxy idsProxy] initializeTimeTable:key];
        return;
    }
    *consecutiveWrites = [[NSNumber alloc]initWithInt:cWrites];
}
- (void)recordTimestampOfWriteToIDS:(NSDictionary *)values deviceName:(NSString*)name peerID:(NSString*)peerid
{
    if([[KeychainSyncingOverIDSProxy idsProxy].monitor count] == 0){
        [values enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
         {
             [self initializeKeyEntry: key];
         }];
    }
    else{
        [values enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
         {
             NSMutableDictionary *keyEntry = [[KeychainSyncingOverIDSProxy idsProxy].monitor objectForKey:key];
             if(keyEntry == nil){
                 [self initializeKeyEntry: key];
             }
             else{
                 NSNumber *penalty_timeout = [keyEntry objectForKey:kMonitorPenaltyBoxKey];
                 NSDate *lastWriteTimestamp = [keyEntry objectForKey:kMonitorLastWriteTimestamp];
                 NSMutableDictionary *timeTable = [keyEntry objectForKey: kMonitorTimeTable];
                 NSNumber *existingWrites = [keyEntry objectForKey: kMonitorConsecutiveWrites];
                 NSDate *currentTime = [NSDate date];
                 
                 //record the write happened in our timetable structure
                 [self recordTimestampForAppropriateInterval:&timeTable key:key consecutiveWrites:&existingWrites];
                 
                 int consecutiveWrites = [existingWrites intValue];
                 secnotice("backoff","consecutive writes: %d", consecutiveWrites);
                 [keyEntry setObject:existingWrites forKey:kMonitorConsecutiveWrites];
                 [keyEntry setObject:timeTable forKey:kMonitorTimeTable];
                 [keyEntry setObject:currentTime forKey:kMonitorLastWriteTimestamp];
                 [[KeychainSyncingOverIDSProxy idsProxy].monitor setObject:keyEntry forKey:key];
                 
                 if( (penalty_timeout && [penalty_timeout intValue] != 0 ) || ((double)[currentTime timeIntervalSinceDate: lastWriteTimestamp] <= 60 && consecutiveWrites >= 5)){
                     
                     if( (penalty_timeout == nil || [penalty_timeout intValue] == 0) && consecutiveWrites == 5){
                         secnotice("backoff","written for 5 consecutive minutes, time to start throttling");
                        [self increasePenalty:penalty_timeout key:key keyEntry:&keyEntry deviceName:name peerID:peerid];
                     }
                     else
                         secnotice("backoff","monitor: keys have been written for 5 or more minutes, recording we wrote during timeout");
                     
                     //record we wrote during a timeout
                     [keyEntry setObject: @"YES" forKey:kMonitorDidWriteDuringPenalty];
                 }
                 else if((double)[currentTime timeIntervalSinceDate: lastWriteTimestamp] <= 60 && consecutiveWrites < 5){
                     //for debugging purposes
                     secnotice("backoff","monitor: still writing freely");
                     [keyEntry setObject: @"NO" forKey:kMonitorDidWriteDuringPenalty];
                 }
                 else if([penalty_timeout intValue] != 0 && ((double)[currentTime timeIntervalSinceDate: lastWriteTimestamp] > 60 && consecutiveWrites > 5) ){
                     
                     //encountered a write even though we're in throttle mode
                     [keyEntry setObject: @"YES" forKey:kMonitorDidWriteDuringPenalty];
                 }
             }
         }];
    }
}

- (NSDictionary*)filterForWritableValues:(NSDictionary *)values
{
    secnotice("backoff", "filterForWritableValues: %@", values);
    NSMutableDictionary *keyEntry_operationType = [[KeychainSyncingOverIDSProxy idsProxy].monitor objectForKey:@"IDSMessageOperation"];
    
    secnotice("backoff", "keyEntry_operationType: %@", keyEntry_operationType);

    NSNumber *penalty = [keyEntry_operationType objectForKey:kMonitorPenaltyBoxKey];
 
    if(penalty && [penalty intValue] != 0){
        
        NSMutableArray *queuedMessage = [[KeychainSyncingOverIDSProxy idsProxy].monitor objectForKey:kMonitorMessageQueue];
        if(queuedMessage == nil)
            queuedMessage = [[NSMutableArray alloc] initWithCapacity:queue_depth];

        secnotice("backoff", "writing to queuedMessages: %@", queuedMessage);

        if([queuedMessage count] == 0)
            [queuedMessage addObject:values];
        else
            [queuedMessage replaceObjectAtIndex:(queue_depth-1) withObject: values];

        [[KeychainSyncingOverIDSProxy idsProxy].monitor setObject:queuedMessage forKey:kMonitorMessageQueue];
        return NULL;
    }

    return values;
}

@end
