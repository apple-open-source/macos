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
#include "ccs_server.h"

static cc_uint32 g_initialized = 0;

/* ------------------------------------------------------------------------ */

cc_int32 cci_os_ipc (cc_int32      in_launch_server,
                     cci_stream_t  in_request_stream,
                     cci_stream_t *out_reply_stream)
{
    cc_int32 err = ccNoError;
        
    if (!in_request_stream) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply_stream ) { err = cci_check_error (ccErrBadParam); }
    
    if (!err && !g_initialized && in_launch_server) {
        err = ccs_server_initialize ();
        
        if (!err) {
            g_initialized = 1;
        }
    }
    
    if (!err) { 
        if (g_initialized) {
            err = ccs_server_ipc (in_request_stream, out_reply_stream);
        } else {
            *out_reply_stream = NULL;
        }
    }
    
    return cci_check_error (err);    
}
