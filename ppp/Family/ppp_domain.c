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

/* -----------------------------------------------------------------------------
*
*  Theory of operation :
*
*  this file implements the ppp domain, which is used to communicate
*  between pppd and the interface/link layer
*
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/sysctl.h>
#include <kern/locks.h>
#include <net/if.h>
#include <netinet/in.h>

#include "if_ppplink.h"		// public link API
#include "ppp_domain.h"
#include "ppp_defs.h"		// public ppp values
#include "if_ppp.h"		// public ppp API
#include "ppp_if.h"
#include "ppp_link.h"

/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

#define TYPE_IF		1
#define TYPE_LINK 	2

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

int ppp_proto_attach(struct socket *, int, struct proc *);
int ppp_proto_detach(struct socket *so);
int ppp_proto_connect(struct socket *, struct sockaddr *, struct proc *);
int ppp_proto_ioctl(struct socket *, u_long cmd, caddr_t , struct ifnet *, struct proc *);
int ppp_proto_send(struct socket *, int , struct mbuf * , struct sockaddr *, struct mbuf *, struct proc *);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

struct domain ppp_domain =
{	PF_PPP, PPP_NAME, NULL,
	NULL, NULL, NULL, NULL, NULL,
	0, 0, 0, 0
};

struct pr_usrreqs ppp_usr =
{       pru_abort_notsupp, pru_accept_notsupp, ppp_proto_attach, pru_bind_notsupp,
        ppp_proto_connect, pru_connect2_notsupp, ppp_proto_ioctl, ppp_proto_detach,
        pru_disconnect_notsupp, pru_listen_notsupp, pru_peeraddr_notsupp,
        pru_rcvd_notsupp, pru_rcvoob_notsupp, ppp_proto_send,
        pru_sense_null, pru_shutdown_notsupp, pru_sockaddr_notsupp,
        sosend, soreceive, pru_sopoll_notsupp
};

struct protosw ppp_proto =
{       SOCK_RAW, &ppp_domain, PPPPROTO_CTL, PR_ATOMIC|PR_CONNREQUIRED|PR_PROTOLOCK,
        NULL, NULL, NULL, NULL,
        NULL, NULL,
        NULL, NULL, NULL, NULL, &ppp_usr
};

u_char pppproto_inited = 0;

lck_mtx_t   *ppp_domain_mutex;

SYSCTL_NODE(_net, PF_PPP, ppp, CTLFLAG_RW, 0, "");

/* -----------------------------------------------------------------------------
Initialization function
----------------------------------------------------------------------------- */
int ppp_domain_init()
{
    
    // add domain cannot fail, just add the domain struct to the linked list
    net_add_domain(&ppp_domain);    
	ppp_domain_mutex = ppp_domain.dom_mtx;
	ppp_domain.dom_flags = DOM_REENTRANT;  // tell dlil not to take our lock

    sysctl_register_oid(&sysctl__net_ppp);
	        
    return 0;
}

/* -----------------------------------------------------------------------------
Initialization function
----------------------------------------------------------------------------- */
int ppp_proto_add()
{
    int ret;
        
    ret = net_add_proto(&ppp_proto, &ppp_domain);
    if (ret) {
        net_del_domain(&ppp_domain);
		LOGRETURN(ret, ret, "ppp_proto_add : can't add proto to PPP domain, error = 0x%x\n");
    }
		
    pppproto_inited = 1;
        
    return 0;
}

/* -----------------------------------------------------------------------------
Dispose function
----------------------------------------------------------------------------- */
int ppp_proto_remove()
{
    int ret;

    if (pppproto_inited) {
        ret = net_del_proto(ppp_proto.pr_type, ppp_proto.pr_protocol, ppp_proto.pr_domain);
        LOGRETURN(ret, ret, "ppp_domain_terminate : can't del proto from PPP domain, error = 0x%x\n");
		pppproto_inited = 0;
    }
        
    return 0;
}

/* -----------------------------------------------------------------------------
Dispose function
----------------------------------------------------------------------------- */
int ppp_domain_dispose()
{
    int ret;
    
    ret = net_del_domain(&ppp_domain);
	LOGRETURN(ret, ret, "ppp_domain_terminate : can't del PPP domain, error = 0x%x\n");
    
    sysctl_unregister_oid(&sysctl__net_ppp);

    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer when a new socket is created
----------------------------------------------------------------------------- */
int ppp_proto_attach (struct socket *so, int proto, struct proc *p)
{
    int	error = 0;
    
    //log(LOGVAL, "ppp_proto_attach, so = 0x%x\n", so);

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) 
        error = soreserve(so, 8192, 8192);
    
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer when the socket is closed
----------------------------------------------------------------------------- */
int ppp_proto_detach(struct socket *so)
{
    //log(LOGVAL, "ppp_proto_detach, so = 0x%x\n", so);
    
    ppp_proto_free(so);
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer to connect the protocol (PR_CONNREQUIRED)
----------------------------------------------------------------------------- */
int ppp_proto_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{
    soisconnected(so);
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer to handle ioctl
----------------------------------------------------------------------------- */
int ppp_proto_ioctl(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p)
{
    int 	error = 0;
    u_int16_t	unit;
    
    //log(LOGVAL, "ppp_proto_control : so = 0x%x, cmd = %d\n", so, cmd);

    switch (cmd) {
        case PPPIOCNEWUNIT:
            // this ioctl must be done before connecting the socket
            //log(LOGVAL, "ppp_proto_control : PPPIOCNEWUNIT\n");
            unit = *(u_int32_t *)data;
            error = ppp_if_attach(&unit);
            if (error)
                return error;
            *(u_int32_t *)data = unit;
            // no break; PPPIOCNEWUNIT implicitlty attach the client

        case PPPIOCATTACH:
            //log(LOGVAL, "ppp_proto_control : PPPIOCATTACH\n");
            unit = *(u_int32_t *)data;
            error = ppp_if_attachclient(unit, so, (ifnet_t *)&so->so_pcb);
            if (!error)
                so->so_tpcb = (caddr_t)TYPE_IF;
            break;

        case PPPIOCATTCHAN:
            //log(LOGVAL, "ppp_proto_control : PPPIOCATTCHAN\n");
            unit = *(u_int32_t *)data;
            error = ppp_link_attachclient(unit, so, (struct ppp_link **)&so->so_pcb);
            if (!error)
                so->so_tpcb = (caddr_t)TYPE_LINK;
            break;

        case PPPIOCDETACH:
            //log(LOGVAL, "ppp_proto_control : PPPIOCDETACH\n");
            ppp_proto_free(so);
            break;

        default:
            switch ((u_int32_t)so->so_tpcb) {
                case TYPE_IF:
                    error = ppp_if_control((ifnet_t)so->so_pcb, cmd, data);
                    break;
                case TYPE_LINK:
                    error = ppp_link_control((struct ppp_link *)so->so_pcb, cmd, data);
                    break;
                default:
                    error = EINVAL;
            }
    }
    
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to send data out
----------------------------------------------------------------------------- */
int ppp_proto_send(struct socket *so, int flags, struct mbuf *m,
               struct sockaddr *nam, struct mbuf *control, struct proc *p)
{    
    int error = 0;
    
    switch ((u_int32_t)so->so_tpcb) {
        case TYPE_IF:
            error = ppp_if_send((ifnet_t)so->so_pcb, (mbuf_t)m);
            break;
        case TYPE_LINK:
            error = ppp_link_send((struct ppp_link *)so->so_pcb, (mbuf_t)m);
            break;
        default:
            error = EINVAL;
			if (m)
				mbuf_freem(m);
    }
	if (control)
		mbuf_freem(control);
    return error;
}

/* -----------------------------------------------------------------------------
called from ppp_if or ppp_link when data struct disappears
----------------------------------------------------------------------------- */
void ppp_proto_free(void *data)
{
    struct socket *so = (struct socket *)data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    if (!so) 
        return;
    
    switch ((u_int32_t)so->so_tpcb) {
        case TYPE_IF:
            ppp_if_detachclient((ifnet_t)so->so_pcb, so);
            break;
        case TYPE_LINK:
            ppp_link_detachclient((struct ppp_link *)so->so_pcb, so);
            break;
    }
    so->so_pcb = 0;
    so->so_tpcb = 0;
	so->so_flags |= SOF_PCBCLEARING;
}

/* -----------------------------------------------------------------------------
called from ppp_link when data are present
----------------------------------------------------------------------------- */
int ppp_proto_input(void *data, mbuf_t m)
{
    struct socket *so = (struct socket *)data;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    //log(LOGVAL, "ppp_proto_input, so = 0x%x, len = %d\n", pcb->socket, m->m_pkthdr.len);
    
    // use this flag to be sure the app receive packets with End OF Record
    mbuf_setflags(m, mbuf_flags(m) | MBUF_EOR);

    // if there is no pppd attached yet, or if buffer is full, free the packet
    if (!so || sbspace(&so->so_rcv) < mbuf_pkthdr_len(m)) {
        if (so)
	  log(LOGVAL, "ppp_proto_input no space, so = 0x%x, len = %d\n", so, mbuf_pkthdr_len(m));
        mbuf_freem(m);
        return 0;
    }

//    log(LOGVAL, "----------- ppp_proto_input, link %d, packet %x %x %x %x %x %x %x %x \n", *(u_short *)p, p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);

    sbappendrecord(&so->so_rcv, (struct mbuf*)m);
    sorwakeup(so);
    return 0;
}


/* -----------------------------------------------------------------------------
queue utilities
----------------------------------------------------------------------------- */
int ppp_qfull(struct pppqueue *pppq)
{
	return pppq->len >= pppq->maxlen;
}

void ppp_drop(struct pppqueue *pppq)
{
	pppq->drops++;
}

void ppp_enqueue(struct pppqueue *pppq, mbuf_t m)
{
	mbuf_setnextpkt(m, 0);
	if (pppq->tail == 0)
		pppq->head = m;
	else 
		mbuf_setnextpkt(pppq->tail, m);
	pppq->tail = m;
	pppq->len++;
}

mbuf_t ppp_dequeue(struct pppqueue *pppq)
{
	mbuf_t m = pppq->head;
	if (m) {
		if ((pppq->head = mbuf_nextpkt(m)) == 0)
			pppq->tail = 0;
		mbuf_setnextpkt(m, 0);
		pppq->len--;
	}
	return m;
}

void ppp_prepend(struct pppqueue *pppq, mbuf_t m)
{
	mbuf_setnextpkt(m, pppq->head); \
	if (pppq->tail == 0)
		pppq->tail = m;
	pppq->head = m;
	pppq->len++;
}

