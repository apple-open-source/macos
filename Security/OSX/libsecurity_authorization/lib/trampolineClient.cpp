/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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
#include <Security/AuthorizationPriv.h>
#include <Security/SecBase.h>
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


OSStatus AuthorizationExecuteWithPrivileges(AuthorizationRef authorization,
                                            const char *pathToTool,
                                            AuthorizationFlags flags,
                                            char *const *arguments,
                                            FILE **communicationsPipe)
{
	// externalize the authorization
	AuthorizationExternalForm extForm;
	if (OSStatus err = AuthorizationMakeExternalForm(authorization, &extForm))
		return err;
    
    return AuthorizationExecuteWithPrivilegesExternalForm(&extForm, pathToTool, flags, arguments, communicationsPipe);
}

//
// The public client API function.
//
OSStatus AuthorizationExecuteWithPrivilegesExternalForm(const AuthorizationExternalForm * extForm,
	const char *pathToTool,
	AuthorizationFlags flags,
	char *const *arguments,
	FILE **communicationsPipe)
{
	if (extForm == NULL)
        return errAuthorizationInvalidPointer;
    
	// report the caller to the authorities
	aslmsg m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", "com.apple.libsecurity_authorization.AuthorizationExecuteWithPrivileges");
	asl_set(m, "com.apple.message.signature", getprogname());
	asl_log(NULL, m, ASL_LEVEL_NOTICE, "AuthorizationExecuteWithPrivileges!");
	asl_free(m);    

	// flags are currently reserved
	if (flags != 0)
		return errAuthorizationInvalidFlags;

	// compute the argument vector here because we can't allocate memory once we fork.

	// where is the trampoline?
#if defined(NDEBUG)
	const char *trampoline = TRAMPOLINE;
#else //!NDEBUG
	const char *trampoline = getenv("AUTHORIZATIONTRAMPOLINE");
	if (!trampoline)
		trampoline = TRAMPOLINE;
#endif //NDEBUG

	// make a data exchange pipe
	int dataPipe[2];
	if (pipe(dataPipe)) {
		secinfo("authexec", "data pipe failure");
		return errAuthorizationToolExecuteFailure;
	}

	// make text representation of the pipe handle
	char pipeFdText[20];
	snprintf(pipeFdText, sizeof(pipeFdText), "auth %d", dataPipe[READ]);
	const char **argv = argVector(trampoline, pathToTool, pipeFdText, arguments);
	
	// make a notifier pipe
	int notify[2];
	if (pipe(notify)) {
		close(dataPipe[READ]); close(dataPipe[WRITE]);
        if(argv) {
			free(argv);
		}
		secinfo("authexec", "notify pipe failure");
		return errAuthorizationToolExecuteFailure;
    }

	// make the communications pipe if requested
	int comm[2];
	if (communicationsPipe && socketpair(AF_UNIX, SOCK_STREAM, 0, comm)) {
		close(notify[READ]); close(notify[WRITE]);
		close(dataPipe[READ]); close(dataPipe[WRITE]);
        if(argv) {
			free(argv);
		}
		secinfo("authexec", "comm pipe failure");
		return errAuthorizationToolExecuteFailure;
	}

	OSStatus status = errSecSuccess;

	// do the standard forking tango...
	int delay = 1;
	for (int n = 5;; n--, delay *= 2) {
		switch (fork()) {
		case -1:	// error
			if (errno == EAGAIN) {
				// potentially recoverable resource shortage
				if (n > 0) {
					secinfo("authexec", "resource shortage (EAGAIN), delaying %d seconds", delay);
					sleep(delay);
					continue;
				}
			}
			secinfo("authexec", "fork failed (errno=%d)", errno);
			close(notify[READ]); close(notify[WRITE]);
			status = errAuthorizationToolExecuteFailure;
            goto exit_point;

		default: {	// parent
			// close foreign side of pipes
			close(notify[WRITE]);
			if (communicationsPipe)
				close(comm[WRITE]);

			close(dataPipe[READ]);
			if (write(dataPipe[WRITE], extForm, sizeof(*extForm)) != sizeof(*extForm)) {
				secinfo("authexec", "fwrite data failed (errno=%d)", errno);
				status = errAuthorizationInternal;
				close(notify[READ]);
				close(dataPipe[WRITE]);
				if (communicationsPipe) {
					close(comm[READ]);
					close(comm[WRITE]);
				}
				goto exit_point;
			}
			close(dataPipe[WRITE]);
			// get status notification from child
			secinfo("authexec", "parent waiting for status");
			ssize_t rc = read(notify[READ], &status, sizeof(status));
			status = n2h(status);
			switch (rc) {
			default:				// weird result of read: post error
				secinfo("authexec", "unexpected read return value %ld", long(rc));
				status = errAuthorizationToolEnvironmentError;
				// fall through
			case sizeof(status):	// read succeeded: child reported an error
				secinfo("authexec", "parent received status=%d", (int)status);
				close(notify[READ]);
				close(dataPipe[WRITE]);
				if (communicationsPipe) {
					close(comm[READ]);
					close(comm[WRITE]);
				}
				goto exit_point;
			case 0:					// end of file: exec succeeded
				close(notify[READ]);
				close(dataPipe[WRITE]);
				if (communicationsPipe)
					*communicationsPipe = fdopen(comm[READ], "r+");
				secinfo("authexec", "parent resumes (no error)");
				status = errSecSuccess;
				goto exit_point;
			}
        }
		
		case 0:		// child
			// close foreign side of pipes
			close(notify[READ]);
			if (communicationsPipe)
				close(comm[READ]);

			// close write end of the data PIPE
			close(dataPipe[WRITE]);

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
			
			// okay, execute the trampoline
			if (argv)
				execv(trampoline, (char *const*)argv);

			// execute failed - tell the parent
			{
				// in case of failure, close read end of the data pipe as well
				close(dataPipe[WRITE]);
				close(dataPipe[READ]);
				OSStatus error = errAuthorizationToolExecuteFailure;
				error = h2n(error);
				write(1, &error, sizeof(error));
				_exit(1);
			}
		}
	}

exit_point:
	free(argv);
	return status;
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
