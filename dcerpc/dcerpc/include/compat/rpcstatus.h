/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#if defined(__GNUC__)

#if !defined(RPCSTATUS_H)
#define RPCSTATUS_H

#include <dce/rpcbase.h>
#include <dce/rpcsts.h>

/*
 * This is mapping (as close as possible) of MS/RPC return statuses
 * to DCE/RPC return codes
 */

#define RPC_S_OK                           rpc_s_ok
#define RPC_S_MOD                          rpc_s_mod
#define RPC_S_OP_RNG_ERROR                 rpc_s_op_rng_error
#define RPC_S_CANT_CREATE_SOCKET           rpc_s_cant_create_socket
#define RPC_S_CANT_BIND_SOCKET             rpc_s_cant_bind_socket
#define RPC_S_NOT_IN_CALL                  rpc_s_not_in_call
#define RPC_S_NO_PORT                      rpc_s_no_port
#define RPC_S_WRONG_BOOT_TIME              rpc_s_wrong_boot_time
#define RPC_S_TOO_MANY_SOCKETS             rpc_s_too_many_sockets
#define RPC_S_ILLEGAL_REGISTER             rpc_s_illegal_register
#define RPC_S_CANT_RECV                    rpc_s_cant_recv
#define RPC_S_BAD_PKT                      rpc_s_bad_pkt
#define RPC_S_UNBOUND_HANDLE               rpc_s_unbound_handle
#define RPC_S_ADDR_IN_USE                  rpc_s_addr_in_use
#define RPC_S_IN_ARGS_TOO_BIG              rpc_s_in_args_too_big
#define RPC_S_STRING_TOO_LONG              rpc_s_string_too_long
#define RPC_S_TOO_MANY_OBJECTS             rpc_s_too_many_objects
#define RPC_S_BINDING_HAS_NO_AUTH          rpc_s_binding_has_no_auth
#define RPC_S_UNKNOWN_AUTHN_SERVICE        rpc_s_unknown_authn_service
#define RPC_S_OUT_OF_MEMORY                rpc_s_no_memory
#define RPC_S_CANT_NMALLOC                 rpc_s_cant_nmalloc
#define RPC_S_CALL_FAULTED                 rpc_s_call_faulted
#define RPC_S_CALL_FAILED                  rpc_s_call_failed
#define RPC_S_COMM_FAILURE                 rpc_s_comm_failure
#define RPC_S_RPCD_COMM_FAILURE            rpc_s_rpcd_comm_failure
#define RPC_S_ILLEGAL_FAMILY_REBIND        rpc_s_illegal_family_rebind
#define RPC_S_INVALID_HANDLE               rpc_s_invalid_handle
#define RPC_S_CODING_ERROR                 rpc_s_coding_error
#define RPC_S_OBJECT_NOT_FOUND             rpc_s_object_not_found
#define RPC_S_CTHREAD_NOT_FOUND            rpc_s_cthread_not_found
#define RPC_S_INVALID_BINDING              rpc_s_invalid_binding
#define RPC_S_ALREADY_REGISTERED           rpc_s_already_registered
#define RPC_S_ENDPOINT_NOT_FOUND           rpc_s_endpoint_not_found
#define RPC_S_INVALID_RPC_PROTSEQ          rpc_s_invalid_rpc_protseq
#define RPC_S_DESC_NOT_REGISTERED          rpc_s_desc_not_registered
#define RPC_S_ALREADY_LISTENING            rpc_s_already_listening
#define RPC_S_NO_PROTSEQS                  rpc_s_no_protseqs
#define RPC_S_NO_PROTSEQS_REGISTERED       rpc_s_no_protseqs_registered
#define RPC_S_NO_BINDINGS                  rpc_s_no_bindings
#define RPC_S_MAX_DESCS_EXCEEDED           rpc_s_max_descs_exceeded
#define RPC_S_NO_INTERFACES                rpc_s_no_interfaces
#define RPC_S_INVALID_TIMEOUT              rpc_s_invalid_timeout
#define RPC_S_CANT_INQ_SOCKET              rpc_s_cant_inq_socket
#define RPC_S_INVALID_NAF_ID               rpc_s_invalid_naf_id
#define RPC_S_INVALID_NET_ADDR             rpc_s_inval_net_addr
#define RPC_S_UNKNOWN_IF                   rpc_s_unknown_if
#define RPC_S_UNSUPPORTED_TYPE             rpc_s_unsupported_type
#define RPC_S_INVALID_CALL_OPT             rpc_s_invalid_call_opt
#define RPC_S_NO_FAULT                     rpc_s_no_fault
#define RPC_S_CANCEL_TIMEOUT               rpc_s_cancel_timeout
#define RPC_S_CALL_CANCELLED               rpc_s_call_cancelled
#define RPC_S_INVALID_CALL_HANDLE          rpc_s_invalid_call_handle
#define RPC_S_CANNOT_ALLOC_ASSOC           rpc_s_cannot_alloc_assoc
#define RPC_S_CANNOT_CONNECT               rpc_s_cannot_connect
#define RPC_S_CONNECTION_ABORTED           rpc_s_connection_aborted
#define RPC_S_CONNECTION_CLOSED            rpc_s_connection_closed
#define RPC_S_CANNOT_ACCEPT                rpc_s_cannot_accept
#define RPC_S_ASSOC_GRP_NOT_FOUND          rpc_s_assoc_grp_not_found
#define RPC_S_STUB_INTERFACE_ERROR         rpc_s_stub_interface_error
#define RPC_S_INVALID_OBJECT               rpc_s_invalid_object
#define RPC_S_INVALID_TYPE                 rpc_s_invalid_type
#define RPC_S_INVALID_IF_OPNUM             rpc_s_invalid_if_opnum
#define RPC_S_DIFFERENT_SERVER_INSTANCE    rpc_s_different_server_instance
#define RPC_S_PROTOCOL_ERROR               rpc_s_protocol_error
#define RPC_S_CANT_RECVMSG                 rpc_s_cant_recvmsg
#define RPC_S_INVALID_STRING_BINDING       rpc_s_invalid_string_binding
#define RPC_S_CONNECT_TIMED_OUT            rpc_s_connect_timed_out
#define RPC_S_CONNECT_REJECTED             rpc_s_connect_rejected
#define RPC_S_NETWORK_UNREACHABLE          rpc_s_network_unreachable
#define RPC_S_CONNECT_NO_RESOURCES         rpc_s_connect_no_resources
#define RPC_S_REM_NETWORK_SHUTDOWN         rpc_s_rem_network_shutdown
#define RPC_S_TOO_MANY_REM_CONNECTS        rpc_s_too_many_rem_connects
#define RPC_S_NO_REM_ENDPOINT              rpc_s_no_rem_endpoint
#define RPC_S_REM_HOST_DOWN                rpc_s_rem_host_down
#define RPC_S_HOST_UNREACHABLE             rpc_s_host_unreachable
#define RPC_S_ACCESS_CONTROL_INFO_INV      rpc_s_access_control_info_inv
#define RPC_S_LOC_CONNECT_ABORTED          rpc_s_loc_connect_aborted
#define RPC_S_CONNECT_CLOSED_BY_REM        rpc_s_connect_closed_by_rem
#define RPC_S_REM_HOST_CRASHED             rpc_s_rem_host_crashed
#define RPC_S_INVALID_ENDPOINT_FORMAT      rpc_s_invalid_endpoint_format
#define RPC_S_UNKNOWN_STATUS_CODE          rpc_s_unknown_status_code
#define RPC_S_UNKNOWN_MGR_TYPE             rpc_s_unknown_mgr_type
#define RPC_S_ASSOC_CREATION_FAILED        rpc_s_assoc_creation_failed
#define RPC_S_ASSOC_GRP_MAX_EXCEEDED       rpc_s_assoc_grp_max_exceeded
#define RPC_S_ASSOC_GRP_ALLOC_FAILED       rpc_s_assoc_grp_alloc_failed
#define RPC_S_SM_INVALID_STATE             rpc_s_sm_invalid_state
#define RPC_S_ASSOC_REQ_REJECTED           rpc_s_assoc_req_rejected
#define RPC_S_ASSOC_SHUTDOWN               rpc_s_assoc_shutdown
#define RPC_S_TSYNTAXES_UNSUPPORTED        rpc_s_tsyntaxes_unsupported
#define RPC_S_CONTEXT_ID_NOT_FOUND         rpc_s_context_id_not_found
#define RPC_S_CANT_LISTEN_SOCKET           rpc_s_cant_listen_socket
#define RPC_S_NO_ADDRS                     rpc_s_no_addrs
#define RPC_S_CANT_GETPEERNAME             rpc_s_cant_getpeername
#define RPC_S_CANT_GET_IF_ID               rpc_s_cant_get_if_id
#define RPC_S_PROTSEQ_NOT_SUPPORTED        rpc_s_protseq_not_supported
#define RPC_S_CALL_ORPHANED                rpc_s_call_orphaned
#define RPC_S_WHO_ARE_YOU_FAILED           rpc_s_who_are_you_failed
#define RPC_S_UNKNOWN_REJECT               rpc_s_unknown_reject
#define RPC_S_TYPE_ALREADY_REGISTERED      rpc_s_type_already_registered
#define RPC_S_STOP_LISTENING_DISABLED      rpc_s_stop_listening_disabled
#define RPC_S_INVALID_ARG                  rpc_s_invalid_arg
#define RPC_S_NOT_SUPPORTED                rpc_s_not_supported
#define RPC_S_WRONG_KIND_OF_BINDING        rpc_s_wrong_kind_of_binding
#define RPC_S_AUTHN_AUTHZ_MISMATCH         rpc_s_authn_authz_mismatch
#define RPC_S_CALL_QUEUED                  rpc_s_call_queued
#define RPC_S_CANNOT_SET_NODELAY           rpc_s_cannot_set_nodelay
#define RPC_S_NOT_RPC_TOWER                rpc_s_not_rpc_tower
#define RPC_S_INVALID_RPC_PROTID           rpc_s_invalid_rpc_protid
#define RPC_S_INVALID_RPC_FLOOR            rpc_s_invalid_rpc_floor
#define RPC_S_CALL_TIMEOUT                 rpc_s_call_timeout
#define RPC_S_MGMT_OP_DISALLOWED           rpc_s_mgmt_op_disallowed
#define RPC_S_MANAGER_NOT_ENTERED          rpc_s_manager_not_entered
#define RPC_S_CALLS_TOO_LARGE_FOR_WK_EP    rpc_s_calls_too_large_for_wk_ep
#define RPC_S_SERVER_TOO_BUSY              rpc_s_server_too_busy
#define RPC_S_PROT_VERSION_MISMATCH        rpc_s_prot_version_mismatch
#define RPC_S_RPC_PROT_VERSION_MISMATCH    rpc_s_rpc_prot_version_mismatch
#define RPC_S_SS_NO_IMPORT_CURSOR          rpc_s_ss_no_import_cursor
#define RPC_S_FAULT_ADDR_ERROR             rpc_s_fault_addr_error
#define RPC_S_FAULT_CONTEXT_MISMATCH       rpc_s_fault_context_mismatch
#define RPC_S_FAULT_FP_DIV_BY_ZERO         rpc_s_fault_fp_div_by_zero
#define RPC_S_FAULT_FP_ERROR               rpc_s_fault_fp_error
#define RPC_S_FAULT_FP_OVERFLOW            rpc_s_fault_fp_overflow
#define RPC_S_FAULT_FP_UNDERFLOW           rpc_s_fault_fp_underflow
#define RPC_S_FAULT_ILL_INST               rpc_s_fault_ill_inst
#define RPC_S_FAULT_INT_DIV_BY_ZERO        rpc_s_fault_int_div_by_zero
#define RPC_S_FAULT_INT_OVERFLOW           rpc_s_fault_int_overflow
#define RPC_S_FAULT_INVALID_BOUND          rpc_s_fault_invalid_bound
#define RPC_S_FAULT_INVALID_TAG            rpc_s_fault_invalid_tag
#define RPC_S_FAULT_PIPE_CLOSED            rpc_s_fault_pipe_closed
#define RPC_S_FAULT_PIPE_COMM_ERROR        rpc_s_fault_pipe_comm_error
#define RPC_S_FAULT_PIPE_DISCIPLINE        rpc_s_fault_pipe_discipline
#define RPC_S_FAULT_PIPE_EMPTY             rpc_s_fault_pipe_empty
#define RPC_S_FAULT_PIPE_MEMORY            rpc_s_fault_pipe_memory
#define RPC_S_FAULT_PIPE_ORDER             rpc_s_fault_pipe_order
#define RPC_S_FAULT_REMOTE_COMM_FAILURE    rpc_s_fault_remote_comm_failure
#define RPC_S_FAULT_REMOTE_NO_MEMORY       rpc_s_fault_remote_no_memory
#define RPC_S_FAULT_UNSPEC                 rpc_s_fault_unspec
#define UUID_S_BAD_VERSION                 uuid_s_bad_version
#define UUID_S_SOCKET_FAILURE              uuid_s_socket_failure
#define UUID_S_GETCONF_FAILURE             uuid_s_getconf_failure
#define UUID_S_NO_ADDRESS                  uuid_s_no_address
#define UUID_S_OVERRUN                     uuid_s_overrun
#define UUID_S_INTERNAL_ERROR              uuid_s_internal_error
#define UUID_S_CODING_ERROR                uuid_s_coding_error
#define UUID_S_INVALID_STRING_UUID         uuid_s_invalid_string_uuid
#define UUID_S_NO_MEMORY                   uuid_s_no_memory
#define RPC_S_NO_MORE_ENTRIES              rpc_s_no_more_entries
#define RPC_S_UNKNOWN_NS_ERROR             rpc_s_unknown_ns_error
#define RPC_S_NAME_SERVICE_UNAVAILABLE     rpc_s_name_service_unavailable
#define RPC_S_INCOMPLETE_NAME              rpc_s_incomplete_name
#define RPC_S_GROUP_NOT_FOUND              rpc_s_group_not_found
#define RPC_S_INVALID_NAME_SYNTAX          rpc_s_invalid_name_syntax
#define RPC_S_NO_MORE_MEMBERS              rpc_s_no_more_members
#define RPC_S_NO_MORE_INTERFACES           rpc_s_no_more_interfaces
#define RPC_S_INVALID_NAME_SERVICE         rpc_s_invalid_name_service
#define RPC_S_NO_NAME_MAPPING              rpc_s_no_name_mapping
#define RPC_S_PROFILE_NOT_FOUND            rpc_s_profile_not_found
#define RPC_S_NOT_FOUND                    rpc_s_not_found
#define RPC_S_NO_UPDATES                   rpc_s_no_updates
#define RPC_S_UPDATE_FAILED                rpc_s_update_failed
#define RPC_S_NO_MATCH_EXPORTED            rpc_s_no_match_exported
#define RPC_S_ENTRY_NOT_FOUND              rpc_s_entry_not_found
#define RPC_S_INVALID_INQUIRY_CONTEXT      rpc_s_invalid_inquiry_context
#define RPC_S_INTERFACE_NOT_FOUND          rpc_s_interface_not_found
#define RPC_S_GROUP_MEMBER_NOT_FOUND       rpc_s_group_member_not_found
#define RPC_S_ENTRY_ALREADY_EXISTS         rpc_s_entry_already_exists
#define RPC_S_NSINIT_FAILURE               rpc_s_nsinit_failure
#define RPC_S_UNSUPPORTED_NAME_SYNTAX      rpc_s_unsupported_name_syntax
#define RPC_S_NO_MORE_ELEMENTS             rpc_s_no_more_elements
#define RPC_S_NO_NS_PERMISSION             rpc_s_no_ns_permission
#define RPC_S_INVALID_INQUIRY_TYPE         rpc_s_invalid_inquiry_type
#define RPC_S_PROFILE_ELEMENT_NOT_FOUND    rpc_s_profile_element_not_found
#define RPC_S_PROFILE_ELEMENT_REPLACED     rpc_s_profile_element_replaced
#define RPC_S_IMPORT_ALREADY_DONE          rpc_s_import_already_done
#define RPC_S_DATABASE_BUSY                rpc_s_database_busy
#define RPC_S_INVALID_IMPORT_CONTEXT       rpc_s_invalid_import_context
#define RPC_S_UUID_SET_NOT_FOUND           rpc_s_uuid_set_not_found
#define RPC_S_UUID_MEMBER_NOT_FOUND        rpc_s_uuid_member_not_found
#define RPC_S_NO_INTERFACES_EXPORTED       rpc_s_no_interfaces_exported
#define RPC_S_TOWER_SET_NOT_FOUND          rpc_s_tower_set_not_found
#define RPC_S_TOWER_MEMBER_NOT_FOUND       rpc_s_tower_member_not_found
#define RPC_S_OBJ_UUID_NOT_FOUND           rpc_s_obj_uuid_not_found
#define RPC_S_NO_MORE_BINDINGS             rpc_s_no_more_bindings
#define RPC_S_INVALID_PRIORITY             rpc_s_invalid_priority
#define RPC_S_NOT_RPC_ENTRY                rpc_s_not_rpc_entry
#define RPC_S_INVALID_LOOKUP_CONTEXT       rpc_s_invalid_lookup_context
#define RPC_S_BINDING_VECTOR_FULL          rpc_s_binding_vector_full
#define RPC_S_CYCLE_DETECTED               rpc_s_cycle_detected
#define RPC_S_NOTHING_TO_EXPORT            rpc_s_nothing_to_export
#define RPC_S_NOTHING_TO_UNEXPORT          rpc_s_nothing_to_unexport
#define RPC_S_INVALID_VERS_OPTION          rpc_s_invalid_vers_option
#define RPC_S_NO_RPC_DATA                  rpc_s_no_rpc_data
#define RPC_S_MBR_PICKED                   rpc_s_mbr_picked
#define RPC_S_NOT_ALL_OBJS_UNEXPORTED      rpc_s_not_all_objs_unexported
#define RPC_S_NO_ENTRY_NAME                rpc_s_no_entry_name
#define RPC_S_PRIORITY_GROUP_DONE          rpc_s_priority_group_done
#define RPC_S_PARTIAL_RESULTS              rpc_s_partial_results
#define RPC_S_NO_ENV_SETUP                 rpc_s_no_env_setup
#define TWR_S_UNKNOWN_SA                   twr_s_unknown_sa
#define TWR_S_UNKNOWN_TOWER                twr_s_unknown_tower
#define TWR_S_NOT_IMPLEMENTED              twr_s_not_implemented
#define RPC_S_MAX_CALLS_TOO_SMALL          rpc_s_max_calls_too_small
#define RPC_S_CTHREAD_CREATE_FAILED        rpc_s_cthread_create_failed
#define RPC_S_CTHREAD_POOL_EXISTS          rpc_s_cthread_pool_exists
#define RPC_S_CTHREAD_NO_SUCH_POOL         rpc_s_cthread_no_such_pool
#define RPC_S_CTHREAD_INVOKE_DISABLED      rpc_s_cthread_invoke_disabled
#define EPT_S_CANT_PERFORM_OP              ept_s_cant_perform_op
#define EPT_S_NO_MEMORY                    ept_s_no_memory
#define EPT_S_DATABASE_INVALID             ept_s_database_invalid
#define EPT_S_CANT_CREATE                  ept_s_cant_create
#define EPT_S_CANT_ACCESS                  ept_s_cant_access
#define EPT_S_DATABASE_ALREADY_OPEN        ept_s_database_already_open
#define EPT_S_INVALID_ENTRY                ept_s_invalid_entry
#define EPT_S_UPDATE_FAILED                ept_s_update_failed
#define EPT_S_INVALID_CONTEXT              ept_s_invalid_context
#define EPT_S_NOT_REGISTERED               ept_s_not_registered
#define EPT_S_SERVER_UNAVAILABLE           ept_s_server_unavailable
#define RPC_S_UNDERSPECIFIED_NAME          rpc_s_underspecified_name
#define RPC_S_INVALID_NS_HANDLE            rpc_s_invalid_ns_handle
#define RPC_S_UNKNOWN_ERROR                rpc_s_unknown_error
#define RPC_S_SS_CHAR_TRANS_OPEN_FAIL      rpc_s_ss_char_trans_open_fail
#define RPC_S_SS_CHAR_TRANS_SHORT_FILE     rpc_s_ss_char_trans_short_file
#define RPC_S_SS_CONTEXT_DAMAGED           rpc_s_ss_context_damaged
#define RPC_S_SS_IN_NULL_CONTEXT           rpc_s_ss_in_null_context
#define RPC_S_SOCKET_FAILURE               rpc_s_socket_failure
#define RPC_S_UNSUPPORTED_PROTECT_LEVEL    rpc_s_unsupported_protect_level
#define RPC_S_INVALID_CHECKSUM             rpc_s_invalid_checksum
#define RPC_S_INVALID_CREDENTIALS          rpc_s_invalid_credentials
#define RPC_S_CREDENTIALS_TOO_LARGE        rpc_s_credentials_too_large
#define RPC_S_CALL_ID_NOT_FOUND            rpc_s_call_id_not_found
#define RPC_S_KEY_ID_NOT_FOUND             rpc_s_key_id_not_found
#define RPC_S_AUTH_BAD_INTEGRITY           rpc_s_auth_bad_integrity
#define RPC_S_AUTH_TKT_EXPIRED             rpc_s_auth_tkt_expired
#define RPC_S_AUTH_TKT_NYV                 rpc_s_auth_tkt_nyv
#define RPC_S_AUTH_REPEAT                  rpc_s_auth_repeat
#define RPC_S_AUTH_NOT_US                  rpc_s_auth_not_us
#define RPC_S_AUTH_BADMATCH                rpc_s_auth_badmatch
#define RPC_S_AUTH_SKEW                    rpc_s_auth_skew
#define RPC_S_AUTH_BADADDR                 rpc_s_auth_badaddr
#define RPC_S_AUTH_BADVERSION              rpc_s_auth_badversion
#define RPC_S_AUTH_MSG_TYPE                rpc_s_auth_msg_type
#define RPC_S_AUTH_MODIFIED                rpc_s_auth_modified
#define RPC_S_AUTH_BADORDER                rpc_s_auth_badorder
#define RPC_S_AUTH_BADKEYVER               rpc_s_auth_badkeyver
#define RPC_S_AUTH_NOKEY                   rpc_s_auth_nokey
#define RPC_S_AUTH_MUT_FAIL                rpc_s_auth_mut_fail
#define RPC_S_AUTH_BADDIRECTION            rpc_s_auth_baddirection
#define RPC_S_AUTH_METHOD                  rpc_s_auth_method
#define RPC_S_AUTH_BADSEQ                  rpc_s_auth_badseq
#define RPC_S_AUTH_INAPP_CKSUM             rpc_s_auth_inapp_cksum
#define RPC_S_AUTH_FIELD_TOOLONG           rpc_s_auth_field_toolong
#define RPC_S_INVALID_CRC                  rpc_s_invalid_crc
#define RPC_S_BINDING_INCOMPLETE           rpc_s_binding_incomplete
#define RPC_S_KEY_FUNC_NOT_ALLOWED         rpc_s_key_func_not_allowed
#define RPC_S_UNKNOWN_STUB_RTL_IF_VERS     rpc_s_unknown_stub_rtl_if_vers
#define RPC_S_UNKNOWN_IFSPEC_VERS          rpc_s_unknown_ifspec_vers
#define RPC_S_PROTO_UNSUPP_BY_AUTH         rpc_s_proto_unsupp_by_auth
#define RPC_S_AUTHN_CHALLENGE_MALFORMED    rpc_s_authn_challenge_malformed
#define RPC_S_PROTECT_LEVEL_MISMATCH       rpc_s_protect_level_mismatch
#define RPC_S_NO_MEPV                      rpc_s_no_mepv
#define RPC_S_STUB_PROTOCOL_ERROR          rpc_s_stub_protocol_error
#define RPC_S_CLASS_VERSION_MISMATCH       rpc_s_class_version_mismatch
#define RPC_S_HELPER_NOT_RUNNING           rpc_s_helper_not_running
#define RPC_S_HELPER_SHORT_READ            rpc_s_helper_short_read
#define RPC_S_HELPER_CATATONIC             rpc_s_helper_catatonic
#define RPC_S_HELPER_ABORTED               rpc_s_helper_aborted
#define RPC_S_NOT_IN_KERNEL                rpc_s_not_in_kernel
#define RPC_S_HELPER_WRONG_USER            rpc_s_helper_wrong_user
#define RPC_S_HELPER_OVERFLOW              rpc_s_helper_overflow
#define RPC_S_DG_NEED_WAY_AUTH             rpc_s_dg_need_way_auth
#define RPC_S_UNSUPPORTED_AUTH_SUBTYPE     rpc_s_unsupported_auth_subtype
#define RPC_S_WRONG_PICKLE_TYPE            rpc_s_wrong_pickle_type
#define RPC_S_NOT_LISTENING                rpc_s_not_listening
#define RPC_S_SS_BAD_BUFFER                rpc_s_ss_bad_buffer
#define RPC_S_SS_BAD_ES_ACTION             rpc_s_ss_bad_es_action
#define RPC_S_SS_WRONG_ES_VERSION          rpc_s_ss_wrong_es_version
#define RPC_S_FAULT_USER_DEFINED           rpc_s_fault_user_defined
#define RPC_S_SS_INCOMPATIBLE_CODESETS     rpc_s_ss_incompatible_codesets
#define RPC_S_TX_NOT_IN_TRANSACTION        rpc_s_tx_not_in_transaction
#define RPC_S_TX_OPEN_FAILED               rpc_s_tx_open_failed
#define RPC_S_PARTIAL_CREDENTIALS          rpc_s_partial_credentials
#define RPC_S_SS_INVALID_CODESET_TAG       rpc_s_ss_invalid_codeset_tag
#define RPC_S_MGMT_BAD_TYPE                rpc_s_mgmt_bad_type
#define RPC_S_SS_INVALID_CHAR_INPUT        rpc_s_ss_invalid_char_input
#define RPC_S_SS_SHORT_CONV_BUFFER         rpc_s_ss_short_conv_buffer
#define RPC_S_SS_ICONV_ERROR               rpc_s_ss_iconv_error
#define RPC_S_SS_NO_COMPAT_CODESET         rpc_s_ss_no_compat_codeset
#define RPC_S_SS_NO_COMPAT_CHARSETS        rpc_s_ss_no_compat_charsets
#define DCE_CS_C_OK                        dce_cs_c_ok
#define DCE_CS_C_UNKNOWN                   dce_cs_c_unknown
#define DCE_CS_C_NOTFOUND                  dce_cs_c_notfound
#define DCE_CS_C_CANNOT_OPEN_FILE          dce_cs_c_cannot_open_file
#define DCE_CS_C_CANNOT_READ_FILE          dce_cs_c_cannot_read_file
#define DCE_CS_C_CANNOT_ALLOCATE_MEMORY    dce_cs_c_cannot_allocate_memory
#define RPC_S_SS_CLEANUP_FAILED            rpc_s_ss_cleanup_failed
#define RPC_SVC_DESC_GENERAL               rpc_svc_desc_general
#define RPC_SVC_DESC_MUTEX                 rpc_svc_desc_mutex
#define RPC_SVC_DESC_XMIT                  rpc_svc_desc_xmit
#define RPC_SVC_DESC_RECV                  rpc_svc_desc_recv
#define RPC_SVC_DESC_DG_STATE              rpc_svc_desc_dg_state
#define RPC_SVC_DESC_CANCEL                rpc_svc_desc_cancel
#define RPC_SVC_DESC_ORPHAN                rpc_svc_desc_orphan
#define RPC_SVC_DESC_CN_STATE              rpc_svc_desc_cn_state
#define RPC_SVC_DESC_CN_PKT                rpc_svc_desc_cn_pkt
#define RPC_SVC_DESC_PKT_QUOTAS            rpc_svc_desc_pkt_quotas
#define RPC_SVC_DESC_AUTH                  rpc_svc_desc_auth
#define RPC_SVC_DESC_SOURCE                rpc_svc_desc_source
#define RPC_SVC_DESC_STATS                 rpc_svc_desc_stats
#define RPC_SVC_DESC_MEM                   rpc_svc_desc_mem
#define RPC_SVC_DESC_MEM_TYPE              rpc_svc_desc_mem_type
#define RPC_SVC_DESC_DG_PKTLOG             rpc_svc_desc_dg_pktlog
#define RPC_SVC_DESC_THREAD_ID             rpc_svc_desc_thread_id
#define RPC_SVC_DESC_TIMESTAMP             rpc_svc_desc_timestamp
#define RPC_SVC_DESC_CN_ERRORS             rpc_svc_desc_cn_errors
#define RPC_SVC_DESC_CONV_THREAD           rpc_svc_desc_conv_thread
#define RPC_SVC_DESC_PID                   rpc_svc_desc_pid
#define RPC_SVC_DESC_ATFORK                rpc_svc_desc_atfork
#define RPC_SVC_DESC_CMA_THREAD            rpc_svc_desc_cma_thread
#define RPC_SVC_DESC_INHERIT               rpc_svc_desc_inherit
#define RPC_SVC_DESC_DG_SOCKETS            rpc_svc_desc_dg_sockets
#define RPC_SVC_DESC_TIMER                 rpc_svc_desc_timer
#define RPC_SVC_DESC_THREADS               rpc_svc_desc_threads
#define RPC_SVC_DESC_SERVER_CALL           rpc_svc_desc_server_call
#define RPC_SVC_DESC_NSI                   rpc_svc_desc_nsi
#define RPC_SVC_DESC_DG_PKT                rpc_svc_desc_dg_pkt
#define RPC_M_CN_ILL_STATE_TRANS_SA        rpc_m_cn_ill_state_trans_sa
#define RPC_M_CN_ILL_STATE_TRANS_CA        rpc_m_cn_ill_state_trans_ca
#define RPC_M_CN_ILL_STATE_TRANS_SG        rpc_m_cn_ill_state_trans_sg
#define RPC_M_CN_ILL_STATE_TRANS_CG        rpc_m_cn_ill_state_trans_cg
#define RPC_M_CN_ILL_STATE_TRANS_SR        rpc_m_cn_ill_state_trans_sr
#define RPC_M_CN_ILL_STATE_TRANS_CR        rpc_m_cn_ill_state_trans_cr
#define RPC_M_BAD_PKT_TYPE                 rpc_m_bad_pkt_type
#define RPC_M_PROT_MISMATCH                rpc_m_prot_mismatch
#define RPC_M_FRAG_TOOBIG                  rpc_m_frag_toobig
#define RPC_M_UNSUPP_STUB_RTL_IF           rpc_m_unsupp_stub_rtl_if
#define RPC_M_UNHANDLED_CALLSTATE          rpc_m_unhandled_callstate
#define RPC_M_CALL_FAILED                  rpc_m_call_failed
#define RPC_M_CALL_FAILED_NO_STATUS        rpc_m_call_failed_no_status
#define RPC_M_CALL_FAILED_ERRNO            rpc_m_call_failed_errno
#define RPC_M_CALL_FAILED_S                rpc_m_call_failed_s
#define RPC_M_CALL_FAILED_C                rpc_m_call_failed_c
#define RPC_M_ERRMSG_TOOBIG                rpc_m_errmsg_toobig
#define RPC_M_INVALID_SRCHATTR             rpc_m_invalid_srchattr
#define RPC_M_NTS_NOT_FOUND                rpc_m_nts_not_found
#define RPC_M_INVALID_ACCBYTCNT            rpc_m_invalid_accbytcnt
#define RPC_M_PRE_V2_IFSPEC                rpc_m_pre_v2_ifspec
#define RPC_M_UNK_IFSPEC                   rpc_m_unk_ifspec
#define RPC_M_RECVBUF_TOOSMALL             rpc_m_recvbuf_toosmall
#define RPC_M_UNALIGN_AUTHTRL              rpc_m_unalign_authtrl
#define RPC_M_UNEXPECTED_EXC               rpc_m_unexpected_exc
#define RPC_M_NO_STUB_DATA                 rpc_m_no_stub_data
#define RPC_M_EVENTLIST_FULL               rpc_m_eventlist_full
#define RPC_M_UNK_SOCK_TYPE                rpc_m_unk_sock_type
#define RPC_M_UNIMP_CALL                   rpc_m_unimp_call
#define RPC_M_INVALID_SEQNUM               rpc_m_invalid_seqnum
#define RPC_M_CANT_CREATE_UUID             rpc_m_cant_create_uuid
#define RPC_M_PRE_V2_SS                    rpc_m_pre_v2_ss
#define RPC_M_DGPKT_POOL_CORRUPT           rpc_m_dgpkt_pool_corrupt
#define RPC_M_DGPKT_BAD_FREE               rpc_m_dgpkt_bad_free
#define RPC_M_LOOKASIDE_CORRUPT            rpc_m_lookaside_corrupt
#define RPC_M_ALLOC_FAIL                   rpc_m_alloc_fail
#define RPC_M_REALLOC_FAIL                 rpc_m_realloc_fail
#define RPC_M_CANT_OPEN_FILE               rpc_m_cant_open_file
#define RPC_M_CANT_READ_ADDR               rpc_m_cant_read_addr
#define RPC_SVC_DESC_LIBIDL                rpc_svc_desc_libidl
#define RPC_M_CTXRUNDOWN_NOMEM             rpc_m_ctxrundown_nomem
#define RPC_M_CTXRUNDOWN_EXC               rpc_m_ctxrundown_exc
#define RPC_S_FAULT_CODESET_CONV_ERROR     rpc_s_fault_codeset_conv_error
#define RPC_S_NO_CALL_ACTIVE               rpc_s_no_call_active
#define RPC_S_NO_CONTEXT_AVAILABLE         rpc_s_no_context_available

#endif /* RPCSTATUS_H */

#endif /* __GNUC__ */
