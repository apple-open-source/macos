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
**      cnxfer.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definitions of entrypoints to support buffered data transfer
**  within the Connection-oriented protocol services component of
**  the RPC runtime.
**
**
*/

#ifndef _CNXFER_H
#define _CNXFER_H	1

/***********************************************************************/
/*
 * rpc_c_cn_bcopy_lim determines the maximum byte count which we
 * will copy instead of allocating an I/O vector element.  i.e., if
 * the stub data contains less than or equal to rpc_c_cn_bcopy_lim,
 * then the data will be copied to an internal buffer.
 */
#define RPC_C_CN_BCOPY_LIM 200

/***********************************************************************/
/*
 * R P C _ _ C N _ C O P Y _ B U F F E R
 *
 */

PRIVATE void rpc__cn_copy_buffer (
        rpc_cn_call_rep_p_t /* call_rep */,
        rpc_iovector_elt_p_t /* iov_elt_p */,
        unsigned32     * /* status */
    );

/***********************************************************************/
/*
 * R P C _ _ C N _ T R A N S M I T _ B U F F E R S
 *
 */

PRIVATE void rpc__cn_transmit_buffers (
        rpc_cn_call_rep_p_t /* call_rep */,
        unsigned32     */* status */
    );

#if 0
/***********************************************************************/
/*
 * R P C _ _ C N _ F L U S H _ B U F F E R S
 *
 */

PRIVATE void rpc__cn_flush_buffers (
        rpc_cn_call_rep_p_t /* call_rep */,
        unsigned32     */* status */
    );
#endif /* 0 */

/***********************************************************************/
/*
 * R P C _ _ C N _ A D D _ N E W _ I O V E C T O R _ E L M T
 *
 */

PRIVATE void rpc__cn_add_new_iovector_elmt (
        rpc_cn_call_rep_p_t /* call_rep */,
        rpc_iovector_elt_p_t /* iov_elt_p */,
        unsigned32     */* status */
        );

/***********************************************************************/
/*
 * R P C _ _ C N _ D E A L L O C _ B U F F E R E D _ D A T A
 *
 */

PRIVATE void rpc__cn_dealloc_buffered_data (
     rpc_cn_call_rep_p_t /*call_rep*/
    );

/***********************************************************************/
/*
 * R P C _ _ C N _ G E T _ A L L O C _ H I N T
 *
 */

PRIVATE unsigned32 rpc__cn_get_alloc_hint (
    rpc_iovector_p_t /* stub_data_p */
    );


/*
 * Macro to fix up the iovector in the call_rep so that we have
 * only the cached protocol header (and no stub data).
 */
#define RPC_CN_FREE_ALL_EXCEPT_PROT_HDR(call_rep) \
{\
    RPC_CN_CREP_IOVLEN (call_rep) = 1; \
    RPC_CN_CREP_CUR_IOV_INDX (call_rep) = 0; \
    RPC_CN_CREP_ACC_BYTCNT (call_rep) = RPC_CN_CREP_SIZEOF_HDR (call_rep); \
    RPC_CN_CREP_FREE_BYTES (call_rep) = \
        RPC_C_CN_SMALL_FRAG_SIZE - RPC_CN_CREP_SIZEOF_HDR (call_rep); \
    RPC_CN_CREP_FREE_BYTE_PTR (call_rep) = \
        (byte_p_t) RPC_CN_CREP_IOV(call_rep)[0].data_addr; \
    RPC_CN_CREP_FREE_BYTE_PTR (call_rep) += RPC_CN_CREP_SIZEOF_HDR (call_rep); \
    RPC_CN_CREP_IOV(call_rep)[0].data_len = RPC_CN_CREP_SIZEOF_HDR (call_rep);\
    if ((call_rep)->sec != NULL)\
    {\
        RPC_CN_CREP_FREE_BYTE_PTR (call_rep) -= call_rep->prot_tlr->data_size; \
        RPC_CN_CREP_IOVLEN (call_rep)++;\
    }\
}
#endif /* _CNXFER_H */
