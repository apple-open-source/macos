#include "MachIPCStub.h"
#include "CCache.MachIPC.h"

#include <Kerberos/mach_client_utilities.h>

mach_port_t CCIMachIPCStub::sLastServerPort = MACH_PORT_NULL;
CCIChangeTimeStub CCIMachIPCStub::sServerStateChangedTime;

CCIMachIPCStub::CCIMachIPCStub () : mPort (NULL)
{
    // Removed OS check because we ship with the OS
    // The check was causing problems in B&I during configure checks
    // for Kerberized packages.
}

CCIMachIPCStub::~CCIMachIPCStub ()
{
    if (mPort != NULL) {
        delete mPort; 
    }
}

mach_port_t 
CCIMachIPCStub::GetPort () const
{ 
    mach_port_t serverPort;
    
    if (mPort == NULL) {
        // Haven't tried to talk to the server yet. lookup or launch it.
        mPort = new MachServerPort (kCCacheServerBundleID, kCCacheServerPath, TRUE);
    }
    serverPort = mPort->Get ();
    
    UpdateServerPortState (serverPort);
    return serverPort; 
} 

mach_port_t 
CCIMachIPCStub::GetPortNoLaunch () const
{
    mach_port_t serverPort;
    
    if (mPort != NULL) {
        serverPort = mPort->Get ();
    } else {
        // lookup but don't launch
        mPort = new MachServerPort (kCCacheServerBundleID, kCCacheServerPath, FALSE);
        if (mPort->Get () != MACH_PORT_NULL) {
            serverPort = mPort->Get ();
        } else {
            delete mPort; // port is no good
            mPort = NULL;
            serverPort = MACH_PORT_NULL;
        }
    }

    UpdateServerPortState (serverPort);
    return serverPort;
}

void
CCIMachIPCStub::InvalidatePort () const
{
    if (mPort != NULL) {
        delete mPort;
        mPort = NULL;
    }
    UpdateServerPortState (MACH_PORT_NULL);
}

void 
CCIMachIPCStub::UpdateServerPortState (mach_port_t newServerPort) 
{ 
    if ((sLastServerPort != newServerPort)) {
        if (newServerPort == MACH_PORT_NULL) {
            // We knew a server, but now it quit.  Update change time.
            sServerStateChangedTime.UpdateWhenServerDies ();
        }
        sLastServerPort = newServerPort;
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

mach_port_t
CCIMachIPCStub::GetLastServerPort ()
{ 
    return sLastServerPort; 
}
