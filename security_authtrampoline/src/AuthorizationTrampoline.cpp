/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// AuthorizationTrampoline - simple suid-root execution trampoline
// for the authorization API.
//
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <security_utilities/endian.h>
#include <security_utilities/debugging.h>
#include <security_utilities/logging.h>


#define EXECUTERIGHT kAuthorizationRightExecute


static void fail(OSStatus cause) __attribute__ ((noreturn));


//
// Main program entry point.
//
// Arguments:
//	argv[0] = my name
//	argv[1] = path to user tool
//	argv[2] = "auth n", n=file descriptor of mailbox temp file
//	argv[3..n] = arguments to pass on
//
// File descriptors (set by fork/exec code in client):
//	0 -> communications pipe (perhaps /dev/null)
//	1 -> notify pipe write end
//	2 and above -> unchanged from original client
//
int main(int argc, const char *argv[])
{
	// initial setup
	Syslog::open("authexec", LOG_AUTH);

	// validate basic integrity
	if (!argv[0] || !argv[1] || !argv[2]) {
		Syslog::alert("invalid argument vector");
		exit(1);
	}
	
	// pick up arguments
	const char *pathToTool = argv[1];
	const char *mboxFdText = argv[2];
	const char **restOfArguments = argv + 3;
	secdebug("authtramp", "trampoline(%s,%s)", pathToTool, mboxFdText);

    // read the external form
    AuthorizationExternalForm extForm;
    int fd;
    if (sscanf(mboxFdText, "auth %d", &fd) != 1)
        return errAuthorizationInternal;
    if (lseek(fd, 0, SEEK_SET) ||
            read(fd, &extForm, sizeof(extForm)) != sizeof(extForm)) {
        close(fd);
        return errAuthorizationInternal;
    }

	// internalize the authorization
	AuthorizationRef auth;
	if (OSStatus error = AuthorizationCreateFromExternalForm(&extForm, &auth))
		fail(error);
	secdebug("authtramp", "authorization recovered");
	
	// are we allowed to do this?
	AuthorizationItem right = { EXECUTERIGHT, 0, NULL, 0 };
	AuthorizationRights inRights = { 1, &right };
	AuthorizationRights *outRights;
	if (OSStatus error = AuthorizationCopyRights(auth, &inRights, NULL /*env*/,
			kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, &outRights))
		fail(error);
	if (outRights->count != 1 || strcmp(outRights->items[0].name, EXECUTERIGHT))
		fail(errAuthorizationDenied);
		
	// ----- AT THIS POINT WE COMMIT TO PERMITTING THE EXECUTION -----
	
	// let go of our authorization - the client tool will re-internalize it
	AuthorizationFree(auth, kAuthorizationFlagDefaults);
	
	// put the external authorization form into the environment
	setenv("__AUTHORIZATION", mboxFdText, true);
	setenv("_BASH_IMPLICIT_DASH_PEE", "-p", true);

	// shuffle file descriptors
	int notify = dup(1);		// save notify port
	fcntl(notify, F_SETFD, 1);	// close notify port on (successful) exec
	dup2(0, 1);					// make stdin, stdout point to the comms pipe
	
	// prepare the argv for the tool (prepend the "myself" element)
	// note how this overwrites a known-existing argv element (that we copied earlier)
	*(--restOfArguments) = pathToTool;
	
	secdebug("authtramp", "trampoline executes %s", pathToTool);
	Syslog::notice("executing %s", pathToTool);
	execv(pathToTool, (char *const *)restOfArguments);
	secdebug("authexec", "exec(%s) failed (errno=%d)", pathToTool, errno);
	
	// report failure
	OSStatus error = h2n(OSStatus(errAuthorizationToolExecuteFailure));
	write(notify, &error, sizeof(error));
	exit(1);
}


void fail(OSStatus cause)
{
	OSStatus tmp = h2n(cause);
	write(1, &tmp, sizeof(tmp));	// ignore error - can't do anything if error
	secdebug("authtramp", "trampoline aborting with status %ld", cause);
	exit(1);
}
