//
//  report.m
//  IOHIDFamily
//
//  Created by Matt Dekom on 8/15/17.
//

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <getopt.h>
#include <mach/mach_time.h>
#include "utility.h"

int report(int argc, const char * argv[]);

static const char MAIN_OPTIONS_SHORT[] = "m:g:s:t:h";
static const struct option MAIN_OPTIONS[] =
{
    { "matching",   required_argument,  NULL,   'm' },
    { "get",        required_argument,  NULL,   'g' },
    { "set",        required_argument,  NULL,   's' },
    { "type",       required_argument,  NULL,   't' },
    { "help",       0,                  NULL,   'h' },
    { NULL,         0,                  NULL,    0  }
};

const char reportUsage[] =
"\nMonitor HID device reports\n"
"\nUsage:\n\n"
"  hidutil report [--get <reportID> ][ --set <reportID> <bytes> ][ --type <reportType> ][ --matching <matching> ]\n"
"\nFlags:\n\n"
"  -g  --get...................get report for report ID. Use --type to specify report type\n"
"  -s  --set...................set report with report ID and data. Use --type to specify report type\n"
"  -t  --type..................report type (input, output, feature), defaults to feature\n"
MATCHING_HELP
"\nExamples:\n\n"
"  hidutil report\n"
"  hidutil report --get 01 --type feature\n"
"  hidutil report --set 02 0a 0b 0c --type output\n"
"  hidutil report --matching '{\"PrimaryUsagePage\":1,\"PrimaryUsage\":6}'\n";

static void reportCallback(void *context __unused, IOReturn result __unused, void *sender, IOHIDReportType type __unused, uint32_t reportID __unused, uint8_t *report, CFIndex reportLength) {
    IOHIDDeviceRef      device  = (IOHIDDeviceRef)sender;
    uint64_t            absTime = mach_absolute_time();
    uint64_t            regID;
    
    IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(device), &regID);
    
    printf("timestamp:%llu sender:0x%llx length:%ld bytes:", absTime, regID, reportLength);
    for (unsigned int index=0; index < (unsigned int)reportLength; index++)
        printf("%02x ", report[index]);
    printf("\n");
}

static void getReport(IOHIDManagerRef manager, uint8_t reportID, IOHIDReportType reportType) {
    NSArray *devices = (NSArray *)CFBridgingRelease(IOHIDManagerCopyDevices(manager));
    
    for (id device in devices) {
        IOHIDDeviceRef  deviceRef   = (__bridge IOHIDDeviceRef)device;
        NSNumber        *reportSize = nil;
        IOReturn        result      = kIOReturnError;
        uint8_t         *report     = NULL;
        CFIndex         reportLength;
        uint64_t        regID;
        
        IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(deviceRef), &regID);
        
        if (reportType == kIOHIDReportTypeInput) {
            reportSize = (__bridge NSNumber *)IOHIDDeviceGetProperty(deviceRef, CFSTR(kIOHIDMaxInputReportSizeKey));
        } else if (reportType == kIOHIDReportTypeOutput) {
            reportSize = (__bridge NSNumber *)IOHIDDeviceGetProperty(deviceRef, CFSTR(kIOHIDMaxOutputReportSizeKey));
        } else if (reportType == kIOHIDReportTypeFeature) {
            reportSize = (__bridge NSNumber *)IOHIDDeviceGetProperty(deviceRef, CFSTR(kIOHIDMaxFeatureReportSizeKey));
        }
        
        if (!reportSize) {
            continue;
        }
        
        IOHIDDeviceOpen(deviceRef, 0);
        
        reportLength = reportSize.unsignedIntValue;
        report  = malloc(reportLength);
        bzero(report, reportLength);
        
        result = IOHIDDeviceGetReport(deviceRef, reportType, reportID, report, &reportLength);
        
        if (result != kIOReturnSuccess) {
            printf("GetReport failed for device 0x%llx: 0x%x\n", regID, result);
        } else {
            printf("GetReport device:0x%llx reportID:%d type:%d length:%ld bytes:", regID, reportID, reportType, reportLength);
            for (unsigned int index=0; index < (unsigned int)reportLength; index++)
                printf("%02x ", report[index]);
            printf("\n");
        }
        
        IOHIDDeviceClose(deviceRef, 0);
        free(report);
    }
}

static void setReport(IOHIDManagerRef manager, uint8_t reportID, IOHIDReportType reportType, uint8_t *report, uint32_t reportLength) {
    NSArray *devices = (NSArray *)CFBridgingRelease(IOHIDManagerCopyDevices(manager));
    
    for (id device in devices) {
        IOHIDDeviceRef  deviceRef   = (__bridge IOHIDDeviceRef)device;
        IOReturn        result      = kIOReturnError;
        uint64_t        regID;

        IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(deviceRef), &regID);

        IOHIDDeviceOpen(deviceRef, 0);

        result = IOHIDDeviceSetReport(deviceRef, reportType, reportID, report, reportLength);
        
        if (result != kIOReturnSuccess) {
            printf("SetReport failed for device 0x%llx: 0x%x\n", regID, result);
        } else {
            printf("SetReport device:0x%llx reportID:%d type:%d length:%d bytes:", regID, reportID, reportType, reportLength);
            for (unsigned int index=0; index < (unsigned int)reportLength; index++)
                printf("%02x ", report[index]);
            printf("\n");
        }
        
        IOHIDDeviceClose(deviceRef, 0);
    }
}

int report(int argc __unused, const char * argv[] __unused) {
    int                 status          = STATUS_SUCCESS;
    IOHIDManagerRef     manager         = NULL;
    char                *matching       = NULL;
    bool                get             = false;
    bool                set             = false;
    uint8_t             reportID        = 0;
    uint8_t             *reportData     = NULL;
    uint32_t            reportLength    = 0;
    IOHIDReportType     reportType      = kIOHIDReportTypeFeature;
    int                 arg, tmpLength;
    
    while ((arg = getopt_long(argc, (char **) argv, MAIN_OPTIONS_SHORT, MAIN_OPTIONS, NULL)) != -1) {
        switch (arg) {
                // --help
            case 'h':
                printf("%s", reportUsage);
                goto exit;
                // --matching
            case 'm':
                matching = optarg;
                break;
                // --get
            case 'g':
                get = true;
                reportID = strtol(optarg, NULL, 16);
                break;
                // --set
            case 's':
                if (set) {
                    continue;
                }
                
                set = true;
                reportID = strtol(optarg, NULL, 16);
                tmpLength = optind;
                
                for(;tmpLength < argc && *argv[tmpLength] != '-'; tmpLength++) {
                    reportLength++;
                }
                
                if (!reportLength) {
                    status = STATUS_ERROR;
                    goto exit;
                }
                
                reportData = malloc(reportLength);
                
                tmpLength = 0;
                for(;optind < argc && *argv[optind] != '-'; optind++) {
                    reportData[tmpLength++] = strtol(argv[optind], NULL, 16);
                }
                break;
                // --type
            case 't':
                if (strcmp(optarg, "input") == 0) {
                    reportType = kIOHIDReportTypeInput;
                } else if (strcmp(optarg, "output") == 0) {
                    reportType = kIOHIDReportTypeOutput;
                } else if (strcmp(optarg, "feature") == 0) {
                    reportType = kIOHIDReportTypeFeature;
                } else {
                    printf("Unknown report type: %s\n", optarg);
                    status = STATUS_ERROR;
                    goto exit;
                }
                break;
            default:
                printf("%s", reportUsage);
                goto exit;
                break;
        }
    }
    
    manager = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    if (!manager) {
        goto exit;
    }
    
    if (matching) {
        if (!setManagerMatching(manager, matching)) {
            printf("bad matching string: %s\n", matching);
            IOHIDManagerSetDeviceMatching(manager, NULL);
        }
    } else {
        IOHIDManagerSetDeviceMatching(manager, NULL);
    }
    
    // get report
    if (get) {
        getReport(manager, reportID, reportType);
        goto exit;
    }
    
    // set report
    if (set) {
        setReport(manager, reportID, reportType, reportData, reportLength);
        goto exit;
    }
    
    IOHIDManagerRegisterInputReportCallback(manager, reportCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    
    CFRunLoopRun();
    
    IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    
exit:
    if (manager) {
        CFRelease(manager);
    }
    
    if (reportData) {
        free(reportData);
    }
    
    return status;
}
