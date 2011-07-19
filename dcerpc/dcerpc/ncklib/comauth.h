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

/*
**
**  NAME
**
**      comauth.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Generic interface to authentication services
**
**
*/

#ifndef _COMAUTH_H
#define _COMAUTH_H	1

/***********************************************************************/
/*
 * Function pointer types of functions exported by an authentication
 * service.
 */

/*
 * The next four function pointer types correspond to the similarly
 * named functions in the API.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rpc_auth_bnd_set_auth_info_fn_t) (
        unsigned_char_p_t                   /* in  */   /*server_princ_name*/,
        rpc_authn_level_t                   /* in  */   /*authn_level*/,
        rpc_auth_identity_handle_t          /* in  */   /*auth_identity*/,
        rpc_authz_protocol_id_t             /* in  */   /*authz_protocol*/,
        rpc_binding_handle_t                /* in  */   /*binding_h*/,
        rpc_auth_info_p_t                   /* out*/   * /*auth_info*/,
        unsigned32                          /* out */   *st
    );

typedef void (*rpc_auth_serv_reg_auth_info_fn_t) (
        unsigned_char_p_t                   /* in  */   /*server_princ_name*/,
        rpc_auth_key_retrieval_fn_t         /* in  */   /*get_key_func*/,
        dce_pointer_t                           /* in  */   /*arg*/,
        unsigned32                          /* out */   *st
    );

typedef void (*rpc_mgmt_inq_def_authn_lvl_fn_t) (
        unsigned32                          /* out */   * /*authn_level*/,
        unsigned32                          /* out */   *st
    );

/*
 * This function pointer type describes an authentication
 * procedure that frees the auth info memory and resources.
 */

typedef void (*rpc_auth_free_info_fn_t) (
        rpc_auth_info_p_t                   /* in/out */  * /*auth_info*/
    );

/*
 * This function pointer type defines a procedure which frees key info
 * memory and resources.
 */

typedef void (*rpc_auth_free_key_fn_t) (
        rpc_key_info_p_t                    /* in/out */ * /*key_info*/
    );

/*
 * These function pointer types resolve a passed-in "auth_identity"
 * into a reference to a "real" auth identity, and release the
 * reference.  Note that these return the error status in the return
 * value (instead of in an output argument) because this means that
 * the function pointer has the exact same signature as the routine in
 * the security component which implements this function.
 */

typedef error_status_t (*rpc_auth_resolve_identity_fn_t) (
        rpc_auth_identity_handle_t                 /* in */ /*in_identity*/,
        rpc_auth_identity_handle_t                 /* out */ *out_identity
    );

typedef void (*rpc_auth_release_identity_fn_t) (
        rpc_auth_identity_handle_t                 /* in/out */ * /*identity*/
    );

/*
 * This function pointer type describes an authentication service procedure
 * that returns the principal name (possibly just one of many) that server
 * is running as.
 */

typedef void (*rpc_auth_inq_my_princ_name_fn_t) (
        unsigned32                          /* in */    /*princ_name_size*/,
        unsigned_char_p_t                   /* out */   /*princ_name*/,
        unsigned32                          /* out */   * /*st*/
    );

/*
 * This function pointer type describes an authentication service procedure
 * that returns the mechanism-specific security context.
 */

typedef void (*rpc_auth_inq_sec_context_fn_t) (
        rpc_auth_info_p_t                   /* in */    /*auth_info*/,
        void                                /* out */   ** /*mech_context*/,
        unsigned32                          /* out */   * /*st*/
   );

typedef void (*rpc_auth_inq_access_token_fn_t) (
        rpc_auth_info_p_t,
        rpc_access_token_p_t*,
        unsigned32*);

/***********************************************************************/
/*
 * RPC authentication service API EPV.
 */

typedef struct
{
    rpc_auth_bnd_set_auth_info_fn_t     binding_set_auth_info;
    rpc_auth_serv_reg_auth_info_fn_t    server_register_auth_info;
    rpc_mgmt_inq_def_authn_lvl_fn_t     mgmt_inq_dflt_auth_level;
    rpc_auth_inq_my_princ_name_fn_t     inq_my_princ_name;
    rpc_auth_free_info_fn_t             free_info;
    rpc_auth_free_key_fn_t              free_key;
    rpc_auth_resolve_identity_fn_t      resolve_id;
    rpc_auth_release_identity_fn_t      release_id;
    rpc_auth_inq_sec_context_fn_t       inq_sec_context;
    rpc_auth_inq_access_token_fn_t      inq_access_token;
} rpc_auth_epv_t, *rpc_auth_epv_p_t;

/***********************************************************************/

PRIVATE void rpc__auth_info_binding_release (
        rpc_binding_rep_p_t     binding_rep
    );

PRIVATE void rpc__np_auth_info_binding_release (
        rpc_binding_rep_p_t     binding_rep
    );

PRIVATE void rpc__auth_inq_my_princ_name (
        unsigned32              /*dce_rpc_authn_protocol*/,
        unsigned32              /*princ_name_size*/,
        unsigned_char_p_t       /*princ_name*/,
        unsigned32              * /*status*/
    );

PRIVATE void rpc__auth_info_cache_init (
        unsigned32              * /*status*/
    );

/***********************************************************************/

/*
 * Signature of the init routine provided.  Each authentication service
 * must return both an API and a common EPV.
 */

typedef void (*rpc_auth_init_fn_t) (
        rpc_auth_epv_p_t            * /*auth_epv*/,
        rpc_auth_rpc_prot_epv_tbl_t * /*auth_rpc_prot_epv_tbl*/,
        unsigned32                  * /*status*/
    );

/*
 * Declarations of the RPC authentication service init routines.
 */

void rpc__krb_init (
        rpc_auth_epv_p_t            * /*auth_epv*/,
        rpc_auth_rpc_prot_epv_tbl_t * /*auth_rpc_prot_epv_tbl*/,
        unsigned32                  * /*status*/
    );

void rpc__noauth_init (
        rpc_auth_epv_p_t            * /*auth_epv*/,
        rpc_auth_rpc_prot_epv_tbl_t * /*auth_rpc_prot_epv_tbl*/,
        unsigned32                  * /*status*/
    );

void rpc__key_info_release (
    rpc_key_info_p_t *
);

void rpc__key_info_reference (
    rpc_key_info_p_t
);

#ifdef _cplusplus
}
#endif

#endif /*  _COMAUTH_H */
