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

#warning "Remove __CredentialsCacheInternalTellCCacheServerToBecomeUser"
cc_int32 __CredentialsCacheInternalTellCCacheServerToBecomeUser (uid_t inNewUID)
{
    return ccNoError;
}

#ifdef Classic_Ticket_Sharing
cc_int32 __CredentialsCacheInternalGetDiffs (
        cc_uint32		inServerID,
	cc_uint32		inSeqNo,
	Handle			ioHandle)
{
	CCIMachIPCStub		stub;
	
    mach_port_t 	servicePort = stub.GetPort ();

	CCIResult	result = ccNoError;
	
	char*					data = NULL;
	mach_msg_type_number_t	dataSize = 0;
    kern_return_t err = InternalIPC_GetDiffs (servicePort, inServerID, inSeqNo, &data, &dataSize, &result);
	if (err == KERN_SUCCESS) {
	
		if (result == noErr) {
			OSErr osErr = PtrAndHand (data, ioHandle, dataSize);
                        (void) vm_deallocate (mach_task_self (), (vm_offset_t) data, dataSize);

			
			if (osErr != noErr) {
				result = ccErrNoMem;
			}
		}
        } else {
		result = ccErrServerUnavailable;
	}
	
	return result;
}

cc_int32 __CredentialsCacheInternalGetInitialDiffs (
	Handle			ioHandle,
        CCIUInt32		inServerID)
{
	CCIMachIPCStub		stub;
	
    mach_port_t 	servicePort = stub.GetPort ();

	CCIResult	result = ccNoError;
	
	char*			data = NULL;
	mach_msg_type_number_t	dataSize = 0;
    kern_return_t err = InternalIPC_FabricateInitialDiffs (servicePort, inServerID, &data, &dataSize, &result);
	if (err == KERN_SUCCESS) {
	
		if (result == noErr) {
			OSErr osErr = PtrAndHand (data, ioHandle, dataSize);
                        (void) vm_deallocate (mach_task_self (), (vm_offset_t) data, dataSize);
			
			if (osErr != noErr) {
				result = ccErrNoMem;
			}
		}
    } else {
		result = ccErrServerUnavailable;
	}
	
	return result;
}

cc_int32 __CredentialsCacheInternalCheckServerID (
        CCIUInt32		inServerID,
        CCIUInt32*		outCorrect)
{
	CCIMachIPCStub		stub;
	
        mach_port_t 	servicePort = stub.GetPort ();

	CCIResult	result = ccNoError;
	
        kern_return_t err = InternalIPC_CheckServerID (servicePort, inServerID, outCorrect, &result);
	if (err != KERN_SUCCESS) {
		result = ccErrServerUnavailable;
	}
	
	return result;
}
#endif // Classic_Ticket_Sharing

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

