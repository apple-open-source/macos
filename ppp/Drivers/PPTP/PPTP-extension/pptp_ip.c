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
#include <kern/locks.h>

#include <net/if_types.h>
#include <net/dlil.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>


#include "../../../Family/ppp_domain.h"
#include "pptp_rfc.h"
#include "pptp_ip.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */

mbuf_t 	pptp_ip_input(mbuf_t , int len, int other);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

extern lck_mtx_t	*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
intialize pptp datalink attachment strutures
----------------------------------------------------------------------------- */
int pptp_ip_init()
{
	ip_gre_register_input((gre_input_func_t)pptp_ip_input);
	return 0;
}

/* -----------------------------------------------------------------------------
dispose pptp datalink structures
can't dispose if clients are still attached
----------------------------------------------------------------------------- */
int pptp_ip_dispose()
{
	ip_gre_register_input(NULL);
	return 0;
}

/* -----------------------------------------------------------------------------
callback from ip
----------------------------------------------------------------------------- */
mbuf_t pptp_ip_input(mbuf_t m, int len, int other)
{
    struct ip 		*ip, ip_data;
    u_int32_t 		from;
	int				success;

#if 0
    u_int8_t 		*d, i;
    d = mtod(m, u_int8_t *);
    for (i = 0; i < 64; i+=16) {
    IOLog("pptp_ip_input: data 0x %x %x %x %x %x %x %x %x - %x %x %x %x %x %x %x %x\n",
        d[i+0],d[i+1],d[i+2],d[i+3],d[i+4],d[i+5], d[i+6], d[i+7],
        d[i+8], d[i+9], d[i+10], d[i+11], d[i+12], d[i+13], d[i+14], d[i+15]);
    }
#endif

    ip = &ip_data; 
    memcpy(ip, mbuf_data(m), sizeof(ip_data));
    from = ip->ip_src.s_addr;

    /* remove the IP header */
    mbuf_adj(m, ip->ip_hl * 4);

	lck_mtx_lock(ppp_domain_mutex);
    success = pptp_rfc_lower_input(m, from);
	lck_mtx_unlock(ppp_domain_mutex);
	if (success)
        return NULL;
	
	return m;
}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int pptp_ip_output(mbuf_t m, u_int32_t from, u_int32_t to)
{
    struct ip 	*ip, ip_data;

#if 0
    u_int8_t 	*d, i;

    d = mtod(m, u_int8_t *);
    for (i = 0; i < 64; i+=16) {
    IOLog("pptp_ip_output: data 0x %x %x %x %x %x %x %x %x - %x %x %x %x %x %x %x %x\n",
        d[i+0],d[i+1],d[i+2],d[i+3],d[i+4],d[i+5], d[i+6], d[i+7],
        d[i+8], d[i+9], d[i+10], d[i+11], d[i+12], d[i+13], d[i+14], d[i+15]);
    }
#endif

    if (mbuf_prepend(&m, sizeof(struct ip), MBUF_WAITOK) != 0)
        return 1;
        
    ip = &ip_data; 
    memcpy(ip, mbuf_data(m), sizeof(ip_data));
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_p = IPPROTO_GRE;
    ip->ip_len = mbuf_pkthdr_len(m);
    ip->ip_src.s_addr = from;
    ip->ip_dst.s_addr = to;
    ip->ip_ttl = MAXTTL;
    memcpy(mbuf_data(m), ip, sizeof(ip_data));
 
	lck_mtx_unlock(ppp_domain_mutex);
    ip_gre_output((struct mbuf *)m);
	lck_mtx_lock(ppp_domain_mutex);

    return 0;
}

