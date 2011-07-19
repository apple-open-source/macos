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
**      cnclsm.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Definitions of types/constants internal to the NCA Connection
**  RPC Protocol Service client call state machine.
**
**
*/

#ifndef _CNCALLSM_H
#define _CNCALLSM_H 1

/***********************************************************************/
/*
 * C L I E N T   C A L L   S T A T E S
 */
/*
 * State values are incremented by 100 to distinguish them from
 * action routine indexes which are all < 100.  This was done as
 * an efficiency measure to the engine, rpc__cn_sm_eval_event().
 */
#define RPC_C_CLIENT_CALL_INIT              100
#define RPC_C_CLIENT_CALL_ASSOC_ALLOC_WAIT  101
#define RPC_C_CLIENT_CALL_STUB_WAIT         102
#define RPC_C_CLIENT_CALL_REQUEST           103
#define RPC_C_CLIENT_CALL_RESPONSE          104
#define RPC_C_CLIENT_CALL_CALL_COMPLETED    105
#define RPC_C_CLIENT_CALL_CFDNE             106
#define RPC_C_CLIENT_CALL_CALL_FAILED       107
#define RPC_C_CLIENT_CALL_STATES            108

/***********************************************************************/
/*
 * C L I E N T / S E R V E R   C A L L   E V E N T S
 */

/*
 * Events common to both client and server state machines
 */
#define RPC_C_CALL_SEND                     100
#define RPC_C_CALL_TRANSMIT_REQ             100    /* client */
#define RPC_C_CALL_RPC_RESP                 100    /* server */
#define RPC_C_CALL_RECV                     101
#define RPC_C_CALL_RPC_CONF                 101   /* client */
#define RPC_C_CALL_RPC_IND                  101   /* server */
#define RPC_C_CALL_FAULT_DNE                102
#define RPC_C_CALL_FAULT                    103
#define RPC_C_CALL_LOCAL_ALERT              104
#define RPC_C_CALL_END                      105

/*
 * Events only applicable to client state machine
 */
#define RPC_C_CALL_ALLOC_ASSOC_ACK          106
#define RPC_C_CALL_ALLOC_ASSOC_NAK          107
#define RPC_C_CALL_START_CALL               108
#define RPC_C_CALL_LAST_TRANSMIT_REQ        109
#define RPC_C_CALL_LOCAL_ERR                110
#define RPC_C_CALL_ALERT_TIMEOUT            111
#define RPC_C_CALL_CLIENT_EVENTS            112

/*
 * Events only applicable to server state machine
 */
#define RPC_C_CALL_REMOTE_ALERT_IND         106
#define RPC_C_CALL_ORPHANED                 107
#define RPC_C_CALL_SERVER_EVENTS            108

/***********************************************************************/
/*
 * C L I E N T   C A L L   T A B L E S
 */
EXTERNAL rpc_cn_sm_state_entry_p_t rpc_g_cn_client_call_sm [];
EXTERNAL rpc_cn_sm_action_fn_t rpc_g_cn_client_call_action_tbl [];

#ifdef DEBUG
EXTERNAL const char   *rpc_g_cn_call_client_events [];
EXTERNAL const char   *rpc_g_cn_call_client_states [];
#endif

/***********************************************************************/
/*
 * S E R V E R   C A L L   S T A T E S
 */
#define RPC_C_SERVER_CALL_INIT              100
#define RPC_C_SERVER_CALL_CALL_REQUEST      101
#define RPC_C_SERVER_CALL_CALL_RESPONSE     102
#define RPC_C_SERVER_CALL_CALL_COMPLETED    103

/***********************************************************************/
/*
 * S E R V E R   C A L L   T A B L E S
 */
EXTERNAL rpc_cn_sm_state_entry_p_t rpc_g_cn_server_call_sm [];
EXTERNAL rpc_cn_sm_action_fn_t     rpc_g_cn_server_call_action_tbl [];

#ifdef DEBUG
EXTERNAL const char   *rpc_g_cn_call_server_events [];
EXTERNAL const char   *rpc_g_cn_call_server_states [];
#endif

/*
 * Action routine to invoke in case of a protocol error detected
 * during an illegal state transition.
 */
PRIVATE unsigned32     rpc__cn_call_sm_protocol_error (
        dce_pointer_t /* sc_struct */,
        dce_pointer_t /* event_param */,
	dce_pointer_t /* sm		 */
    );


#ifdef DEBUG

#define RPC_CN_CALL_CLIENT_STATE(state) ( \
    rpc_g_cn_call_client_states[ (state) - RPC_C_CN_STATEBASE ] \
)

#define RPC_CN_CALL_CLIENT_EVENT(event_id) ( \
    rpc_g_cn_call_client_events[ (event_id) - RPC_C_CN_STATEBASE ] \
)

#define RPC_CN_CALL_SERVER_STATE(state) ( \
    rpc_g_cn_call_server_states[ (state) - RPC_C_CN_STATEBASE ] \
)

#define RPC_CN_CALL_SERVER_EVENT(event_id) ( \
    rpc_g_cn_call_server_events[ (event_id) - RPC_C_CN_STATEBASE ] \
)

#endif
/***********************************************************************/
/*
 * R P C _ C N _ C A L L _ S M _ T R C
 */
#ifdef DEBUG

PRIVATE void rpc__cn_call_sm_trace (
    rpc_cn_call_rep_t *         /* call rep */,
    unsigned32                  /* event_id */,
    unsigned32                  /* id */,
    const char *                /* file */,
    const char *                /* funcname */,
    int                         /* lineno */);

#define RPC_CN_CALL_SM_TRC(crep, event_id, id) \
    rpc__cn_call_sm_trace(crep, event_id, id, __FILE__, __func__, __LINE__)

#else

#define RPC_CN_CALL_SM_TRC(crep, event_id, id)

#endif


/***********************************************************************/
/*
 * R P C _ C N _ C A L L _ S M _ T R C _ S T A T E
 */
#ifdef DEBUG

PRIVATE void rpc__cn_call_sm_trace_state (
    rpc_cn_call_rep_t *         /* call rep */,
    unsigned32                  /* id */,
    const char *                /* file */,
    const char *                /* funcname */,
    int                         /* lineno */);

#define RPC_CN_CALL_SM_TRC_STATE(crep, id) \
    rpc__cn_call_sm_trace_state(crep, id, __FILE__, __func__, __LINE__)

#else

#define RPC_CN_CALL_SM_TRC_STATE(crep, id)

#endif

/***********************************************************************/
/*
 * R P C _ C N _ P O S T _ C A L L _ S M _ E V E N T
 *
 * Posts an event to either a server or client call state machine.
 *
 * Sample usage:
 *
 *
 *    rpc_cn_assoc_p_t          assoc;
 *    unsigned8                 event;
 *    rpc_cn_fragbuf_p_t        fragbuf;
 *    unsigned32                st;
 *
 * RPC_CN_POST_CALL_SM_EVENT (assoc, event, fragbuf, st);
 */

#define RPC_CN_POST_CALL_SM_EVENT(assoc, event_id, fragbuf, st) \
{ \
    rpc_cn_call_rep_p_t crep; \
\
    crep = RPC_CN_ASSOC_CALL (assoc); \
    if (crep != NULL) \
    { \
        if (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep)) \
            == \
            RPC_CN_PKT_CALL_ID (RPC_CN_FRAGBUF_PKT_HDR (fragbuf))) \
        { \
            RPC_CN_CALL_SM_TRC (crep, event_id, (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep))));\
            st = rpc__cn_sm_eval_event (event_id, (dce_pointer_t) fragbuf, \
                 (dce_pointer_t) crep, &(crep->call_state)); \
            RPC_CN_CALL_SM_TRC_STATE (crep, (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep)))); \
        } \
        else \
        { \
            (*fragbuf->fragbuf_dealloc)(fragbuf); \
        } \
    } \
    else \
    { \
        (*fragbuf->fragbuf_dealloc)(fragbuf); \
    } \
}

/***********************************************************************/
/*
 * R P C _ C N _ P O S T _ F I R S T _ C A L L _ S M _ E V E N T
 *
 * Posts the first [server] event to the call state machine.
 * This differs from the normal post call sm event because the
 * callid field has not been initialized upon the first server
 * event.
 *
 * Sample usage:
 *
 *    rpc_cn_call_rep_p_t       crep;
 *    rpc_cn_assoc_p_t          assoc;
 *    unsigned8                 event;
 *    rpc_cn_fragbuf_p_t        fragbuf;
 *    unsigned32                st;
 *
 * RPC_CN_POST_FIRST_CALL_SM_EVENT (crep, assoc, event, fragbuf, st);
 */

#define RPC_CN_POST_FIRST_CALL_SM_EVENT(crep, assoc, event_id, fragbuf, st) \
{ \
    rpc__cn_assoc_alloc ((assoc), &(st));\
    if (st == rpc_s_ok)\
    {\
        RPC_CN_CALL_SM_TRC (crep, event_id, (RPC_CN_PKT_CALL_ID (RPC_CN_FRAGBUF_PKT_HDR (fragbuf))));\
        st = rpc__cn_sm_eval_event (event_id, (dce_pointer_t)fragbuf, \
             (dce_pointer_t)(crep), &((crep)->call_state)); \
        RPC_CN_CALL_SM_TRC_STATE (crep, (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep))));\
    }\
}

/***********************************************************************/
/*
 * R P C _ C N _ C A L L _ E V A L _ E V E N T
 *
 * Posts an event from either a client caller or server call
 * executor thread to the call state machine.
 *
 * Sample usage:
 *
 *    rpc_cn_call_rep_p_t       crep;
 *    unsigned8                 event;
 *    dce_pointer_t                 spc_struct;
 *    unsigned32                st;
 *
 * RPC_CN_CALL_EVAL_EVENT (event_id, spc_struct, crep, st);
 */

#define RPC_CN_CALL_EVAL_EVENT(event_id, spc_struct, crep, st)\
{ \
    RPC_CN_CALL_SM_TRC (crep, event_id, (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep))));\
    st = rpc__cn_sm_eval_event ((event_id), (dce_pointer_t)(spc_struct), \
                                (dce_pointer_t)(crep), \
                                &(crep)->call_state); \
    RPC_CN_CALL_SM_TRC_STATE (crep, (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep)))); \
}


/***********************************************************************/
/*
 * R P C _ C N _ C A L L _ I N S E R T _ E V E N T
 *
 * This macro will be called when an event is generated inside an
 * action routine of the call state machine.
 */
#define RPC_CN_CALL_INSERT_EVENT(crep, event)\
{\
    RPC_DBG_PRINTF (rpc_e_dbg_cn_state, RPC_C_CN_DBG_CALL_SM_TRACE, \
                    ("STATE INSERT EVENT ")); \
    RPC_CN_CALL_SM_TRC ((crep), (event)->event_id, (RPC_CN_PKT_CALL_ID ((rpc_cn_packet_p_t) RPC_CN_CREP_SEND_HDR (crep))));\
    rpc__cn_sm_insert_event ((event),\
                             &((crep)->assoc_state));\
}

#endif /* _CNCLSM_H */
