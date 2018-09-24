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
static const NSArray *_debugKeys;

static const char MAIN_OPTIONS_SHORT[] = "m:g:s:t:hv";
static const struct option MAIN_OPTIONS[] =
{
    { "matching",   required_argument,  NULL,   'm' },
    { "get",        required_argument,  NULL,   'g' },
    { "set",        required_argument,  NULL,   's' },
    { "type",       required_argument,  NULL,   't' },
    { "help",       0,                  NULL,   'h' },
    { "verbose",    0,                  NULL,   'v' },
    { NULL,         0,                  NULL,    0  }
};

const char reportUsage[] =
"\nMonitor HID device reports\n"
"\nUsage:\n\n"
"  hidutil report [--get <reportID> ][ --set <reportID> <bytes> ][ --type <reportType> ][ --matching <matching> ] [ --verbose ]\n"
"\nFlags:\n\n"
"  -g  --get...................get report for report ID. Use --type to specify report type\n"
"  -s  --set...................set report with report ID and data. Use --type to specify report type\n"
"  -t  --type..................report type (input, output, feature), defaults to feature\n"
"  -v  --verbose...............print device enumerated/terminated info\n"
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

static OSStatus getReport(IOHIDManagerRef manager, uint8_t reportID, IOHIDReportType reportType) {
    NSArray *devices = (NSArray *)CFBridgingRelease(IOHIDManagerCopyDevices(manager));
    OSStatus  ret = EXIT_SUCCESS;
    
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
            printf("GetReport failed for device:0x%llx reportid:%d type:%d status:0x%x\n", regID, reportID, reportType, result);
            ret = EXIT_FAILURE;
        } else {
            printf("GetReport device:0x%llx reportID:%d type:%d length:%ld bytes:", regID, reportID, reportType, reportLength);
            for (unsigned int index=0; index < (unsigned int)reportLength; index++)
                printf("%02x ", report[index]);
            printf("\n");
        }
        
        IOHIDDeviceClose(deviceRef, 0);
        free(report);
    }
    return ret;
}

static OSStatus setReport(IOHIDManagerRef manager, uint8_t reportID, IOHIDReportType reportType, uint8_t *report, uint32_t reportLength) {
    NSArray *devices = (NSArray *)CFBridgingRelease(IOHIDManagerCopyDevices(manager));
    OSStatus ret = noErr;
    
    for (id device in devices) {
        IOHIDDeviceRef  deviceRef   = (__bridge IOHIDDeviceRef)device;
        IOReturn        result      = kIOReturnError;
        uint64_t        regID;

        IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(deviceRef), &regID);

        IOHIDDeviceOpen(deviceRef, 0);

        result = IOHIDDeviceSetReport(deviceRef, reportType, reportID, report, reportLength);
        
        if (result != kIOReturnSuccess) {
            printf("SetReport failed for device:0x%llx reportid:%d type:%d status:0x%x\n", regID, reportID, reportType, result);
            ret = kCommandErr;
        } else {
            printf("SetReport device:0x%llx reportID:%d type:%d length:%d bytes:", regID, reportID, reportType, reportLength);
            for (unsigned int index=0; index < (unsigned int)reportLength; index++)
                printf("%02x ", report[index]);
            printf("\n");
        }
        
        IOHIDDeviceClose(deviceRef, 0);
    }
    return ret;
}

static NSString *_flatten(id object)
{
    NSMutableString *result = [NSMutableString new];
    
    if ([object isKindOfClass:[NSArray class]]) {
        NSArray *arr = (NSArray *)object;
        [result appendString:@"("];
        for (id objs in arr) {
            [result appendString:_flatten(objs)];
            
            if (objs != arr.lastObject) {
                [result appendString:@", "];
            }
        }
        [result appendString:@")"];
    } else if ([object isKindOfClass:[NSDictionary class]]) {
        [result appendString:@"{"];
        NSDictionary *dict = (NSDictionary *)object;
        NSArray *keys = [dict allKeys];
        
        for (id key in keys) {
            [result appendFormat:@"%@:", key];
            [result appendString:_flatten(dict[key])];
            
            if (key != keys.lastObject) {
                [result appendString:@", "];
            }
        }
        [result appendString:@"}"];
    } else {
        [result appendFormat:@"%@", object];
    }
    
    return result;
}

static void _deviceAddedCallback(void *context __unused,
                                 IOReturn result __unused,
                                 void *sender __unused,
                                 IOHIDDeviceRef device)
{
    uint64_t regID;
    
    IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(device), &regID);
    
    printf("device added:0x%llx ", regID);
    for (NSString *key in _debugKeys) {
        id val = (__bridge id)IOHIDDeviceGetProperty(device, (__bridge CFStringRef)key);
        printf("%s ", [[NSString stringWithFormat:@"%@:%@", key, _flatten(val)] UTF8String]);
    }
    printf("\n");
}

static void _deviceRemovedCallback(void *context __unused,
                                   IOReturn result __unused,
                                   void *sender __unused,
                                   IOHIDDeviceRef device)
{
    uint64_t regID;
    
    IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(device), &regID);
    
    printf("device removed:0x%llx ", regID);
    for (NSString *key in _debugKeys) {
        id val = (__bridge id)IOHIDDeviceGetProperty(device, (__bridge CFStringRef)key);
        printf("%s ", [[NSString stringWithFormat:@"%@:%@", key, _flatten(val)] UTF8String]);
    }
    printf("\n");
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
    bool                verbose         = false;
    int                 arg, tmpLength;
    _debugKeys = @[ @kIOClassKey, @kIOHIDPrimaryUsagePageKey, @kIOHIDPrimaryUsageKey, @kIOHIDVendorIDKey, @kIOHIDProductIDKey,
                     @kIOHIDTransportKey, @kIOHIDLocationIDKey, @kIOHIDProductKey, @kIOHIDManufacturerKey, @kIOHIDDeviceUsagePairsKey ];
    
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
                // --verbose
            case 'v':
                verbose = true;
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
                    status = kOptionErr;
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
                    status = kOptionErr;
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
        status = getReport(manager, reportID, reportType);
        goto exit;
    }
    
    // set report
    if (set) {
        status = setReport(manager, reportID, reportType, reportData, reportLength);
        goto exit;
    }
    
    if (verbose) {
        IOHIDManagerRegisterDeviceMatchingCallback(manager, _deviceAddedCallback, 0);
        IOHIDManagerRegisterDeviceRemovalCallback(manager, _deviceRemovedCallback, 0);
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
