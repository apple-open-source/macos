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
**      cnassoc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Interface to the NCA Connection Protocol Service's Association Service.
**
**
*/

#ifndef _CNASSOC_H
#define _CNASSOC_H 1

/******************************************************************************/
/*
 * R P C _ C N _ A S S O C _ A C B _ I N C _ R E F
 */

#ifdef RPC_CN_DEBUG_REFCNT
static void RPC_CN_ASSOC_ACB_INC_REF(rpc_cn_assoc_t *assoc)
{
    (assoc)->assoc_acb_ref_count++;
    RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_GENERAL,
                    ("(RPC_CN_ASSOC_ACB_INC_REF) assoc->%x new refcnt->%d\n",
                     assoc, assoc->assoc_acb_ref_count));
}
#else
#define RPC_CN_ASSOC_ACB_INC_REF(assoc) (assoc)->assoc_acb_ref_count++;
#endif /* RPC_CN_DEBUG_REFCNT */

/*
 * R P C _ C N _ A S S O C _ A C B _ D E C _ R E F
 */

#ifdef RPC_CN_DEBUG_REFCNT
static void RPC_CN_ASSOC_ACB_DEC_REF(rpc_cn_assoc_t *assoc)
{
    (assoc)->assoc_acb_ref_count--;
    RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_GENERAL,
                    ("(RPC_CN_ASSOC_ACB_DEC_REF) assoc->%x new refcnt->%d\n",
                     assoc, assoc->assoc_acb_ref_count));
}
#else
#define RPC_CN_ASSOC_ACB_DEC_REF(assoc) (assoc)->assoc_acb_ref_count--;
#endif /* RPC_CN_DEBUG_REFCNT */

/*
 * R P C _ C N _ A S S O C _ G R P
 */

#define RPC_CN_ASSOC_GRP(grp_id)\
    (RPC_CN_LOCAL_ID_VALID (grp_id)) ?\
        &rpc_g_cn_assoc_grp_tbl.assoc_grp_vector[(grp_id).parts.id_index] : NULL;

/*
 * R P C _ C N _ A S S O C _ S Y N T A X _ E Q U A L
 */
#if (uuid_c_version == 1)
#define RPC_CN_ASSOC_SYNTAX_EQUAL(s1, s2, st)\
    ((memcmp (&((s1)->id), &((s2)->id), sizeof (idl_uuid_t)) == 0) &&\
     (((s1)->version & 0xFFFF) == ((s2)->version & 0xFFFF)) &&\
     (((s1)->version >> 16) == ((s2)->version >> 16)))
#else
error "***Make sure memcmp works on this version of UUIDs***"
#endif

/*
 * R P C _ C N _ A S S O C _ C A L L
 */

#define RPC_CN_ASSOC_CALL(assoc)          (assoc)->call_rep

/*
 * R P C _ C N _ A S S O C _ M A X _ X M I T _ F R A G
 */

#define RPC_CN_ASSOC_MAX_XMIT_FRAG(assoc) (assoc)->assoc_max_xmit_frag

/*
 * R P C _ C N _ A S S O C _ M A X _ R E C V _ F R A G
 */

#define RPC_CN_ASSOC_MAX_RECV_FRAG(assoc) (assoc)->assoc_max_recv_frag

/*
 * R P C _ C N _ A S S O C _ C O N T E X T _ I D
 */

#define RPC_CN_ASSOC_CONTEXT_ID(assoc)     (assoc)->assoc_pres_context_id

/*
 * R P C _ C N _ A S S O C _ N D R _ F O R M A T
 */

#define RPC_CN_ASSOC_NDR_FORMAT(assoc)      (assoc)->assoc_remote_ndr_format

/*
 * R P C _ C N _ A S S O C _ S E C U R I T Y
 */

#define RPC_CN_ASSOC_SECURITY(assoc)        &(assoc)->security

/*
 * R P C _ C N _ A S S O C _ W A K E U P
 */

#define RPC_CN_ASSOC_WAKEUP(assoc)          rpc__cn_assoc_queue_dummy_frag(assoc);

/*
 * R P C _ C N _ A S S O C _ C A N C E L _ A N D _ W A K E U P
 */

#define RPC_CN_ASSOC_CANCEL_AND_WAKEUP(assoc)\
{\
    RPC_CALL_LOCK (((rpc_call_rep_t *) assoc->call_rep));\
    rpc__cthread_cancel (((rpc_call_rep_t *) assoc->call_rep));\
    rpc__cn_assoc_queue_dummy_frag(assoc);\
    RPC_CALL_UNLOCK (((rpc_call_rep_t *) assoc->call_rep));\
}


/******************************************************************************/
/*
 * R P C _ _ C N _ A S S O C _  R E Q U E S T
 */

rpc_cn_assoc_t *rpc__cn_assoc_request (
    rpc_cn_call_rep_p_t          /* call_r */,
    rpc_cn_binding_rep_p_t       /* binding_r */,
    rpc_if_rep_p_t               /* if_r */,
    rpc_transfer_syntax_t       * /* syntax */,
    unsigned16                  * /* context_id */,
    rpc_cn_sec_context_p_t      * /* sec */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ L I S T E N
 */

rpc_cn_assoc_t *rpc__cn_assoc_listen (
    rpc_socket_t                 /* newsock */,
    unsigned_char_p_t            /* endpoint */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ A L L O C
 */

PRIVATE void rpc__cn_assoc_alloc (
    rpc_cn_assoc_p_t             /* assoc */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ D E A L L O C
 */

PRIVATE void rpc__cn_assoc_dealloc (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_cn_call_rep_p_t          /* call_rep */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ A B O R T
 */

void rpc__cn_assoc_abort (
    rpc_cn_assoc_p_t             /* assoc */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ P O P _ C A L L
 */

PRIVATE rpc_cn_call_rep_t *rpc__cn_assoc_pop_call (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_cn_call_rep_p_t         /* call_rep */ );

/*
 * R P C _ _ C N _ A S S O C _ P U S H _ C A L L
 */

PRIVATE void rpc__cn_assoc_push_call (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_cn_call_rep_p_t         /* call_r */,
    unsigned32                   * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ Q U E U E _ F R A G
 */

PRIVATE void rpc__cn_assoc_queue_frag (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_cn_fragbuf_p_t           /* fragbuf */,
    boolean32                   /* signal */ );

/*
 * R P C _ _ C N _ A S S O C _ Q U E U E _ D U M M Y _ F R A G
 */

PRIVATE void rpc__cn_assoc_queue_dummy_frag (
    rpc_cn_assoc_p_t            /* assoc */ );

/*
 * R P C _ _ C N _ A S S O C _ R E C E I V E _ F R A G
 */

PRIVATE void rpc__cn_assoc_receive_frag (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_cn_fragbuf_p_t          * /* frag_buf */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S E N D _ F R A G
 */

PRIVATE void rpc__cn_assoc_send_frag (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_iovector_p_t             /* iovector */,
    rpc_cn_sec_context_p_t       /* sec */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S E N D _ F R A G B U F
 */

PRIVATE void rpc__cn_assoc_send_fragbuf (
    rpc_cn_assoc_p_t             /* assoc */,
    rpc_cn_fragbuf_p_t           /* fragbuf */,
    rpc_cn_sec_context_p_t       /* sec */,
    boolean32                    /* freebuf */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S Y N T A X _ F R E E
 */

PRIVATE void rpc__cn_assoc_syntax_free (
    rpc_cn_syntax_p_t           */* syntax */ );

/*
 * R P C _ _ C N _ A S S O C _ S Y N T A X _ N E G O T I A T E
 */

PRIVATE void rpc__cn_assoc_syntax_negotiate (
    rpc_cn_assoc_p_t                 /* assoc */,
    rpc_cn_pres_cont_list_p_t        /* pres_cont_list */,
    unsigned32                      * /* size */,
    rpc_cn_pres_result_list_t       * /* pres_result_list */,
    unsigned32                      * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S Y N T A X _ L K U P _ B Y _ I D
 */

PRIVATE void rpc__cn_assoc_syntax_lkup_by_id (
    rpc_cn_assoc_p_t             /* assoc */,
    unsigned32                   /* context_id */,
    rpc_cn_syntax_p_t           * /* pres_context */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S Y N T A X _ L K U P _ B Y _ C L
 */

PRIVATE void rpc__cn_assoc_syntax_lkup_by_cl (
    rpc_cn_assoc_p_t             /* assoc */,
    unsigned32                   /* call_id */,
    rpc_cn_syntax_p_t           * /* pres_context */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S E C _ L K U P _ B Y _ I D
 */

PRIVATE void rpc__cn_assoc_sec_lkup_by_id (
    rpc_cn_assoc_p_t             /* assoc */,
    unsigned32                   /* key_id */,
    rpc_cn_sec_context_p_t      * /* sec */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S E C _ L K U P _ B Y _ CL
 */

PRIVATE void rpc__cn_assoc_sec_lkup_by_cl (
    rpc_cn_assoc_p_t		 /* assoc */,
    unsigned32                   /* call_id */,
    rpc_cn_sec_context_p_t      * /* sec */,
    unsigned32    		* /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S E C _ A L L O C
 */

PRIVATE rpc_cn_sec_context_t *rpc__cn_assoc_sec_alloc (
    rpc_auth_info_p_t            /* info */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S E C _ F R E E
 */

PRIVATE void rpc__cn_assoc_sec_free (
    rpc_cn_sec_context_p_t      */* sec */ );

/*
 * R P C _ _ C N _ A S S O C _ P O S T _ E R R O R
 */

PRIVATE void rpc__cn_assoc_post_error (
    rpc_cn_assoc_p_t             /* assoc */,
    unsigned32                  /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ S M _ P R O T O C O L _ E R R O R
 */

PRIVATE unsigned32  rpc__cn_assoc_sm_protocol_error (
    dce_pointer_t                    /* spc_struct */,
    dce_pointer_t                   /* event_param */,
    dce_pointer_t                   /* sm */ );

/*
 * R P C _ _ C N _ A S S O C _ S T A T U S _ T O _ P R E J
 */

PRIVATE unsigned32  rpc__cn_assoc_status_to_prej (
    unsigned32                  /* prej */ );

/*
 * R P C _ _ C N _ A S S O C _ P R E J _ T O _ S T A T U S
 */

PRIVATE unsigned32  rpc__cn_assoc_prej_to_status (
    unsigned32                  /* prej */ );

/*
 * R P C _ _ C N _ A S S O C _ P P R O V _ T O _ S T A T U S
 */

PRIVATE unsigned32  rpc__cn_assoc_pprov_to_status (
    unsigned32                  /* pprov */ );

/*
 * R P C _ _ C N _ A S S O C _ A C B _ C R E A T E
 */

void rpc__cn_assoc_acb_create ( rpc_cn_assoc_p_t/* assoc */ );

/*
 * R P C _ _ C N _ A S S O C _ A C B _ F R E E
 */

void rpc__cn_assoc_acb_free ( rpc_cn_assoc_p_t /* assoc */ );

/*
 * R P C _ _ C N _ A S S O C _ A C B _ D E A L L O C
 */

PRIVATE void rpc__cn_assoc_acb_dealloc (rpc_cn_assoc_p_t/* assoc */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ A L L O C
 */

PRIVATE rpc_cn_local_id_t rpc__cn_assoc_grp_alloc (
    rpc_addr_p_t             /* rpc_addr */,
    rpc_transport_info_p_t    /* prot_info */,
    unsigned32               /* type */,
    unsigned32               /* rem_id */,
    unsigned32              * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ D E A L L O C
 */

PRIVATE void rpc__cn_assoc_grp_dealloc (
    rpc_cn_local_id_t       /* grp_id */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ A D D _ A S S O C
 */

PRIVATE void rpc__cn_assoc_grp_add_assoc (
    rpc_cn_local_id_t        /* grp_id */,
    rpc_cn_assoc_p_t        /* assoc */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ R E M _ A S S O C
 */

PRIVATE void rpc__cn_assoc_grp_rem_assoc (
    rpc_cn_local_id_t        /* grp_id */,
    rpc_cn_assoc_p_t        /* assoc */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ L K U P _ B Y _ A D D R
 */

PRIVATE rpc_cn_local_id_t rpc__cn_assoc_grp_lkup_by_addr (
    rpc_addr_p_t                 /* rpc_addr */,
    rpc_transport_info_p_t       /* transport_info */,
    unsigned32                   /* type */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ L K U P _ B Y _ R E M I D
 */

PRIVATE rpc_cn_local_id_t rpc__cn_assoc_grp_lkup_by_remid (
    unsigned32                   /* rem_id */,
    unsigned32                   /* type */,
    rpc_addr_p_t                 /* rpc_addr */,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ L K U P _ B Y _ I D
 */

PRIVATE rpc_cn_local_id_t rpc__cn_assoc_grp_lkup_by_id (
    rpc_cn_local_id_t            /* grp_id */,
    unsigned32                   /* type */,
    rpc_transport_info_p_t  transport_info,
    unsigned32                  * /* st */ );

/*
 * R P C _ _ C N _ A S S O C _ G R P _ T B L _ I N I T
 */

PRIVATE void rpc__cn_assoc_grp_tbl_init (void);

/*
 * R P C _ _ C N _ G R P _ S M _ P R O T O C O L _ E R R O R
 */

PRIVATE unsigned32  rpc__cn_grp_sm_protocol_error (
    dce_pointer_t                    /* spc_struct */,
    dce_pointer_t                   /* event_param */,
    dce_pointer_t                   /* sm */);

#endif
