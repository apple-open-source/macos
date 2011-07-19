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
**      comfwd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Private interface to the Common Communications Forwarding Service for use
**  by RPC Protocol Services and Local Location Broker.  This service is
**  in its own file (rather then com.h) so that the llb does not have to
**  include other runtime internal include files.
**
**
*/

#ifndef _COMFWD_H
#define _COMFWD_H	1

/***********************************************************************/
/*
 * The beginning of this file specifies the "public" (visible to the llbd)
 * portion of the fwd interface.
 */

/*
 * The signature of a server forwarding map function.
 *
 * The function determines an appropriate forwarding address for the
 * packet based on the provided info.  The "fwd_action" output parameter
 * determines the disposition of the packet; "drop" means just drop the
 * packet (don't forward, don't send anything back to the client), "reject"
 * means send a rejection back to the client (and drop the packet), and
 * "forward" means that the packet should be forwarded to the address
 * specified in the "fwd_addr" output parameter.
 *
 * WARNING:  This should be a relatively light weight, non-blocking
 * function as the runtime may (will likely) be calling the function
 * from the context of the runtime's listener thread (i.e. handling
 * of additional incoming received packets is suspended while this
 * provided function is executing).
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    rpc_e_fwd_drop,
    rpc_e_fwd_reject,
    rpc_e_fwd_forward,
    rpc_e_fwd_delayed
} rpc_fwd_action_t;

typedef void (*rpc_fwd_map_fn_t) (
        /* [in] */    uuid_p_t           /*obj_uuid*/,
        /* [in] */    rpc_if_id_p_t      /*if_id*/,
        /* [in] */    rpc_syntax_id_p_t  /*data_rep*/,
        /* [in] */    rpc_protocol_id_t  /*rpc_protocol*/,
        /* [in] */    unsigned32         /*rpc_protocol_vers_major*/,
        /* [in] */    unsigned32         /*rpc_protocol_vers_minor*/,
        /* [in] */    rpc_addr_p_t       /*addr*/,
        /* [in] */    uuid_p_t           /*actuuid*/,
        /* [out] */   rpc_addr_p_t      * /*fwd_addr*/,
        /* [out] */   rpc_fwd_action_t  * /*fwd_action*/,
        /* [out] */   unsigned32        * /*status*/
    );

/*
 * Register a forwarding map function with the runtime.  This registered
 * function will be called by the protocol services to determine an
 * appropriate forwarding endpoint for a received pkt that is not for
 * any of the server's registered interfaces.
 */
PRIVATE void rpc__server_register_fwd_map (
        /* [in] */    rpc_fwd_map_fn_t    /*map_fn*/,
        /* [out] */   unsigned32          * /*status*/
    );

PRIVATE void rpc__server_fwd_resolve_delayed (
	/* [in] */   uuid_p_t            /*actuuid*/,
        /* [in] */   rpc_addr_p_t        /*fwd_addr*/,
        /* [in] */   rpc_fwd_action_t  * /*fwd_action*/,
        /* [out] */  unsigned32        * /*status*/
    );

/***********************************************************************/
/*
 * The following are to be considered internal to the runtime.
 */

/*
 * R P C _ G _ F W D _ F N
 *
 * The global forwarding map function variable.  Its value indicates
 * whether or not the RPC runtime should be performing forwarding services
 * and if so, the forwarding map function to use. Its definition is in comp.c.
 */
EXTERNAL rpc_fwd_map_fn_t       rpc_g_fwd_fn;

#endif /* _COMFWD_H_ */
