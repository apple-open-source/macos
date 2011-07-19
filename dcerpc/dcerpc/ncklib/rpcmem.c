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
**      rpcmem.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  non-macro version of runtime memory allocator (see rpcmem.h)
**
**
*/

#include <commonp.h>

PRIVATE dce_pointer_t rpc__mem_alloc
(
    size_t size,
    unsigned32 type,
    unsigned32 flags ATTRIBUTE_UNUSED
)
{
    char *addr;

    RPC_MEM_ALLOC_IL(addr, char *, size, type, flags);

#ifdef DEBUG
    if ((type & 0xff) == rpc_g_dbg_switches[rpc_es_dbg_mem_type])
    {
        RPC_DBG_PRINTF(rpc_e_dbg_mem, 1,
	    ("(rpc__mem_alloc) type %x - %lx @ %p\n",
	    type, size, addr));
    }
    else
    {
	RPC_DBG_PRINTF(rpc_e_dbg_mem, 10,
	    ("(rpc__mem_alloc) type %x - %lx @ %p\n",
	    type, size, addr));
    }
#endif

    return addr;
}

PRIVATE dce_pointer_t rpc__mem_realloc
(
    dce_pointer_t addr,
    unsigned32 size,
    unsigned32 type,
    unsigned32 flags ATTRIBUTE_UNUSED
)
{
    RPC_MEM_REALLOC_IL(addr, dce_pointer_t, size, type, flags);

#ifdef DEBUG
    if ((type & 0xff) == rpc_g_dbg_switches[rpc_es_dbg_mem_type])
    {
        RPC_DBG_PRINTF(rpc_e_dbg_mem, 1,
	    ("(rpc__mem_realloc) type %x - %x @ %p\n",
	    type, size, addr));
    }
    else
    {
	RPC_DBG_PRINTF(rpc_e_dbg_mem, 10,
	    ("(rpc__mem_realloc) type %x - %x @ %p\n",
	    type, size, addr));
    }
#endif

    return addr;
}

PRIVATE void rpc__mem_free
(
    dce_pointer_t addr,
    unsigned32 type
)
{

#ifdef DEBUG
    if ((type & 0xff) == rpc_g_dbg_switches[rpc_es_dbg_mem_type])
    {
        RPC_DBG_PRINTF(rpc_e_dbg_mem, 1,
	    ("(rpc__mem_free) type %x @ %p\n",
	    type, addr));
    }
    else
    {
	RPC_DBG_PRINTF(rpc_e_dbg_mem, 10,
	    ("(rpc__mem_free) type %x @ %p\n",
	    type, addr));
    }
#endif

    RPC_MEM_FREE_IL(addr, type);
}
