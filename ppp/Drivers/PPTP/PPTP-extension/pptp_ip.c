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
#include <netinet/ip_var.h>
#include <netinet/ip.h>


#include "pptp_rfc.h"
#include "pptp_ip.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */
extern void    m_copydata __P((struct mbuf *, int, int, caddr_t));

void	pptp_ip_input(struct mbuf *, int len);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

struct protosw 	gre_pr, *old_pr;

/* -----------------------------------------------------------------------------
intialize pptp datalink attachment strutures
----------------------------------------------------------------------------- */
int pptp_ip_init()
{

    bzero(&gre_pr, sizeof(gre_pr));
    gre_pr.pr_input = pptp_ip_input;
    old_pr = ip_protox[IPPROTO_GRE];
    ip_protox[IPPROTO_GRE] = &gre_pr;
    return 0;
}

/* -----------------------------------------------------------------------------
dispose pptp datalink structures
can't dispose if clients are still attached
----------------------------------------------------------------------------- */
int pptp_ip_dispose()
{
    ip_protox[IPPROTO_GRE] = old_pr;
    return 0;
}

/* -----------------------------------------------------------------------------
callback from ip
----------------------------------------------------------------------------- */
void pptp_ip_input(struct mbuf *m, int len)
{
    struct ip 		*ip;
    u_int32_t 		from;
    struct mbuf 	*m1;

#if 0
    u_int8_t 		*d, i;
    d = mtod(m, u_int8_t *);
    for (i = 0; i < 64; i+=16) {
    log(LOG_INFO, "pptp_ip_input: data 0x %x %x %x %x %x %x %x %x - %x %x %x %x %x %x %x %x\n",
        d[i+0],d[i+1],d[i+2],d[i+3],d[i+4],d[i+5], d[i+6], d[i+7],
        d[i+8], d[i+9], d[i+10], d[i+11], d[i+12], d[i+13], d[i+14], d[i+15]);
    }
#endif

    /* 
    let's make life simpler for upper layers, and make everything contiguous
    could be more efficient dealing with the original mbuf, but ppp doesn't like much
    to mbuf chain, neither data compression nor IP layer
    IP layer seems to expext IP header in a contiguous block
    */
    m1 = m_getpacket();
    if (m1 == 0)
        goto fail;    
    m_copydata(m, 0, m->m_pkthdr.len, mtod(m1, caddr_t));
    m1->m_len = m->m_pkthdr.len;
    m1->m_pkthdr.len = m->m_pkthdr.len;
    
    ip = mtod(m1, struct ip *);
    from = ip->ip_src.s_addr;

    /* remove the IP header */
    m_adj(m1, ip->ip_hl * 4);

    if (pptp_rfc_lower_input(m1, from)) {
        // success, free the original mbuf
        m_freem(m);
        return;
    }

fail:
    // the packet was not for us, just call the old hook
    if (m1)
	m_freem(m1);
    (*old_pr).pr_input(m, len);
}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int pptp_ip_output(struct mbuf *m, u_int32_t from, u_int32_t to)
{
    struct route ro;
    struct ip 	*ip;

#if 0
    u_int8_t 	*d, i;

    d = mtod(m, u_int8_t *);
    for (i = 0; i < 64; i+=16) {
    log(LOG_INFO, "pptp_ip_output: data 0x %x %x %x %x %x %x %x %x - %x %x %x %x %x %x %x %x\n",
        d[i+0],d[i+1],d[i+2],d[i+3],d[i+4],d[i+5], d[i+6], d[i+7],
        d[i+8], d[i+9], d[i+10], d[i+11], d[i+12], d[i+13], d[i+14], d[i+15]);
    }
#endif

    M_PREPEND(m, sizeof(struct ip), M_WAIT);
    if (m == 0)
        return 1;
        
    ip = mtod(m, struct ip *);
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_p = IPPROTO_GRE;
    ip->ip_len = m->m_pkthdr.len;
    ip->ip_src.s_addr = from;
    ip->ip_dst.s_addr = to;
    ip->ip_ttl = MAXTTL;
 
    bzero(&ro, sizeof(ro));
    ip_output(m, 0, &ro, 0, 0);
    return 0;
}

