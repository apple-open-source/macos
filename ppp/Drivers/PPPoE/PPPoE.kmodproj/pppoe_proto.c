/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/protosw.h>

#include <machine/spl.h>

#include "PPPoE.h"
#include "pppoe_proto.h"
#include "pppoe_rfc.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */
#define LOGVAL LOG_INFO

struct pppoe_pcb {
    // socket related info
    struct socket 	*socket;

    // protosw related info
    //u_int32_t		dl_tag;
    //u_int16_t  		loopback;

    // protocol private info
    void 		*rfc;
};

#define MAX_PCB 	32 	// humm? is there a better way ?



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

struct pppoe_pcb 	*pppoe_pcb[MAX_PCB];
u_long			pppoe_npcb;

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
    u_long	i;

    pppoe_npcb = 0;
    for (i = 0; i < MAX_PCB; pppoe_pcb[i++] = 0);
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
    pppoe.pr_protocol 	= PPPOEPROTO_PPPOE;
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
    log(LOGVAL, "pppoe_init\n");
}

/* -----------------------------------------------------------------------------
This function is called by socket layer to handle get/set-socketoption
----------------------------------------------------------------------------- */
int pppoe_ctloutput(struct socket *so, struct sockopt *sopt)
{
    int	error, optval;

    log(LOGVAL, "pppoe_ctloutput, so = 0x%x\n", so);

    error = optval = 0;
    if (sopt->sopt_level != PPPOEPROTO_PPPOE) {
        return EINVAL;
    }

    switch (sopt->sopt_dir) {
        case SOPT_SET:
            switch (sopt->sopt_name) {
                default:
                    error = ENOPROTOOPT;
                    break;
            }
            break;

        case SOPT_GET:
            switch (sopt->sopt_name) {
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
void pppoe_slowtimo()
{
    int s = splnet();

    // run the slowtimer only every second
    if (pppoe_timer_count++ % 2) {
        splx(s);
        return;
    }

    // run timer for RFC
    // is slow_timer called when no socket exist for that proto ?
    // we should probably used real timer function, directly instantiated from rfc.
    pppoe_rfc_timer();

    splx(s);
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- pr_usrreqs functions ---------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct pppoe_pcb *pppoe_allocpcb ()
{
    u_long	 i;

    for (i = 0; i < MAX_PCB; i++) {
        if (pppoe_pcb[i] == 0) {
            pppoe_pcb[i] = (struct pppoe_pcb *)_MALLOC(sizeof (struct pppoe_pcb), M_DEVBUF, M_WAITOK);
            if (pppoe_pcb[i] == 0)
                return 0;
            return pppoe_pcb[i];
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void pppoe_freepcb (struct pppoe_pcb *pcb)
{
    u_long	 i;

    for (i = 0; i < MAX_PCB; i++) {
        if (pppoe_pcb[i] == pcb) {
            _FREE(pppoe_pcb[i], M_DEVBUF);
            pppoe_pcb[i] = 0;
            return;
        }
    }
}

/* -----------------------------------------------------------------------------
Called by socket layer when a new socket is created
Should create all the structures and prepare for pppoe dialog
----------------------------------------------------------------------------- */
int pppoe_attach (struct socket *so, int proto, struct proc *p)
{
    struct pppoe_pcb 	*pcb;
    int			error;
    u_short 		unit;

    log(LOGVAL, "pppoe_attach, so = 0x%x, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);
    if (so->so_pcb)
        return EINVAL;

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        error = soreserve(so, 8192, 8192);
        if (error)
            return error;
    }

    pcb = pppoe_allocpcb();
    if (pcb == 0)
        return ENOMEM;

    bzero((char *)pcb, sizeof (struct pppoe_pcb));

    // save info about socket
    pcb->socket = so;

    // fix me : change association between socket and dltag to support multiple interface
    //pcb->dl_tag = pppoe_domain_find_dl_tag();
    unit = 0;
   
    so->so_pcb = (caddr_t)pcb;

    // call pppoe init with the rfc specific structure
    if (pppoe_rfc_new_client(pcb, unit, &pcb->rfc, pppoe_input, pppoe_event)) {
        pppoe_freepcb(pcb);
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
    struct pppoe_pcb *pcb = (struct pppoe_pcb *)so->so_pcb;

    log(LOGVAL, "pppoe_detach, so = 0x%x, dom_ref = %d\n", so, so->so_proto->pr_domain->dom_refs);

    if (pcb) {
        pppoe_rfc_free_client(pcb->rfc);
        pppoe_freepcb(pcb);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
this function is not yet complete
----------------------------------------------------------------------------- */
int pppoe_shutdown(struct socket *so)
{
    int 	error = 0, s = splnet();

    log(LOGVAL, "pppoe_shutdown, so = 0x%x\n", so);

    socantsendmore(so);

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to bind to the protocol
----------------------------------------------------------------------------- */
int pppoe_bind(struct socket *so, struct sockaddr *nam, struct proc *p)
{
    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;
    struct sockaddr_pppoe *adr = (struct sockaddr_pppoe *)nam;

    log(LOGVAL, "pppoe_bind, so = 0x%x\n", so);

    // bind pppoe protocol
    if (pppoe_rfc_bind(pcb->rfc, &adr->pppoe_ac_name[0], &adr->pppoe_service[0]))
        error = EINVAL;		/* XXX ??? */

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to connect the protocol (PR_CONNREQUIRED)
----------------------------------------------------------------------------- */
int pppoe_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{
    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;
    struct sockaddr_pppoe *adr = (struct sockaddr_pppoe *)nam;

    log(LOGVAL, "pppoe_connect, so = 0x%x\n", so);

    // connect pppoe protocol
    if (pppoe_rfc_connect(pcb->rfc, &adr->pppoe_ac_name[0], &adr->pppoe_service[0]))
        error = EINVAL;		/* XXX ??? */
    else {
        soisconnecting(so);
    }

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to disconnect the protocol (PR_CONNREQUIRED)
----------------------------------------------------------------------------- */
int pppoe_disconnect(struct socket *so)
{
    int s = splnet();
    struct pppoe_pcb *pcb = (struct pppoe_pcb *)so->so_pcb;

    log(LOGVAL, "pppoe_disconnect, so = 0x%x\n", so);

    // disconnect pppoe protocol
    if (pppoe_rfc_disconnect(pcb->rfc)) {
        // ???
    }

    soisdisconnected(so); // let's say we are disconnected anyway...

    splx(s);
    return 0;
}

/* -----------------------------------------------------------------------------
Prepare to accept connections
----------------------------------------------------------------------------- */
int pppoe_listen(struct socket *so, struct proc *p)
{
    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;

    log(LOGVAL, "pppoe_listen, so = 0x%x\n", so);

    if (pppoe_rfc_listen(pcb->rfc))
        error = EINVAL; // XXX ???

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Accept connection
----------------------------------------------------------------------------- */
int pppoe_accept(struct socket *so, struct sockaddr **nam)
{
    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;
    struct sockaddr_pppoe *addr;

    log(LOGVAL, "pppoe_accept, so = 0x%x\n", so);

    *nam = 0;
    if (pppoe_rfc_accept(pcb->rfc))
        error = EINVAL; // XXX ???
    else {
        // pppoe_rfc_getboundaddr(pcb->rfc, addr.ac_name, addr.service);
        addr = (struct sockaddr_pppoe *)_MALLOC(sizeof (struct sockaddr_pppoe), M_SONAME, M_WAITOK);
        if (addr) {
            addr->pppoe_len = sizeof(struct sockaddr_pppoe);
            addr->pppoe_family = AF_PPPOE;
            addr->pppoe_ac_name[0] = 0;
            addr->pppoe_service[0] = 0;
            pppoe_rfc_getpeeraddr(pcb->rfc, addr->pppoe_mac_addr);
            // may be should we add the MAC address
            *nam = (struct sockaddr *)addr;
        }
    }

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to abort call
----------------------------------------------------------------------------- */
int pppoe_abort(struct socket *so)
{
    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;

    log(LOGVAL, "pppoe_abort, so = 0x%x\n", so);

    if (pppoe_rfc_abort(pcb->rfc))
        error = EINVAL; // XXX ???

    soisdisconnected(so); // let's say we are disconnected anyway...

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to handle ioctl
----------------------------------------------------------------------------- */
int pppoe_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p)
{
    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;

    log(LOGVAL, "pppoe_control : so = 0x%x, cmd = %d\n", so, cmd);

    switch (cmd) {
        case IOC_PPPOE_SETLOOPBACK:
            pppoe_rfc_command(pcb->rfc, PPPOE_CMD_SETLOOPBACK, data);
            //pcb->loopback = *(u_int32_t *)data ? 1 : 0;
            break;
        default:
            ;
    }

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to send data out
----------------------------------------------------------------------------- */
int pppoe_send(struct socket *so, int flags, struct mbuf *m,
               struct sockaddr *nam, struct mbuf *control, struct proc *p)
{

    int 		error = 0, s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)so->so_pcb;

    //log(LOGVAL, "pppoe_send, so = 0x%x\n", so);

    if (pppoe_rfc_output(pcb->rfc, m)) {
        error = ENOTCONN; // XXX ???
        // shall I free the buffer ???
    }

    splx(s);
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
clone the two data structures
NOTE : the socket and rfc fields is NOT copied
----------------------------------------------------------------------------- */
void pppoe_clone(struct pppoe_pcb *pcb1, struct pppoe_pcb *pcb2)
{
    struct socket 	*so2;
    void 		*rfc2;

    rfc2 = pcb2->rfc;
    so2 = pcb2->socket;
    bcopy(pcb1, pcb2, sizeof(struct pppoe_pcb));
    pcb2->rfc = rfc2;
    pcb2->socket = so2;
}

/* -----------------------------------------------------------------------------
called from pppoe_rfc when change state occurs
----------------------------------------------------------------------------- */
void pppoe_event(void *data, u_int32_t event, u_int32_t msg)
{
    int 		s = splnet();
    static int 		ignore = 0; // change it
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)data, *pcb2;
    struct socket 	*so2;

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
            so2 = sonewconn(pcb->socket, SS_ISCONFIRMING);	// create the accepting connection
            log(LOGVAL, "pppoe_event, so = 0x%x, so2 = 0x%x, evt = PPPOE_EVT_RINGING\n", pcb->socket, so2);
            pcb2 = (struct pppoe_pcb *)so2->so_pcb;		// and get its pcb
            pppoe_clone(pcb, pcb2);			// transfer all the PCB info to the new connection
            pppoe_rfc_clone(pcb->rfc, pcb2->rfc);	// transfer all the RFC info to the new connection, including ringing state
            ignore = 1;					// set a flag to ignore the abort resulting events
            pppoe_rfc_abort(pcb->rfc);			// abort the ring on the listening connection
            ignore = 0;					// reset the flag
            pppoe_rfc_listen(pcb->rfc);			// relisten again, because pppoe can handle multiple connections
            break;

        case PPPOE_EVT_CONNECTED:
            log(LOGVAL, "pppoe_event, so = 0x%x, evt = PPPOE_EVT_CONNECTED\n", pcb->socket);
            soisconnected(pcb->socket);
            break;
        case PPPOE_EVT_DISCONNECTED:
            if (ignore)	// this event is due to the abort, following the ring (may be need to find a better way)
                break;
            log(LOGVAL, "pppoe_event, so = 0x%x, evt = PPPOE_EVT_DISCONNECTED\n", pcb->socket);
            pcb->socket->so_error = msg;
            soisdisconnected(pcb->socket);
            break;
    }
    splx(s);
}

/* -----------------------------------------------------------------------------
called from pppoe_rfc when data are present
----------------------------------------------------------------------------- */
int pppoe_input(void *data, struct mbuf *m)
{
    int 		s = splnet();
    struct pppoe_pcb 	*pcb = (struct pppoe_pcb *)data;
    struct mbuf 	*m0;
    u_long 		len;

    len = 0;
    for (m0 = m; m0 != 0; m0 = m0->m_next)
        len += m0->m_len;

    //log(LOGVAL, "pppoe_input, so = 0x%x, len = %d\n", pcb->socket, len);

    if (sbspace(&pcb->socket->so_rcv) < len) {
        m_freem(m);
        splx(s);
        log(LOGVAL, "pppoe_input no space, so = 0x%x, len = %d\n", pcb->socket, len);
        return 0;
    }

    sbappend(&pcb->socket->so_rcv, m);
    sorwakeup(pcb->socket);
    splx(s);
    return 0;
}


