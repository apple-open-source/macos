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
**      noauthdgp.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definition of types private to the noauth-datagram glue module.
**
**
*/

#ifndef _NOAUTHDG_H
#define _NOAUTHDG_H	1

#define NCK_NEED_MARSHALLING

#include <dg.h>
#include <noauth.h>
#include <dce/conv.h>

/*
 * For various reasons, it's painful to get at the NDR tag of the
 * underlying data, so we cheat and just encode it in big-endian order.
 */

#define rpc_marshall_be_long_int(mp, bei) \
{       long temp = htonl(bei);            \
        rpc_marshall_long_int (mp, temp);  \
}

#define rpc_convert_be_long_int(mp, bei) \
{                                       \
    rpc_unmarshall_long_int(mp, bei);   \
    bei = ntohl(bei);                   \
}

#define rpc_marshall_be_short_int(mp, bei) \
{       short temp = htons(bei);            \
        rpc_marshall_short_int (mp, temp);  \
}

#define rpc_convert_be_short_int(mp, bei) \
{                                       \
    rpc_unmarshall_short_int(mp, bei);   \
    bei = ntohs(bei);                   \
}


/*
 * DG EPV routines.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rpc__noauth_dg_pre_call (
        rpc_auth_info_p_t               ,
        handle_t                        ,
        unsigned32                      *
    );

rpc_auth_info_p_t rpc__noauth_dg_create (
        unsigned32                      * /*st*/
    );

void rpc__noauth_dg_encrypt (
        rpc_auth_info_p_t                /*info*/,
        rpc_dg_xmitq_elt_p_t            ,
        unsigned32                      * /*st*/
    );

void rpc__noauth_dg_pre_send (
        rpc_auth_info_p_t                /*info*/,
        rpc_dg_xmitq_elt_p_t             /*pkt*/,
        rpc_dg_pkt_hdr_p_t               /*hdrp*/,
        rpc_socket_iovec_p_t             /*iov*/,
        int                              /*iovlen*/,
        dce_pointer_t                        /*cksum*/,
        unsigned32                      * /*st*/
    );

void rpc__noauth_dg_recv_ck (
        rpc_auth_info_p_t                /*info*/,
        rpc_dg_recvq_elt_p_t             /*pkt*/,
        dce_pointer_t                        /*cksum*/,
        error_status_t                  * /*st*/
    );

void rpc__noauth_dg_who_are_you (
        rpc_auth_info_p_t                /*info*/,
        handle_t                        ,
        idl_uuid_t                          *,
        unsigned32                      ,
        unsigned32                      *,
        idl_uuid_t                          *,
        unsigned32                      *
    );

void rpc__noauth_dg_way_handler (
        rpc_auth_info_p_t                /*info*/,
        ndr_byte                        * /*in_data*/,
        signed32                         /*in_len*/,
        signed32                         /*out_max_len*/,
        ndr_byte                        * /*out_data*/,
        signed32                        * /*out_len*/,
        unsigned32                      * /*st*/
    );

#ifdef __cplusplus
}
#endif

#endif /* _NOAUTHDG_H */
