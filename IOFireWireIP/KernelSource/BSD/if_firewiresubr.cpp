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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/dlil.h>
//#include <sys/sysctl.h>
//#include <sys/systm.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
//#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>


#if INET || INET6
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#if IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#include <sys/socketvar.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

#if BRIDGE
#include <net/bridge.h>
#endif

// #include "vlan.h"
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

}

extern "C" {
#include "firewire.h"
extern void _logMbuf(struct mbuf * m);
}

#include "ip_firewire.h"
#include "IOFireWireIP.h"

static int firewire_resolvemulti __P((struct ifnet*, struct sockaddr**, struct sockaddr*));
                                    
static int firewire_ioctl __P((struct ifnet*, u_long, void*));

static int firewire_output __P((struct ifnet*, struct mbuf*));

static int firewire_free __P((struct ifnet *ifp));

//int firewire_demux __P((struct ifnet *ifp, u_long, char *frame_header,struct if_proto **proto));

extern void	firewire_arp_ifinit __P((struct arpcom *, struct ifaddr *));
extern void firewire_arpwhohas __P((struct arpcom *ac, struct in_addr *addr));

// 1394 broadcast address
extern TNF_MULTICAST_HANDLE broadcastAddress;

#define senderr(e) do { error = (e); goto bad;} while (0)
#define IFP2AC(IFP) ((struct arpcom *)IFP)


////////////////////////////////////////////////////////////////////////////////
//
// firewire_ifdetach  
// At present called from nic_detach from IOFireWireIP.cpp
//
////////////////////////////////////////////////////////////////////////////////
void
firewire_ifdetach(register struct ifnet *ifp)
{
	dlil_if_detach(ifp);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_ifattach  
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_ifattach(register struct ifnet *ifp)
{
    register struct ifaddr *ifa;
	register struct sockaddr_dl *sdl;
	boolean_t funnel_state;
	char macAddr[FIREWIRE_ADDR_LEN];
	u_long dl_tag;
	int ret = 0;
	
    IOFireWireIP *fIPObj = (IOFireWireIP*)(ifp->if_softc);
//    LCB *lcb  = fIPObj->getLcb();

	funnel_state = thread_funnel_set(network_flock, TRUE);
    
    ifp->if_name = "fw";
	ifp->if_family = APPLE_IF_FAM_FIREWIRE;
	ifp->if_type = IFT_IEEE1394;
	ifp->if_addrlen = FIREWIRE_ADDR_LEN;
	ifp->if_hdrlen = 18;
    ifp->if_flags 	= IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST | IFF_RUNNING;
    getmicrotime(&ifp->if_lastchange);

    ifp->if_resolvemulti = firewire_resolvemulti;
    ifp->if_ioctl   = firewire_ioctl;
    ifp->if_output  = firewire_output;
    ifp->if_free    = firewire_free;
    
	if (ifp->if_baudrate == 0)
	    ifp->if_baudrate = 10000000;

	ret = dlil_if_attach(ifp);
	ifa = ifnet_addrs[ifp->if_index - 1];
	if (ifa == 0) {
		printf("firewire_ifattach: no lladdr!\n");
		(void) thread_funnel_set(network_flock, funnel_state);
		return -1;
	}
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_IEEE1394;
	sdl->sdl_alen = ifp->if_addrlen;
    /* will use the 3 bytes for storing "fw0" */
    sdl->sdl_nlen = 3;
    
	ifp->if_unit = fIPObj->getInstanceID();
	// copy the interface name
	sprintf(sdl->sdl_data, "%s%d", ifp->if_name, ifp->if_unit);
	
	if(fIPObj)
		fIPObj->getMacAddress(macAddr, ifp->if_addrlen);

	bcopy(macAddr, LLADDR(sdl), ifp->if_addrlen);

    /* change required by increasing the size of ac_enaddr from 6 -> 12 */
    memcpy((IFP2AC(ifp))->ac_enaddr, macAddr, ifp->if_addrlen);

	//log(LOG_DEBUG,"firewire_ifattach called for %s%d\n", ifp->if_name, ifp->if_unit);

	// Do dl_tag specific information
	dl_tag = firewire_attach_inet(ifp);
	
    ifa->ifa_dlt = dl_tag;

	(void) thread_funnel_set(network_flock, funnel_state);
	
	return ret;
}

/* SYSCTL_DECL(_net_link); */
/* SYSCTL_NODE(_net_link, IFT_ETHER, ether, CTLFLAG_RW, 0, "Ethernet"); */

////////////////////////////////////////////////////////////////////////////////
//
// firewire_resolvemulti 
// Gets invoked for resolving multicast address
//
////////////////////////////////////////////////////////////////////////////////
int firewire_resolvemulti(struct ifnet *ifp, struct sockaddr **llsa, struct sockaddr *sa)
{
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
	u_char *e_addr;
#if INET6
    struct sockaddr_in6 *sin6;
#endif
    ARB	   *arb;
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
    LCB	*lcb = fwIpObj->getLcb();

	switch(sa->sa_family) {
	case AF_UNSPEC:
		/* AppleTalk uses AF_UNSPEC for multicast registration.
		 * No mapping needed. Just check that it's a valid MC address.
		 */
		return EAFNOSUPPORT;

	case AF_LINK:
		/* 
		 * No mapping needed. Just check that it's a valid MC address.
		 */
		sdl = (struct sockaddr_dl *)sa;
		e_addr = (u_char*)LLADDR(sdl);
		//
		// deviceID for multicast handle is always zero, so if its not zero then 
		// its not multicast handle
		//
		if ((e_addr[0] & 0) != kInvalidIPDeviceRefID)
			return EADDRNOTAVAIL;
		*llsa = 0;
		return 0;

#if INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_IEEE1394;
		sdl->sdl_nlen = 0;
		sdl->sdl_alen = sizeof(TNF_HANDLE);
		sdl->sdl_slen = 0;
		e_addr = (u_char*)LLADDR(sdl);
		
		log(LOG_DEBUG, "   if_firewiresubr firewire_resolvemulti\n");
		//
		// Get the multicast arb and store it in a multicast arb
		//
        arb = fwIpObj->getMulticastArb(lcb, ntohl(sin->sin_addr.s_addr));
            
        // Allocated by MCAP to a particular channel
        if (arb == NULL) {
			// Allocate a handle
            if ((arb = (ARB*)fwIpObj->allocateCBlk(lcb)) == NULL){
				log(LOG_DEBUG, "No multicast CBLK's \n");
                break;
			}
                
            memcpy(&arb->handle, &broadcastAddress, sizeof(TNF_HANDLE));
            ((TNF_MULTICAST_HANDLE*) arb)->groupAddress = ntohl(sin->sin_addr.s_addr);
            fwIpObj->linkCBlk(&lcb->multicastArb, arb);
                    
            fwIpObj->txMCAP(lcb, NULL, arb->handle.multicast.groupAddress);
		}
        // end of mapping the IP address to multicastable address
        memcpy(e_addr, &arb->handle, sizeof(TNF_HANDLE));
		*llsa = (struct sockaddr *)sdl;
		return 0;
		
#endif
#if INET6  // FIREWIRETODO - INET6 multicast handling
        case AF_INET6:
                sin6 = (struct sockaddr_in6 *)sa;
                if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
                        /*
                         * An IP6 address of 0 means listen to all
                         * of the Ethernet multicast address used for IP6.
                         * (This is used for multicast routers.)
                         */
                        ifp->if_flags |= IFF_ALLMULTI;
                        *llsa = 0;
                        return 0;
                }
                MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
                       M_WAITOK);
                sdl->sdl_len = sizeof *sdl;
                sdl->sdl_family = AF_LINK;
                sdl->sdl_index = ifp->if_index;
                sdl->sdl_type = IFT_IEEE1394;
                sdl->sdl_nlen = 0;
                sdl->sdl_alen = sizeof(TNF_HANDLE);
                sdl->sdl_slen = 0;
                e_addr = LLADDR(sdl);
                ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, e_addr);
                log(LOG_DEBUG,"ether_resolvemulti Adding %x:%x:%x:%x:%x:%x\n",
                                e_addr[0], e_addr[1], e_addr[2], e_addr[3], e_addr[4], e_addr[5]);
                *llsa = (struct sockaddr *)sdl;
                return 0;
#endif

	default:
		/* 
		 * Well, the text isn't quite right, but it's the name
		 * that counts...
		 */
		return EAFNOSUPPORT;
	}
    return 0;
}

u_char	firewire_ipmulticast_min[6] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 };
u_char	firewire_ipmulticast_max[6] = { 0x01, 0x00, 0x5e, 0x7f, 0xff, 0xff };

////////////////////////////////////////////////////////////////////////////////
//
// firewire_addmulti  - 
// Add multicast address or range of addresses to the list for a
// given interface
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_addmulti(struct ifreq *ifr, register struct ifnet *ifp)
{
//	register struct ether_multi *enm;
	struct sockaddr_in *sin;
	u_char addrlo[6];
	u_char addrhi[6];
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
    LCB	*lcb = fwIpObj->getLcb();
	ARB *arb = NULL;
	
//	int s = splimp();

	switch (ifr->ifr_addr.sa_family) {

	case AF_UNSPEC:
		return (EAFNOSUPPORT);

#if INET
	case AF_INET:
		sin = (struct sockaddr_in *)&(ifr->ifr_addr);
		if (sin->sin_addr.s_addr == INADDR_ANY) {
			/*
			 * An IP address of INADDR_ANY means listen to all
			 * of the Ethernet multicast addresses used for IP.
			 * (This is for the sake of IP multicast routers.)
			 */
			bcopy(firewire_ipmulticast_max, addrlo, 6);
			bcopy(firewire_ipmulticast_max, addrhi, 6);
		}
		else {
			ETHER_MAP_IP_MULTICAST(&sin->sin_addr, addrlo);
			bcopy(addrlo, addrhi, 6);
		}
		break;
#endif

	default:
		// splx(s);
		return (EAFNOSUPPORT);
	}

	/*
	 * Verify that we have valid Ethernet multicast addresses.
	 */
	if ((addrlo[0] & 0x01) != 1 || (addrhi[0] & 0x01) != 1) {
		// splx(s);
		return (EINVAL);
	}

	log(LOG_DEBUG,"   if_firewiresubr firewire_addmulti %x:%x:%x:%x:%x:%x\n",
                                addrhi[0], addrhi[1], addrhi[2], addrhi[3], addrhi[4], addrhi[5]);

	log(LOG_DEBUG,"  if_firewiresubr firewire_addmulti %x\n", ntohl(sin->sin_addr.s_addr));
	//
	// Get the multicast arb and store it in a multicast arb
	//
	arb = fwIpObj->getMulticastArb(lcb, ntohl(sin->sin_addr.s_addr));
            
    // Allocated by MCAP to a particular channel
    if (arb == NULL) {
		// Allocate a handle
		if ((arb = (ARB*)fwIpObj->allocateCBlk(lcb)) == NULL){
			log(LOG_DEBUG, "No multicast CBLK's \n");
			// splx(s);
            return ENOMEM;
		}
                
        memcpy(&arb->handle, &broadcastAddress, sizeof(TNF_HANDLE));
        ((TNF_MULTICAST_HANDLE*) arb)->groupAddress = ntohl(sin->sin_addr.s_addr);
        fwIpObj->linkCBlk(&lcb->multicastArb, arb);
                    
        fwIpObj->txMCAP(lcb, NULL, arb->handle.multicast.groupAddress);
	}
	
	// splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_delmulti
// Delete a multicast address 
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_delmulti(struct ifreq *ifr,register struct ifnet *ifp, struct ether_addr * ret_mca)
{
//	register struct ether_multi *enm;
//	register struct ether_multi **p;
	struct sockaddr_in *sin;
	u_char addrlo[6];
	u_char addrhi[6];
	ARB *arb;
	UNSIGNED channel;
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
    LCB	*lcb = fwIpObj->getLcb();
	
//	int s = splimp();
	
	switch (ifr->ifr_addr.sa_family) {

	case AF_UNSPEC:
		return (EAFNOSUPPORT);

#if INET
	case AF_INET:
		sin = (struct sockaddr_in *)&(ifr->ifr_addr);
		if (sin->sin_addr.s_addr == INADDR_ANY) {
			/*
			 * An IP address of INADDR_ANY means stop listening
			 * to the range of Ethernet multicast addresses used
			 * for IP.
			 */
			bcopy(firewire_ipmulticast_min, addrlo, 6);
			bcopy(firewire_ipmulticast_max, addrhi, 6);
		}
		else {
			ETHER_MAP_IP_MULTICAST(&sin->sin_addr, addrlo);
			bcopy(addrlo, addrhi, 6);
		}
		break;
#endif

	default:
		// splx(s);
		return (EAFNOSUPPORT);
	}

#ifdef FIREWIRETODO  // why do we have to store the return multicast address
	/* save the low and high address of the range before deletion */
	if (ret_mca) {
	   	*ret_mca	= *((struct ether_addr *)addrlo);
	   	*(ret_mca + 1)	= *((struct ether_addr *)addrhi);
	}
#endif
	log(LOG_DEBUG,"  if_firewiresubr firewire_delmulti %x:%x:%x:%x:%x:%x\n",
                                addrhi[0], addrhi[1], addrhi[2], addrhi[3], addrhi[4], addrhi[5]);
	log(LOG_DEBUG,"  if_firewiresubr firewire_delmulti %x\n", ntohl(sin->sin_addr.s_addr));
								
	/* Search MCAP cache for group address */
	arb = fwIpObj->getMulticastArb(lcb, ntohl(sin->sin_addr.s_addr));
	/* Find a corresponding entry? */
	if (arb != NULL) {         
		channel = arb->handle.multicast.channel;
		/* Related MCB? */
        if (channel == DEFAULT_BROADCAST_CHANNEL) {
			/* No, just remove it */
			fwIpObj->unlinkCBlk(&lcb->multicastArb, arb);   
			fwIpObj->deallocateCBlk(lcb, arb);
		} else if (lcb->mcapState[channel].ownerNodeID == lcb->ownNodeID)
				if (lcb->mcapState[channel].groupCount == 0) {
					fwIpObj->unlinkCBlk(&lcb->multicastArb, arb);
					fwIpObj->deallocateCBlk(lcb, arb);
				} else {
					/* Signal watchdog to clean up */
					arb->deletionPending = TRUE;  
					lcb->mcapState[channel].groupCount--;
				}
         else{               
			/* Owned by another node, safe to release ARB */
			fwIpObj->unlinkCBlk(&lcb->multicastArb, arb);
			fwIpObj->deallocateCBlk(lcb, arb);
		}
	}


	// splx(s);
	/*
	 * Return ENETRESET to inform the driver that the list has changed
	 * and its reception filter should be adjusted accordingly.
	 */
	return (ENETRESET);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_free - No cleanup necessary over here
// Frees the firewire interface
//
////////////////////////////////////////////////////////////////////////////////
static int 	
firewire_free (struct ifnet	*ifp)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_output  
// Send the packet out to the firewire node
//
////////////////////////////////////////////////////////////////////////////////
static int 	
firewire_output (struct ifnet *ifp, struct mbuf *m)
{
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
	register struct firewire_header *fwh;
	int	status = 0;
	
	fwh = mtod(m, struct firewire_header*);
	
	//log(LOG_DEBUG,"firewire_output\n");
	
	//
	// Switch based on the ether type in the firewire header
	// 
	if (fwh->ether_type == htons(ETHERTYPE_ARP)){
		//
		// It should be a Arp packet
		//
		fwIpObj->txARP(ifp, m);
	
	}else if(fwh->ether_type == htons(ETHERTYPE_IP)) {
		// 
		// It should be multicast/unicast packet
		//
		status = fwIpObj->txIP(ifp, m, NULL);
	
	} else {
	
		log(LOG_DEBUG,"firewire: ether type not supported %x\n", fwh->ether_type);
	
	}
	
	// log(LOG_DEBUG,"mbuf %x freed\n", m);
	// _logMbuf(m);

	// Free the mbuf
	m_freem(m);

	return status;
} 

////////////////////////////////////////////////////////////////////////////////
//
// firewire_ioctl 
// Process the ioctl request
//
////////////////////////////////////////////////////////////////////////////////
static int
firewire_ioctl(struct ifnet *ifp, u_long cmd, void* data)
{
    struct ifaddr *ifa = (struct ifaddr *) data;
    struct ifreq *ifr = (struct ifreq *) data;
	// register struct in_ifaddr *ia = 0, *iap = NULL;
	struct in_ifaddr 	*ia = (struct in_ifaddr *)data;
    struct rslvmulti_req *rsreq = (struct rslvmulti_req *) data;
    int error = 0, s = 0;
	// boolean_t funnel_state;
    struct arpcom *ac = (struct arpcom *) ifp;
    struct sockaddr_dl *sdl;
    struct sockaddr_in *sin;
    u_char *e_addr;
    ARB	   *arb;
	u_long dl_tag;
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
    LCB	*lcb = fwIpObj->getLcb();
    
	/* Not needed at soo_ioctlis already funnelled */
    // funnel_state = thread_funnel_set(network_flock,TRUE);

    switch (cmd) {
    
        case SIOCRSLVMULTI:
			{
			//log(LOG_DEBUG, "if_firewiresubr SIOCRSLVMULTI\n");
            switch(rsreq->sa->sa_family) {

            case AF_INET:
                sin = (struct sockaddr_in *)rsreq->sa;
                if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
                    return EADDRNOTAVAIL;
                MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR, M_WAITOK);
                sdl->sdl_len = sizeof *sdl;
                sdl->sdl_family = AF_LINK;
                sdl->sdl_index = ifp->if_index;
                sdl->sdl_type = IFT_IEEE1394;
                sdl->sdl_nlen = 0;
                sdl->sdl_alen = sizeof(TNF_HANDLE);
                sdl->sdl_slen = 0;
                e_addr = (u_char*)LLADDR(sdl);
                
                //
                // Get the multicast arb and store it in a multicast arb
                //
                arb = fwIpObj->getMulticastArb(lcb, ntohl(sin->sin_addr.s_addr));
            
                // Allocated by MCAP to a particular channel
                if (arb == NULL) {
                    // Allocate a handle
                    if ((arb = (ARB*)fwIpObj->allocateCBlk(lcb)) == NULL){
                        log(LOG_DEBUG, "No multicast CBLK's \n");
                        break;
                    }
                
                    memcpy(&arb->handle, &broadcastAddress, sizeof(TNF_HANDLE));
                    ((TNF_MULTICAST_HANDLE*) arb)->groupAddress = ntohl(sin->sin_addr.s_addr);
                    fwIpObj->linkCBlk(&lcb->multicastArb, arb);
                    
                    fwIpObj->txMCAP(lcb, NULL, arb->handle.multicast.groupAddress);
                }
                // end of mapping the IP address to multicastable address
                memcpy(e_addr, &arb->handle, sizeof(TNF_HANDLE));
                *rsreq->llsa = (struct sockaddr *)sdl;
				log(LOG_DEBUG, "Resolve multicast called %d \n", __LINE__);
                return 0;

            default:
                // 
                // Well, the text isn't quite right, but it's the name
                // that counts...
                //
                return EAFNOSUPPORT;
            }
        }
    
        case SIOCSIFADDR:
			//log(LOG_DEBUG, "if_firewiresubr SIOCSIFADDR\n");
            if ((ifp->if_flags & IFF_RUNNING) == 0) {
				//log(LOG_DEBUG, "if_firewiresubr SIOCSIFADDR %d \n", __LINE__);
				ifp->if_flags |= IFF_UP;
                dlil_ioctl(0, ifp, SIOCSIFFLAGS, (caddr_t) 0);
            }

            switch (ifa->ifa_addr->sa_family) {
                case AF_INET:
					//log(LOG_DEBUG, "if_firewiresubr SIOCSIFADDR %s: %d \n", __FILE__, __LINE__);
                    // Initialize the driver state if you want to !
                    if (ifp->if_init)
                        ifp->if_init(ifp->if_softc);	/* before arpwhohas */
                    
                    // Attach to the protocol interface
                    dl_tag = firewire_attach_inet(ifp);
					
					// Add the dl_tag to the configured interface address
					if (ia == (struct in_ifaddr *)0) {
	
						ia = (struct in_ifaddr *)_MALLOC(sizeof *ia, M_IFADDR, M_WAITOK);
						
						if (ia == (struct in_ifaddr *)NULL)
							return (ENOBUFS);

						bzero((caddr_t)ia, sizeof *ia);
						/*
						 * Protect from ipintr() traversing address list
						 * while we're modifying it.
						 */
						s = splnet();
			
						TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);
						ifa = &ia->ia_ifa;
						TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

						log(LOG_DEBUG,"Added address and dl_tag %d to the ifp list of address\n", dl_tag);

						ifa->ifa_dlt = dl_tag;
						ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
						ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
						ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
						ia->ia_sockmask.sin_len = 8;
						if (ifp->if_flags & IFF_BROADCAST) {
							ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
							ia->ia_broadaddr.sin_family = AF_INET;
						}
						ia->ia_ifp = ifp;
						//		if (!(ifp->if_flags & IFF_LOOPBACK))
						//			in_interfaces++;
						splx(s);
					}else {
						//log(LOG_DEBUG," DID NOT ALLOCATE ! GOT dl_tag %d \n", dl_tag);
						ia->ia_ifa.ifa_dlt = dl_tag;
					}
                    
                    //
                    // See if another station has *our* IP address.
                    // i.e.: There is an address conflict! If a
                    // conflict exists, a message is sent to the
                    // console.
                    //
                    if (IA_SIN(ifa)->sin_addr.s_addr != 0)
                    {
                        // don't bother for 0.0.0.0
                        ac->ac_ipaddr = IA_SIN(ifa)->sin_addr;
						// Set the ip address in the link control block
						fwIpObj->setIPAddress(&IA_SIN(ifa)->sin_addr);
                        firewire_arpwhohas(ac, &IA_SIN(ifa)->sin_addr);
                    }
					//log(LOG_DEBUG, "if_firewiresubr SIOCSIFADDR %d %x %x\n", __LINE__, ac->ac_ipaddr, IA_SIN(ifa)->sin_addr);
                    firewire_arp_ifinit(IFP2AC(ifp), ifa);
                    break;
					
				case AF_INET6:
					// Do the dl_tag magic here to get it attached to the IPV6 layer
					//log(LOG_DEBUG, "if_firewiresubr SIOCSIFADDR IPV6 ARP here %d %x\n", __LINE__, IA_SIN(ifa)->sin_addr);
                    break;

                default:
					log(LOG_DEBUG, "if_firewiresubr SIOCSIFADDR  %d ac->ip_addr %x sin_addr %x family %d\n", __LINE__, 															ac->ac_ipaddr, IA_SIN(ifa)->sin_addr, ifa->ifa_addr->sa_family);
                    break;
            }
            break;

        case SIOCGIFADDR:
        {
			//log(LOG_DEBUG, "if_firewiresubr SIOCGIFADDR\n");
            struct sockaddr *sa;
            char macAddr[FIREWIRE_ADDR_LEN];

            sa = (struct sockaddr *) & ifr->ifr_data;
            
            // Get the cooked hardware adddress from IOFireWireIP combination of ChipID and VendorID
            // we will make it look like ethernet hardware address so the UI and pref panes are happy 
            if(fwIpObj != NULL)
				fwIpObj->getMacAddress(macAddr, FIREWIRE_ADDR_LEN);

            bcopy(macAddr, (caddr_t) sa->sa_data, FIREWIRE_ADDR_LEN);
        }
        break;

		case SIOCGIFMTU:
			//log(LOG_DEBUG, "if_firewiresubr SIOCGIFMTU\n");
			ifr->ifr_mtu = fwIpObj->getMTU();
			break;
		
		case SIOCGIFMETRIC:
			//log(LOG_DEBUG, "if_firewiresubr SIOCGIFMETRIC\n");
			// ifr->ifru_metric = 
			break;

		case SIOCADDMULTI:
			log(LOG_DEBUG, "%s add multicast\n", __FILE__);
			if(ifr != NULL) 
				error = firewire_addmulti(ifr, ifp);
			break;
			
		case SIOCDELMULTI:
			char buf[sizeof(struct ether_addr)];
			log(LOG_DEBUG, "%s del multicast\n", __FILE__);
			if(ifr != NULL)
				error = firewire_delmulti(ifr, ifp, (struct ether_addr*)buf);
			break;
			
		case SIOCGIFMEDIA:
			struct ifmediareq *req;
			int *kptr;
			u_long count;
			
			//log(LOG_DEBUG, "if_firewiresubr SIOCGIFMEDIA %d\n", req->ifm_count);
			
			req = (struct ifmediareq *)data;
			req->ifm_active  = req->ifm_current = IFM_AUTO;
			req->ifm_count = 1;
			req->ifm_status = 0;
			count = 0;
			
			if(fwIpObj != NULL)
				count = fwIpObj->getUnitCount();
				
			if(count == 0)
				req->ifm_status = IFM_AVALID;
			else
				req->ifm_status = IFM_AVALID | IFM_ACTIVE;
			
			if (req->ifm_count != 0) {
				kptr = (int *) _MALLOC(req->ifm_count * sizeof(int),
										M_TEMP, M_WAITOK);
				if(kptr != NULL) {
					kptr[0] = IFM_AUTO;
					error = copyout((caddr_t)kptr, (caddr_t)req->ifm_ulist, req->ifm_count * sizeof(int));
				}
			}
			
			if (req->ifm_count != 0)
				FREE(kptr, M_TEMP);
			
			//log(LOG_DEBUG, "if_firewiresubr SIOCGIFMEDIA\n");
			return 0;

		case SIOCGIFSTATUS:
			struct ifstat *ifs;
			ifs = (struct ifstat *)data;
			ifs->ascii[0] = '\0';
			// log(LOG_DEBUG, "if_firewiresubr SIOCGIFSTATUS\n");
			return 0;

		case SIOCSIFFLAGS:
			// log(LOG_DEBUG, "if_firewiresubr SIOCSIFFLAGS\n");
			if (ifp->if_flags & IFF_UP && (ifr->ifr_flags & IFF_UP) == 0) {
					// make the interface down
					ifp->if_flags &= ~IFF_UP;
					getmicrotime(&ifp->if_lastchange);
			} else if (ifr->ifr_flags & IFF_UP && (ifp->if_flags & IFF_UP) == 0) {
					// make the interface up 
					ifp->if_flags |= IFF_UP;
					getmicrotime(&ifp->if_lastchange);
			}			
			return 0;

        case SIOCSIFMTU:
			log(LOG_DEBUG, "if_firewiresubr SIOCSIFMTU\n");
			return EOPNOTSUPP;
			
        case SIOCSIFMEDIA:
			log(LOG_DEBUG, "if_firewiresubr SIOCSIFMEDIA\n");
			return EOPNOTSUPP;
			
        case SIOCSIFLLADDR:
			log(LOG_DEBUG, "if_firewiresubr SIOCSIFLLADDR\n");
			return EOPNOTSUPP;
			
        default:
			log(LOG_DEBUG, "if_firewiresubr firewire_ioctl unsupported cmd %x\n", cmd);
            return EOPNOTSUPP;
    }

    //(void) thread_funnel_set(network_flock, FALSE);

	return (error);
}


