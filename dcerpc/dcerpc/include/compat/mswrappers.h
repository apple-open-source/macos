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

/* ex: set shiftwidth=4 softtabstop=4 expandtab: */
#ifndef _MSWRAPPERS_H_
#define _MSWRAPPERS_H_

#if defined(IDLBASE_H) && !defined(IDL_CHAR_IS_CHAR)
#error Include mswrappers.h before including dce/idlbase.h
#endif
#define IDL_CHAR_IS_CHAR

#include <stdint.h>

#include <dce/idlbase.h>
#include <dce/rpc.h>
#define DCETHREAD_CHECKED
#define DCETHREAD_USE_THROW
#include <dce/dcethread.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_wchar16_t_DEFINED)
#define _wchar16_t_DEFINED
typedef uint16_t wchar16_t;
#endif

#if !defined(_UCHAR_DEFINED)
#define _UCHAR_DEFINED
typedef uint8_t UCHAR;
#endif

#if !defined(_WCHAR_DEFINED)
#define _WCHAR_DEFINED
typedef wchar16_t WCHAR;
#endif

#if !defined(_DWORD_DEFINED)
#define _DWORD_DEFINED
typedef uint32_t DWORD;
#endif

#if !defined(_PWSTR_DEFINED)
#define _PWSTR_DEFINED
typedef WCHAR * PWSTR;
#endif

#if !defined(_PUCHAR_DEFINED)
#define _PUCHAR_DEFINED
typedef UCHAR * PUCHAR;
#endif

typedef unsigned int RPC_STATUS;
typedef handle_t RPC_BINDING_HANDLE;
typedef rpc_if_handle_t RPC_IF_HANDLE;
typedef idl_uuid_t UUID;
typedef rpc_mgr_proc_t RPC_MGR_EPV;
typedef idl_ushort_int *RPC_WSTR;
typedef rpc_auth_identity_handle_t RPC_AUTH_IDENTITY_HANDLE;

#define RPC_C_PROTSEQ_MAX_REQS_DEFAULT rpc_c_protseq_max_reqs_default
#define RPC_C_LISTEN_MAX_CALLS_DEFAULT rpc_c_listen_max_calls_default

#define RpcTryExcept	DCETHREAD_TRY
#define RpcExcept	DCETHREAD_CATCH_EXPR
#define RpcEndExcept	DCETHREAD_ENDTRY
#define RpcExceptionCode() RpcCompatExceptionToCode(DCETHREAD_EXC_CURRENT)

RPC_STATUS RpcCompatExceptionToCode(dcethread_exc *exc);
typedef RPC_STATUS (*RpcCompatReturnCodeFuncPtr)(void);
RpcCompatReturnCodeFuncPtr RpcCompatReturnLater(RPC_STATUS value);

//User programs put this inside of their type
#define __RPC_USER

RPC_STATUS RpcStringBindingComposeA(
    /* [in] */ UCHAR *string_object_uuid,
    /* [in] */ UCHAR *string_protseq,
    /* [in] */ UCHAR *string_netaddr,
    /* [in] */ UCHAR *string_endpoint,
    /* [in] */ UCHAR *string_options,
    /* [out] */ UCHAR **string_binding
);

RPC_STATUS RpcStringBindingComposeW(
    /* [in] */ PWSTR string_object_uuid,
    /* [in] */ PWSTR string_protseq,
    /* [in] */ PWSTR string_netaddr,
    /* [in] */ PWSTR string_endpoint,
    /* [in] */ PWSTR string_options,
    /* [out] */ PWSTR *string_binding
);

RPC_STATUS RpcBindingFromStringBindingA(
    /* [in] */ UCHAR *string_binding,
    /* [out] */ RPC_BINDING_HANDLE *binding_handle
);

RPC_STATUS RpcBindingFromStringBindingW(
    /* [in] */ PWSTR string_binding,
    /* [out] */ RPC_BINDING_HANDLE *binding_handle
);

RPC_STATUS RpcBindingSetAuthInfoA(
    /* [in] */ RPC_BINDING_HANDLE binding_h,
    /* [in] */ UCHAR* server_princ_name,
    /* [in] */ DWORD authn_level,
    /* [in] */ DWORD authn_protocol,
    /* [in] */ RPC_AUTH_IDENTITY_HANDLE auth_identity,
    /* [in] */ DWORD authz_protocol
);

RPC_STATUS RpcBindingSetAuthInfoW(
    /* [in] */ RPC_BINDING_HANDLE binding_h,
    /* [in] */ PWSTR server_princ_name,
    /* [in] */ DWORD authn_level,
    /* [in] */ DWORD authn_protocol,
    /* [in] */ RPC_AUTH_IDENTITY_HANDLE auth_identity,
    /* [in] */ DWORD authz_protocol
);

RPC_STATUS RpcStringFreeA(
    /* [in, out] */ PUCHAR *string
);

RPC_STATUS RpcStringFreeW(
    /* [in, out] */ PWSTR *string
);

RPC_STATUS RpcBindingFree(
    /* [in, out] */ RPC_BINDING_HANDLE *binding_handle
);

RPC_STATUS RpcServerUseProtseqEpA(
    /* [in] */ PUCHAR protseq,
    /* [in] */ unsigned int max_call_requests,
    /* [in] */ PUCHAR endpoint,
    void *security /*not used*/
);
RPC_STATUS RpcServerUseProtseqEpW(
    /* [in] */ PWSTR protseq,
    /* [in] */ unsigned int max_call_requests,
    /* [in] */ PWSTR endpoint,
    void *security /*not used*/
);

RPC_STATUS RpcServerRegisterIf(
    /* [in] */ RPC_IF_HANDLE if_spec,
    /* [in] */ UUID *mgr_type_uuid,
    /* [in] */ RPC_MGR_EPV *mgr_epv
);

RPC_STATUS RpcServerListen(
    unsigned32 minimum_call_threads, /*not used*/
    /* [in] */ unsigned32 max_calls_exec,
    unsigned32 dont_wait /*not used*/
);

RPC_STATUS LwMapDCEStatusToWinerror(
    RPC_STATUS dceStatus
);

#define RpcStringBindingCompose RpcStringBindingComposeA
#define RpcServerUseProtseqEp RpcServerUseProtseqEpA
#define RpcBindingFromStringBinding RpcBindingFromStringBindingA
#define RpcStringFree RpcStringFreeA
#define RpcBindingSetAuthInfo RpcBindingSetAuthInfoA
#define RpcSsDestroyClientContext(x) rpc_ss_destroy_client_context((rpc_ss_context_t *)x)

#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY rpc_c_protect_level_pkt_privacy

#define RPC_C_AUTHN_GSS_NEGOTIATE   rpc_c_authn_gss_negotiate

#define RPC_C_AUTHZ_NAME    rpc_c_authz_name

#ifdef __cplusplus
} //extern C
#endif

#endif // _MSWRAPPERS_H_
