#include <Kerberos/mach_server_utilities.h>

#include "CCache.MachIPC.h"

extern boolean_t CCacheIPC_server (
    mach_msg_header_t*	inHeadP,
    mach_msg_header_t*	outHeadP);

int main (int argc, const char * argv[])
{
    kern_return_t	err = KERN_SUCCESS;
    
    syslog (LOG_INFO, "Starting up.");
    
    err = mach_server_init (CCacheMachIPCServiceName, CCacheIPC_server);
    if (err != KERN_SUCCESS) {
        syslog (LOG_NOTICE, "%s server failed to launch: %s (%d)", CCacheMachIPCServiceName, 
                    mach_error_string (err), err);
    }

    err = mach_server_run_server ();
    if (err != KERN_SUCCESS) {
        syslog (LOG_NOTICE, "%s server quitting: %s (%d)", CCacheMachIPCServiceName, mach_error_string (err), err);
    }

    return err;
}

                    