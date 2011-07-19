/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
/*
 *
These tests elicit crashes in PM configd plugin as seen in:
    <rdar://problem/6765534> STOMPER? 10A322: configd crash (loss of external DNS resolution on 10A322-10A343)
 *
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h> 

#include "PMTestLib.h"

#define	MAXPASS			1000
#define	MAXPASS_FORK		50
#define	MAXPASS_NOTIFY		20

void check_IOPMAssertionCreate_and_Release(bool doFork, int *didTests);
void check_IOPMConnectionCreate_and_Release(bool doFork, int *didTests);

#define kMainDoLoops    5


int main(int argc, char **argv)
{
    int i = 0;
    int didTests = 0;

    PMTestInitialize("Assertions crash configd - 6765534", "com.apple.iokit.powermanagement.assertions_stress");

    while (i++ < kMainDoLoops)
    {
        PMTestLog("MainLoop=%d Executing check_IOPMConnectionCreate_and_Release", i);
        check_IOPMConnectionCreate_and_Release(FALSE, &didTests);
        PMTestPass("IOPMConnection create and release loops SUCCESS: did %d", didTests);

        PMTestLog("MainLoop=%d Executing check_IOPMAssertionCreate_and_Release", i);
        check_IOPMAssertionCreate_and_Release(FALSE, &didTests);
        PMTestPass("IOPMConnection Assertion create and release loops SUCCESS: did %d", didTests);

        PMTestLog("MainLoop=%d Executing check_IOPMAssertionCreate_and_Release with child deaths", i);
        check_IOPMAssertionCreate_and_Release(TRUE, &didTests);
        PMTestPass("IOPMConnection Assertion create and child death SUCCESS: did %d", didTests);
    }
}


void check_IOPMAssertionCreate_and_Release(bool doFork, int *didTests)
{
	int		i;
	int		n	= doFork ? MAXPASS_FORK : MAXPASS;
	IOReturn	ret;

	for (i = 0; i < n; i++) {
		IOPMAssertionID	assertion_id;
		pid_t		pid;
		int		status;

//        printf("Assertion %d of %d, %s", i, n, doFork?"fork":"no fork");
		pid = doFork ? fork() : 0;
		switch (pid) {
		    case -1 :
			perror("fork()");
			exit(1);
			// not reached
		    case  0 :
			ret = IOPMAssertionCreateWithName(kIOPMCPUBoundAssertion,
						  kIOPMAssertionLevelOn,
                          CFSTR("com.apple.iokit.powermanagement.assertions_stress"),
						  &assertion_id);
			if (ret != kIOReturnSuccess) {
				PMTestFail("IOPMAssertionCreate() failed, error = 0x%08x", ret);
				exit(1);
			}
			
			// Set a timeout on every even numbered assertion
			if (0 == i%2) {
			    ret = IOPMAssertionSetTimeout(assertion_id, (CFTimeInterval)30.0);
                if (ret != kIOReturnSuccess) {
                    PMTestFail("IOPMAssertionSetTimeout() failed 0x%08x", ret);
                    exit(1);
                }
			}

			// if doFork, let app exit release the assertion
			if (doFork) _exit(0);

			// explicitly release the assertion
			ret = IOPMAssertionRelease(assertion_id);
			if (ret != kIOReturnSuccess) {
				PMTestFail("IOPMAssertionRelease() failed 0x%08x", ret);
				exit(1);
			}

			break;
		    default :
			pid = wait4(pid, &status, 0, NULL);
			if ((pid == -1) ||
			    !WIFEXITED(status) ||
			    (WEXITSTATUS(status) != 0)) {
				PMTestFail("child process did not exit cleanly");
				exit(1);
			}
		}
	}

    *didTests = i;

	return;
}


void myDummyPMConnectionHandler(
    void *param, 
    IOPMConnection connection, 
    IOPMConnectionMessageToken token, 
    IOPMSystemPowerStateCapabilities eventDescriptor)
{
    PMTestFail("myDummyPMConnectionHandler: Unexpectedly got PMConnection callout param=%d capabilities=0x%04x",
                (int)(uintptr_t)param, eventDescriptor);
    return;
}

void
check_IOPMConnectionCreate_and_Release(bool doFork, int *didTests)
{
	int		i;
	int		n	= doFork ? MAXPASS_FORK : MAXPASS;
	IOReturn	ret;

	for (i = 0; i < n; i++) {
		IOPMConnection	connect;

		pid_t		pid;
		int		status;

//        printf("Connection %d of %d, %s", i, n, doFork?"fork":"no fork");
		pid = doFork ? fork() : 0;
		switch (pid) {
		    case -1 :
			PMTestFail("Connection_Create_And_Release failure fork() iteration %d", i);
			exit(1);
			// not reached
		    case  0 :
                ret = IOPMConnectionCreate(CFSTR("I am so forked"),
                              kIOPMSystemPowerStateCapabilityDisk | kIOPMSystemPowerStateCapabilityNetwork
                             | kIOPMSystemPowerStateCapabilityCPU,
                              &connect);
                if (ret != kIOReturnSuccess) {
                    PMTestFail("IOPMConnectionCreate() failed, error = 0x%08x", ret);
                    exit(1);
                }
                
                ret = IOPMConnectionSetNotification(connect, NULL, myDummyPMConnectionHandler);
                if (ret != kIOReturnSuccess) {
                    PMTestFail("IOPMConnectionSetNotification() failed 0x%08x", ret);
                    exit(1);
                }

                ret = IOPMConnectionScheduleWithRunLoop(connect, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
                if (ret != kIOReturnSuccess) {
                    PMTestFail("IOPMConnectionScheduleWithRunLoop() failed 0x%08x", ret);
                    exit(1);
                }
    
                // if doFork, let app exit release the assertion
                if (doFork) _exit(0);
    
                // explicitly release
                ret = IOPMConnectionRelease(connect);
                if (ret != kIOReturnSuccess) {
                    PMTestFail("IOPMConnectionRelease() failed");
                    exit(1);
                }

			break;
		    default :
			pid = wait4(pid, &status, 0, NULL);
			if ((pid == -1) ||
			    !WIFEXITED(status) ||
			    (WEXITSTATUS(status) != 0)) {
				PMTestFail("child process did not exit cleanly");
				exit(1);
			}
		}
	}

    *didTests = i;

	return;
}

