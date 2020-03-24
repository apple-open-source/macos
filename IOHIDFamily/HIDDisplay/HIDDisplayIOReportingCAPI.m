//
//  HIDDisplayIOReportingCAPI.m
//  HIDDisplay
//
//  Created by AB on 4/22/19.
//

#include "HIDDisplayIOReportingCAPI.h"
#include "HIDDisplayPrivate.h"
#include "HIDDisplayIOReportingInterface.h"
#include <IOKit/IOReturn.h>


HIDDisplayIOReportingInterfaceRef __nullable HIDDisplayCreateIOReportingInterfaceWithContainerID(CFStringRef containerID)
{
    HIDDisplayIOReportingInterface *device = [[HIDDisplayIOReportingInterface alloc] initWithContainerID:(__bridge NSString*)containerID];
    
    if (!device) {
        return NULL;
    }
    
    return (__bridge_retained HIDDisplayIOReportingInterfaceRef)device;
}

void HIDDisplayIOReportingSetDispatchQueue(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, dispatch_queue_t queue)
{
    
    HIDDisplayIOReportingInterface *device = (__bridge HIDDisplayIOReportingInterface*)hidDisplayInterface;
    
    [device setDispatchQueue:queue];
    
}

void HIDDisplayIOReportingSetInputDataHandler(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, IOReportingInputDataHandler handler)
{
    
    HIDDisplayIOReportingInterface *device = (__bridge HIDDisplayIOReportingInterface*)hidDisplayInterface;
    
    [device setInputDataHandler:handler];
    
}

bool HIDDisplayIOReportingSetOutputData(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, CFDataRef data, CFErrorRef* error)
{
    HIDDisplayIOReportingInterface *device = (__bridge HIDDisplayIOReportingInterface*)hidDisplayInterface;
    
    NSError *err = nil;
    
    bool ret = [device setOutputData:(__bridge NSData*)data error:&err];
    
    if (ret == false && error) {
        *error = (__bridge_retained CFErrorRef)err;
    }
    return ret;
}

void HIDDisplayIOReportingActivate(HIDDisplayIOReportingInterfaceRef hidDisplayInterface)
{
    HIDDisplayIOReportingInterface *device = (__bridge HIDDisplayIOReportingInterface*)hidDisplayInterface;
    
    [device activate];
}

void HIDDisplayIOReportingCancel(HIDDisplayIOReportingInterfaceRef hidDisplayInterface)
{
    HIDDisplayIOReportingInterface *device = (__bridge HIDDisplayIOReportingInterface*)hidDisplayInterface;
    
    [device cancel];
}

void HIDDisplayIOReportingSetCancelHandler(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, dispatch_block_t handler)
{
    
    HIDDisplayIOReportingInterface *device = (__bridge HIDDisplayIOReportingInterface*)hidDisplayInterface;
    
    [device setCancelHandler:handler];
    
}

