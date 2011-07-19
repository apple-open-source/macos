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
**      nbaseool.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Support routines to un/marshall idl_uuid_t
**
**  VERSION: DCE 1.0
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#define IDL_CAUX
#define IDL_SAUX

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

void rpc_ss_m_uuid
(
idl_uuid_t *p_node,
rpc_ss_marsh_state_t *NIDL_msp
)
{

  /* local variables */
  unsigned long space_for_node;
  rpc_mp_t mp;
  rpc_op_t op;

#ifdef PERFMON
    RPC_SS_M_UUID_N;
#endif

  space_for_node=((4)+(2)+(2)+(1)+(1)+(0)+(6*1))+7;
  if (space_for_node > NIDL_msp->space_in_buff)
  {
    rpc_ss_marsh_change_buff(NIDL_msp,space_for_node);
  }
  mp = NIDL_msp->mp;
  op = NIDL_msp->op;
  rpc_align_mop(mp, op, 4);
  rpc_marshall_ulong_int(mp, (*p_node).time_low);
  rpc_advance_mp(mp, 4);
  rpc_marshall_ushort_int(mp, (*p_node).time_mid);
  rpc_advance_mp(mp, 2);
  rpc_marshall_ushort_int(mp, (*p_node).time_hi_and_version);
  rpc_advance_mp(mp, 2);
  rpc_marshall_usmall_int(mp, (*p_node).clock_seq_hi_and_reserved);
  rpc_advance_mp(mp, 1);
  rpc_marshall_usmall_int(mp, (*p_node).clock_seq_low);
  rpc_advance_mp(mp, 1);

#ifdef PACKED_BYTE_ARRAYS
  memcpy((char *)mp, (char *)&(*p_node).node[0], (5-0+1)*1);
  rpc_advance_mp(mp, (5-0+1)*1);
#else
  IDL_element_0 = &(*p_node).node[0];
  rpc_marshall_byte(mp, IDL_element_0[0]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[1]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[2]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[3]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[4]); rpc_advance_mp(mp,1);
  rpc_marshall_byte(mp, IDL_element_0[5]); rpc_advance_mp(mp,1);
#endif
  rpc_advance_op(op, 16);
  NIDL_msp->space_in_buff -= (op - NIDL_msp->op);
  NIDL_msp->mp = mp;
  NIDL_msp->op = op;

#ifdef PERFMON
    RPC_SS_M_UUID_X;
#endif

}

void rpc_ss_u_uuid
(
idl_uuid_t *p_node,
rpc_ss_marsh_state_t *p_unmar_params
)
{

  /* local variables */
  idl_byte *IDL_element_1;

#ifdef PERFMON
    RPC_SS_U_UUID_N;
#endif

  rpc_align_mop(p_unmar_params->mp, p_unmar_params->op, 4);
  if ((unsigned32)((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr) >=
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h,
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_ulong_int(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, (*p_node).time_low);
  rpc_advance_mp(p_unmar_params->mp, 4);

  if ((unsigned32)((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr) >=
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h,
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_ushort_int(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, (*p_node).time_mid);
  rpc_advance_mp(p_unmar_params->mp, 2);
  rpc_convert_ushort_int(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, (*p_node).time_hi_and_version);
  rpc_advance_mp(p_unmar_params->mp, 2);

  if ((unsigned32)((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr) >=
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h,
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_usmall_int(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, (*p_node).clock_seq_hi_and_reserved);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_usmall_int(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, (*p_node).clock_seq_low);
  rpc_advance_mp(p_unmar_params->mp, 1);
  IDL_element_1 = &(*p_node).node[0];
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, IDL_element_1[0]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, IDL_element_1[1]);
  rpc_advance_mp(p_unmar_params->mp, 1);

  if ((unsigned32)((byte_p_t)p_unmar_params->mp - p_unmar_params->p_rcvd_data->data_addr) >=
p_unmar_params->p_rcvd_data->data_len)
  {
    rpc_ss_new_recv_buff(p_unmar_params->p_rcvd_data, p_unmar_params->call_h,
&(p_unmar_params->mp), &(*p_unmar_params->p_st));
  }
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, IDL_element_1[2]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, IDL_element_1[3]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, IDL_element_1[4]);
  rpc_advance_mp(p_unmar_params->mp, 1);
  rpc_convert_byte(p_unmar_params->src_drep, ndr_g_local_drep,
p_unmar_params->mp, IDL_element_1[5]);
  rpc_advance_mp(p_unmar_params->mp, 1);

  rpc_advance_op(p_unmar_params->op, 16);

#ifdef PERFMON
    RPC_SS_U_UUID_X;
#endif

}
