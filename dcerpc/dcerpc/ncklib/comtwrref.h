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
**
**  NAME:
**
**      nsrttwr.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Header file containing macros, definitions, typedefs and prototypes of
**      exported routines from the nsrttwr.c module.
**
**
**/

#ifndef _COMTWRREF_H
#define _COMTWRREF_H

/*
 * Type Definitions
 */

/*
 * Prototypes
 */

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void rpc__tower_ref_add_floor (
    unsigned32           /*floor_number*/,
    rpc_tower_floor_p_t  /*floor*/,
    rpc_tower_ref_t     * /*tower_ref*/,
    unsigned32          * /*status*/
);

PRIVATE void rpc__tower_ref_alloc (
    unsigned8           * /*tower_octet_string*/,
    unsigned32           /*num_flrs*/,
    unsigned32           /*start_flr*/,
    rpc_tower_ref_p_t   * /*tower_ref*/,
    unsigned32          * /*status*/
);

PRIVATE void rpc__tower_ref_copy (
    rpc_tower_ref_p_t    /*source_tower*/,
    rpc_tower_ref_p_t   * /*dest_tower*/,
    unsigned32          * /*status*/
);

PRIVATE void rpc__tower_ref_free (
    rpc_tower_ref_p_t       * /*tower_ref*/,
    unsigned32              * /*status*/
);

PRIVATE void rpc__tower_ref_inq_protseq_id (
    rpc_tower_ref_p_t    /*tower_ref*/,
    rpc_protseq_id_t    * /*protseq_id*/,
    unsigned32          * /*status*/
);

PRIVATE boolean rpc__tower_ref_is_compatible (
    rpc_if_rep_p_t           /*if_spec*/,
    rpc_tower_ref_p_t        /*tower_ref*/,
    unsigned32              * /*status*/
);

PRIVATE void rpc__tower_ref_vec_free (
    rpc_tower_ref_vector_p_t    * /*tower_vector*/,
    unsigned32                  * /*status*/
);

PRIVATE void rpc__tower_ref_vec_from_binding (
    rpc_if_rep_p_t               /*if_spec*/,
    rpc_binding_handle_t         /*binding*/,
    rpc_tower_ref_vector_p_t    * /*tower_vector*/,
    unsigned32                  * /*status*/
);

#endif /* _COMTWRREF_H */
