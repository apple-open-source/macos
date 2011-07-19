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
**      comcthd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definitions of types/constants for the Call Thread Services
**  of the Common Communications Service component of the RPC runtime.
**
**
*/

#ifndef _COMCTHD_H
#define _COMCTHD_H	1

#ifdef _cplusplus
extern "C" {
#endif

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ I N I T
 *
 */

PRIVATE void rpc__cthread_init (
        unsigned32                  * /*status*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ S T A R T _ A L L
 *
 */

PRIVATE void rpc__cthread_start_all (
        unsigned32              /*default_pool_cthreads*/,
        unsigned32              * /*status*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ S T O P _ A L L
 *
 */

PRIVATE void rpc__cthread_stop_all (
        unsigned32              * /*status*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ I N V O K E _ N U L L
 *
 */

PRIVATE void rpc__cthread_invoke_null (
        rpc_call_rep_p_t        /*call_rep*/,
        uuid_p_t                /*object*/,
        uuid_p_t                /*if_uuid*/,
        unsigned32              /*if_ver*/,
        unsigned32              /*if_opnum*/,
        rpc_prot_cthread_executor_fn_t /*cthread_executor*/,
        dce_pointer_t               /*call_args*/,
        unsigned32              * /*status*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ D E Q U E U E
 *
 */

PRIVATE boolean32 rpc__cthread_dequeue (
        rpc_call_rep_p_t        /*call*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ C A N C E L
 *
 */

PRIVATE void rpc__cthread_cancel (
        rpc_call_rep_p_t        /*call*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ C A N C E L _ C A F
 *
 */

PRIVATE boolean32 rpc__cthread_cancel_caf (
        rpc_call_rep_p_t        /*call*/
    );

/***********************************************************************/
/*
 * R P C _ _ C T H R E A D _ C A N C E L _ E N A B L E _ P O S T I N G
 *
 */
 PRIVATE void rpc__cthread_cancel_enable_post (
        rpc_call_rep_p_t        /*call*/
    );

#ifdef _cplusplus
}
#endif

#endif /* _COMCTHD_H */
