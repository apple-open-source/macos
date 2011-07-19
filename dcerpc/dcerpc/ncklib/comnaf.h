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
**      comnaf.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Generic interface to Network Address Family Extension Services
**
**
*/

#ifndef _COMNAF_H
#define _COMNAF_H

/***********************************************************************/
/*
 * The Network Address Family EPV.
 */

typedef void (*rpc_naf_addr_alloc_fn_t)
    (
        rpc_protseq_id_t            rpc_protseq_id,
        rpc_naf_id_t                naf_id,
        unsigned_char_p_t           endpoint,
        unsigned_char_p_t           netaddr,
        unsigned_char_p_t           network_options,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_copy_fn_t)
    (
        rpc_addr_p_t                src_rpc_addr,
        rpc_addr_p_t                *dst_rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_free_fn_t)
    (
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_set_endpoint_fn_t)
    (
        unsigned_char_p_t           endpoint,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_inq_endpoint_fn_t)
    (
        rpc_addr_p_t                rpc_addr,
        unsigned_char_p_t           *endpoint,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_set_netaddr_fn_t)
    (
        unsigned_char_p_t           netaddr,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_inq_netaddr_fn_t)
    (
        rpc_addr_p_t                rpc_addr,
        unsigned_char_p_t           *netaddr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_set_options_fn_t)
    (
        unsigned_char_p_t           network_options,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_addr_inq_options_fn_t)
    (
        rpc_addr_p_t                rpc_addr,
        unsigned_char_p_t           *network_options,
        unsigned32                  *status
    );

typedef void (*rpc_naf_desc_inq_addr_fn_t)
    (
        rpc_protseq_id_t            protseq_id,
        rpc_socket_t                desc,
        rpc_addr_vector_p_t         *rpc_addr_vec,
        unsigned32                  *status
    );

typedef void (*rpc_naf_inq_max_tsdu_fn_t)
    (
        rpc_naf_id_t                naf_id,
        rpc_network_if_id_t         iftype,
        rpc_network_protocol_id_t   protocol,
        unsigned32                  *max_tsdu,
        unsigned32                  *status
    );

typedef void (*rpc_naf_get_broadcast_fn_t)
    (
        rpc_naf_id_t                naf_id,
        rpc_protseq_id_t            protseq_id,
        rpc_addr_vector_p_t         *rpc_addrs,
        unsigned32                  *status
    );

typedef boolean (*rpc_naf_addr_compare_fn_t)
    (
        rpc_addr_p_t                addr1,
        rpc_addr_p_t                addr2,
        unsigned32                  *status
    );

typedef void (*rpc_naf_inq_pth_unfrg_tpdu_fn_t)
    (
        rpc_addr_p_t                rpc_addr,
        rpc_network_if_id_t         iftype,
        rpc_network_protocol_id_t   protocol,
        unsigned32                  *max_tpdu,
        unsigned32                  *status
    );

typedef void (*rpc_naf_inq_loc_unfrg_tpdu_fn_t)
    (
        rpc_naf_id_t                naf_id,
        rpc_network_if_id_t         iftype,
        rpc_network_protocol_id_t   protocol,
        unsigned32                  *max_tpdu,
        unsigned32                  *status
    );

typedef void (*rpc_naf_desc_inq_network_fn_t)
    (
        rpc_socket_t                desc,
        rpc_network_if_id_t         *socket_type,
        rpc_network_protocol_id_t   *protocol_id,
        unsigned32                  *status
    );

typedef void (*rpc_naf_set_pkt_nodelay_fn_t)
    (
        rpc_socket_t                desc,
        unsigned32                  *status
    );

typedef boolean (*rpc_naf_is_connect_closed_fn_t)
    (
        rpc_socket_t                desc,
        unsigned32                  *status
    );

typedef void (*rpc_naf_twr_flrs_from_addr_fn_t)
    (
        rpc_addr_p_t                rpc_addr,
        twr_p_t                     *lower_flrs,
        unsigned32                  *status
    );

typedef void (*rpc_naf_twr_flrs_to_addr_fn_t)
    (
        byte_p_t                    tower_octet_string,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_desc_inq_peer_addr_fn_t)
    (
        rpc_protseq_id_t            protseq_id,
        rpc_socket_t                desc,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    );

typedef void (*rpc_naf_set_port_restriction_fn_t)
    (
        rpc_protseq_id_t            protseq_id,
        unsigned32                  n_elements,
        unsigned_char_p_t           *first_port_name_list,
        unsigned_char_p_t           *last_port_name_list,
        unsigned32                  *status
    );

typedef void (*rpc_naf_get_next_restricted_port_fn_t)
    (
        rpc_protseq_id_t            protseq_id,
        unsigned_char_p_t           *port_name,
        unsigned32                  *status
    );

typedef void (*rpc_naf_inq_max_frag_size_fn_t)
    (
        rpc_addr_p_t                rpc_addr,
        unsigned32                  *max_frag_size,
        unsigned32                  *status
    );

typedef struct
{
    rpc_naf_addr_alloc_fn_t         naf_addr_alloc;
    rpc_naf_addr_copy_fn_t          naf_addr_copy;
    rpc_naf_addr_free_fn_t          naf_addr_free;
    rpc_naf_addr_set_endpoint_fn_t  naf_addr_set_endpoint;
    rpc_naf_addr_inq_endpoint_fn_t  naf_addr_inq_endpoint;
    rpc_naf_addr_set_netaddr_fn_t   naf_addr_set_netaddr;
    rpc_naf_addr_inq_netaddr_fn_t   naf_addr_inq_netaddr;
    rpc_naf_addr_set_options_fn_t   naf_addr_set_options;
    rpc_naf_addr_inq_options_fn_t   naf_addr_inq_options;
    rpc_naf_desc_inq_addr_fn_t      naf_desc_inq_addr;
    rpc_naf_desc_inq_network_fn_t   naf_desc_inq_network;
    rpc_naf_inq_max_tsdu_fn_t       naf_inq_max_tsdu;
    rpc_naf_get_broadcast_fn_t      naf_get_broadcast;
    rpc_naf_addr_compare_fn_t       naf_addr_compare;
    rpc_naf_inq_pth_unfrg_tpdu_fn_t naf_inq_max_pth_unfrg_tpdu;
    rpc_naf_inq_loc_unfrg_tpdu_fn_t naf_inq_max_loc_unfrg_tpdu;
    rpc_naf_set_pkt_nodelay_fn_t    naf_set_pkt_nodelay;
    rpc_naf_is_connect_closed_fn_t  naf_is_connect_closed;
    rpc_naf_twr_flrs_from_addr_fn_t naf_tower_flrs_from_addr;
    rpc_naf_twr_flrs_to_addr_fn_t   naf_tower_flrs_to_addr;
    rpc_naf_desc_inq_peer_addr_fn_t naf_desc_inq_peer_addr;
    rpc_naf_set_port_restriction_fn_t naf_set_port_restriction;
    rpc_naf_get_next_restricted_port_fn_t naf_get_next_restricted_port;
    rpc_naf_inq_max_frag_size_fn_t  naf_inq_max_frag_size;
} rpc_naf_epv_t;

/* NAF endpoint vectors don't need to be modified, const them. */
typedef const rpc_naf_epv_t * rpc_naf_epv_p_t;

/***********************************************************************/
/*
 * Signature of the init routine provided.
 */
typedef void (*rpc_naf_init_fn_t) (
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    );

/*
 * Declarations of the Network Address Family Extension Service init
 * routines.
 */
PRIVATE void rpc__unix_init (
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    );

PRIVATE void rpc__ip_init (
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    );

PRIVATE void rpc__dnet_init (
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    );

PRIVATE void rpc__osi_init (
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    );

PRIVATE void rpc__dds_init (
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    );

PRIVATE void rpc__naf_set_port_restriction (
    rpc_protseq_id_t,
    unsigned32,
    unsigned_char_p_t *,
    unsigned_char_p_t *,
    unsigned32 *
);

PRIVATE void rpc__naf_get_next_restricted_port  (
    rpc_protseq_id_t,
    unsigned_char_p_t *,
    unsigned32 *
);

#endif /* _COMNAF_H */
