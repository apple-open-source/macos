/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**  NAME:
**
**      comnet.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Network Listener Service Interface.
**
**      This file provides (1) all of the PUBLIC Network Listener Service
**      API operations, and (2) the "portable" PRIVATE service operations.
**      
**
**
*/

#include <commonp.h>    /* Common internals for RPC Runtime system  */
#include <com.h>        /* Externals for Common Services component  */
#include <comprot.h>    /* Externals for common Protocol Services   */
#include <comnaf.h>     /* Externals for common NAF Services        */
#include <comp.h>       /* Internals for Common Services component  */
#include <comcthd.h>    /* Externals for Call Thread sub-component  */
#include <comnetp.h>    /* Internals for Network sub-component      */
#include <comfwd.h>     /* Externals for Common Services Fwd comp   */

/*
*****************************************************************************
*
* local data structures
*
*****************************************************************************
*/

/*
 * Miscellaneous Data Declarations
 */

/*
 * Data Declarations for rpc_network_inq_protseqs()
 *
 * Note: These are setup at initialization time
 */

INTERNAL rpc_protseq_vector_p_t psv = NULL; /* ptr to local protseq vector */
INTERNAL int    psv_size;	/* mem alloc size for protseq vector  */
INTERNAL int    psv_str_size;	/* mem alloc size for protseq strings */

#define PSV_SIZE        sizeof (rpc_protseq_vector_t) + \
                        RPC_C_PROTSEQ_MAX * (RPC_C_PROTSEQ_ID_MAX-1)

/*
 * The state of the listener thread that need to be shared across modules.
 */
INTERNAL rpc_listener_state_t       listener_state;


/*
**++
**
**  ROUTINE NAME:       rpc_network_inq_protseqs
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Return all protocol sequences supported by both the Common
**  Communication Service and the operating system.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      protseq_vec     The vector of RPC protocol sequences supported by
**                      this RPC runtime system.
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_no_protseqs
**                          rpc_s_no_memory
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_network_inq_protseqs (protseq_vec, status)

rpc_protseq_vector_p_t  *protseq_vec;
unsigned32              *status;

{
    unsigned32              psid;       /* loop index into protseq id table */
    unsigned_char_p_t       ps;         /* pointer to protseq string        */
    rpc_protseq_vector_p_t  pvp;        /* local pointer to protseq vector  */


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    /*
     * Return with status if there aren't any protocol sequences.
     */
    if (psv->count == 0)
    {
        *status = rpc_s_no_protseqs;
        return;
    }

    /*
     * Mem alloc the return vector plus the required string space.
     */
    RPC_MEM_ALLOC (
        pvp,
        rpc_protseq_vector_p_t,
        psv_size + psv_str_size,
        RPC_C_MEM_PROTSEQ_VECTOR,
        RPC_C_MEM_WAITOK);

    *protseq_vec = pvp;

    /*
     * Copy the local protseq vector to the users return vector
     * and setup a pointer to the start of the returned strings.
     */
    /* b_c_o_p_y ((char *) psv, (char *) pvp, psv_size); */
    memmove((char *)pvp, (char *)psv, psv_size) ;
    ps = (unsigned_char_p_t) (((char *)pvp) + psv_size);
 
    /*
     * Loop through the local protocol sequence id table:
     *   - copy each protseq string to the return vector string space
     *   - bump the string space pointer
     */
    for (psid = 0; psid < psv->count; psid++)
    {
        pvp->protseq[psid] = ps;
        strcpy ((char *) ps, (char *) psv->protseq[psid]);
        ps += strlen ((char *) ps) + 1;
    }

    *status = rpc_s_ok;
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc_network_is_protseq_valid
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine determines whether the Common Communications Service
**  supports a given RPC Protocol Sequence.
**
**  INPUTS:
**
**      rpc_protseq     The RPC protocol sequence whose validity is to be
**                      determined.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      return  -   true if the protocol sequence is supported
**                  false if the protocol sequence is not supported
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 rpc_network_is_protseq_valid (rpc_protseq, status)

unsigned_char_p_t       rpc_protseq;
unsigned32              *status;

{
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    /*
     * Find the correct entry in the RPC Protocol Sequence ID table using the
     * RPC Protocol Sequence string passed in as an argument.
     */
    (void) rpc__network_pseq_id_from_pseq (rpc_protseq, status);

    if (*status == rpc_s_ok)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_protseq_vector_free
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will free the RPC Protocol Sequence strings pointed to in
**  the vector and the vector itself.
**
**  Note: The service that allocates this vector (rpc_network_inq_protseqs())
**      mem alloc()'s the memory required for the vector in one large chunk.
**      We therefore don't have to play any games, we just free once
**      for the base vector pointer.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      protseq_vec     The vector of RPC protocol sequences to be freed.
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_protseq_vector_free (protseq_vector, status)

rpc_protseq_vector_p_t  *protseq_vector;
unsigned32              *status;

{
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_MEM_FREE (*protseq_vector, RPC_C_MEM_PROTSEQ_VECTOR);

    *protseq_vector = NULL;

    *status = rpc_s_ok;
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc_network_monitor_liveness
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine tells the Common Communication Service to call the routine
**  provided if communications are lost to the process represented by the
**  client handle provided.
**
**  INPUTS:
**
**      binding_h       The binding on which to monitor liveness.
**
**      client_handle   The client for which liveness is to be monitored.
**
**      rundown_fn      The routine to be called if communications are lost.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_invalid_binding
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_network_monitor_liveness 
(
    rpc_binding_handle_t    binding_h,
    rpc_client_handle_t     client_handle,
    rpc_network_rundown_fn_t rundown_fn,
    unsigned32              *status
)
{
    rpc_protocol_id_t       protid;
    rpc_prot_network_epv_p_t net_epv;
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * Get the protocol id from the binding handle (binding_rep) 
     */

    protid = binding_rep->protocol_id;
    net_epv = RPC_PROTOCOL_INQ_NETWORK_EPV (protid);


    /*
     * Pass through to the network protocol routine.
     */
    (*net_epv->network_mon)
        (binding_rep, client_handle, rundown_fn, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_network_stop_monitoring
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine tells the Common Communication Service to cancel
**  rpc_network_monitor_liveness.
**
**  INPUTS:
**
**      binding_h       The binding on which to stop monitoring liveness.
**
**      client_handle   The client for which liveness monitoring is to be
**                      stopped.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_invalid_binding
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_network_stop_monitoring (binding_h, client_h, status)

rpc_binding_handle_t        binding_h;
rpc_client_handle_t         client_h;
unsigned32                  *status;

{
    rpc_protocol_id_t       protid;
    rpc_prot_network_epv_p_t net_epv;
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * Get the protocol id from the binding handle (binding_rep) 
     */
    protid = binding_rep->protocol_id;
    net_epv = RPC_PROTOCOL_INQ_NETWORK_EPV (protid);


    /*
     * Pass through to the network protocol routine.
     */
    (*net_epv->network_stop_mon)
        (binding_rep, client_h, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_network_maintain_liveness
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine tells the Common Communication Service to actively keep
**  communications alive with the process identified in the binding.
**
**  INPUTS:
**
**      binding_h       The binding on which to maintain liveness.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_invalid_binding
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_network_maintain_liveness (binding_h, status)

rpc_binding_handle_t    binding_h;
unsigned32              *status;

{
    rpc_protocol_id_t       protid;
    rpc_prot_network_epv_p_t net_epv;
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * Get the protocol id from the binding handle (binding_rep) 
     */
    protid = binding_rep->protocol_id;
    net_epv = RPC_PROTOCOL_INQ_NETWORK_EPV (protid);


    /*
     * Pass through to the network protocol routine.
     */
    (*net_epv->network_maint) (binding_rep, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_network_stop_maintaining
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine tells the Common Communication Service to cancel
**  rpc_network_maintain_liveness.
**
**  INPUTS:
**
**      binding_h       The binding on which to stop maintaining liveness.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_invalid_binding
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_network_stop_maintaining 
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
{
    rpc_protocol_id_t       protid;
    rpc_prot_network_epv_p_t net_epv;
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * Get the protocol id from the binding handle (binding_rep) 
     */
    protid = binding_rep->protocol_id;
    net_epv = RPC_PROTOCOL_INQ_NETWORK_EPV (protid);


    /*
     * Pass through to the network protocol routine.
     */
    (*net_epv->network_stop_maint)
        (binding_rep, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__network_init
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Initialization for this module.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_no_memory
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__network_init 
(
    unsigned32              *status
)
{
    int                     pseq_id;    /* protocol sequence id/index   */


    CODING_ERROR (status);

    /*
     * Initialize our mutex.  Initialize our conditional variable used
     * for shutdown indication.  Note that the mutex covers ALL the state
     * in this module, not just the values in "listener_state".
     */

    RPC_MUTEX_INIT (listener_state.mutex);
    RPC_COND_INIT (listener_state.cond, listener_state.mutex);

    /*
     * Allocate a local protseq vector structure.
     */
    RPC_MEM_ALLOC(psv, rpc_protseq_vector_p_t, PSV_SIZE,
        RPC_C_MEM_PROTSEQ_VECTOR, RPC_C_MEM_WAITOK);

    psv->count = 0;                     /* zero out the count */
    psv_size = 0;                       /* zero out the vector malloc size */
    psv_str_size = 0;                   /* zero out the string malloc size */

    /*
     * Loop through the protocol sequence id table and ...
     *
     *   test each protocol sequence to see if it is supported and ...
     *     if so:
     *      - fetch the pointer to the protseq
     *      - bump the amount of string memory required
     *      - bump the number of supported protseq's
     */
    for (pseq_id = 0; pseq_id < RPC_C_PROTSEQ_ID_MAX; pseq_id++)
    {
        if (RPC_PROTSEQ_INQ_SUPPORTED (pseq_id))
        {
            psv->protseq[psv->count] = RPC_PROTSEQ_INQ_PROTSEQ (pseq_id);
            psv_str_size += strlen ((char *) psv->protseq[psv->count]) + 1;
            psv->count++;
        }
    }

    /*
     * Figure the total amount of memory required for the return vector.
     */
    psv_size += sizeof (rpc_protseq_vector_t)       /* sizeof basic struct */
        + (RPC_C_PROTSEQ_MAX * (psv->count - 1));   /* sizeof protseq ptrs */

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc__network_set_priv_info
**
**  SCOPE:              PRIVATE - declared in comnet.h
**
**  DESCRIPTION:
**      
**  This routine changes the private information stored with a descriptor
**  being listened on.
**
**  INPUTS:
**
**      desc            The descriptor whose private info is to be set.
**
**      priv_info       The private info to set.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_desc_not_registered
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__network_set_priv_info 
(
    rpc_socket_t            desc,
    pointer_t               priv_info,
    unsigned32              *status
)
{
    int                     i;


    CODING_ERROR (status);

    /*
     * scan for the entry whose descriptor matches the requested
     * descriptor and set the corresponding entry's private info
     */

    for (i = 0; i < listener_state.high_water; i++)
    {
        if (listener_state.socks[i].busy && listener_state.socks[i].desc == desc)
        {
            listener_state.socks[i].priv_info = priv_info;
            *status = rpc_s_ok;
            return;
        }
    }
    *status = rpc_s_desc_not_registered;
}

/*
**++
**
**  ROUTINE NAME:       rpc__network_inq_priv_info
**
**  SCOPE:              PRIVATE - declared in comnet.h
**
**  DESCRIPTION:
**      
**  This routine returns the private information stored with the given
**  descriptor.
**
**  INPUTS:
**
**      desc            The descriptor whose private info is to be returned.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      priv_info       The private info stored with this descriptor.
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_desc_not_registered
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__network_inq_priv_info 
(
    rpc_socket_t            desc,
    pointer_t               *priv_info,
    unsigned32              *status
)
{
    int                     i;


    CODING_ERROR (status);

    /*
     * scan for the entry whose descriptor matches the requested
     * descriptor and get the corresponding entry's private info
     */

    for (i = 0; i < listener_state.high_water; i++)
    {
        if (listener_state.socks[i].busy && listener_state.socks[i].desc == desc)
        {
            *priv_info = listener_state.socks[i].priv_info;
            *status = rpc_s_ok;
            return;
        }
    }

    *status = rpc_s_desc_not_registered;
}



/*
**++
**
**  ROUTINE NAME:       rpc__network_inq_prot_version
**
**  SCOPE:              PRIVATE - declared in comnet.h
**
**  DESCRIPTION:
**      
**  Return the version number of the RPC protocol sequence requested.
**
**  INPUTS:
**
**      rpc_protseq_id  The protocol sequence id whose architected protocol id
**                      and version number is to be returned.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      prot_id         The RPC protocol sequence protocol id.
**	version_major	The RPC protocol sequence major version number.
**	version_minor	The RPC protocol sequence minor version number.
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_coding_error
**                          rpc_s_invalid_rpc_protseq
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__network_inq_prot_version 
(
    rpc_protseq_id_t        rpc_protseq_id,
    unsigned8               *prot_id,
    unsigned32		*version_major,
    unsigned32		*version_minor,
    unsigned32              *status
)
{
    rpc_protocol_id_t           rpc_prot_id;
    rpc_prot_network_epv_p_t    net_epv;

    CODING_ERROR (status);

    /*
     * Check that protocol sequence is supported by this host
     */
    if (! RPC_PROTSEQ_INQ_SUPPORTED(rpc_protseq_id))
    {
        *status = rpc_s_protseq_not_supported;
        return;
    }

    rpc_prot_id = RPC_PROTSEQ_INQ_PROT_ID (rpc_protseq_id);
    net_epv = RPC_PROTOCOL_INQ_NETWORK_EPV (rpc_prot_id);

    (*net_epv->network_inq_prot_vers)
        (prot_id, version_major, version_minor, status);
    
}

/*
**++
**
**  ROUTINE NAME:       rpc__network_protseq_id_from_protseq
**
**  SCOPE:              PRIVATE - declared in comnet.h
**
**  DESCRIPTION:
**      
**  This routine searches the RPC Protocol Sequence ID table and returns
**  the Protocol Sequence ID for the given RPC Protocol Sequence string.
**
**  INPUTS:
**
**      rpc_protseq     The RPC protocol sequence whose id is to be returned.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_invalid_rpc_protseq
**                          rpc_s_protseq_not_supported
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     rpc_protocol_id_t
**
**      The RPC protocol sequence id.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE rpc_protocol_id_t rpc__network_pseq_id_from_pseq 
(
    unsigned_char_p_t       rpc_protseq,
    unsigned32              *status
)
{
    rpc_protocol_id_t       pseqid;


    CODING_ERROR (status);

    /*
     * The protseq is not a special case string. Check the vector of
     * supported protocol sequences.
     */
    for (pseqid = 0; pseqid < RPC_C_PROTSEQ_ID_MAX; pseqid++)
    {
        if ((strcmp ((char *) rpc_protseq,
            (char *) RPC_PROTSEQ_INQ_PROTSEQ (pseqid))) == 0)
        {
            /*
             * Verify whether the protocol sequence ID is supported.
             */
            if (RPC_PROTSEQ_INQ_SUPPORTED (pseqid))
            {
                *status = rpc_s_ok;
                return (pseqid);
            }
            else
            {
                *status = rpc_s_protseq_not_supported;
                return (RPC_C_INVALID_PROTSEQ_ID);
            }
        }
    }

    /*
     * If we got this far the protocol sequence given is not valid.
     */
    *status = rpc_s_invalid_rpc_protseq;
    return (RPC_C_INVALID_PROTSEQ_ID);
}

/*
**++
**
**  ROUTINE NAME:       rpc__network_pseq_from_pseq_id
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Return the protseq (string) rep for the protseq_id.
**
**  This is an internal routine that needs to be relatively streamlined
**  as it is used by the forwarding mechanism.  As such we assume that
**  the input args (created by some other runtime component) are valid.
**  Additionally, we return a pointer to the global protseq string.  If
**  future callers of this function want to do other manipulations of
**  the string, they can copy it to private storage.
**
**  INPUTS:
**
**      protseq_id      A *valid* protocol sequence id.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      protseq         Pointer to the protseq_id's string rep.
**
**      status          The result of the operation. One of:
**                          rpc_s_ok
**                          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__network_pseq_from_pseq_id 
(
    rpc_protseq_id_t    protseq_id,
    unsigned_char_p_t   *protseq,
    unsigned32          *status
)
{
    CODING_ERROR (status);

    *protseq = RPC_PROTSEQ_INQ_PROTSEQ (protseq_id);

    *status = rpc_s_ok;
    return;
}


#ifdef ATFORK_SUPPORTED
/*
**++
**
**  ROUTINE NAME:       rpc__network_fork_handler
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Initializes this module.
**
**  INPUTS:             stage   The stage of the fork we are 
**                              currently handling.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__network_fork_handler
(
  rpc_fork_stage_id_t stage
)
{   
    switch ((int)stage)
    {
    case RPC_C_PREFORK:
        rpc__nlsn_fork_handler(&listener_state, stage);
        break;
    case RPC_C_POSTFORK_CHILD:  
        /*
         * Reset the listener_state table to 0's.
         */
        /*b_z_e_r_o((char *)&listener_state, sizeof(listener_state));*/
        memset( &listener_state, 0, sizeof listener_state );
        /*
         * Reset the global forwarding map function variable.
         */
        rpc_g_fwd_fn = NULL;
        /* fall through */
    case RPC_C_POSTFORK_PARENT:
        rpc__nlsn_fork_handler(&listener_state, stage);
        break;
    }
}
#endif
