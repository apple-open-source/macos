/*
 * mach_server_utilities.c
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Sources/mach_server_utilities.c,v 1.4 2003/07/18 18:58:15 lxs Exp $
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
#include "notifyServer.h"


// Global variables for servers (used by demux)
static mach_port_t  gBootPort;
static mach_port_t  gServerPort;
static boolean_t    gReadyToQuit = false;
static boolean_t  (*gServerDemuxProc)(mach_msg_header_t *, mach_msg_header_t *);
static char         gServerName [kServiceNameMaxLength];
static uid_t        gServerUID = 0;

static boolean_t 
mach_server_allow_client (mach_msg_header_t *request);

#pragma mark -

// ---------------------------------------------------------------------------

mach_port_t
mach_server_get_server_port ()
{
    return gServerPort;
}

#pragma mark -

// ---------------------------------------------------------------------------

kern_return_t
mach_server_init (const char *inServiceNamePrefix, boolean_t (*inDemuxProc)(mach_msg_header_t *, mach_msg_header_t *))
{
    kern_return_t err = KERN_SUCCESS;
    boolean_t     active = false;
    const char   *serviceName = LoginSessionGetServiceName (inServiceNamePrefix);
    
    // Set up the globals so the demux can find them
    gServerDemuxProc = inDemuxProc;
    gServerUID = LoginSessionGetSessionUID ();

    strncpy (gServerName, serviceName, kServiceNameMaxLength);
    gServerName [kServiceNameMaxLength - 1] = '\0';
    
    // Get the bootstrap port
    if (err == KERN_SUCCESS) {
        err = task_get_bootstrap_port (mach_task_self (), &gBootPort);
    }

    // Does our service exist already?
    if (err == KERN_SUCCESS) {
        err = bootstrap_status (gBootPort, gServerName, &active);
    }
    
    if (err == BOOTSTRAP_UNKNOWN_SERVICE) {
        err = KERN_SUCCESS;
    }
    
    // Create the server port
    if (err == KERN_SUCCESS) {
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &gServerPort);
    }
    
    if (err == KERN_SUCCESS) {
        err = mach_port_insert_right (mach_task_self (), gServerPort, gServerPort, MACH_MSG_TYPE_MAKE_SEND);
    }
    
    // Ask for notification when the bootstrap server becomes a dead name (Puma)
    if (err == KERN_SUCCESS) {
        mach_port_t previousNotifyPort;
        err = mach_port_request_notification (mach_task_self (), gBootPort, MACH_NOTIFY_DEAD_NAME,
                                              true, gServerPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotifyPort);
        dprintf ("requesting notification for dead name of %lx returned '%s', err = %ld\n",
                    gBootPort, mach_error_string (err), err);
    }

    // Ask for notification when the server port has no more senders (Jaguar?)
    // A send-once right != a send right so our send-once right will not interfere with the notification
    if (err == KERN_SUCCESS) {
        mach_port_t previousNotifyPort;
        err = mach_port_request_notification (mach_task_self (), gServerPort, MACH_NOTIFY_NO_SENDERS,
                                              true, gServerPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotifyPort);
        dprintf ("requesting notification for no senders of %lx returned '%s', err = %ld\n",
                    gServerPort, mach_error_string (err), err);
    }

    // Register the server port
    if (err == KERN_SUCCESS) {
        err = bootstrap_register (gBootPort, gServerName, gServerPort);
    }
    
    // The bootstrap server copied our server rights, so decrement the right
    if (err == KERN_SUCCESS) {
        err = mach_port_mod_refs (mach_task_self (), gServerPort, MACH_PORT_RIGHT_SEND, -1);
        dprintf ("mach_port_mod_refs (MACH_PORT_RIGHT_SEND, -1) on %lx returned '%s', err = %ld\n",
                    gServerPort, mach_error_string (err), err);
    }
     
    return err;   
}

// ---------------------------------------------------------------------------

kern_return_t
mach_server_run_server ()
{
    kern_return_t err = KERN_SUCCESS;
    
    if (err == KERN_SUCCESS) {
        dprintf ("\"%s\": entering main server loop. ServerPort = %lx, BootstrapPort = %lx\n", 
                    gServerName, gServerPort, gBootPort);
        // ask for MACH_RCV_TRAILER_SENDER so we can check the uid in the mach_msg_security_trailer_t
        err = mach_msg_server (mach_server_demux, kMachIPCMaxMsgSize, gServerPort,
                                MACH_RCV_TRAILER_ELEMENTS (MACH_RCV_TRAILER_SENDER));
        if (gReadyToQuit) {
            err = KERN_SUCCESS;
        } else {
            dprintf ("%s: mach_msg_server: returned '%s', err = %ld\n", gServerName, 
                        mach_error_string (err), err);
        }
    }
 
    return err;   

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
                        gServerName, clientUID, gServerUID);
            return true;
        } else {
            dprintf ("%s: client uid %ld not allowed to connect to server uid %ld\n", 
                        gServerName, clientUID, gServerUID);
        }
    } else {
        dprintf ("%s: Invalid trailer type %ld (should be %ld), length %ld (should be %ld)\n", 
                    gServerName, 
                    trailer->msgh_trailer_type, MACH_MSG_TRAILER_FORMAT_0,
                    trailer->msgh_trailer_size, MACH_MSG_TRAILER_FORMAT_0_SIZE);
    }
    
    return false;
}

// ---------------------------------------------------------------------------

boolean_t
mach_server_quit_self ()
{
    kern_return_t err = KERN_SUCCESS;
    
    dprintf ("%s: attempting to destroy server port %lx.\n", gServerName, gServerPort);
    err = mach_port_destroy (mach_task_self (), gServerPort);
    if (err == KERN_SUCCESS) {
        gReadyToQuit = true;
    } else {
        dprintf ("%s: failed to destroy server port %lx (err = %ld) '%s'\n", 
                gServerName, gServerPort, err, mach_error_string (err));
        
    }
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


// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_port_deleted (mach_port_t notify, mach_port_name_t name)
{
    dprintf ("%s: Received MACH_NOTIFY_PORT_DELETED... quitting self\n", gServerName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_port_destroyed (mach_port_t notify, mach_port_t rights)
{
    dprintf ("%s: Received MACH_NOTIFY_PORT_DESTROYED... quitting self\n", gServerName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_no_senders (mach_port_t notify, mach_port_mscount_t mscount)
{
    dprintf ("%s: Received MACH_NOTIFY_NO_SENDERS... quitting self\n", gServerName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_send_once (mach_port_t notify)
{
    dprintf ("%s: Received MACH_NOTIFY_SEND_ONCE\n", gServerName);
    return KERN_SUCCESS;
}

// ---------------------------------------------------------------------------

kern_return_t 
do_mach_notify_dead_name (mach_port_t notify, mach_port_name_t name)
{
    dprintf ("%s: Received MACH_NOTIFY_DEAD_NAME... quitting self\n", gServerName);
    mach_server_quit_self ();
    return KERN_SUCCESS;
}
