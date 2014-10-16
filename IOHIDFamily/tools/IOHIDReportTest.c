#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include "IOHIDReportDescriptorParser.h"

static CFTimeInterval   gPollInterval       = 0.0;
static bool             gReport             = TRUE;
static bool             gValue              = FALSE;
static bool             gSend               = FALSE;
static bool             gSendTransaction    = FALSE;
static bool             gPrintDescriptor    = FALSE;

static CFMutableDictionaryRef   gOutputElements = NULL;

static char * getReportTypeString(IOHIDReportType type)
{
    switch ( type ) {
        case kIOHIDReportTypeInput:
            return "INPUT";
        case kIOHIDReportTypeOutput:
            return "OUTPUT";
        case kIOHIDReportTypeFeature:
            return "FEATURE";
        default:
            return "DUH";
    }
}

static void __deviceReportCallback(void * context, IOReturn result, void * sender, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength)
{
    int index;
    
    printf("IOHIDDeviceRef[%p]: result=0x%08x reportType=%s reportID=%d reportLength=%ld: ", sender, result, getReportTypeString(type), reportID, reportLength);
    for (index=0; result==kIOReturnSuccess && index<reportLength; index++)
        printf("%02x ", report[index]);
    printf("\n");
    
    // toggle a report
    if ( gSend || gSendTransaction ) {
        CFArrayRef outputElements  = NULL;
        
        outputElements = CFDictionaryGetValue(gOutputElements, sender);
        
        if ( outputElements ) {
            static uint8_t      value       = 0;
            IOHIDTransactionRef transaction = NULL;
            
            transaction = IOHIDTransactionCreate(kCFAllocatorDefault, (IOHIDDeviceRef)sender, kIOHIDTransactionDirectionTypeOutput, 0);
            if ( transaction ) {
                IOReturn    ret;
                CFIndex     index, count;
                
                for ( index=0, count = CFArrayGetCount(outputElements); index<count; index++) {
                    
                    IOHIDElementRef element     = (IOHIDElementRef)CFArrayGetValueAtIndex(outputElements, index);
                    IOHIDValueRef   hidValue    = 0;
                    
                    if ( !element )
                        continue;
                    
                    IOHIDTransactionAddElement(transaction, element);
                    
                    hidValue = IOHIDValueCreateWithIntegerValue(kCFAllocatorDefault, element, 0, value);
                    if ( !hidValue )
                        continue;
                    
                    if ( gSendTransaction ) {
                        IOHIDTransactionSetValue(transaction, element, hidValue, 0);
                    } else {
                        ret = IOHIDDeviceSetValue((IOHIDDeviceRef)sender, element, hidValue);
                        printf("Attempt to send value. Ret = 0x%08x\n", ret);
                    }
                    CFRelease(hidValue);
                    
                }
                
                if ( gSendTransaction ) {
                    ret = IOHIDTransactionCommit(transaction);
                    printf("Attempt to send transaction. Ret = 0x%08x\n", ret);
                }
                value = value+1 % 2;
                
                CFRelease(transaction);
            }
        }
    }
}

void __deviceValueCallback (void * context, IOReturn result, void * sender, IOHIDValueRef value)
{
    IOHIDElementRef element = IOHIDValueGetElement(value);
    
    printf("IOHIDDeviceRef[%p]: value=%p timestamp=%lld cookie=%d usagePage=0x%02X usage=0x%02X intValue=%ld\n", sender, value, IOHIDValueGetTimeStamp(value), (uint32_t)IOHIDElementGetCookie(element), IOHIDElementGetUsagePage(element), IOHIDElementGetUsage(element), IOHIDValueGetIntegerValue(value));
}

static void __timerCallback(CFRunLoopTimerRef timer, void *info)
{
    IOHIDDeviceRef  device = (IOHIDDeviceRef)info;
    
    CFNumberRef     number      = NULL;
    CFIndex         reportSize  = 0;
    IOReturn        result;
    
    number = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDMaxInputReportSizeKey));
    if ( !number )
        return;
    
    CFNumberGetValue(number, kCFNumberCFIndexType, &reportSize);
    
    uint8_t report[reportSize];
    
    bzero(report, reportSize);
    
    result = IOHIDDeviceGetReport(device, kIOHIDReportTypeInput, 0, report, &reportSize);
    
    __deviceReportCallback(NULL, result, device, kIOHIDReportTypeInput, 0, report, reportSize);
}

static void __deviceCallback(void * context, IOReturn result, void * sender, IOHIDDeviceRef device)
{
    
    boolean_t   terminated  = context == 0;
    CFStringRef debugString = CFCopyDescription(device);
    
    static CFMutableDictionaryRef s_timers = NULL;
    
    if ( !s_timers )
        s_timers = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    printf("%-10.10s: %s\n", terminated ? "terminated" : "matched", debugString ? CFStringGetCStringPtr(debugString, CFStringGetSystemEncoding()) : "");
    
    if ( debugString )
        CFRelease(debugString);
    
    if ( terminated ) {
        CFDictionaryRemoveValue(gOutputElements, device);
        
        
        CFRunLoopTimerRef timer = (CFRunLoopTimerRef)CFDictionaryGetValue(s_timers, device);
        if ( timer ) {
            CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
        }
        CFDictionaryRemoveValue(s_timers, device);
        
    } else {
        CFArrayRef              outputElements  = NULL;
        CFMutableDictionaryRef  matching        = NULL;
        
        if ( gPrintDescriptor ) {
            CFDataRef descriptor = NULL;
            
            descriptor = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey));
            if ( descriptor ) {
                PrintHIDDescriptor(CFDataGetBytePtr(descriptor), CFDataGetLength(descriptor));
            }
        }
        
        if ( gPollInterval != 0.0 ) {
            CFRunLoopTimerContext   context = {.info=device};
            CFRunLoopTimerRef       timer   = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), gPollInterval, 0, 0, __timerCallback, &context);
            
            CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
            
            CFDictionaryAddValue(s_timers, device, timer);
            CFRelease(timer);
            
            printf("Adding polling timer @ %4.6f s\n", gPollInterval);
        }
        
        matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if ( matching ) {
            uint32_t    value   = kIOHIDElementTypeOutput;
            CFNumberRef number  = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
            
            if ( number ) {
                CFDictionarySetValue(matching, CFSTR(kIOHIDElementTypeKey), number);
                
                outputElements = IOHIDDeviceCopyMatchingElements(device, matching, 0);
                if ( outputElements ) {
                    CFDictionarySetValue(gOutputElements, device, outputElements);
                    CFRelease(outputElements);
                }
                
                CFRelease(number);
            }
            CFRelease(matching);
        }
    }
    
    
}

static void printHelp()
{
    printf("\n");
    printf("hidReportTest usage:\n\n");
    printf("\t-p    Parse descriptor data\n");
    printf("\t-i    Manually poll at a given interval (s)");
    printf("\t--usage <usage>\n");
    printf("\t--usagepage <usage page>\n");
    printf("\t--transport <transport string value\n>");
    printf("\t--vid <vendor id>\n");
    printf("\t--pid <product id>\n");
    printf("\t--transport <transport string value>\n");
    printf("\n");
}

int main (int argc, const char * argv[]) {
    
    IOHIDManagerRef         manager     = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    CFMutableDictionaryRef  matching    = NULL;
    
    int argi;
    for (argi=1; argi<argc; argi++) {
        if ( 0 == strcmp("-v", argv[argi]) ) {
            gValue = TRUE;
        }
        else if ( 0 == strcmp("-p", argv[argi]) ) {
            gPrintDescriptor = TRUE;
        }
        else if ( 0 == strcmp("-s", argv[argi]) ) {
            gSend = TRUE;
        }
        else if ( 0 == strcmp("-st", argv[argi]) ) {
            gSendTransaction = TRUE;
        }
        else if ( 0 == strcmp("-nr", argv[argi]) ) {
            gReport = FALSE;
        }
        else if ( !strcmp("-i", argv[argi]) && (argi+1) < argc) {
            gPollInterval = atof(argv[++argi]);
            printf("gPollInterval = %f seconds\n", gPollInterval);
        }
        else if ( !strcmp("--usage", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsageKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--usagepage", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsagePageKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--vid", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDVendorIDKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--pid", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDProductIDKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--transport", argv[argi]) && (argi+1) < argc) {
            CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, argv[++argi], CFStringGetSystemEncoding());
            if ( string ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDTransportKey), string);
                CFRelease(string);
            }
        }
        else {
            printHelp();
            return 0;
        }
    }
    
    gOutputElements = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    IOHIDManagerRegisterDeviceMatchingCallback(manager, __deviceCallback, (void*)TRUE);
    IOHIDManagerRegisterDeviceRemovalCallback(manager, __deviceCallback, (void*)FALSE);
    
    if ( gReport ) {
        
        if ( gPollInterval == 0.0 ) {
            IOHIDManagerRegisterInputReportCallback(manager, __deviceReportCallback, NULL);
        }
        
    }
    if ( gValue )
        IOHIDManagerRegisterInputValueCallback(manager, __deviceValueCallback, NULL);
    
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerSetDeviceMatching(manager, matching);
    IOHIDManagerOpen(manager, 0);
    
    CFRunLoopRun();
    
    if ( matching )
        CFRelease(matching);
    
    return 0;
}
