/*
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
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

#include "k5_mig_server.h"

#include <syslog.h>
#include "k5_mig_requestServer.h"
#include "k5_mig_reply.h"

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <bsm/audit.h>
#include <servers/bootstrap.h>
#include <dispatch/dispatch.h>
#include <launch.h>
#include <string.h>

/* Map of receive rights to libdispatch sources. */
static CFMutableDictionaryRef mig_clients = NULL;
__unused static int _assert_mach_port_can_be_used_as_cfdictionary_key_
	[sizeof(mach_port_t) <= sizeof(void *) ? 0 : -1];

/* ------------------------------------------------------------------------ */

static boolean_t k5_ipc_request_demux (mach_msg_header_t *request, 
                                       mach_msg_header_t *reply) 
{
    boolean_t handled = 0;
    
    if (!handled) {
        handled = k5_ipc_request_server (request, reply);
    }
    
    /* Check here for a client death.  If so remove it */
    if (!handled && request->msgh_id == MACH_NOTIFY_NO_SENDERS) {
        kern_return_t err = KERN_SUCCESS;
        
        err = k5_ipc_server_remove_client (request->msgh_local_port);
        
        if (!err) {
            void *key = (void *)((uintptr_t)request->msgh_local_port);
            dispatch_source_t source = (dispatch_source_t)CFDictionaryGetValue
                                        (mig_clients, key);
            CFDictionaryRemoveValue (mig_clients, key);
            dispatch_release (source);
            handled = 1;  /* was a port we are tracking */
        }
    }
     
    return handled;    
}

/* ------------------------------------------------------------------------ */

kern_return_t k5_ipc_server_create_client_connection (mach_port_t    in_server_port,
                                                      mach_port_t   *out_connection_port)
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t connection_port = MACH_PORT_NULL;
    mach_port_t old_notification_target = MACH_PORT_NULL;
    dispatch_source_attr_t attr = NULL;
    dispatch_source_t source = NULL;
    
    if (!err) {
        err = mach_port_allocate (mach_task_self (), 
                                  MACH_PORT_RIGHT_RECEIVE, &connection_port);
    }
    
    if (!err) {
        /* request no-senders notification so we can tell when client quits/crashes */
        err = mach_port_request_notification (mach_task_self (), 
                                              connection_port, 
                                              MACH_NOTIFY_NO_SENDERS, 1, 
                                              connection_port, 
                                              MACH_MSG_TYPE_MAKE_SEND_ONCE, 
                                              &old_notification_target );
    }
    
    if (!err) {
        err = k5_ipc_server_add_client (connection_port);
    }
    
    if (!err) {
	attr = dispatch_source_attr_create ();
	if (attr == NULL) {
	    err = KERN_FAILURE;
	}
    }

    if (!err) {
	dispatch_source_finalizer_t finalizer;
	finalizer = ^(dispatch_source_t s){
	    mach_port_mod_refs (mach_task_self (), connection_port, 
	                        MACH_PORT_RIGHT_RECEIVE, -1);
	};
	if (dispatch_source_attr_set_finalizer (attr, finalizer)) {
	    err = KERN_FAILURE;
	}
    }

    if (!err) {
	dispatch_queue_t queue;
	queue = dispatch_get_main_queue ();
	source = dispatch_source_mig_create (connection_port,
	                                     K5_IPC_MAX_MSG_SIZE, attr, queue,
	                                     k5_ipc_request_demux);
	if (source == NULL) {
	    err = KERN_FAILURE;
	}
    }

    if (!err) {
        CFDictionaryAddValue (mig_clients,
                              (void *)((uintptr_t)connection_port), source);
        *out_connection_port = connection_port;
        connection_port = MACH_PORT_NULL;
    }
    
    if (MACH_PORT_VALID (connection_port)) { mach_port_deallocate (mach_task_self (), connection_port); }

    if (attr != NULL) { dispatch_release (attr); }
    
    return err;    
}

/* ------------------------------------------------------------------------ */

kern_return_t k5_ipc_server_request (mach_port_t             in_connection_port,
				     audit_token_t	     remote_creds,
                                     mach_port_t             in_reply_port,
                                     k5_ipc_inl_request_t    in_inl_request,
                                     mach_msg_type_number_t  in_inl_requestCnt,
                                     k5_ipc_ool_request_t    in_ool_request,
                                     mach_msg_type_number_t  in_ool_requestCnt)
{
    kern_return_t err = KERN_SUCCESS;
    k5_ipc_stream request_stream = NULL;
    
    if (!err) {
        err = k5_ipc_stream_new (&request_stream);
    }
    
    if (!err) {
        if (in_inl_requestCnt) {
            err = k5_ipc_stream_write (request_stream, in_inl_request, in_inl_requestCnt);
            
        } else if (in_ool_requestCnt) {
            err = k5_ipc_stream_write (request_stream, in_ool_request, in_ool_requestCnt);
            
        } else {
            err = EINVAL;
        }
    }
    
    if (!err) {
        err = k5_ipc_server_handle_request (in_connection_port, remote_creds,
					    in_reply_port, request_stream);
    }
    
    k5_ipc_stream_release (request_stream);
    if (in_ool_requestCnt) { vm_deallocate (mach_task_self (), (vm_address_t) in_ool_request, in_ool_requestCnt); }
    
    return err;
}

/* ------------------------------------------------------------------------ */

static kern_return_t k5_ipc_server_get_lookup_and_service_names (char **out_lookup, 
                                                                 char **out_service)
{
    kern_return_t err = KERN_SUCCESS;
    CFBundleRef bundle = NULL;
    CFStringRef id_string = NULL;
    CFIndex len = 0;
    char *service_id = NULL;
    char *lookup = NULL;
    char *service = NULL;
    
    if (!out_lookup ) { err = EINVAL; }
    if (!out_service) { err = EINVAL; }
    
    if (!err) {
        bundle = CFBundleGetMainBundle ();
        if (!bundle) { err = ENOENT; }
    }
    
    if (!err) {
        id_string = CFBundleGetIdentifier (bundle);
        if (!id_string) { err = ENOMEM; }
    }
    
    if (!err) {
        len = CFStringGetMaximumSizeForEncoding (CFStringGetLength (id_string), 
                                                    kCFStringEncodingUTF8) + 1;
    }
    
    if (!err) {
        service_id = calloc (len, sizeof (char));
        if (!service_id) { err = errno; }
    }
    
    if (!err && !CFStringGetCString (id_string, service_id, len, 
                                     kCFStringEncodingUTF8)) { 
        err = ENOMEM;
    }

    if (!err) {
        int w = asprintf (&lookup, "%s%s", service_id, K5_MIG_LOOKUP_SUFFIX);
        if (w < 0) { err = ENOMEM; }
    }
    
    if (!err) {
        int w = asprintf (&service, "%s%s", service_id, K5_MIG_SERVICE_SUFFIX);
        if (w < 0) { err = ENOMEM; }
    }
    
    if (!err) {
        *out_lookup = lookup;
        lookup = NULL;
        *out_service = service;
        service = NULL;
    }
    
    free (service);
    free (lookup);
    free (service_id);
   
    return err;
}

static void
start_mach_service(launch_data_t obj, const char *key, void *context)
{
    dispatch_source_t source;
  
    if (launch_data_get_type(obj) == LAUNCH_DATA_MACHPORT) {
        mach_port_t port = launch_data_get_machport(obj);
	source = dispatch_source_mig_create(port,
					    K5_IPC_MAX_MSG_SIZE,
					    NULL,
					    dispatch_get_main_queue(),
					    k5_ipc_request_demux);
	if (source == NULL) {
	    syslog(LOG_NOTICE, "Failed to register Mach source.");
	}
    } else {
        syslog(LOG_NOTICE, "%s: not a mach port", key);
    }
}

#pragma mark -

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_listen (void)
{
    launch_data_t resp, tmp, msg;
	
    msg = launch_data_new_string(LAUNCH_KEY_CHECKIN);

    resp = launch_msg(msg);
    if (resp == NULL) {
	syslog(LOG_NOTICE, "launch_msg(): %s", strerror(errno));
	return 1;
    }
    
    if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO) {
	errno = launch_data_get_errno(resp);
	syslog(LOG_NOTICE, "launch_msg() response: %s", strerror(errno));
	return 1;
    }

    mig_clients = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);

    if (mig_clients == NULL) {
	syslog(LOG_NOTICE, "Failed to create client dictionary.");
	return 1;
    }
    
    tmp = launch_data_dict_lookup(resp, LAUNCH_JOBKEY_MACHSERVICES);
    
    if (tmp == NULL) {
	syslog(LOG_NOTICE, "No mach services found!");
    } else {
	launch_data_dict_iterate(tmp, start_mach_service, NULL);
    }
    return 0;
}

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_send_reply (mach_port_t   in_reply_port,
                                  k5_ipc_stream in_reply_stream)
{
    kern_return_t err = KERN_SUCCESS;
    k5_ipc_inl_reply_t inl_reply;
    mach_msg_type_number_t inl_reply_length = 0;
    k5_ipc_ool_reply_t ool_reply = NULL;
    mach_msg_type_number_t ool_reply_length = 0;
    
    if (!MACH_PORT_VALID (in_reply_port)) { err = EINVAL; }
    if (!in_reply_stream                ) { err = EINVAL; }
    
    if (!err) {
        /* depending on how big the message is, use the fast inline buffer or  
         * the slow dynamically allocated buffer */
        mach_msg_type_number_t reply_length = k5_ipc_stream_size (in_reply_stream);
        
        if (reply_length > K5_IPC_MAX_INL_MSG_SIZE) {            
            //dprintf ("%s choosing out of line buffer (size is %d)", 
            //                  __FUNCTION__, reply_length);
            
            err = vm_read (mach_task_self (), 
                           (vm_address_t) k5_ipc_stream_data (in_reply_stream), reply_length, 
                           (vm_address_t *) &ool_reply, &ool_reply_length);
            
        } else {
            //cci_debug_printf ("%s choosing in line buffer (size is %d)",
            //                  __FUNCTION__, reply_length);
            
            inl_reply_length = reply_length;
            memcpy (inl_reply, k5_ipc_stream_data (in_reply_stream), reply_length);
        }
    }
    
    if (!err) {
        err = k5_ipc_server_reply (in_reply_port, 
                                   inl_reply, inl_reply_length,
                                   ool_reply, ool_reply_length);
    }
    
    if (!err) {
        /* Because we use ",dealloc" ool_reply will be freed by mach. Don't double free it. */
        ool_reply = NULL;
        ool_reply_length = 0;
    }
    
    if (ool_reply_length) { vm_deallocate (mach_task_self (), (vm_address_t) ool_reply, ool_reply_length); }
    
    return err;
}

/* ------------------------------------------------------------------------ */

void k5_ipc_server_quit (void)
{
}
