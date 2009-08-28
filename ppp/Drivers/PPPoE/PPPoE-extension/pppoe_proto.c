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


#include "PPPoE.h"
#include "pppoe_proto.h"
#include "pppoe_rfc.h"
#include "pppoe_wan.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */

void pppoe_init();
int pppoe_ctloutput(struct socket *so, struct sockopt *sopt);
int pppoe_usrreq();
void pppoe_slowtimo();

int pppoe_attach(struct socket *, int, struct proc *);
int pppoe_detach(struct socket *);
int pppoe_shutdown(struct socket *);
int pppoe_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p);
int pppoe_connect(struct socket *so, struct sockaddr *nam, struct proc *p);
int pppoe_disconnect(struct socket *so);
int pppoe_send(struct socket *so, int flags, struct mbuf *m,
               struct sockaddr *nam, struct mbuf *control, struct proc *p);
int pppoe_bind(struct socket *so, struct sockaddr *nam, struct proc *p);
int pppoe_accept(struct socket *so, struct sockaddr **nam);
int pppoe_listen(struct socket *so, struct proc *p);

// callback from rfc layer
void pppoe_event(void *data, u_int32_t event, u_int32_t msg);
int pppoe_input(void *data, mbuf_t m);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
struct pr_usrreqs 	pppoe_usr;	/* pr_usrreqs extension to the protosw */
struct protosw 		pppoe;		/* describe the protocol switch */

u_int32_t			pppoe_timer_count;

extern lck_mtx_t	*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------- Admistrative functions, called by ppp_domain -----------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Called when we need to add the PPPoE protocol to the domain
Typically, ppp_add is called by ppp_domain when we add the domain,
but we can add the protocol anytime later, if the domain is present
----------------------------------------------------------------------------- */
int pppoe_add(struct domain *domain)
{
    int 	err;

    pppoe_timer_count = 0;

    bzero(&pppoe_usr, sizeof(struct pr_usrreqs));
    pppoe_usr.pru_abort 	= pru_abort_notsupp;
    pppoe_usr.pru_accept 	= pppoe_accept;
    pppoe_usr.pru_attach 	= pppoe_attach;
    pppoe_usr.pru_bind 		= pppoe_bind;
    pppoe_usr.pru_connect 	= pppoe_connect;
    pppoe_usr.pru_connect2 	= pru_connect2_notsupp;
    pppoe_usr.pru_control 	= pppoe_control;
    pppoe_usr.pru_detach 	= pppoe_detach;
    pppoe_usr.pru_disconnect 	= pppoe_disconnect;
    pppoe_usr.pru_listen 	= pppoe_listen;
    pppoe_usr.pru_peeraddr 	= pru_peeraddr_notsupp;
    pppoe_usr.pru_rcvd 		= pru_rcvd_notsupp;
    pppoe_usr.pru_rcvoob 	= pru_rcvoob_notsupp;
    pppoe_usr.pru_send 		= pppoe_send;
    pppoe_usr.pru_sense 	= pru_sense_null;
    pppoe_usr.pru_shutdown 	= pppoe_shutdown;
    pppoe_usr.pru_sockaddr 	= pru_sockaddr_notsupp;
    pppoe_usr.pru_sosend 	= sosend;
    pppoe_usr.pru_soreceive = soreceive;
    pppoe_usr.pru_sopoll 	= pru_sopoll_notsupp;


    bzero(&pppoe, sizeof(struct protosw));
    pppoe.pr_type		= SOCK_DGRAM;
    pppoe.pr_domain 	= domain;
    pppoe.pr_protocol 	= PPPPROTO_PPPOE;
    pppoe.pr_flags		= PR_ATOMIC|PR_CONNREQUIRED|PR_PROTOLOCK;
    pppoe.pr_ctloutput 	= pppoe_ctloutput;
    pppoe.pr_init		= pppoe_init;
    pppoe.pr_slowtimo  	= pppoe_slowtimo;
    pppoe.pr_usrreqs 	= &pppoe_usr;

    err = net_add_proto(&pppoe, domain);
    if (err)
        return err;

    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
Called when we need to remove the PPPoE protocol from the domain
----------------------------------------------------------------------------- */
int pppoe_remove(struct domain *domain)
{
    int err;

    err = net_del_proto(pppoe.pr_type, pppoe.pr_protocol, domain);
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
void pppoe_init()
{
    //IOLog("pppoe_init\n");
}

/* -----------------------------------------------------------------------------
This function is called by socket layer to handle get/set-socketoption
----------------------------------------------------------------------------- */
int pppoe_ctloutput(struct socket *so, struct sockopt *sopt)
{
    int		error, optval, i, mult;
    u_int16_t	val;
    u_int32_t	lval;
    u_char 	str[IFNAMSIZ];
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    //IOLog("pppoe_ctloutput, so = %p\n", so);

    error = optval = 0;
    if (sopt->sopt_level != PPPPROTO_PPPOE) {
        return EINVAL;
    }

    switch (sopt->sopt_dir) {
        case SOPT_SET:
            switch (sopt->sopt_name) {
                case PPPOE_OPT_FLAGS:
                    if (sopt->sopt_valsize != 4)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &lval, 4, 4)) == 0)
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_SETFLAGS, &lval);
                    break;
                case PPPOE_OPT_INTERFACE:
                    if (sopt->sopt_valsize > IFNAMSIZ) {
                        error = EMSGSIZE;
                        break;
                    }
                    bzero(str, sizeof(str));
                    if (error = sooptcopyin(sopt, str, sopt->sopt_valsize, 0))
                        break;
                    val = 0;
                    for (i = IFNAMSIZ - 1; i && !str[i]; i--);
                    for (mult = 1; i && (str[i] >= '0' && str[i] <= '9'); i--) {
                        val += (str[i] - '0') * mult;
                        mult *= 10;
                    }
                    //IOLog("pppoe_ctloutput (set) : PPPOE_OPT_INTERFACE = %s, %d\n", ifname, val);
                    pppoe_rfc_command(so->so_pcb, PPPOE_CMD_SETUNIT, &val);
                    break;
                case PPPOE_OPT_CONNECT_TIMER:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &val, 2, 2)) == 0)
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_SETCONNECTTIMER , &val);
                    break;
                case PPPOE_OPT_RING_TIMER:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &val, 2, 2)) == 0)
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_SETRINGTIMER , &val);
                    break;
                case PPPOE_OPT_RETRY_TIMER:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &val, 2, 2)) == 0)
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_SETRETRYTIMER , &val);
                    break;
                case PPPOE_OPT_PEER_ENETADDR:
                    if (sopt->sopt_valsize != 6)
                        error = EMSGSIZE;
                    else if ((error = sooptcopyin(sopt, &str, 2, 2)) == 0)
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_SETPEERADDR , &str);
                    break;
                default:
                    error = ENOPROTOOPT;
            }
            break;

        case SOPT_GET:
            switch (sopt->sopt_name) {
                case PPPOE_OPT_FLAGS:
                    if (sopt->sopt_valsize != 4)
                        error = EMSGSIZE;
                    else {
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_GETFLAGS, &lval);
                        error = sooptcopyout(sopt, &lval, 4);
                    }
                    break;
                case PPPOE_OPT_INTERFACE:
                    if (sopt->sopt_valsize < IFNAMSIZ)
                        error = EMSGSIZE;
                    else {
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_GETUNIT, &val);
                        // Fix Me : should get the name from ifnet
                        snprintf(str, sizeof(str), "en%d", val);
                        error = sooptcopyout(sopt, str, strlen(str));
                    }
                    break;
                case PPPOE_OPT_CONNECT_TIMER:
                    if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else {
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_GETCONNECTTIMER, &val);
                        error = sooptcopyout(sopt, &val, 2);
                    }
                    break;
                case PPPOE_OPT_RING_TIMER:
                     if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else {
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_GETRINGTIMER, &val);
                        error = sooptcopyout(sopt, &val, 2);
                    }
                    break;
                case PPPOE_OPT_RETRY_TIMER:
                     if (sopt->sopt_valsize != 2)
                        error = EMSGSIZE;
                    else {
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_GETRETRYTIMER, &val);
                        error = sooptcopyout(sopt, &val, 2);
                    }
                    break;
                case PPPOE_OPT_PEER_ENETADDR:
                    if (sopt->sopt_valsize != 6)
                        error = EMSGSIZE;
                    else {
                        pppoe_rfc_command(so->so_pcb, PPPOE_CMD_GETPEERADDR, &str);
                        error = sooptcopyout(sopt, &str, 6);
                    }
                    break;
                default:
                    error = ENOPROTOOPT;
            }
            break;

    }
    return error;
}

/* -----------------------------------------------------------------------------
slow timer function, called every 500ms
----------------------------------------------------------------------------- */
void pppoe_slowtimo()
{
	
	lck_mtx_lock(ppp_domain_mutex);
    // run the slowtimer only every second
    if (pppoe_timer_count++ % 2) {
		lck_mtx_unlock(ppp_domain_mutex);
        return;
    }

    // run timer for RFC
    // is slow_timer called when no socket exist for that proto ?
    // we should probably used real timer function, directly instantiated from rfc.
    pppoe_rfc_timer();
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
Should create all the structures and prepare for pppoe dialog
----------------------------------------------------------------------------- */
int pppoe_attach (struct socket *so, int proto, struct proc *p)
{
    int			error;
    u_short 		unit;

    //IOLog("pppoe_attach, so = %p, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);
    if (so->so_pcb)
        return EINVAL;

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        error = soreserve(so, 8192, 8192);
        if (error)
            return error;
    }

    // fix me : change association between socket and dltag to support multiple interface
    //pcb->dl_tag = pppoe_domain_find_dl_tag();
    unit = 0;
   
    // call pppoe init with the rfc specific structure
	lck_mtx_lock(ppp_domain_mutex);
    if (pppoe_rfc_new_client(so, (void**)&(so->so_pcb), pppoe_input, pppoe_event)) {
		lck_mtx_unlock(ppp_domain_mutex);
        return ENOMEM;
    }
	lck_mtx_unlock(ppp_domain_mutex);
	
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer when the socket is closed
Should free all the pppoe structures
----------------------------------------------------------------------------- */
int pppoe_detach(struct socket *so)
{

    //IOLog("pppoe_detach, so = %p, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (so->so_tpcb) {
        pppoe_wan_detach((struct ppp_link *)so->so_tpcb);
        so->so_tpcb = 0;
    }
    if (so->so_pcb) {
        pppoe_rfc_free_client(so->so_pcb);
        so->so_pcb = 0;
    }
	so->so_flags |= SOF_PCBCLEARING;
    return 0;
}

/* -----------------------------------------------------------------------------
this function is not yet complete
----------------------------------------------------------------------------- */
int pppoe_shutdown(struct socket *so)
{
    int 	error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_shutdown, so = %p\n", so);

    socantsendmore(so);
    pppoe_disconnect(so);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to bind to the protocol
----------------------------------------------------------------------------- */
int pppoe_bind(struct socket *so, struct sockaddr *nam, struct proc *p)
{
    int 		error = 0;
    struct sockaddr_pppoe *adr = (struct sockaddr_pppoe *)nam;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_bind, so = %p\n", so);

    // bind pppoe protocol
    if (pppoe_rfc_bind(so->so_pcb, &adr->pppoe_ac_name[0], &adr->pppoe_service[0]))
        error = EINVAL;		/* XXX ??? */

    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to connect the protocol (PR_CONNREQUIRED)
----------------------------------------------------------------------------- */
int pppoe_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{
    int 		error = 0;
    struct sockaddr_pppoe *adr = (struct sockaddr_pppoe *)nam;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_connect, so = %p\n", so);

    // connect pppoe protocol
    if (pppoe_rfc_connect(so->so_pcb, adr->pppoe_ac_name, adr->pppoe_service))
        error = EINVAL;		/* XXX ??? */
    else {
        soisconnecting(so);
    }

    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to disconnect the protocol (PR_CONNREQUIRED)
----------------------------------------------------------------------------- */
int pppoe_disconnect(struct socket *so)
{

    //IOLog("pppoe_disconnect, so = %p\n", so);
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    // disconnect pppoe protocol
    if (pppoe_rfc_disconnect(so->so_pcb)) {
        // ???
    }

    soisdisconnected(so); // let's say we are disconnected anyway...
    return 0;
}

/* -----------------------------------------------------------------------------
Prepare to accept connections
----------------------------------------------------------------------------- */
int pppoe_listen(struct socket *so, struct proc *p)
{
    int 		error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_listen, so = %p\n", so);

    if (pppoe_rfc_listen(so->so_pcb))
        error = EINVAL; // XXX ???

    return error;
}

/* -----------------------------------------------------------------------------
Accept connection
----------------------------------------------------------------------------- */
int pppoe_accept(struct socket *so, struct sockaddr **nam)
{
    int 		error = 0;
    struct sockaddr_pppoe *addr;

    //IOLog("pppoe_accept, so = %p\n", so);
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    *nam = 0;
    if (pppoe_rfc_accept(so->so_pcb))
        error = EINVAL; // XXX ???
    else {
        // pppoe_rfc_getboundaddr(pcb->rfc, addr.ac_name, addr.service);
        addr = (struct sockaddr_pppoe *)_MALLOC(sizeof (struct sockaddr_pppoe), M_SONAME, M_WAITOK);
        if (addr) {
            addr->ppp.ppp_len = sizeof(struct sockaddr_pppoe);
            addr->ppp.ppp_family = AF_PPP;
            addr->ppp.ppp_proto = PPPPROTO_PPPOE;
            addr->ppp.ppp_cookie = 0;
            addr->pppoe_ac_name[0] = 0;
            addr->pppoe_service[0] = 0;
            *nam = (struct sockaddr *)addr;
        }
    }

    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to abort call
----------------------------------------------------------------------------- */
int pppoe_abort(struct socket *so)
{
    int 		error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_abort, so = %p\n", so);

    if (pppoe_rfc_abort(so->so_pcb, 1))
        error = EINVAL; // XXX ???

    soisdisconnected(so); // let's say we are disconnected anyway...
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to handle ioctl
----------------------------------------------------------------------------- */
int pppoe_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p)
{
    int 		error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_control : so = %p, cmd = %d\n", so, cmd);

    switch (cmd) {
	case PPPIOCGCHAN:
            //IOLog("pppoe_control : PPPIOCGCHAN\n");
            if (!so->so_tpcb)
                return EINVAL;// not attached
            *(u_int32_t *)data = ((struct ppp_link *)so->so_tpcb)->lk_index;
            break;
	case PPPIOCATTACH:
            //IOLog("pppoe_control : PPPIOCATTACH\n");
           if (so->so_tpcb)
                return EINVAL;// already attached
            sbflush(&so->so_rcv);	// flush all data received
            error = pppoe_wan_attach(so->so_pcb, (struct ppp_link **)&so->so_tpcb);
            break;
	case PPPIOCDETACH:
            //IOLog("pppoe_control : PPPIOCDETACH\n");
            if (!so->so_tpcb)
                return EINVAL;// already detached
            pppoe_wan_detach((struct ppp_link *)so->so_tpcb);
            so->so_tpcb = 0;
            break;
        default:
            ;
    }

    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to send data out
----------------------------------------------------------------------------- */
int pppoe_send(struct socket *so, int flags, struct mbuf *m,
               struct sockaddr *nam, struct mbuf *control, struct proc *p)
{

    int 		error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //IOLog("pppoe_send, so = %p\n", so);

    if (error = pppoe_rfc_output(so->so_pcb, (mbuf_t)m)) {
        m_freem(m);
    }

    return error;
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- callbacks from pppoe rfc or from dlil ----------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
called from pppoe_rfc when change state occurs
----------------------------------------------------------------------------- */
void pppoe_event(void *data, u_int32_t event, u_int32_t msg)
{
    struct socket 	*so = (struct socket *)data, *so2;
	struct sockaddr_pppoe addr;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    switch (event) {
        case PPPOE_EVT_RINGING:
            /*
             at this point, we need to introduce our semantic about how ring/accept will work.
             when there is an incoming call, we create a new socket with sonewconn, so socket layer is happy.
             the sonewconn will wake up the original listening app with the ogirinal socket.
             the app will decide wether it wants to accept or refuse the call.
             if the app decides to accept it, il will call accept with the original listening socket,
             which will be turned by the socket layer into a call to the accept function to our protocol
             with the second socket (created with sonewconn). [the socket layer knows that
                 those two sockets are related, because sonewconn had linked them together]
             when we enter into our accept function, we will then physically answer the call.
             (that's a difference with tcp, where the call has already be answer at ring time)
             the original listening socket will still be listening (probably for nothing if the protocol
                                                                    can only accept one call)
             then, when later the app close the socket (the one returned by accept) the protocol can listen again
             it's up to this protocol to know what's really happening here, and to do the correct work,
             wether we have only one active listening socket, or whatever makes sense.
             in case of PPPoE, let's say we can continue listening....
             */
			 
			addr.ppp.ppp_len = sizeof(struct sockaddr_pppoe);
			addr.ppp.ppp_family = AF_PPP;
			addr.ppp.ppp_proto = PPPPROTO_PPPOE;
			addr.ppp.ppp_cookie = 0;
			addr.pppoe_ac_name[0] = 0;
			addr.pppoe_service[0] = 0;

            so2 = sonewconn(so, SS_ISCONFIRMING, (struct sockaddr*)(&addr));	// create the accepting connection
            //IOLog("pppoe_event, so = %p, so2 = %p, evt = PPPOE_EVT_RINGING\n", so, so2);
            
			if (so2)
				pppoe_rfc_clone(so->so_pcb, so2->so_pcb);	// transfer all the RFC info to the new connection, including ringing state

            pppoe_rfc_abort(so->so_pcb, 0);		// abort the ring on the listening connection
            pppoe_rfc_listen(so->so_pcb);		// relisten again, because pppoe can handle multiple connections
            break;

        case PPPOE_EVT_CONNECTED:
            //IOLog("pppoe_event, so = %p, evt = PPPOE_EVT_CONNECTED\n", so);
            soisconnected(so);
            break;

        case PPPOE_EVT_DISCONNECTED:
            //IOLog("pppoe_event, so = %p, evt = PPPOE_EVT_DISCONNECTED\n", so);
            so->so_error = msg;
     
            //if (so->so_tpcb) {
             //   pppoe_wan_detach((struct ppp_link *)so->so_tpcb);
            //    so->so_tpcb = 0;
            //}
            soisdisconnected(so);
            break;
    }
}

/* -----------------------------------------------------------------------------
called from pppoe_rfc when data are present
----------------------------------------------------------------------------- */
int pppoe_input(void *data, mbuf_t m)
{
    struct socket 	*so = (struct socket *)data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (so->so_tpcb) {
        // we are hooked to ppp
	return pppoe_wan_input((struct ppp_link *)so->so_tpcb, m);
    }

    //IOLog("pppoe_input, so = %p, len = %d\n", so, m_pkthdr.len);

    if (sbspace(&so->so_rcv) < mbuf_pkthdr_len(m)) {
        mbuf_freem(m);
        IOLog("pppoe_input no space, so = %p, len = %d\n", so, mbuf_pkthdr_len(m));
        return 0;
    }

    sbappendrecord(&so->so_rcv, (struct mbuf*)m);
    sorwakeup(so);
    return 0;
}


