/*
 * mach_server_utilities.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Sources/mach_server_utilities.c,v 1.9 2004/11/30 22:56:09 lxs Exp $
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

#include <Kerberos/mach_server_utilities.h>
#include <Kerberos/mach_client_utilities.h>
#include "notifyServer.h"

// Global variables for servers (used by demux)
static mach_port_t  gBootPort = MACH_PORT_NULL;
static mach_port_t  gServicePort = MACH_PORT_NULL;
static mach_port_t  gNotifyPort = MACH_PORT_NULL;
static mach_port_t  gServerPortSet = MACH_PORT_NULL;
static boolean_t    gReadyToQuit = false;
static boolean_t  (*gServerDemuxProc)(mach_msg_header_t *, mach_msg_header_t *);
static char         gServiceName [kServiceNameMaxLength];
static uid_t        gServerUID = 0;

static boolean_t 
mach_server_allow_client (mach_msg_header_t *request);

#pragma mark -

// ---------------------------------------------------------------------------

mach_port_t
mach_server_get_server_port ()
{
    return gServicePort;
}

#pragma mark -

// ---------------------------------------------------------------------------

kern_return_t
mach_server_run_server (boolean_t (*inDemuxProc)(mach_msg_header_t *, mach_msg_header_t *))
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t   previousNotifyPort = MACH_PORT_NULL;
    boolean_t     active = false;
        
    // Set up the globals so the demux can find them
    gServerDemuxProc = inDemuxProc;
    gServerUID = LoginSessionGetSessionUID ();
    
    if (err == KERN_SUCCESS) {
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
    if (err == KERN_SUCCESS) {
        err = task_get_bootstrap_port (mach_task_self (), &gBootPort);
        dprintf ("task_get_bootstrap_port(): port is %lx (err = %ld '%s')", 
                 gBootPort, err, mach_error_string (err));
    }
    
    // Does our service exist already?
    if (err == KERN_SUCCESS) {
        if (bootstrap_status (gBootPort, gServiceName, &active) == KERN_SUCCESS) {
            dprintf ("'%s' state is %ld", gServiceName, active);
        }
    }
    
    if (err == KERN_SUCCESS) {   
        // We are an on-demand server so our port already exists.  Just ask for it.
        err = bootstrap_check_in (gBootPort, (char *) gServiceName, &gServicePort);
        dprintf ("bootstrap_check_in('%s'): port is %ld (err = %ld '%s')", 
                 gServiceName, gServicePort, err, mach_error_string (err));
    }
    
    if (err == KERN_SUCCESS) {
        if (bootstrap_status (gBootPort, gServiceName, &active) == KERN_SUCCESS) {
            dprintf ("'%s' state is now %ld", gServiceName, active);
        }
    }
    
    // Create the port set that the server will listen on
    if (err == KERN_SUCCESS) {
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_PORT_SET, &gServerPortSet);
    }    
    
    // Add the service port to the port set
    if (err == KERN_SUCCESS) {
        err = mach_port_move_member (mach_task_self (), gServicePort, gServerPortSet);
    }    
    
    // Create the notification port:
    if (err == KERN_SUCCESS) {
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &gNotifyPort);
    }    
    
    // Ask for notification when the server port has no more senders
    // A send-once right != a send right so our send-once right will not interfere with the notification
    if (err == KERN_SUCCESS) {
        err = mach_port_request_notification (mach_task_self (), gServicePort, MACH_NOTIFY_NO_SENDERS, true, 
                                              gNotifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotifyPort);
        dprintf ("requesting notification for no senders of %lx returned '%s', err = %ld\n",
                 gServicePort, mach_error_string (err), err);
    }
    
    // Add the notify port to the port set
    if (err == KERN_SUCCESS) {
        err = mach_port_move_member (mach_task_self (), gNotifyPort, gServerPortSet);
    }    
    
    if (err == KERN_SUCCESS) {
        dprintf ("\"%s\": starting up. ServicePort = %lx, BootstrapPort = %lx\n", 
                 gServiceName, gServicePort, gBootPort);
    }
    
    while ((err == KERN_SUCCESS) && !gReadyToQuit) {
        // Handle one message at a time so we can check to see if the server wants to quit
        // ask for MACH_RCV_TRAILER_SENDER so we can check the uid in the mach_msg_security_trailer_t
        err = mach_msg_server_once (mach_server_demux, kMachIPCMaxMsgSize, gServerPortSet,
                                    MACH_RCV_TRAILER_ELEMENTS (MACH_RCV_TRAILER_SENDER));
    }
    
    // Regardless of whether there was an error, unregister ourselves from no senders notifications 
    // so we don't get launchen again by the notification message when we quit
    // A send-once right != a send right so our send-once right will not interfere with the notification
    if (gServicePort != MACH_PORT_NULL) {
        kern_return_t terr = mach_port_request_notification (mach_task_self (), gServicePort, MACH_NOTIFY_NO_SENDERS, 
                                                             true, MACH_PORT_NULL, MACH_MSG_TYPE_MAKE_SEND_ONCE, 
                                                             &previousNotifyPort);
        dprintf ("removing notification for no senders of %lx returned '%s', err = %ld\n", 
                 previousNotifyPort, mach_error_string (err), err);
        if (!err) { err = terr; }
    }
    
    // free allocated port sets
    if (gNotifyPort    != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), gNotifyPort); }
    if (gServerPortSet != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), gServerPortSet); }
    
    return KerberosIPCError_ (err);    
}

#pragma mark -

// ---------------------------------------------------------------------------

static boolean_t
mach_server_allow_client (mach_msg_header_t *request)
{
    // Following the received data, at the next natural boundary, is a message trailer. 
    // The msgh_size field of the received message does not include the length of this trailer; 
    // the trailers length is given by the msgh_trailer_size field within the trailer. 
    mach_msg_security_trailer_t *trailer = NULL;
    trailer = (mach_msg_security_trailer_t *) round_msg (((char *)request) + request->msgh_size);
    
    if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) && 
        (trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {
        // The trailer contains security information... we will allow it if it's valid
        uid_t clientUID = trailer->msgh_sender.val[0];
        if (clientUID == gServerUID) {
            return true;
        } else if (clientUID == 0) {
            dprintf ("%s: WARNING, seteuid root client approved for server uid %ld\n", 
                        gServiceName, clientUID, gServerUID);
            return true;
        } else {
            dprintf ("%s: client uid %ld not allowed to connect to server uid %ld\n", 
                        gServiceName, clientUID, gServerUID);
        }
    } else {
        dprintf ("%s: Invalid trailer type %ld (should be %ld), length %ld (should be %ld)\n", 
                    gServiceName, 
                    trailer->msgh_trailer_type, MACH_MSG_TRAILER_FORMAT_0,
                    trailer->msgh_trailer_size, MACH_MSG_TRAILER_FORMAT_0_SIZE);
    }
    
    return false;
}

// ---------------------------------------------------------------------------

boolean_t
mach_server_quit_self ()
{
    // Do not unregister our port because then we won't get automatically launched again.
    dprintf ("mach_server_quit_self(): quitting...");
    gReadyToQuit = true;
    return gReadyToQuit;
}

// ---------------------------------------------------------------------------

boolean_t
mach_server_become_user (uid_t inNewServerUID) 
{
    int err = 0;
    
    dprintf ("Entering mach_server_become_user (%ld): euid = %ld, ruid = %ld\n", 
                inNewServerUID, geteuid (), getuid ());

    if ((geteuid () != 0) && (getuid () != 0) && (geteuid () != inNewServerUID) && (getuid () != inNewServerUID)) {
        dprintf ("Insufficient priviledges to become uid %ld\n", inNewServerUID);
        err = EPERM;
    }
    
    // We want to call setuid (inNewServerUID), but in order for that to succeed, 
    // either euid == 0 or euid == inNewServerUID, so we swap if necessary...
    if ((geteuid () != 0) && (geteuid () != inNewServerUID)) {
        err = setreuid (geteuid (), getuid ());
        dprintf ("setreuid (%ld, %ld) returned %ld '%s', (euid = %ld, ruid = %ld)\n",
                    inNewServerUID, getuid (), err, strerror (errno), geteuid(), getuid());
    }
    
    if (!err) {
        err = setuid (inNewServerUID);
        dprintf ("setuid (%ld) returned %ld '%s', (euid = %ld, ruid = %ld)\n",
                    inNewServerUID, err, strerror (errno), geteuid(), getuid());
    }
    
    if (!err) {
        // Record our new name and uid
        gServerUID = inNewServerUID;
    }

    if (!err) {
        dprintf ("mach_server_become_user succeeded in becoming uid '%ld' (euid = %ld, ruid = %ld)\n", 
                    inNewServerUID, geteuid(), getuid());
        return true;
    } else {
        dprintf ("mach_server_become_user: failed to become '%ld' (euid = %ld, ruid = %ld)\n",
                     inNewServerUID, geteuid(), getuid());
        return false;
    }
}

#pragma mark -

// ---------------------------------------------------------------------------

boolean_t 
mach_server_demux (mach_msg_header_t *request, mach_msg_header_t *reply) 
{
    if (mach_notify_server (request, reply) != false) {
        return true;
    } else {
        // Verify that the sender is either root or the user whose creds we are storing
        if (mach_server_allow_client (request)) {
            return gServerDemuxProc (request, reply);
        }
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

