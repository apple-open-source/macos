#include "MachIPCStub.h"
#include "CCache.MachIPC.h"

#include <Kerberos/mach_client_utilities.h>

mach_port_t CCIMachIPCStub::sLastServerPort = MACH_PORT_NULL;
CCIChangeTimeStub CCIMachIPCStub::sServerStateChangedTime;

CCIMachIPCStub::CCIMachIPCStub () : mPort (NULL)
{
    SInt32	version;
    OSErr err = Gestalt (gestaltSystemVersion, &version);
    if ((err != noErr) || (version < 0x01012)) { 
        // Require Mac OS X 10.1.2
        CFOptionFlags	responseFlags;
        CFUserNotificationDisplayAlert (0, 0, NULL /* Icon */,
            NULL /* Sound */, NULL /* Localization */, 
            CFSTR ("Kerberos Error"), 
            CFSTR ("This version of Kerberos requires Mac OS X 10.1.2 or later. Please upgrade your computer."), 
            CFSTR ("OK"), NULL, NULL,  &responseFlags);
        throw CCIException (ccErrServerUnavailable);
    }
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
        mPort = new MachServerPort (CCacheMachIPCServiceName, NULL, 
                    "/System/Library/Frameworks/Kerberos.framework/Servers", 
                    "CCacheServer.app");
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
        mPort = new MachServerPort (CCacheMachIPCServiceName);
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
