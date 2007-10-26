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

#include "ccs_common.h"

/* Server Globals: */

cci_uuid_string_t g_server_id = NULL;
ccs_cache_collection_t g_cache_collection = NULL;
ccs_client_array_t g_client_array = NULL;

#pragma mark -

/* ------------------------------------------------------------------------ */

 cc_int32 ccs_server_initialize (void)
{
    cc_int32 err = ccNoError;

    if (!err) {
        err = cci_identifier_new_uuid (&g_server_id);
    }
    
    if (!err) {
        err = ccs_cache_collection_new (&g_cache_collection);
    }
    
    if (!err) {
        err = ccs_client_array_new (&g_client_array);
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

 cc_int32 ccs_server_cleanup (void)
{
    cc_int32 err = ccNoError;
    
    if (!err) {
        free (g_server_id);
        cci_check_error (ccs_cache_collection_release (g_cache_collection));
        cci_check_error (ccs_client_array_release (g_client_array));
    }
    
    return cci_check_error (err);    
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 ccs_server_new_identifier (cci_identifier_t *out_identifier)
{
    return cci_check_error (cci_identifier_new (out_identifier,
                                                g_server_id));
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 ccs_server_add_client (ccs_os_port_t in_receive_port)
{
    cc_int32 err = ccNoError;
    ccs_client_t client = NULL;
    
    if (in_receive_port == CCS_OS_PORT_INITIALIZER) { 
        err = cci_check_error (ccErrBadParam); 
    }
    
    if (!err) {
        err = ccs_client_new (&client, in_receive_port);
    }
    
    if (!err) {
        cci_debug_printf ("%s: Adding client %x.", __FUNCTION__, in_receive_port);
        err = ccs_client_array_insert (g_client_array, 
                                       client,
                                       ccs_client_array_count (g_client_array));
    }
    
    if (!err) {
        client = NULL; /* take ownership */
    }
    
    if (client) { ccs_client_release (client); }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_server_remove_client (ccs_os_port_t in_receive_port)
{
    cc_int32 err = ccNoError;

    if (in_receive_port == CCS_OS_PORT_INITIALIZER) { 
        err = cci_check_error (ccErrBadParam); 
    }
    
    if (!err) {
        cc_uint64 i;
        cc_uint64 count = ccs_client_array_count (g_client_array);
        
        for (i = 0; !err && i < count; i++) {
            ccs_client_t client = ccs_client_array_object_at_index (g_client_array, i);
            cc_uint32 sends_to_port = 0;
            
            err = ccs_client_sends_to_port (client, in_receive_port, 
                                            &sends_to_port);
            
            if (!err && sends_to_port) {
                cci_debug_printf ("%s: Removing client %x.", __FUNCTION__, in_receive_port);
                err = ccs_client_array_remove (g_client_array, i);
                break;
            }
        }
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_server_find_client_by_identifier (cci_identifier_t  in_identifier,
                                               ccs_client_t     *out_client)
{
    cc_int32 err = ccNoError;
    cc_uint32 found = 0;
    
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_client   ) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        cc_uint64 i;
        cc_uint64 count = ccs_client_array_count (g_client_array);
        
        for (i = 0; !err && i < count; i++) {
            ccs_client_t client = ccs_client_array_object_at_index (g_client_array, i);
            
            err = ccs_client_compare_identifier (client, in_identifier, &found);
            
            if (!err && found) {
                *out_client = client;
                break;
            }
        }
    }
    
    if (!err && !found) {
        err = cci_check_error (ccIteratorEnd);
    }
    
    return cci_check_error (err);    
}

#pragma mark -

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_server_handle_message (ccs_cache_collection_t  in_cache_collection,
                                           enum cci_msg_id_t       in_request_name,
                                           cci_identifier_t        in_request_identifier,
                                           cci_stream_t            in_request_data,
                                           cci_stream_t           *out_reply_data)
{
    cc_int32 err = ccNoError;

    if (!in_request_data) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply_data ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        if (in_request_name > cci_context_first_msg_id &&
            in_request_name < cci_context_last_msg_id) {
            
#warning Need code to deal with case where context identifier doesn't match.
            
            err = ccs_cache_collection_handle_message (in_cache_collection,
                                                       in_request_name,
                                                       in_request_data, 
                                                       out_reply_data);

        } else if (in_request_name > cci_ccache_first_msg_id &&
                   in_request_name < cci_ccache_last_msg_id) {
            ccs_ccache_t ccache = NULL;
            
            err = ccs_cache_collection_find_ccache (in_cache_collection,
                                                    in_request_identifier,
                                                    &ccache);            
            
            if (!err) {
                err = ccs_ccache_handle_message (ccache,
                                                 in_cache_collection,
                                                 in_request_name,
                                                 in_request_data, 
                                                 out_reply_data);
            }                
                        
        } else if (in_request_name > cci_ccache_iterator_first_msg_id &&
                   in_request_name < cci_ccache_iterator_last_msg_id) {
            ccs_ccache_iterator_t ccache_iterator = NULL;
            
            err = ccs_cache_collection_find_ccache_iterator (in_cache_collection,
                                                             in_request_identifier,
                                                             &ccache_iterator);            
            
            if (!err) {
                err = ccs_ccache_iterator_handle_message (ccache_iterator,
                                                          in_cache_collection,
                                                          in_request_name,
                                                          in_request_data, 
                                                          out_reply_data);
            }                
            
        } else if (in_request_name > cci_credentials_iterator_first_msg_id &&
                   in_request_name < cci_credentials_iterator_last_msg_id) {
            ccs_credentials_iterator_t credentials_iterator = NULL;
            ccs_ccache_t ccache = NULL;
            
            err = ccs_cache_collection_find_credentials_iterator (in_cache_collection,
                                                                  in_request_identifier,
                                                                  &ccache,
                                                                  &credentials_iterator);            
            
            if (!err) {
                err = ccs_credentials_iterator_handle_message (credentials_iterator,
                                                               ccache,
                                                               in_request_name,
                                                               in_request_data, 
                                                               out_reply_data);
            }
            
        } else {
            err = ccErrBadInternalMessage;
        }
    }
    
    return cci_check_error (err);
}

#pragma mark -

/* ------------------------------------------------------------------------ */

 cc_int32 ccs_server_ipc (cci_stream_t  in_request,
                          cci_stream_t *out_reply)
{
    cc_int32 err = ccNoError;
    enum cci_msg_id_t request_name = 0;
    cci_identifier_t request_identifier = NULL;
    cci_stream_t reply_data = NULL;
    cci_stream_t reply = NULL;
    
    if (!in_request) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply ) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = cci_message_read_request_header (in_request,
                                               &request_name,
                                               &request_identifier);
    }
    
    if (!err) {
        cc_uint32 server_err = 0;
        cc_uint32 valid = 0;
        ccs_cache_collection_t cache_collection = g_cache_collection;
        
        server_err = cci_identifier_is_for_server (request_identifier, 
                                                   g_server_id, 
                                                   &valid);
        
        if (!server_err && !valid) {
            server_err = cci_message_invalid_object_err (request_name);
        }
        
#warning Insert code to select cache collection here.
        
        if (!server_err) {
            server_err = ccs_server_handle_message (cache_collection,
                                                    request_name,
                                                    request_identifier,
                                                    in_request, &reply_data);
        }
        
        err = cci_message_new_reply_header (&reply, server_err);
    }
    
    if (!err && reply_data && cci_stream_size (reply_data) > 0) {
        err = cci_stream_write (reply, 
                                cci_stream_data (reply_data), 
                                cci_stream_size (reply_data));
    }
    
    if (!err) {
        *out_reply = reply;
        reply = NULL;
    }
    
    cci_identifier_release (request_identifier);
    cci_stream_release (reply_data);
    cci_stream_release (reply);
    
    return cci_check_error (err);    
}
