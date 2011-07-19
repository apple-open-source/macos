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
**      ipnaf.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definitions and Data Type declarations
**  used by the Internet Network Address Family Extension
**  service.
**
**
*/

#ifndef _IPNAF_H
#define _IPNAF_H	1

/***********************************************************************
 *
 *  Include the Internet specific socket address
 */

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

/***********************************************************************
 *
 * provides a masked unsigned8 required in conversion of IP network
 * address to ascii dot notation string.
 */

#define UC(b)               (((int) b) & 0xff)

/***********************************************************************
 *
 *  The representation of an RPC Address that holds an IP address.
 */

typedef struct rpc_addr_ip_t
{
    rpc_protseq_id_t        rpc_protseq_id;
    unsigned32              len;
    struct sockaddr_in      sa;
} rpc_ip_addr_t, *rpc_ip_addr_p_t;

/***********************************************************************
 *
 * Define some TPDU/TSDU constants, if they haven't already been defined
 * (typically in a system header file).
 */

/*
 * The max # of data bytes that can go into a UDP packet body such that
 * the resulting IP packet can fit through any of the local network
 * interfaces without inducing IP fragmentation.
 *
 * NOTE WELL:  This value is derived from
 *
 *      (1) The size of the data section of data link packets.  For the
 *          time being, the data link is assumed to be ethernet.
 *
 *      (2) The size of the LLC frame.  RFC 1042, which specifies IP
 *          over 802 networks, calls for the use of the SNAP protocol.
 *          SNAP takes up 8 bytes of the ethernet frame's data section.
 *
 *      (3) The size of the UDP and IP headers, from RFCs 768 and 791.
 *
 *      (4) The length of the IP options part of the header.  Since we
 *          do not currently use any of the IP options, this value is
 *          0.   *** This constant must be modified if we ever make use
 *          of IP options in the future. ***
 *
 * !!! THIS VALUE SHOULD BE COMPUTED AT RUNTIME and with a little work
 * could be.
 */

#define RPC_C_ETHER_MAX_DATA_SIZE 1500
#define RPC_C_IP_LLC_SIZE            8 /* LLC frame for SNAP protocol */
#define RPC_C_IP_HDR_SIZE           20 /* Base IP header */
#define RPC_C_IP_OPTS_SIZE           0 /* IP options length */
#define RPC_C_UDP_HDR_SIZE           8 /* UDP header */
#ifndef RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU
#define RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU ( \
        RPC_C_ETHER_MAX_DATA_SIZE - \
        (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE + \
         RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE) \
)
#endif /* RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU */

#define RPC_C_FDDI_MAX_DATA_SIZE 4352

/*
 * The max # of data bytes that can go into a UDP packet body such that
 * the resulting IP packet can be sent to some host without being
 * fragmented along the path.
 *
 * For now, we just set this value to be the same as above.  In general,
 * this value would be <= the above value.
 *
 * !!! THIS VALUE SHOULD BE COMPUTED AT RUNTIME as a function of the
 * target host address but can't be given the tools we have today.
 *
 */
#ifndef RPC_C_IP_UDP_MAX_PTH_UNFRG_TPDU
#define RPC_C_IP_UDP_MAX_PTH_UNFRG_TPDU RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU
#endif

/*
 * The max # of data bytes that can go into a UDP packet body and be
 * accepted through the transport service interface (i.e., sockets).
 * While logically this value is 2**16-1 (see RFC 768), in practice many
 * systems don't support such a large value.
 * This value is really only useful when it is necessary to *limit*
 * the fragment size because of a limitation in the service
 * interface.
 */
#ifndef RPC_C_IP_UDP_MAX_TSDU
#define RPC_C_IP_UDP_MAX_TSDU ( \
        64 * 1024 - \
        (RPC_C_IP_HDR_SIZE + RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE) \
)
#endif

/*
 * Max Path DG Fragment Size:
 *
 *   The size in bytes of the largest DG fragment that can be sent to
 *   a particular address. This is determined when the call handle to
 *   a particular address is created and may change in the life of the
 *   call handle.
 *
 * The constant defined here is based on experimentation.
 *
 * Caution: This must be less than RPC_C_DG_MAX_FRAG_SIZE::dg.h!
 */

#ifndef RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE
#define RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE ( \
        RPC_C_FDDI_MAX_DATA_SIZE - \
        (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE + \
         RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE) \
)
#endif

/*
 * Max Local DG Fragment Size:
 *
 *   The size in bytes of the largest DG fragment that can be sent to
 *   a "local" address. The data won't be transmitted over the "wire"
 *   by the transport service, i.e., the loopback is done on the local
 *   host. This is determined when the socket is created and won't
 *   change in the life of the socket.
 *
 * The constant defined here is based on experimentation.
 *
 * Caution: This must be less than RPC_C_DG_MAX_FRAG_SIZE::dg.h!
 */

#ifndef RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE
#define RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE (8 * 1024)
#endif

/***********************************************************************
 *
 * The IP-specific representation of rpc_port_restriction_list_t.range_list
 * (see com.h).  The low and high are in native machine representation, not
 * network rep.
 */

typedef struct struct_rpc_port_range_element
{
    unsigned32                      low;
    unsigned32                      high;
} rpc_port_range_element_t, *rpc_port_range_element_p_t;

/***********************************************************************
 *
 *  Routine Prototypes for the Internet Extension service routines.
 */

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void rpc__ip_init (
        rpc_naf_epv_p_t             * /*naf_epv*/,
        unsigned32                  * /*status*/
    );

PRIVATE void rpc__ip_desc_inq_addr (
        rpc_protseq_id_t             /*protseq_id*/,
        rpc_socket_t                 /*desc*/,
        rpc_addr_vector_p_t         * /*rpc_addr_vec*/,
        unsigned32                  * /*st*/
    );

PRIVATE void rpc__ip_get_broadcast (
        rpc_naf_id_t                 /*naf_id*/,
        rpc_protseq_id_t             /*rpc_protseq_id*/,
        rpc_addr_vector_p_t         * /*rpc_addrs*/,
        unsigned32                  * /*status*/
    );

PRIVATE void rpc__ip_init_local_addr_vec (
        unsigned32                  * /*status*/
    );

PRIVATE boolean32 rpc__ip_is_local_network (
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

PRIVATE boolean32 rpc__ip_is_local_addr (
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

PRIVATE void rpc__ip_naf_init_func(void);

#ifdef __cplusplus
}
#endif

#endif /* _IPNAF_H */
