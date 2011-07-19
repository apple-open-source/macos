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
**      cnsm.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Interface to the NCA Connection Protocol State Machine Service
**
**
*/

#ifndef _CNSM_H
#define _CNSM_H	1

/*
 * R P C _ _ C N _ S M _ I N I T
 */

void rpc__cn_sm_init (
    rpc_cn_sm_state_entry_p_t   */* state_tbl */,
    rpc_cn_sm_action_fn_p_t     /* action_tbl */,
    rpc_cn_sm_ctlblk_p_t         /* sm */,
    unsigned32			 /* tbl_id */);

/*
 * R P C _ _ C N _ S M _ E V A L _ E V E N T
 */

unsigned32     rpc__cn_sm_eval_event (
    unsigned32                  /* event_id */,
    dce_pointer_t                   /* event_parameter */,
    dce_pointer_t                   /* spc_struct */,
    rpc_cn_sm_ctlblk_p_t         /* sm */);

/*
 * R P C _ _ C N _ S M _ I N I T _ E V E N T _ L I S T
 */

void rpc__cn_sm_init_event_list (rpc_cn_sm_ctlblk_t  *);

/*
 * R P C _ _ C N _ S M _ I N S E R T _ E V E N T
 */

void rpc__cn_sm_insert_event (
    rpc_cn_sm_event_entry_p_t   /* event */,
    rpc_cn_sm_ctlblk_t          * /* sm */);

/*
**++
**
**  MACRO NAME:       RPC_CN_INCR_ACTIVE_SVR_ACTION
**
**  SCOPE:            Internal to the rpc;  used here and in cnassoc.c.
**
**  DESCRIPTION:
**
**  MACRO to set the active predicate to true. The server
**  runtime allocated the association for the new call and its
**  callbacks. Only one call and its related callbacks may allocate an
**  association at a time.  This macro includes the essence of
**  incr_active_action_rtn.
**
**  INPUTS:
**
**      assoc		Pointer to the association.
**
**   	sm 		The control block from the event evaluation
**                      routine.  Input is the current state and
**                      event for the control block.  Output is the
**                      next state or updated current state, for the
**                      control block.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            Modifies the association reference count and the
**			current state of the control block.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/
#define RPC_CN_INCR_ACTIVE_SVR_ACTION(assoc, sm)\
{\
    	RPC_CN_DBG_RTN_PRINTF(SERVER rpc_cn_incr_active_svr_action_macro); \
	assoc->assoc_ref_count++;\
	sm->cur_state = RPC_C_SERVER_ASSOC_OPEN;\
}



/*
**++
**
**  MACRO NAME:         RPC_CN_INCR_ACTIVE_CL_ACTION
**
**  SCOPE:              GLOBAL
**
**  DESCRIPTION:
**
**  Action client side macro, to set the active predicate to true. The client
**  runtime allocated the association for the new call and its
**  callbacks. Only one call and its related callbacks may allocate an
**  association at a time.
**
**  INPUTS:
**
**      assoc		A pointer to the association.
**
**   	sm 		The control block from the event evaluation
**                      routine.  Input is the current state and
**                      event for the control block.  Output is the
**                      next state or updated current state, for the
**                      control block.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            Modifies the association reference count and the
**			current state of the control block.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
*/
#define RPC_CN_INCR_ACTIVE_CL_ACTION(assoc, sm)\
{\
    RPC_CN_DBG_RTN_PRINTF(CLIENT rpc_cn_incr_active_cl_action_macro); \
    assoc->assoc_ref_count++;\
    sm->cur_state = RPC_C_CLIENT_ASSOC_ACTIVE;\
}
/*
**++
**
**  MACRO NAME:       RPC_CN_DECR_ACTIVE_SVR_ACTION
**
**  SCOPE:            Internal to the rpc;  used here and in cnassoc.c.
**
**  DESCRIPTION:
**
**  MACRO to set the active predicate to true. The server
**  runtime allocated the association for the new call and its
**  callbacks. Only one call and its related callbacks may allocate an
**  association at a time.  This macro includes the essence of
**  decr_active_action_rtn.
**
**  INPUTS:
**
**      assoc		Pointer to the association.
**
**   	sm 		The control block from the event evaluation
**                      routine.  Input is the current state and
**                      event for the control block.  Output is the
**                      next state or updated current state, for the
**                      control block.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            Modifies the association reference count and the
**			current state of the control block.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/
#define RPC_CN_DECR_ACTIVE_SVR_ACTION(assoc, sm)\
{\
    	RPC_CN_DBG_RTN_PRINTF(SERVER rpc_cn_decr_active_svr_action_macro); \
	assoc->assoc_ref_count--;\
	sm->cur_state = RPC_C_SERVER_ASSOC_OPEN;\
}



#define RPC_CN_SM_GET_NEXT_EVENT(sm, event, more)\
{\
    if ((sm)->event_list_state == RPC_C_CN_SM_EVENT_LIST_EMPTY)\
    {\
        more = false;\
    }\
    else\
    {\
        (event)->event_id = (sm)->event_list[(sm)->event_list_hindex].event_id;\
        (event)->event_param = (sm)->event_list[(sm)->event_list_hindex].event_param;\
        (sm)->event_list_hindex = ((sm)->event_list_hindex + 1) &\
        (RPC_C_CN_SM_EVENT_LIST_MAX_ENTRIES - 1); \
        if ((sm)->event_list_hindex == (sm)->event_list_tindex)\
        {\
            (sm)->event_list_state = RPC_C_CN_SM_EVENT_LIST_EMPTY;\
        }\
        more = true;\
    }\
}

#endif
