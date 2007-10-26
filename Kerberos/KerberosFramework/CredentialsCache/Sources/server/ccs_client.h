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

#ifndef CCS_CLIENT_H
#define CCS_CLIENT_H

#include "ccs_types.h"

cc_int32 ccs_client_new (ccs_client_t  *out_client,
                         ccs_os_port_t  in_receive_port);

inline cc_int32 ccs_client_release (ccs_client_t io_client);

cc_int32 ccs_client_object_release (void *io_client);

cc_int32 ccs_client_compare_identifier (ccs_client_t      in_client,
                                        cci_identifier_t  in_identifier,
                                        cc_uint32        *out_equal);

cc_int32 ccs_client_sends_to_port (ccs_client_t   in_client,
                                   ccs_os_port_t  in_receive_port,
                                   cc_uint32     *out_sends_to_port);

#endif /* CCS_CLIENT_H */
