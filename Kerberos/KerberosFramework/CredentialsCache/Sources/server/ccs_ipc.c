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

#include "ccs_ipc.h"

#include "ccs_server.h"
#include "cci_debugging.h"
#include "cci_message.h"

/* ------------------------------------------------------------------------ */

cc_uint32 ccs_ipc (cci_stream_t  in_request,
                   cci_stream_t *out_reply)
{
    cc_uint32 err = ccNoError;
    cc_uint32 request_name = 0;
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
        
        server_err = ccs_server_handle_message (request_name, 
                                                request_identifier,
                                                in_request, &reply_data);
        
        err = cci_message_new_reply_header (&reply, server_err);
    }
    
    if (!err && reply_data) {
        const char *data = NULL;
        cc_uint64 size = 0;
        
        err = cci_stream_data (reply_data, &data, &size);
        
        if (!err) {
            err = cci_stream_write (reply, data, size);
        }
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
