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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <Security/SecBase.h>
#include <security_utilities/endian.h>
#include <security_utilities/debugging.h>

#include "Security/Authorization.h"
#include "AuthorizationPriv.h"
#include "AuthorizationTrampolinePriv.h"
#include <dispatch/semaphore.h>
#include <unistd.h>
#include <os/log.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <os/log.h>

//
// A few names for clarity's sake
//
enum {
    READ = 0,        // read end of standard UNIX pipe
    WRITE = 1        // write end of standard UNIX pipe
};

static os_log_t AUTH_LOG_DEFAULT() {
    static dispatch_once_t once;
    static os_log_t log;
    dispatch_once(&once, ^{ log = os_log_create("com.apple.Authorization", "Trampoline"); });
    return log;
};

#define AUTH_LOG AUTH_LOG_DEFAULT()

//
// Where is the trampoline itself?
//
#if !defined(TRAMPOLINE)
# define TRAMPOLINE "/usr/libexec/security_authtrampoline" /* fallback */
#endif


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
    os_log(AUTH_LOG, "AuthorizationExecuteWithPrivileges and AuthorizationExecuteWithPrivilegesExternalForm are deprecated and functionality will be removed soon - please update your application");
    if (extForm == NULL)
        return errAuthorizationInvalidPointer;
    
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
        os_log_error(AUTH_LOG, "data pipe failure");
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
        os_log_error(AUTH_LOG, "notify pipe failure");
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
        os_log_error(AUTH_LOG, "comm pipe failure");
        return errAuthorizationToolExecuteFailure;
    }
    
    OSStatus status = errSecSuccess;
    
    // do the standard forking tango...
    int delay = 1;
    for (int n = 5;; n--, delay *= 2) {
        switch (fork()) {
            case -1:    // error
                if (errno == EAGAIN) {
                    // potentially recoverable resource shortage
                    if (n > 0) {
                        os_log(AUTH_LOG, "resource shortage (EAGAIN), delaying %d seconds", delay);
                        sleep(delay);
                        continue;
                    }
                }
                os_log_error(AUTH_LOG, "fork failed (errno=%d)", errno);
                close(notify[READ]); close(notify[WRITE]);
                status = errAuthorizationToolExecuteFailure;
                goto exit_point;
                
            default: {    // parent
                // close foreign side of pipes
                close(notify[WRITE]);
                if (communicationsPipe)
                    close(comm[WRITE]);
                
                close(dataPipe[READ]);
                if (write(dataPipe[WRITE], extForm, sizeof(*extForm)) != sizeof(*extForm)) {
                    os_log_error(AUTH_LOG, "fwrite data failed (errno=%d)", errno);
                    status = errAuthorizationInternal;
                    close(notify[READ]);
                    close(dataPipe[WRITE]);
                    if (communicationsPipe) {
                        close(comm[READ]);
                    }
                    goto exit_point;
                }
                // get status notification from child
                os_log_debug(AUTH_LOG, "parent waiting for status");
                ssize_t rc = read(notify[READ], &status, sizeof(status));
                status = n2h(status);
                switch (rc) {
                    default:                // weird result of read: post error
                        os_log_error(AUTH_LOG, "unexpected read return value %ld", long(rc));
                        status = errAuthorizationToolEnvironmentError;
                        // fall through
                    case sizeof(status):    // read succeeded: child reported an error
                        os_log_error(AUTH_LOG, "parent received status=%d", (int)status);
                        close(notify[READ]);
                        close(dataPipe[WRITE]);
                        if (communicationsPipe) {
                            close(comm[READ]);
                            close(comm[WRITE]);
                        }
                        goto exit_point;
                    case 0:                    // end of file: exec succeeded
                        close(notify[READ]);
                        close(dataPipe[WRITE]);
                        if (communicationsPipe)
                            *communicationsPipe = fdopen(comm[READ], "r+");
                        os_log_debug(AUTH_LOG, "parent resumes (no error)");
                        status = errSecSuccess;
                        goto exit_point;
                }
            }
                
            case 0:        // child
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



OSStatus AuthorizationExecuteWithPrivilegesInternal(const AuthorizationRef authorization,
                                                    const char * _Nonnull pathToTool,
                                                    const char * _Nonnull const * arguments,
                                                    pid_t * newProcessPid,
                                                    const uid_t uid,
                                                    int stdOut,
                                                    int stdErr,
                                                    int stdIn,
                                                    void(^processFinished)(const int exitStatus))
{
    // externalize the authorization
    AuthorizationExternalForm extForm;
    if (OSStatus err = AuthorizationMakeExternalForm(authorization, &extForm))
        return err;
    
    return AuthorizationExecuteWithPrivilegesExternalFormInternal(&extForm, pathToTool, arguments, newProcessPid, uid, stdOut, stdErr, stdIn, processFinished);
}

OSStatus AuthorizationExecuteWithPrivilegesExternalFormInternal(const AuthorizationExternalForm *extAuthorization,
                                                                const char * _Nonnull pathToTool,
                                                                const char * _Nullable const * _Nullable arguments,
                                                                pid_t * newProcessPid,
                                                                const uid_t uid,
                                                                int stdOut,
                                                                int stdErr,
                                                                int stdIn,
                                                                void(^processFinished)(const int exitStatus))
{
    xpc_object_t message;
    __block OSStatus retval = errAuthorizationInternal;
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    if (!sema) {
        os_log_error(AUTH_LOG, "Unable to create trampoline semaphore");
        return retval;
    }
    __block xpc_connection_t trampolineConnection = xpc_connection_create_mach_service("com.apple.security.authtrampoline", NULL, XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);
    
    if (!trampolineConnection) {
        os_log_error(AUTH_LOG, "Unable to create trampoline mach service");
        dispatch_release(sema);
        return retval;
    }
    
    xpc_connection_set_event_handler(trampolineConnection, ^(xpc_object_t event) {
        xpc_type_t type = xpc_get_type(event);
        
        if (type == XPC_TYPE_ERROR) {
            if (trampolineConnection) {
                xpc_release(trampolineConnection);
                trampolineConnection = NULL;
            }
            if (event == XPC_ERROR_CONNECTION_INTERRUPTED && processFinished) {
                os_log_error(AUTH_LOG, "Connection with trampoline was interruped");
                processFinished(134); // simulate killed by SIGABRT
            }
        } else {
            const char *requestId = xpc_dictionary_get_string(event, XPC_REQUEST_ID);
            if (requestId && strncmp(XPC_EVENT_MSG, requestId, strlen(XPC_EVENT_MSG)) == 0) {
                const char *eventType = xpc_dictionary_get_string(event, XPC_EVENT_TYPE);
                if (eventType && strncmp(XPC_EVENT_TYPE_CHILDEND, eventType, strlen(XPC_EVENT_TYPE_CHILDEND)) == 0) {
                    int exitStatus = (int)xpc_dictionary_get_int64(event, RETVAL_STATUS);
                    os_log_debug(AUTH_LOG, "Child process ended with exit status %d", exitStatus);
                    
                    if (trampolineConnection) {
                        xpc_connection_cancel(trampolineConnection);
                        xpc_release(trampolineConnection);
                        trampolineConnection = NULL;
                    }
                    if (processFinished) {
                        processFinished(exitStatus);
                    };
                } else {
                    os_log_error(AUTH_LOG, "Unknown event type [%s] arrived from trampoline", eventType);
                }
            } else {
                os_log_error(AUTH_LOG, "Unknown request [%s] arrived from trampoline", requestId);
            }
        }
    });
    
    xpc_connection_resume(trampolineConnection);
    
    message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(message, XPC_REQUEST_ID, XPC_REQUEST_CREATE_PROCESS);
    
    Boolean waitForEndNeeded = (processFinished != NULL);
    if (stdIn >= 0) {
        xpc_object_t xpcInFd = xpc_fd_create(stdIn);
        if (!xpcInFd) {
            os_log_error(AUTH_LOG, "Unable to create XPC stdin FD");
            goto finish;
        }
        xpc_dictionary_set_value(message, PARAM_STDIN, xpcInFd);
        xpc_release(xpcInFd);
        waitForEndNeeded = true;
    }
    
    if (stdOut >= 0) {
        xpc_object_t xpcOutFd = xpc_fd_create(stdOut);
        if (!xpcOutFd) {
            os_log_error(AUTH_LOG, "Unable to create XPC stdout FD");
            goto finish;
        }
        xpc_dictionary_set_value(message, PARAM_STDOUT, xpcOutFd);
        xpc_release(xpcOutFd);
        waitForEndNeeded = true;
    }

    if (stdErr >= 0) {
        xpc_object_t xpcErrFd = xpc_fd_create(stdErr);
        if (!xpcErrFd) {
            os_log_error(AUTH_LOG, "Unable to create XPC stderr FD");
            goto finish;
        }
        xpc_dictionary_set_value(message, PARAM_STDERR, xpcErrFd);
        xpc_release(xpcErrFd);
        waitForEndNeeded = true;
    }

    extern char** environ;

    if (environ) {
        xpc_object_t envArray = xpc_array_create(NULL, 0);
        char **ptr = environ;

        while (*ptr) {
            xpc_object_t xpcString = xpc_string_create(*ptr++);
            xpc_array_append_value(envArray, xpcString);
            xpc_release(xpcString);
        }
        xpc_dictionary_set_value(message, PARAM_ENV, envArray);
        xpc_release(envArray);
    }

    xpc_dictionary_set_string(message, PARAM_TOOL_PATH, pathToTool);
    xpc_dictionary_set_uint64(message, PARAM_EUID, uid);
    {
        const char *cwd = getcwd(NULL, 0);
        if (cwd) {
            xpc_dictionary_set_string(message, PARAM_CWD, cwd);
        }
    }
    xpc_dictionary_set_bool(message, PARAM_CHILDEND_NEEDED, waitForEndNeeded);

    if (arguments) {
        xpc_object_t paramsArray = xpc_array_create(NULL, 0);
        int i = 0;
        while (arguments[i] != NULL) {
            xpc_object_t xpcString = xpc_string_create(arguments[i++]);
            xpc_array_append_value(paramsArray, xpcString);
            xpc_release(xpcString);
        }
        xpc_dictionary_set_value(message, PARAM_TOOL_PARAMS, paramsArray);
        xpc_release(paramsArray);
    }
    xpc_dictionary_set_data(message, PARAM_AUTHREF, extAuthorization, sizeof(*extAuthorization));
    
    retval = errAuthorizationToolExecuteFailure;
    if (trampolineConnection) {
        xpc_connection_send_message_with_reply(trampolineConnection, message, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^(xpc_object_t event) {
            xpc_type_t type = xpc_get_type(event);
            const char *requestId = xpc_dictionary_get_string(event, XPC_REQUEST_ID);
            if (type == XPC_TYPE_ERROR) {
                os_log_error(AUTH_LOG, "Error when trying to communicate with the trampoline");
            }
            else if (requestId && strncmp(XPC_REPLY_MSG, requestId, strlen(XPC_REPLY_MSG)) == 0) {
                retval = (OSStatus)xpc_dictionary_get_int64(event, RETVAL_STATUS);
                if (newProcessPid && retval == errAuthorizationSuccess) {
                    *newProcessPid = (OSStatus)xpc_dictionary_get_uint64(event, RETVAL_CHILD_PID);
                }
            } else {
                os_log_error(AUTH_LOG, "Trampoline returned invalid data");
            }
            dispatch_semaphore_signal(sema);
        });
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    } else {
        os_log_error(AUTH_LOG, "Unable to establish connection to the trampoline");
    }
    dispatch_release(sema);
    
finish:
    if (message) {
        xpc_release(message);
    }
    return retval;
}
