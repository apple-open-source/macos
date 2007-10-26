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

#include "ccapi_os_ipc.h"

#include <Kerberos/kipc_client.h>
#include "cci_mig.h"
#include "k5-thread.h"
#include "k5-platform.h"

#define cci_server_bundle_id "edu.mit.Kerberos.CCacheServer"
#define cci_server_path "/System/Library/CoreServices/CCacheServer.app/Contents/MacOS/CCacheServer"

static mach_port_t g_connection_port = MACH_PORT_NULL;
static k5_mutex_t g_connection_port_mutex = K5_MUTEX_PARTIAL_INITIALIZER;

MAKE_INIT_FUNCTION(cci_os_ipc_init);

/* ------------------------------------------------------------------------ */

static int cci_os_ipc_init (void)
{
    return k5_mutex_finish_init(&g_connection_port_mutex);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_os_ipc (cc_int32      in_launch_server,
                     cci_stream_t  in_request_stream,
                     cci_stream_t *out_reply_stream)
{
    cc_int32 err = ccNoError;
    mach_port_t server_port = MACH_PORT_NULL;
    const char *inl_request = NULL; /* char * so we can pass the buffer in directly */
    mach_msg_type_number_t inl_request_length = 0;
    cci_mipc_ool_request_t ool_request = NULL;
    mach_msg_type_number_t ool_request_length = 0;
    cci_mipc_inl_reply_t inl_reply;
    mach_msg_type_number_t inl_reply_length = 0;
    cci_mipc_ool_reply_t ool_reply = NULL;
    mach_msg_type_number_t ool_reply_length = 0;
    cci_stream_t reply_stream = NULL;
    
    if (!in_request_stream) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply_stream ) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = cci_stream_new (&reply_stream);
    }
    
    if (!err) {
        err = kipc_client_lookup_server (cci_server_bundle_id, cci_server_path, 
                                         in_launch_server, &server_port);
    }
    
    if (!err) {
        /* depending on how big the message is, use the fast inline buffer or  
         * the slow dynamically allocated buffer */
        mach_msg_type_number_t request_length = cci_stream_size (in_request_stream);
        
        if (request_length > kCCAPIMaxILMsgSize) {
            cci_debug_printf ("%s choosing out of line buffer (size is %d)", 
                              __FUNCTION__, request_length);
            
            err = vm_read (mach_task_self (), 
                           (vm_address_t) cci_stream_data (in_request_stream), request_length, 
                           (vm_address_t *) &ool_request, &ool_request_length);        
        } else {
            //cci_debug_printf ("%s choosing in line buffer (size is %d)",
            //                  __FUNCTION__, request_length);
            
            inl_request_length = request_length;
            inl_request = cci_stream_data (in_request_stream);
        }
    }
    
    if (!err) {
        cc_int32 sent_message = 0;
        cc_int32 try_count = 0;

        err = k5_mutex_lock (&g_connection_port_mutex);
    
        while (!err && !sent_message) {
            if (g_connection_port == MACH_PORT_NULL) {
                /* If there is no port, ask the server for a new one */
                err = cci_mipc_create_client_connection (server_port, &g_connection_port);
            }
            
            if (!err) {
                err = cci_mipc_handle_message (g_connection_port, 
                                               inl_request, inl_request_length,
                                               ool_request, ool_request_length, 
                                               inl_reply, &inl_reply_length, 
                                               &ool_reply, &ool_reply_length);
            }
            
            if (err == MACH_SEND_INVALID_DEST) {
                /* Server died or relaunched -- clean up old connection_port */
                mach_port_mod_refs (mach_task_self(), g_connection_port, MACH_PORT_RIGHT_SEND, -1 );
                g_connection_port = MACH_PORT_NULL;
                
                if (try_count < 2) { 
                    try_count++; 
                    err = ccNoError; 
                }
            } else {
                /* Talked to server, though we may have gotten an error */
                sent_message = 1;
                
                /* Because we use ",dealloc" ool_request will be freed by mach. Don't double free it. */
                ool_request = NULL; 
                ool_request_length = 0;                
            }
        }

        k5_mutex_unlock (&g_connection_port_mutex);    
    }
    
    if (!err) {
        if (inl_reply_length) {
            err = cci_stream_write (reply_stream, inl_reply, inl_reply_length);
            
        } else if (ool_reply_length) {
            err = cci_stream_write (reply_stream, ool_reply, ool_reply_length);
            
        } else {
            err = cci_check_error (ccErrBadInternalMessage);
        }
    }
    
    if (err == BOOTSTRAP_UNKNOWN_SERVICE && !in_launch_server) {
        err = ccNoError;  /* If the server is not running just return an empty stream. */
    } 
    
    if (!err) {
        *out_reply_stream = reply_stream;
        reply_stream = NULL;
    }
    
    if (ool_reply_length  ) { vm_deallocate (mach_task_self (), (vm_address_t) ool_reply, ool_reply_length); }
    if (ool_request_length) { vm_deallocate (mach_task_self (), (vm_address_t) ool_request, ool_request_length); }
    if (reply_stream      ) { cci_stream_release (reply_stream); }
    
    return cci_check_error (err);    
}
