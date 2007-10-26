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

/* On Mac OS X ccs_os_port_t is a mach_port_t */

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_port_copy (ccs_os_port_t *out_port,
                           ccs_os_port_t  in_port)
{
    cc_int32 err = ccNoError;
    
    if (!out_port) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_port = in_port;
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_port_release (ccs_os_port_t io_port)
{
    return ccNoError;    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_port_compare (ccs_os_port_t  in_port,
                              ccs_os_port_t  in_compare_to_port,
                              cc_uint32     *out_equal)
{
    cc_int32 err = ccNoError;
    
    if (!out_equal) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_equal = (in_port == in_compare_to_port);
    }
    
    return cci_check_error (err);    
}
