/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 *
 * Portions Copyright (C) 2005 - 2007 Apple Inc. All rights reserved.
 *
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
**  NAME
**
**      npnaf.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This module contains routines specific to DCE RPC over SMB.
**  An initialization routine is provided to be called at RPC
**  initialization time provided, the Internet Protocol is supported
**  on the local host platform.  The remaining routines are entered
**  through an Entry Point Vector specific to the Internet Protocol.
**
**
*/

#include <commonp.h>
#include <com.h>
#include <comnaf.h>
#include <npnaf.h>

#include <dce/assert.h>


/***********************************************************************
 *
 *  Macros for sprint/scanf substitutes.
 */

#ifndef NO_SSCANF
#  define RPC__IP_ENDPOINT_SSCANF   sscanf
#else
#  define RPC__IP_ENDPOINT_SSCANF   rpc__ip_endpoint_sscanf
#endif

#ifndef NO_SPRINTF
#  define RPC__IP_ENDPOINT_SPRINTF  sprintf
#  define RPC__IP_NETWORK_SPRINTF   sprintf
#else
#  define RPC__IP_ENDPOINT_SPRINTF  rpc__ip_endpoint_sprintf
#  define RPC__IP_NETWORK_SPRINTF   rpc__ip_network_sprintf
#endif


/***********************************************************************
 *
 *  Routine Prototypes for the Internet Extension service routines.
 */

INTERNAL void addr_alloc(
        rpc_protseq_id_t             /*rpc_protseq_id*/,
        rpc_naf_id_t                 /*naf_id*/,
        unsigned_char_p_t            /*endpoint*/,
        unsigned_char_p_t            /*netaddr*/,
        unsigned_char_p_t            /*network_options*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_copy(
        rpc_addr_p_t                 /*srpc_addr*/,
        rpc_addr_p_t                * /*drpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_free(
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_set_endpoint(
        unsigned_char_p_t            /*endpoint*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_inq_endpoint(
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned_char_t             ** /*endpoint*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_set_netaddr(
        unsigned_char_p_t            /*netaddr*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_inq_netaddr(
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned_char_t             ** /*netaddr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_set_options(
        unsigned_char_p_t            /*network_options*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void addr_inq_options(
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned_char_t             ** /*network_options*/,
        unsigned32                  * /*status*/
    );

INTERNAL void desc_inq_network(
        rpc_socket_t                 /*desc*/,
        rpc_network_if_id_t         * /*socket_type*/,
        rpc_network_protocol_id_t   * /*protocol_id*/,
        unsigned32                  * /*status*/
    );

INTERNAL void inq_max_tsdu(
        rpc_naf_id_t                 /*naf_id*/,
        rpc_network_if_id_t          /*iftype*/,
        rpc_network_protocol_id_t    /*protocol*/,
        unsigned32                  * /*max_tsdu*/,
        unsigned32                  * /*status*/
    );

INTERNAL boolean addr_compare(
        rpc_addr_p_t                 /*addr1*/,
        rpc_addr_p_t                 /*addr2*/,
        unsigned32                  * /*status*/
    );
      
INTERNAL void inq_max_pth_unfrag_tpdu(
        rpc_addr_p_t                 /*rpc_addr*/,
        rpc_network_if_id_t          /*iftype*/,
        rpc_network_protocol_id_t    /*protocol*/,
        unsigned32                  * /*max_tpdu*/,
        unsigned32                  * /*status*/
    );

INTERNAL void inq_max_loc_unfrag_tpdu(
        rpc_naf_id_t                 /*naf_id*/,
        rpc_network_if_id_t          /*iftype*/,
        rpc_network_protocol_id_t    /*protocol*/,
        unsigned32                  * /*max_tpdu*/,
        unsigned32                  * /*status*/
    );

INTERNAL void set_pkt_nodelay(
        rpc_socket_t                 /*desc*/,
        unsigned32                  * /*status*/
    );
              
INTERNAL boolean is_connect_closed(
        rpc_socket_t                 /*desc*/,
        unsigned32                  * /*status*/
    );

INTERNAL void desc_inq_peer_addr(
        rpc_protseq_id_t             /*protseq_id*/,
        rpc_socket_t                 /*desc*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    );

INTERNAL void set_port_restriction(
        rpc_protseq_id_t             /*protseq_id*/,
        unsigned32                   /*n_elements*/,
        unsigned_char_p_t           * /*first_port_name_list*/,
        unsigned_char_p_t           * /*last_port_name_list*/,
        unsigned32                  * /*status*/
    );

INTERNAL void get_next_restricted_port(
        rpc_protseq_id_t             /*protseq_id*/,
        unsigned_char_p_t           * /*port_name*/,
        unsigned32                  * /*status*/
    );

INTERNAL void inq_max_frag_size(
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned32                  * /*max_frag_size*/,
        unsigned32                  * /*status*/
    );



/*
**++
**
**  ROUTINE NAME:       rpc__ip_init
**
**  SCOPE:              PRIVATE - EPV declared in ipnaf.h
**
**  DESCRIPTION:
**      
**  Internet Address Family Initialization routine, rpc__ip_init, is
**  calld only once, by the Communications Service initialization
**  procedure,  at the time RPC is initialized.  If the Communications
**  Service initialization determines that the Internet protocol
**  family is supported on the local host platform it will call this
**  initialization routine.  It is responsible for all Internet
**  specific initialization for the current RPC.  It will place in
**  Network Address Family Table, a pointer to the Internet family Entry
**  Point Vector.  Afterward all calls to the IP extension service
**  routines will be vectored through this EPV.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      naf_epv         The address of a pointer in the Network Address Family
**                      Table whre the pointer to the Entry Point Vectorto
**                      the IP service routines is inserted by this routine.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**          Any of the RPC Protocol Service status codes.
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

PRIVATE void  rpc__np_init 
(
    rpc_naf_epv_p_t         *naf_epv,
    unsigned32              *status
)
{
    /*  
     * The Internal Entry Point Vectors for the named pipe Extension Service
     * routines.  At RPC startup time, the named pipe init routine,
     * rpc__np_init, is responsible for inserting a pointer to this EPV
     * into the Network Address Family Table.  Afterward, all calls to the
     * named pipe Extension Service are vectored through these EPVs.
     */

    static rpc_naf_epv_t rpc_np_epv =
    {
        addr_alloc,
        addr_copy,
        addr_free,
        addr_set_endpoint,
        addr_inq_endpoint,
        addr_set_netaddr,
        addr_inq_netaddr,
        addr_set_options,
        addr_inq_options,
        NULL, /* XXX */
        desc_inq_network,
        inq_max_tsdu,
        NULL, /* XXX */
        addr_compare,
        inq_max_pth_unfrag_tpdu,
        inq_max_loc_unfrag_tpdu,
        set_pkt_nodelay,
        is_connect_closed,
        desc_inq_peer_addr,
        set_port_restriction,
        get_next_restricted_port,
        inq_max_frag_size
    };      

    /*
     * place the address of EPV into Network Address Family Table
     */
    *naf_epv = &rpc_np_epv;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_alloc
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Create a copy of an RPC address. Allocate memory for a variable
**  length RPC address, for the named pipe service.  Insert the endpoint
**  along with the overall length of the allocated memory, together with
**  any additional parameters required by the named pipe service.
**
**  INPUTS:
**
**      rpc_protseq_id  Protocol Sequence ID representing an named pipe
**                      Network Address Family, its Transport Protocol,
**                      and type.
**
**      naf_id          Network Address Family ID serves as index into
**                      EPV for named pipe routines.
**
**      endpoint        String containing endpoint to insert into newly
**                      allocated RPC address.
**
**      netaddr         String containing network address to be inserted
**                      in RPC addr.  - Not used by name pipe service.
**
**      network_options String containing options to be placed in
**                      RPC address.  - Not used by named pipe service.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address -
**                      returned with the address of the memory
**                      allocated by this routine. 
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
**
**          rpc_s_no_memory     Call to malloc failed to allocate memory
**
**          Any of the RPC Protocol Service status codes.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**
**--
**/

INTERNAL void addr_alloc 
(
    rpc_protseq_id_t        rpc_protseq_id,
    rpc_naf_id_t            naf_id __attribute((unused)),
    unsigned_char_p_t       endpoint,
    unsigned_char_p_t       netaddr,
    unsigned_char_p_t       network_options __attribute((unused)),
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
{
    CODING_ERROR (status);
    
    /*
     * allocate memory for the new RPC address
     */

    RPC_MEM_ALLOC (
        *rpc_addr,
        rpc_addr_p_t,
        sizeof (**rpc_addr),
        RPC_C_MEM_RPC_ADDR,
        RPC_C_MEM_WAITOK);

    if (*rpc_addr == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * zero allocated memory
     */
    /* b_z_e_r_o ((unsigned8 *) *rpc_addr, sizeof (rpc_np_addr_t));*/

    memset( *rpc_addr, 0, sizeof (**rpc_addr));

    /*
     * insert id, length, family into rpc address
     */
    (*rpc_addr)->rpc_protseq_id = rpc_protseq_id;
    (*rpc_addr)->len = sizeof (char *);
    
    /*
     * set the endpoint in the RPC addr
     */
    addr_set_endpoint (endpoint, rpc_addr, status);
    if (*status != rpc_s_ok) return;

    /*
     * set the network address in the RPC addr
     */
    addr_set_netaddr (netaddr, rpc_addr, status);
    if (*status != rpc_s_ok) return;
    
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_copy
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Obtain the length from the source RPC address.  Allocate memory for a
**  new, destination  RPC address. Do a byte copy from the surce address
**  to the destination address.
**
**  INPUTS:
**
**     src_rpc_addr     The address of a pointer to an RPC address to be
**                      copied.  It must be the correct format for Internet
**                      Protocol. 
**
**  INPUTS/OUTPUTS:
**
**     dst_rpc_addr     The address of a pointer to an RPC address -returned
**                      with the address of the memory allocated by
**                      this routine. 
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**      rpc_s_ok            The call was successful.
**
**      rpc_s_no_memory     Call to malloc failed to allocate memory
**
**      rpc_s_invalid_naf_id  Source RPC address appeared invalid
**
**
**  IMPLICIT INPUTS:
**
**        A check is performed on the source RPC address before malloc.  It
**        must be the IP family.
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:
**
**           In the event, the addres of a of memory segment contained in
**           rpc_addr, is not valid or the length isn't as long as is 
**           indicated, a memory fault may result.
**
**--
**/

INTERNAL void addr_copy 
(
    rpc_addr_p_t            src_rpc_addr,
    rpc_addr_p_t            *dst_rpc_addr,
    unsigned32              *status
)
{
    size_t      path_len;
    char        *path;

    CODING_ERROR (status);
    
    /*
     * allocate memory for the new RPC address
     */
    RPC_MEM_ALLOC (
        *dst_rpc_addr,
        rpc_addr_p_t,
        sizeof(**dst_rpc_addr),
        RPC_C_MEM_RPC_ADDR,
        RPC_C_MEM_WAITOK);

    if (*dst_rpc_addr == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * Allocate memory for the pipe path in the new address.
     */
    path_len = strlen(src_rpc_addr->pipe_path) + 1;
    RPC_MEM_ALLOC (
        path,
        char *,
        path_len,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);

    if (path == NULL)
    {
        RPC_MEM_FREE (*dst_rpc_addr, RPC_C_MEM_RPC_ADDR);
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * Copy source rpc address to destination rpc address
     */
    /* b_c_o_p_y ((unsigned8 *) src_rpc_addr, (unsigned8 *) *dst_rpc_addr,
            sizeof (rpc_ip_addr_t));*/

    memmove( *dst_rpc_addr, src_rpc_addr, sizeof (**dst_rpc_addr));
    (*dst_rpc_addr)->pipe_path = path;
    memcpy(path, src_rpc_addr->pipe_path, path_len);

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_free
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Free the memory for the RPC address pointed to by the argument
**  address pointer rpc_addr.  Null the address pointer.  The memory
**  must have been previously allocated by RPC_MEM_ALLC.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**     rpc_addr         The address of a pointer to an RPC address -returned
**                      with a NULL value.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       
**           In the event, the segment of memory refered to by pointer
**           rpc_addr, is allocated by means other than RPC_MEM_ALLOC,
**           unpredictable results will occur when this routine is called.
**--
**/

INTERNAL void addr_free 
(
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
{
    CODING_ERROR (status);
    /* If we still have a pipe path free it */
	if ((*rpc_addr)->pipe_path)
		RPC_MEM_FREE ((*rpc_addr)->pipe_path, RPC_C_MEM_SOCKADDR);
	(*rpc_addr)->pipe_path = NULL;
   /*
     * free memory of RPC addr
     */
    RPC_MEM_FREE (*rpc_addr, RPC_C_MEM_RPC_ADDR);

    /*
     * indicate that the rpc_addr is now empty
     */
    *rpc_addr = NULL;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_set_endpoint
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    Receive the null terminated ascii character string,  rpc_endpoint
**    and insert into the RPC address pointed to by argument rpc_addr.
**
**  INPUTS:
**
**      endpoint        String containing endpoint to insert into RPC address.
**                      For NP contains a named pipe pathname, or NULL.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address where
**                      the endpoint is to be inserted.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok                 The call was successful.
**
**          rpc_s_invalid_naf_id  Argument, endpoint contains an 
**                                  unauthorized pointer value.
**          rpc_s_no_memory       No memory for copy of pipe path.
**
**
**  IMPLICIT INPUTS:
**
**      A NULL, (first byte NULL), endpoint string is an indicator to 
**      the routine to delete the endpoint from the RPC address. indicated.
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void addr_set_endpoint 
(
    unsigned_char_p_t       endpoint,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
{
    size_t              ep_len;
    char                *ep_copy;

    
    CODING_ERROR (status);
    
    /*
     * check to see if this is a request to remove the endpoint
     */
    if (endpoint == NULL || strlen ((char *) endpoint) == 0)
    {
        RPC_MEM_FREE ((*rpc_addr)->pipe_path, RPC_C_MEM_SOCKADDR);
        (*rpc_addr)->pipe_path = NULL;
        *status = rpc_s_ok;
        return;
    }

    /*
     * copy the endpoint string and insert in RPC address
     */

    ep_len = strlen((char *) endpoint) + 1;
    RPC_MEM_ALLOC (
        ep_copy,
        char *,
        ep_len,
        RPC_C_MEM_SOCKADDR,
        RPC_C_MEM_WAITOK);
    if (ep_copy == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }
    memcpy (ep_copy, endpoint, ep_len);

    RPC_MEM_FREE ((*rpc_addr)->pipe_path, RPC_C_MEM_SOCKADDR);
    (*rpc_addr)->pipe_path = ep_copy;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_inq_endpoint
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    From the RPC address indicated by arg., rpc_addr, examine the
**    endpoint.  Convert the endopint value to a NULL terminated asci
**    character string to be returned in the memory segment pointed to
**    by arg., endpoint.
**
**  INPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address that
**                      to be inspected.
**
**  INPUTS/OUTPUTS:     none
**
**
**  OUTPUTS:
**
**      endpoint        String pointer indicating where the endpoint
**                      string is to be placed.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
**
**          Any of the RPC Protocol Service status codes.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:
**
**      A zero length string will be returned if the RPC address contains
**      no endpoint.
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:
**--
**/

INTERNAL void addr_inq_endpoint 
(
    rpc_addr_p_t            rpc_addr,
    unsigned_char_t         **endpoint,
    unsigned32              *status
)
{
    size_t              ep_len;


    CODING_ERROR (status);

    /*
     * if no endpoint present, return null string. Otherwise,
     * return the endpoint string.
     */    
    if (rpc_addr->pipe_path == NULL)
    {
        RPC_MEM_ALLOC(
            *endpoint,
            unsigned_char_p_t,
            sizeof(unsigned32),     /* can't stand to get just 1 byte */
            RPC_C_MEM_STRING,
            RPC_C_MEM_WAITOK);
        *endpoint[0] = 0;
    }
    else
    {
        ep_len = strlen(rpc_addr->pipe_path) + 1;
        RPC_MEM_ALLOC(
            *endpoint,
            unsigned_char_p_t,
            ep_len < sizeof(unsigned32) ? sizeof(unsigned32) : ep_len,
            RPC_C_MEM_STRING,
            RPC_C_MEM_WAITOK);
        memcpy ((char *) *endpoint, rpc_addr->pipe_path, ep_len);
    }

    *status = rpc_s_ok;    
}

/*
**++
**
**  ROUTINE NAME:       addr_set_netaddr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    Receive the null terminated ascii character string, netaddr,
**    and convert to the Internet Protocol Network Address format.  Insert
**    into the RPC address, indicated by argument rpc_addr.
**
**  INPUTS:
**
**      netaddr         Hex string for 
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address where
**                      the network address is to be inserted.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**      rpc_s_ok                  The call was successful.
**      rpc_s_inval_net_addr      Invalid IP network address string passed
**                                in netaddr
**
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

INTERNAL void addr_set_netaddr 
(
    unsigned_char_p_t       netaddr,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
{
    void *p;

    CODING_ERROR (status);  

    /*
     * check to see if this is a request to remove the netaddr
     */
    if (netaddr == NULL || strlen ((char *) netaddr) == 0)
    {
        (*rpc_addr)->sctx = NULL;
        *status = rpc_s_ok;
        return;
    }

    /*
    ** The string is assumed to be a hex string giving a pointer
    ** value that points to an smb_ctx.
    */
    if (sscanf((char *)netaddr, "%p", &p) != 1)
    {
        *status = rpc_s_inval_net_addr;
        return;
    }

    (*rpc_addr)->sctx = p;
    *status = rpc_s_ok;
   
}

/*
**++
**
**  ROUTINE NAME:       addr_inq_netaddr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    From the RPC address indicated by arg., rpc_addr, examine the
**    network address.  Convert the network address from its network
**    format  to a NULL terminated ascii character string in IP dot
**    notation format.  The character string to be returned in the
**    memory segment pointed to by arg., netaddr.
**
**  INPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address that
**                      is to be inspected.
**
**  INPUTS/OUTPUTS:
**
**
**  OUTPUTS:
**
**      netaddr         String pointer indicating where the network
**                      address string is to be placed.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
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

INTERNAL void addr_inq_netaddr 
(
    rpc_addr_p_t            rpc_addr __attribute((unused)),
    unsigned_char_t         **netaddr,
    unsigned32              *status
)
{
    
    CODING_ERROR (status);
    
    RPC_MEM_ALLOC(
        *netaddr,
        unsigned_char_p_t,
        sizeof(unsigned32),     /* only really need 1 byte */
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);

    /*
    ** We supply no host information.
    */
    *netaddr[0] = 0;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_set_options
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**     Receive a NULL terminated network options string and insert
**     into the RPC address indicated by art., rpc_addr.
**
**      NOTE - there are no options used with the named pipe service this
**             routine is here only to serve as a stub.
**
**  INPUTS:
**
**      options         String containing network options to insert
**                      into  RPC address.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address where
**                      the network options strig is to be inserted.
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
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


INTERNAL void addr_set_options 
(
    unsigned_char_p_t       network_options __attribute((unused)),
    rpc_addr_p_t            *rpc_addr __attribute((unused)),
    unsigned32              *status
)
{
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_inq_options
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**      Extract the network options from the RPC address pointed to
**      by rpc_addr and convert to a NULL terminated string placed
**      in a buffer indicated by the options arg.
**
**      NOTE - there are no options used with the named pipe service this
**             routine is here only to serve as a stub.
**
**  INPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address that
**                      is to be inspected.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      options         String pointer indicating where the network
**                      options string is to be placed.
**
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
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

INTERNAL void addr_inq_options 
(
    rpc_addr_p_t            rpc_addr __attribute((unused)),
    unsigned_char_t         **network_options,
    unsigned32              *status
)
{
    RPC_MEM_ALLOC(
        *network_options,
        unsigned_char_p_t,
        sizeof(unsigned32),     /* only really need 1 byte */
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);

    *network_options[0] = 0;
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       inq_max_tsdu
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tsdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_tsdu 
(
    rpc_naf_id_t            naf_id __attribute((unused)),
    rpc_network_if_id_t     iftype __attribute((unused)),
    rpc_network_protocol_id_t protocol __attribute((unused)),
    unsigned32              *max_tsdu __attribute((unused)),
    unsigned32              *status __attribute((unused))
)
{
    /*
     * This should be called from ncadg_ip_udp only.
     * Since we're not doing datagrams or UDP, we just fail.
     */
    assert(false);      /* !!! */
}

/*
**++
**
**  ROUTINE NAME:       addr_compare
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Determine if two address are equal.
**
**  INPUTS:
**
**      addr1
**
**      addr2
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:    
**
**      return          Boolean; true if address are the same.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean addr_compare 
(
    rpc_addr_p_t            addr1, 
    rpc_addr_p_t            addr2,
    unsigned32              *status __attribute((unused))
)
{
    if (addr1->sctx == addr2->sctx &&
        strcmp(addr1->pipe_path, addr2->pipe_path) == 0)
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
**  ROUTINE NAME:       inq_max_pth_unfrag_tpdu
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tpdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_pth_unfrag_tpdu 
(
    rpc_addr_p_t            rpc_addr __attribute((unused)),
    rpc_network_if_id_t     iftype __attribute((unused)),
    rpc_network_protocol_id_t protocol __attribute((unused)),
    unsigned32              *max_tpdu __attribute((unused)),
    unsigned32              *status __attribute((unused))
)
{
    /*
     * This should be called from ncadg_ip_udp only.
     * Since we're not doing datagrams or UDP, we just fail.
     */
    assert(false);      /* !!! */
}

/*
**++
**
**  ROUTINE NAME:       inq_max_loc_unfrag_tpdu
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tpdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_loc_unfrag_tpdu 
(
    rpc_naf_id_t            naf_id __attribute((unused)),
    rpc_network_if_id_t     iftype __attribute((unused)),
    rpc_network_protocol_id_t protocol __attribute((unused)),
    unsigned32              *max_tpdu __attribute((unused)),
    unsigned32              *status __attribute((unused))
)
{
    /*
     * This should be called from ncadg_ip_udp only.
     * Since we're not doing datagrams or UDP, we just fail.
     */
    assert(false);      /* !!! */
}

/*
**++
**
**  ROUTINE NAME:       desc_inq_network
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  This routine is responsible for "reverse-engineering" the parameters to
**  the original socket call that was made to create the socket "desc".
**
**  INPUTS:
**
**      desc            socket descriptor to query
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      socket_type     network interface type id
**
**      protocol_id     network protocol family id
**
**      status          status returned
**                              rpc_s_ok
**                              rpc_s_cant_get_if_id
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

INTERNAL void desc_inq_network 
(
    rpc_socket_t              desc __attribute((unused)),
    rpc_network_if_id_t       *socket_type,
    rpc_network_protocol_id_t *protocol_id,
    unsigned32                *status
)
{
    
    CODING_ERROR (status);

    /*
     * Get the socket type.
     */
     
    *socket_type = 666;  /* XXX - SOCK_STREAM, etc. */

    *protocol_id = RPC_C_NETWORK_PROTOCOL_ID_NP;

    *status = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       set_pkt_nodelay
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      desc            The network descriptor to apply the nodelay
**                      option to.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      status          A value indicating the status of the routine.
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

INTERNAL void set_pkt_nodelay 
(
    rpc_socket_t            desc __attribute((unused)),
    unsigned32              *status
)
{
    /*
     * XXX - this turns the Nagle algorithm off for TCP; what should it
     * do for us?  Succeed, or fail with "rpc_s_cannot_set_nodelay"?
     */
    *status = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       is_connect_closed
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine is called when a recv on a sequenced packet
**      socket has returned zero bytes. Since named pipes are not
**      sockets, and implement a byte stream, the routine is a no-op.
**      "true" is returned because zero bytes received on a byte stream
**      does mean the connection is closed.
**      
**  INPUTS:
**
**      desc            The network descriptor representing the connection.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     
**
**      boolean         true if the connection is closed, false otherwise.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean is_connect_closed 
(
    rpc_socket_t            desc __attribute((unused)),
    unsigned32              *status
)
{
    *status = rpc_s_ok;
    return (true);
}


/*
**++
**
**  ROUTINE NAME:       desc_inq_peer_addr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    Receive a socket descriptor which is queried to obtain family,
**    remote endpoint and remote network address.  If this information appears valid
**    for an DECnet IV address,  space is allocated for an RPC address which
**    is initialized with the information obtained from this socket.  The
**    address indicating the created RPC address is returned in, arg., rpc_addr.
**
**  INPUTS:
**
**      protseq_id             Protocol Sequence ID representing a
**                             particular Network Address Family,
**                             its Transport Protocol, and type.
**
**      desc                   Descriptor, indicating a socket that
**                             has been created on the local operating
**                             platform.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer where the RPC address
**                      created by this routine will be indicated.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok               The call was successful.
**
**          rpc_s_no_memory         Call to malloc failed to allocate memory.
**
**          rpc_s_cant_get_peername Call to getpeername failed.
**
**          rpc_s_invalid_naf_id   Socket that arg desc refers is not DECnet IV.
**
**          Any of the RPC Protocol Service status codes.
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

INTERNAL void desc_inq_peer_addr 
(
    rpc_protseq_id_t        protseq_id,
    rpc_socket_t            desc __attribute((unused)),
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
{
    CODING_ERROR (status);


    /*
     * allocate memory for the new RPC address
     */
    RPC_MEM_ALLOC (*rpc_addr,
                   rpc_addr_p_t,
                   sizeof (**rpc_addr),
                   RPC_C_MEM_RPC_ADDR,
                   RPC_C_MEM_WAITOK);
    
    /*
     * successful malloc
     */
    if (*rpc_addr == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * insert individual parameters into RPC address
     */
    (*rpc_addr)->rpc_protseq_id = protseq_id;
    (*rpc_addr)->len = 0;

    /*
     * Get the peer address (name).
     *
     * We don't currently support this.
     */

    if (true)
    {
        RPC_MEM_FREE (*rpc_addr, RPC_C_MEM_RPC_ADDR);
        *rpc_addr = (rpc_addr_p_t)NULL;
        *status = rpc_s_cant_getpeername;
    }
    else
    {
        *status = rpc_s_ok;
    }
}


/*
**++
**
**  ROUTINE NAME:       set_port_restriction
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Builds an rpc_port_restriction_list_t and glues it into the
**  rpc_protseq_id_elt_t in the rpc_g_protseq_id[] list.  
**  
**  INPUTS:
**
**      protseq_id
**                      The protocol sequence id to set port restriction
**                      on. 
**      n_elements
**                      The number of port ranges passed in.
**
**      first_port_name_list
**                      An array of pointers to strings containing the
**                      lower bound port names. 
**
**      last_port_name_list
**                      An array of pointers to strings containing the
**                      upper bound port names.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status
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


INTERNAL void set_port_restriction
(
     rpc_protseq_id_t            protseq_id,
     unsigned32                  n_elements,
     unsigned_char_p_t           *first_port_name_list,
     unsigned_char_p_t           *last_port_name_list,
     unsigned32                  *status
)
{

    rpc_port_restriction_list_p_t list_p;
    rpc_port_range_element_p_t   range_elements;
    unsigned32                   i;


    CODING_ERROR (status);

    /* 
     * It is only meaningful to do this once per protocol sequence.
     */

    if (rpc_g_protseq_id [protseq_id].port_restriction_list != NULL)
    {
        *status = rpc_s_already_registered;
        return;
    }

    /* 
     * Allocate the port_restriction_list.
     */

    RPC_MEM_ALLOC 
        (list_p,
         rpc_port_restriction_list_p_t,
         sizeof (rpc_port_restriction_list_t),
         RPC_C_MEM_PORT_RESTRICT_LIST,
         RPC_C_MEM_WAITOK);

    if (list_p == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }
                   
    /* 
     * Allocate the port_range_element vector.
     */

    RPC_MEM_ALLOC (range_elements,        
                   rpc_port_range_element_p_t,
                   sizeof (rpc_port_range_element_t) * n_elements,
                   RPC_C_MEM_PORT_RANGE_ELEMENTS,
                   RPC_C_MEM_WAITOK);

    if (range_elements == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /* 
     * Initialize the rpc_port_restriction_list 
     */

    list_p -> n_tries = 0;
    list_p -> n_elements = n_elements;
    list_p -> range_elements = range_elements;

    /* 
     * Loop and initialize the range element list.
     */

    for (i = 0; i < n_elements; i++)
    {
        
        if ((RPC__IP_ENDPOINT_SSCANF
             ((char *) first_port_name_list[i], "%u", &range_elements[i].low)
             != 1)       ||
            (RPC__IP_ENDPOINT_SSCANF
             ((char *) last_port_name_list[i], "%u", &range_elements[i].high) 
             != 1)       ||
            (range_elements[i].low > range_elements[i].high))
        {
            RPC_MEM_FREE (list_p, RPC_C_MEM_PORT_RESTRICT_LIST);
            RPC_MEM_FREE (range_elements, RPC_C_MEM_PORT_RANGE_ELEMENTS);

            *status = rpc_s_invalid_endpoint_format;

            return;
        }                               /* error from scanf */

        list_p -> n_tries += 
            range_elements[i].high - range_elements[i].low + 1;
    }                                   /* for i */

    /* 
     * Randomly choose a starting range and a port within the range.
     */

    list_p -> current_range_element = RPC_RANDOM_GET (0, n_elements - 1);
    i = list_p -> current_range_element;

    list_p -> current_port_in_range = 
        RPC_RANDOM_GET (range_elements[i].low, range_elements[i].high);

    /* 
     * Everything was successful.  Wire the port_restriction_list into the 
     * protseq descriptor table.
     */

    rpc_g_protseq_id [protseq_id].port_restriction_list = list_p;
    *status = rpc_s_ok;

}                                       /* set_port_restriction */


/*
**++
**
**  ROUTINE NAME:       get_next_restricted_port
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Fail - SMB named pipes have no notion of a port number.
**  
**  INPUTS:
**
**      protseq_id
**                      The protocol sequence id to get the port on. 
**      
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      port_name
**                      An IP port name as a text string.
**      status
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

INTERNAL void get_next_restricted_port
(
    rpc_protseq_id_t           protseq_id __attribute((unused)),
    unsigned_char_p_t          *port_name __attribute((unused)),
    unsigned32                 *status __attribute((unused))
)
{
    assert(false);      /* !!! */
}                                       /* get_next_restricted_port */

/*
**++
**
**  ROUTINE NAME:       inq_max_frag_size
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tpdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_frag_size
(
 rpc_addr_p_t rpc_addr __attribute((unused)),
 unsigned32   *max_frag_size __attribute((unused)),
 unsigned32   *status __attribute((unused))
)
{
    /*
     * This should be called from ncadg_ip_udp only.
     * Since we're not doing datagrams or UDP, we just fail.
     */
    assert(false);      /* !!! */
}
