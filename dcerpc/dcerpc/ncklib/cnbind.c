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
**      cnbind.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  The NCA Connection Protocol Service's Binding Service.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Common communications services */
#include <comprot.h>    /* Common protocol services */
#include <cnp.h>        /* NCA Connection private declarations */
#include <cnpkt.h>	/* NCA Connection packet encoding */
#include <cnassoc.h>    /* NCA Connection association service */
#include <cnid.h>       /* NCA Connection local id service */
#include <cnbind.h>


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_alloc
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine will allocate a binding rep data structure. A
**      binding rep contains a protocol specific part and therefore
**      must be allocated by the protocol service.
**
**  INPUTS:
**
**      is_server       Boolean indicating server-side binding handle should
**                      be created.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      return          The allocated binding rep structure.
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE rpc_binding_rep_t *rpc__cn_binding_alloc
(
  boolean32               is_server,
  unsigned32              *st
)
{
    rpc_cn_binding_rep_p_t brp;

    CODING_ERROR (st);

    /*
     * A server binding is allocated from the receiver thread which
     * has already acquired the CN global mutex.
     */
    if (!is_server)
    {
        RPC_CN_LOCK ();
    }
    else
    {
        RPC_CN_LOCK_ASSERT ();
    }

    /*
     * Allocate a binding rep off the lookaside list.
     */
    brp = (rpc_cn_binding_rep_p_t)
        rpc__list_element_alloc (&rpc_g_cn_binding_lookaside_list,
                                 true);
    if (brp == NULL)
    {
        *st = rpc_s_no_memory;
        RPC_CN_UNLOCK ();
        return NULL;
    }
    if (!is_server)
    {
        ((rpc_cn_binding_rep_t *)brp)->being_resolved = false;
        RPC_CN_UNLOCK ();
    }

    brp->common.rpc_addr = NULL;
    brp->common.auth_info = NULL;
    brp->common.transport_info = NULL;

    *st = rpc_s_ok;
    return ((rpc_binding_rep_p_t) brp);
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_init
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine will initialize the protocol specific part of a
**      previously allocated binding rep data structure. For the NCA
**      Connection protocol this means locating an association group
**      whose primary address matches that in the binding rep. Note
**      that this can only be done if an RPC address is supplied.
**
**  INPUTS:
**
**      binding_r       The binding rep to be initialized.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_init
(
 rpc_binding_rep_p_t     binding_r,
 unsigned32              *st
)
{
    unsigned32                  type;
    rpc_cn_local_id_t           grp_id;

    CODING_ERROR (st);

    /*
     * Determine the type of association group we are looking for.
     */
    if (RPC_BINDING_IS_SERVER (binding_r))
    {
        //type = RPC_C_CN_ASSOC_GRP_SERVER;
    }
    else
    {
        type = RPC_C_CN_ASSOC_GRP_CLIENT;

        /*
         * Use the RPC address contained in the binding rep to find an
         * association group.
         */
        RPC_CN_LOCK ();
        grp_id = rpc__cn_assoc_grp_lkup_by_addr (binding_r->rpc_addr,
                                                 binding_r->transport_info,
                                                 type,
                                                 st);

        /*
         * Record its the association group id returned into the
         * binding rep. Note that the group id returned may be invalid.
         */
        ((rpc_cn_binding_rep_t *)binding_r)->grp_id = grp_id;
        RPC_CN_UNLOCK ();
    }

    /*
     * Return a good status code no matter what happened.
     */
    *st = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_reset
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine will clear the endpoint in the RPC address
**      contained in the binding rep data structure given.
**
**  INPUTS:
**
**      binding_r       The binding rep containing the RPC address
**                      to be reset.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_reset
(
  rpc_binding_rep_p_t     binding_r,
  unsigned32              *st
)
{
    CODING_ERROR (st);

    /*
     * The endpoint in the binding is being reset. Since we assume
     * endpoints are tied to address spaces and an association group
     * id identifies an address space we should reset the group ID.
     * When the endpoint is added to the binding
     * rpc__cn_binding_init should be called again to set the group id.
     */
    RPC_CN_LOCAL_ID_CLEAR (((rpc_cn_binding_rep_t *)binding_r)->grp_id);

    *st = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_changed
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine is called when something in the common part of
**      the binding rep given has changed.
**
**  INPUTS:
**
**      binding_r       The binding rep which changed.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_changed
(
  rpc_binding_rep_p_t     binding_r,
  unsigned32              *st
)
{
    CODING_ERROR (st);

    /*
     * The exact field which changed is not known. Init the binding.
     */
    rpc__cn_binding_init (binding_r, st);
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_free
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine frees the binding rep data structure given.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      binding_r       The binding rep to be freed
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_free
(
  rpc_binding_rep_p_t     *binding_r,
  unsigned32              *st
)
{
    CODING_ERROR (st);

    RPC_CN_LOCK_ASSERT ();

    /*
     * Put the binding rep back on the lookaside list.
     */
    rpc__list_element_free (&rpc_g_cn_binding_lookaside_list,
                            (dce_pointer_t) *binding_r);

    /*
     * Null the caller's pointer to this memory.
     */
    *binding_r = NULL;
    *st = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_inq_addr
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine allocates and returns the RPC address
**      associated with the binding rep data structure given.
**
**  INPUTS:
**
**      binding_r       The binding rep to be freed
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The RPC address which is allocated.
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**      rpc_s_assoc_grp_not_found
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_inq_addr
(
  rpc_binding_rep_p_t     binding_r,
  rpc_addr_p_t            *rpc_addr,
  unsigned32              *st
)
{
    unsigned32          type;
    rpc_cn_local_id_t   grp_id;
    rpc_cn_assoc_grp_t  *assoc_grp;
    rpc_protseq_id_t    protseq_id;

    CODING_ERROR (st);

    /*
     * Determine the type of association group we are looking for.
     */
    if (RPC_BINDING_IS_SERVER (binding_r))
    {
        type = RPC_C_CN_ASSOC_GRP_SERVER;
    }
    else
    {
        type = RPC_C_CN_ASSOC_GRP_CLIENT;
    }

    /*
     * Use the group id contained in the binding rep to find an
     * association group.
     */
    RPC_CN_LOCK ();
    grp_id = rpc__cn_assoc_grp_lkup_by_id (
                 ((rpc_cn_binding_rep_t *)binding_r)->grp_id,
                 type,
                 binding_r->transport_info,
                 st);

    /*
     * Check whether an association group was found or not.
     */
    if (RPC_CN_LOCAL_ID_VALID (grp_id))
    {
        /*
         * An association group was found. Check whether it
         * contains an RPC address.
         */
        assoc_grp = RPC_CN_ASSOC_GRP (grp_id);
        assert(assoc_grp != NULL);
        if (assoc_grp->grp_address != NULL)
        {
            /*
             * The association group contains an RPC address.
             * Copy it into the binding.
             */
            rpc__naf_addr_copy (assoc_grp->grp_address,
                                rpc_addr,
                                st);
        }
        else
        {
            /*
             * The association doesn't have an RPC address. Find
             * it out using one of the connections attached to the
             * association group.
             */
            rpc__naf_desc_inq_protseq_id
                (((rpc_cn_assoc_t *)assoc_grp->grp_assoc_list.next)->cn_ctlblk.cn_sock,
                 RPC_C_PROTOCOL_ID_NCACN,
                 &protseq_id,
                 st);
            if (*st == rpc_s_ok)
            {
                rpc__naf_desc_inq_peer_addr
                    (((rpc_cn_assoc_t *)assoc_grp->grp_assoc_list.next)->cn_ctlblk.cn_sock,
                     protseq_id,
                     rpc_addr,
                     st);
                if (*st == rpc_s_ok)
                {
                    rpc__naf_addr_copy (*rpc_addr,
                                        &assoc_grp->grp_address,
                                        st);
                }
            }
        }
        binding_r->rpc_addr = *rpc_addr;
    }
    else
    {
        *st = rpc_s_connection_closed;
    }
    RPC_CN_UNLOCK ();
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_inq_client
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine returns the client process identifier, i.e. the
**      association group id, for a given binding rep.
**
**  INPUTS:
**
**      binding_r       The binding rep to be freed
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The RPC address which is allocated.
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_inq_client
(
  rpc_binding_rep_p_t     binding_r,
  rpc_client_handle_t     *client_h,
  unsigned32              *st
)
{
    rpc_cn_local_id_t   grp_id;

    CODING_ERROR (st);

    /*
     * Get the association group id from the binding rep.
     */
    grp_id = ((rpc_cn_binding_rep_t *)binding_r)->grp_id;

    /*
     * Make sure the group id is valid.
     */
    *client_h = (rpc_client_handle_t) grp_id.all;
    if (RPC_CN_LOCAL_ID_VALID (grp_id))
    {
        *st = rpc_s_ok;
    }
    else
    {
        *st = rpc_s_invalid_binding;
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_copy
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine will init the CN specific fields of a copied
**      binding rep.
**
**  INPUTS:
**
**      src_binding_r   The binding rep to be copied.
**      dst_binding_r   The binding rep being copied into.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_copy
(
  rpc_binding_rep_p_t     src_binding_r,
  rpc_binding_rep_p_t     dst_binding_r,
  unsigned32              *st
)
{
    CODING_ERROR (st);

    /*
     * Get the association group id from the src binding rep and put
     * it in the dst binding rep.
     */
    ((rpc_cn_binding_rep_t *)dst_binding_r)->grp_id =
        ((rpc_cn_binding_rep_t *)src_binding_r)->grp_id;

    *st = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_binding_cross_fork
**
**  SCOPE:              PRIVATE - declared in cnbind.h
**
**  DESCRIPTION:
**
**      This routine makes it possible for children of forks to use
**      binding handles inherited from their parents.
**
**  INPUTS:
**
**      binding_r       The binding rep to be inherited
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      rpc_s_coding_error
**      rpc_s_ok
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__cn_binding_cross_fork
(
  rpc_binding_rep_p_t     binding_r ATTRIBUTE_UNUSED,
  unsigned32              *st
)
{
    CODING_ERROR (st);

    /*
     * This is a dummy function to avoid the null reference
     * causing a core dump.
     */

    *st = rpc_s_ok;
}
