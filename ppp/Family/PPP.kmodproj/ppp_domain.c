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

/* -----------------------------------------------------------------------------
*
* HISTORY
*
*  May 2000 Christophe Allie - created.
*
*  Theory of operation :
*
*  this file implements ppp domain, which is used to communicate with PPPController
*
----------------------------------------------------------------------------- */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <mach/vm_types.h>
#include <mach/kmod.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <machine/spl.h>

#include <net/if_var.h>

#include "ppp.h"
#include "ppp_fam.h"
#include "ppp_domain.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */



/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

void ppp_proto_init();
int ppp_proto_ctloutput(struct socket *so, struct sockopt *sopt);
int ppp_proto_usrreq();

int ppp_proto_attach(struct socket *, int, struct proc *);
int ppp_proto_detach(struct socket *);
int ppp_proto_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p);
int ppp_proto_send(struct socket *so, int flags, struct mbuf *m,
               struct sockaddr *nam, struct mbuf *control, struct proc *p);
int ppp_proto_connect(struct socket *so, struct sockaddr *nam, struct proc *p);

int ppp_proto_add(struct domain *domain);
int ppp_proto_remove(struct domain *domain);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

char 			*ppp_domain_name = PPP_NAME;
struct domain 		ppp_domain;

struct pr_usrreqs 	ppp_proto_usr;	/* pr_usrreqs extension to the protosw */
struct protosw 		ppp;		/* describe the protocol switch */



/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_domain_init()
{
    int ret;

//    log(LOGVAL, "ppp_domain_init\n");

    bzero(&ppp_domain, sizeof(struct domain));
    ppp_domain.dom_family = PF_PPP;
    ppp_domain.dom_name = ppp_domain_name;

    // add domain cannot fail, just add the domain struct to the linked list
    net_add_domain(&ppp_domain);

    ret = ppp_proto_add(&ppp_domain);
    if (ret) {
        log(LOGVAL, "ppp_domain_init : can't add proto to PPP domain, err : %d\n", ret);
        net_del_domain(&ppp_domain);
        return ret;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_domain_dispose()
{
    int ret;

//    log(LOGVAL, "ppp_domain_terminate\n");

    ret = ppp_proto_remove(&ppp_domain);
    LOGRETURN(ret, ret, "ppp_domain_terminate : can't del proto from PPP domain, error = 0x%x\n");

    ret = net_del_domain(&ppp_domain);
    LOGRETURN(ret, ret, "ppp_domain_terminate : can't del PPP domain, error = 0x%x\n");

    return 0;
}


/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------- Admistrative functions, called by ppp_domain -----------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Called when we need to add the PPPoE protocol to the domain
Typically, ppp_proto_add is called by ppp_domain when we add the domain,
but we can add the protocol anytime later, if the domain is present
----------------------------------------------------------------------------- */
int ppp_proto_add(struct domain *domain)
{
    int 	ret = 0;

    bzero(&ppp_proto_usr, sizeof(struct pr_usrreqs));
    ppp_proto_usr.pru_abort 		= pru_abort_notsupp;
    ppp_proto_usr.pru_accept 		= pru_accept_notsupp;
    ppp_proto_usr.pru_attach 		= ppp_proto_attach;
    ppp_proto_usr.pru_bind 		= pru_bind_notsupp;
    ppp_proto_usr.pru_connect 		= ppp_proto_connect;
    ppp_proto_usr.pru_connect2 		= pru_connect2_notsupp;
    ppp_proto_usr.pru_control 		= ppp_proto_control;
    ppp_proto_usr.pru_detach 		= ppp_proto_detach;
    ppp_proto_usr.pru_disconnect 	= pru_disconnect_notsupp;
    ppp_proto_usr.pru_listen 		= pru_listen_notsupp;
    ppp_proto_usr.pru_peeraddr 		= pru_peeraddr_notsupp;
    ppp_proto_usr.pru_rcvd 		= pru_rcvd_notsupp;
    ppp_proto_usr.pru_rcvoob 		= pru_rcvoob_notsupp;
    ppp_proto_usr.pru_send 		= ppp_proto_send;
    ppp_proto_usr.pru_sense 		= pru_sense_null;
    ppp_proto_usr.pru_shutdown 		= pru_shutdown_notsupp;
    ppp_proto_usr.pru_sockaddr 		= pru_sockaddr_notsupp;
    ppp_proto_usr.pru_sosend 		= sosend;
    ppp_proto_usr.pru_soreceive 	= soreceive;
    ppp_proto_usr.pru_sopoll 		= sopoll;


    bzero(&ppp, sizeof(struct protosw));
    ppp.pr_type	= SOCK_DGRAM;
    ppp.pr_domain 	= domain;
    ppp.pr_protocol 	= PPPPROTO_CTL;
    ppp.pr_flags 	= PR_ATOMIC|PR_CONNREQUIRED;
    ppp.pr_ctloutput 	= ppp_proto_ctloutput;
    ppp.pr_init 	= ppp_proto_init;
    ppp.pr_usrreqs 	= &ppp_proto_usr;

    ret = net_add_proto(&ppp, domain);
        
    return ret;
}

/* -----------------------------------------------------------------------------
Called when we need to remove the PPPoE protocol from the domain
----------------------------------------------------------------------------- */
int ppp_proto_remove(struct domain *domain)
{
    int ret = 0;

    ret = net_del_proto(ppp.pr_type, ppp.pr_protocol , domain);
    if (ret)
        return ret;

    return 0;
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
void ppp_proto_init()
{
    //log(LOGVAL, "ppp_proto_init\n");
}

/* -----------------------------------------------------------------------------
This function is called by socket layer to handle get/set-socketoption
----------------------------------------------------------------------------- */
int ppp_proto_ctloutput(struct socket *so, struct sockopt *sopt)
{
    int	error, optval;

   // log(LOGVAL, "ppp_proto_ctloutput, so = 0x%x\n", so);

    error = optval = 0;
    if (sopt->sopt_level != PPPPROTO_CTL) {
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
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- pr_usrreqs functions ---------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Called by socket layer when a new socket is created
Should create all the structures and prepare for ppp dialog
----------------------------------------------------------------------------- */
int ppp_proto_attach (struct socket *so, int proto, struct proc *p)
{
    int		error;

    //log(LOGVAL, "ppp_proto_attach, so = 0x%x\n", so);
    if (so->so_pcb)
        return EINVAL;

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        error = soreserve(so, 8192, 8192);
        if (error)
            return error;
    }

    so->so_pcb = (caddr_t)1;

    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer when the socket is closed
Should free all the ppp structures
----------------------------------------------------------------------------- */
int ppp_proto_detach(struct socket *so)
{

    //log(LOGVAL, "ppp_proto_detach, so = 0x%x\n", so);

    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer to connect the protocol (PR_CONNREQUIRED)
any address will fit...
----------------------------------------------------------------------------- */
int ppp_proto_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{

    //log(LOGVAL, "ppp_proto_connect, so = 0x%x\n", so);

    soisconnected(so);
    return 0;
}

/* -----------------------------------------------------------------------------
Called by socket layer to handle ioctl
----------------------------------------------------------------------------- */
int ppp_proto_control(struct socket *so, u_long cmd, caddr_t data,
                  struct ifnet *ifp, struct proc *p)
{
    int 			error = 0, s = splnet();
    u_short 			subfam, unit;

    //log(LOGVAL, "ppp_proto_control : so = 0x%x, cmd = %d\n", so, cmd);

    subfam = (*(u_long*)data) >> 16;
    unit = (*(u_long*)data) & 0xFFFF;
    
    // note: ifp is NULL (see kern/sys_socket.c soo_ioctl())
    switch (cmd) {
        case IOC_PPP_ATTACHMUX:
            if (ppp_fam_sockregister(subfam, unit, (void *)so)) {
                error = EINVAL;		/* XXX ??? */
            }
            break;
        case IOC_PPP_DETACHMUX:
            ppp_fam_sockderegister(subfam, unit);
            break;
    }

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
Called by socket layer to send data out
----------------------------------------------------------------------------- */
int ppp_proto_send(struct socket *so, int flags, struct mbuf *m,
               struct sockaddr *nam, struct mbuf *control, struct proc *p)
{
    int 			error = 0, s = splnet();
    u_char			*p1;
    u_short			subfam, unit;
    
    //log(LOGVAL, "ppp_proto_proto_send, so = 0x%x\n", so);

    p1 = mtod(m, u_char *);
    subfam = *(u_short *)p1;
    unit = *(u_short *)(p1 + 2);

    m_adj(m, 4);

    error = ppp_fam_sockoutput(subfam, unit, m);

    if (error) {
        m_free(m);
        error = 0;
    }

    splx(s);
    return error;
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
------------------------- callbacks from ppp family  ---------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
called from ppp_fam when data are present
----------------------------------------------------------------------------- */
int ppp_proto_input(u_short subfam, u_short unit, void *data, struct mbuf *m)
{
    int 		s = splnet();
    struct socket 	*so = (struct socket *)data;
    struct mbuf 	*m0;
    u_long 		len;
    u_char		*p;

    //log(LOGVAL, "ppp_proto_input\n");

    M_PREPEND (m, 4, M_DONTWAIT);
    if (m == 0) 
        return 1;	// just return, because the buffer was freed in m_prepend

    p = mtod(m, u_char *);
    *(u_short *)p = subfam;
    *(u_short *)(p + 2) = unit;
    
    len = 0;
    for (m0 = m; m0 != 0; m0 = m0->m_next)
        len += m0->m_len;

    // use this flag to be sure the app receive packets with End OF Record
    m->m_flags |= M_EOR;

    //log(LOGVAL, "ppp_proto_input, so = 0x%x, len = %d\n", pcb->socket, len);

    if (sbspace(&so->so_rcv) < len) {
        m_free(m);
        splx(s);
        log(LOGVAL, "ppp_proto_input no space, so = 0x%x, len = %d\n", so, len);
        return 0;
    }

//    log(LOG_INFO, "----------- ppp_proto_input, link %d, packet %x %x %x %x %x %x %x %x \n", *(u_short *)p, p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);

    sbappendrecord(&so->so_rcv, m);
    sorwakeup(so);
    splx(s);
    return 0;
}
