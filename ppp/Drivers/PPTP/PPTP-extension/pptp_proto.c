/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#include <net/if.h>

#include "../../../Family/ppp_defs.h"
#include "../../../Family/if_ppplink.h"
#include "../../../Family/if_ppp.h"
#include "../../../Family/ppp_domain.h"


#include "PPTP.h"
#include "pptp_proto.h"
#include "pptp_rfc.h"
#include "pptp_wan.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */
/* Wcast-align fix - cast away alignment warning when buffer is aligned */
#define ALIGNED_CAST(type)	(type)(void *) 


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */

void pptp_init();
int pptp_ctloutput(struct socket *so, struct sockopt *sopt);
int pptp_usrreq();

int pptp_attach(struct socket *, int, struct proc *);
int pptp_detach(struct socket *);
int pptp_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p);

// callback from rfc layer
int pptp_input(void *data, mbuf_t m);
void pptp_event(void *data, u_int32_t event, u_int32_t msg);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
struct pr_usrreqs 	pptp_usr;	/* pr_usrreqs extension to the protosw */
struct protosw 		pptp;		/* describe the protocol switch */

extern lck_mtx_t	*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------- Admistrative functions, called by ppp_domain -----------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 PPTP Timer, at 500 ms. Replaces pptp_slowtimo, which is deprecated.
 ----------------------------------------------------------------------------- */
static uint8_t pptp_timer_thread_is_dying = 0; /* > 0 if dying */
static uint8_t pptp_timer_thread_is_dead = 0; /* > 0 if dead */

static void pptp_timer()
{
    struct timespec ts = {0};
    
    /* timeout of 500 ms */
    ts.tv_nsec = 500 * 1000 * 1000;
    ts.tv_sec = 0;
    
    lck_mtx_lock(ppp_domain_mutex);
    while (TRUE) {
        if (pptp_timer_thread_is_dying > 0) {
            break;
        }
        
        pptp_rfc_slowtimer();
        
        msleep(&pptp_timer_thread_is_dying, ppp_domain_mutex, PSOCK, "pptp_timer_sleep", &ts);
    }
    
    pptp_timer_thread_is_dead++;
    wakeup(&pptp_timer_thread_is_dead);
    lck_mtx_unlock(ppp_domain_mutex);
    
    thread_terminate(current_thread());
}

/* -----------------------------------------------------------------------------
Called when we need to add the PPTP protocol to the domain
Typically, ppp_add is called by ppp_domain when we add the domain,
but we can add the protocol anytime later, if the domain is present
----------------------------------------------------------------------------- */
int pptp_add(struct domain *domain)
{
    int 	 err;
    thread_t pptp_timer_thread = NULL;

    bzero(&pptp_usr, sizeof(struct pr_usrreqs));
    pptp_usr.pru_abort 	= pru_abort_notsupp;
    pptp_usr.pru_accept 	= pru_accept_notsupp;
    pptp_usr.pru_attach 	= pptp_attach;
    pptp_usr.pru_bind 		= pru_bind_notsupp;
    pptp_usr.pru_connect 	= pru_connect_notsupp;
    pptp_usr.pru_connect2 	= pru_connect2_notsupp;
    pptp_usr.pru_control 	= pptp_control;
    pptp_usr.pru_detach 	= pptp_detach;
    pptp_usr.pru_disconnect 	= pru_disconnect_notsupp;
    pptp_usr.pru_listen 	= pru_listen_notsupp;
    pptp_usr.pru_peeraddr 	= pru_peeraddr_notsupp;
    pptp_usr.pru_rcvd 		= pru_rcvd_notsupp;
    pptp_usr.pru_rcvoob 	= pru_rcvoob_notsupp;
    pptp_usr.pru_send 		= pru_send_notsupp;
    pptp_usr.pru_sense 		= pru_sense_null;
    pptp_usr.pru_shutdown 	= pru_shutdown_notsupp;
    pptp_usr.pru_sockaddr 	= pru_sockaddr_notsupp;
    pptp_usr.pru_sosend 	= sosend;
    pptp_usr.pru_soreceive 	= soreceive;
    pptp_usr.pru_sopoll 	= pru_sopoll_notsupp;


    bzero(&pptp, sizeof(struct protosw));
    pptp.pr_type		= SOCK_DGRAM;
    pptp.pr_domain		= domain;
    pptp.pr_protocol 	= PPPPROTO_PPTP;
    pptp.pr_flags		= PR_ATOMIC | PR_PROTOLOCK;
    pptp.pr_ctloutput 	= pptp_ctloutput;
    pptp.pr_init		= pptp_init;
    pptp.pr_usrreqs 	= &pptp_usr;
    
    pptp_timer_thread_is_dying = 0;
    if (kernel_thread_start((thread_continue_t)pptp_timer, NULL, &pptp_timer_thread) == KERN_SUCCESS) {
        thread_deallocate(pptp_timer_thread);
    }
    
    err = net_add_proto(&pptp, domain);
    if (err)
        return err;

    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
Called when we need to remove the PPTP protocol from the domain
----------------------------------------------------------------------------- */
int pptp_remove(struct domain *domain)
{
    int err;

    lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    /* Cleanup timer thread */
    if (pptp_timer_thread_is_dead == 0) {
        pptp_timer_thread_is_dying++;           /* Tell thread to die */
        wakeup(&pptp_timer_thread_is_dying);    /* Wake thread */
        msleep(&pptp_timer_thread_is_dying, ppp_domain_mutex, PSOCK, "pptp_timer_sleep", 0);
    }
    
    err = net_del_proto(pptp.pr_type, pptp.pr_protocol, domain);
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
void pptp_init()
{
    //IOLog("pptp_init\n");
}

/* -----------------------------------------------------------------------------
This function is called by socket layer to handle get/set-socketoption
----------------------------------------------------------------------------- */
int pptp_ctloutput(struct socket *so, struct sockopt *sopt)
{
    int		error, optval;
    u_int32_t	lval, cmd = 0;
    u_int16_t	val;
    
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    //IOLog("pptp_ctloutput, so = %p\n", so);

    error = optval = 0;
    if (sopt->sopt_level != PPPPROTO_PPTP) {
        return EINVAL;
    }

    switch (sopt->sopt_dir) {
        case SOPT_SET:
            switch (sopt->sopt_name) {
                case PPTP_OPT_FLAGS:
                case PPTP_OPT_OURADDRESS:
                case PPTP_OPT_PEERADDRESS:
                case PPTP_OPT_BAUDRATE:
                    if (sopt->sopt_valsize != 4)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &lval, 4, 4)) == 0) {
                        switch (sopt->sopt_name) {
                            case PPTP_OPT_OURADDRESS: 	cmd = PPTP_CMD_SETOURADDR; break;
                            case PPTP_OPT_PEERADDRESS: 	cmd = PPTP_CMD_SETPEERADDR; break;
                            case PPTP_OPT_FLAGS: 	cmd = PPTP_CMD_SETFLAGS; break;
                            case PPTP_OPT_BAUDRATE: 	cmd = PPTP_CMD_SETBAUDRATE; break;
                        }
                        pptp_rfc_command(so->so_pcb, cmd , &lval);
                    }
                    break;
                case PPTP_OPT_CALL_ID:
                case PPTP_OPT_PEER_CALL_ID:
                case PPTP_OPT_WINDOW:
                case PPTP_OPT_PEER_WINDOW:
                case PPTP_OPT_PEER_PPD:
                case PPTP_OPT_MAXTIMEOUT:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &val, 2, 2)) == 0) {
                        switch (sopt->sopt_name) {
                            case PPTP_OPT_CALL_ID: 	cmd = PPTP_CMD_SETCALLID; break;
                            case PPTP_OPT_PEER_CALL_ID: cmd = PPTP_CMD_SETPEERCALLID; break;
                            case PPTP_OPT_WINDOW: 	cmd = PPTP_CMD_SETWINDOW; break;
                            case PPTP_OPT_PEER_WINDOW: 	cmd = PPTP_CMD_SETPEERWINDOW; break;
                            case PPTP_OPT_PEER_PPD: 	cmd = PPTP_CMD_SETPEERPPD; break;
                            case PPTP_OPT_MAXTIMEOUT: 	cmd = PPTP_CMD_SETMAXTIMEOUT; break;
                        }
                        pptp_rfc_command(so->so_pcb, cmd , &val);
                    }
                    break;
                default:
                    error = ENOPROTOOPT;
            }
            break;

        case SOPT_GET:
            error = ENOPROTOOPT;
            break;

    }
    return error;
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
Should create all the structures and prepare for pptp dialog
----------------------------------------------------------------------------- */
int pptp_attach (struct socket *so, int proto, struct proc *p)
{
    int			error;

    //IOLog("pptp_attach, so = %p, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);
    if (so->so_pcb)
        return EINVAL;

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        error = soreserve(so, 8192, 8192);
        if (error)
            return error;
    }
   
    // call pptp init with the rfc specific structure
	lck_mtx_lock(ppp_domain_mutex);
    if (pptp_rfc_new_client(so, (void**)&(so->so_pcb), pptp_input, pptp_event)) {
		lck_mtx_unlock(ppp_domain_mutex);
        return ENOMEM;
    }

	lck_mtx_unlock(ppp_domain_mutex);
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer when the socket is closed
Should free all the pptp structures
----------------------------------------------------------------------------- */
int pptp_detach(struct socket *so)
{

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
	
    //IOLog("pptp_detach, so = %p, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);

    if (so->so_tpcb) {
        pptp_wan_detach(ALIGNED_CAST(struct ppp_link *)so->so_tpcb);    
        so->so_tpcb = 0;
    }
    if (so->so_pcb) {
        pptp_rfc_free_client(so->so_pcb);
        so->so_pcb = 0;
    }
	so->so_flags |= SOF_PCBCLEARING;
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer to handle ioctl
----------------------------------------------------------------------------- */
int pptp_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p)
{
    int 		error = 0;
    u_int32_t aligned_data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pptp_control : so = %p, cmd = %d\n", so, cmd);

    switch (cmd) {
	case PPPIOCGCHAN:
            //IOLog("pptp_control : PPPIOCGCHAN\n");
            if (!so->so_tpcb)
                return EINVAL;// not attached
            aligned_data = (ALIGNED_CAST(struct ppp_link *)so->so_tpcb)->lk_index;              // Wcast-align fix - we malloc so->so_tpcb - lk_index is u_1616_t copying to u_int32_t
            memcpy(data, &aligned_data, sizeof(u_int32_t));                                     // Wcast-align fix - memcpy for unaligned move
            break;
	case PPPIOCATTACH:
            //IOLog("pptp_control : PPPIOCATTACH\n");
           if (so->so_tpcb)
                return EINVAL;// already attached
            error = pptp_wan_attach(so->so_pcb, ALIGNED_CAST(struct ppp_link **)&so->so_tpcb);
            break;
	case PPPIOCDETACH:
            //IOLog("pptp_control : PPPIOCDETACH\n");
            if (!so->so_tpcb)
                return EINVAL;// already detached
            pptp_wan_detach(ALIGNED_CAST(struct ppp_link *)so->so_tpcb);
            so->so_tpcb = 0;
            break;
        default:
            ;
    }

    return error;
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- callbacks from pptp rfc or from dlil ----------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
called from pptp_rfc when data are present
----------------------------------------------------------------------------- */
int pptp_input(void *data, mbuf_t m)
{
    struct socket 	*so = (struct socket *)data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (so->so_tpcb) {
        // we are hooked to ppp
	return pptp_wan_input(ALIGNED_CAST(struct ppp_link *)so->so_tpcb, m);
    }

    mbuf_freem(m);
    IOLog("pptp_input unexpected, so = %p, len = %lu\n", so, mbuf_pkthdr_len(m));
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void pptp_event(void *data, u_int32_t event, u_int32_t msg)
{
    struct socket 	*so = (struct socket *)data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (so->so_tpcb) {
        switch (event) {
            case PPTP_EVT_XMIT_FULL:
                pptp_wan_xmit_full(ALIGNED_CAST(struct ppp_link *)so->so_tpcb);         // Wcast-align fix - we malloc so->so_tpcb
                break;
            case PPTP_EVT_XMIT_OK:
                pptp_wan_xmit_ok(ALIGNED_CAST(struct ppp_link *)so->so_tpcb);
                break;
            case PPTP_EVT_INPUTERROR:
                pptp_wan_input_error(ALIGNED_CAST(struct ppp_link *)so->so_tpcb);
                break;
        }
    }
}
