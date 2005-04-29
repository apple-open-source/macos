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
#include <kern/locks.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/route.h>
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

void	l2tp_ip_input(mbuf_t , int len);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
extern lck_mtx_t	*ppp_domain_mutex;

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
void l2tp_udp_input(socket_t so, void *arg, int waitflag)
{
    mbuf_t mp = 0, m1 = 0;
	size_t recvlen = 1000000000;
    struct sockaddr from;
    struct msghdr msg;
		
    do {
    
		bzero(&from, sizeof(from));
		bzero(&msg, sizeof(msg));
		msg.msg_namelen = sizeof(from);
		msg.msg_name = &from;
	
		if (sock_receivembuf(so, &msg, &mp, MSG_DONTWAIT, &recvlen) != 0)
			break;

        if (mp == 0) 
            break;

        /* let's make life simpler for upper layers, and make everything contiguous
        could be more efficient dealing with the original mbuf, but ppp doesn't like much
        to mbuf chain, neither data compression nor IP layer
        IP layer seems to expext IP header in a contiguous block
        */
        
        /* ??? test if already in a contiguous block ??? */
        if (mbuf_getpacket(MBUF_WAITOK, &m1) == 0) {
                mbuf_copydata(mp, 0, mbuf_pkthdr_len(mp), mbuf_data(m1));
                mbuf_setlen(m1, mbuf_pkthdr_len(mp));
                mbuf_pkthdr_setlen(m1, mbuf_pkthdr_len(mp));
				lck_mtx_lock(ppp_domain_mutex);
                l2tp_rfc_lower_input(so, m1, &from);
				lck_mtx_unlock(ppp_domain_mutex);
        }
        
        mbuf_freem(mp);	
		
    } while (1);

}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int l2tp_udp_output(socket_t so, mbuf_t m, struct sockaddr* to)
{
	int result;
	
    if (so == 0 || to == 0) {
        mbuf_freem(m);	
        return EINVAL;
    }
	lck_mtx_unlock(ppp_domain_mutex);
    result = sock_sendmbuf(so, 0, m, MSG_DONTWAIT, 0);
	lck_mtx_lock(ppp_domain_mutex);
	
	return result;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_setpeer(socket_t so, struct sockaddr *addr)
{
	int result;
	
    if (so == 0)
        return EINVAL;
    
	lck_mtx_unlock(ppp_domain_mutex);
    result = sock_connect(so, addr, 0);
	lck_mtx_lock(ppp_domain_mutex);
	
	return result;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_attach(socket_t *socket, struct sockaddr *addr)
{
    int				val;
	errno_t			err;
    socket_t		so = 0;
	
	lck_mtx_unlock(ppp_domain_mutex);
    
    /* open a UDP socket for use by the L2TP client */
    if (err = sock_socket(AF_INET, SOCK_DGRAM, 0, l2tp_udp_input, 0, &so)) 
        goto fail;

    /* configure the socket to reuse port */
    val = 1;
    if (err = sock_setsockopt(so, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)))
        goto fail;
        
    if (err = sock_bind(so, addr))
        goto fail;

    /* fill in the incomplete part of the address assigned by UDP */ 
    if (err = sock_getsockname(so, addr, addr->sa_len))
        goto fail;
    
	lck_mtx_lock(ppp_domain_mutex);
    *socket = so;
    return 0;
    
fail:
    if (so) 
        sock_close(so);
	lck_mtx_lock(ppp_domain_mutex);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_udp_detach(socket_t so)
{
	lck_mtx_unlock(ppp_domain_mutex);
    if (so)
        sock_close(so); 		/* close the UDP socket */
	lck_mtx_lock(ppp_domain_mutex);
    return 0;
}

