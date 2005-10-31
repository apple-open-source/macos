#include "MachIPCStub.h"
#include "CCache.MachIPC.h"

extern "C" {
#include "CCacheIPC.h"
};

#include <Kerberos/mach_client_utilities.h>

pid_t CCIMachIPCStub::sLastServerPID = -1;
CCIChangeTimeStub CCIMachIPCStub::sServerStateChangedTime;

CCIMachIPCStub::CCIMachIPCStub () : mPort (MACH_PORT_NULL)
{
    // Removed OS check because we ship with the OS
    // The check was causing problems in B&I during configure checks
    // for Kerberized packages.
}

CCIMachIPCStub::~CCIMachIPCStub ()
{
    if (mPort != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self(), mPort); }
}

mach_port_t 
CCIMachIPCStub::GetPort () const
{ 
    if (mPort == MACH_PORT_NULL) {
        kern_return_t err = KERN_SUCCESS;
        mach_port_t   port = MACH_PORT_NULL;
        
        // Haven't tried to talk to the server yet. lookup or launch it.
        err = mach_client_lookup_and_launch_server (kCCacheServerBundleID, kCCacheServerPath, &port);
        
        if (!err) {
            mPort = port;
            port = MACH_PORT_NULL;
        }
        
        if (port != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self(), port); }
    }

    UpdateServerPortState (mPort);
    //dprintf ("CCIMachIPCStub::GetPort () returning server port %ld", mPort);
    return mPort; 
} 

mach_port_t 
CCIMachIPCStub::GetPortNoLaunch () const
{
    if (mPort == MACH_PORT_NULL) {
        kern_return_t err = KERN_SUCCESS;
        mach_port_t   port = MACH_PORT_NULL;
        
        // Haven't tried to talk to the server yet. lookup or launch it.
        err = mach_client_lookup_server (kCCacheServerBundleID, &port);
        
        if (!err) {
            mPort = port;
            port = MACH_PORT_NULL;
        }
        
        if (port != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self(), port); }
    }
    
    UpdateServerPortState (mPort);
    //dprintf ("CCIMachIPCStub::GetPortNoLaunch () returning server port %ld", mPort);
    return mPort; 
}

void
CCIMachIPCStub::InvalidatePort () const
{
    if (mPort != MACH_PORT_NULL) { 
        mach_port_deallocate (mach_task_self(), mPort); 
        mPort = MACH_PORT_NULL;
    }
    UpdateServerPortState (mPort);
}

pid_t
CCIMachIPCStub::GetServerPID (mach_port_t serverPort)
{
    pid_t serverPID = -1;
    if (serverPort != MACH_PORT_NULL) {
        cc_int32 result = ccNoError;
        kern_return_t err = InternalIPC_GetServerPID (serverPort, &serverPID, &result);
        if (err) { serverPID = -1; }
    }
    
    return serverPID;
}

void 
CCIMachIPCStub::UpdateServerPortState (mach_port_t newServerPort) 
{ 
    pid_t newServerPID = GetServerPID (newServerPort);
    
    if (sLastServerPID != newServerPID) {
        if (newServerPID < 0) {
            // The server quit.  Update change time.
            sServerStateChangedTime.UpdateWhenServerDies ();
        }
        sLastServerPID = newServerPID;
    }
}

CCITime 
CCIMachIPCStub::GetServerStateChangedTime ()
{ 
    return sServerStateChangedTime.Get ();
}

void 
CCIMachIPCStub::UpdateStateChangedTimeFromServer (CCITime newTime)
{ 
    sServerStateChangedTime.UpdateFromServer (newTime);
}

pid_t
CCIMachIPCStub::GetLastServerPID ()
{ 
    return sLastServerPID; 
}
