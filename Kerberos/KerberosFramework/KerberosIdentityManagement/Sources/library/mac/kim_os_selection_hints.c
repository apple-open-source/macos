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

#include "kim_os_private.h"

/* ------------------------------------------------------------------------ */

kim_error_t kim_os_selection_hints_cache_lookup (kim_selection_hints_t  in_selection_hints,
                                                 kim_identity_t        *out_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_identity       == NULL) { err = param_error (2, "out_identity", "NULL"); }
    
    if (!err) {
        
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_os_selection_hints_cache_results (kim_selection_hints_t in_selection_hints,
                                                  kim_identity_t        in_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (in_identity        == NULL) { err = param_error (2, "in_identity", "NULL"); }
    
    if (!err) {
        
    }
    
    return check_error (err);
}
