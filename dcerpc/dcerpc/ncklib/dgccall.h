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
**      dgccall.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  DG protocol service routines
**
**
*/

#ifndef _DGCCALL_H
#define _DGCCALL_H	1

#include <dgccallt.h>

/*
 * R P C _ D G _ C C A L L _ S E T _ S T A T E _ I D L E
 *
 * Remove the call handle from the CCALLT.  Release our reference to
 * our CCTE.  (In the case of CCALLs created to do server callbacks there
 * won't be a ccte_ref.)  Change to the idle state.  If you're trying
 * to get rid of the ccall use rpc__dg_ccall_free_prep() instead.
 */

#define RPC_DG_CCALL_SET_STATE_IDLE(ccall) { \
    if ((ccall)->c.state == rpc_e_dg_cs_final) \
        rpc__dg_ccall_ack(ccall); \
    rpc__dg_ccallt_remove(ccall); \
    if (! (ccall)->c.is_cbk)\
        RPC_DG_CCT_RELEASE(&(ccall)->ccte_ref); \
    RPC_DG_CALL_SET_STATE(&(ccall)->c, rpc_e_dg_cs_idle); \
}

/*
 * R P C _ D G _ C C A L L _ R E L E A S E
 *
 * Decrement the reference count for the CCALL and
 * NULL the reference.
 */

#define RPC_DG_CCALL_RELEASE(ccall) { \
    RPC_DG_CALL_LOCK_ASSERT(&(*(ccall))->c); \
    assert((*(ccall))->c.refcnt > 0); \
    if (--(*(ccall))->c.refcnt == 0) \
        rpc__dg_ccall_free(*(ccall)); \
    else \
        RPC_DG_CALL_UNLOCK(&(*(ccall))->c); \
    *(ccall) = NULL; \
}

/*
 * R P C _ D G _ C C A L L _ R E L E A S E _ N O _ U N L O C K
 *
 * Like RPC_DG_CCALL_RELEASE, except doesn't unlock the CCALL.  Note
 * that the referencing counting model requires that this macro can be
 * used iff the release will not be the "last one" (i.e., the one that
 * would normally cause the CCALL to be freed).
 */

#define RPC_DG_CCALL_RELEASE_NO_UNLOCK(ccall) { \
    RPC_DG_CALL_LOCK_ASSERT(&(*(ccall))->c); \
    assert((*(ccall))->c.refcnt > 1); \
    --(*(ccall))->c.refcnt; \
    *(ccall) = NULL; \
}

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void rpc__dg_ccall_lsct_inq_scall (
        rpc_dg_ccall_p_t  /*ccall*/,
        rpc_dg_scall_p_t * /*scallp*/
    );

PRIVATE void rpc__dg_ccall_lsct_new_call (
        rpc_dg_ccall_p_t  /*ccall*/,
        rpc_dg_sock_pool_elt_p_t  /*si*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_scall_p_t * /*scallp*/
    );

PRIVATE void rpc__dg_ccall_ack (
        rpc_dg_ccall_p_t /*ccall*/
    );

PRIVATE void rpc__dg_ccall_free (
        rpc_dg_ccall_p_t /*ccall*/
    );

PRIVATE void rpc__dg_ccall_free_prep (
        rpc_dg_ccall_p_t /*ccall*/
    );

PRIVATE void rpc__dg_ccall_timer ( dce_pointer_t /*p*/ );

PRIVATE void rpc__dg_ccall_xmit_cancel_quit (
        rpc_dg_ccall_p_t  /*ccall*/,
        unsigned32 /*cancel_id*/
    );

PRIVATE void rpc__dg_ccall_setup_cancel_tmo (
        rpc_dg_ccall_p_t /*ccall*/
    );

#ifdef __cplusplus
}
#endif

#endif /* _DGCCALL_H */
