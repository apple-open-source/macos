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
#include "ccs_os_port.h"

struct ccs_client_d {
    cci_identifier_t identifier;
    ccs_os_port_t receive_port;
#warning Add support for tracking callbacks here
};

struct ccs_client_d ccs_client_initializer = { NULL, CCS_OS_PORT_INITIALIZER };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_new (ccs_client_t  *out_client,
                         ccs_os_port_t  in_receive_port)
{
    cc_int32 err = ccNoError;
    ccs_client_t client = NULL;
    
    if (!out_client     ) { err = cci_check_error (ccErrBadParam); }
    if (in_receive_port == CCS_OS_PORT_INITIALIZER) { 
        err = cci_check_error (ccErrBadParam); 
    }
    
    if (!err) {
        client = malloc (sizeof (*client));
        if (client) { 
            *client = ccs_client_initializer;
        } else {
            err = cci_check_error (ccErrNoMem); 
        }
    }
    
    if (!err) {
        err = ccs_server_new_identifier (&client->identifier);
    }
    
    if (!err) {
        err = ccs_os_port_copy (&client->receive_port, in_receive_port);
    }
    
    if (!err) {
        *out_client = client;
        client = NULL;
    }

    if (client) { ccs_client_release (client); }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

inline cc_int32 ccs_client_release (ccs_client_t io_client)
{
    cc_int32 err = ccNoError;
    
    if (!io_client) { err = ccErrBadParam; }
    
    if (!err) {
        cci_identifier_release (io_client->identifier);
        ccs_os_port_release (io_client->receive_port);
        free (io_client);
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_object_release (void *io_client)
{
    return cci_check_error (ccs_client_release ((ccs_client_t) io_client));    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_compare_identifier (ccs_client_t      in_client,
                                        cci_identifier_t  in_identifier,
                                        cc_uint32        *out_equal)
{
    cc_int32 err = ccNoError;
    
    if (!in_client    ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal    ) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = cci_identifier_compare (in_client->identifier, 
                                      in_identifier, 
                                      out_equal);
    }
    
    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_sends_to_port (ccs_client_t   in_client,
                                   ccs_os_port_t  in_receive_port,
                                   cc_uint32     *out_sends_to_port)
{
    cc_int32 err = ccNoError;
    
    if (!in_client) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = ccs_os_port_compare (in_client->receive_port, in_receive_port, 
                                   out_sends_to_port);
    }    
    
    return cci_check_error (err);    
}
