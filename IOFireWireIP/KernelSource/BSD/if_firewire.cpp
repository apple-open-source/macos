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
 
#ifndef INET
#define INET 1
#endif

extern "C"{
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/dlil.h>
#include <net/if_llc.h>
#if BRIDGE
#include <net/ethernet.h>
#include <net/bridge.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
}

extern "C" {
#include "firewire.h"
#include "if_firewire.h"
extern void _logMbuf(struct mbuf * m);
}
//#include "ip_firewire.h"
#include "IOFireWireIP.h"


#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)


// SYSCTL_DECL(_net_link_firewire);
// SYSCTL_NODE(_net_link_firewire, PF_INET, inet, CTLFLAG_RW, 0, "");

// Timer values

// walk list every 5 minutes
static int arpt_prune = (5*60*1);

// once resolved, good for 20 more minutes  
static int arpt_keep = (20*60); 

// once declared down, don't send for 20 sec 
static int arpt_down = 20;	

// Apple Hardware SUM16 checksuming
//static int apple_hwcksum_tx = 1;
//static int apple_hwcksum_rx = 1;

//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, prune_intvl, CTLFLAG_RW, &arpt_prune, 0, "");
//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, max_age, CTLFLAG_RW, &arpt_keep, 0, "");
//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, host_down_time, CTLFLAG_RW,&arpt_down, 0, "");
//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, apple_hwcksum_tx, CTLFLAG_RW, &apple_hwcksum_tx, 0, "");
//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, apple_hwcksum_rx, CTLFLAG_RW, &apple_hwcksum_rx, 0, "");

#define	rt_expire rt_rmx.rmx_expire

struct llinfo_arp {
	LIST_ENTRY(llinfo_arp) la_le;
	struct	rtentry *la_rt;
	struct	mbuf *la_hold;		/* last packet until resolved/timeout */
	long	la_asked;		/* last time we QUERIED for this addr */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};

static	LIST_HEAD(, llinfo_arp) llinfo_arp;

static int	arp_inuse, arp_allocated;

static int	arp_maxtries = 5;
static int	useloopback = 1; /* use loopback interface for local traffic */
//static int	arp_proxyall = 0;
//static int	arp_init_called = 0;

//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, maxtries, CTLFLAG_RW,
//	   &arp_maxtries, 0, "");
//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, useloopback, CTLFLAG_RW,
//	   &useloopback, 0, "");
//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, proxyall, CTLFLAG_RW,
//	   &arp_proxyall, 0, "");

void firewire_arp_rtrequest __P((int, struct rtentry *, struct sockaddr *));
static void	firewire_arprequest __P((struct arpcom *, struct in_addr *, struct in_addr *, u_char *));
void firewire_arpintr __P((struct mbuf *));
static void	firewire_arptfree __P((struct llinfo_arp *));
static void	firewire_arptimer __P((void *));

u_char *firewire_sprintf(register u_char *p, register u_char *ap);

static u_char fwbroadcastaddr[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static struct llinfo_arp *firewire_arplookup __P((u_long, int, int));
#if INET
static void	in_firewirearpinput __P((struct mbuf *));
#endif

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arptimer
//
// Timeout routine.  Age arp_tab entries periodically.
//
////////////////////////////////////////////////////////////////////////////////
static void
firewire_arptimer(void *ignored_arg)
{
    int s;
    
#ifdef __APPLE__
	// boolean_t 	funnel_state = 
	thread_funnel_set(network_flock, TRUE);
#endif
	s = splnet();
	register struct llinfo_arp *la = llinfo_arp.lh_first;
	struct llinfo_arp *ola;

	timeout(firewire_arptimer, (caddr_t)0, arpt_prune * hz);
	while ((ola = la) != 0) {
		register struct rtentry *rt = la->la_rt;
		la = la->la_le.le_next;
		if (rt->rt_expire && rt->rt_expire <= (u_long)time_second)
			firewire_arptfree(ola); /* timer has expired, clear */
	}
	splx(s);
     
#ifdef __APPLE__
	(void) thread_funnel_set(network_flock, FALSE);
#endif
}


////////////////////////////////////////////////////////////////////////////////
//
// firewire_arp_rtrequest
//
// Invoked by:
// from route.c for adding, resolving and deleting address
//
////////////////////////////////////////////////////////////////////////////////
void
firewire_arp_rtrequest(int req, register struct rtentry *rt, struct sockaddr *sa)
{
	register struct sockaddr *gate = rt->rt_gateway;
	register struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
	static int arpinit_done;
	int ret = 0;
    		
	//log(LOG_DEBUG, "firewire_arp_rtrequest: begin \n");

	if (!arpinit_done) {
		arpinit_done = 1;
		LIST_INIT(&llinfo_arp);
		timeout(firewire_arptimer, (caddr_t)0, hz);
#ifndef __APPLE__
		// register_netisr(NETISR_ARP, arpintr);
#endif
	}
    
	if (rt->rt_flags & RTF_GATEWAY){
		return;
	}
        
	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 && SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
            
		if (rt->rt_flags & RTF_CLONING) 
		{
			/*
			 * Case 1: This route should come from a route to iface.
			 */
			ret = rt_setgate(rt, rt_key(rt), (struct sockaddr *)&null_sdl);
			if(ret != 0)
			{
				log(LOG_DEBUG, " firewire_arp_rtrequest: RTM_ADD, return %x \n", ret);
			}
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			rt->rt_expire = time_second;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			firewire_arprequest((struct arpcom *)rt->rt_ifp,
                                            &SIN(rt_key(rt))->sin_addr,
                                            &SIN(rt_key(rt))->sin_addr,
                                            (u_char *)LLADDR(SDL(gate)));
		/* FALLTHROUGH */
	case RTM_RESOLVE:
	
		if (gate->sa_family != AF_LINK || gate->sa_len < sizeof(null_sdl)) 
		{
			log(LOG_DEBUG, " firewire_arp_rtrequest: bad gateway value\n");
			break;
		}
		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;
		if (la != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) 
		{
			log(LOG_DEBUG, " firewire_arp_rtrequest: malloc failed\n");
			break;
		}
		arp_inuse++, arp_allocated++;
		Bzero(la, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		LIST_INSERT_HEAD(&llinfo_arp, la, la_le);

#if INET
		/*
		 * This keeps the multicast addresses from showing up
		 * in `arp -a' listings as unresolved.  It's not actually
		 * functional.  Then the same for broadcast.
		 */
		if (IN_MULTICAST(ntohl(SIN(rt_key(rt))->sin_addr.s_addr))) 
		{
			FIREWIRE_MAP_IP_MULTICAST(&SIN( rt_key(rt))->sin_addr,
											LLADDR(SDL(gate)));
			SDL(gate)->sdl_alen = 8;
			rt->rt_expire = 0;
		}
		if (in_broadcast(SIN(rt_key(rt))->sin_addr, rt->rt_ifp)) 
		{
            memcpy(LLADDR(SDL(gate)), fwbroadcastaddr, 8);
			SDL(gate)->sdl_alen = 8;
			rt->rt_expire = time_second;
		}
#endif
		if (SIN(rt_key(rt))->sin_addr.s_addr == (IA_SIN(rt->rt_ifa))->sin_addr.s_addr) 
		{
		    /*
		     * This test used to be
		     *	if (loif.if_flags & IFF_UP)
		     * It allowed local traffic to be forced
		     * through the hardware by configuring the loopback down.
		     * However, it causes problems during network configuration
		     * for boards that can't receive packets they send.
		     * It is now necessary to clear "useloopback" and remove
		     * the route to force traffic out to the hardware.
		     */
			rt->rt_expire = 0;
			Bcopy(((struct arpcom *)rt->rt_ifp)->ac_enaddr,
				LLADDR(SDL(gate)), SDL(gate)->sdl_alen = 8);
			if (useloopback)
				rt->rt_ifp = loif;
		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		arp_inuse--;
		LIST_REMOVE(la, la_le);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold)
			m_freem(la->la_hold);
		Free((caddr_t)la);
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arpwhohas
//
// Invoked by:
// firewire_inet_prmod_ioctl from firewire_inet_pr_module.cpp
//
// Broadcast an ARP packet, asking who has addr on interface ac
//
////////////////////////////////////////////////////////////////////////////////
void
firewire_arpwhohas(struct arpcom *ac, struct in_addr *addr)
{
	struct ifnet *ifp = (struct ifnet *)ac;
	struct ifaddr *ifa = TAILQ_FIRST(&ifp->if_addrhead);

	while (ifa) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			firewire_arprequest(ac, &SIN(ifa->ifa_addr)->sin_addr, addr, ac->ac_enaddr);
			return;
		}
		ifa = TAILQ_NEXT(ifa, ifa_link);
	}
	return;	
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arprequest
//
// Invoked by : 
//   local to the file
//
// Broadcast an ARP request. Caller specifies:
//     - arp header source ip address
//     - arp header target ip address
//     - arp header source ethernet address
//
//
////////////////////////////////////////////////////////////////////////////////
static void
firewire_arprequest(register struct arpcom *ac,	register struct in_addr *sip, 
                    register struct in_addr *tip, register u_char *enaddr)
{
    register struct mbuf *m;
    register struct firewire_header *fwh;
	register IP1394_ARP *fwa;
    struct sockaddr sa;
    struct ifnet *ifp = (struct ifnet *)ac;
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
    LCB	*lcb;
    
	if(fwIpObj == NULL)
	{
		log(LOG_DEBUG, "if_softc is NULL \n");
		return;
	}
	lcb = fwIpObj->getLcb();
	//log(LOG_DEBUG, "firewire_arprequest \n");

	if((m = fwIpObj->getMBuf(sizeof(IP1394_ARP))) == NULL)
		return;
	
	m->m_pkthdr.rcvif = (struct ifnet *)0;
    
	fwa = mtod(m, IP1394_ARP*);
    bzero((caddr_t)fwa, sizeof(*fwa));
    m->m_len = sizeof(*fwa);
    m->m_pkthdr.len = sizeof(*fwa);

    fwa->hardwareType = htons(ARP_HDW_TYPE);
    fwa->protocolType = htons(ETHERTYPE_IP);
    fwa->hwAddrLen = sizeof(IP1394_HDW_ADDR);
    fwa->ipAddrLen = IPV4_ADDR_SIZE;
    fwa->opcode = htons(ARPOP_REQUEST);
    fwa->senderUniqueID.hi = htonl(lcb->ownHardwareAddress.eui64.hi);
    fwa->senderUniqueID.lo = htonl(lcb->ownHardwareAddress.eui64.lo);
    fwa->senderMaxRec = lcb->ownHardwareAddress.maxRec;
    fwa->sspd = lcb->ownHardwareAddress.spd;
    fwa->senderUnicastFifoHi = htons(lcb->ownHardwareAddress.unicastFifoHi);
    fwa->senderUnicastFifoLo = htonl(lcb->ownHardwareAddress.unicastFifoLo);
    fwa->senderIpAddress = sip->s_addr;   // Already in network order
    fwa->targetIpAddress = tip->s_addr;

    // Fill in the sock_addr structure
    fwh = (struct firewire_header *)sa.sa_data;
    fwh->ether_type = htons(ETHERTYPE_ARP);
    
    // To avoid loopback in resolving the address and indicates the header is filled
    sa.sa_family = AF_UNSPEC;
    sa.sa_len = sizeof(sa);

    dlil_output(((struct ifnet *)ac)->if_data.default_proto, m, 0, &sa, 0);
}


////////////////////////////////////////////////////////////////////////////////
//
// firewire_arpresolve
//
// IN:	struct arpcom	*ac
// IN:	struct rtentry	*rt
// IN:	struct mbuf	*m
// IN:	struct sockaddr	*dst
// OUT:	u_char		*desten
// IN:	struct rtentry	*rt0
//
// Invoked by:
// dlil.c for
// dlil_output=>(*proto)->dl_pre_output=>inet_firewire_pre_output=>
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_arpresolve(register struct arpcom *ac, register struct rtentry *rt,struct mbuf *m,
                    register struct sockaddr *dst,register u_char *desten,struct rtentry *rt0)
{
	struct	llinfo_arp *la = 0;
	struct	sockaddr_dl *sdl;
    
	// log(LOG_DEBUG, "firewire_arpresolve: called \n");

    /* Check for broadcast first */
	if (m->m_flags & M_BCAST) 
	{	/* broadcast */
		// log(LOG_DEBUG, "firewire_arpresolve: %d \n", __LINE__);
		(void)memcpy(desten, fwbroadcastaddr, FIREWIRE_ADDR_LEN);
		return (1);
	}
        
    /* Multicast very similar to broadcast */
	if (m->m_flags & M_MCAST) 
	{	
		// log(LOG_DEBUG, "firewire_arpresolve: %d \n", __LINE__);
  		FIREWIRE_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
		return(1);
	}

	if (rt)
		la = (struct llinfo_arp *)rt->rt_llinfo;
	if (la == 0) 
	{
		la = firewire_arplookup(SIN(dst)->sin_addr.s_addr, 1, 0);
		if (la)
			rt = la->la_rt;
	}
        
	if (la == 0 || rt == 0) 
	{
		log(LOG_DEBUG, "firewire_arpresolve: can't allocate llinfo for %s%s%s\n",
				inet_ntoa(SIN(dst)->sin_addr), la ? "la" : "",
				rt ? "rt" : "");
		m_freem(m);
		return (0);
	}
        
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > (u_long)time_second) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) 
	{
		// log(LOG_DEBUG, "firewire_arpresolve: %d \n", __LINE__);
   		bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
		return 1;
	}
	
	/*
	 * If ARP is disabled on this interface, stop.
	 * XXX
	 * Probably should not allocate empty llinfo struct if we are
	 * not going to be sending out an arp request.
	 */
	if (ac->ac_if.if_flags & IFF_NOARP)
		return (0);
		
	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	if (la->la_hold)
		m_freem(la->la_hold);
	la->la_hold = m;
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != (u_long)time_second) 
		{
			rt->rt_expire = time_second;
			if (la->la_asked++ < arp_maxtries)
			    firewire_arprequest(ac,
									&SIN(rt->rt_ifa->ifa_addr)->sin_addr,
									&SIN(dst)->sin_addr, ac->ac_enaddr);
			else 
			{
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
			}

		}
	}
	
	// log(LOG_DEBUG, "firewire_arpresolve: end \n");
	
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arpintr
//
// IN: register struct mbuf *m
// 
// Invoked by : 
// inet_firewire_input in firewire_inet_pr_module.c and it will be called from 
// the context of dlil_input_thread queue
//
// Common length and type checks are done here, then the protocol-specific 
// routine is called.
//
////////////////////////////////////////////////////////////////////////////////
void
firewire_arpintr(register struct mbuf *m)
{
    if (m == 0 || (m->m_flags & M_PKTHDR) == 0)
        panic("arpintr");
		
    if (m->m_len < (int)sizeof(IP1394_ARP) &&
	    (m = m_pullup(m, sizeof(IP1394_ARP))) == NULL) {
		log(LOG_ERR, "in_arp: runt packet -- m_pullup failed\n");
        m_freem(m);
		return;
	}
    
	in_firewirearpinput(m);
}

#if INET

static int log_arp_wrong_iface = 0;

//SYSCTL_INT(_net_link_firewire_inet, OID_AUTO, log_arp_wrong_iface, CTLFLAG_RW,
//	&log_arp_wrong_iface, 0, "log arp packets arriving on the wrong interface");

////////////////////////////////////////////////////////////////////////////////
//
// in_firewirearpinput
//
// IN: register struct mbuf *m
// 
// Invoked by : 
// firewire_arpintr calls it from the context of dlil_input_thread queue
//
// ARP for Internet protocols on 10 Mb/s Ethernet. 
// Algorithm is that given in RFC 826.
// In addition, a sanity check is performed on the sender
// protocol address, to catch impersonators.
// We no longer handle negotiations for use of trailer protocol:
// Formerly, ARP replied for protocol type ETHERTYPE_TRAIL sent
// along with IP replies if we wanted trailers sent to us,
// and also sent them in response to IP replies.
// This allowed either end to announce the desire to receive trailer packets.
// We no longer reply to requests for ETHERTYPE_TRAIL protocol either,
// but formerly didn't normally send requests.
//
////////////////////////////////////////////////////////////////////////////////
static void
in_firewirearpinput(register struct mbuf *m)
{
    register IP1394_ARP *fwa;
	register struct arpcom *ac = (struct arpcom *)m->m_pkthdr.rcvif;
    struct firewire_header *fwh;
	register struct llinfo_arp *la = 0;
	register struct rtentry *rt;
	struct in_ifaddr *ia, *maybe_ia = 0;
	struct sockaddr_dl *sdl;
    struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op;
	unsigned char buf[18];
    unsigned char buf2[18];
    struct ifnet *ifp = (struct ifnet *)ac;
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
    LCB	*lcb;
    ARB *fwarb = NULL;

	//log(LOG_DEBUG," in_firewirearpinput\n");

	if(fwIpObj == NULL)
	{
		log(LOG_DEBUG, "if_softc is NULL %d \n", __LINE__);
		m_freem(m);
		return;
	}
	
	lcb = fwIpObj->getLcb();
	
    // Get the ARP packet from the mbuf
	fwa = mtod(m, IP1394_ARP *);
        
    // Get the opcode
	op  = ntohs(fwa->opcode);
        
	// Do all checks to see whether its and ARP packet
    if (fwa->hardwareType != htons(ARP_HDW_TYPE) 
        || fwa->protocolType != htons(ETHERTYPE_IP)
        || fwa->hwAddrLen != sizeof(IP1394_HDW_ADDR)
        || fwa->ipAddrLen != IPV4_ADDR_SIZE)
	{
		log(LOG_DEBUG," firewire arp packet corrupt\n");
        m_freem(m);
        return;
    }
    
    isaddr.s_addr = fwa->senderIpAddress;
    itaddr.s_addr = fwa->targetIpAddress;
	
    //(void)memcpy(&isaddr, fwa->senderIpAddress, sizeof (isaddr));
	//(void)memcpy(&itaddr, fwa->targetIpAddress, sizeof (itaddr));
    
#if __APPLE__
    /* Don't respond to requests for 0.0.0.0 */
    if (itaddr.s_addr == 0 && op == ARPOP_REQUEST) 
	{
        m_freem(m);
        return;
    }
#endif
    
	for (ia = in_ifaddrhead.tqh_first; ia; ia = ia->ia_link.tqe_next) {
		/*
		 * For a bridge, we want to check the address irrespective
		 * of the receive interface. (This will change slightly
		 * when we have clusters of interfaces).
		 */
#if BRIDGE
#define BRIDGE_TEST (do_bridge)
#else
#define BRIDGE_TEST (0) /* cc will optimise the test away */
#endif
		if ((BRIDGE_TEST) || (ia->ia_ifp == &ac->ac_if)) {
			maybe_ia = ia;
			if ((itaddr.s_addr == ia->ia_addr.sin_addr.s_addr) ||
			     (isaddr.s_addr == ia->ia_addr.sin_addr.s_addr)) {
				break;
			}
		}
	}
    
	if (maybe_ia == 0) {
		m_freem(m);
		return;
	}

    
	myaddr = ia ? ia->ia_addr.sin_addr : maybe_ia->ia_addr.sin_addr;
    
    // check to see whether its our own arp packet
	if (!bcmp((void*)&fwa->senderUniqueID, (void*)&lcb->ownHardwareAddress.eui64,
	    sizeof (fwa->senderUniqueID))) 
	{
		m_freem(m);	// it's from me, ignore it.
        return;
	}
    
	//log(LOG_DEBUG," in_firewirearpinput %d\n", __LINE__);
	
#ifdef FIREWIRETODO
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)fwbroadcastaddr,
	    sizeof (ea->arp_sha))) {
		log(LOG_ERR,
		    "arp: ether address is broadcast for IP address %s!\n",
		    inet_ntoa(isaddr));
		m_freem(m);
		return;
	}
#endif

	if (isaddr.s_addr == myaddr.s_addr) 
	{
		struct kev_msg        ev_msg;
		struct kev_in_collision	*in_collision;
		u_char	storage[sizeof(struct kev_in_collision) + 8];
		in_collision = (struct kev_in_collision*)storage;
		
		log(LOG_ERR,
				"duplicate IP address %s sent from ethernet address %s\n",
				inet_ntoa(isaddr), firewire_sprintf(buf, (u_char*)&fwa->senderUniqueID));
		
		/* Send a kernel event so anyone can learn of the conflict */
		in_collision->link_data.if_family = ac->ac_if.if_family;
		in_collision->link_data.if_unit = ac->ac_if.if_unit;
		strncpy(&in_collision->link_data.if_name[0], ac->ac_if.if_name, IFNAMSIZ);
		in_collision->ia_ipaddr = isaddr;
		in_collision->hw_len = FIREWIRE_ADDR_LEN;
		bcopy((caddr_t)&fwa->senderUniqueID, (caddr_t)in_collision->hw_addr, sizeof(fwa->senderUniqueID));
		ev_msg.vendor_code = KEV_VENDOR_APPLE;
		ev_msg.kev_class = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass = KEV_INET_SUBCLASS;
		ev_msg.event_code = KEV_INET_ARPCOLLISION;
		ev_msg.dv[0].data_ptr = in_collision;
		ev_msg.dv[0].data_length = sizeof(struct kev_in_collision) + 8;
		ev_msg.dv[1].data_length = 0;
		kev_post_msg(&ev_msg);
	
		log(LOG_ERR, "duplicate IP address %s sent from address\n",
								inet_ntoa(isaddr));
		itaddr = myaddr;
		goto firewirearpreply;
	}
	
	//log(LOG_DEBUG," in_firewirearpinput sAddr=%s myAddr=%s itAddr=%s %d\n", 
	//						inet_ntoa(isaddr), inet_ntoa(myaddr), inet_ntoa(itaddr),__LINE__);
    
    // lookup to see whether we have an entry already, we don't create if it does not exist
	la = firewire_arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) 
	{
		//log(LOG_DEBUG," in_firewirearpinput %d\n", __LINE__);
	
		// the following is not an error when doing bridging
		if (!BRIDGE_TEST && rt->rt_ifp != &ac->ac_if) {
		    if (log_arp_wrong_iface)
			log(LOG_ERR, "arp: %s is on %s%d but got reply on %s%d\n",
			    inet_ntoa(isaddr),
			    rt->rt_ifp->if_name, rt->rt_ifp->if_unit,
			    ac->ac_if.if_name, ac->ac_if.if_unit);
		    goto firewirearpreply;
		}

        // Get the arb pointer from sdl->data
        fwarb = fwIpObj->getUnicastArb(lcb, isaddr.s_addr);

		//log(LOG_DEBUG," fwarb %x sip %x tip %x \n", fwarb, isaddr.s_addr, itaddr.s_addr);

        // If old entry then does the compare
        if (sdl->sdl_alen && bcmp((void*)&fwa->senderUniqueID, (void*)&fwarb->eui64, FIREWIRE_ADDR_LEN)) 
		{
			if (rt->rt_expire)
			    log(LOG_INFO, "arp: %s moved from %s to %s on %s%d\n",
											inet_ntoa(isaddr),
											firewire_sprintf(buf, (u_char*)&fwarb->eui64),
											firewire_sprintf(buf2, (u_char*)&fwa->senderUniqueID),
											ac->ac_if.if_name, ac->ac_if.if_unit);
			else
			{
			    log(LOG_ERR,
							"arp: %s attempts to modify permanent entry for %s on %s%d",
							firewire_sprintf(buf, (u_char*)&fwa->senderUniqueID), inet_ntoa(isaddr),
							ac->ac_if.if_name, ac->ac_if.if_unit);
			    goto firewirearpreply;
			}
		}
                
        // New entry, so create an arb cache
        if(sdl->sdl_alen == 0)
		{
			//log(LOG_DEBUG," in_firewirearpinput new entry %d\n", __LINE__);

            if(fwarb == NULL)
			{
				// Create a new entry if it does not exist
                fwarb = (ARB*)fwIpObj->allocateCBlk(lcb);
                if(fwarb == NULL)
				{
                    log(LOG_ERR,"arp: arb allocation failed !\n");
                    m_freem(m);
                    return;
                }
                // Fill in the details for the arb cache
                fwarb->ipAddress = fwa->senderIpAddress; // Initialize some parts
                fwIpObj->linkCBlk(&lcb->unicastArb, fwarb);  
            }
        } 
        // End of creating new entry

        // Update the hardware information for the existing arb entry
        fwarb->handle.unicast.maxRec = fwa->senderMaxRec; // Volatile fields
        fwarb->handle.unicast.spd = fwa->sspd;
        fwarb->handle.unicast.unicastFifoHi = htons(fwa->senderUnicastFifoHi);
        fwarb->handle.unicast.unicastFifoLo = htonl(fwa->senderUnicastFifoLo);
        fwarb->eui64.hi = htonl(fwa->senderUniqueID.hi);	
        fwarb->eui64.lo = htonl(fwa->senderUniqueID.lo);
        if (op != ARPOP_REQUEST && fwarb->datagramPending == TRUE) 
		{
			log(LOG_DEBUG,"ARB ipAddress : %X resolved and timer updated\n", fwarb->ipAddress);
			fwarb->timer = 0;
		}
        fwarb->datagramPending = FALSE;
        fwarb->handle.unicast.deviceID = fwIpObj->getDeviceID(lcb, fwarb->eui64, &fwarb->itsMac);    
		fwIpObj->getBytesFromGUID(&fwarb->eui64, fwarb->fwaddr, 0);
        
        // Update the old hardware address with new value if sdl_alen != 0 
		(void)memcpy(LLADDR(sdl), fwarb->fwaddr, FIREWIRE_ADDR_LEN);
        sdl->sdl_alen = FIREWIRE_ADDR_LEN;

		if (rt->rt_expire)
			rt->rt_expire = time_second + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		if (la->la_hold) {
			dlil_output(((struct ifnet *)ac)->if_data.default_proto, la->la_hold, (char*)rt, rt_key(rt), 0);
			la->la_hold = 0;
		}
	}

firewirearpreply:
	if (op != ARPOP_REQUEST) {
		// log(LOG_DEBUG,	"%s %u:mbuf %p freed\n", __FILE__, __LINE__, m);
		m_freem(m);
		return;
	}

	// Free the old mbuf
	if(m)
	{
		// log(LOG_DEBUG,	"%s %u:mbuf %p freed\n", __FILE__, __LINE__, m);
		m_freem(m);
	}
	
	if((m = fwIpObj->getMBuf(sizeof(IP1394_ARP))) == NULL)
		return;
			
	m->m_pkthdr.rcvif = (struct ifnet *)0;
   
	fwa = mtod(m, IP1394_ARP*);
    bzero((caddr_t)fwa, sizeof(*fwa));
    m->m_len = sizeof(*fwa);
    m->m_pkthdr.len = sizeof(*fwa);

//	if (itaddr.s_addr == myaddr.s_addr) {
	// I am the target 
	fwa->senderUnicastFifoHi = ntohl(lcb->ownHardwareAddress.unicastFifoHi);
	fwa->senderUnicastFifoLo = ntohl(lcb->ownHardwareAddress.unicastFifoLo);
	fwa->senderUniqueID.hi   = ntohl(lcb->ownHardwareAddress.eui64.hi);      
	fwa->senderUniqueID.lo   = ntohl(lcb->ownHardwareAddress.eui64.lo);
	fwa->senderMaxRec        = lcb->ownMaxPayload;
	fwa->sspd                = lcb->ownMaxSpeed;
//	}
#ifdef FIREWIRETODO
          else {
		la = firewire_arplookup(itaddr.s_addr, 0, SIN_PROXY);
		if (la == NULL) {
			struct sockaddr_in sin;

			/* FIREWIRETODO: to be changed the way syscntl */
			if (!arp_proxyall) {
				m_freem(m);
				return;
			}

			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = itaddr;

			rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
			if (!rt) {
				m_freem(m);
				return;
			}
			/*
			 * Don't send proxies for nodes on the same interface
			 * as this one came out of, or we'll get into a fight
			 * over who claims what Ether address.
			 */
			if (rt->rt_ifp == &ac->ac_if) {
				rtfree(rt);
				m_freem(m);
				return;
			}
			(void)memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
			(void)memcpy(ea->arp_sha, ac->ac_enaddr, sizeof(ea->arp_sha));
			rtfree(rt);
#if DEBUG_PROXY
			printf("arp: proxying for %s\n",
			       inet_ntoa(itaddr));
#endif
		} else {
			rt = la->la_rt;
			sdl = SDL(rt->rt_gateway);
                        memcpy(handle, LLADDR(sdl), sizeof(TNF_HANDLE));
                        if(sdl->sdl_alen != 0){
                            fwa->senderUnicastFifoHi = ntohl(handle.unicastFifoHi);
                            fwa->senderUnicastFifoLo = ntohl(handle.unicastFifoLo);
                            fwa->senderUniqueID.hi   = ntohl(fwarb->eui64.unicastFifoHi);      
                            fwa->senderUniqueID.lo   = ntohl(fwarb->eui64.unicastFifoLo)
                            fwa->senderMaxRec        = fwarb->handle.maxRec;
                            fwa->sspd                = fwarb->handle.spd;
                        }
		}
	}
#endif
    fwa->hardwareType = htons(ARP_HDW_TYPE);
    fwa->protocolType = htons(ETHERTYPE_IP);
    fwa->hwAddrLen = sizeof(IP1394_HDW_ADDR);
    fwa->ipAddrLen = IPV4_ADDR_SIZE;
    fwa->opcode = htons(ARP_RESPONSE);
    fwa->senderIpAddress = ac->ac_ipaddr.s_addr;
    fwa->targetIpAddress = isaddr.s_addr;

    fwh = (struct firewire_header *)sa.sa_data;
	fwh->ether_type = htons(ETHERTYPE_ARP);
	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);

    // call dlil to send the packet
    dlil_output(((struct ifnet *)ac)->if_data.default_proto, m, 0, &sa, 0);
    
	return; 
}
#endif

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arptfree
//
// Invoked by:
// firewire_arptimer
//
// arp free routine
//
////////////////////////////////////////////////////////////////////////////////
static void firewire_arptfree(register struct llinfo_arp *la)
{
	register struct rtentry *rt = la->la_rt;
	register struct sockaddr_dl *sdl;
	
	if (rt == 0)
		panic("firewire_arptfree");
	
	if (rt->rt_refcnt > 0 && (sdl = SDL(rt->rt_gateway)) &&
	    sdl->sdl_family == AF_LINK) 
	{
		sdl->sdl_alen = 0;
		la->la_asked = 0;
		rt->rt_flags &= ~RTF_REJECT;
		return;
	}
	rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0, rt_mask(rt),
			        0, (struct rtentry **)0);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arplookup
// 
// Invoked by:
//  Local to the file
//
// Lookup or enter a new address in arptab.
// FIREWIRETOD: should we initialize an "arb" here ?. 
//
////////////////////////////////////////////////////////////////////////////////
static struct llinfo_arp *firewire_arplookup(u_long addr,int create, int proxy)
{
	register struct rtentry *rt;
	static struct sockaddr_inarp sin = {sizeof(sin), AF_INET };
	const char *why = 0;

	sin.sin_addr.s_addr = addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;
	rt = rtalloc1((struct sockaddr *)&sin, create, 0UL);
	if (rt == 0)
	{
		log(LOG_DEBUG,"%s %d\n", __FILE__, __LINE__);
		return (0);
	}
	rtunref(rt);

	if (rt->rt_flags & RTF_GATEWAY)
		why = "host is not on local network";
	else if ((rt->rt_flags & RTF_LLINFO) == 0)
		why = "could not allocate llinfo";
	else if (rt->rt_gateway->sa_family != AF_LINK)
		why = "gateway route is not ours";

	if (why && create) 
	{
		log(LOG_DEBUG, "firewire_arplookup %s failed: %s\n",
		    inet_ntoa(sin.sin_addr), why);
		return 0;
	} 
	else if (why) 
	{
		return 0;
	}

	return ((struct llinfo_arp *)rt->rt_llinfo);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arp_ifinit
// 
// Invoked by:
//  firewire_arp_ifinit invoked from firewire_ioctl 
//  in if_firewiresubr.c.
//
////////////////////////////////////////////////////////////////////////////////
void firewire_arp_ifinit(struct arpcom *ac, struct ifaddr *ifa)
{
	// log(LOG_DEBUG, "firewire_arp_ifinit \n");

	if (ntohl(IA_SIN(ifa)->sin_addr.s_addr) != INADDR_ANY)
		firewire_arprequest(ac, &IA_SIN(ifa)->sin_addr, &IA_SIN(ifa)->sin_addr, ac->ac_enaddr);
        
	ifa->ifa_rtrequest = firewire_arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}


/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static u_char digits[] = "0123456789abcdef";
u_char *
firewire_sprintf(register u_char *p, register u_char *ap)
{	
    register u_char *cp;
    register int i;

        for (cp = p, i = 0; i < 8; i++) {
                *cp++ = digits[*ap >> 4];
                *cp++ = digits[*ap++ & 0xf];
                *cp++ = ':';
        }
        *--cp = 0;
        return (p);
}
