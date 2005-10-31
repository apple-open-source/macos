#ifndef CCache_MachIPC_h
#define CCache_MachIPC_h

#pragma once

#include <syslog.h>
#include <mach/mach_error.h>
#include <Kerberos/com_err.h>

#define kCCacheServerBundleID "edu.mit.Kerberos.CCacheServer"
#define kCCacheServerPath     "/System/Library/CoreServices/CCacheServer.app/Contents/MacOS/CCacheServer"

#define __AfterRcvRpc(num, name)														\
    if (OutP->outResult != 0) { dprintf ("Handled IPC request %d (%s) --> %d\n", num, name, OutP->outResult); }
/*#define __BeforeRcvRpc(num, name)														\
    dprintf ("Handling IPC request %d (%s)\n", num, name);
#define __AfterSendRpc(num, name)														\
    dprintf ("Sent IPC request %d (%s) to %x\n", num, name, inServerPort);
#define __BeforeSendRpc(num, name)														\
    dprintf ("Sending IPC request %d (%s) to %x\n", num, name, inServerPort);*/

#define ThrowIfIPCError_(err, result) 												\
    do {																			\
        if (err != KERN_SUCCESS) {													\
            dprintf ("%s() got IPC error %d '%s' (%s:%d)", \
                     __FUNCTION__, err, mach_error_string (err), __FILE__, __LINE__);                          \
            InvalidatePort ();														\
            CCIDebugThrow_ (CCIException (ccErrServerUnavailable));					\
        } else if (result != ccNoError) {											\
            dprintf ("%s() got CCAPI result %d '%s' (%s:%d)", \
                     __FUNCTION__, result, error_message (result), __FILE__, __LINE__);                          \
            CCIDebugThrow_ (CCIException (result));									\
        }																			\
    } while (false)
    
#define ThrowIfIPCAllocateFailed_(pointer, err)				\
    do {								\
        if (err != KERN_SUCCESS) {					\
            dprintf ("%s(): VM allocation failed with error %d '%s' (%s:%d)", \
                __FUNCTION__, err, mach_error_string (err), __FILE__, __LINE__);				\
            throw (CCIException (ccErrNoMem));				\
        }								\
    } while (false)

#define CatchForIPCReturn_(err)						\
    catch (CCIException& e) {						\
        dprintf ("%s(): caught CCIException, returning error %d (%s:%d)", \
                 __FUNCTION__, e.Error (), __FILE__, __LINE__);	\
        *err = e.Error ();						\
    } catch (...) {							\
        dprintf ("%s(): uncaught exception, returning ccErrBadParam (%s:%d)", \
                    __FUNCTION__, __FILE__, __LINE__);	\
        *err = ccErrBadParam;						\
    }

typedef		pid_t				CCIPID;

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
