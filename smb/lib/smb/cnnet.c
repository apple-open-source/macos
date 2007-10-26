/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 *
 * Portions Copyright (C) 2007 Apple Inc. All rights reserved.
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
**      cnnet.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  The NCA Connection Protocol Service's Network Service.
**
**
 */

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Common communications services */
#include <comprot.h>    /* Common protocol services */
#include <cnp.h>        /* NCA Connection private declarations */
#include <cnpkt.h>      /* NCA Connection packet encoding */
#include <cnid.h>       /* NCA Connection local ID service */
#include <cnassoc.h>    /* NCA Connection association service */
#include <cnassm.h>     /* NCA Connection association state machine */
#include <cnfbuf.h>     /* NCA Connection fragment buffer service */
#include <cncall.h>     /* NCA Connection call service */
#include <cnnet.h>

/***********************************************************************/
/*
 * Global variables
 */
static unsigned32	rpc_g_cn_socket_read_buffer=0;
static unsigned32	rpc_g_cn_socket_write_buffer=0;


/***********************************************************************/
/*
 * Local routines
 */
INTERNAL void rpc__cn_network_serr_to_status(
    rpc_socket_error_t       /*serr*/,
    unsigned32              *st);


/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_inq_prot_vers
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**      
**  Return the version number of the NCA CN protocol currently in use.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      prot_id         The NCA protocol identifier.
**	version_major	The NCA CN major version number.
**	version_minor	The NCA CN minor version number.
**      status          The result of the operation. One of:
**                          rpc_s_ok
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

PRIVATE void rpc__cn_network_inq_prot_vers 
(
  unsigned8               *prot_id,
  unsigned32		*version_major,
  unsigned32		*version_minor,
  unsigned32              *status
)
{
    CODING_ERROR (status);

    *prot_id = RPC_C_CN_PROTO_ID;
    *version_major = RPC_C_CN_PROTO_VERS;
    *version_minor = RPC_C_CN_PROTO_VERS_MINOR;
    *status = rpc_s_ok;
}


/***********************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_req_connect
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine will create a connection to the address given as an
**  argument.
**
**  INPUTS:
**
**      assoc           The association control block allocated for
**                      this connection.
**      rpc_addr        The RPC address to which the connection is
**                      to be made.
**      call_r          The call rep data structure to be linked to
**                      the association control block. This may be
**                      NULL if this association is being allocated
**                      on the server side. 
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**                      rpc_s_ok
**                      rpc_s_cant_create_sock
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

PRIVATE void rpc__cn_network_req_connect 
(
  rpc_addr_p_t            rpc_addr,
  rpc_cn_assoc_p_t        assoc,
  unsigned32              *st
)
{
    volatile rpc_socket_error_t  serr;
    volatile boolean32           retry_op;
    volatile boolean32           connect_completed;

    RPC_CN_DBG_RTN_PRINTF (rpc__cn_network_req_connect);
    CODING_ERROR(st);
    
    /*
     * Now we need to create a connection to the server address
     * contained in the RPC address given. First create a socket to
     * do the connect on.
     */
    serr = rpc__socket_new (&assoc->cn_ctlblk.cn_sock);
    if (RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_ERRORS,
                        ("(rpc__cn_network_req_connect) call_rep->%x assoc->%x desc->%p rpc__socket_new failed\n",
                         assoc->call_rep,
                         assoc,
                         assoc->cn_ctlblk.cn_sock));
        
        *st = rpc_s_cant_create_sock;
    }
    else
    {
        /*
         * Indicate that the connection is being attempted.
         */
        assoc->cn_ctlblk.cn_state = RPC_C_CN_CONNECTING;
        
        /*
         * Since the connect call will block we will release the CN
         * global mutex before the call and reqaquire it after the call.
         */
        RPC_CN_UNLOCK ();

        /*
         * Now actually do the connect to the server.
         */
        retry_op = true;

        /*
         * Poll for cancels while the connect is in progress.
         * Only exit the while loop on success, failure, or
         * when you've received a cancel and the cancel timer
         * has expired.
         * If it is just a cancel, set cancel_pending and 
         * start the cancel timer.
         */
        connect_completed = false;
        while (! connect_completed)
        {
            TRY
            {

#ifdef NON_CANCELLABLE_IO
	    /*
             * By posix definition pthread_setasynccancel is not a "cancel
             * point" because it must return an error status and an errno.
             * pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &xxx)
             * will not deliver a pending cancel nor will the cancel be
             * delivered asynchronously, thus the need for pthread_testcancel.
             * 
	     */
                pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &dummy);
	        pthread_testcancel();
#endif
                serr = rpc__socket_connect (assoc->cn_ctlblk.cn_sock, rpc_addr);

#ifdef NON_CANCELLABLE_IO
                pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &dummy);
#endif   
                /*
                 * If we got here, then the connect was not cancelled;
                 * it has therefore completed either successfully or
                 * with serr set.
                 */
                connect_completed = true;
            }
            CATCH (pthread_cancel_e)
            {
#ifdef NON_CANCELLABLE_IO
                pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &dummy);
#endif   

                RPC_CN_LOCK ();
                /*
                 * Record the fact that a cancel has been
                 * detected. Also, start a timer if this is
                 * the first cancel detected.
                 */
                rpc__cn_call_local_cancel (assoc->call_rep,
                                           &retry_op,
                                           st);
                RPC_DBG_PRINTF (rpc_e_dbg_cancel, RPC_C_CN_DBG_CANCEL,
                                ("(rpc__cn_network_req_connect) call_rep->%x assoc->%x desc->%p cancel caught before association setup\n", 
                                 assoc->call_rep,
                                 assoc,
                                 assoc->cn_ctlblk.cn_sock));
                RPC_CN_UNLOCK ();
            }
            ENDTRY
            if (!retry_op)
            {
                RPC_CN_LOCK ();
                rpc__socket_close (assoc->cn_ctlblk.cn_sock);
                return;
            }

        }

        /* 
         * The connect completed; see if it completed successfully.
         */
        RPC_CN_LOCK ();

        if (RPC_SOCKET_IS_ERR(serr))
        {
            RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_ERRORS,
                            ("(rpc__cn_network_req_connect) call_rep->%p assoc->%p sock->%p can't open pipe, error = %d\n",
                             assoc->call_rep,
                             assoc,
                             assoc->cn_ctlblk.cn_sock,
                             RPC_SOCKET_ETOI(serr)));
        
            rpc__cn_network_serr_to_status (serr, st);
            
            /*
             * The connect request failed. Close the socket just created
             * and free the association control block.
             */
            serr = rpc__socket_close (assoc->cn_ctlblk.cn_sock);
            if (RPC_SOCKET_IS_ERR(serr))
            {
                /*
                 * The socket close failed.
                 */
                RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_ERRORS,
                                ("(rpc__cn_network_req_connect) sock->%p rpc__socket_close failed, error = %d\n", 
                                 assoc->cn_ctlblk.cn_sock, 
                                 RPC_SOCKET_ETOI(serr)));
            }
            return;
        }
        else
        {
            /*
             * Indicate that there is a valid connection.
             */
            assoc->cn_ctlblk.cn_state = RPC_C_CN_OPEN;
            
            /*
             * A connection is now set up. Tell the receiver thread to begin
             * receiving on the connection.
             */
            if (assoc->cn_ctlblk.cn_rcvr_waiters)
            {
                RPC_COND_SIGNAL (assoc->cn_ctlblk.cn_rcvr_cond, 
                                 rpc_g_global_mutex);
            }
            
            /*
             * Everything went OK. Return a successful status code. 
             */
            *st = rpc_s_ok;
        }
    }
}


/***********************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_close_connect
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine will close an open connection.
**
**  INPUTS:
**
**      assoc           The association control block to which this connection
**                      is attached. 
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            
**
**      st              The return status of this routine.
**                      rpc_s_ok
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

PRIVATE void rpc__cn_network_close_connect 
(
  rpc_cn_assoc_p_t        assoc,
  unsigned32              *st
)
{

    RPC_CN_DBG_RTN_PRINTF (rpc__cn_network_close_connect);
    CODING_ERROR (st);
    
    /*
     * We can't safely close the pipe out from under the receiver
     * thread if it's trying to read from it.
     */
    if (assoc->cn_ctlblk.cn_state == RPC_C_CN_OPEN)
    {
        pthread_cancel (assoc->cn_ctlblk.cn_rcvr_thread_id);
    }
    *st = rpc_s_ok;
}


/***********************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_mon
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine notifies the connection protocol that at least one
**  connection to the address space identified in the binding rep
**  provided as input should be kept open.
**
**  INPUTS:
**
**      binding_r       The binding rep identifying the client
**                      process to be monitored.
**      client_h        The unique identifier of the client process
**                      which is really the id of an association group.
**      rundown         The routine to call if the communications
**                      link to the client process fails.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**                      rpc_s_ok
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

PRIVATE void rpc__cn_network_mon 
(
  rpc_binding_rep_p_t     binding_r __attribute((unused)),
  rpc_client_handle_t     client_h,
  rpc_network_rundown_fn_t rundown,
  unsigned32              *st
)
{
    rpc_cn_assoc_grp_t  *assoc_grp;
    rpc_cn_local_id_t   grp_id;

    RPC_CN_DBG_RTN_PRINTF (rpc__cn_network_mon);
    CODING_ERROR(st);
    
    /*
     * Get the association group using the group id provided as a
     * client handle. 
     */
    grp_id.all = (unsigned long)client_h;
    grp_id = rpc__cn_assoc_grp_lkup_by_id (grp_id,
                                           RPC_C_CN_ASSOC_GRP_SERVER,
                                           st);

    /*
     * If the association group control block can't be found
     * return an error.
     */
    if (RPC_CN_LOCAL_ID_VALID (grp_id))
    {
        /*
         * Now we have the association group. Place the rundown function
         * in it and bump the reference count.
         */
        assoc_grp = RPC_CN_ASSOC_GRP (grp_id);
        assoc_grp->grp_liveness_mntr = rundown;
        assoc_grp->grp_refcnt++;
        *st = rpc_s_ok;
    }
}


/***********************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_stop_mon
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine is called when it is no longer necessary for the
**  runtime to keep a connection open for the caller to the address
**  space identified by the provided client handle.
**
**  INPUTS:
**
**      binding_r       The binding rep identifying the client
**                      process to be monitored.
**      client_h        The unique identifier of the client process
**                      which is really an association group control block.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**                      rpc_s_ok
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

PRIVATE void rpc__cn_network_stop_mon 
(
  rpc_binding_rep_p_t     binding_r __attribute((unused)),
  rpc_client_handle_t     client_h,
  unsigned32              *st
)
{
    rpc_cn_assoc_grp_t  *assoc_grp;
    rpc_cn_local_id_t   grp_id;
    
    CODING_ERROR(st);
    RPC_CN_DBG_RTN_PRINTF (rpc__cn_network_stop_mon);
    
    /*
     * Get the association group using the group id provided as a
     * client handle. 
     */
    grp_id.all = (unsigned long)client_h;
    grp_id = rpc__cn_assoc_grp_lkup_by_id (grp_id,
                                           RPC_C_CN_ASSOC_GRP_SERVER,
                                           st);

    /*
     * If the association group control block can't be found
     * return an error.
     */
    if (RPC_CN_LOCAL_ID_VALID (grp_id))
    {
        /*
         * Now we have the association group. Decrement the reference count.
         */
        assoc_grp = RPC_CN_ASSOC_GRP (grp_id);
        assoc_grp->grp_refcnt--;
        *st = rpc_s_ok;
    }
}


/***********************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_maint
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine is called when a connection to the address space
**  represented by the binding rep should be kept alive. Since we are
**  assuming all our connections have the KEEPALIVE feature there is
**  nothing for us to do here except make sure we keep at least one
**  connection open.
**
**  INPUTS:
**
**      binding_r       The binding rep identifying the server
**                      process to which a communications link
**                      should be maintained.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**                      rpc_s_ok
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

PRIVATE void rpc__cn_network_maint 
(
  rpc_binding_rep_p_t     binding_r,
  unsigned32              *st
)
{
    rpc_cn_assoc_grp_t          *assoc_grp;
    rpc_cn_local_id_t           grp_id;

    CODING_ERROR(st);
    RPC_CN_DBG_RTN_PRINTF (rpc__cn_network_maint);
    
    /*
     * Get the association group using the group id contained in the
     * binding handle.
     */
    grp_id = rpc__cn_assoc_grp_lkup_by_id (((rpc_cn_binding_rep_t *)
                                            (binding_r))->grp_id,
                                           RPC_C_CN_ASSOC_GRP_CLIENT,
                                           st);  
    
    /*
     * If the association group control block can't be found
     * return an error.
     */
    if (RPC_CN_LOCAL_ID_VALID (grp_id))
    {
        /*
         * We now have the association group control block we've been
         * looking for. Increment the reference count.
         */
        assoc_grp = RPC_CN_ASSOC_GRP (grp_id);
        assoc_grp->grp_refcnt++;
        *st = rpc_s_ok;
    }
}


/***********************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_stop_maint
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine is called when a connection to the address space
**  represented by the binding rep need no longer be kept alive.
**
**  INPUTS:
**
**      binding_r       The binding rep identifying the server
**                      process to which a communications link
**                      was being maintained.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**                      rpc_s_ok
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

PRIVATE void rpc__cn_network_stop_maint 
(
  rpc_binding_rep_p_t     binding_r,
  unsigned32              *st
)
{
    rpc_cn_assoc_grp_t  *assoc_grp;
    rpc_cn_local_id_t   grp_id;

    CODING_ERROR(st);
    RPC_CN_DBG_RTN_PRINTF (rpc__cn_network_stop_maint);
    
    /*
     * Get the association group using the group id contained in the
     * binding handle.
     */
    grp_id = rpc__cn_assoc_grp_lkup_by_id (((rpc_cn_binding_rep_t *)
                                            (binding_r))->grp_id,
                                           RPC_C_CN_ASSOC_GRP_CLIENT,
                                           st); 
    
    /*
     * If the association group control block can't be found
     * return an error.
     */
    if (RPC_CN_LOCAL_ID_VALID (grp_id))
    {
        /*
         * We now have the association group control block we've been
         * looking for. Decrement the reference count.
         */
        assoc_grp = RPC_CN_ASSOC_GRP (grp_id);
        assoc_grp->grp_refcnt--;
        *st = rpc_s_ok;
    }
}

/***********************************************************************/


/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_connect_fail
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine determine whether a given status code indicated a connect
**  request failed.
**
**  INPUTS:
**
**      st              The status code.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean32
**
**      true            The status code is from a failed connection
**                      request false otherwise
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE boolean32 rpc__cn_network_connect_fail 
(
unsigned32              st
)
{
    switch ((int)st)
    {
        case rpc_s_cancel_timeout:
        case rpc_s_connect_timed_out:
        case rpc_s_connect_rejected:
        case rpc_s_network_unreachable:
        case rpc_s_connect_no_resources:
        case rpc_s_rem_network_shutdown:
        case rpc_s_too_many_rem_connects:
        case rpc_s_no_rem_endpoint:
        case rpc_s_rem_host_down:
        case rpc_s_host_unreachable:
        case rpc_s_access_control_info_inv:
        case rpc_s_loc_connect_aborted:
        case rpc_s_connect_closed_by_rem:
        case rpc_s_rem_host_crashed:
        case rpc_s_invalid_endpoint_format:
        case rpc_s_cannot_connect:
        {
            return (true);
        }
        
        default:
        {
            return (false);
        }
    }
}

/***********************************************************************/


/*
**++
**
**  ROUTINE NAME:       rpc__cn_network_serr_to_status
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**  This routine converts a socket interface error into an RPC
**  status code.
**
**  INPUTS:
**
**      serr            The socket error.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            
**
**      st              The status code.
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

INTERNAL void rpc__cn_network_serr_to_status 
(
  rpc_socket_error_t      serr,
  unsigned32              *st
)
{
    switch (serr)
    {
        case RPC_C_SOCKET_ETIMEDOUT:
        *st = rpc_s_connect_timed_out;
        break;
        
        case RPC_C_SOCKET_ECONNREFUSED:
        *st = rpc_s_connect_rejected;
        break;
        
        case RPC_C_SOCKET_ENETUNREACH:
        *st = rpc_s_network_unreachable;
        break;
        
        case RPC_C_SOCKET_ENOSPC:
        *st = rpc_s_connect_no_resources;
        break;
        
        case RPC_C_SOCKET_ENETDOWN:
        *st = rpc_s_rem_network_shutdown;
        break;
        
        case RPC_C_SOCKET_ETOOMANYREFS:
        *st = rpc_s_too_many_rem_connects;
        break;
        
        case RPC_C_SOCKET_ESRCH:
        *st = rpc_s_no_rem_endpoint;
        break;
        
        case RPC_C_SOCKET_EHOSTDOWN:
        *st = rpc_s_rem_host_down;
        break;
        
        case RPC_C_SOCKET_EHOSTUNREACH:
        *st = rpc_s_host_unreachable;
        break;
        
        case RPC_C_SOCKET_EACCESS:
        *st = rpc_s_access_control_info_inv;
        break;
        
        case RPC_C_SOCKET_ECONNABORTED:
        *st = rpc_s_loc_connect_aborted;
        break;
        
        case RPC_C_SOCKET_ECONNRESET:
        *st = rpc_s_connect_closed_by_rem;
        break;
        
        case RPC_C_SOCKET_ENETRESET:
        *st = rpc_s_rem_host_crashed;
        break;
        
        case RPC_C_SOCKET_ENOEXEC:
        *st = rpc_s_invalid_endpoint_format;
        break;
        
        default:
        *st = rpc_s_cannot_connect;
        break;
    }
}    


/*
**++
**
**  ROUTINE NAME:       rpc__cn_inq_sock_buffsize
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine returns the CN global socket buffer sizes.
**  A zero value means the operating system default.
**
**  INPUTS:		none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            
**
**      rsize           The receive buffer size (rpc_g_cn_socket_read_buffer)
**
**      ssize           The send buffer size (rpc_g_cn_socket_read_buffer)
**
**      st              The status code.
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

PRIVATE void
rpc__cn_inq_sock_buffsize(
	unsigned32	*rsize,
	unsigned32	*ssize,
	error_status_t	*st)
{
    *rsize = rpc_g_cn_socket_read_buffer;
    *ssize = rpc_g_cn_socket_write_buffer;
    *st = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc__cn_set_sock_buffsize
**
**  SCOPE:              PRIVATE - declared in cnnet.h
**
**  DESCRIPTION:
**
**  This routine sets the CN global socket buffer sizes.
**  A zero value for either buffer will use the OS default buffering.
**
**  INPUTS:
**
**       rsize          The socket receive buffer size
**
**       ssize          The socket send buffer size
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            
**
**      st              The status code.
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

PRIVATE void
rpc__cn_set_sock_buffsize(
	unsigned32	rsize,
	unsigned32	ssize,
	error_status_t	*st)
{
    rpc_g_cn_socket_read_buffer = rsize;
    rpc_g_cn_socket_write_buffer = ssize;
    *st = rpc_s_ok;
}
