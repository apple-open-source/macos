//
//  log_control.c
//
//  sec
//

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

static const char * getScopeIDName(int id)
{
    switch (id) {
        case kScopeIDXPC:           return "XPC";
        case kScopeIDDefaults:      return "Defaults";
        case kScopeIDEnvironment:   return "Environment Variables";
        case kScopeIDConfig:        return "Config";
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
    while ((ch = getopt(argc, argv, "ls:")) != -1)
    {
        switch  (ch)
        {
            case 'l':
                list = true;
                break;
            case 's':
                set_log_settings(optarg);
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
