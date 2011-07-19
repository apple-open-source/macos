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
**      comnetp.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Network Listener Service *Internal* types, etc...
**      (see comnet.c and comnlsn.c).
**
**/

#ifndef _COMNETP_H
#define _COMNETP_H

/*
 * The max number of socket that the listener can keep track of.
 */

#ifndef RPC_C_SERVER_MAX_SOCKETS
#  define RPC_C_SERVER_MAX_SOCKETS      64
#endif

/*
 * A structure that captures the listener's information about a single socket.
 */

typedef struct
{
    rpc_socket_t                desc;           /* socket descriptor */
    rpc_protseq_id_t            protseq_id;
    rpc_protocol_id_t           protocol_id;
    rpc_prot_network_epv_p_t    network_epv;
    dce_pointer_t                   priv_info;      /* prot service private info */
    unsigned                    busy: 1;        /* T => contains valid data */
    unsigned                    is_server: 1;   /* T => created via use_protseq */
    unsigned                    is_dynamic: 1;  /* T => dynamically alloc'd endpoint */
    unsigned                    is_active: 1;   /* T => events should NOT be discarded */
} rpc_listener_sock_t, *rpc_listener_sock_p_t;

/*
 * A structure that captures the listener's state that needs to be shared
 * between modules.
 */

typedef struct
{
    rpc_mutex_t         mutex;
    rpc_cond_t          cond;
    unsigned16          num_desc;    /* number "busy" */
    unsigned16          high_water;  /* highest entry in use */
    unsigned32          status;      /* used to convey information about */
                                     /* the state of the table.  see     */
                                     /* rpc_server_listen.               */
    unsigned32          idle_timeout_secs;
    rpc_listener_sock_t socks[ RPC_C_SERVER_MAX_SOCKETS ];
    unsigned            reload_pending: 1;
} rpc_listener_state_t, *rpc_listener_state_p_t;

/*
 * The operations provided by any implementation of a Network Listener
 * "thread".
 */

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void rpc__nlsn_activate_desc (
        rpc_listener_state_p_t  /*lstate*/,
        unsigned32              /*idx*/,
        unsigned32              * /*status*/
    );

PRIVATE void rpc__nlsn_deactivate_desc (
        rpc_listener_state_p_t  /*lstate*/,
        unsigned32              /*idx*/,
        unsigned32              * /*status*/
    );

PRIVATE void rpc__nlsn_fork_handler (
        rpc_listener_state_p_t  /*lstate*/,
        rpc_fork_stage_id_t /*stage*/
    );

#ifdef __cplusplus
}
#endif

#endif /* _COMNETP_H */
