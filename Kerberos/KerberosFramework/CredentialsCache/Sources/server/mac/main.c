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

#include "cci_common.h"

#include <syslog.h>
#include <Kerberos/kipc_server.h>
#include "cci_migServer.h"
#include "ccs_server.h"

/* ------------------------------------------------------------------------ */

static kipc_boolean_t ccs_server_demux (mach_msg_header_t *request, 
                                        mach_msg_header_t *reply) 
{
    kipc_boolean_t handled = false;
    
    if (!handled && request->msgh_id == MACH_NOTIFY_NO_SENDERS) {
        kern_return_t err = KERN_SUCCESS;
        
        /* Check here for a client in our table and free rights associated with it */
        handled = 1;
        err = mach_port_mod_refs (mach_task_self (), request->msgh_local_port, 
                                  MACH_PORT_RIGHT_RECEIVE, -1);
        
        if (!err) {
            err = ccs_server_remove_client (request->msgh_local_port);
        }
        
        cci_check_error (err);
    }
    
    if (!handled) {
        handled = ccapi_server (request, reply);
    }
    
    return handled;    
}

/* ------------------------------------------------------------------------ */

int main (int argc, const char *argv[])
{
    cc_int32 err = 0;
    
    openlog (argv[0], LOG_CONS | LOG_PID, LOG_AUTH);
    syslog (LOG_INFO, "Starting up.");   
    
    if (!err) {
        err = ccs_server_initialize ();
    }
    
    if (!err) {
        err = kipc_server_run_server (ccs_server_demux);
    }
    
    /* cleanup ccs resources */
    ccs_server_cleanup ();
    
    syslog (LOG_NOTICE, "Exiting: %s (%d)", kipc_error_string (err), err);
    
    /* exit */
    return err ? 1 : 0;
}

/* ------------------------------------------------------------------------ */

kern_return_t ccs_mipc_create_client_connection (mach_port_t    in_server_port, 
                                                 mach_port_t   *out_connection_port)
{
    kern_return_t err = KERN_SUCCESS;
    mach_port_t connection_port = MACH_PORT_NULL;
    mach_port_t old_notification_target = MACH_PORT_NULL;
    
    if (!err) {
        err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &connection_port);
    }
    
    if (!err) {
        err = mach_port_move_member (mach_task_self (), connection_port, kipc_server_get_listen_portset ());
    }
    
    if (!err) {
        /* request no-senders notification so we can tell when client quits/crashes */
        err = mach_port_request_notification (mach_task_self (), connection_port, 
                                              MACH_NOTIFY_NO_SENDERS, 1, connection_port, 
                                              MACH_MSG_TYPE_MAKE_SEND_ONCE, &old_notification_target );
    }
    
    if (!err) {
        err = ccs_server_add_client (connection_port);
    }
    
    if (!err) {
        *out_connection_port = connection_port;
        connection_port = MACH_PORT_NULL;
    }
    
    if (connection_port != MACH_PORT_NULL) { mach_port_deallocate (mach_task_self (), connection_port); }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

kern_return_t ccs_mipc_handle_message (mach_port_t             in_connection_port,
                                       cci_mipc_inl_request_t  in_inl_request,
                                       mach_msg_type_number_t  in_inl_requestCnt,
                                       cci_mipc_ool_request_t  in_ool_request,
                                       mach_msg_type_number_t  in_ool_requestCnt,
                                       cci_mipc_inl_reply_t    out_inl_reply,
                                       mach_msg_type_number_t *out_inl_replyCnt,
                                       cci_mipc_ool_reply_t   *out_ool_reply,
                                       mach_msg_type_number_t *out_ool_replyCnt )
{
    kern_return_t err = KERN_SUCCESS;
    cci_stream_t request_stream = NULL;
    cci_stream_t reply_stream = NULL;
    
    if (!err) {
        err = cci_stream_new (&request_stream);
    }
    
    if (!err) {
        if (in_inl_requestCnt) {
            err = cci_stream_write (request_stream, in_inl_request, in_inl_requestCnt);
            
        } else if (in_ool_requestCnt) {
            err = cci_stream_write (request_stream, in_ool_request, in_ool_requestCnt);
            
        } else {
            err = cci_check_error (ccErrBadInternalMessage);
        }
    }
    
    if (!err) {
        err = ccs_server_ipc (request_stream, &reply_stream);
    }
    
    if (!err) {
        /* depending on how big the message is, use the fast inline buffer or  
        * the slow dynamically allocated buffer */
        mach_msg_type_number_t reply_length = cci_stream_size (reply_stream);
        
        if (reply_length > *out_inl_replyCnt) {
            cci_mipc_ool_reply_t ool_reply = NULL;
            mach_msg_type_number_t ool_reply_length = 0;    

            cci_debug_printf ("%s choosing out of line buffer (size is %d)", 
                              __FUNCTION__, reply_length);
            
            err = vm_read (mach_task_self (), 
                           (vm_address_t) cci_stream_data (reply_stream), reply_length, 
                           (vm_address_t *) &ool_reply, &ool_reply_length);
            
            if (!err) {
                *out_inl_replyCnt = 0;
                *out_ool_reply = ool_reply;
                *out_ool_replyCnt = ool_reply_length;
                
                /* Because we use ",dealloc" ool_reply will be freed by mach. Don't double free it. */
                ool_reply = NULL;
                ool_reply_length = 0;
            }
        
            if (ool_reply_length) { vm_deallocate (mach_task_self (), (vm_address_t) ool_reply, ool_reply_length); }

        } else {
            //cci_debug_printf ("%s choosing in line buffer (size is %d)",
            //                  __FUNCTION__, reply_length);
            
            *out_ool_replyCnt = 0;
            *out_inl_replyCnt = reply_length;
            memcpy (out_inl_reply, cci_stream_data (reply_stream), reply_length);
        }
    }
        
    cci_stream_release (request_stream);
    cci_stream_release (reply_stream);
    if (in_ool_requestCnt) { vm_deallocate (mach_task_self (), (vm_address_t) in_ool_request, in_ool_requestCnt); }
    
    return cci_check_error (err);
}
