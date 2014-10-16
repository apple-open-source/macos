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
#include <getopt.h>

#include <dispatch/dispatch.h>
#include <CoreFoundation/CFNumber.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

typedef enum {
    kDefaultAssertionFlag    = 0,
    kIdleAssertionFlag       = (1 << 0),
    kDisplayAssertionFlag    = (1 << 1),
    kSystemAssertionFlag     = (1 << 2),
    kUserActiveAssertionFlag = (1 << 3),
    kDiskAssertionFlag       = (1 << 4)
} AssertionFlag;

typedef struct {
    AssertionFlag assertionFlag;
    CFStringRef assertionType;
} AssertionMapEntry;


AssertionMapEntry assertionMap[] = {
    { kIdleAssertionFlag,       kIOPMAssertionTypePreventUserIdleSystemSleep },
    { kDisplayAssertionFlag,    kIOPMAssertionTypePreventUserIdleDisplaySleep },
    { kSystemAssertionFlag,     kIOPMAssertionTypePreventSystemSleep},
    { kUserActiveAssertionFlag, kIOPMAssertionUserIsActive},
    { kDiskAssertionFlag,       kIOPMAssertPreventDiskIdle}};


static CFStringRef        kHumanReadableReason = CFSTR("THE CAFFEINATE TOOL IS PREVENTING SLEEP.");
static CFStringRef        kLocalizationBundlePath = CFSTR("/System/Library/CoreServices/powerd.bundle");

#define kAssertionNameString    "caffeinate command-line tool"

int createAssertions(const char *progname, AssertionFlag flags, long timeout);
void forkChild(char *argv[]);
void usage(void);

pid_t   waitforpid;


int
main(int argc, char *argv[])
{
    AssertionFlag flags = kDefaultAssertionFlag;
    char ch;
    unsigned long timeout = 0;
    dispatch_source_t   disp_src;

    errno = 0;
    while ((ch = getopt(argc, argv, "mdhisut:w:")) != -1) {
        switch((char)ch) {
            case 'm':
                flags |= kDiskAssertionFlag;
                break;
            case 'd':
                flags |= kDisplayAssertionFlag;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'i':
                flags |= kIdleAssertionFlag;
                break;
            case 's':
                flags |= kSystemAssertionFlag;
                break;
            case 'u':
                flags |= kUserActiveAssertionFlag;
                break;
            case 'w':
                waitforpid = strtol(optarg, NULL, 0);
                if (waitforpid == 0 && errno != 0) {
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;

            case 't':
                timeout = strtol(optarg, NULL,  0);
                if (timeout == 0 && errno != 0) {
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;

            case '?':
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if (flags == kDefaultAssertionFlag) {
        flags = kIdleAssertionFlag;
    }

    /* Spwan early, otherwise libraries might open resources that don't survive fork or exec */
    if (argc - optind) {
        argv += optind;
        forkChild(argv);
        if (createAssertions(*argv, flags, timeout)) {
            exit(EXIT_FAILURE);
        }
    } else {
        if (timeout) {
            dispatch_time_t d_timeout = dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC);
            dispatch_after(d_timeout, dispatch_get_main_queue(), ^{
                           exit(EXIT_SUCCESS);
                           });
        }
        if (waitforpid) {
            disp_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, waitforpid, 
                                              DISPATCH_PROC_EXIT, dispatch_get_main_queue());

            dispatch_source_set_event_handler(disp_src, ^{
                                              exit(EXIT_SUCCESS);
                                              });

            dispatch_source_set_cancel_handler(disp_src, ^{
                                               dispatch_release(disp_src);
                                               });

            dispatch_resume(disp_src);

        }
        if (createAssertions(NULL, flags, timeout)) {
            exit(EXIT_FAILURE);
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

    assertionDetails[0] = 0;
    if (progname) {
        (void)snprintf(assertionDetails, sizeof(assertionDetails),
            "caffeinate asserting on behalf of \'%s\' (pid %d)", progname, getppid());
    } else if (waitforpid) {
        if (timeout) {
            snprintf(assertionDetails, sizeof(assertionDetails), 
                     "caffeinate asserting on behalf of Process ID %d for %ld secs", 
                     waitforpid, timeout); 
        }
        else {
            snprintf(assertionDetails, sizeof(assertionDetails), 
                     "caffeinate asserting on behalf of Process ID %d", waitforpid); 
        }
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
        if (!progname && waitforpid) {
            CFNumberRef waitforpidRef = CFNumberCreate(0, kCFNumberIntType, &waitforpid);
            if (waitforpidRef) {
                IOPMAssertionSetProperty(assertionID, kIOPMAssertionOnBehalfOfPID, waitforpidRef);
                CFRelease(waitforpidRef);
            }
        }
    }

    result = kIOReturnSuccess;
finish:
    if (assertionDetailsString) CFRelease(assertionDetailsString);

    return result;
}

void
forkChild(char *argv[])
{
    pid_t pid;
    dispatch_source_t source;
    int fd, max_fd;

    /* Our parent might care about the total life cycle of this process,
    * therefore rather than propagate exit status, Unix signals, Mach
    * exceptions, etc, we just flip the normal parent/child relationship and
    * have the parent exec() and the child monitor the parent for death rather
    * than the other way around.
    */
    switch(fork()) {
        case 0:     /* child */
            break;
        case -1:    /* error */
            perror("");
            exit(EXIT_SUCCESS);
            /* NOTREACHED */
        default:    /* parent */
            execvp(*argv, argv);
            int saved_errno = errno;
            perror(*argv);
            _exit((saved_errno == ENOENT) ? 127 : 126);
            /* NOTREACHED */
    }

    pid = getppid();
    if (pid == 1) {
        exit(EXIT_SUCCESS);
    }

    /* Don't leak inherited FDs from our grandparent. */
    max_fd = getdtablesize();
    for (fd = STDERR_FILENO + 1; fd < max_fd; fd++) {
        close(fd);
    }

    (void)signal(SIGINT, SIG_IGN);
    (void)signal(SIGQUIT, SIG_IGN);

    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid,
        DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    dispatch_source_set_event_handler(source, ^{
        /* we do not need to waitpid() because we are actually the child */
        exit(EXIT_SUCCESS);
    });
    dispatch_resume(source);

    return;
}

void
usage(void)
{
    fprintf(stderr, "usage: caffeinate [-disu] [-t timeout] [-w Process ID] [command arguments...]\n");
    return;
}
