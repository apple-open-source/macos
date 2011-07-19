/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <kern/locks.h>

#include <net/if_types.h>
#include <net/dlil.h>

#include "../../../Family/ppp_defs.h"
#include "../../../Family/if_ppplink.h"
#include "../../../Family/if_ppp.h"
#include "../../../Family/ppp_domain.h"


#include "l2tpk.h"
#include "l2tp_proto.h"
#include "l2tp_rfc.h"
#include "l2tp_wan.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */

void l2tp_init();
int l2tp_ctloutput(struct socket *so, struct sockopt *sopt);
int l2tp_usrreq();
void l2tp_slowtimo();

int l2tp_attach(struct socket *, int, struct proc *);
int l2tp_detach(struct socket *);
int l2tp_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p);

int l2tp_send(struct socket *so, int flags, mbuf_t m, struct sockaddr *addr,
	    mbuf_t control, struct proc *p);

// callback from rfc layer
int l2tp_input(void *data, mbuf_t m, struct sockaddr *from, int more);
void l2tp_event(void *data, u_int32_t event, void *msg);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
struct pr_usrreqs 	l2tp_usr;	/* pr_usrreqs extension to the protosw */
struct protosw 		l2tp;		/* describe the protocol switch */

extern lck_mtx_t	*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------- Admistrative functions, called by ppp_domain -----------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Called when we need to add the L2TP protocol to the domain
Typically, ppp_add is called by ppp_domain when we add the domain,
but we can add the protocol anytime later, if the domain is present
----------------------------------------------------------------------------- */
int l2tp_add(struct domain *domain)
{
    int 	err;

    bzero(&l2tp_usr, sizeof(struct pr_usrreqs));
    l2tp_usr.pru_abort 		= pru_abort_notsupp;
    l2tp_usr.pru_accept 	= pru_accept_notsupp;
    l2tp_usr.pru_attach 	= l2tp_attach;
    l2tp_usr.pru_bind 		= pru_bind_notsupp;
    l2tp_usr.pru_connect 	= pru_connect_notsupp;
    l2tp_usr.pru_connect2 	= pru_connect2_notsupp;
    l2tp_usr.pru_control 	= l2tp_control;
    l2tp_usr.pru_detach 	= l2tp_detach;
    l2tp_usr.pru_disconnect	= pru_disconnect_notsupp;
    l2tp_usr.pru_listen 	= pru_listen_notsupp;
    l2tp_usr.pru_peeraddr 	= pru_peeraddr_notsupp;
    l2tp_usr.pru_rcvd 		= pru_rcvd_notsupp;
    l2tp_usr.pru_rcvoob 	= pru_rcvoob_notsupp;
    l2tp_usr.pru_send 		= (int	(*)(struct socket *, int, struct mbuf *, 
				 struct sockaddr *, struct mbuf *, struct proc *))l2tp_send;
    l2tp_usr.pru_sense 		= pru_sense_null;
    l2tp_usr.pru_shutdown 	= pru_shutdown_notsupp;
    l2tp_usr.pru_sockaddr 	= pru_sockaddr_notsupp;
    l2tp_usr.pru_sosend 	= sosend;
    l2tp_usr.pru_soreceive 	= soreceive;
    l2tp_usr.pru_sopoll 	= pru_sopoll_notsupp;


    bzero(&l2tp, sizeof(struct protosw));
    l2tp.pr_type		= SOCK_DGRAM;
    l2tp.pr_domain		= domain;
    l2tp.pr_protocol 	= PPPPROTO_L2TP;
    l2tp.pr_flags		= PR_ATOMIC | PR_ADDR | PR_PROTOLOCK;
    l2tp.pr_ctloutput 	= l2tp_ctloutput;
    l2tp.pr_init		= l2tp_init;
    l2tp.pr_slowtimo  	= l2tp_slowtimo;
    l2tp.pr_usrreqs 	= &l2tp_usr;

    err = net_add_proto(&l2tp, domain);
    if (err)
        return err;

    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
Called when we need to remove the L2TP protocol from the domain
----------------------------------------------------------------------------- */
int l2tp_remove(struct domain *domain)
{
    int err;

    err = net_del_proto(l2tp.pr_type, l2tp.pr_protocol, domain);
    if (err)
        return err;

    // shall we test that all the pcbs have been freed ?

    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------- protosw functions ----------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
This function is called by socket layer when the protocol is added
----------------------------------------------------------------------------- */
void l2tp_init()
{
    //IOLog("l2tp_init\n");
}

/* -----------------------------------------------------------------------------
This function is called by socket layer to handle get/set-socketoption
----------------------------------------------------------------------------- */
int l2tp_ctloutput(struct socket *so, struct sockopt *sopt)
{
    int		error, optval;
    u_int32_t	lval, cmd = 0;
    u_int16_t	val;
    u_char 	*addr;
		
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    //IOLog("l2tp_ctloutput, so = %p\n", so);

    error = optval = 0;
    if (sopt->sopt_level != PPPPROTO_L2TP) {
        return EINVAL;
    }

    switch (sopt->sopt_dir) {
        case SOPT_SET:
            switch (sopt->sopt_name) {
                case L2TP_OPT_FLAGS:
                case L2TP_OPT_BAUDRATE:
                    if (sopt->sopt_valsize != 4)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &lval, 4, 4)) == 0) {
                        switch (sopt->sopt_name) {
                            case L2TP_OPT_BAUDRATE: 		cmd = L2TP_CMD_SETBAUDRATE; break;
                            case L2TP_OPT_FLAGS:			cmd = L2TP_CMD_SETFLAGS; break;
                        }
                        l2tp_rfc_command(so->so_pcb, cmd, &lval);
					}
                    break;
                case L2TP_OPT_ACCEPT:
                    if (sopt->sopt_valsize != 0)
                    	error = EMSGSIZE;
                    else
                        l2tp_rfc_command(so->so_pcb, L2TP_CMD_ACCEPT, 0);
                    break;
                case L2TP_OPT_OURADDRESS:
                case L2TP_OPT_PEERADDRESS:
                    if (sopt->sopt_valsize < sizeof(struct sockaddr))
                        error = EMSGSIZE;
                    else {
                        if ((addr = _MALLOC(sopt->sopt_valsize, M_TEMP, M_WAITOK)) == 0)
                            error = ENOMEM;
                        else {
                            if ((error = sooptcopyin(sopt, addr, sopt->sopt_valsize, sopt->sopt_valsize)) == 0)
                                error = l2tp_rfc_command(so->so_pcb, 
                                    sopt->sopt_name == L2TP_OPT_OURADDRESS ? L2TP_CMD_SETOURADDR : L2TP_CMD_SETPEERADDR,
                                    addr);
                            _FREE(addr, M_TEMP);
                        }
                    }
                    break;
                case L2TP_OPT_TUNNEL_ID:
                case L2TP_OPT_PEER_TUNNEL_ID:
                case L2TP_OPT_SESSION_ID:
                case L2TP_OPT_PEER_SESSION_ID:
                case L2TP_OPT_WINDOW:
                case L2TP_OPT_PEER_WINDOW:
                case L2TP_OPT_INITIAL_TIMEOUT:
                case L2TP_OPT_TIMEOUT_CAP:
                case L2TP_OPT_MAX_RETRIES:
                case L2TP_OPT_RELIABILITY:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &val, 2, 2)) == 0) {
                        switch (sopt->sopt_name) {
                            case L2TP_OPT_TUNNEL_ID: 		cmd = L2TP_CMD_SETTUNNELID; break;
                            case L2TP_OPT_PEER_TUNNEL_ID: 	cmd = L2TP_CMD_SETPEERTUNNELID; break;
                            case L2TP_OPT_SESSION_ID: 		cmd = L2TP_CMD_SETSESSIONID; break;
                            case L2TP_OPT_PEER_SESSION_ID: 	cmd = L2TP_CMD_SETPEERSESSIONID; break;
                            case L2TP_OPT_WINDOW: 		cmd = L2TP_CMD_SETWINDOW; break;
                            case L2TP_OPT_PEER_WINDOW: 		cmd = L2TP_CMD_SETPEERWINDOW; break;
                            case L2TP_OPT_INITIAL_TIMEOUT: 	cmd = L2TP_CMD_SETTIMEOUT; break;
                            case L2TP_OPT_TIMEOUT_CAP: 		cmd = L2TP_CMD_SETTIMEOUTCAP; break;
                            case L2TP_OPT_MAX_RETRIES: 		cmd = L2TP_CMD_SETMAXRETRIES; break;
                            case L2TP_OPT_RELIABILITY: 		cmd = L2TP_CMD_SETRELIABILITY; break;
                        }
                        l2tp_rfc_command(so->so_pcb, cmd, &val);
                    }
                    break;
                default:
                    error = ENOPROTOOPT;
            }
            break;

        case SOPT_GET:
            switch (sopt->sopt_name) {
                case L2TP_OPT_NEW_TUNNEL_ID:
                case L2TP_OPT_TUNNEL_ID:
                case L2TP_OPT_SESSION_ID:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else {
                        switch (sopt->sopt_name) {
                            case L2TP_OPT_NEW_TUNNEL_ID: 	cmd = L2TP_CMD_GETNEWTUNNELID; break;
                            case L2TP_OPT_TUNNEL_ID: 		cmd = L2TP_CMD_GETTUNNELID; break;
                            case L2TP_OPT_SESSION_ID: 		cmd = L2TP_CMD_GETSESSIONID; break;
                        }
                        l2tp_rfc_command(so->so_pcb, cmd, &val);
                        error = sooptcopyout(sopt, &val, 2);
                    }
                    break;
                 case L2TP_OPT_FLAGS:
                    if (sopt->sopt_valsize != 4)
                        error = EMSGSIZE;
                    else {
                        l2tp_rfc_command(so->so_pcb, L2TP_CMD_GETFLAGS, &lval);
                        error = sooptcopyout(sopt, &lval, 4);
                    }
                    break;
                case L2TP_OPT_OURADDRESS:
                case L2TP_OPT_PEERADDRESS:
                    if ((addr = _MALLOC(sopt->sopt_valsize, M_TEMP, M_WAITOK)) == 0)
                        error = ENOMEM;
                    else {
                        *addr = sopt->sopt_valsize; /* max size */
                        if ((error = l2tp_rfc_command(so->so_pcb, 
                                sopt->sopt_name == L2TP_OPT_OURADDRESS ? L2TP_CMD_GETOURADDR : L2TP_CMD_GETPEERADDR,
                                addr)) == 0) {
                            error = sooptcopyout(sopt, addr, sopt->sopt_valsize);
                            _FREE(addr, M_TEMP);
                        }
                    }
                    break;
            
                default:
                    error = ENOPROTOOPT;
                    break;
            }
            break;
    }
    return error;
}

/* -----------------------------------------------------------------------------
slow timer function, called every 500ms
----------------------------------------------------------------------------- */
void l2tp_slowtimo()
{
    lck_mtx_lock(ppp_domain_mutex);
    l2tp_rfc_slowtimer();
	lck_mtx_unlock(ppp_domain_mutex);
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- pr_usrreqs functions ---------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Called by socket layer when a new socket is created
Should create all the structures and prepare for L2TP dialog
----------------------------------------------------------------------------- */
int l2tp_attach (struct socket *so, int proto, struct proc *p)
{
    int			error;

    //IOLog("l2tp_attach, so = %p, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);
    if (so->so_pcb)
        return EINVAL;

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        error = soreserve(so, 8192, 8192);
        if (error)
            return error;
    }
   
    // call l2tp init with the rfc specific structure
	lck_mtx_lock(ppp_domain_mutex);
    if (l2tp_rfc_new_client(so, (void**)&(so->so_pcb), l2tp_input, l2tp_event)) {
		lck_mtx_unlock(ppp_domain_mutex);
        return ENOMEM;
    }

	lck_mtx_unlock(ppp_domain_mutex);
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer when the socket is closed
Should free all the L2TP structures
----------------------------------------------------------------------------- */
int l2tp_detach(struct socket *so)
{

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
	
    //IOLog("l2tp_detach, so = %p, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);

    if (so->so_tpcb) {
        l2tp_wan_detach((struct ppp_link *)so->so_tpcb);
        so->so_tpcb = 0;
    }
    if (so->so_pcb) {
        l2tp_rfc_free_client(so->so_pcb);
        so->so_pcb = 0;
    }
	so->so_flags |= SOF_PCBCLEARING;
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer to handle ioctl
----------------------------------------------------------------------------- */
int l2tp_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p)
{
    int 		error = 0;

    //IOLog("l2tp_control : so = %p, cmd = %d\n", so, cmd);
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    switch (cmd) {
	case PPPIOCGCHAN:
            //IOLog("l2tp_control : PPPIOCGCHAN\n");
            if (!so->so_tpcb)
                return EINVAL;// not attached
            *(u_int32_t *)data = ((struct ppp_link *)so->so_tpcb)->lk_index;
            break;
	case PPPIOCATTACH:
            //IOLog("l2tp_control : PPPIOCATTACH\n");
           if (so->so_tpcb)
                return EINVAL;// already attached
            error = l2tp_wan_attach(so->so_pcb, (struct ppp_link **)&so->so_tpcb);
            break;
	case PPPIOCDETACH:
            //IOLog("l2tp_control : PPPIOCDETACH\n");
            if (!so->so_tpcb)
                return EINVAL;// already detached
            l2tp_wan_detach((struct ppp_link *)so->so_tpcb);
            so->so_tpcb = 0;
            break;
        default:
            ;
    }

    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to send a packet
----------------------------------------------------------------------------- */
int l2tp_send(struct socket *so, int flags, mbuf_t m, struct sockaddr *to,
	    mbuf_t control, struct proc *p)
{

    if (control)
        mbuf_freem(control);
    if (mbuf_len(m) == 0) {
        mbuf_freem(m);
        return 0;
    }

    return l2tp_rfc_output(so->so_pcb, m, to);
}


/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- callbacks from L2TP rfc or from dlil ----------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
called from l2tp_rfc when data are present
----------------------------------------------------------------------------- */
int l2tp_input(void *data, mbuf_t m, struct sockaddr *from, int more)
{
    struct socket 	*so = (struct socket *)data;
	int		err;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (so->so_tpcb) {
        // we are hooked to ppp
	return l2tp_wan_input((struct ppp_link *)so->so_tpcb, m);
    }
    
    if (m) {
	if (from == 0) {            
            // no from address, just free the buffer
            mbuf_freem(m);
            return 1;
        }

	if (sbappendaddr(&so->so_rcv, from, (struct mbuf *)m, 0, &err) == 0) {
            //IOLog("l2tp_input no space, so = %p\n", so);
            return 1;
	}
    }
    
    if (!more)
        sorwakeup(so);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void l2tp_event(void *data, u_int32_t event, void *msg)
{
    struct socket 	*so = (struct socket *)data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (so->so_tpcb) {
        switch (event) {
            case L2TP_EVT_XMIT_FULL:
                l2tp_wan_xmit_full((struct ppp_link *)so->so_tpcb);
                break;
            case L2TP_EVT_XMIT_OK:
                l2tp_wan_xmit_ok((struct ppp_link *)so->so_tpcb);
                break;
            case L2TP_EVT_INPUTERROR:
                l2tp_wan_input_error((struct ppp_link *)so->so_tpcb);
                break;
        }
    }
    else {
        switch (event) {
            case L2TP_EVT_RELIABLE_FAILED:
                /* wake up the client with no data */
                socantrcvmore(so);
                break;
        }
    }
}
