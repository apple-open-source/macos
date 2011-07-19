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
**      dgutl.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Utility routines for the NCA RPC datagram protocol implementation.
**
**
*/

#ifndef _DGUTL_H
#define _DGUTL_H

/* ========================================================================= */

#ifndef RPC_DG_PLOG

#define RPC_DG_PLOG_RECVFROM_PKT(hdrp, bodyp)
#define RPC_DG_PLOG_SENDMSG_PKT(iov, iovlen)
#define RPC_DG_PLOG_LOSSY_SENDMSG_PKT(iov, iovlen, lossy_action)
#define rpc__dg_plog_pkt(hdrp, bodyp, recv, lossy_action)

#else

#define RPC_DG_PLOG_RECVFROM_PKT(hdrp, bodyp) \
    { \
        if (RPC_DBG(rpc_es_dbg_dg_pktlog, 100)) \
            rpc__dg_plog_pkt((hdrp), (bodyp), true, 0); \
    }

#define RPC_DG_PLOG_SENDMSG_PKT(iov, iovlen) \
    { \
        if (RPC_DBG(rpc_es_dbg_dg_pktlog, 100)) \
            rpc__dg_plog_pkt((rpc_dg_raw_pkt_hdr_p_t) (iov)[0].base,  \
                    (iovlen) < 2 ? NULL : (rpc_dg_pkt_body_p_t) (iov)[1].base,  \
                    false, 3); \
    }

#define RPC_DG_PLOG_LOSSY_SENDMSG_PKT(iov, iovlen, lossy_action) \
    { \
        if (RPC_DBG(rpc_es_dbg_dg_pktlog, 100)) \
            rpc__dg_plog_pkt((rpc_dg_raw_pkt_hdr_p_t) (iov)[0].base,  \
                    (iovlen) < 2 ? NULL : (rpc_dg_pkt_body_p_t) (iov)[1].base,  \
                    false, lossy_action); \
    }

PRIVATE void rpc__dg_plog_pkt (
        rpc_dg_raw_pkt_hdr_p_t  /*hdrp*/,
        rpc_dg_pkt_body_p_t  /*bodyp*/,
        boolean32  /*recv*/,
        unsigned32  /*lossy_action*/
    );

PRIVATE void rpc__dg_plog_dump (
         /*void*/
    );

#endif

/* ========================================================================= */

PRIVATE void rpc__dg_xmit_pkt (
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t  /*addr*/,
        rpc_socket_iovec_p_t  /*iov*/,
        int  /*iovlen*/,
        boolean * /*b*/
    );

PRIVATE void rpc__dg_xmit_hdr_only_pkt (
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t  /*addr*/,
        rpc_dg_pkt_hdr_p_t  /*hdrp*/,
        rpc_dg_ptype_t  /*ptype*/
    );

PRIVATE void rpc__dg_xmit_error_body_pkt (
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t  /*addr*/,
        rpc_dg_pkt_hdr_p_t  /*hdrp*/,
        rpc_dg_ptype_t  /*ptype*/,
        unsigned32  /*errst*/
    );

PRIVATE const char *rpc__dg_act_seq_string (
        rpc_dg_pkt_hdr_p_t  /*hdrp*/
    );

PRIVATE const char *rpc__dg_pkt_name (
        rpc_dg_ptype_t  /*ptype*/
    );

PRIVATE unsigned16 rpc__dg_uuid_hash (
        uuid_p_t  /*uuid*/
    );

#endif /* _DGUTL_H */
