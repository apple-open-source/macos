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
**      cnnet.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Interface to the NCA Connection Protocol Service's Network Service.
**
**
*/

#ifndef _CNNET_H
#define _CNNET_H	1

/*
 * C O N N E C T I O N   S T A T E S
 */
#define RPC_C_CN_CLOSED         0
#define RPC_C_CN_CONNECTING     1
#define RPC_C_CN_OPEN           2

/*
 * R P C _ C N _ N E T W O R K _ I O V E C T O R _ T O _ I O V
 */

#define RPC_CN_NETWORK_IOVECTOR_TO_IOV(iovector, iov, iovcnt, bytes_to_send)\
{\
    unsigned8   _num_elts;\
\
    for (_num_elts = 0, (bytes_to_send) = 0; _num_elts < (iovector)->num_elt; _num_elts++) \
    { \
        (iov)[_num_elts].iov_base = (byte_p_t) ((iovector)->elt[_num_elts].data_addr); \
        (iov)[_num_elts].iov_len = (int) ((iovector)->elt[_num_elts].data_len); \
        (bytes_to_send) += (iov)[_num_elts].iov_len; \
    } \
    (iovcnt) = _num_elts; \
}

/*
 * R P C _ C N _ N E T W O R K _ I O V _ A D J U S T
 */

#define RPC_CN_NETWORK_IOV_ADJUST(iovp, iovcnt, cc)\
{\
    unsigned8   _num_elts;\
    size_t    _bytes_to_adjust;\
\
    for (_bytes_to_adjust = (cc), _num_elts = 0;; _num_elts++, iovp++) \
    { \
        if ((iovp)->iov_len > _bytes_to_adjust) \
        { \
            (iovp)->iov_len -= _bytes_to_adjust; \
            (iovp)->iov_base = (unsigned8 *)(iovp)->iov_base + _bytes_to_adjust; \
            break; \
        } \
        _bytes_to_adjust -= (iovp)->iov_len; \
    } \
    (iovcnt) -= _num_elts; \
}

/*
 * R P C _ _ C N _ N E T W O R K _ U S E _ S O C K E T
 */

PRIVATE void rpc__cn_network_use_socket (
    rpc_socket_t		/* int */   /*rpc_sock*/,
    unsigned32                  /* in  */   /*max_calls*/,
    unsigned32                  /* out */   * /*st*/);

/*
 * R P C _ _ C N _ N E T W O R K _ U S E _ P R O T S E Q
 */

PRIVATE void rpc__cn_network_use_protseq (
    rpc_protseq_id_t            /* pseq_id */,
    unsigned32                  /* max_calls */,
    rpc_addr_p_t                /* rpc_addr */,
    unsigned_char_p_t           /* endpoint */,
    unsigned32                  */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ M O N
 */

PRIVATE void rpc__cn_network_mon (
    rpc_binding_rep_p_t     /* binding_r */,
    rpc_client_handle_t     /* client_h */,
    rpc_network_rundown_fn_t /* rundown */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ S T O P _ M O N
 */

PRIVATE void rpc__cn_network_stop_mon (
    rpc_binding_rep_p_t     /* binding_r */,
    rpc_client_handle_t     /* client_h */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ M A I N T
 */

PRIVATE void rpc__cn_network_maint (
    rpc_binding_rep_p_t     /* binding_r */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ S T O P _ M A I N T
 */

PRIVATE void rpc__cn_network_stop_maint (
    rpc_binding_rep_p_t     /* binding_r */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ C L O S E
 */

PRIVATE void rpc__cn_network_close (
    rpc_binding_rep_p_t     /* binding_r */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ S E L E C T _ D I S P A T C H
 */

PRIVATE void rpc__cn_network_select_dispatch (
    rpc_socket_t            /* desc */,
    dce_pointer_t               /* priv_info */,
    boolean32               /* is_active */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ I N Q _ P R O T _ V E R S
 */

PRIVATE void rpc__cn_network_inq_prot_vers (
    unsigned8               */* prot_id */,
    unsigned32              */* version_major */,
    unsigned32              */* version_minor */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ R E Q _ C O N N E C T
 */

PRIVATE void rpc__cn_network_req_connect (
    rpc_addr_p_t            /* rpc_addr */,
    rpc_cn_assoc_p_t        /* assoc */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ C L O S E _ C O N N E C T
 */

PRIVATE void rpc__cn_network_close_connect (
    rpc_cn_assoc_p_t        /* assoc */,
    unsigned32              */* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ C O N N E C T _ F A I L
 */

PRIVATE boolean32 rpc__cn_network_connect_fail (unsigned32);

/*
 * These two are internal API's so you can tweak the TCP buffering.
 */

/*
 * R P C _ _ C N _ S E T _ S O C K _ B U F F S I Z E
 */
PRIVATE void rpc__cn_set_sock_buffsize (
        unsigned32	  /* rsize */,
        unsigned32	  /* ssize */,
        unsigned32	* /* st */);

/*
 * R P C _ _ C N _ I N Q _ S O C K _ B U F F S I Z E
 */
PRIVATE void rpc__cn_inq_sock_buffsize (
        unsigned32	* /* rsize */,
        unsigned32	* /* ssize */,
        unsigned32  * /* st */);

/*
 * R P C _ _ C N _ N E T W O R K _ G E T P E E R E I D
 */

PRIVATE void rpc__cn_network_getpeereid (
        rpc_binding_rep_p_t     /* binding_r */,
        uid_t                   */* euid */,
        gid_t                   */* egid */,
        unsigned32              */* st */);

#endif /* _CNNET_H */
