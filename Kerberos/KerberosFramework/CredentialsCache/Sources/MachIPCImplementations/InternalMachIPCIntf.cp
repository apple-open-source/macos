#include <Kerberos/mach_server_utilities.h>

#include "CCache.MachIPC.h"
#include "MachIPCInterface.h"

extern "C" {
    #include "CCacheIPCServer.h"
};

// Function to quit the CCache Server for the login window plugin:
kern_return_t  InternalIPC_TellServerToQuit (
	mach_port_t inServerPort,
	CCIResult *outResult) 
{
    dprintf ("InternalIPC_TellServerToQuit: quitting server.\n");
    mach_server_quit_self ();
     
    *outResult = ccNoError;  
    return KERN_SUCCESS;
}

kern_return_t	InternalIPC_GetServerPID (mach_port_t inServerPort,
                                          CCIPID*     outPID,
                                          CCIResult*  outResult)
{
    try {
        *outPID = getpid ();
        *outResult = ccNoError;
    } CatchForIPCReturn_ (outResult)
    
    return KERN_SUCCESS;
}
