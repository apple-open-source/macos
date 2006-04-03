#include <Kerberos/mach_client_utilities.h>
#include <Kerberos/CredentialsCacheInternal.h>
#include "CCache.MachIPC.h"

#include "MachIPCStub.h"

extern "C" {
    #include "CCacheIPC.h"
};

cc_int32 __CredentialsCacheInternalTellCCacheServerToQuit (void) 
{
    CCIResult		result = ccNoError;
    kern_return_t	err;
    CCIMachIPCStub      stub;
    mach_port_t 	servicePort = stub.GetPortNoLaunch ();
    
    if (servicePort != MACH_PORT_NULL) {
        // The server exists.  Tell it to quit, then remove our reference to it:
        err = InternalIPC_TellServerToQuit (servicePort, &result);
        if (err != KERN_SUCCESS) {
            return ccErrServerUnavailable;
        }
    }
    
    // Server wasn't launched to begin with
    return result;
}

#warning Remove __CredentialsCacheInternalTellCCacheServerToBecomeUser
cc_int32 __CredentialsCacheInternalTellCCacheServerToBecomeUser (uid_t inNewUID)
{
    return ccNoError;
}

cc_int32 __CredentialsCacheInternalGetServerPID (pid_t  *outServerID)
{
    CCIResult		result = ccErrServerUnavailable;
    CCIMachIPCStub      stub;
    mach_port_t 	servicePort = stub.GetPortNoLaunch ();

    if (servicePort != MACH_PORT_NULL) {
        kern_return_t err = InternalIPC_GetServerPID (servicePort, outServerID, &result);
        if (err != KERN_SUCCESS) {
            result = ccErrServerUnavailable;
        }
    }
    
    return result;
}

