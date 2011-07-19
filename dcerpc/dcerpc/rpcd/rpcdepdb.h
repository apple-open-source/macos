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
**      rpcdepdb.h
**
**  FACILITY:
**
**      RPC Daemon
**
**  ABSTRACT:
**
**  Generic Endpoint Database Manager.
**
**
*/

#ifndef RPCDEPDB_H
#define RPCDEPDB_H

typedef void *epdb_handle_t;

/*  Get the handle for the ep database from
 *  a handle to the endpoint object
 */
PRIVATE void epdb_handle_from_ohandle
    (
        handle_t            h,
        epdb_handle_t       *epdb_h,
        error_status_t      *status
    );

/*  Return the handle to the ep database
 */
PRIVATE epdb_handle_t epdb_inq_handle (void);

PRIVATE epdb_handle_t epdb_init
    (
        unsigned char       *pathname,
        error_status_t      *status
    );

PRIVATE void epdb_insert
    (
        epdb_handle_t       h,
        ept_entry_p_t       xentry,
        boolean32           replace,
        error_status_t      *status
    );

PRIVATE void epdb_delete
    (
        epdb_handle_t       h,
        ept_entry_p_t       xentry,
        error_status_t      *status
    );

PRIVATE void epdb_mgmt_delete
    (
        epdb_handle_t       h,
        boolean32           object_speced,
        uuid_p_t            object,
        twr_p_t             tower,
        error_status_t      *status
    );

PRIVATE void epdb_lookup
    (
        epdb_handle_t       h,
        unsigned32          inquiry_type,
        uuid_p_t            object,
        rpc_if_id_p_t       interface,
        unsigned32          vers_option,
        ept_lookup_handle_t *entry_handle,
        unsigned32          max_ents,
        unsigned32          *num_ents,
        ept_entry_t         entries[],
        error_status_t      *status
    );

PRIVATE void epdb_map
    (
        epdb_handle_t       h,
        uuid_p_t            object,
        twr_p_t             map_tower,
        ept_lookup_handle_t *entry_handle,
        unsigned32          max_towers,
        unsigned32          *num_towers,
        twr_t               *fwd_towers[],
        unsigned32          *status
    );

PRIVATE void epdb_fwd
    (
        epdb_handle_t       h,
        uuid_p_t            object,
        rpc_if_id_p_t       interface,
        rpc_syntax_id_p_t   data_rep,
        rpc_protocol_id_t   rpc_protocol,
        unsigned32          rpc_protocol_vers_major,
        unsigned32          rpc_protocol_vers_minor,
        rpc_addr_p_t        addr,
        ept_lookup_handle_t *map_handle,
        unsigned32          max_addrs,
        unsigned32          *num_addrs,
        rpc_addr_p_t        fwd_addrs[],
        unsigned32          *status
    );

PRIVATE void epdb_inq_object
    (
        epdb_handle_t h,
        idl_uuid_t *object,
        error_status_t *status
    );

PRIVATE void epdb_delete_lookup_handle
    (
        epdb_handle_t       h,
        ept_lookup_handle_t *entry_handle
    );

#endif
