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
/*
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ndrv.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_ifattach.h>

#include <sys/socketvar.h>

#include <net/dlil.h>
#include <net/kpi_protocol.h>

#include "firewire.h"
#include "if_firewire.h"

extern void ip6_input(struct mbuf *);

/*
 * Process a received firewire packet;
 * the packet is in the mbuf chain m without
 * the firewire header, which is provided separately.
 */
int
inet6_firewire_input(
			ifnet_t				ifp,
			protocol_family_t	protocol_fmaily, 
			mbuf_t				m, 
			char				*frame_header)
{
    register struct firewire_header *eh = (struct firewire_header *) frame_header;

    if ((ifnet_flags(ifp) & IFF_UP) == 0) 
	{
		mbuf_freem(m);
		return EJUSTRETURN;
    }

	ifnet_touch_lastchange(ifp);

    if (eh->fw_dhost[0] & 1) 
	{
		int flags = (bcmp((caddr_t)fwbroadcastaddr, (caddr_t)eh->fw_dhost, sizeof(fwbroadcastaddr)) == 0) 
					? MBUF_BCAST : MBUF_MCAST;
			
		mbuf_setflags(m, mbuf_flags(m) | flags);	
    }
	
	errno_t ret = proto_input(PF_INET6, (struct __mbuf*)m);
	
	if( ret )
		mbuf_freem(m);
	
	return ret;
}




int
inet6_firewire_pre_output(
	ifnet_t				ifp,
	protocol_family_t	protocol_family,
	mbuf_t				*m0,
	const struct sockaddr	*dst_netaddr,
	void				*route,
	char				*type,
	char				*edst)
{
	errno_t	result;
	struct	sockaddr_dl	sdl;
	
    mbuf_setflags(*m0, mbuf_flags(*m0) | MBUF_LOOP);
	
	result = nd6_lookup_ipv6(ifp, (const struct sockaddr_in6*)dst_netaddr,
							 &sdl, sizeof(sdl), (route_t)route, *m0);
	
	if (result == 0) 
	{
		*(u_int16_t*)type = htons(FWTYPE_IPV6);
		bcopy(LLADDR(&sdl), edst, sdl.sdl_alen);
	}

    return result;
}

static int
firewire_inet6_resolve_multi(
	ifnet_t	ifp,
	const struct sockaddr *proto_addr,
	struct sockaddr_dl *out_ll,
	size_t	ll_len)
{
	static const size_t minsize = offsetof(struct sockaddr_dl, sdl_data[0]) + FIREWIRE_ADDR_LEN;
	const struct sockaddr_in6	*sin6 = (const struct sockaddr_in6*)proto_addr;
	
	if (proto_addr->sa_family != AF_INET6)
		return EAFNOSUPPORT;
	
	if (proto_addr->sa_len < sizeof(struct sockaddr_in6))
		return EINVAL;
	
	if (ll_len < minsize)
		return EMSGSIZE;
	
	bzero(out_ll, minsize);
	out_ll->sdl_len = minsize;
	out_ll->sdl_family = AF_LINK;
	out_ll->sdl_index = ifnet_index(ifp);
	out_ll->sdl_type = IFT_IEEE1394;
	out_ll->sdl_nlen = 0;
	out_ll->sdl_alen = FIREWIRE_ADDR_LEN;
	out_ll->sdl_slen = 0;
 	
	FIREWIRE_MAP_IPV6_MULTICAST(&sin6->sin6_addr, LLADDR(out_ll));
	
	return 0;
}

errno_t
firewire_inet6_prmod_ioctl(
						__unused ifnet_t			ifp,
						__unused protocol_family_t	protocol_family,
						__unused unsigned long		command,
						__unused void				*data)
{
	return EOPNOTSUPP;
}

int
firewire_attach_inet6(
	ifnet_t ifp,
	__unused protocol_family_t	protocol_family)
{
	struct ifnet_attach_proto_param	proto;
	struct ifnet_demux_desc demux[1];
	u_short en_6native=htons(FWTYPE_IPV6);
	errno_t	error;
	
	bzero(&proto, sizeof(proto));
	demux[0].type	= DLIL_DESC_ETYPE2;
	demux[0].data	= &en_6native;
	demux[0].datalen = sizeof(en_6native);
	proto.demux_list = demux;
	proto.demux_count = 1;
	proto.input			= inet6_firewire_input;
	proto.pre_output	= inet6_firewire_pre_output;
	proto.ioctl			= firewire_inet6_prmod_ioctl;
	proto.resolve		= firewire_inet6_resolve_multi;
	
	error = ifnet_attach_protocol(ifp, protocol_family, &proto);
	
	if (error && error != EEXIST) 
	{
		printf("WARNING: firewire_attach_inet6 can't attach ipv6 to %s%d\n",
												ifnet_name(ifp), ifnet_unit(ifp));
	}
	
	return error;
}