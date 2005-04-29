#ifndef CCache_MachIPC_h
#define CCache_MachIPC_h

#pragma once

#include <syslog.h>
#include <mach/mach_error.h>

#define kCCacheServerBundleID "edu.mit.Kerberos.CCacheServer"
#define kCCacheServerPath     "/System/Library/CoreServices/CCacheServer.app/Contents/MacOS/CCacheServer"

#define __AfterRcvRpc(num, name)														\
    if (OutP->outResult != 0) { dprintf ("Handled IPC request %d (%s) --> %d\n", num, name, OutP->outResult); }
/*#define __BeforeRcvRpc(num, name)														\
    dprintf ("Handling IPC request %d (%s)\n", num, name);
#define __AfterSendRpc(num, name)														\
    dprintf ("Sent IPC request %d (%s) to %x\n", num, name, inServerPort);
#define __BeforeSendRpc(num, name)														\
    dprintf ("Sending IPC request %d (%s) to %x;\n" num, name, inServerPort);*/

#define ThrowIfIPCError_(err, result) 												\
    do {																			\
        if (err != KERN_SUCCESS) {													\
            InvalidatePort ();														\
            CCIDebugThrow_ (CCIException (ccErrServerUnavailable));					\
        } else if (result != ccNoError) {											\
            CCIDebugThrow_ (CCIException (result));									\
        }																			\
    } while (false)
    
#define ThrowIfIPCAllocateFailed_(pointer, err)				\
    do {								\
        if (err != KERN_SUCCESS) {					\
            syslog (LOG_DEBUG, "VM allocation failed with %s (%d)",	\
                mach_error_string (err), err);				\
            throw (CCIException (ccErrNoMem));				\
        }								\
    } while (false)

#define CatchForIPCReturn_(err)						\
    catch (CCIException& e) {						\
        syslog (LOG_DEBUG, "IPC returning error %d", e.Error ());	\
        *err = e.Error ();						\
    } catch (...) {							\
        CCISignal_ ("Uncaught exception, returning ccErrBadParam");	\
        *err = ccErrBadParam;						\
    }
    
typedef		CCITime				Time;
typedef		CCIObjectID			ContextID;
typedef		CCIObjectID			CCacheID;
typedef		CCIObjectID			CredentialsID;

typedef		CCacheID*			CCacheIDArray;
typedef		CredentialsID*			CredentialsIDArray;

/* Need separate in/out typenames to make constness happy */
typedef		const char*			CCacheInName;
typedef		const char*			CCacheInPrincipal;
typedef		char*				CCacheOutName;
typedef		char*				CCacheOutPrincipal;

typedef		const char*			FlattenedInCredentials;
typedef		char*				FlattenedOutCredentials;

typedef		char*				CCacheDiffs;


extern Boolean gDone;

#endif /* CCache_MachIPC_h */
