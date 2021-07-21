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
//  AggdDailyReport.c
//  PowerManagement
//
//  Created by Mahdi Hamzeh on 12/7/16.
//
//

#include "AggdDailyReport.h"

os_log_t    aggd_log = NULL;

#undef   LOG_STREAM
#define  LOG_STREAM   aggd_log

static IOReportSubscriptionRef IOReporter_subscription_init(CFMutableDictionaryRef *subscribed_channels);
static void initializeDailySample(void);
static void submitAggdDailyReport(void);
static void Process_IOReporter_Sample(CFMutableDictionaryRef newSample);

void initializeAggdDailyReport(void)
{
    aggd_log = os_log_create(PM_LOG_SYSTEM, AGGD_REPORTS_LOG);

    IOReporter_subscription =
    (struct IOReporter_client_subscription *)malloc(sizeof(struct IOReporter_client_subscription));
    //Make sure it is clean
    memset(IOReporter_subscription, 0, sizeof(struct IOReporter_client_subscription));
    //initialize the subscription
    IOReporter_subscription->subscription = IOReporter_subscription_init(&IOReporter_subscription->subscribed_channels);

    if (IOReporter_subscription->subscription!=NULL)
    {
        //allocate memory for sampling buffers
        IOReport_DailySample_Buffer =(struct IOReport_DailySample *)malloc(sizeof(struct IOReport_DailySample));

        //make sure it is clean
        memset(IOReport_DailySample_Buffer, 0, sizeof(struct IOReport_DailySample));

        //read the kernel value and update the buffer
        initializeDailySample();

        gAggdMonitor = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());

        dispatch_source_set_event_handler(gAggdMonitor, ^{ submitAggdDailyReport(); });

        dispatch_source_set_cancel_handler(gAggdMonitor, ^{
            dispatch_release(gAggdMonitor);
            gAggdMonitor = NULL;
        });

        dispatch_source_set_timer(gAggdMonitor, dispatch_walltime(NULL, gAggdMonitorInterval), gAggdMonitorInterval, 0);
        dispatch_resume(gAggdMonitor);
    }
}

static IOReportSubscriptionRef IOReporter_subscription_init(CFMutableDictionaryRef *subscribed_channels)
{
    int channel_count = 0;
    IOReportIterationResult ires;
    CFMutableDictionaryRef power_channels = NULL;
    IOReportSubscriptionRef subscription = NULL;
    CFMutableDictionaryRef matchingDict = NULL;

    matchingDict = IOServiceMatching("IOPMGR");
    if (!matchingDict) {
        goto exit;
    }

    power_channels = IOReportCopyChannelsForDrivers(matchingDict, kIOReportOptGroupSubs, NULL);
    if (power_channels == NULL)
    {
        ERROR_LOG("Power channel subscription error for aggd report\n");
        goto exit;
    }
    /* subscribe to only the CPU power part of the energy model: */
    ires = IOReportPrune(power_channels, (IOReportIteratorBlock)^(IOReportChannelRef ch) {
        if ((IOReportChannelGetCategories(ch) & kIOReportCategoryPower) == 0) {
            return kIOReportIterSkipped;
        }

        CFStringRef chname = IOReportChannelGetChannelName(ch);
        if (chname == NULL) {
            ERROR_LOG("IOReportChannelName is NULL");
            return kIOReportIterSkipped;;
        }
        for(int index = 0 ; index < NO_DAILY_REPORT_EVENTS; index++)
        {
            if (CFStringCompare(chname, Daily_Report_Name_Keys.reportNameKeyBundle[index].name, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                INFO_LOG("Channel to report event %@ is found\n",Daily_Report_Name_Keys.reportNameKeyBundle[index].name);
                return kIOReportIterOk;
            }
        }
        return kIOReportIterSkipped;
    });
    channel_count = IOReportGetChannelCount(power_channels);
    if (channel_count == 0 )
    {
        DEBUG_LOG("Could not find VDROOP channel for aggd report\n");
        goto exit;
    }
    
    subscription = IOReportCreateSubscription(NULL,
                                              power_channels,
                                              subscribed_channels,
                                              kIOReportSubOptsNone,
                                              NULL);
    if (!subscription)
    {
        ERROR_LOG("Could not subscribe to any channel for aggd report\n");
    }
exit:
    if (power_channels) CFRelease(power_channels);
    if (matchingDict) CFRelease(matchingDict);
    return subscription;
}

//Initialized sample from driver
//Make sure initial value is what kernel reports, if initial value is not zero, the reported data would not be pulloted
static void initializeDailySample(void)
{
    CFMutableDictionaryRef sample;
    if (IOReporter_subscription->subscription)
    {
        sample = IOReportCreateSamples(IOReporter_subscription->subscription, IOReporter_subscription->subscribed_channels, NULL);
        if (!sample)
        {
            ERROR_LOG("Error in initial sampling from channel for aggd report\n");
        }
        else
        {
            IOReport_DailySample_Buffer->sampledData = sample;
        }
        
    }
    
}

static void submitAggdDailyReport(void)
{
    CFMutableDictionaryRef sample;
    CFMutableDictionaryRef sampleDelta=NULL;
    
    if (IOReporter_subscription->subscription)
    {
        sample = IOReportCreateSamples(IOReporter_subscription->subscription, IOReporter_subscription->subscribed_channels, NULL);
        if (!sample)
        {
            ERROR_LOG("Error in sampling from channel for aggd report\n");
        }
        else
        {
            //initial sample failed?
            if (IOReport_DailySample_Buffer->sampledData)
            {
                sampleDelta = IOReportCreateSamplesDelta(IOReport_DailySample_Buffer->sampledData, sample, NULL);
                CFRelease(IOReport_DailySample_Buffer->sampledData);
            }
            
            //update current sample
            IOReport_DailySample_Buffer->sampledData=sample;
            
            //computing delta sample failed?
            if (!sampleDelta)
            {
                ERROR_LOG("Error in computing samples delta\n");
            }
            else
            {
                Process_IOReporter_Sample(sampleDelta);
                CFRelease(sampleDelta);
            }
        }
    }
}
static IOReportIterationResult process_event(IOReportSampleRef spl, uint64_t unit, struct IOReport_Event_Buffer *IOReportSampleTotal, struct Daily_Report_Name_AggdKeys_Bundle *NameKeysBundle)
{
    switch (IOReportChannelGetFormat(spl))
    {
            //we expect channel report format
        case kIOReportFormatState:
            
            if (IOReportStateGetCount(spl)!=CHANNEL_NUMSTATES)
            {
                ERROR_LOG("Number of states does not match channel %@\n", NameKeysBundle->name);
                return kIOReportIterFailed;
                
            };
            UInt64 total_res = 0;
            UInt64 chan_Val = 0;
            for (int idx = 0; idx < IOReportStateGetCount(spl); idx++)
            {
                //read value from channel
                chan_Val = IOReportStateGetResidency(spl, idx);
                //scale to ms unit
                IOReportSampleTotal->Event_StateResidency[idx] = IOReportScaleValue(chan_Val, unit, kIOReportUnit_ms);
                total_res += chan_Val;
                
            }
            //if total residency is zero, something is wrong
            if (total_res == 0)
            {
                ERROR_LOG("Total residency on channel %@ is 0\n", NameKeysBundle->name);
                return kIOReportIterFailed;
                
            }
            
            
            IOReportSampleTotal->Event_Triggered_Count = IOReportStateGetInTransitions(spl, 1);
            if (IOReportSampleTotal->Event_Triggered_Count == kIOReportInvalidValue)
            {
               ERROR_LOG("Bad transition count on channel %@\n",NameKeysBundle->name);
                return kIOReportIterFailed;
            }
            
            double delta_time = IOReportSampleTotal->Event_StateResidency[CHANNEL_NUMSTATES-1];
            ADClientAddValueForScalarKey(NameKeysBundle->CountAggdKey,IOReportSampleTotal->Event_Triggered_Count);
            ADClientAddValueForScalarKey(NameKeysBundle->DurationAggdKey,delta_time);
            INFO_LOG("Add to aggd string %@ and %@ for value %llu and %f\n", NameKeysBundle->CountAggdKey, NameKeysBundle->DurationAggdKey, IOReportSampleTotal->Event_Triggered_Count, delta_time);
            
            return kIOReportIterOk;
            
            //Channel report format did not match with our expectation!
        default:
            INFO_LOG("Unexpected channel %@ type\n", NameKeysBundle->name);
            return kIOReportIterFailed;
    }
}

static void Process_IOReporter_Sample(CFMutableDictionaryRef newSample)
{
    IOReportIterationResult ires;
    
    /* Extract channels from IOReporting data */
    ires = IOReportIterate((CFDictionaryRef)newSample, (IOReportIteratorBlock)^(IOReportSampleRef spl) {
        CFStringRef chname = IOReportChannelGetChannelName(spl);
        
        uint64_t unit = IOReportChannelGetUnit(spl);
        for (int index = 0; index < NO_DAILY_REPORT_EVENTS; index++)
        {
            if (CFStringCompare(chname, Daily_Report_Name_Keys.reportNameKeyBundle[index].name, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            {
                return process_event(spl,  unit,
                                     &IOReport_DailySample_Buffer->DailyReportingEvent,
                                     &Daily_Report_Name_Keys.reportNameKeyBundle[index]);
                
                
            }
        }
        //We should not reach here unless something goes wrong
        ERROR_LOG("Could not find event to report for channel %@\n", chname);
        return kIOReportIterFailed;
    });
    
}

