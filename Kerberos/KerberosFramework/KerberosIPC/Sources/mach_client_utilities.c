/*
 * mach_client_utilities.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Sources/mach_client_utilities.c,v 1.16 2005/06/15 20:59:14 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/KerberosDebug.h>
#include <mach/mach.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>

#include <Kerberos/mach_client_utilities.h>


kern_return_t __KerberosIPCError (kern_return_t inError, const char *function, const char *file, int line)
{
    if ((inError != KERN_SUCCESS) && (ddebuglevel () > 0)) {
        dprintf ("%s() got %ld ('%s') at %s: %d", function, inError, mach_error_string (inError), file, line);
        dprintf ("Session is %s", LoginSessionGetSecuritySessionName ());
        //dprintbootstrap (mach_task_self ());
    }    
    return inError;
}

#pragma mark -

// ---------------------------------------------------------------------------

static kern_return_t
mach_client_check_server_registration (mach_port_t inBootstrapPort,
                                       const char *inServiceName,
                                       const char *inServerPath)
{
    kern_return_t err = KERN_SUCCESS;
    boolean_t     active = FALSE;
    
    if (inBootstrapPort == MACH_PORT_NULL) { err = EINVAL; }
    if (inServiceName   == NULL          ) { err = EINVAL; }
    if (inServerPath    == NULL          ) { err = EINVAL; }

    if (err == KERN_SUCCESS) {
        // Is the service registered?
        err = bootstrap_status (inBootstrapPort, (char *) inServiceName, &active);

        if ((err != KERN_SUCCESS) || (active == BOOTSTRAP_STATUS_INACTIVE)) {
            if (!LoginSessionIsRootSession ()) {   // Never register per-user servers in the root bootstrap
                mach_port_t   serverPrivPort = MACH_PORT_NULL;
                mach_port_t   servicePort = MACH_PORT_NULL;
                uid_t         serverUID = LoginSessionGetSessionUID ();
                if (serverUID == 0 /* root */) { serverUID = LoginSessionGetSecurityAgentUID (); } // don't be root
                
                // The service got unregistered or was never registered.  Register it!
                err = bootstrap_create_server (inBootstrapPort, (char *) inServerPath, serverUID, TRUE /* on demand */, &serverPrivPort);
                dprintf ("mach_client_check_server_registration(): bootstrap_create_server() returned err %ld", err);
                
                if (err == KERN_SUCCESS) {
                    // Set up the server to autolaunch
                    err = bootstrap_create_service (serverPrivPort, (char *) inServiceName, &servicePort);
                    dprintf ("mach_client_check_server_registration(): bootstrap_create_service() returned err %ld", err);
                }
                
                if (err == KERN_SUCCESS) {
                    dprintf ("mach_client_check_server_registration(): registered '%s' for service '%s' and uid '%ld'.", 
                             inServerPath, inServiceName, serverUID);
                }
                
                if (serverPrivPort != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), serverPrivPort); }
                if (servicePort    != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), servicePort); }
            }
        }
    }
    
    return KerberosIPCError_ (err);    
}

// ---------------------------------------------------------------------------

kern_return_t
mach_client_lookup_server (const char *inServiceName, 
                           mach_port_t *outServicePort) 
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t   bootstrapPort = MACH_PORT_NULL;
    boolean_t     active = FALSE;
        
    if (inServiceName  == NULL) { err = EINVAL; }
    if (outServicePort == NULL) { err = EINVAL; }
    
    if (err == KERN_SUCCESS) {
        // Get our bootstrap port
        err = task_get_bootstrap_port (mach_task_self (), &bootstrapPort);
    }

    if (err == KERN_SUCCESS) {
        // Is the service running?
        err = bootstrap_status (bootstrapPort, (char *) inServiceName, &active);
//        dprintf ("mcls: bootstrap_status('%s'): status is %ld (err = %ld '%s')", 
//                 inServiceName, active, err, mach_error_string (err));
    }
    
    if (err == KERN_SUCCESS) {
        if (active != BOOTSTRAP_STATUS_ACTIVE) { err = BOOTSTRAP_UNKNOWN_SERVICE; }
    }
    
    if (err == KERN_SUCCESS) {   
        // This will return a valid port even if the server isn't running
        // which is why we call bootstrap_status() above
        err = bootstrap_look_up (bootstrapPort, (char *) inServiceName, outServicePort);
//        dprintf ("mcls: bootstrap_look_up('%s'): port is %lx (err = %ld '%s')", 
//                 inServiceName, outServicePort, err, mach_error_string (err));
    }

    if (bootstrapPort != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), bootstrapPort); }
    
    if (err == BOOTSTRAP_UNKNOWN_SERVICE) {
        return err;  // Avoid spewing to the log file
    } else {
        return KerberosIPCError_ (err);
    }
}

// ---------------------------------------------------------------------------

kern_return_t
mach_client_lookup_and_launch_server (const char *inServiceName,
                                      const char *inServerPath,
                                      mach_port_t *outServicePort)
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t   bootstrapPort = MACH_PORT_NULL;
    
    if (inServiceName  == NULL) { err = EINVAL; }
    if (inServerPath   == NULL) { err = EINVAL; }
    if (outServicePort == NULL) { err = EINVAL; }
    
    if (err == KERN_SUCCESS) {
        // Get our bootstrap port
        err = task_get_bootstrap_port (mach_task_self (), &bootstrapPort);
    }
    
    if (err == KERN_SUCCESS) {
        // Make sure the service is registered
        err = mach_client_check_server_registration (bootstrapPort, inServiceName, inServerPath);
    }

    if (err == KERN_SUCCESS) {   
        // This will return a valid port even if the server isn't running
        err = bootstrap_look_up (bootstrapPort, (char *) inServiceName, outServicePort);
//        dprintf ("mclals: bootstrap_look_up('%s'): port is %lx (err = %ld '%s')", 
//                 inServiceName, outServicePort, err, mach_error_string (err));
    }
    
    if (bootstrapPort != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), bootstrapPort); }

    return KerberosIPCError_ (err);
}

#pragma mark -


