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
**      schnauth.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Types and routines private to the schannel authentication
**      module.
**
**
*/

#ifndef _SCHNAUTH_H
#define _SCHNAUTH_H	1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <commonp.h>
#include <com.h>
#include <comp.h>

#include <dce/id_base.h>
#include <dce/sec_authn.h>

#include <schannel.h>

typedef struct rpc_schnauth_info_t {
    rpc_auth_info_t auth_info;  /* This must be the first element. */
    rpc_mutex_t lock;

    /*
     * Schannel security context
     */

    struct schn_auth_ctx sec_ctx;

} rpc_schnauth_info_t, *rpc_schnauth_info_p_t;

/*
 * Locking macros.
 */

#define RPC_SCHN_INFO_LOCK(info) RPC_MUTEX_LOCK ((info)->lock)
#define RPC_SCHN_INFO_UNLOCK(info) RPC_MUTEX_UNLOCK ((info)->lock)

/*
 * Prototypes for PRIVATE routines.
 */

PRIVATE rpc_protocol_id_t       rpc__schnauth_cn_init (
    rpc_auth_rpc_prot_epv_p_t       *epv,
    unsigned32                      *st
    );

/*
 * Prototypes for API EPV routines.
 */

void rpc__schnauth_bnd_set_auth (
        unsigned_char_p_t                   /* in  */    /*server_princ_name*/,
        rpc_authn_level_t                   /* in  */    /*authn_level*/,
        rpc_auth_identity_handle_t          /* in  */    /*auth_identity*/,
        rpc_authz_protocol_id_t             /* in  */    /*authz_protocol*/,
        rpc_binding_handle_t                /* in  */    /*binding_h*/,
        rpc_auth_info_p_t                   /* out */   * /*auth_info*/,
        unsigned32                          /* out */   * /*st*/
    );

void rpc__schnauth_srv_reg_auth (
        unsigned_char_p_t                   /* in  */    /*server_princ_name*/,
        rpc_auth_key_retrieval_fn_t         /* in  */    /*get_key_func*/,
        dce_pointer_t                           /* in  */    /*arg*/,
        unsigned32                          /* out */   * /*st*/
    );

void rpc__schnauth_mgt_inq_def (
        unsigned32                          /* out */   * /*authn_level*/,
        unsigned32                          /* out */   * /*st*/
    );

void rpc__schnauth_inq_my_princ_name (
        unsigned32                          /* in */     /*princ_name_size*/,
        unsigned_char_p_t                   /* out */    /*princ_name*/,
        unsigned32                          /* out */   * /*st*/
    );

void rpc__schnauth_free_info (
        rpc_auth_info_p_t                   /* in/out */ * /*info*/
    );

void rpc__schnauth_free_key (
        rpc_key_info_p_t                    /* in/out */ * /*key_info*/
    );

error_status_t rpc__schnauth_resolve_identity (
        rpc_auth_identity_handle_t          /* in */ /*in_identity*/,
        rpc_auth_identity_handle_t          /* out */ *out_identity
    );

void rpc__schnauth_release_identity (
        rpc_auth_identity_handle_t          /* in/out */ * /*identity*/
    );

void rpc__schnauth_inq_sec_context (
        rpc_auth_info_p_t           auth_info,
        void                        **mech_context,
        unsigned32                  *st
    );

/*
 * Miscellaneous internal entry points.
 */

sec_id_pac_t *rpc__schnauth_get_pac (void);

#ifdef notdef
/* From sec_id_pickle.h */

typedef struct pickle_handle_s * pickle_handle_t;

/*
 * Functions
 */

/* s e c _ p i c k l e _ c r e a t e
 *
 * Create a pickling context.  This must be called to obtain a pickling
 * context before any pickling calls can be performed.
 */
pickle_handle_t sec_pickle_create ( void );

/* s e c _ p i c k l e _ r e l e a s e
 *
 * Terminate a pickling context.  This function will release any storage
 * associated with the pickling context.
 */
void sec_pickle_release ( pickle_handle_t * /*p*/);

/* s e c _ i d _ p a c _ f r e e
 *
 * Release dynamic storage associated with a PAC.
 */

void sec_id_pac_free ( sec_id_pac_t *) ;

/* s e c _ i d _ p a c _ p i c k l e
 *
 * Pickle a pac.
 */
extern void     sec_id_pac_pickle (
        /* [in] */      pickle_handle_t          /*pickle_handle*/,
        /* [in] */      sec_id_pac_t            *  /*pac*/,
        /* [out] */     sec_id_pickled_pac_t    **  /*pickled_pac*/
  );

/* s e c _ i d _ p a c _ u n p i c k l e
 *
 * unpickle a pac
 */

extern void     sec_id_pac_unpickle (
        /* [in] */      sec_id_pickled_pac_t    *  /*pickled_pac*/,
        /* [out] */     sec_id_pac_t            *  /*pac*/
  );
#endif /* notdef */

#endif /* _SCHNAUTH_H */
