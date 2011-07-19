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
**      cnfbuf.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Fragment buffer management routines for connection based
**  protocol services.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* More common declarations */
#include <cnp.h>        /* Connection common declarations */
#include <cnfbuf.h>	/* Fragment buffer declarations */

GLOBAL unsigned32 rpc_g_cn_large_frag_size = RPC_C_CN_LARGE_FRAG_SIZE;


/*
**++
**
**  ROUTINE NAME:       rpc__cn_fragbuf_free
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**
**  Deallocates a large fragment buffer.
**
**  INPUTS:
**
**      buffer_p        Pointer to the large fragment buffer which is to be
**                      deallocated.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    lg_fragbuf_list
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_fragbuf_free
(
   rpc_cn_fragbuf_p_t buffer_p
)
{
#ifdef MAX_DEBUG
    memset ((char *) buffer_p->data_area, 0, rpc_g_cn_large_frag_size);
    memset ((char *) buffer_p, 0, sizeof (rpc_cn_fragbuf_t));
#endif
    rpc__list_element_free (&rpc_g_cn_lg_fbuf_lookaside_list,
                            (dce_pointer_t) buffer_p);
}

/*
**++
**
**  ROUTINE NAME:       rpc__cn_smfragbuf_free
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**
**  Deallocates a small fragment buffer.
**
**  INPUTS:
**
**      buffer_p        Pointer to the small fragment buffer which is to be
**                      deallocated.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    sm_fragbuf_list
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_smfragbuf_free
(
   rpc_cn_fragbuf_p_t      buffer_p
)
{
#ifdef MAX_DEBUG
    memset ((char *) buffer_p->data_area, 0, RPC_C_CN_SMALL_FRAG_SIZE);
    memset ((char *) buffer_p, 0, sizeof (rpc_cn_fragbuf_t));
#endif
    rpc__list_element_free (&rpc_g_cn_sm_fbuf_lookaside_list,
                            (dce_pointer_t) buffer_p );
}

/*
**++
**
**  ROUTINE NAME:       rpc__cn_fragbuf_alloc
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**
**  Allocates a fragment buffer and returns a pointer to it.
**
**  INPUTS:
**
**      alloc_large_buf If TRUE, then allocates a large fragment
**                      buffer.  Otherwise, allocates a small one.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     Address of the allocated fragment buffer.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE rpc_cn_fragbuf_p_t rpc__cn_fragbuf_alloc
(
    boolean32               alloc_large_buf
)
{
    rpc_cn_fragbuf_p_t  fbp;

    /*
     * Get a fragment buffer from the appropriate lookaside list
     */

    if (alloc_large_buf)
    {
	fbp = (rpc_cn_fragbuf_p_t)
            rpc__list_element_alloc (&rpc_g_cn_lg_fbuf_lookaside_list,
                                     true);
        if (fbp != NULL)
        {
            fbp->fragbuf_dealloc = rpc__cn_fragbuf_free;
            fbp->max_data_size = rpc_g_cn_large_frag_size;
        }
        else
        {
            return (NULL);
        }
    }
    else
    {
        fbp = (rpc_cn_fragbuf_p_t)
            rpc__list_element_alloc (&rpc_g_cn_sm_fbuf_lookaside_list,
                                     true);
        if (fbp != NULL)
        {
            fbp->fragbuf_dealloc = rpc__cn_smfragbuf_free;
            fbp->max_data_size = RPC_C_CN_SMALL_FRAG_SIZE;
        }
        else
        {
            return (NULL);
        }
    }

    /*
     * Set the data pointer to an 8 byte aligned boundary.
     */

    fbp->data_p = (dce_pointer_t) RPC_CN_ALIGN_PTR(fbp->data_area, 8);
    memset (fbp->data_area, 0, fbp->max_data_size);

    /*
     * Set up the size of the data being pointed to.
     */
    fbp->data_size = 0;

    /*
     * Return a pointer to the "filled-in" fragment buffer
     */
    return (fbp);
}

/*
**++
**
**  ROUTINE NAME:       rpc__cn_dynfragbuf_free
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**
**  Deallocates a dynamic fragment buffer.
**
**  INPUTS:
**
**      buffer_p        Pointer to the dynamic fragment buffer which is to be
**                      deallocated.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
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

INTERNAL void rpc__cn_dynfragbuf_free
(
   rpc_cn_fragbuf_p_t buffer_p
)
{
#ifdef MAX_DEBUG
    memset ((char *) buffer_p->data_area, 0, buffer_p->max_data_size);
    memset ((char *) buffer_p, 0, sizeof (rpc_cn_fragbuf_t));
#endif

    RPC_MEM_FREE(buffer_p, RPC_C_MEM_CN_PAC_BUF);
}

/*
**++
**
**  ROUTINE NAME:       rpc__cn_fragbuf_alloc_dyn
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**
**  Allocates a fragment buffer and returns a pointer to it.
**
**  INPUTS:
**
**      alloc_size The size of the dynamic allocated fragment buffer.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     Address of the allocated fragment buffer.
**
**  SIDE EFFECTS:       none
**
**--
**/
PRIVATE rpc_cn_fragbuf_p_t rpc__cn_fragbuf_alloc_dyn
(
    unsigned32               alloc_size
)
{
    rpc_cn_fragbuf_p_t  fbp;

    RPC_MEM_ALLOC (fbp,
                   rpc_cn_fragbuf_p_t,
                   sizeof(rpc_cn_fragbuf_t) + alloc_size,
                   RPC_C_MEM_CN_PAC_BUF,
                   RPC_C_MEM_NOWAIT);

    if (fbp != NULL)
    {
        memset(fbp, 0, sizeof(rpc_cn_fragbuf_t));
        fbp->fragbuf_dealloc = rpc__cn_dynfragbuf_free;
        fbp->max_data_size = alloc_size;
    }
    else
    {
        return (NULL);
    }

    /*
     * Set the data pointer to an 8 byte aligned boundary.
     */

    fbp->data_p = (dce_pointer_t) RPC_CN_ALIGN_PTR(fbp->data_area, 8);
    memset(fbp->data_area, 0, fbp->max_data_size);

    /*
     * Set up the size of the data being pointed to.
     */
    fbp->data_size = 0;

    /*
     * Return a pointer to the "filled-in" fragment buffer
     */
    return (fbp);
}
