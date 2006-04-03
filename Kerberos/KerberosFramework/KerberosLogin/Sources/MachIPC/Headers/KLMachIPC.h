#ifndef Login_MachIPC_h
#define Login_MachIPC_h

#pragma once

#include <Kerberos/KerberosLogin.h>
#include <Kerberos/mach_client_utilities.h>

#define kKerberosAgentBundleID "edu.mit.Kerberos.KerberosAgent"
#define kKerberosAgentPath "/System/Library/CoreServices/KerberosAgent.app/Contents/MacOS/KerberosAgent"

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

#define SafeIPCCallBegin_(ipcErr, result)                                                                           \
    {                                                                                                               \
        u_int32_t retriesLeft = kMachIPCRetryCount;                                                                 \
        gServerKilled = false;                                                                                      \
        mach_port_t machPort = MACH_PORT_NULL;                                                                      \
        char *path = NULL;                                                                                          \
        KLIPCInString applicationPath = NULL;                                                                       \
        mach_msg_type_number_t applicationPathLength = 0;                                                           \
        task_t applicationTask = mach_task_self ();                                                                 \
                                                                                                                    \
        if (__KLGetApplicationPathString (&path) == klNoErr) {                                                      \
            applicationPath = path;                                                                                 \
            applicationPathLength = strlen (applicationPath) + 1;                                                   \
        }                                                                                                           \
                                                                                                                    \
        ipcErr = mach_client_lookup_and_launch_server (kKerberosAgentBundleID, kKerberosAgentPath, &machPort);      \
                                                                                                                    \
        if (ipcErr == BOOTSTRAP_SUCCESS) {                                                                          \
            do {                                                                                                    \
                ipcErr = KLIPCGetServerPID (machPort, &gServerPID);                                                 \
                if (ipcErr == KERN_SUCCESS) {                                                                       
                    
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
        if (path     != NULL)           { KLDisposeString (path); }                                                 \
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
            mach_port_t save_reply_port = InP->Head.msgh_reply_port;                                  \
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
            InP->Head.msgh_reply_port = save_reply_port;                                              \
        }                                                                                             \
    }

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
