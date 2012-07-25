/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dispatch/dispatch.h>
#include <CoreFoundation/CFNumber.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

typedef enum {
    kDefaultAssertionFlag    = 0,
    kIdleAssertionFlag       = (1 << 0),
    kDisplayAssertionFlag    = (1 << 1),
    kSystemAssertionFlag     = (1 << 2),
    kUserActiveAssertionFlag = (1 << 3)
} AssertionFlag;

typedef struct {
    AssertionFlag assertionFlag;
    CFStringRef assertionType;
} AssertionMapEntry;


AssertionMapEntry assertionMap[] = {
    { kIdleAssertionFlag,       kIOPMAssertionTypePreventUserIdleSystemSleep },
    { kDisplayAssertionFlag,    kIOPMAssertionTypePreventUserIdleDisplaySleep },
    { kSystemAssertionFlag,     kIOPMAssertionTypePreventSystemSleep},
    { kUserActiveAssertionFlag, kIOPMAssertionUserIsActive}};


static CFStringRef        kHumanReadableReason = CFSTR("THE CAFFEINATE TOOL IS PREVENTING SLEEP.");
static CFStringRef        kLocalizationBundlePath = CFSTR("/System/Library/CoreServices/powerd.bundle");

#define kAssertionNameString    "caffeinate command-line tool"

int createAssertions(const char *progname, AssertionFlag flags, long timeout);
void forkChild(char *argv[], AssertionFlag flag);
void usage(void);

int
main(int argc, char *argv[])
{
    AssertionFlag flags = kDefaultAssertionFlag;
    char ch;
    long timeout = 0;

    errno = 0;
    while ((ch = getopt(argc, argv, "dhisut:")) != -1) {
        switch((char)ch) {
            case 'd':
                flags |= kDisplayAssertionFlag;
                break;
            case 'h':
                usage();
                exit(0);
            case 'i':
                flags |= kIdleAssertionFlag;
                break;
            case 's':
                flags |= kSystemAssertionFlag;
                break;
            case 'u':
                flags |= kUserActiveAssertionFlag;
                break;
            case 't':
                timeout = strtol(optarg, NULL,  0);
                if (errno != 0) {
                    usage();
                    exit(1);
                }
                break;

            case '?':
            default:
                usage();
                exit(1);
        }
    }

    if (flags == kDefaultAssertionFlag) {
        flags = kIdleAssertionFlag;
    }

    if (argc - optind) {
        argv += optind;
        (void) forkChild(argv, flags);
    } else {
        if (createAssertions(NULL, flags, timeout)) {
            exit(1);
        }
        if (timeout) {
            sleep(timeout+5);
            exit(0);
        }
    }

    dispatch_main();
}

int
createAssertions(const char *progname, AssertionFlag flags, long timeout)
{
    IOReturn result = 1;
    char assertionDetails[128];
    CFStringRef assertionDetailsString = NULL;
    IOPMAssertionID assertionID = 0;
    u_int i = 0;

    if (progname) {
        (void)snprintf(assertionDetails, sizeof(assertionDetails),
            "caffeinate asserting on behalf of %s", progname);
    } else if (timeout) {
        (void)snprintf(assertionDetails, sizeof(assertionDetails),
            "caffeinate asserting for %ld secs", timeout);
    } else {
        (void)snprintf(assertionDetails, sizeof(assertionDetails),
            "caffeinate asserting forever");
    }

    assertionDetailsString = CFStringCreateWithCString(kCFAllocatorDefault,
                assertionDetails, kCFStringEncodingMacRoman);
    if (!assertionDetailsString) {
        fprintf(stderr, "Failed to create assertion name %s\n", progname);
        goto finish;
    }

    for (i = 0; i < sizeof(assertionMap)/sizeof(AssertionMapEntry); ++i)
    {
        AssertionMapEntry *entry = assertionMap + i;

        if (!(flags & entry->assertionFlag)) continue;
        if ( (entry->assertionFlag == kUserActiveAssertionFlag) && (timeout == 0))
            timeout = 5;  /* Force a 5sec timeout on user active assertions */

        result = IOPMAssertionCreateWithDescription(entry->assertionType,
                    CFSTR(kAssertionNameString), assertionDetailsString,
                    kHumanReadableReason, kLocalizationBundlePath,
                    (CFTimeInterval)timeout, kIOPMAssertionTimeoutActionRelease,
                    &assertionID);

        if (result != kIOReturnSuccess)
        {
            fprintf(stderr, "Failed to create %s assertion\n",
                CFStringGetCStringPtr(entry->assertionType, kCFStringEncodingMacRoman));
            goto finish;
        }
    }

    result = kIOReturnSuccess;
finish:
    if (assertionDetailsString) CFRelease(assertionDetailsString);

    return result;
}

void
forkChild(char *argv[], AssertionFlag flags)
{
    pid_t pid;
    dispatch_source_t source;

    switch(pid = fork()) {
        case -1:    /* error */
            perror("");
            exit(1);
            /* NOTREACHED */
        case 0:     /* child */
            if (createAssertions(*argv, flags, 0)) {
                _exit(1);
            }
            execvp(*argv, argv);
            perror(*argv);
            _exit((errno == ENOENT) ? 127 : 126);
            /* NOTREACHED */
    }

    /* parent */

    (void)signal(SIGINT, SIG_IGN);
    (void)signal(SIGQUIT, SIG_IGN);

    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid,
        DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    dispatch_source_set_event_handler(source, ^{
        int status;

        if (waitpid(pid, &status, 0) < 0) {
            perror("");
            exit(1);
        }

        exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
    });
    dispatch_resume(source);

    return;
}

void
usage(void)
{
    fprintf(stderr, "usage: caffeinate [-disu] [-t timeout] [command] [arguments]\n");
    return;
}
