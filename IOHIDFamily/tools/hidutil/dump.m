//
//  dump.m
//  IOHIDFamily
//
//  Created by YG on 4/14/16.
//
//

#import  <Foundation/Foundation.h>
#include <stdio.h>
#include <strings.h>
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include "hdutil.h"
#include "utility.h"

static int                          _outFile            = STDOUT_FILENO; // default stdout
static CFMutableDictionaryRef       _output             = NULL;
static IOHIDEventSystemClientRef    _eventSystemRef     = NULL;

int dump (int argc, const char * argv[]);

static const char MAIN_OPTIONS_SHORT[] = "o:f:h";
static const struct option MAIN_OPTIONS[] =
{
    { "output",     required_argument,  NULL,   'o' },
    { "format",     required_argument,  NULL,   'f' },
    { "help",       0,                  NULL,   'h' },
    { NULL,         0,                  NULL,    0  }
};

const char dumpUsage[] =
"\nDump HID Event System state\n"
"\nUsage:\n\n"
"  hidutil dump [object] [flags]\n"
"\nExamples:\n\n"
"  hidutil dump system -f xml\n"
"  hidutil dump system -o /tmp/state_dump.txt -f text\n"
"\nObject:\n\n"
"  system.....................HID Event System\n"
"  clients....................HID Event System Clients\n"
"  services...................HID Event System services\n"
"\nFlags:\n\n"
"  -f  --format ..............Format type (xml, text)\n"
"  -o  --output...............Output file (or stdout if not specified)\n";


static void printXML()
{
    CFDataRef xml = CFPropertyListCreateData(kCFAllocatorDefault, _output, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    if (xml) {
        if ((write(_outFile, (char *)CFDataGetBytePtr(xml), CFDataGetLength(xml))) != CFDataGetLength(xml)) {
            printf("Error writing to output.\n");
        }
        
        CFRelease(xml);
    }
}

static void printText()
{
    // TODO: print something prettier than this
    CFStringRef outStr = CFCopyDescription(_output);
    if (outStr) {
        if ((write(_outFile, CFStringGetCStringPtr(outStr, kCFStringEncodingUTF8), CFStringGetLength(outStr))) != CFStringGetLength(outStr)) {
            printf("Error writing to output.\n");
        }
        CFRelease(outStr);
    }
}

static void copyProperty(CFStringRef key)
{
    CFArrayRef arr = IOHIDEventSystemClientCopyProperty(_eventSystemRef, key);
    if (arr) {
        CFDictionaryAddValue(_output, key, arr);
        CFRelease(arr);
    } else {
        CFDictionaryAddValue(_output, key, CFSTR("Unavailable"));
    }
}

static void getClients()
{
    copyProperty(CFSTR(kIOHIDClientRecordsKey));
}

static void getServices()
{
    copyProperty(CFSTR(kIOHIDServiceRecordsKey));
}

static void getSessionFilters()
{
    copyProperty(CFSTR(kIOHIDSessionFilterDebugKey));
}

static void getSystem()
{
    getClients();
    getServices();
    getSessionFilters();
}




int dump (int argc, const char * argv[]) {
    int arg;
    void (*callFunc)(void) = NULL;
    void (*printFunc)(void) = &printXML;
    int  status = STATUS_SUCCESS;
    NSDictionary * matching = @{@"Hidden":@"*"};

    // get options
    while ((arg = getopt_long(argc, (char **) argv, MAIN_OPTIONS_SHORT, MAIN_OPTIONS, NULL)) != -1) {
        switch (arg) {
            // --help
            case 'h':
                printf("%s", dumpUsage);
                return STATUS_SUCCESS;
             // --output
            case 'o':
                if (_outFile > STDERR_FILENO) {
                    close(_outFile);
                }
                if ((_outFile = open(optarg, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
                    printf("Error opening output file\n");
                    return STATUS_ERROR;
                }
                break;
            // --format
            case 'f':
                if (strcmp(optarg, "xml") == 0) {
                    printFunc = &printXML;
                } else if (strcmp(optarg, "text") == 0) {
                    printFunc = &printText;
                }
                break;
            default:
                //return printUsage();
                break;
        }
    }
    
    // get command
    if (optind+1 == argc) {
        callFunc = &getSystem;
    } else {
        if (strcmp(argv[optind+1], "system") == 0) {
            callFunc = &getSystem;
        } else if (strcmp(argv[optind+1], "clients") == 0) {
            callFunc = &getClients;
        } else if (strcmp(argv[optind+1], "services") == 0) {
            callFunc = &getServices;
        } else {
            printf("unrecognized object: %s\n", argv[optind+1]);
            status = STATUS_ERROR;
            goto exit;
        }
    }
#ifdef INTERNAL
    _eventSystemRef = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
#else
    _eventSystemRef = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeSimple, NULL);
#endif
    if (!_eventSystemRef) {
        printf("Unable to create client\n");
        status = STATUS_ERROR;
        goto exit;
    }
    
    
    IOHIDEventSystemClientSetMatching(_eventSystemRef, (__bridge CFDictionaryRef)matching);

    _output = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!_output) {
        status = STATUS_ERROR;
        goto exit;
    }
    
    if (callFunc) {
        callFunc();
    }
    
    if (printFunc) {
        printFunc();
    }
    
exit:
    if (_eventSystemRef)
        CFRelease(_eventSystemRef);
    if (_output)
        CFRelease(_output);
    if (_outFile > STDERR_FILENO) {
        close(_outFile);
    }
    return status;
}
