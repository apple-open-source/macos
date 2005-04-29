/*
 * 
 * (c) Copyright 1990 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1990 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1990 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**
**  NAME:
**
**      autohndl.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Support for [auto_handle] client
**
**  VERSION: DCE 1.0
**
**
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

/*******************************************************************************/
/*                                                                             */
/*   If there is not currently a valid import cursor, get one                  */
/*                                                                             */
/*******************************************************************************/
void rpc_ss_make_import_cursor_valid
(
    RPC_SS_THREADS_MUTEX_T *p_import_cursor_mutex __attribute((unused)),
    rpc_ns_handle_t *p_import_cursor __attribute((unused)),
    rpc_if_handle_t p_if_spec __attribute((unused)),
    error_status_t *p_import_cursor_status __attribute((unused))
)
{
    /*
     * We don't support the Common Communications Service.
     */
}

/*******************************************************************************/
/*                                                                             */
/*   Get next potential server                                                 */
/*                                                                             */
/*******************************************************************************/
void rpc_ss_import_cursor_advance
(
    RPC_SS_THREADS_MUTEX_T *p_import_cursor_mutex __attribute((unused)),
    ndr_boolean *p_cache_timeout_was_set_lo __attribute((unused)),
                                              /* true if the cache time out */
                                              /* for this import context    */
                                              /* was set low at some point. */
    rpc_ns_handle_t *p_import_cursor __attribute((unused)),
    rpc_if_handle_t p_if_spec __attribute((unused)),
    ndr_boolean *p_binding_had_error __attribute((unused)),
        /* TRUE if an error occurred using the current binding */
    rpc_binding_handle_t *p_interface_binding __attribute((unused)),
        /* Ptr to binding currently being used for this interface */
    rpc_binding_handle_t *p_operation_binding __attribute((unused)),
      /* Ptr to location for binding operation is using, NULL if first attempt */
    error_status_t *p_import_cursor_status __attribute((unused)),
    error_status_t *p_st
)
{
    /*
     * We don't support the Common Communications Service.
     */
    *p_st = rpc_s_no_more_bindings;
}

/*******************************************************************************/
/*                                                                             */
/*   Flag "error occurred when an operation used this binding"                 */
/*                                                                             */
/*******************************************************************************/
void rpc_ss_flag_error_on_binding
(
    RPC_SS_THREADS_MUTEX_T *p_import_cursor_mutex __attribute((unused)),
    ndr_boolean *p_binding_had_error,
    rpc_binding_handle_t *p_interface_binding __attribute((unused)),
        /* Ptr to binding currently being used for this interface */
    rpc_binding_handle_t *p_operation_binding __attribute((unused))
      /* Ptr to location for binding operation is using */
)
{
    /*
     * We don't support the Common Communications Service.
     */
    *p_binding_had_error = ndr_true;
}
