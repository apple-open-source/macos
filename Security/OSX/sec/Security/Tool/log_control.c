/*
 * Copyright (c) 2003-2007,2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 *
 * log_control.c
 */

#include <string.h>
#include <getopt.h>
#include <stdlib.h>

#include <Security/SecItem.h>
#include <CoreFoundation/CoreFoundation.h>

#include <SecurityTool/tool_errors.h>

#include <Security/SecLogging.h>

#include <utilities/debugging.h>

#include <utilities/SecCFWrappers.h>

#include "SecurityCommands.h"


static void
set_log_settings(const char * settings)
{
    CFErrorRef error = NULL;

    CFStringRef scope = CFStringCreateWithCString(kCFAllocatorDefault, settings, kCFStringEncodingUTF8);

    if (!SecSetLoggingInfoForXPCScope((CFPropertyListRef) scope, &error)) {
        fprintf(stderr, "Failed: ");
        CFShow(error);
    }

    CFReleaseSafe(scope);
    CFReleaseSafe(error);
}

static void
set_circle_settings(const char * settings)
{
    CFErrorRef error = NULL;
    
    CFStringRef scope = CFStringCreateWithCString(kCFAllocatorDefault, settings, kCFStringEncodingUTF8);
    
    if (!SecSetLoggingInfoForCircleScope((CFPropertyListRef) scope, &error)) {
        fprintf(stderr, "Failed: ");
        CFShow(error);
    }
    
    CFReleaseSafe(scope);
    CFReleaseSafe(error);
}

static const char * getScopeIDName(int id)
{
    switch (id) {
        case kScopeIDXPC:           return "XPC";
        case kScopeIDDefaults:      return "Defaults";
        case kScopeIDEnvironment:   return "Environment Variables";
        case kScopeIDConfig:        return "Config";
        case kScopeIDCircle:        return "Circle";
        default:                    return "Unknown";
    }
};

static const char * getPriorityName(CFNumberRef id_number)
{
    int priority = -1;

    CFNumberGetValue(id_number, kCFNumberIntType, &priority);

    switch (priority) {
        case ASL_LEVEL_EMERG:   return ASL_STRING_EMERG;
        case ASL_LEVEL_ALERT:   return ASL_STRING_ALERT;
        case ASL_LEVEL_CRIT:    return ASL_STRING_CRIT;
        case ASL_LEVEL_ERR:     return ASL_STRING_ERR;
        case ASL_LEVEL_WARNING: return ASL_STRING_WARNING;
        case ASL_LEVEL_NOTICE:  return ASL_STRING_NOTICE;
        case ASL_LEVEL_INFO:    return ASL_STRING_INFO;
        case ASL_LEVEL_DEBUG:   return ASL_STRING_DEBUG;
        default: return "Unknown";

    }
};

static void print_comma_separated(FILE* file, CFArrayRef array)
{
    fprintf(file, "[");
    __block const char *separator = "";
    CFArrayForEach(array, ^(const void *value) {
        cffprint(file, CFSTR("%s%@"), separator, value);
        separator = ", ";
    });
    fprintf(file, "]");

}

static void
list_log_settings()
{
    CFErrorRef error = NULL;

    CFArrayRef result = SecGetCurrentServerLoggingInfo(&error);
    if (result) {
        __block int index = 0;
        CFArrayForEach(result, ^(const void *value) {
            printf("%s: ", getScopeIDName(index));

            if (isArray(value)) {
                print_comma_separated(stdout, (CFArrayRef) value);
                printf("\n");
            } else if (isDictionary(value)) {
                printf("\n");
                CFDictionaryForEach((CFDictionaryRef) value, ^(const void *level, const void *array) {
                    printf("   %s: ", getPriorityName(level));
                    if (isArray(array)) {
                        print_comma_separated(stdout, (CFArrayRef) array);
                    } else {
                        cffprint(stdout, CFSTR("%@"), array);
                    }
                    printf("\n");
                });
            } else {
                cffprint(stdout, CFSTR("%@\n"), value);
            }

            ++index;
        });
    } else {
        fprintf(stderr, "Failed: ");
        CFShow(error);
    }

    CFReleaseSafe(error);
}

int log_control(int argc, char * const *argv)
{
    int ch, result = 2; /* @@@ Return 2 triggers usage message. */

    bool list = false;
    /*
     "-l - list"
     */
    while ((ch = getopt(argc, argv, "ls:c:")) != -1)
    {
        switch  (ch)
        {
            case 'l':
                list = true;
                break;
            case 's':
                set_log_settings(optarg);
                break;
            case 'c':
                set_circle_settings(optarg);
                break;
            case '?':
            default:
                goto fail;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc > 1)
        goto fail;

    if (argc == 1) {
        set_log_settings(argv[0]);

        argc -= 1;
        argv += 1;
    }

    (void) argv;

    if (argc != 0)
        goto fail;

    if (list)
        list_log_settings();

    result = 0;

fail:
    return result;
}
