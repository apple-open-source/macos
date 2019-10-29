/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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


#include <utilities/SecCFWrappers.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"

static int kTestTestCount = 10;
static int MAX_PENALTY_TIME = 32;

struct monitor_traffic {
    CFStringRef key;
    int penalty_box;
    int total_consecutive_attempts;
    int last_write_timestamp;
};

static void clear_penalty(struct monitor_traffic *monitor){
    monitor->penalty_box = 0;
    monitor->total_consecutive_attempts = 0;
    monitor->last_write_timestamp = 0;
}
static int increase_penalty(int time){
    if(time == 32)
        return time;
    else if(time == 0)
        return 1;
    else
        return (time * 2);
}

static int decrease_penalty(int time){
    if(time == 0)
        return time;
    else if(time == 1)
        return 0;
    else
        return (time/2);
}

//calculate new penalty time based off time passed
static void calculate_new_penalty(struct monitor_traffic *monitor, int current_time){
    int difference = current_time - monitor->last_write_timestamp;

    while(difference){
        if(difference >= monitor->penalty_box * 2){
            monitor->penalty_box = decrease_penalty(monitor->penalty_box);
            difference =- monitor->penalty_box *2;
        }
        else
            break;
    }
}

static void keychain_changed_notification(struct monitor_traffic *monitor){
    clear_penalty(monitor);
}

static void initialize_monitor(struct monitor_traffic *monitor){
    monitor->key = CFSTR("ak|alskdfj:a;lskdjf");
    monitor->penalty_box = 0;
    monitor->total_consecutive_attempts = 0;
    monitor->last_write_timestamp = 0;
}

static int backoff_algorithm(CFArrayRef timestamps, struct monitor_traffic *monitor)
{
    __block int successful_writes = 0;
    CFNumberRef timestamp;

    CFArrayForEachC(timestamps, timestamp) {
        int current_time;
        if(!CFNumberGetValue(timestamp, kCFNumberSInt32Type, &current_time))
            return successful_writes;

        if(monitor->last_write_timestamp == 0){ //successful default case, initially sending to another peer
            successful_writes++;
        }
        else if(current_time == 0){ //keychain changed notification fired
            keychain_changed_notification(monitor);

        }
        else{
            if(monitor->last_write_timestamp == (current_time -1) && monitor->total_consecutive_attempts >= 4){
                monitor->penalty_box= increase_penalty(monitor->penalty_box);
                monitor->total_consecutive_attempts++;
            }
            else if(monitor->last_write_timestamp == (current_time -1) && monitor->total_consecutive_attempts < 4 ){
                monitor->total_consecutive_attempts++;
                if(monitor->penalty_box == 0)
                    successful_writes++;
            }
            else if((current_time - monitor->last_write_timestamp) >= (2*monitor->penalty_box)){ //we haven't written consecutively for 2* the penalty time
                monitor->total_consecutive_attempts = 0;
                calculate_new_penalty(monitor, current_time);
                successful_writes++;
            }
            else if((current_time - monitor->last_write_timestamp) <= (2*monitor->penalty_box)){ //nonconsecutive write came in within the penalty time
                monitor->penalty_box= increase_penalty(monitor->penalty_box);
                if(monitor->last_write_timestamp != (current_time-1))
                    monitor->total_consecutive_attempts = 0;
                else
                    monitor->total_consecutive_attempts++;
            }
        }
        if(current_time != 0)
            monitor->last_write_timestamp = current_time;
    }

    return successful_writes;
}

static void tests(void)
{
    struct monitor_traffic *monitor = (struct monitor_traffic*)malloc(sizeof(struct monitor_traffic));
    initialize_monitor(monitor);
    CFMutableArrayRef write_attempts = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFNumberRef timestamp = NULL;
    int time;

/*
 * first test: peer continuously writes for 12 minutes
 */
    for(int i = 1; i< 13; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }
    int successful_writes = backoff_algorithm(write_attempts, monitor);
    ok(successful_writes == 5, "successfull writes should have only reached 5 minutes");
    ok(monitor->penalty_box == MAX_PENALTY_TIME, "penalty box should have maxed out to 32 minutes");

    //reset monitor
    initialize_monitor(monitor);
    CFArrayRemoveAllValues(write_attempts);

/*
 * first test: peer continuously writes for 12 minutes, then backs off 2*(max penalty timeout)
 */
    for(int i = 1; i< 13; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }
    time = 77;
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    time = 109;
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    successful_writes = backoff_algorithm(write_attempts, monitor);
    ok(successful_writes == 7, "successfull writes should have only reached 6"); //5 initial writes, then 1 write after enough time passes
    ok(monitor->penalty_box == (MAX_PENALTY_TIME/4), "penalty box should have maxed out to 16 minutes");

    //reset
    initialize_monitor(monitor);
    CFArrayRemoveAllValues(write_attempts);
    
/*
 * first test: peer continuously writes for 12 minutes, then backs off exponentially until everything effectively resets
 */

    for(int i = 1; i< 13; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }
    time = 76; //+ 32*2
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    time = 108; //+ 16*2
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    time = 124; //+ 8*2
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    time = 132; //+ 4*2
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    time = 136; //+ 2*2
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);
    time = 138; //+ 1*2
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);

    successful_writes = backoff_algorithm(write_attempts, monitor);
    ok(successful_writes == 11, "successfull writes should have only reached 11");
    ok(monitor->penalty_box == 0, "penalty box should reset back to 0");

    //reset
    initialize_monitor(monitor);
    CFArrayRemoveAllValues(write_attempts);

/*
 * first test: peer continuously writes for 12 minutes, then backs off exponentially until everything effectively resets
 */

    for(int i = 1; i< 13; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }
    time = 0; //flag that keychain changed notification fired
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);

    for(int i = 1; i< 13; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }

    successful_writes = backoff_algorithm(write_attempts, monitor);
    ok(successful_writes == 10, "successfull writes should have only reached 10");
    ok(monitor->penalty_box == MAX_PENALTY_TIME, "penalty box should reset back to 0");

    //reset
    initialize_monitor(monitor);
    CFArrayRemoveAllValues(write_attempts);

/*
 * first test: peer continuously writes for 5 minutes, then attempts to write again for another 5 minutes, the once much later
 */
    for(int i = 1; i< 6; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }
    time = 40;
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);

    for(int i = 100; i< 106; i++){
        timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i);
        CFArrayAppendValue(write_attempts, timestamp);
        CFReleaseNull(timestamp);
    }
    time = 250;
    timestamp = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &time);
    CFArrayAppendValue(write_attempts, timestamp);

    successful_writes = backoff_algorithm(write_attempts, monitor);
    ok(successful_writes == 12, "successfull writes should have only reached 10");
    ok(monitor->penalty_box == 0, "penalty box should reset back to 0");
}

int secd_154_engine_backoff(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();
    
    return 0;
}
