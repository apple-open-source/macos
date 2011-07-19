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
**      dginit.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**
**
**
*/

#include <dg.h>
#include <dghnd.h>
#include <dgcall.h>
#include <dgslive.h>
#include <dgxq.h>
#include <dgrq.h>
#include <dgpkt.h>
#include <dgcct.h>
#include <dgsct.h>
#include <dgccallt.h>
#include <dgfwd.h>

#include <comprot.h>

#include <comp.h>
void rpc__dg_init_func(void)
{
	static rpc_protocol_id_elt_t prot[1] = {
		{
			rpc__ncadg_init,                /* Datagram-RPC */
			NULL,
			RPC_C_PROTOCOL_ID_NCADG,
			NULL, NULL, NULL, NULL
		}
	};
	rpc__register_protocol_id(prot, 1);
}

void rpc__ncadg_init
(
    rpc_prot_call_epv_t **call_epv,
    rpc_prot_mgmt_epv_t **mgmt_epv,
    rpc_prot_binding_epv_t **binding_epv,
    rpc_prot_network_epv_t **network_epv,
    rpc_prot_fork_handler_fn_t *fork_handler,
    unsigned32 *st
)
{
    static rpc_prot_call_epv_t dg_call_epv =
    {
        .call_start =               rpc__dg_call_start,
        .call_transmit =            rpc__dg_call_transmit,
        .call_transceive =          rpc__dg_call_transceive,
        .call_receive =             rpc__dg_call_receive,
        .call_end =                 rpc__dg_call_end,
        .call_block_until_free =    rpc__dg_call_block_until_free,
        .call_transmit_fault =      rpc__dg_call_fault,
        .call_cancel =              rpc__dg_call_alert,
        .call_receive_fault =       rpc__dg_call_receive_fault,
        .call_did_mgr_execute =     rpc__dg_call_did_mgr_execute
    };
    static rpc_prot_mgmt_epv_t dg_mgmt_epv =
    {
        .mgmt_inq_calls_sent =  rpc__dg_mgmt_inq_calls_sent,
        .mgmt_inq_calls_rcvd =  rpc__dg_mgmt_inq_calls_rcvd,
        .mgmt_inq_pkts_sent =   rpc__dg_mgmt_inq_pkts_sent,
        .mgmt_inq_pkts_rcvd =   rpc__dg_mgmt_inq_pkts_rcvd
    };
    static rpc_prot_binding_epv_t dg_binding_epv =
    {
        .binding_alloc =        rpc__dg_binding_alloc,
        .binding_init =         rpc__dg_binding_init,
        .binding_reset =        rpc__dg_binding_reset,
        .binding_changed =      rpc__dg_binding_changed,
        .binding_free =         rpc__dg_binding_free,
        .binding_inq_addr =     rpc__dg_binding_inq_addr,
        .binding_inq_client =   rpc__dg_binding_inq_client,
        .binding_copy =         rpc__dg_binding_copy,
        .binding_cross_fork =   rpc__dg_binding_cross_fork
    };
    static rpc_prot_network_epv_t dg_network_epv =
    {
        .network_use_socket =   NULL,
        .network_use_protseq =  rpc__dg_network_use_protseq_sv,
        .network_mon =          rpc__dg_network_mon,
        .network_stop_mon =     rpc__dg_network_stop_mon,
        .network_maint =        rpc__dg_network_maint,
        .network_stop_maint =   rpc__dg_network_stop_maint,
        .network_select_disp =  rpc__dg_network_select_dispatch,
        .network_inq_prot_vers =rpc__dg_network_inq_prot_vers,
        .network_close =        rpc__dg_network_close,
        .network_getpeereid =   NULL
    };

    *call_epv    = &dg_call_epv;
    *mgmt_epv    = &dg_mgmt_epv;
    *binding_epv = &dg_binding_epv;
    *network_epv = &dg_network_epv;
#ifdef ATFORK_SUPPORTED
    *fork_handler= rpc__ncadg_fork_handler;
#else
    *fork_handler= NULL;
#endif
    /*
     * Establish a server boot time.
     */

    if (rpc_g_dg_server_boot_time == 0) {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        rpc_g_dg_server_boot_time = (unsigned32) tv.tv_sec;
    }

    rpc__dg_pkt_pool_init();

    rpc__dg_network_init();

    rpc__dg_maintain_init();
    rpc__dg_monitor_init();

    rpc__dg_conv_init();
    rpc__dg_fwd_init();

    if (RPC_DBG(rpc_es_dbg_stats, 5))
    {
        atexit(rpc__dg_stats_print);
    }

    *st = rpc_s_ok;
}

#ifdef ATFORK_SUPPORTED
/*
**++
**
**  ROUTINE NAME:       rpc__ncadg_fork_handler
**
**  SCOPE:              PRIVATE - declared in comprot.h
**
**  DESCRIPTION:
**
**  This routine is called prior to, and immediately after, forking
**  the process's address space.  The input argument specifies which
**  stage of the fork we're currently in.
**
**  INPUTS:
**
**        stage         indicates the stage in the fork operation
**                      (prefork | postfork_parent | postfork_child)
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

void rpc__ncadg_fork_handler
(
    rpc_fork_stage_id_t stage
)
{
    /*
     * Pre-fork handlers are called in reverse order of rpc__ncadg_init().
     * Post-fork handlers are called in same order, except pkt_pool.
     *
     * First, call any module specific fork handlers.
     * Next, handle any stage-specific operations for this
     * module.
     */
    switch ((int)stage)
    {
    case RPC_C_PREFORK:
        rpc__dg_conv_fork_handler(stage);
        rpc__dg_cct_fork_handler(stage);
        rpc__dg_sct_fork_handler(stage);
        rpc__dg_ccallt_fork_handler(stage);
        rpc__dg_monitor_fork_handler(stage);
        rpc__dg_maintain_fork_handler(stage);
        rpc__dg_network_fork_handler(stage);
        rpc__dg_pkt_pool_fork_handler(stage);
        break;
    case RPC_C_POSTFORK_CHILD:
        /*
         * Clear out statistics gathering structure
         */
        /* b_z_e_r_o_((char *) &rpc_g_dg_stats, sizeof(rpc_dg_stats_t)); */

        memset( &rpc_g_dg_stats, 0, sizeof(rpc_dg_stats_t));
        /*
         * Reset Server Boot Time.
         */
        rpc_g_dg_server_boot_time = 0;
        /* fall through */
    case RPC_C_POSTFORK_PARENT:
        /* pkt_pool */
        rpc__dg_network_fork_handler(stage);
        rpc__dg_maintain_fork_handler(stage);
        rpc__dg_monitor_fork_handler(stage);
        rpc__dg_ccallt_fork_handler(stage);
        rpc__dg_sct_fork_handler(stage);
        rpc__dg_cct_fork_handler(stage);
        rpc__dg_conv_fork_handler(stage);
        /*
         * conv_fork_handler() must be called before
         * pkt_pool_fork_handler().
         */
        rpc__dg_pkt_pool_fork_handler(stage);
        break;
    }
}
#endif /* ATFORK_SUPPORTED */
