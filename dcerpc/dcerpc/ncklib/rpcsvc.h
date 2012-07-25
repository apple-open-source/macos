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
 * Macros, typedefs, and function prototypes
 * for Serviceability.
 */

#ifndef _RPCSVC_H
#define _RPCSVC_H	1

#ifdef	DCE_RPC_SVC

#include <dce/dce.h>
#include <dce/dce_msg.h>
#include <dce/dce_svc.h>
#include <dce/dcerpcmac.h>
#include <dce/dcerpcmsg.h>
#include <dce/dcerpcsvc.h>

#include <string.h>
#endif

#ifndef PD_BUILD

void rpc_dce_svc_printf (
     const char* file,
     unsigned int line,
     const char *format,
     unsigned32 dbg_switch,
     unsigned32 sev_action_flags,
     unsigned32 error_code,
     ... )
#if __GNUC__
__attribute__((__format__ (__printf__, 3, 7)))
#endif
;

typedef enum {
    svc_c_sev_fatal     = 0x00000001,
    svc_c_sev_error     = 0x00000002,
    svc_c_sev_warning   = 0x00000004
} svc_c_severity_t;

typedef enum {
    svc_c_action_abort      = 0x00010000,
    svc_c_action_exit_bad   = 0x00020000,
    svc_c_action_none       = 0x00040000
} svc_c_action_t;

#define rpc_svc_general rpc_es_dbg_general
#define rpc_svc_xmit rpc_es_dbg_xmit
#define rpc_svc_recv rpc_es_dbg_recv
#define rpc_svc_cn_state rpc_es_dbg_cn_state
#define rpc_svc_cn_pkt rpc_es_dbg_cn_pkt
#define rpc_svc_auth rpc_es_dbg_auth
#define rpc_svc_mem rpc_es_dbg_mem
#define rpc_svc_cn_errors rpc_es_dbg_cn_errors
#define rpc_svc_server_call rpc_es_dbg_server_call
#define rpc_svc_libidl rpc_es_dbg_libidl
#define rpc_svc_dg_pkt rpc_es_dbg_dg_pkt

/*
 * New debug switches map to offsets in
 * rpc_g_dbg_switches in kernel.  In user
 * space, they map to S12Y sub-components.
 */
#define rpc_e_dbg_general rpc_es_dbg_general
#define rpc_e_dbg_mutex rpc_es_dbg_mutex
#define rpc_e_dbg_xmit  rpc_es_dbg_xmit
#define rpc_e_dbg_recv rpc_es_dbg_recv
#define rpc_e_dbg_dg_lossy rpc_es_dbg_dg_lossy
#define rpc_e_dbg_dg_state rpc_es_dbg_dg_state
#define rpc_e_dbg_ip_max_pth_unfrag_tpdu \
    rpc_es_dbg_ip_max_pth_unfrag_tpdu
#define rpc_e_dbg_ip_max_loc_unfrag_tpdu \
    rpc_es_dbg_ip_max_loc_unfrag_tpdu
#define rpc_e_dbg_dds_max_pth_unfrag_tpdu \
    rpc_es_dbg_dds_max_pth_unfrag_tpdu
#define rpc_e_dbg_dds_max_loc_unfrag_tpdu \
    rpc_es_dbg_dds_max_loc_unfrag_tpdu
#define rpc_e_dbg_dg_rq_qsize rpc_es_dbg_dg_rq_qsize
#define rpc_e_dbg_cancel rpc_es_dbg_cancel
#define rpc_e_dbg_orphan rpc_es_dbg_orphan
#define rpc_e_dbg_cn_state rpc_es_dbg_cn_state
#define rpc_e_dbg_cn_pkt rpc_es_dbg_cn_pkt
#define rpc_e_dbg_pkt_quotas rpc_es_dbg_pkt_quotas
#define rpc_e_dbg_auth rpc_es_dbg_auth
#define rpc_e_dbg_source rpc_es_dbg_source
#define rpc_e_dbg_pkt_quota_size rpc_es_dbg_pkt_quota_size
#define rpc_e_dbg_stats rpc_es_dbg_stats
#define rpc_e_dbg_mem rpc_es_dbg_mem
#define rpc_e_dbg_mem_type rpc_es_dbg_mem_type
#define rpc_e_dbg_dg_pktlog rpc_es_dbg_dg_pktlog
#define rpc_e_dbg_thread_id rpc_es_dbg_thread_id
#define rpc_e_dbg_timer rpc_es_dbg_timestamp
#define rpc_e_dbg_timestamp rpc_es_dbg_timestamp
#define rpc_e_dbg_cn_errors rpc_es_dbg_cn_errors
#define rpc_e_dbg_conv_thread rpc_es_dbg_conv_thread
#define rpc_e_dbg_pid rpc_es_dbg_pid
#define rpc_e_dbg_atfork rpc_es_dbg_atfork
#define rpc_e_dbg_cma_thread rpc_es_dbg_cma_thread
#define rpc_e_dbg_inherit rpc_es_dbg_inherit
#define rpc_e_dbg_dg_sockets rpc_es_dbg_dg_sockets
#define rpc_e_dbg_ip_max_tsdu rpc_es_dbg_ip_max_tsdu
#define rpc_e_dbg_dg_max_psock rpc_es_dbg_dg_max_psock
#define rpc_e_dbg_dg_max_window_size \
    rpc_es_dbg_dg_max_window_size
#define rpc_e_dbg_threads rpc_es_dbg_threads
#define rpc_e_dbg_last_switch rpc_es_dbg_last_switch

#ifdef DEBUG
#define RPC_DBG2(switch, level) \
         (rpc_g_dbg_switches[(int) (switch)] >= (level))

#endif /* DEBUG */

#else /* ! PD_BUILD */

#ifdef	DCE_RPC_SVC

/*
 * Map the old debug switches to the
 * new serviceability "sub-components".
 */

#define	rpc_e_dbg_general	rpc_svc_general
#define	rpc_e_dbg_mutex		rpc_svc_mutex
#define	rpc_e_dbg_xmit		rpc_svc_xmit
#define	rpc_e_dbg_recv		rpc_svc_recv
#define	rpc_e_dbg_dg_state	rpc_svc_dg_state
#define	rpc_e_dbg_cancel	rpc_svc_cancel
#define	rpc_e_dbg_orphan	rpc_svc_orphan
#define	rpc_e_dbg_cn_state	rpc_svc_cn_state
#define	rpc_e_dbg_cn_pkt	rpc_svc_cn_pkt
#define	rpc_e_dbg_pkt_quotas	rpc_svc_pkt_quotas
#define	rpc_e_dbg_auth		rpc_svc_auth
#define	rpc_e_dbg_stats		rpc_svc_stats
#define	rpc_e_dbg_mem		rpc_svc_mem
#define	rpc_e_dbg_dg_pktlog	rpc_svc_dg_pktlog
#define	rpc_e_dbg_conv_thread	rpc_svc_conv_thread
#define	rpc_e_dbg_atfork	rpc_svc_atfork
#define	rpc_e_dbg_dg_sockets	rpc_svc_dg_sockets
#define	rpc_e_dbg_timer		rpc_svc_timer
#define	rpc_e_dbg_threads	rpc_svc_threads

#define EPRINTF           	rpc__svc_eprintf

/*
 * Buffer size for messages
 */
#define	RPC__SVC_MSG_SZ	300

#ifdef DEBUG

/*
 * Map "old" RPC debug levels to new serviceability
 * debug levels.
 */
#define	RPC__SVC_DBG_LEVEL(level) \
    ((unsigned32)(level > 9 ? 9 : (level < 1 ? 1 : level)))

/*
 * Buffer size for debug messages
 */
#define	RPC__SVC_DBG_MSG_SZ	RPC__SVC_MSG_SZ

#define	RPC_DBG2(switch,level) \
    DCE_SVC_DEBUG_ATLEAST(RPC__SVC_HANDLE,switch,RPC__SVC_DBG_LEVEL(level))

/*
 * R P C _ D B G _ P R I N T F
 *
 * A macro that prints debug info based on a debug switch's level.  Note
 * that this macro is intended to be used as follows:
 *
 *      RPC_DBG_PRINTF(rpc_e_dbg_xmit, 3, ("Sent pkt %d", pkt_count));
 *
 * I.e. the third parameter is the argument list to "printf" and must be
 * enclosed in parens.  The macro is designed this way to allow us to
 * eliminate all debug code when DEBUG is not defined.
 *
 */

/*
 * Recoded to use serviceability debug interface ...
 *
 * Call a function that can deal with the "pargs"
 * argument and then call DCE_SVC_DEBUG ...
 *
 * rpc__svc_fmt_dbg_msg returns a pointer to malloc()'ed
 * storage, since trying to use internal allocation
 * interfaces could lead to infinte recursion.  Finite
 * recursion (due to pargs containing a call to a function
 * that calls RPC_DBG_PRINTF) prevents an implementation
 * that uses a static buffer protected by a mutex.
 *
 * Storage is deallocated with free().
 */

#define RPC_DBG_PRINTF(switch, level, pargs)						\
    if( DCE_SVC_DEBUG_ATLEAST(RPC__SVC_HANDLE,switch,RPC__SVC_DBG_LEVEL(level)) )	\
    {											\
        char *__mptr = rpc__svc_fmt_dbg_msg pargs ;					\
        DCE_SVC_DEBUG((RPC__SVC_HANDLE, switch, RPC__SVC_DBG_LEVEL(level), __mptr));	\
        free(__mptr);									\
    }

#endif	/* DEBUG */

/*
 * R P C _ _ S V C _ E P R I N T F
 *
 * Format arguments and print as a serviceability
 * debug message.  (This routine is what EPRINTF
 * now evaluates to.)
 *
 * We need stdarg and all DCE code is now
 * supposed to be ANSI.
 */

#include <stdarg.h>

int rpc__svc_eprintf (
	char          * /*fmt*/,
	...
    );

#ifdef DEBUG
/*
 * R P C _ _ S V C _ F M T _ D B G _ MSG
 */

/*
 * Requires ANSI prototype for stdargs
 * DCE is supposed to be pure ANSI now anyway ...
 *
 * Called only by RPC_DBG_PRINTF macro.
 */

#include <stdarg.h>

char * rpc__svc_fmt_dbg_msg (
	char          * /*fmt*/,
	...
    );

#endif	/* DEBUG */

#endif	/* DCE_RPC_SVC */
#endif /* ! PD_BUILD */
#ifndef DCE_RPC_SVC

#ifndef DEBUG

#define RPC_DBG_PRINTF(switch, level, pargs)	do {;} while(0)
#define RPC_DBG2(switch, level) 		(0)

#endif /* DEBUG */

/*
 * Handle name defined in rpc.sams ...
 */
#define	RPC__SVC_HANDLE		rpc_g_svc_handle

#ifndef DIE
#  define DIE(text)         rpc__die(text, __FILE__, __LINE__)
#endif /* DIE */

#endif	/* DCE_RPC_SVC */

#endif /* _RPCSVC_H */
