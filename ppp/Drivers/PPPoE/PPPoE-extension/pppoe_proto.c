/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <net/if_types.h>
#include <net/dlil.h>
#include <net/if_var.h>

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
int pppoe_input(void *data, struct mbuf *m);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
struct pr_usrreqs 	pppoe_usr;	/* pr_usrreqs extension to the protosw */
struct protosw 		pppoe;		/* describe the protocol switch */

u_long			pppoe_timer_count;

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
    pppoe_usr.pru_soreceive 	= soreceive;
    pppoe_usr.pru_sopoll 	= sopoll;


    bzero(&pppoe, sizeof(struct protosw));
    pppoe.pr_type	= SOCK_DGRAM;
    pppoe.pr_domain 	= domain;
    pppoe.pr_protocol 	= PPPPROTO_PPPOE;
    pppoe.pr_flags 	= PR_ATOMIC|PR_CONNREQUIRED;
    pppoe.pr_ctloutput 	= pppoe_ctloutput;
    pppoe.pr_init 	= pppoe_init;
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
    //log(LOGVAL, "pppoe_init\n");
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
    
    //log(LOGVAL, "pppoe_ctloutput, so = 0x%x\n", so);

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
                    //log(LOGVAL, "pppoe_ctloutput (set) : PPPOE_OPT_INTERFACE = %s, %d\n", ifname, val);
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
                        sprintf(str, "en%d", val);
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

    // run the slowtimer only every second
    if (pppoe_timer_count++ % 2) {
        return;
    }

    // run timer for RFC
    // is slow_timer called when no socket exist for that proto ?
    // we should probably used real timer function, directly instantiated from rfc.
    pppoe_rfc_timer();

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

    //log(LOGVAL, "pppoe_attach, so = 0x%x, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);
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
    if (pppoe_rfc_new_client(so, (void**)&(so->so_pcb), pppoe_input, pppoe_event)) {
        return ENOMEM;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer when the socket is closed
Should free all the pppoe structures
----------------------------------------------------------------------------- */
int pppoe_detach(struct socket *so)
{

    //log(LOGVAL, "pppoe_detach, so = 0x%x, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);

    if (so->so_tpcb) {
        pppoe_wan_detach((struct ppp_link *)so->so_tpcb);
        so->so_tpcb = 0;
    }
    if (so->so_pcb) {
        pppoe_rfc_free_client(so->so_pcb);
        so->so_pcb = 0;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
this function is not yet complete
----------------------------------------------------------------------------- */
int pppoe_shutdown(struct socket *so)
{
    int 	error = 0;

    //log(LOGVAL, "pppoe_shutdown, so = 0x%x\n", so);

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

    //log(LOGVAL, "pppoe_bind, so = 0x%x\n", so);

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

    //log(LOGVAL, "pppoe_connect, so = 0x%x\n", so);

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

    //log(LOGVAL, "pppoe_disconnect, so = 0x%x\n", so);

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

    //log(LOGVAL, "pppoe_listen, so = 0x%x\n", so);

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

    //log(LOGVAL, "pppoe_accept, so = 0x%x\n", so);

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

    //log(LOGVAL, "pppoe_abort, so = 0x%x\n", so);

    if (pppoe_rfc_abort(so->so_pcb))
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

    //log(LOGVAL, "pppoe_control : so = 0x%x, cmd = %d\n", so, cmd);

    switch (cmd) {
	case PPPIOCGCHAN:
            //log(LOGVAL, "pppoe_control : PPPIOCGCHAN\n");
            if (!so->so_tpcb)
                return EINVAL;// not attached
            *(u_int32_t *)data = ((struct ppp_link *)so->so_tpcb)->lk_index;
            break;
	case PPPIOCATTACH:
            //log(LOGVAL, "pppoe_control : PPPIOCATTACH\n");
           if (so->so_tpcb)
                return EINVAL;// already attached
            sbflush(&so->so_rcv);	// flush all data received
            error = pppoe_wan_attach(so->so_pcb, (struct ppp_link **)&so->so_tpcb);
            break;
	case PPPIOCDETACH:
            //log(LOGVAL, "pppoe_control : PPPIOCDETACH\n");
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

    //log(LOGVAL, "pppoe_send, so = 0x%x\n", so);

    if (pppoe_rfc_output(so->so_pcb, m)) {
        error = ENOTCONN; // XXX ???
        // shall I free the buffer ???
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
    static int 		ignore = 0; // change it
    struct socket 	*so = (struct socket *)data, *so2;

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
            so2 = sonewconn(so, SS_ISCONFIRMING);	// create the accepting connection
            //log(LOGVAL, "pppoe_event, so = 0x%x, so2 = 0x%x, evt = PPPOE_EVT_RINGING\n", so, so2);
            pppoe_rfc_clone(so->so_pcb, so2->so_pcb);	// transfer all the RFC info to the new connection, including ringing state
            ignore = 1;					// set a flag to ignore the abort resulting events
            pppoe_rfc_abort(so->so_pcb);		// abort the ring on the listening connection
            ignore = 0;					// reset the flag
            pppoe_rfc_listen(so->so_pcb);		// relisten again, because pppoe can handle multiple connections
            break;

        case PPPOE_EVT_CONNECTED:
            //log(LOGVAL, "pppoe_event, so = 0x%x, evt = PPPOE_EVT_CONNECTED\n", so);
            soisconnected(so);
            break;
        case PPPOE_EVT_DISCONNECTED:
            if (ignore)	// this event is due to the abort, following the ring (may be need to find a better way)
                break;
            //log(LOGVAL, "pppoe_event, so = 0x%x, evt = PPPOE_EVT_DISCONNECTED\n", so);
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
int pppoe_input(void *data, struct mbuf *m)
{
    struct socket 	*so = (struct socket *)data;

    if (so->so_tpcb) {
        // we are hooked to ppp
	return pppoe_wan_input((struct ppp_link *)so->so_tpcb, m);
    }

    //log(LOGVAL, "pppoe_input, so = 0x%x, len = %d\n", so, m_pkthdr.len);

    if (sbspace(&so->so_rcv) < m->m_pkthdr.len) {
        m_freem(m);
        log(LOGVAL, "pppoe_input no space, so = 0x%x, len = %d\n", so, m->m_pkthdr.len);
        return 0;
    }

    sbappendrecord(&so->so_rcv, m);
    sorwakeup(so);
    return 0;
}


