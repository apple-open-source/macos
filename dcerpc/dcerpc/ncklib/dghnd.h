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
**  NAME:
**
**      dghnd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Include file for "dghnd.c".
**
**
*/

#ifndef _DGHND_H
#define _DGHND_H

/*
 * R P C _ D G _ B I N D I N G _ S E R V E R _ R E I N I T
 *
 * Reinitialize the per-call variant information in a cached handle.
 * The binding handle is bound to the scall; the scall is bound to an
 * actid; the actid is bound to the auth identity - so the auth identity
 * can't change.  The object id can change.  The address of the client
 * can change (actid's aren't even bound to a protseq let alone specific
 * network-addr/endpoint pairs; sigh).
 */
#define RPC_DG_BINDING_SERVER_REINIT(h) { \
    unsigned32 st; \
    \
    (h)->c.c.obj = (h)->scall->c.call_object; \
    rpc__naf_addr_overcopy((h)->scall->c.addr, &(h)->c.c.rpc_addr, &st); \
}

/* ========================================================================= */

rpc_binding_rep_t *rpc__dg_binding_alloc    (
        boolean32  /*is_server*/,
        unsigned32 * /*st*/
    );
void rpc__dg_binding_init    (
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    );
void rpc__dg_binding_reset    (
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    );
void rpc__dg_binding_changed    (
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    );
void rpc__dg_binding_free    (
        rpc_binding_rep_p_t * /*h*/,
        unsigned32 * /*st*/
    );
void rpc__dg_binding_inq_addr    (
        rpc_binding_rep_p_t  /*h*/,
        rpc_addr_p_t * /*rpc_addr*/,
        unsigned32 * /*st*/
    );
void rpc__dg_binding_copy    (
        rpc_binding_rep_p_t  /*src_h*/,
        rpc_binding_rep_p_t  /*dst_h*/,
        unsigned32 * /*st*/
    );

rpc_dg_binding_client_p_t rpc__dg_binding_srvr_to_client    (
        rpc_dg_binding_server_p_t  /*shand*/,
        unsigned32 * /*st*/
    );

void rpc__dg_binding_cross_fork    (
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    );

#endif /* _DGHND_H */
