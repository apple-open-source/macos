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

#if !defined(_WIN32) && !defined(DCERPC_H)
#define DCERPC_H
#include <dce/rpc.h>
#define DCETHREAD_CHECKED
#define DCETHREAD_USE_THROW
#include <dce/dcethread.h>
#include <dce/dce_error.h>

// Make sure the parent directory is in the include path
#include <compat/rpcfields.h>
#elif !defined(DCERPC_H)
#define DCERPC_H
#include <rpc.h>

// Make sure the parent directory is in the include path
#include <compat/rpcfields.h>

#define error_status_ok 0

// DCE-to-MS RPC

#define rpc_binding_free(binding, status) \
    (*(status) = RpcBindingFree(binding))

#define rpc_binding_from_string_binding(string_binding, binding, status) \
    (*(status) = RpcBindingFromStringBinding(string_binding, binding))

#define rpc_binding_to_string_binding(binding, string_binding, status) \
    (*(status) = RpcBindingToStringBinding(binding, string_binding))

#define rpc_ep_register(if_handle, binding_vec, object_uuid_vec, annotation, status) \
    (*(status) = RpcEpRegister(if_handle, binding_vec, object_uuid_vec, annotation))

#define rpc_ep_resolve_binding(binding, if_handle, status) \
    (*(status) = RpcEpResolveBinding(binding, if_handle))

#define rpc_ep_unregister(if_handle, binding_vec, object_uuid_vec, status) \
    (*(status) = RpcEpUnregister(if_handle, binding_vec, object_uuid_vec))

#define rpc_server_inq_bindings(binding_vector, status) \
    (*(status) = RpcServerInqBindings(binding_vector))

// NOTE: Difference between DCE and MS
#define rpc_server_listen(max_calls_exec, status) \
    (*(status) = RpcServerListen(1, max_calls_exec, FALSE))

#define rpc_server_register_if(if_handle, mgr_type_uuid, mgr_epv, status) \
    (*(status) = RpcServerRegisterIf(if_handle, mgr_type_uuid, mgr_epv))

// NOTE: Difference between DCE and MS
// TODO: Determine what to use for WaitForCallsToComplete argument
#define rpc_server_unregister_if(if_handle, mgr_type_uuid, status) \
    (*(status) = RpcServerUnregisterIf(if_handle, mgr_type_uuid, TRUE))

// NOTE: Difference between DCE and MS
#define rpc_server_use_all_protseqs(max_call_requests, status) \
    (*(status) = RpcServerUseAllProtseqs(max_call_requests, NULL))

// NOTE: Difference between DCE and MS
#define rpc_server_use_all_protseqs_if(max_call_requests, if_handle, status) \
    (*(status) = RpcServerUseAllProtseqsIf(max_call_requests, if_handle, NULL))

// NOTE: Difference between DCE and MS
#define rpc_server_use_protseq(protseq, max_call_requests, status) \
    (*(status) = RpcServerUseProtseq(protseq, max_call_requests, NULL))

// NOTE: Difference between DCE and MS
#define rpc_server_use_protseq_ep(protseq, max_call_requests, endpoint, status) \
    (*(status) = RpcServerUseProtseqEp(protseq, max_call_requests, endpoint, NULL))

#define rpc_string_binding_compose(obj_uuid, protseq, network_addr, endpoint, options, string_binding, status) \
    (*(status) = RpcStringBindingCompose(obj_uuid, protseq, network_addr, endpoint, options, string_binding))

#define rpc_string_free(string, status) \
    (*(status) = RpcStringFree(string))

#define rpc_ss_allocate RpcSsAllocate

typedef RPC_BINDING_VECTOR* rpc_binding_vector_p_t;
typedef RPC_BINDING_HANDLE rpc_binding_handle_t;
typedef RPC_IF_HANDLE rpc_if_handle_t;
typedef RPC_IF_ID rpc_if_id_t;

#define ATTRIBUTE_UNUSED

#define DCETHREAD_TRY RpcTryExcept
#define DCETHREAD_CATCH_ALL(_exc) RpcExcept(RpcExceptionCode == (_exc))
#define DCETHREAD_ENDTRY RpcEndExcept

#define rpc_c_protseq_max_calls_default RPC_C_PROTSEQ_MAX_REQS_DEFAULT
#define rpc_c_listen_max_calls_default RPC_C_LISTEN_MAX_CALLS_DEFAULT

typedef boolean idl_boolean;

#define rpc_s_ok RPC_S_OK

typedef TCHAR dce_error_string_t[1024];
#define dce_error_inq_text(error_code, error_string, status) (*(status) = DceErrorInqText(error_code, error_string))

#endif
