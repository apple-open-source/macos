/*
 * mach_server_utilities.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Sources/mach_server_utilities.c,v 1.12 2005/06/15 20:59:15 lxs Exp $
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

#include <Kerberos/mach_server_utilities.h>
#include <Kerberos/mach_client_utilities.h>
#include "notifyServer.h"

// Global variables for servers (used by demux)
static mach_port_t  gBootPort = MACH_PORT_NULL;
static mach_port_t  gServicePort = MACH_PORT_NULL;
static mach_port_t  gNotifyPort = MACH_PORT_NULL;
static mach_port_t  gServerPortSet = MACH_PORT_NULL;
static boolean_t    gReadyToQuit = FALSE;
static boolean_t  (*gServerDemuxProc)(mach_msg_header_t *, mach_msg_header_t *);
static char         gServiceName [kServiceNameMaxLength];

#pragma mark -

// ---------------------------------------------------------------------------

mach_port_t
mach_server_get_server_port ()
{
    return gServicePort;
}

#pragma mark -

// ---------------------------------------------------------------------------

static kern_return_t
mach_server_setup_ports (void)
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t   previousNotifyPort = MACH_PORT_NULL;

    // Create the port set that the server will listen on
    if (!err) {
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_PORT_SET, &gServerPortSet);
    }    
    
    // Add the service port to the port set
    if (!err) {
        err = mach_port_move_member (mach_task_self (), gServicePort, gServerPortSet);
    }    
    
    // Create the notification port:
    if (!err) {
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &gNotifyPort);
    }    
    
    // Ask for notification when the server port has no more senders
    // A send-once right != a send right so our send-once right will not interfere with the notification
    if (!err) {
        err = mach_port_request_notification (mach_task_self (), gServicePort, MACH_NOTIFY_NO_SENDERS, true, 
                                              gNotifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotifyPort);
        dprintf ("requesting notification for no senders of %lx returned '%s', err = %ld\n",
                 gServicePort, mach_error_string (err), err);
    }
    
    // Add the notify port to the port set
    if (!err) {
        err = mach_port_move_member (mach_task_self (), gNotifyPort, gServerPortSet);
    }
            
    return KerberosIPCError_ (err);
}    


// ---------------------------------------------------------------------------

static kern_return_t
mach_server_cleanup_ports (void)
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t   previousNotifyPort = MACH_PORT_NULL;
    
    // Regardless of whether there was an error, unregister ourselves from no senders notifications 
    // so we don't get launched again by the notification message when we quit
    // A send-once right != a send right so our send-once right will not interfere with the notification
    if (gServicePort != MACH_PORT_NULL) {
        err = mach_port_request_notification (mach_task_self (), gServicePort, MACH_NOTIFY_NO_SENDERS, 
                                              true, MACH_PORT_NULL, MACH_MSG_TYPE_MAKE_SEND_ONCE, 
                                              &previousNotifyPort);
        dprintf ("removing notification for no senders of %lx returned '%s', err = %ld\n", 
                 previousNotifyPort, mach_error_string (err), err);
    }
    
    // free allocated port sets
    if (gNotifyPort != MACH_PORT_NULL) { 
        mach_port_deallocate (mach_task_self (), gNotifyPort); 
        gNotifyPort = MACH_PORT_NULL; 
    }
    
    if (gServerPortSet != MACH_PORT_NULL) { 
        mach_port_deallocate (mach_task_self (), gServerPortSet); 
        gServerPortSet = MACH_PORT_NULL; 
    }
    
    return KerberosIPCError_ (err);
}

// ---------------------------------------------------------------------------

kern_return_t
mach_server_run_server (boolean_t (*inDemuxProc)(mach_msg_header_t *, mach_msg_header_t *))
{
    kern_return_t err = KERN_SUCCESS;
    boolean_t     active = false;
        
    
    // Shed root privileges if any
    if (!err && (geteuid () == 0)) {
        uid_t newUID = LoginSessionGetSecurityAgentUID ();
        if (setuid (newUID) < 0) {
            dprintf ("%s: setuid(%d) failed (euid is %d)", __FUNCTION__, newUID, geteuid ());
        }
    }
    
    // Set up the globals so the demux can find them
    if (!err) {
        gServerDemuxProc = inDemuxProc;
        
        err = ENOMEM;  // Assume failure
        CFBundleRef mainBundle = CFBundleGetMainBundle ();
        if (mainBundle != NULL) {
            CFStringRef mainBundleID = CFBundleGetIdentifier (mainBundle);
            if (mainBundleID != NULL) {
                if (CFStringGetCString (mainBundleID, gServiceName, kServiceNameMaxLength, 
                                        kCFStringEncodingASCII) == TRUE) {
                    err = KERN_SUCCESS;
                }
            }
        }
    }
    
    // Get the bootstrap port
    if (!err) {
        err = task_get_bootstrap_port (mach_task_self (), &gBootPort);
        dprintf ("task_get_bootstrap_port(): port is %lx (err = %ld '%s')", 
                 gBootPort, err, mach_error_string (err));
    }
    
    if (!err) {
        // Is the service registered?
        err = bootstrap_status (gBootPort, gServiceName, &active);
        dprintf ("bootstrap_status(%ld, '%s'): returned state %d (err = %ld '%s')", 
                 gBootPort, gServiceName, active, err, mach_error_string (err));
        if (!err && (active == BOOTSTRAP_STATUS_ACTIVE))   { err = BOOTSTRAP_NAME_IN_USE; }
        if (!err && (active == BOOTSTRAP_STATUS_INACTIVE)) { err = BOOTSTRAP_UNKNOWN_SERVICE; } 
    }
    
    if (!err) {
        // Yes.  We are an on-demand server so our port already exists.  Just ask for it.
        err = bootstrap_check_in (gBootPort, (char *) gServiceName, &gServicePort);
        dprintf ("bootstrap_check_in('%s'): port is %ld (err = %ld '%s')", 
                 gServiceName, gServicePort, err, mach_error_string (err));
    }      
    
    if (!err) {
        if (bootstrap_status (gBootPort, gServiceName, &active) == KERN_SUCCESS) {
            dprintf ("'%s' state is now %ld", gServiceName, active);
        }
    }
        
    // Create the port set that the server will listen on
    if (!err) {
        err = mach_server_setup_ports ();
    }    
    
    if (!err) {
        dprintf ("\"%s\": starting up. ServicePort = %lx, BootstrapPort = %lx\n", 
                 gServiceName, gServicePort, gBootPort);
    }
    
    while (!err && !gReadyToQuit) {
        // Handle one message at a time so we can check to see if the server wants to quit
        err = mach_msg_server_once (mach_server_demux, kMachIPCMaxMsgSize, gServerPortSet, MACH_MSG_OPTION_NONE);
    }
        
    // Clean up the port set that the server will listen on
    mach_server_cleanup_ports ();
    
    return KerberosIPCError_ (err);    
}

#pragma mark -

// ---------------------------------------------------------------------------

boolean_t
mach_server_quit_self ()
{
    // Do not unregister our port because then we won't get automatically launched again.
    dprintf ("mach_server_quit_self(): quitting...");
    gReadyToQuit = true;
    return gReadyToQuit;
}

#pragma mark -

// ---------------------------------------------------------------------------

boolean_t 
mach_server_demux (mach_msg_header_t *request, mach_msg_header_t *reply) 
{
    if (mach_notify_server (request, reply) != false) {
        return true;
    } else {
        return gServerDemuxProc (request, reply);
    }
    return false;
}

#pragma mark -

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_port_deleted (mach_port_t notify, mach_port_name_t name)
{
    dprintf ("%s: Received MACH_NOTIFY_PORT_DELETED... quitting self\n", gServiceName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_port_destroyed (mach_port_t notify, mach_port_t rights)
{
    dprintf ("%s: Received MACH_NOTIFY_PORT_DESTROYED... quitting self\n", gServiceName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_no_senders (mach_port_t notify, mach_port_mscount_t mscount)
{
    dprintf ("%s: Received MACH_NOTIFY_NO_SENDERS... quitting self\n", gServiceName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_send_once (mach_port_t notify)
{
    dprintf ("%s: Received MACH_NOTIFY_SEND_ONCE\n", gServiceName);
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_dead_name (mach_port_t notify, mach_port_name_t name)
{
    dprintf ("%s: Received MACH_NOTIFY_DEAD_NAME... quitting self\n", gServiceName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

