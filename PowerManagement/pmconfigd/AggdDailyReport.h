/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

//
//  AggdDailyReprt.h
//  PowerManagement
//
//  Created by Mahdi Hamzeh on 12/6/16.
//
//
// To add a new event to the list of daily report
// define a event name, aggd key for count and duration
// increase NO_DAILY_REPORT_EVENTS by the number of new events to be tracked
// and add these new name/keys to Daily_Report_Name_Keys structure

#ifndef AggdDailyReprt_h
#define AggdDailyReprt_h

#include <IOReport.h>
#include <AggregateDictionary/ADClient.h>
#include <syslog.h>
#include "PrivateLib.h"



//channel subscription handle
static dispatch_source_t                 gAggdMonitor = NULL;
//interval between two collection
static uint64_t                          gAggdMonitorInterval = (1 *3600LL * NSEC_PER_SEC);  // read events report every 1 hour
//4 test
//static uint64_t                            gAggdMonitorInterval = (20*NSEC_PER_SEC);  // report every 24



//Bundle of event name and aggd keys associated with the event
struct Daily_Report_Name_AggdKeys_Bundle
{
    CFStringRef name;
    CFStringRef CountAggdKey;
    CFStringRef DurationAggdKey;
};

//the set of event names and keys associated with them
static struct Daily_Report
{
    struct Daily_Report_Name_AggdKeys_Bundle reportNameKeyBundle[NO_DAILY_REPORT_EVENTS];
    
}
//a list of events along with keys
Daily_Report_Name_Keys = {{
    
}};



/* We use this structure to hold channel subscription to ioreporter. */
static struct IOReporter_client_subscription {
    IOReportSubscriptionRef subscription;
    CFMutableDictionaryRef subscribed_channels;
} *IOReporter_subscription = NULL;

#define CHANNEL_NUMSTATES 2

//the structure that collects data
//we expect two state for each event, 0 when it is not occured and 1 for duration when it is occuring
struct IOReport_Event_Buffer
{
    double Event_StateResidency[CHANNEL_NUMSTATES];
    uint64_t Event_Triggered_Count;
};

//One buffer per event we are collecting
static struct IOReport_DailySample
{
    struct IOReport_Event_Buffer DailyReportingEvent;
    CFMutableDictionaryRef sampledData;
    
}
//for holding total event values collected
*IOReport_DailySample_Buffer = NULL;

//Setup daily report
void initializeAggdDailyReport(void);

#endif /* AggdDailyReprt_h */
