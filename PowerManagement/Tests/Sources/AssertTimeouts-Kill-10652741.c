/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#import <IOKit/pwr_mgt/IOPMKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <stdlib.h>
#include <stdio.h>
#include "PMTestLib.h"
#include <spawn.h>
#include <sys/wait.h>
#include <sysexits.h>


/***
    cc -g -o ./AssertTimeouts-Kill-10652741 AssertTimeouts-Kill-10652741.c -arch x86_64 -framework IOKit -framework CoreFoundation

xcrun -sdk iphoneos cc -g -o ./AssertTimeouts-Kill-10652741 AssertTimeouts-Kill-10652741.c -arch armv7 -framework IOKit -framework CoreFoundation \
    -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.Internal.sdk

    
    <rdar://problem/10652741> IOPMAssertionCreateWithDescription should add "kill me" action
***/

#ifndef __PMTESTLIB__
#define PMTestInitialize(x, y)  do { printf(x); printf("\n"); printf(y); printf("\n"); } while (0)
#define PMTestPass      printf
#define PMTestFail      printf
#define PMTestLog       printf
#endif

#ifndef kIOPMAssertionTimeoutActionKillProcess
#define kIOPMAssertionTimeoutActionKillProcess      CFSTR("TimeoutActionKillProcess")
#endif


static void assertToBeKilled(const char *);

static char *stringForSignal(int s) {
    if (SIGTERM == s) {
        return "SIGTERM";
    } else if (SIGKILL == s) {
        return "SIGKILL";
    } else {
        return "SIG?";
    }
}


int main (int argc, char * const argv[])
{
    char ch;
    pid_t child_pid;
    int child_exit;
    char * const child_args[] = {"AssertTimeouts-Kill-10652741", "-s", NULL};
    int spawn_err = 0;
    
    while ((ch = getopt(argc, argv, "s")) != -1)
    {
        if ('s' == ch) {
            assertToBeKilled(argv[0]);
            // Function does not return
        }
        break;
    }
    

    PMTestInitialize("Creating a process that times out an assertion and powerd will kill the process.", "com.apple.iokit.powermanagement");
    
    if (0 != (spawn_err = posix_spawn(&child_pid, argv[0], NULL, NULL, child_args, NULL)))
    {
        PMTestFail("FAIL: posix_spawn error %d\n", spawn_err);
        exit(1);
    }
    
    if (0 == child_pid) {
        PMTestFail("Couldn't spawn worker processes.\n");
        return 1;
    }

    if (-1 == waitpid(child_pid, &child_exit, 0))
    {
        PMTestFail("waitpid returns error. errno=%d\n", errno);
        exit(1);
    }
    
    if (WIFEXITED(child_exit))
    {
        if (EX_NOHOST == WEXITSTATUS(child_exit)) {
            PMTestFail("FAIL: Child process failed creating an assertion.\n");
            return 1;
        } else
        if (EX_SOFTWARE == WEXITSTATUS(child_exit)) {
            PMTestFail("FAIL: Child process timed out. It was supposed to get signal'd to death by powerd.\n");
            return 1;
        } else {
            PMTestFail("FAIL: Child process died with unexpected exit code %d\n", WEXITSTATUS(child_exit));
        }
    }
    
    if (WIFSIGNALED(child_exit)) {
        PMTestPass("PASS: Child exited due to a signal %s(%d).\n", stringForSignal(WTERMSIG(child_exit)), WTERMSIG(child_exit));
    }
}

static const int                kExpectDeathIn = 30;
static const CFTimeInterval     kScheduleKillIn = 10.0;

void assertToBeKilled(const char *label)
{
    IOReturn ret;
    IOPMAssertionID assertion_id;
    
    int i;
    int countdown_to_kill;
    CFStringRef labelString = CFStringCreateWithCString(0, label, kCFStringEncodingUTF8);
    
    ret = IOPMAssertionCreateWithDescription(
                    kIOPMAssertionTypePreventUserIdleSystemSleep,
                    labelString,
                    NULL, NULL, NULL, 
                    kScheduleKillIn, kIOPMAssertionTimeoutActionKillProcess, 
                    &assertion_id);
    
    if (kIOReturnSuccess != ret) {
        exit (EX_NOHOST);
    }

    // Expect that powerd will kill our process while running this for loop.
    for (i=0; i<kExpectDeathIn; i++) {
        sleep(1);
    }
    
    // ERROR: powerd should have killed us by now.
    exit(EX_SOFTWARE);
}
