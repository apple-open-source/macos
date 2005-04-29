/*
 * 
 * (c) Copyright 1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1993 DIGITAL EQUIPMENT CORPORATION
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
**
**  NAME:
**
**      sscmaset.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      CMA machinery used by IDL stubs, conversion between
**       exceptions and status codes, maps between statuses,
**       ASCII/EBCDIC support, optimization support
**
**  VERSION: DCE 1.0
**
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef MIA
#include <dce/idlddefs.h>
#endif

#   include <stdio.h>
#   ifdef __STDC__
#       include <stdlib.h>
#   endif

/*
 * declare the default character translation tables.  Not in stubbase, since
 * the stubs never reference them directly.
 */
globalref rpc_trans_tab_t ndr_g_def_ascii_to_ebcdic;
globalref rpc_trans_tab_t ndr_g_def_ebcdic_to_ascii;

/*
 * Forward referenced routines.
 */
static error_status_t rpc_ss_map_fault_code
(
    ndr_ulong_int fault_code
);

/* rpc_ss_call_free is needed because we can't use 'free' as a procedure argument
    or in an assignment, because CMA has redefined free to be
    a macro with an argument */
void rpc_ss_call_free
(
    rpc_void_p_t address
)
{
    free( address );
}


#if defined(CMA_INCLUDE) && !defined(VMS)  /* Need CMA_INCLUDE which provides cma_global_lock */

/******************************************************************************/
/*                                                                            */
/*    Set up jacket for getenv() to make it thread safe.                      */
/*    If the base implementation provides a safe getenv, this can be discarded*/
/*                                                                            */
/******************************************************************************/
char * safe_getenv
(
    char *variable
)
{
    char *result;
    cma_lock_global ();
    result = (char*)getenv (variable);
    cma_unlock_global ();
    return result;
}
#define getenv safe_getenv
#endif

#if defined(CMA_INCLUDE) && defined(VMS) 
/******************************************************************************/
/* Note: CMA now provides these jackets on all platforms other than VAX/VMS   */
/*       so we don't need them anywhere else                                  */
/*                                                                            */
/*    Set up jackets for fopen, fread, and fclose to make them thread safe.   */
/*    If the base implementation provides safe ones, these can be discarded.  */
/*                                                                            */
/******************************************************************************/
FILE * safe_fopen
(
    char *filename,
    char *type
)
{
    FILE *result;
    cma_lock_global();
    result = fopen(filename, type);
    cma_unlock_global();
    return result;
}
#define fopen safe_fopen

int safe_fread
(
    char        *ptr,
    unsigned    sizeof_ptr,
    unsigned    nitems,
    FILE        *stream
)
{
    int result;
    cma_lock_global();
    result = fread(ptr, sizeof_ptr, nitems, stream);
    cma_unlock_global();
    return result;
}
#define fread safe_fread

int safe_fclose
(
    FILE *stream
)
{
    int result;
    cma_lock_global();
    result = fclose(stream);
    cma_unlock_global();
    return result;
}
#define fclose safe_fclose

#endif

/***************************************************************************
 * Initialize the character translation tables.
 * This routine is designed to be called from rpc_ss_init_client.
 * That guarantees that it will only be invoked once per address space.
 **************************************************************************/
void rpc_ss_trans_table_init(
    void
)
{
char *filename;
FILE *fid;

    /*
     * Attempt to translate the environment variable which points us
     * to a file containing replacement translation tables.
     */
    if (!(filename = (char*)getenv("DCERPCCHARTRANS"))) {
        ndr_g_ascii_to_ebcdic = (rpc_trans_tab_p_t) &ndr_g_def_ascii_to_ebcdic[0];
        ndr_g_ebcdic_to_ascii = (rpc_trans_tab_p_t) &ndr_g_def_ebcdic_to_ascii[0];
        return;
    }

    /*
     * We have a translation, therefore we attempt to open the file.
     * A failure to open the file must be reported.  We can't use
     * the defalt translation table if the user told us to use some
     * alternate ones. Raise a file open failure exception.
     */
    ndr_g_ascii_to_ebcdic = (rpc_trans_tab_p_t) malloc (512);

    if (ndr_g_ascii_to_ebcdic==NULL)
        RAISE (rpc_x_no_memory);


    if (!(fid = fopen (filename, "r")))
        RAISE (rpc_x_ss_char_trans_open_fail);

    /*
     * Successfully opened the file.  There must be at least 512 bytes
     * in the file.  The first 256 are the ASCII->EBCDIC table--the second
     * 256 are the EBCDIC->ASCII table.  Bytes after 512 are ignored.
     */
    ndr_g_ebcdic_to_ascii = (rpc_trans_tab_p_t)(
                     (char*) ndr_g_ascii_to_ebcdic + 256);

    /*
     * If fewer than 512 bytes are read, raise a shortfile exception.
     */
    if (fread ((char *)ndr_g_ascii_to_ebcdic, 1, 512, fid) < (size_t)512)
    {
        fclose (fid);
        RAISE (rpc_x_ss_char_trans_short_file);
    }

    /*
     * We are done, with the table pointers initialized to either the default
     * tables, or to the custom tables read from the file.
     */
    fclose (fid);
}

/******************************************************************************/
/*                                                                            */
/*    Set up CMA machinery required by client                                 */
/*                                                                            */
/******************************************************************************/

#ifndef VMS
ndr_boolean rpc_ss_client_is_set_up = ndr_false;
#endif
static RPC_SS_THREADS_ONCE_T client_once = RPC_SS_THREADS_ONCE_INIT;

globaldef EXCEPTION rpc_x_assoc_grp_not_found;
globaldef EXCEPTION rpc_x_call_timeout;
globaldef EXCEPTION rpc_x_cancel_timeout;
globaldef EXCEPTION rpc_x_coding_error;
globaldef EXCEPTION rpc_x_context_id_not_found;
globaldef EXCEPTION rpc_x_comm_failure;
globaldef EXCEPTION rpc_x_endpoint_not_found;
globaldef EXCEPTION rpc_x_in_args_too_big;
globaldef EXCEPTION rpc_x_invalid_binding;
globaldef EXCEPTION rpc_x_invalid_bound;
globaldef EXCEPTION rpc_x_invalid_call_opt;
globaldef EXCEPTION rpc_x_invalid_naf_id;
globaldef EXCEPTION rpc_x_invalid_rpc_protseq;
globaldef EXCEPTION rpc_x_invalid_tag;
globaldef EXCEPTION rpc_x_invalid_timeout;
globaldef EXCEPTION rpc_x_manager_not_entered;
globaldef EXCEPTION rpc_x_max_descs_exceeded;
globaldef EXCEPTION rpc_x_no_fault;
globaldef EXCEPTION rpc_x_no_memory;
globaldef EXCEPTION rpc_x_not_rpc_tower;
globaldef EXCEPTION rpc_x_op_rng_error;
globaldef EXCEPTION rpc_x_object_not_found;
globaldef EXCEPTION rpc_x_protocol_error;
globaldef EXCEPTION rpc_x_protseq_not_supported;
globaldef EXCEPTION rpc_x_rpcd_comm_failure;
globaldef EXCEPTION rpc_x_server_too_busy;
globaldef EXCEPTION rpc_x_unknown_error;
globaldef EXCEPTION rpc_x_unknown_if;
globaldef EXCEPTION rpc_x_unknown_mgr_type;
globaldef EXCEPTION rpc_x_unknown_reject;
globaldef EXCEPTION rpc_x_unknown_remote_fault;
globaldef EXCEPTION rpc_x_unsupported_type;
globaldef EXCEPTION rpc_x_who_are_you_failed;
globaldef EXCEPTION rpc_x_wrong_boot_time;
globaldef EXCEPTION rpc_x_wrong_kind_of_binding;
globaldef EXCEPTION uuid_x_getconf_failure;
globaldef EXCEPTION uuid_x_no_address;
globaldef EXCEPTION uuid_x_internal_error;
globaldef EXCEPTION uuid_x_socket_failure;

globaldef EXCEPTION rpc_x_access_control_info_inv;
globaldef EXCEPTION rpc_x_assoc_grp_max_exceeded;
globaldef EXCEPTION rpc_x_assoc_shutdown;
globaldef EXCEPTION rpc_x_cannot_accept;
globaldef EXCEPTION rpc_x_cannot_connect;
globaldef EXCEPTION rpc_x_cannot_set_nodelay;
globaldef EXCEPTION rpc_x_cant_inq_socket;
globaldef EXCEPTION rpc_x_connect_closed_by_rem;
globaldef EXCEPTION rpc_x_connect_no_resources;
globaldef EXCEPTION rpc_x_connect_rejected;
globaldef EXCEPTION rpc_x_connect_timed_out;
globaldef EXCEPTION rpc_x_connection_aborted;
globaldef EXCEPTION rpc_x_connection_closed;
globaldef EXCEPTION rpc_x_host_unreachable;
globaldef EXCEPTION rpc_x_invalid_endpoint_format;
globaldef EXCEPTION rpc_x_loc_connect_aborted;
globaldef EXCEPTION rpc_x_network_unreachable;
globaldef EXCEPTION rpc_x_no_rem_endpoint;
globaldef EXCEPTION rpc_x_rem_host_crashed;
globaldef EXCEPTION rpc_x_rem_host_down;
globaldef EXCEPTION rpc_x_rem_network_shutdown;
globaldef EXCEPTION rpc_x_rpc_prot_version_mismatch;
globaldef EXCEPTION rpc_x_string_too_long;
globaldef EXCEPTION rpc_x_too_many_rem_connects;
globaldef EXCEPTION rpc_x_tsyntaxes_unsupported;

globaldef EXCEPTION rpc_x_binding_vector_full;
globaldef EXCEPTION rpc_x_entry_not_found;
globaldef EXCEPTION rpc_x_group_not_found;
globaldef EXCEPTION rpc_x_incomplete_name;
globaldef EXCEPTION rpc_x_invalid_arg;
globaldef EXCEPTION rpc_x_invalid_import_context;
globaldef EXCEPTION rpc_x_invalid_inquiry_context;
globaldef EXCEPTION rpc_x_invalid_inquiry_type;
globaldef EXCEPTION rpc_x_invalid_lookup_context;
globaldef EXCEPTION rpc_x_invalid_name_syntax;
globaldef EXCEPTION rpc_x_invalid_object;
globaldef EXCEPTION rpc_x_invalid_vers_option;
globaldef EXCEPTION rpc_x_name_service_unavailable;
globaldef EXCEPTION rpc_x_no_env_setup;
globaldef EXCEPTION rpc_x_no_more_bindings;
globaldef EXCEPTION rpc_x_no_more_elements;
globaldef EXCEPTION rpc_x_no_ns_permission;
globaldef EXCEPTION rpc_x_not_found;
globaldef EXCEPTION rpc_x_not_rpc_entry;
globaldef EXCEPTION rpc_x_obj_uuid_not_found;
globaldef EXCEPTION rpc_x_profile_not_found;
globaldef EXCEPTION rpc_x_unsupported_name_syntax;

globaldef EXCEPTION rpc_x_auth_bad_integrity;
globaldef EXCEPTION rpc_x_auth_badaddr;
globaldef EXCEPTION rpc_x_auth_baddirection;
globaldef EXCEPTION rpc_x_auth_badkeyver;
globaldef EXCEPTION rpc_x_auth_badmatch;
globaldef EXCEPTION rpc_x_auth_badorder;
globaldef EXCEPTION rpc_x_auth_badseq;
globaldef EXCEPTION rpc_x_auth_badversion;
globaldef EXCEPTION rpc_x_auth_field_toolong;
globaldef EXCEPTION rpc_x_auth_inapp_cksum;
globaldef EXCEPTION rpc_x_auth_method;
globaldef EXCEPTION rpc_x_auth_msg_type;
globaldef EXCEPTION rpc_x_auth_modified;
globaldef EXCEPTION rpc_x_auth_mut_fail;
globaldef EXCEPTION rpc_x_auth_nokey;
globaldef EXCEPTION rpc_x_auth_not_us;
globaldef EXCEPTION rpc_x_auth_repeat;
globaldef EXCEPTION rpc_x_auth_skew;
globaldef EXCEPTION rpc_x_auth_tkt_expired;
globaldef EXCEPTION rpc_x_auth_tkt_nyv;
globaldef EXCEPTION rpc_x_call_id_not_found;
globaldef EXCEPTION rpc_x_credentials_too_large;
globaldef EXCEPTION rpc_x_invalid_checksum;
globaldef EXCEPTION rpc_x_invalid_crc;
globaldef EXCEPTION rpc_x_invalid_credentials;
globaldef EXCEPTION rpc_x_key_id_not_found;

globaldef EXCEPTION rpc_x_ss_pipe_empty;
globaldef EXCEPTION rpc_x_ss_pipe_closed;
globaldef EXCEPTION rpc_x_ss_pipe_order;
globaldef EXCEPTION rpc_x_ss_pipe_discipline_error;
globaldef EXCEPTION rpc_x_ss_pipe_comm_error;
globaldef EXCEPTION rpc_x_ss_pipe_memory;
globaldef EXCEPTION rpc_x_ss_context_mismatch;
globaldef EXCEPTION rpc_x_ss_context_damaged;
globaldef EXCEPTION rpc_x_ss_in_null_context;
globaldef EXCEPTION rpc_x_ss_char_trans_short_file;
globaldef EXCEPTION rpc_x_ss_char_trans_open_fail;
globaldef EXCEPTION rpc_x_ss_remote_comm_failure;
globaldef EXCEPTION rpc_x_ss_remote_no_memory;
globaldef EXCEPTION rpc_x_ss_bad_buffer;
globaldef EXCEPTION rpc_x_ss_bad_es_action;
globaldef EXCEPTION rpc_x_ss_wrong_es_version;
globaldef EXCEPTION rpc_x_ss_incompatible_codesets;
globaldef EXCEPTION rpc_x_stub_protocol_error;
globaldef EXCEPTION rpc_x_unknown_stub_rtl_if_vers;
globaldef EXCEPTION rpc_x_ss_codeset_conv_error;

static void rpc_ss_init_client(
    void
)
{
    /* Initialize exceptions */
    EXCEPTION_INIT(rpc_x_assoc_grp_not_found);
    exc_set_status(&rpc_x_assoc_grp_not_found,rpc_s_assoc_grp_not_found);
    EXCEPTION_INIT(rpc_x_call_timeout);
    exc_set_status(&rpc_x_call_timeout,rpc_s_call_timeout);
    EXCEPTION_INIT(rpc_x_cancel_timeout);
    exc_set_status(&rpc_x_cancel_timeout,rpc_s_cancel_timeout);
    EXCEPTION_INIT(rpc_x_coding_error);
    exc_set_status(&rpc_x_coding_error,rpc_s_coding_error);
    EXCEPTION_INIT(rpc_x_comm_failure);
    exc_set_status(&rpc_x_comm_failure,rpc_s_comm_failure);
    EXCEPTION_INIT(rpc_x_context_id_not_found);
    exc_set_status(&rpc_x_context_id_not_found,rpc_s_context_id_not_found);
    EXCEPTION_INIT(rpc_x_endpoint_not_found);
    exc_set_status(&rpc_x_endpoint_not_found,rpc_s_endpoint_not_found);
    EXCEPTION_INIT(rpc_x_in_args_too_big);
    exc_set_status(&rpc_x_in_args_too_big,rpc_s_in_args_too_big);
    EXCEPTION_INIT(rpc_x_invalid_binding);
    exc_set_status(&rpc_x_invalid_binding,rpc_s_invalid_binding);
    EXCEPTION_INIT(rpc_x_invalid_bound);
    exc_set_status(&rpc_x_invalid_bound,rpc_s_fault_invalid_bound);
    EXCEPTION_INIT(rpc_x_invalid_call_opt);
    exc_set_status(&rpc_x_invalid_call_opt,rpc_s_invalid_call_opt);
    EXCEPTION_INIT(rpc_x_invalid_naf_id);
    exc_set_status(&rpc_x_invalid_naf_id,rpc_s_invalid_naf_id);
    EXCEPTION_INIT(rpc_x_invalid_rpc_protseq);
    exc_set_status(&rpc_x_invalid_rpc_protseq,rpc_s_invalid_rpc_protseq);
    EXCEPTION_INIT(rpc_x_invalid_tag);
    exc_set_status(&rpc_x_invalid_tag,rpc_s_fault_invalid_tag);
    EXCEPTION_INIT(rpc_x_invalid_timeout);
    exc_set_status(&rpc_x_invalid_timeout,rpc_s_invalid_timeout);
    EXCEPTION_INIT(rpc_x_manager_not_entered);
    exc_set_status(&rpc_x_manager_not_entered,rpc_s_manager_not_entered);
    EXCEPTION_INIT(rpc_x_max_descs_exceeded);
    exc_set_status(&rpc_x_max_descs_exceeded,rpc_s_max_descs_exceeded);
    EXCEPTION_INIT(rpc_x_no_fault);
    exc_set_status(&rpc_x_no_fault,rpc_s_no_fault);
    EXCEPTION_INIT(rpc_x_no_memory);
    exc_set_status(&rpc_x_no_memory,rpc_s_no_memory);
    EXCEPTION_INIT(rpc_x_not_rpc_tower);
    exc_set_status(&rpc_x_not_rpc_tower,rpc_s_not_rpc_tower);
    EXCEPTION_INIT(rpc_x_object_not_found);
    exc_set_status(&rpc_x_object_not_found,rpc_s_object_not_found);
    EXCEPTION_INIT(rpc_x_op_rng_error);
    exc_set_status(&rpc_x_op_rng_error,rpc_s_op_rng_error);
    EXCEPTION_INIT(rpc_x_protocol_error);
    exc_set_status(&rpc_x_protocol_error,rpc_s_protocol_error);
    EXCEPTION_INIT(rpc_x_protseq_not_supported);
    exc_set_status(&rpc_x_protseq_not_supported,rpc_s_protseq_not_supported);
    EXCEPTION_INIT(rpc_x_rpcd_comm_failure);
    exc_set_status(&rpc_x_rpcd_comm_failure,rpc_s_rpcd_comm_failure);
    EXCEPTION_INIT(rpc_x_server_too_busy);
    exc_set_status(&rpc_x_server_too_busy,rpc_s_server_too_busy);
    EXCEPTION_INIT(rpc_x_unknown_error);
    exc_set_status(&rpc_x_unknown_error,rpc_s_unknown_error);
    EXCEPTION_INIT(rpc_x_unknown_if);
    exc_set_status(&rpc_x_unknown_if,rpc_s_unknown_if);
    EXCEPTION_INIT(rpc_x_unknown_mgr_type);
    exc_set_status(&rpc_x_unknown_mgr_type,rpc_s_unknown_mgr_type);
    EXCEPTION_INIT(rpc_x_unknown_reject);
    exc_set_status(&rpc_x_unknown_reject,rpc_s_unknown_reject);
    EXCEPTION_INIT(rpc_x_unknown_remote_fault);
    exc_set_status(&rpc_x_unknown_remote_fault,rpc_s_fault_unspec);
    EXCEPTION_INIT(rpc_x_unsupported_type);
    exc_set_status(&rpc_x_unsupported_type,rpc_s_unsupported_type);
    EXCEPTION_INIT(rpc_x_who_are_you_failed);
    exc_set_status(&rpc_x_who_are_you_failed,rpc_s_who_are_you_failed);
    EXCEPTION_INIT(rpc_x_wrong_boot_time);
    exc_set_status(&rpc_x_wrong_boot_time,rpc_s_wrong_boot_time);
    EXCEPTION_INIT(rpc_x_wrong_kind_of_binding);
    exc_set_status(&rpc_x_wrong_kind_of_binding,rpc_s_wrong_kind_of_binding);
    EXCEPTION_INIT(uuid_x_getconf_failure);
    exc_set_status(&uuid_x_getconf_failure,uuid_s_getconf_failure);
    EXCEPTION_INIT(uuid_x_internal_error);
    exc_set_status(&uuid_x_internal_error,uuid_s_internal_error);
    EXCEPTION_INIT(uuid_x_no_address);
    exc_set_status(&uuid_x_no_address,uuid_s_no_address);
    EXCEPTION_INIT(uuid_x_socket_failure);
    exc_set_status(&uuid_x_socket_failure,uuid_s_socket_failure);

    EXCEPTION_INIT(rpc_x_access_control_info_inv);
    exc_set_status(&rpc_x_access_control_info_inv,rpc_s_access_control_info_inv);
    EXCEPTION_INIT(rpc_x_assoc_grp_max_exceeded);
    exc_set_status(&rpc_x_assoc_grp_max_exceeded,rpc_s_assoc_grp_max_exceeded);
    EXCEPTION_INIT(rpc_x_assoc_shutdown);
    exc_set_status(&rpc_x_assoc_shutdown,rpc_s_assoc_shutdown);
    EXCEPTION_INIT(rpc_x_cannot_accept);
    exc_set_status(&rpc_x_cannot_accept,rpc_s_cannot_accept);
    EXCEPTION_INIT(rpc_x_cannot_connect);
    exc_set_status(&rpc_x_cannot_connect,rpc_s_cannot_connect);
    EXCEPTION_INIT(rpc_x_cannot_set_nodelay);
    exc_set_status(&rpc_x_cannot_set_nodelay,rpc_s_cannot_set_nodelay);
    EXCEPTION_INIT(rpc_x_cant_inq_socket);
    exc_set_status(&rpc_x_cant_inq_socket,rpc_s_cant_inq_socket);
    EXCEPTION_INIT(rpc_x_connect_closed_by_rem);
    exc_set_status(&rpc_x_connect_closed_by_rem,rpc_s_connect_closed_by_rem);
    EXCEPTION_INIT(rpc_x_connect_no_resources);
    exc_set_status(&rpc_x_connect_no_resources,rpc_s_connect_no_resources);
    EXCEPTION_INIT(rpc_x_connect_rejected);
    exc_set_status(&rpc_x_connect_rejected,rpc_s_connect_rejected);
    EXCEPTION_INIT(rpc_x_connect_timed_out);
    exc_set_status(&rpc_x_connect_timed_out,rpc_s_connect_timed_out);
    EXCEPTION_INIT(rpc_x_connection_aborted);
    exc_set_status(&rpc_x_connection_aborted,rpc_s_connection_aborted);
    EXCEPTION_INIT(rpc_x_connection_closed);
    exc_set_status(&rpc_x_connection_closed,rpc_s_connection_closed);
    EXCEPTION_INIT(rpc_x_host_unreachable);
    exc_set_status(&rpc_x_host_unreachable,rpc_s_host_unreachable);
    EXCEPTION_INIT(rpc_x_invalid_endpoint_format);
    exc_set_status(&rpc_x_invalid_endpoint_format,rpc_s_invalid_endpoint_format);
    EXCEPTION_INIT(rpc_x_loc_connect_aborted);
    exc_set_status(&rpc_x_loc_connect_aborted,rpc_s_loc_connect_aborted);
    EXCEPTION_INIT(rpc_x_network_unreachable);
    exc_set_status(&rpc_x_network_unreachable,rpc_s_network_unreachable);
    EXCEPTION_INIT(rpc_x_no_rem_endpoint);
    exc_set_status(&rpc_x_no_rem_endpoint,rpc_s_no_rem_endpoint);
    EXCEPTION_INIT(rpc_x_rem_host_crashed);
    exc_set_status(&rpc_x_rem_host_crashed,rpc_s_rem_host_crashed);
    EXCEPTION_INIT(rpc_x_rem_host_down);
    exc_set_status(&rpc_x_rem_host_down,rpc_s_rem_host_down);
    EXCEPTION_INIT(rpc_x_rem_network_shutdown);
    exc_set_status(&rpc_x_rem_network_shutdown,rpc_s_rem_network_shutdown);
    EXCEPTION_INIT(rpc_x_rpc_prot_version_mismatch);
    exc_set_status(&rpc_x_rpc_prot_version_mismatch,rpc_s_rpc_prot_version_mismatch);
    EXCEPTION_INIT(rpc_x_string_too_long);
    exc_set_status(&rpc_x_string_too_long,rpc_s_string_too_long);
    EXCEPTION_INIT(rpc_x_too_many_rem_connects);
    exc_set_status(&rpc_x_too_many_rem_connects,rpc_s_too_many_rem_connects);
    EXCEPTION_INIT(rpc_x_tsyntaxes_unsupported);
    exc_set_status(&rpc_x_tsyntaxes_unsupported,rpc_s_tsyntaxes_unsupported);

    EXCEPTION_INIT(rpc_x_binding_vector_full);
    exc_set_status(&rpc_x_binding_vector_full,rpc_s_binding_vector_full);
    EXCEPTION_INIT(rpc_x_entry_not_found);
    exc_set_status(&rpc_x_entry_not_found,rpc_s_entry_not_found);
    EXCEPTION_INIT(rpc_x_group_not_found);
    exc_set_status(&rpc_x_group_not_found,rpc_s_group_not_found);
    EXCEPTION_INIT(rpc_x_incomplete_name);
    exc_set_status(&rpc_x_incomplete_name,rpc_s_incomplete_name);
    EXCEPTION_INIT(rpc_x_invalid_arg);
    exc_set_status(&rpc_x_invalid_arg,rpc_s_invalid_arg);
    EXCEPTION_INIT(rpc_x_invalid_import_context);
    exc_set_status(&rpc_x_invalid_import_context,rpc_s_invalid_import_context);
    EXCEPTION_INIT(rpc_x_invalid_inquiry_context);
    exc_set_status(&rpc_x_invalid_inquiry_context,rpc_s_invalid_inquiry_context);
    EXCEPTION_INIT(rpc_x_invalid_inquiry_type);
    exc_set_status(&rpc_x_invalid_inquiry_type,rpc_s_invalid_inquiry_type);
    EXCEPTION_INIT(rpc_x_invalid_lookup_context);
    exc_set_status(&rpc_x_invalid_lookup_context,rpc_s_invalid_lookup_context);
    EXCEPTION_INIT(rpc_x_invalid_name_syntax);
    exc_set_status(&rpc_x_invalid_name_syntax,rpc_s_invalid_name_syntax);
    EXCEPTION_INIT(rpc_x_invalid_object);
    exc_set_status(&rpc_x_invalid_object,rpc_s_invalid_object);
    EXCEPTION_INIT(rpc_x_invalid_vers_option);
    exc_set_status(&rpc_x_invalid_vers_option,rpc_s_invalid_vers_option);
    EXCEPTION_INIT(rpc_x_name_service_unavailable);
    exc_set_status(&rpc_x_name_service_unavailable,rpc_s_name_service_unavailable);
    EXCEPTION_INIT(rpc_x_no_env_setup);
    exc_set_status(&rpc_x_no_env_setup,rpc_s_no_env_setup);
    EXCEPTION_INIT(rpc_x_no_more_bindings);
    exc_set_status(&rpc_x_no_more_bindings,rpc_s_no_more_bindings);
    EXCEPTION_INIT(rpc_x_no_more_elements);
    exc_set_status(&rpc_x_no_more_elements,rpc_s_no_more_elements);
    EXCEPTION_INIT(rpc_x_no_ns_permission);
    exc_set_status(&rpc_x_no_ns_permission,rpc_s_no_ns_permission);
    EXCEPTION_INIT(rpc_x_not_found);
    exc_set_status(&rpc_x_not_found,rpc_s_not_found);
    EXCEPTION_INIT(rpc_x_not_rpc_entry);
    exc_set_status(&rpc_x_not_rpc_entry,rpc_s_not_rpc_entry);
    EXCEPTION_INIT(rpc_x_obj_uuid_not_found);
    exc_set_status(&rpc_x_obj_uuid_not_found,rpc_s_obj_uuid_not_found);
    EXCEPTION_INIT(rpc_x_profile_not_found);
    exc_set_status(&rpc_x_profile_not_found,rpc_s_profile_not_found);
    EXCEPTION_INIT(rpc_x_unsupported_name_syntax);
    exc_set_status(&rpc_x_unsupported_name_syntax,rpc_s_unsupported_name_syntax);

    EXCEPTION_INIT(rpc_x_auth_bad_integrity);
    exc_set_status(&rpc_x_auth_bad_integrity,rpc_s_auth_bad_integrity);
    EXCEPTION_INIT(rpc_x_auth_badaddr);
    exc_set_status(&rpc_x_auth_badaddr,rpc_s_auth_badaddr);
    EXCEPTION_INIT(rpc_x_auth_baddirection);
    exc_set_status(&rpc_x_auth_baddirection,rpc_s_auth_baddirection);
    EXCEPTION_INIT(rpc_x_auth_badkeyver);
    exc_set_status(&rpc_x_auth_badkeyver,rpc_s_auth_badkeyver);
    EXCEPTION_INIT(rpc_x_auth_badmatch);
    exc_set_status(&rpc_x_auth_badmatch,rpc_s_auth_badmatch);
    EXCEPTION_INIT(rpc_x_auth_badorder);
    exc_set_status(&rpc_x_auth_badorder,rpc_s_auth_badorder);
    EXCEPTION_INIT(rpc_x_auth_badseq);
    exc_set_status(&rpc_x_auth_badseq,rpc_s_auth_badseq);
    EXCEPTION_INIT(rpc_x_auth_badversion);
    exc_set_status(&rpc_x_auth_badversion,rpc_s_auth_badversion);
    EXCEPTION_INIT(rpc_x_auth_field_toolong);
    exc_set_status(&rpc_x_auth_field_toolong,rpc_s_auth_field_toolong);
    EXCEPTION_INIT(rpc_x_auth_inapp_cksum);
    exc_set_status(&rpc_x_auth_inapp_cksum,rpc_s_auth_inapp_cksum);
    EXCEPTION_INIT(rpc_x_auth_method);
    exc_set_status(&rpc_x_auth_method,rpc_s_auth_method);
    EXCEPTION_INIT(rpc_x_auth_msg_type);
    exc_set_status(&rpc_x_auth_msg_type,rpc_s_auth_msg_type);
    EXCEPTION_INIT(rpc_x_auth_modified);
    exc_set_status(&rpc_x_auth_modified,rpc_s_auth_modified);
    EXCEPTION_INIT(rpc_x_auth_mut_fail);
    exc_set_status(&rpc_x_auth_mut_fail,rpc_s_auth_mut_fail);
    EXCEPTION_INIT(rpc_x_auth_nokey);
    exc_set_status(&rpc_x_auth_nokey,rpc_s_auth_nokey);
    EXCEPTION_INIT(rpc_x_auth_not_us);
    exc_set_status(&rpc_x_auth_not_us,rpc_s_auth_not_us);
    EXCEPTION_INIT(rpc_x_auth_repeat);
    exc_set_status(&rpc_x_auth_repeat,rpc_s_auth_repeat);
    EXCEPTION_INIT(rpc_x_auth_skew);
    exc_set_status(&rpc_x_auth_skew,rpc_s_auth_skew);
    EXCEPTION_INIT(rpc_x_auth_tkt_expired);
    exc_set_status(&rpc_x_auth_tkt_expired,rpc_s_auth_tkt_expired);
    EXCEPTION_INIT(rpc_x_auth_tkt_nyv);
    exc_set_status(&rpc_x_auth_tkt_nyv,rpc_s_auth_tkt_nyv);
    EXCEPTION_INIT(rpc_x_call_id_not_found);
    exc_set_status(&rpc_x_call_id_not_found,rpc_s_call_id_not_found);
    EXCEPTION_INIT(rpc_x_credentials_too_large);
    exc_set_status(&rpc_x_credentials_too_large,rpc_s_credentials_too_large);
    EXCEPTION_INIT(rpc_x_invalid_checksum);
    exc_set_status(&rpc_x_invalid_checksum,rpc_s_invalid_checksum);
    EXCEPTION_INIT(rpc_x_invalid_crc);
    exc_set_status(&rpc_x_invalid_crc,rpc_s_invalid_crc);
    EXCEPTION_INIT(rpc_x_invalid_credentials);
    exc_set_status(&rpc_x_invalid_credentials,rpc_s_invalid_credentials);
    EXCEPTION_INIT(rpc_x_key_id_not_found);
    exc_set_status(&rpc_x_key_id_not_found,rpc_s_key_id_not_found);

    EXCEPTION_INIT(rpc_x_ss_char_trans_open_fail);
    exc_set_status(&rpc_x_ss_char_trans_open_fail,rpc_s_ss_char_trans_open_fail);
    EXCEPTION_INIT(rpc_x_ss_char_trans_short_file);
    exc_set_status(&rpc_x_ss_char_trans_short_file,rpc_s_ss_char_trans_short_file);
    EXCEPTION_INIT(rpc_x_ss_pipe_empty);
    exc_set_status(&rpc_x_ss_pipe_empty,rpc_s_fault_pipe_empty);
    EXCEPTION_INIT(rpc_x_ss_pipe_closed);
    exc_set_status(&rpc_x_ss_pipe_closed,rpc_s_fault_pipe_closed);
    EXCEPTION_INIT(rpc_x_ss_pipe_order);
    exc_set_status(&rpc_x_ss_pipe_order,rpc_s_fault_pipe_order);
    EXCEPTION_INIT(rpc_x_ss_pipe_discipline_error);
    exc_set_status(&rpc_x_ss_pipe_discipline_error,rpc_s_fault_pipe_discipline);
    EXCEPTION_INIT(rpc_x_ss_pipe_comm_error);
    exc_set_status(&rpc_x_ss_pipe_comm_error,rpc_s_fault_pipe_comm_error);
    EXCEPTION_INIT(rpc_x_ss_pipe_memory);
    exc_set_status(&rpc_x_ss_pipe_memory,rpc_s_fault_pipe_memory);
    EXCEPTION_INIT(rpc_x_ss_context_mismatch);
    exc_set_status(&rpc_x_ss_context_mismatch,rpc_s_fault_context_mismatch);
    EXCEPTION_INIT(rpc_x_ss_context_damaged);
    exc_set_status(&rpc_x_ss_context_damaged,rpc_s_ss_context_damaged);
    EXCEPTION_INIT(rpc_x_ss_in_null_context);
    exc_set_status(&rpc_x_ss_in_null_context,rpc_s_ss_in_null_context);
    EXCEPTION_INIT(rpc_x_ss_remote_comm_failure);
    exc_set_status(&rpc_x_ss_remote_comm_failure,rpc_s_fault_remote_comm_failure);
    EXCEPTION_INIT(rpc_x_ss_remote_no_memory);
    exc_set_status(&rpc_x_ss_remote_no_memory,rpc_s_fault_remote_no_memory);
    EXCEPTION_INIT(rpc_x_ss_bad_buffer);
    exc_set_status(&rpc_x_ss_bad_buffer,rpc_s_ss_bad_buffer);
    EXCEPTION_INIT(rpc_x_ss_bad_es_action);
    exc_set_status(&rpc_x_ss_bad_es_action,rpc_s_ss_bad_es_action);
    EXCEPTION_INIT(rpc_x_ss_wrong_es_version);
    exc_set_status(&rpc_x_ss_wrong_es_version,rpc_s_ss_wrong_es_version);
    EXCEPTION_INIT(rpc_x_ss_incompatible_codesets);
    exc_set_status(&rpc_x_ss_incompatible_codesets,rpc_s_ss_incompatible_codesets);
    EXCEPTION_INIT(rpc_x_stub_protocol_error);
    exc_set_status(&rpc_x_stub_protocol_error,rpc_s_stub_protocol_error);
    EXCEPTION_INIT(rpc_x_unknown_stub_rtl_if_vers);
    exc_set_status(&rpc_x_unknown_stub_rtl_if_vers,rpc_s_unknown_stub_rtl_if_vers);

    rpc_ss_trans_table_init ();
}

void rpc_ss_init_client_once(
    void
)
{
    RPC_SS_THREADS_INIT;
    RPC_SS_THREADS_ONCE( &client_once, rpc_ss_init_client );

#ifndef VMS
    rpc_ss_client_is_set_up = ndr_true;
#endif

}


#ifdef IDL_ENABLE_STATUS_MAPPING
/******************************************************************************/
/*                                                                            */
/*    Routines to map a DCE status code to/from the local status code format  */
/*                                                                            */
/******************************************************************************/

/* DCE status format definitions */
#define FACILITY_CODE_MASK          0xF0000000
#define FACILITY_CODE_SHIFT         28
#define COMPONENT_CODE_MASK         0x0FFFF000
#define COMPONENT_CODE_SHIFT        12
#define STATUS_CODE_MASK            0x00000FFF
#define STATUS_CODE_SHIFT           0

/* DCE status definitions */
#define dce_rpc_s_mod 0x16c9a000
#define dce_thd_s_mod 0x177db000
#define dce_sec_s_mod 0x17122000

/* Architecture-Specific Status definitions */
#ifdef VMS
#include <stsdef.h>
#include <ssdef.h>
#ifndef sec_s_mod
#define sec_s_mod 249790466
#endif
#endif

/******************************************************************************/
/*                                                                            */
/*    Map a DCE status code to the local status code format                   */
/*                                                                            */
/******************************************************************************/
void rpc_ss_map_dce_to_local_status
(
    error_status_t *status_code_p   /* [in,out] pointer to DCE status -> local status */
)
{
    unsigned long facility_and_comp_code;
    unsigned short  status_code;

    /*
     * extract the DCE component, facility and status codes
     */
    facility_and_comp_code = (*status_code_p & 
        (FACILITY_CODE_MASK|COMPONENT_CODE_MASK));

    status_code = (*status_code_p & STATUS_CODE_MASK)
        >> STATUS_CODE_SHIFT;

#ifdef VMS
    /* Mapping for DCE/RPC status codes */
    if (facility_and_comp_code == dce_rpc_s_mod)
        *status_code_p =  (rpc_s_mod & ~STS$M_CODE) | (status_code << 3);

    /* Mapping for DCE/Security status codes */
    if (facility_and_comp_code == dce_sec_s_mod)
        *status_code_p = (sec_s_mod & ~STS$M_CODE) | (status_code << 3);

    /* Mapping for DCE/THD status codes */
    if (facility_and_comp_code == dce_thd_s_mod)
    {
        /* Have to case this because some are system-specific errors */
        switch (status_code) 
        {
            case 5:     *status_code_p = SS$_ACCVIO; break;
            case 6:     *status_code_p = SS$_EXQUOTA; break;
            case 7:     *status_code_p = SS$_INSFMEM; break;
            case 8:     *status_code_p = SS$_NOPRIV; break;        
            case 9:     *status_code_p = SS$_NORMAL; break;
            case 10:    *status_code_p = SS$_OPCDEC; break;
            case 11:    *status_code_p = SS$_RADRMOD; break;
            case 12:    *status_code_p = SS$_OPCDEC; break;
            case 13:    *status_code_p = SS$_ROPRAND; break;
            case 14:    *status_code_p = SS$_BREAK; break;
            case 15:    *status_code_p = SS$_ABORT; break;
            case 16:    *status_code_p = SS$_COMPAT; break;
            case 17:    *status_code_p = SS$_FLTOVF; break;
            case 18:    *status_code_p = SS$_BADPARAM; break;
            case 19:    *status_code_p = SS$_NOMBX; break;
            case 20:    *status_code_p = SS$_EXCPUTIM; break;
            case 21:    *status_code_p = SS$_EXDISKQUOTA; break;
            case 22:    *status_code_p = SS$_INTOVF; break;
            case 23:    *status_code_p = SS$_INTDIV; break;
            case 24:    *status_code_p = SS$_FLTOVF; break;
            case 25:    *status_code_p = SS$_FLTDIV; break;
            case 26:    *status_code_p = SS$_FLTUND; break;
            case 27:    *status_code_p = SS$_DECOVF; break;
            case 28:    *status_code_p = SS$_SUBRNG; break;
            default:  
                /* Actual DCE/THD status */
                *status_code_p =  (exc_s_exception & ~STS$M_CODE) | (status_code << 3);
                break;
        }
    }

    /* If mapping isn't available, leave it alone */
#endif
}


/******************************************************************************/
/*                                                                            */
/*    Map to a DCE status code from the local status code format              */
/*                                                                            */
/******************************************************************************/
void rpc_ss_map_local_to_dce_status
(
    error_status_t *status_code_p   /* [in,out] pointer to local status -> DCE status */
)
{
#ifdef VMS
    int facility = $VMS_STATUS_FAC_NO(*status_code_p);
    int message_number = $VMS_STATUS_CODE(*status_code_p);

    /* If success, return error_status_ok */
    if ((*status_code_p & 1) != 0) 
    {
        *status_code_p = error_status_ok;
        return;
    }

    /* Otherwise, map for each facility */
    switch (facility)
    {
        case $VMS_STATUS_FAC_NO(rpc_s_mod): /* DCE RPC facility */
            *status_code_p = (dce_rpc_s_mod | message_number);
            break;
        case $VMS_STATUS_FAC_NO(exc_s_exception): /* CMA facility */
            *status_code_p = (dce_thd_s_mod | message_number);
            break;
        case $VMS_STATUS_FAC_NO(sec_s_mod): /* DCE Security facility */
            *status_code_p = (dce_sec_s_mod | message_number);
            break;
        case 0: /* VMS facility */
            /* Map system errors, onto DCE threads status values */
            switch (*status_code_p) 
            {
                case SS$_ACCVIO:    *status_code_p = dce_thd_s_mod | 5; break;
                case SS$_EXQUOTA:   *status_code_p = dce_thd_s_mod | 6; break;
                case SS$_INSFMEM:   *status_code_p = dce_thd_s_mod | 7; break;
                case SS$_NOPRIV:    *status_code_p = dce_thd_s_mod | 8; break;        
                case SS$_NORMAL:    *status_code_p = dce_thd_s_mod | 9; break;
                case SS$_OPCDEC:    *status_code_p = dce_thd_s_mod |10; break;
                case SS$_RADRMOD:   *status_code_p = dce_thd_s_mod |11; break;
                case SS$_ROPRAND:   *status_code_p = dce_thd_s_mod |13; break;
                case SS$_BREAK:     *status_code_p = dce_thd_s_mod |14; break;
                case SS$_ABORT:     *status_code_p = dce_thd_s_mod |15; break;
                case SS$_COMPAT:    *status_code_p = dce_thd_s_mod |16; break;
                case SS$_FLTOVF:    *status_code_p = dce_thd_s_mod |17; break;
                case SS$_BADPARAM:  *status_code_p = dce_thd_s_mod |18; break;
                case SS$_NOMBX:     *status_code_p = dce_thd_s_mod |19; break;
                case SS$_EXCPUTIM:  *status_code_p = dce_thd_s_mod |20; break;
                case SS$_EXDISKQUOTA:*status_code_p = dce_thd_s_mod |21; break;
                case SS$_INTOVF:    *status_code_p = dce_thd_s_mod |22; break;
                case SS$_INTDIV:    *status_code_p = dce_thd_s_mod |23; break;
                case SS$_FLTDIV:    *status_code_p = dce_thd_s_mod |25; break;
                case SS$_FLTUND:    *status_code_p = dce_thd_s_mod |26; break;
                case SS$_DECOVF:    *status_code_p = dce_thd_s_mod |27; break;
                case SS$_SUBRNG:    *status_code_p = dce_thd_s_mod |28; break;
            }
            break;
        default:
            /* If mapping isn't available, leave value alone */
            break;
    }
#endif
}
#endif /* IDL_ENABLE_STATUS_MAPPING */

/******************************************************************************/
/*                                                                            */
/*    Convert contents of fault packet to exception                           */
/*                                                                            */
/******************************************************************************/
static void rpc_ss_raise_arch_exception
(
    ndr_ulong_int fault_code,
    RPC_SS_THREADS_CANCEL_STATE_T async_cancel_state
)
{
    EXCEPTION *p_exception;

    switch (fault_code) {
        case nca_s_fault_context_mismatch:
            p_exception = &rpc_x_ss_context_mismatch;
            break;
        case nca_s_fault_cancel:
            p_exception = &RPC_SS_THREADS_X_CANCELLED;
            break;
        case nca_s_fault_addr_error:
            p_exception = &exc_e_illaddr;
            break;
        case nca_s_fault_fp_div_zero:
            p_exception = &exc_e_fltdiv;
            break;
        case nca_s_fault_fp_error:
            p_exception = &exc_e_aritherr;
            break;
        case nca_s_fault_fp_overflow:
            p_exception = &exc_e_fltovf;
            break;
        case nca_s_fault_fp_underflow:
            p_exception = &exc_e_fltund;
            break;
        case nca_s_fault_ill_inst:
            p_exception = &exc_e_illinstr;
            break;
        case nca_s_fault_int_div_by_zero:
            p_exception = &exc_e_intdiv;
            break;
        case nca_s_fault_int_overflow:
            p_exception = &exc_e_intovf;
            break;
        case nca_s_fault_invalid_bound:
            p_exception = &rpc_x_invalid_bound;
            break;
        case nca_s_fault_invalid_tag:
            p_exception = &rpc_x_invalid_tag;
            break;
        case nca_s_fault_pipe_closed:
            p_exception = &rpc_x_ss_pipe_closed;
            break;
        case nca_s_fault_pipe_comm_error:
            p_exception = &rpc_x_ss_pipe_comm_error;
            break;
        case nca_s_fault_pipe_discipline:
            p_exception = &rpc_x_ss_pipe_discipline_error;
            break;
        case nca_s_fault_pipe_empty:
            p_exception = &rpc_x_ss_pipe_empty;
            break;
        case nca_s_fault_pipe_memory:
            p_exception = &rpc_x_ss_pipe_memory;
            break;
        case nca_s_fault_pipe_order:
            p_exception = &rpc_x_ss_pipe_order;
            break;
        case nca_s_fault_remote_comm_failure:
            p_exception = &rpc_x_ss_remote_comm_failure;
            break;
        case nca_s_fault_remote_no_memory:
            p_exception = &rpc_x_ss_remote_no_memory;
            break;
        default:
            p_exception = &rpc_x_unknown_remote_fault;
            break;
    }
    RPC_SS_THREADS_RESTORE_ASYNC( async_cancel_state );
    RAISE( *p_exception );
}

/******************************************************************************/
/*                                                                            */
/*    Convert contents of fault packet to local error code                    */
/*                                                                            */
/******************************************************************************/
static error_status_t rpc_ss_map_fault_code
(
    ndr_ulong_int fault_code
)
{
    switch (fault_code) {
        case nca_s_fault_addr_error:
            return( rpc_s_fault_addr_error );
        case nca_s_fault_context_mismatch:
            return( rpc_s_fault_context_mismatch );
        case nca_s_fault_cancel:
            return( rpc_s_call_cancelled );
        case nca_s_fault_fp_div_zero:
            return( rpc_s_fault_fp_div_by_zero );
        case nca_s_fault_fp_error:
            return( rpc_s_fault_fp_error );
        case nca_s_fault_fp_overflow:
            return( rpc_s_fault_fp_overflow );
        case nca_s_fault_fp_underflow:
            return( rpc_s_fault_fp_underflow );
        case nca_s_fault_ill_inst:
            return( rpc_s_fault_ill_inst );
        case nca_s_fault_int_div_by_zero:
            return( rpc_s_fault_int_div_by_zero );
        case nca_s_fault_int_overflow:
            return( rpc_s_fault_int_overflow );
        case nca_s_fault_invalid_bound:
            return( rpc_s_fault_invalid_bound );
        case nca_s_fault_invalid_tag:
            return( rpc_s_fault_invalid_tag );
        case nca_s_fault_pipe_closed:
            return( rpc_s_fault_pipe_closed );
        case nca_s_fault_pipe_comm_error:
            return( rpc_s_fault_pipe_comm_error );
        case nca_s_fault_pipe_discipline:
            return( rpc_s_fault_pipe_discipline );
        case nca_s_fault_pipe_empty:
            return( rpc_s_fault_pipe_empty );
        case nca_s_fault_pipe_memory:
            return( rpc_s_fault_pipe_memory );
        case nca_s_fault_pipe_order:
            return( rpc_s_fault_pipe_order );
        case nca_s_fault_remote_comm_failure:
            return( rpc_s_fault_remote_comm_failure );
        case nca_s_fault_remote_no_memory:
            return( rpc_s_fault_remote_no_memory );
        case nca_s_fault_user_defined:
            return(rpc_s_fault_user_defined);
        default:
            return( rpc_s_fault_unspec );
    }
}

/******************************************************************************/
/*                                                                            */
/*    Convert error code to exception                                         */
/*                                                                            */
/******************************************************************************/
static void rpc_ss_raise_impl_exception
(
    ndr_ulong_int result_code,
    RPC_SS_THREADS_CANCEL_STATE_T async_cancel_state
)
{
    EXCEPTION *p_exception;

    switch (result_code) {
        case rpc_s_assoc_grp_not_found:
            p_exception = &rpc_x_assoc_grp_not_found;
            break;
        case rpc_s_call_timeout:
            p_exception = &rpc_x_call_timeout;
            break;
        case rpc_s_cancel_timeout:
            p_exception = &rpc_x_cancel_timeout;
            break;
        case rpc_s_coding_error:
            p_exception = &rpc_x_coding_error;
            break;
        case rpc_s_comm_failure:
            p_exception = &rpc_x_comm_failure;
            break;
        case rpc_s_context_id_not_found:
            p_exception = &rpc_x_context_id_not_found;
            break;
        case rpc_s_endpoint_not_found:
            p_exception = &rpc_x_endpoint_not_found;
            break;
        case rpc_s_in_args_too_big:
            p_exception = &rpc_x_in_args_too_big;
            break;
        case rpc_s_invalid_binding:
            p_exception = &rpc_x_invalid_binding;
            break;
        case rpc_s_invalid_call_opt:
            p_exception = &rpc_x_invalid_call_opt;
            break;
        case rpc_s_invalid_naf_id:
            p_exception = &rpc_x_invalid_naf_id;
            break;
        case rpc_s_invalid_rpc_protseq:
            p_exception = &rpc_x_invalid_rpc_protseq;
            break;
        case rpc_s_invalid_timeout:
            p_exception = &rpc_x_invalid_timeout;
            break;
        case rpc_s_manager_not_entered:
            p_exception = &rpc_x_manager_not_entered;
            break;
        case rpc_s_max_descs_exceeded:
            p_exception = &rpc_x_max_descs_exceeded;
            break;
        case rpc_s_no_fault:
            p_exception = &rpc_x_no_fault;
            break;
        case rpc_s_no_memory:
            p_exception = &rpc_x_no_memory;
            break;
        case rpc_s_not_rpc_tower:
            p_exception = &rpc_x_not_rpc_tower;
            break;
        case rpc_s_object_not_found:
            p_exception = &rpc_x_object_not_found;
            break;
        case rpc_s_op_rng_error:
            p_exception = &rpc_x_op_rng_error;
            break;
        case rpc_s_protocol_error:
            p_exception = &rpc_x_protocol_error;
            break;
        case rpc_s_protseq_not_supported:
            p_exception = &rpc_x_protseq_not_supported;
            break;
        case rpc_s_rpcd_comm_failure:
            p_exception = &rpc_x_rpcd_comm_failure;
            break;
        case rpc_s_unknown_if:
            p_exception = &rpc_x_unknown_if;
            break;
        case rpc_s_unknown_mgr_type:
            p_exception = &rpc_x_unknown_mgr_type;
            break;
        case rpc_s_unknown_reject:
            p_exception = &rpc_x_unknown_reject;
            break;
        case rpc_s_unsupported_type:
            p_exception = &rpc_x_unsupported_type;
            break;
        case rpc_s_who_are_you_failed:
            p_exception = &rpc_x_who_are_you_failed;
            break;
        case rpc_s_wrong_boot_time:
            p_exception = &rpc_x_wrong_boot_time;
            break;
        case rpc_s_wrong_kind_of_binding:
            p_exception = &rpc_x_wrong_kind_of_binding;
            break;
        case uuid_s_getconf_failure:
            p_exception = &uuid_x_getconf_failure;
            break;
        case uuid_s_internal_error:
            p_exception = &uuid_x_internal_error;
            break;
         case uuid_s_no_address:
            p_exception = &uuid_x_no_address;
            break;
        case uuid_s_socket_failure:
            p_exception = &uuid_x_socket_failure;
            break;
       /* CN errors */
        case rpc_s_access_control_info_inv:
            p_exception = &rpc_x_access_control_info_inv;
            break;
        case rpc_s_assoc_grp_max_exceeded:
            p_exception = &rpc_x_assoc_grp_max_exceeded;
            break;
        case rpc_s_assoc_shutdown:
            p_exception = &rpc_x_assoc_shutdown;
            break;
        case rpc_s_cannot_accept:
            p_exception = &rpc_x_cannot_accept;
            break;
        case rpc_s_cannot_connect:
            p_exception = &rpc_x_cannot_connect;
            break;
        case rpc_s_cannot_set_nodelay:
            p_exception = &rpc_x_cannot_set_nodelay;
            break;
        case rpc_s_cant_inq_socket:
            p_exception = &rpc_x_cant_inq_socket;
            break;
        case rpc_s_connect_closed_by_rem:
            p_exception = &rpc_x_connect_closed_by_rem;
            break;
        case rpc_s_connect_no_resources:
            p_exception = &rpc_x_connect_no_resources;
            break;
        case rpc_s_connect_rejected:
            p_exception = &rpc_x_connect_rejected;
            break;
        case rpc_s_connect_timed_out:
            p_exception = &rpc_x_connect_timed_out;
            break;
        case rpc_s_connection_aborted:
            p_exception = &rpc_x_connection_aborted;
            break;
        case rpc_s_connection_closed:
            p_exception = &rpc_x_connection_closed;
            break;
        case rpc_s_host_unreachable:
            p_exception = &rpc_x_host_unreachable;
            break;
        case rpc_s_invalid_endpoint_format:
            p_exception = &rpc_x_invalid_endpoint_format;
            break;
        case rpc_s_loc_connect_aborted:
            p_exception = &rpc_x_loc_connect_aborted;
            break;
        case rpc_s_network_unreachable:
            p_exception = &rpc_x_network_unreachable;
            break;
        case rpc_s_no_rem_endpoint:
            p_exception = &rpc_x_no_rem_endpoint;
            break;
        case rpc_s_rem_host_crashed:
            p_exception = &rpc_x_rem_host_crashed;
            break;
        case rpc_s_rem_host_down:
            p_exception = &rpc_x_rem_host_down;
            break;
        case rpc_s_rem_network_shutdown:
            p_exception = &rpc_x_rem_network_shutdown;
            break;
        case rpc_s_rpc_prot_version_mismatch:
            p_exception = &rpc_x_rpc_prot_version_mismatch;
            break;
        case rpc_s_string_too_long:
            p_exception = &rpc_x_string_too_long;
            break;
        case rpc_s_too_many_rem_connects:
            p_exception = &rpc_x_too_many_rem_connects;
            break;
        case rpc_s_tsyntaxes_unsupported:
            p_exception = &rpc_x_tsyntaxes_unsupported;
            break;
        /* NS import routine errors */
        case rpc_s_binding_vector_full:
            p_exception = &rpc_x_binding_vector_full;
            break;
        case rpc_s_entry_not_found:
            p_exception = &rpc_x_entry_not_found;
            break;
        case rpc_s_group_not_found:
            p_exception = &rpc_x_group_not_found;
            break;
        case rpc_s_incomplete_name:
            p_exception = &rpc_x_incomplete_name;
            break;
        case rpc_s_invalid_arg:
            p_exception = &rpc_x_invalid_arg;
            break;
        case rpc_s_invalid_import_context:
            p_exception = &rpc_x_invalid_import_context;
            break;
        case rpc_s_invalid_inquiry_context:
            p_exception = &rpc_x_invalid_inquiry_context;
            break;
        case rpc_s_invalid_inquiry_type:
            p_exception = &rpc_x_invalid_inquiry_type;
            break;
        case rpc_s_invalid_lookup_context:
            p_exception = &rpc_x_invalid_lookup_context;
            break;
        case rpc_s_invalid_name_syntax:
            p_exception = &rpc_x_invalid_name_syntax;
            break;
        case rpc_s_invalid_object:
            p_exception = &rpc_x_invalid_object;
            break;
        case rpc_s_invalid_vers_option:
            p_exception = &rpc_x_invalid_vers_option;
            break;
        case rpc_s_name_service_unavailable:
            p_exception = &rpc_x_name_service_unavailable;
            break;
        case rpc_s_no_env_setup:
            p_exception = &rpc_x_no_env_setup;
            break;
        case rpc_s_no_more_bindings:
            p_exception = &rpc_x_no_more_bindings;
            break;
        case rpc_s_no_more_elements:
            p_exception = &rpc_x_no_more_elements;
            break;
        case rpc_s_no_ns_permission:
            p_exception = &rpc_x_no_ns_permission;
            break;
        case rpc_s_not_found:
            p_exception = &rpc_x_not_found;
            break;
        case rpc_s_not_rpc_entry:
            p_exception = &rpc_x_not_rpc_entry;
            break;
        case rpc_s_obj_uuid_not_found:
            p_exception = &rpc_x_obj_uuid_not_found;
            break;
        case rpc_s_profile_not_found:
            p_exception = &rpc_x_profile_not_found;
            break;
        case rpc_s_unsupported_name_syntax:
            p_exception = &rpc_x_unsupported_name_syntax;
            break;
        /* Authentication errors */
        case rpc_s_auth_bad_integrity:
            p_exception = &rpc_x_auth_bad_integrity;
            break;
        case rpc_s_auth_badaddr:
            p_exception = &rpc_x_auth_badaddr;
            break;
        case rpc_s_auth_baddirection:
            p_exception = &rpc_x_auth_baddirection;
            break;
        case rpc_s_auth_badkeyver:
            p_exception = &rpc_x_auth_badkeyver;
            break;
        case rpc_s_auth_badmatch:
            p_exception = &rpc_x_auth_badmatch;
            break;
        case rpc_s_auth_badorder:
            p_exception = &rpc_x_auth_badorder;
            break;
        case rpc_s_auth_badseq:
            p_exception = &rpc_x_auth_badseq;
            break;
        case rpc_s_auth_badversion:
            p_exception = &rpc_x_auth_badversion;
            break;
        case rpc_s_auth_field_toolong:
            p_exception = &rpc_x_auth_field_toolong;
            break;
        case rpc_s_auth_inapp_cksum:
            p_exception = &rpc_x_auth_inapp_cksum;
            break;
        case rpc_s_auth_method:
            p_exception = &rpc_x_auth_method;
            break;
        case rpc_s_auth_msg_type:
            p_exception = &rpc_x_auth_msg_type;
            break;
        case rpc_s_auth_modified:
            p_exception = &rpc_x_auth_modified;
            break;
        case rpc_s_auth_mut_fail:
            p_exception = &rpc_x_auth_mut_fail;
            break;
        case rpc_s_auth_nokey:
            p_exception = &rpc_x_auth_nokey;
            break;
        case rpc_s_auth_not_us:
            p_exception = &rpc_x_auth_not_us;
            break;
        case rpc_s_auth_repeat:
            p_exception = &rpc_x_auth_repeat;
            break;
        case rpc_s_auth_skew:
            p_exception = &rpc_x_auth_skew;
            break;
        case rpc_s_auth_tkt_expired:
            p_exception = &rpc_x_auth_tkt_expired;
            break;
        case rpc_s_auth_tkt_nyv:
            p_exception = &rpc_x_auth_tkt_nyv;
            break;
        case rpc_s_call_id_not_found:
            p_exception = &rpc_x_call_id_not_found;
            break;
        case rpc_s_credentials_too_large:
            p_exception = &rpc_x_credentials_too_large;
            break;
        case rpc_s_invalid_checksum:
            p_exception = &rpc_x_invalid_checksum;
            break;
        case rpc_s_invalid_crc:
            p_exception = &rpc_x_invalid_crc;
            break;
        case rpc_s_invalid_credentials:
            p_exception = &rpc_x_invalid_credentials;
            break;
        case rpc_s_key_id_not_found:
            p_exception = &rpc_x_key_id_not_found;
            break;
        /* Other pickling errors */
        case rpc_s_ss_bad_buffer:
            p_exception = &rpc_x_ss_bad_buffer;
            break;
        case rpc_s_ss_bad_es_action:
            p_exception = &rpc_x_ss_bad_es_action;
            break;
        case rpc_s_ss_wrong_es_version:
            p_exception = &rpc_x_ss_wrong_es_version;
            break;
        case rpc_s_ss_incompatible_codesets:
            p_exception = &rpc_x_ss_incompatible_codesets;
            break;
        case rpc_s_stub_protocol_error:
            p_exception = &rpc_x_stub_protocol_error;
            break;
        case rpc_s_unknown_stub_rtl_if_vers:
            p_exception = &rpc_x_unknown_stub_rtl_if_vers;
            break;
        default:
            {
            EXCEPTION unknown_status_exception;
            EXCEPTION_INIT(unknown_status_exception);
            exc_set_status(&unknown_status_exception, result_code);
            RPC_SS_THREADS_RESTORE_ASYNC( async_cancel_state );
            RAISE( unknown_status_exception );
            p_exception = NULL;
            break;
            }
    }
    RPC_SS_THREADS_RESTORE_ASYNC( async_cancel_state );
    RAISE( *p_exception );
}

#ifdef MIA
/******************************************************************************/
/*                                                                            */
/*    Report error status to the caller                                       */
/*    New version - user exceptions                                           */
/*                                                                            */
/******************************************************************************/
void rpc_ss_report_error_2
(
    ndr_ulong_int fault_code,
    ndr_ulong_int user_fault_id,
    ndr_ulong_int result_code,
    RPC_SS_THREADS_CANCEL_STATE_T *p_async_cancel_state,
    error_status_t *p_comm_status,
    error_status_t *p_fault_status,
    EXCEPTION *user_exception_pointers[],
    IDL_msp_t IDL_msp __attribute((unused))
)
{
    if (p_comm_status != NULL) *p_comm_status = error_status_ok;
    if (p_fault_status != NULL) *p_fault_status = error_status_ok;

    if (fault_code != error_status_ok)
    {
        if ( p_fault_status == NULL )
        {
            if (fault_code == nca_s_fault_user_defined)
            {
                RPC_SS_THREADS_RESTORE_ASYNC( *p_async_cancel_state );
                RAISE( *(user_exception_pointers[user_fault_id]) );
            }
            rpc_ss_raise_arch_exception( fault_code, *p_async_cancel_state );
        }
        else
        {
            *p_fault_status = rpc_ss_map_fault_code( fault_code );
            return;
        }
    }
    else if (result_code != error_status_ok)
    {
        if ( p_comm_status == NULL )
            rpc_ss_raise_impl_exception( result_code, *p_async_cancel_state );
        else
        {
            *p_comm_status = result_code;
            return;
        }
    }
}
#endif

/******************************************************************************/
/*                                                                            */
/*    Report error status to the caller                                       */
/*    Old version - no user exceptions                                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_report_error
(
    ndr_ulong_int fault_code,
    ndr_ulong_int result_code,
    RPC_SS_THREADS_CANCEL_STATE_T async_cancel_state,
    error_status_t *p_comm_status,
    error_status_t *p_fault_status
)
{
    rpc_ss_report_error_2(fault_code, 0, result_code, &async_cancel_state,
                            p_comm_status, p_fault_status, NULL, NULL);
}

/******************************************************************************/
/*                                                                            */
/*  If there is a fault, get the fault packet. Then end the call              */
/*  New interface - user exceptions                                           */
/*                                                                            */
/******************************************************************************/
void rpc_ss_call_end_2
(
    volatile rpc_call_handle_t *p_call_h,
    volatile ndr_ulong_int *p_fault_code,
    volatile ndr_ulong_int *p_user_fault_id,
    volatile error_status_t *p_st
)
{
    rpc_iovector_elt_t iovec_elt;
    ndr_format_t drep;
    error_status_t status;
    rpc_mp_t mp;

/*    *p_fault_code = error_status_ok; Initialization done by stub */
    if ( *p_st == rpc_s_call_faulted )
    {
        rpc_call_receive_fault((rpc_call_handle_t)*p_call_h, &iovec_elt, &drep,
                                 &status );
        if (status == error_status_ok)
        {
            rpc_init_mp(mp, iovec_elt.data_addr);
            rpc_convert_ulong_int(drep, ndr_g_local_drep, mp, (*p_fault_code));
            if (*p_fault_code == nca_s_fault_user_defined)
            {
                rpc_advance_mp(mp, 4);  /* Next longword represents user
                                                                    exception */
                rpc_convert_ulong_int(drep, ndr_g_local_drep, mp, 
                                                    (*p_user_fault_id));
            }
            if (iovec_elt.buff_dealloc != NULL)
            {
                (*iovec_elt.buff_dealloc)(iovec_elt.buff_addr);
                iovec_elt.buff_dealloc = NULL;
            }
            
            /*
             * Remote comm failures are reported by a fault packet.  However,
             * we want to treat them in the same way as a local comm failure,
             * not as a fault.  We do the translation here.
             */
            if (*p_fault_code == nca_s_fault_remote_comm_failure)
            {
                *p_st = rpc_s_fault_remote_comm_failure;
                *p_fault_code = error_status_ok;
            }
        }
        else *p_st = status;
    }
    if ( *p_call_h != NULL )
    {
        rpc_call_end( (rpc_call_handle_t*) p_call_h, &status);
        /* Don't destroy any existing error code with the value from call end */
        if ( *p_st == error_status_ok ) *p_st = status;
    }
}

/******************************************************************************/
/*                                                                            */
/*  If there is a fault, get the fault packet. Then end the call              */
/*  Old interface - no user exceptions                                        */
/*                                                                            */
/******************************************************************************/
void rpc_ss_call_end
(
    volatile rpc_call_handle_t *p_call_h,
    volatile ndr_ulong_int *p_fault_code,
    volatile error_status_t *p_st
)
{
    ndr_ulong_int user_fault_id;    /* Discarded argument */

    rpc_ss_call_end_2(p_call_h, p_fault_code, &user_fault_id, p_st);
}

/******************************************************************************/
/*                                                                            */
/*  Optimization support routine - change receive buffer                      */
/*                                                                            */
/******************************************************************************/
void rpc_ss_new_recv_buff
(
    rpc_iovector_elt_t *elt,
    rpc_call_handle_t call_h,
    rpc_mp_t *p_mp,
    volatile error_status_t *st
)
{
    if (elt->buff_dealloc && (elt->data_len != 0))
    {
        (*elt->buff_dealloc)(elt->buff_addr);
        elt->buff_dealloc = NULL;
    }

    rpc_call_receive(call_h, elt, (unsigned32*)st);
    if (*st == error_status_ok)
    {
        if (elt->data_addr != NULL)
        {
            rpc_init_mp((*p_mp), elt->data_addr);
            return;
        }
        else
            *st = rpc_s_stub_protocol_error;
    }
    {
        /* If cancelled, raise the cancelled exception */
        if (*st==rpc_s_call_cancelled) RAISE(RPC_SS_THREADS_X_CANCELLED);

        /*
         *  Otherwise, raise the pipe comm error which causes the stub to
         *  report the value of the status variable.
         */
        RAISE(rpc_x_ss_pipe_comm_error);
    }
}
