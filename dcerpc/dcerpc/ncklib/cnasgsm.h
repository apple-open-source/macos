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
**      cnasgsm.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definitions of types/constants internal to the
**  NCA Connection (cn) Association (as) Group (g) State Machine (sm).
**
**
*/


#ifndef _CNASGSM_H
#define _CNASGSM_H  1
/***********************************************************************/
/*
 * R P C _ C N _ A S S O C _ G R P _ S M _ T R C
 */
#ifdef DEBUG
#define RPC_CN_ASSOC_GRP_SM_TRC(assoc_grp, event_id)\
{\
    if ((assoc_grp)->grp_flags & RPC_C_CN_ASSOC_GRP_CLIENT)\
    {\
        RPC_DBG_PRINTF (rpc_e_dbg_cn_state, RPC_C_CN_DBG_ASSOC_GRP_SM_TRACE, \
                        ("STATE CLIENT GRP:    %p state->%s event->%s\n",\
                         assoc_grp,\
                         rpc_g_cn_grp_client_states[(assoc_grp)->grp_state.cur_state-RPC_C_CN_STATEBASE],\
                         rpc_g_cn_grp_client_events[event_id-RPC_C_CN_STATEBASE]));\
    }\
    else\
    {\
        RPC_DBG_PRINTF (rpc_e_dbg_cn_state, RPC_C_CN_DBG_ASSOC_GRP_SM_TRACE, \
                        ("STATE SERVER GRP:    %p state->%s event->%s\n",\
                         assoc_grp,\
                         rpc_g_cn_grp_server_states[(assoc_grp)->grp_state.cur_state-RPC_C_CN_STATEBASE],\
                         rpc_g_cn_grp_server_events[event_id-RPC_C_CN_STATEBASE]));\
    }\
}
#else
#define RPC_CN_ASSOC_GRP_SM_TRC(assoc_grp, event_id)
#endif


/***********************************************************************/
/*
 * R P C _ C N _ A S S O C _ G R P _ S M _ T R C _ S T A T E
 */
#ifdef DEBUG
#define RPC_CN_ASSOC_GRP_SM_TRC_STATE(assoc_grp)\
{\
    if ((assoc_grp)->grp_flags & RPC_C_CN_ASSOC_GRP_CLIENT)\
    {\
        RPC_DBG_PRINTF (rpc_e_dbg_cn_state, RPC_C_CN_DBG_ASSOC_GRP_SM_TRACE, \
                        ("STATE CLIENT GRP:    %x new state->%s\n",\
                         assoc_grp->grp_remid.all,\
                         rpc_g_cn_grp_client_states[(assoc_grp)->grp_state.cur_state-RPC_C_CN_STATEBASE])); \
    }\
    else\
    {\
        RPC_DBG_PRINTF (rpc_e_dbg_cn_state, RPC_C_CN_DBG_ASSOC_GRP_SM_TRACE, \
                        ("STATE SERVER GRP:    %x new state->%s\n",\
                         assoc_grp->grp_id.all,\
                         rpc_g_cn_grp_server_states[(assoc_grp)->grp_state.cur_state-RPC_C_CN_STATEBASE])); \
    }\
}
#else
#define RPC_CN_ASSOC_GRP_SM_TRC_STATE(assoc_grp)
#endif


/***********************************************************************/
/*
 * R P C _ C N _ A S S O C _ G R P _ E V A L _ E V E N T
 *
 * This macro will be called when association group events are detected.
 */
#define RPC_CN_ASSOC_GRP_EVAL_EVENT(assoc_grp, event_id, event_param, st)\
{\
    RPC_CN_ASSOC_GRP_SM_TRC (assoc_grp, event_id);\
    st = rpc__cn_sm_eval_event ((event_id),\
                                (dce_pointer_t) (event_param),\
                                (dce_pointer_t) (assoc_grp),\
                                &((assoc_grp)->grp_state));\
    if ((assoc_grp)->grp_state.cur_state == RPC_C_ASSOC_GRP_CLOSED)\
    {\
        rpc__cn_assoc_grp_dealloc (assoc_grp->grp_id);\
    }\
    RPC_CN_ASSOC_GRP_SM_TRC_STATE (assoc_grp); \
}


/***********************************************************************/
/*
 * R P C _ C N _ A S S O C _ G R P _ I N S E R T _ E V E N T
 *
 * This macro will be called when an event is generated inside an
 * action routine of the association group state machine.
 */
#define RPC_CN_ASSOC_GRP_INSERT_EVENT(assoc_grp, event)\
{\
    RPC_DBG_PRINTF (rpc_e_dbg_cn_state, RPC_C_CN_DBG_ASSOC_GRP_SM_TRACE, \
                    ("STATE INSERT EVENT ")); \
    RPC_CN_ASSOC_SM_TRC ((assoc_grp), (event)->event_id);\
    rpc__cn_sm_insert_event ((event),\
                             &((assoc_grp)->grp_state));\
}

/***********************************************************************/
/*
 * A S S O C   G R P   E V E N T S
 */

/*
 * Events common to both client and server state machines
 *
 * State values are incremented by 100 to distinguish them from
 * action routine indexes which are all < 100.  This was done as
 * an efficiency measure to the engine, rpc__cn_sm_eval_event().
 */
#define RPC_C_ASSOC_GRP_NEW                     100
#define RPC_C_ASSOC_GRP_ADD_ASSOC               101
#define RPC_C_ASSOC_GRP_REM_ASSOC               102

/*
 * Events only applicable to server state machine
 */
#define RPC_C_ASSOC_GRP_NO_CALLS_IND            103


/***********************************************************************/
/*
 * A S S O C   G R P   S T A T E S
 */

/*
 * States common to both client and server state machines
 */
#define RPC_C_ASSOC_GRP_CLOSED                  100
#define RPC_C_ASSOC_GRP_OPEN                    101
#define RPC_C_ASSOC_GRP_ACTIVE                  102

/***********************************************************************/
/*
 * C L I E N T   A S S O C   G R P   S T A T E S
 */

#define RPC_C_CLIENT_ASSOC_GRP_STATES	        103

/***********************************************************************/
/*
 * C L I E N T   A S S O C   G R P   T A B L E S
 */
EXTERNAL rpc_cn_sm_state_entry_p_t rpc_g_cn_client_grp_sm [];
EXTERNAL rpc_cn_sm_action_fn_t     rpc_g_cn_client_grp_action_tbl [];

#if DEBUG
EXTERNAL const char   *rpc_g_cn_grp_client_events [];
EXTERNAL const char   *rpc_g_cn_grp_client_states [];
#endif

/***********************************************************************/
/*
 * S E R V E R   A S S O C   G R P   S T A T E S
 */

#define RPC_C_SERVER_ASSOC_GRP_CALL_WAIT        103
#define RPC_C_SERVER_ASSOC_GRP_STATES           104

/***********************************************************************/
/*
 * S E R V E R    A S S O C   G R P   T A B L E S
 */
EXTERNAL rpc_cn_sm_state_entry_p_t rpc_g_cn_server_grp_sm [];
EXTERNAL rpc_cn_sm_action_fn_t     rpc_g_cn_server_grp_action_tbl [];

#if DEBUG
EXTERNAL const char   *rpc_g_cn_grp_server_events [];
EXTERNAL const char   *rpc_g_cn_grp_server_states [];
#endif
#endif /* _CNASGSM_H */
