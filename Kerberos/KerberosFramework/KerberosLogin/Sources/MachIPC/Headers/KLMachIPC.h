#ifndef Login_MachIPC_h
#define Login_MachIPC_h

#pragma once

#include <Kerberos/KerberosLogin.h>
#include <Kerberos/mach_client_utilities.h>

#define kKLMachIPCTimeout	200
#define kMachIPCRetryCount	3

// This code implements a safe wrapper around calls to IPC functions. 
// 
// The client tries kMachIPCRetryCount times to send the IPC (in case the server
// was quitting when we were called) and handles the case when KLCancelAllDialogs was
// called and the server was killed (so we don't retry then)
//
// We call KLIPCGetServerPID to get the pid of the server so that KLCancelAllDialogs can
// kill the server if it needs to.
//
// We also always use KLIPCGetServerPID to get a security token to check the uid of the server
// This is very important because otherwise a malicious server running as another user could trick
// us into giving it information (such as our password!)

#define SafeIPCCallBegin_(ipcErr, result)                                                                           \
    {                                                                                                               \
        security_token_t token;                                                                                     \
        u_int32_t retriesLeft = kMachIPCRetryCount;                                                                 \
        gServerKilled = false;                                                                                      \
        mach_port_t machPort = MACH_PORT_NULL;                                                                      \
        char *name = NULL;                                                                                          \
        char *path = NULL;                                                                                          \
        KLIPCInString applicationName = NULL;                                                                       \
        mach_msg_type_number_t applicationNameLength = 0;                                                           \
        KLIPCInString applicationIconPath = NULL;                                                                   \
        mach_msg_type_number_t applicationIconPathLength = 0;                                                       \
                                                                                                                    \
        if (!__KLIsKerberosApp ()) {                                                                                \
            if (__KLGetApplicationNameString (&name) == klNoErr) {                                                  \
                applicationName = name;                                                                             \
                applicationNameLength = strlen (applicationName) + 1;                                               \
            }                                                                                                       \
                                                                                                                    \
            if (__KLGetApplicationIconPathString (&path) == klNoErr) {                                              \
                applicationIconPath = path;                                                                         \
                applicationIconPathLength = strlen (applicationIconPath) + 1;                                       \
            }                                                                                                       \
        }                                                                                                           \
                                                                                                                    \
        ipcErr = mach_client_lookup_and_launch_server (LoginMachIPCServiceName,                                     \
                                                       NULL,                                                        \
                                                       "/System/Library/Frameworks/Kerberos.framework/Servers",     \
                                                       "KerberosLoginServer.app",                                   \
                                                       &machPort);                                                  \
                                                                                                                    \
        if (ipcErr == BOOTSTRAP_SUCCESS) {                                                                          \
            do {                                                                                                    \
                ipcErr = KLIPCGetServerPID (machPort, &gServerPID, &token);                                         \
                if (ipcErr == KERN_SUCCESS) {                                                                       \
                    if (!mach_client_allow_server (token)) {                                                        \
                        result = klServerInsecureErr;                                                               \
                        break;                                                                                      \
                    }
    
    #define SafeIPCCallEnd_(ipcErr, result)                                                                         \
                }                                                                                                   \
                retriesLeft--;                                                                                      \
            } while ((ipcErr != KERN_SUCCESS) && (retriesLeft > 0) && !gServerKilled);                              \
        }                                                                                                           \
                                                                                                                    \
        if (gServerKilled) {                                                                                        \
            ipcErr = KERN_SUCCESS;                                                                                  \
            result = klUserCanceledErr;                                                                             \
        }                                                                                                           \
                                                                                                                    \
        if (machPort != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), machPort); }                     \
        if (name != NULL) { KLDisposeString (name); }                                                               \
        if (path != NULL) { KLDisposeString (path); }                                                               \
    }

/*#define __AfterRcvRpc(num, name)					\
    if (OutP->outResult != 0) { dprintf ("Handled IPC request %d (%s) --> %d\n", num, name, OutP->outResult); }
#define __AfterRcvRpc(num, name)					\
    dprintf ("Handled IPC request %d (%s) --> %ld\n", num, name, OutP->outResult); */
/*#define __BeforeRcvRpc(num, name)					\
    dprintf ("Handling IPC request %d (%s)\n", num, name);*/

#define __BeforeSendRpc(num, name)                                                                    \
    {


#define __AfterSendRpc(num, name)                                                                     \
        for (;;) {                                                                                    \
            mach_msg_option_t options = MACH_RCV_MSG|MACH_MSG_OPTION_NONE|MACH_RCV_TIMEOUT;           \
            mach_msg_size_t send_size = 0;                                                            \
            if (strcmp (name, "GetServerPID") == 0) {                                                 \
                options |= MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER)|                        \
                           MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0);                          \
            }                                                                                         \
            if (msg_result == MACH_SEND_TIMED_OUT) {                                                  \
                options |= MACH_SEND_MSG|MACH_SEND_TIMEOUT;                                           \
                send_size = sizeof(Request);                                                          \
            } else if (msg_result != MACH_RCV_TIMED_OUT) {                                            \
                break;                                                                                \
            }                                                                                         \
            __KLCallIdleCallback ();                                                                  \
            msg_result = mach_msg(&InP->Head, options,                                                \
                                  send_size, sizeof(Reply),                                           \
                                  InP->Head.msgh_reply_port,                                          \
                                  kKLMachIPCTimeout, MACH_PORT_NULL);                                 \
        }                                                                                             \
    }

#define LoginMachIPCServiceName	"KerberosLoginServer"

typedef const char*                     KLIPCInString;
typedef char*                           KLIPCOutString;

typedef int32_t*                        KLIPCInIntArray;

typedef const char*                     KLIPCInPrincipal;
typedef char*                           KLIPCOutPrincipal;

typedef const char*                     KLIPCInBuffer;
typedef char*                           KLIPCOutBuffer;

typedef int32_t                         KLIPCStatus;
typedef u_int32_t                       KLIPCKerberosVersion;
typedef u_int32_t                       KLIPCBoolean;
typedef u_int32_t                       KLIPCTime;
typedef u_int32_t                       KLIPCIndex;
typedef u_int32_t                       KLIPCSize;
typedef u_int32_t                       KLIPCDialogIdentifier;

typedef int32_t                         KLIPCPid;
typedef int32_t                         KLIPCFlags;
typedef const char*                     KLIPCData;


#endif /* Login_MachIPC_h */
