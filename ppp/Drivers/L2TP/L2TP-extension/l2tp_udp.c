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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/dlil.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>


#include "l2tpk.h"
#include "l2tp_rfc.h"
#include "l2tp_udp.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */
extern void    m_copydata __P((struct mbuf *, int, int, caddr_t));

void	l2tp_ip_input(struct mbuf *, int len);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
intialize L2TP/UDP layer
----------------------------------------------------------------------------- */
int l2tp_udp_init()
{
    return 0;
}

/* -----------------------------------------------------------------------------
dispose L2TP/UDP layer
----------------------------------------------------------------------------- */
int l2tp_udp_dispose()
{

    return 0;
}

/* -----------------------------------------------------------------------------
callback from udp
----------------------------------------------------------------------------- */
void l2tp_udp_input(struct socket *so, caddr_t  arg, int waitflag)
{
    int flags = MSG_DONTWAIT;
    struct mbuf *mp = 0, *m1 = 0;
    struct sockaddr *from;
    struct uio auio;
    
    auio.uio_resid = 1000000000;
    auio.uio_procp = 0;
    while (soreceive(so, &from, &auio, &mp, 0, &flags) == 0) {
    
        if (mp == 0) 
            break;

        if (from == 0) {
            m_freem(mp);	
            break;
        }
        
        /* let's make life simpler for upper layers, and make everything contiguous
        could be more efficient dealing with the original mbuf, but ppp doesn't like much
        to mbuf chain, neither data compression nor IP layer
        IP layer seems to expext IP header in a contiguous block
        */
        
        /* ??? test if already in a contiguous block ??? */
        m1 = m_getpacket();
        if (m1) {
                m_copydata(mp, 0, mp->m_pkthdr.len, mtod(m1, caddr_t));
                m1->m_len = mp->m_pkthdr.len;
                m1->m_pkthdr.len = mp->m_pkthdr.len;
                l2tp_rfc_lower_input(so, m1, from);
        }

        if (from)
            FREE(from, M_SONAME);
        
        m_freem(mp);	
    }

}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int l2tp_udp_output(struct socket *so, struct mbuf *m, struct sockaddr* to)
{

    if (so == 0 || to == 0) {
        m_freem(m);	
        return EINVAL;
    }

    return sosend(so, 0 /* to */, 0, m, 0, MSG_DONTWAIT);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_setpeer(struct socket *so, struct sockaddr *addr)
{
    if (so == 0)
        return EINVAL;
        
    return soconnect(so, addr);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_attach(struct socket **socket, struct sockaddr *addr)
{
    int 		err, val;
    struct sockaddr 	*sa;
    struct socket	*so = 0;
    struct sockopt	sopt;
    
    /* open a UDP socket for use by the L2TP client */
    if (err = socreate(AF_INET, &so, SOCK_DGRAM, 0)) 
        goto fail;

    so->so_upcall = l2tp_udp_input;
    so->so_upcallarg = 0;
    so->so_rcv.sb_flags |= SB_UPCALL;
    
    /* configure the socket to reuse port */
    bzero(&sopt, sizeof(sopt));
    sopt.sopt_dir = SOPT_SET;
    sopt.sopt_level = SOL_SOCKET;
    sopt.sopt_name = SO_REUSEPORT;
    val = 1;
    sopt.sopt_val = &val;
    sopt.sopt_valsize = sizeof(val);
    if (err = sosetopt(so, &sopt))
        goto fail;
        
    if (err = sobind(so, addr))
        goto fail;

    /* fill in the incomplete part of the address assigned by UDP */ 
    if (err = (so->so_proto->pr_usrreqs->pru_sockaddr)(so, &sa))
        goto fail;

    if (addr->sa_len == sa->sa_len)
        bcopy(sa, addr, sa->sa_len);
    FREE(sa, M_SONAME);
    
    *socket = so;
    return 0;
    
fail:
    if (so) 
        soclose(so);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_detach(struct socket *so)
{
    if (so)
        soclose(so); 		/* close the UDP socket */
    return 0;
}

