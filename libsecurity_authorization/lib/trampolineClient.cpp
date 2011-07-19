/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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


//
// trampolineClient - Authorization trampoline client-side implementation
//
#include <asl.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <Security/Authorization.h>
#include <security_utilities/endian.h>
#include <security_utilities/debugging.h>

//
// Where is the trampoline itself?
//
#if !defined(TRAMPOLINE)
# define TRAMPOLINE "/usr/libexec/security_authtrampoline" /* fallback */
#endif


//
// A few names for clarity's sake
//
enum {
	READ = 0,		// read end of standard UNIX pipe
	WRITE = 1		// write end of standard UNIX pipe
};


//
// Local (static) functions
//
static const char **argVector(const char *trampoline,
	const char *tool, const char *commFd,
	char *const *arguments);


//
// The public client API function.
//
OSStatus AuthorizationExecuteWithPrivileges(AuthorizationRef authorization,
	const char *pathToTool,
	AuthorizationFlags flags,
	char *const *arguments,
	FILE **communicationsPipe)
{
	// report the caller to the authorities
	aslmsg m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", "com.apple.libsecurity_authorization.AuthorizationExecuteWithPrivileges");
	asl_set(m, "com.apple.message.signature", getprogname());
	asl_log(NULL, m, ASL_LEVEL_NOTICE, "AuthorizationExecuteWithPrivileges!");
	asl_free(m);    

	// flags are currently reserved
	if (flags != 0)
		return errAuthorizationInvalidFlags;

	// externalize the authorization
	AuthorizationExternalForm extForm;
	if (OSStatus err = AuthorizationMakeExternalForm(authorization, &extForm))
		return err;

    // create the mailbox file
    FILE *mbox = tmpfile();
    if (!mbox)
        return errAuthorizationInternal;
    if (fwrite(&extForm, sizeof(extForm), 1, mbox) != 1) {
        fclose(mbox);
        return errAuthorizationInternal;
    }
    fflush(mbox);
    
    // make text representation of the temp-file descriptor
    char mboxFdText[20];
    snprintf(mboxFdText, sizeof(mboxFdText), "auth %d", fileno(mbox));
    
	// make a notifier pipe
	int notify[2];
	if (pipe(notify)) {
        fclose(mbox);
		return errAuthorizationToolExecuteFailure;
    }

	// make the communications pipe if requested
	int comm[2];
	if (communicationsPipe && socketpair(AF_UNIX, SOCK_STREAM, 0, comm)) {
		close(notify[READ]); close(notify[WRITE]);
        fclose(mbox);
		return errAuthorizationToolExecuteFailure;
	}

	// do the standard forking tango...
	int delay = 1;
	for (int n = 5;; n--, delay *= 2) {
		switch (fork()) {
		case -1:	// error
			if (errno == EAGAIN) {
				// potentially recoverable resource shortage
				if (n > 0) {
					secdebug("authexec", "resource shortage (EAGAIN), delaying %d seconds", delay);
					sleep(delay);
					continue;
				}
			}
			secdebug("authexec", "fork failed (errno=%d)", errno);
			close(notify[READ]); close(notify[WRITE]);
			return errAuthorizationToolExecuteFailure;

		default: {	// parent
			// close foreign side of pipes
			close(notify[WRITE]);
			if (communicationsPipe)
				close(comm[WRITE]);
                
            // close mailbox file (child has it open now)
            fclose(mbox);
			
			// get status notification from child
			OSStatus status;
			secdebug("authexec", "parent waiting for status");
			ssize_t rc = read(notify[READ], &status, sizeof(status));
			status = n2h(status);
			switch (rc) {
			default:				// weird result of read: post error
				secdebug("authexec", "unexpected read return value %ld", long(rc));
				status = errAuthorizationToolEnvironmentError;
				// fall through
			case sizeof(status):	// read succeeded: child reported an error
				secdebug("authexec", "parent received status=%d", (int)status);
				close(notify[READ]);
				if (communicationsPipe) { close(comm[READ]); close(comm[WRITE]); }
				return status;
			case 0:					// end of file: exec succeeded
				close(notify[READ]);
				if (communicationsPipe)
					*communicationsPipe = fdopen(comm[READ], "r+");
				secdebug("authexec", "parent resumes (no error)");
				return noErr;
			}
        }

		case 0:		// child
			// close foreign side of pipes
			close(notify[READ]);
			if (communicationsPipe)
				close(comm[READ]);
			
			// fd 1 (stdout) holds the notify write end
			dup2(notify[WRITE], 1);
			close(notify[WRITE]);
			
			// fd 0 (stdin) holds either the comm-link write-end or /dev/null
			if (communicationsPipe) {
				dup2(comm[WRITE], 0);
				close(comm[WRITE]);
			} else {
				close(0);
				open("/dev/null", O_RDWR);
			}
			
			// where is the trampoline?
#if defined(NDEBUG)
			const char *trampoline = TRAMPOLINE;
#else //!NDEBUG
			const char *trampoline = getenv("AUTHORIZATIONTRAMPOLINE");
			if (!trampoline)
				trampoline = TRAMPOLINE;
#endif //NDEBUG

			// okay, execute the trampoline
			secdebug("authexec", "child exec(%s:%s)",
				trampoline, pathToTool);
			if (const char **argv = argVector(trampoline, pathToTool, mboxFdText, arguments))
				execv(trampoline, (char *const*)argv);
			secdebug("authexec", "trampoline exec failed (errno=%d)", errno);

			// execute failed - tell the parent
			{
				OSStatus error = errAuthorizationToolExecuteFailure;
				error = h2n(error);
				write(1, &error, sizeof(error));
				_exit(1);
			}
		}
	}
}


//
// Build an argv vector
//
static const char **argVector(const char *trampoline, const char *pathToTool,
	const char *mboxFdText, char *const *arguments)
{
	int length = 0;
	if (arguments) {
		for (char *const *p = arguments; *p; p++)
			length++;
	}
	if (const char **args = (const char **)malloc(sizeof(const char *) * (length + 4))) {
		args[0] = trampoline;
		args[1] = pathToTool;
		args[2] = mboxFdText;
		if (arguments)
			for (int n = 0; arguments[n]; n++)
				args[n + 3] = arguments[n];
		args[length + 3] = NULL;
		return args;
	}
	return NULL;
}
