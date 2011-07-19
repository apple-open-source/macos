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
**      eebyte.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Callee marshalling and unmarshalling of pointed_at byte's
**
**  VERSION: DCE 1.0
**
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

void

rpc_ss_me_byte
(
    idl_byte *p_node,
    rpc_ss_node_type_k_t NIDL_node_type,
    rpc_ss_marsh_state_t *NIDL_msp
)
{
  long NIDL_already_marshalled;
  unsigned long space_for_node;
  rpc_mp_t mp;
  rpc_op_t op;

  if(p_node==NULL)return;
  if (NIDL_node_type == rpc_ss_mutable_node_k) {
      rpc_ss_register_node(NIDL_msp->node_table,(byte_p_t)p_node,idl_true,&NIDL_already_marshalled);
      if(NIDL_already_marshalled)return;
  }
  space_for_node=((1))+7;
  if (space_for_node > NIDL_msp->space_in_buff)
  {
    rpc_ss_marsh_change_buff(NIDL_msp,space_for_node);
  }
  mp = NIDL_msp->mp;
  op = NIDL_msp->op;
  rpc_marshall_byte(mp, (*p_node));
  rpc_advance_mop(mp, op, 1);
  NIDL_msp->space_in_buff -= (op - NIDL_msp->op);
  NIDL_msp->mp = mp;
  NIDL_msp->op = op;
}

void

rpc_ss_ue_byte
(
    idl_byte **p_referred_to_by,
    rpc_ss_node_type_k_t NIDL_node_type,
    rpc_ss_marsh_state_t *p_unmar_params
)
{
  idl_byte  *p_node = NULL;
  long NIDL_already_unmarshalled = 0;
  unsigned long node_size;
  idl_ulong_int node_number = 0;

  if ( NIDL_node_type == rpc_ss_unique_node_k )
  {
    if (*p_referred_to_by == NULL) return;
    else if (*p_referred_to_by != (idl_byte *)RPC_SS_NEW_UNIQUE_NODE) p_node = *p_referred_to_by;
  }

  if ( NIDL_node_type == rpc_ss_mutable_node_k )
  {
    node_number = (idl_ulong_int)*p_referred_to_by;
    if(node_number==0)return;
  }
  if ( NIDL_node_type == rpc_ss_old_ref_node_k )
   p_node = *p_referred_to_by;
  else if ( p_node == NULL )
  {
    node_size = sizeof(idl_byte );
    if (NIDL_node_type == rpc_ss_mutable_node_k)
    p_node = (idl_byte *)rpc_ss_return_pointer_to_node(
        p_unmar_params->node_table, node_number, node_size,
        NULL, &NIDL_already_unmarshalled, (long *)NULL);
    else
    p_node = (idl_byte *)rpc_ss_mem_alloc(
        p_unmar_params->p_mem_h, node_size );
    *p_referred_to_by = p_node;
    if (NIDL_already_unmarshalled) return;
  }
  if ( NIDL_node_type == rpc_ss_alloc_ref_node_k )
  {
    return;
  }
  if ((unsigned32)((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr) >= p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h, &(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep, p_unmar_params->mp, (*p_node));
  rpc_advance_mop(p_unmar_params->mp, p_unmar_params->op, 1);
}
