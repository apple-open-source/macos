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
#include <net/dlil.h>
#include <net/if_llc.h>
#if BRIDGE
#include <net/ethernet.h>
#include <net/bridge.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_arp.h>
}

extern "C" 
{
#include "firewire.h"
#include "if_firewire.h"
}
#include "IOFireWireIP.h"

void		firewire_arpintr __P((mbuf_t m));
u_char		*firewire_sprintf __P((register u_char *p, register u_char *ap));
static void inet_firewire_arp_input __P((mbuf_t m));

////////////////////////////////////////////////////////////////////////////////
//
// firewire_arpintr
//
// IN: register mbuf_t m
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
firewire_arpintr(register mbuf_t m)
{
    if (m == 0 || (mbuf_flags(m) & MBUF_PKTHDR) == 0)
        panic("arpintr");
		
    inet_firewire_arp_input(m);
}

#if INET
errno_t
firewire_inet_arp(
	ifnet_t								ifp,
	u_short								arpop,
	const struct sockaddr_dl*			sender_hw,
	const struct sockaddr*				sender_proto,
	const struct sockaddr_dl*			target_hw,
	const struct sockaddr*				target_proto)
{
	mbuf_t	m;
	errno_t	result;
	register struct firewire_header *fwh;
	register IP1394_ARP *fwa;
	const struct sockaddr_in* sender_ip = (const struct sockaddr_in*)sender_proto;
	const struct sockaddr_in* target_ip = (const struct sockaddr_in*)target_proto;
	char *datap;

	IOFWInterface *fwIf	   = (IOFWInterface*)ifnet_softc(ifp);
	
	if(fwIf == NULL)
		return EINVAL;
	
	IOFireWireIP  *fwIpObj = (IOFireWireIP*)fwIf->getController();
    
	if(fwIpObj == NULL)
		return EINVAL;

	LCB	*lcb = fwIpObj->getLcb();
	
	if (target_ip == NULL)
		return EINVAL;
	
	if ((sender_ip && sender_ip->sin_family != AF_INET) ||
		(target_ip && target_ip->sin_family != AF_INET))
		return EAFNOSUPPORT;

	result = mbuf_gethdr(MBUF_DONTWAIT, MBUF_TYPE_DATA, &m);
	if (result != 0)
		return result;

	mbuf_setlen(m, sizeof(*fwa));
	mbuf_pkthdr_setlen(m, sizeof(*fwa));
	
	/* Move the data pointer in the mbuf to the end, aligned to 4 bytes */
	datap = (char*)mbuf_datastart(m);
	datap += mbuf_trailingspace(m);
	datap -= (((u_long)datap) & 0x3);
	mbuf_setdata(m, datap, sizeof(*fwa));
	fwa = (IP1394_ARP*)mbuf_data(m);
	bzero((caddr_t)fwa, sizeof(*fwa));
	
	/* Prepend the ethernet header, we will send the raw frame */
	result = mbuf_prepend(&m, sizeof(*fwh), MBUF_DONTWAIT);
	if(result != 0)
		return result;
	
	fwh = (struct firewire_header*)mbuf_data(m);
    fwh->fw_type = htons(FWTYPE_ARP);
	
	/* Fill out the arp packet */
    fwa->hardwareType = htons(ARP_HDW_TYPE);
    fwa->protocolType = htons(FWTYPE_IP);
    fwa->hwAddrLen = sizeof(IP1394_HDW_ADDR);
    fwa->ipAddrLen = IPV4_ADDR_SIZE;
    fwa->opcode = htons(arpop);
    fwa->senderMaxRec = lcb->ownHardwareAddress.maxRec;
    fwa->sspd = lcb->ownHardwareAddress.spd;
    fwa->senderUnicastFifoHi = htons(lcb->ownHardwareAddress.unicastFifoHi);
    fwa->senderUnicastFifoLo = htonl(lcb->ownHardwareAddress.unicastFifoLo);
	
	/* Sender Hardware */
	if (sender_hw != NULL) 
		bcopy(CONST_LLADDR(sender_hw), &fwa->senderUniqueID, sizeof(fwa->senderUniqueID));
	else 
		ifnet_lladdr_copy_bytes(ifp, &fwa->senderUniqueID, FIREWIRE_ADDR_LEN);

	ifnet_lladdr_copy_bytes(ifp, fwh->fw_shost, sizeof(fwh->fw_shost));
	
	/* Sender IP */
	if (sender_ip != NULL) 
		fwa->senderIpAddress = sender_ip->sin_addr.s_addr;
	else 
	{
		ifaddr_t	*addresses;
		struct sockaddr sa;

		if (ifnet_get_address_list_family(ifp, &addresses, AF_INET) == 0) 
		{
			ifaddr_address( addresses[0], &sa, 16 );
			fwa->senderIpAddress  = ((UInt32)(sa.sa_data[5] & 0xFF)) << 24;
			fwa->senderIpAddress |= ((UInt32)(sa.sa_data[4] & 0xFF)) << 16;
			fwa->senderIpAddress |= ((UInt32)(sa.sa_data[3] & 0xFF)) << 8;
			fwa->senderIpAddress |= ((UInt32)(sa.sa_data[2] & 0xFF));
                        
			ifnet_free_address_list(addresses);
		}
		else 
		{
			mbuf_free(m);
			return ENXIO;
		}
	}
	
	/* Target Hardware */
	if (target_hw == 0) 
		bcopy(fwbroadcastaddr, fwh->fw_dhost, sizeof(fwh->fw_dhost));
	else 
		bcopy(CONST_LLADDR(target_hw), fwh->fw_dhost, sizeof(fwh->fw_dhost));
	
	/* Target IP */
	fwa->targetIpAddress = target_ip->sin_addr.s_addr;
	
	ifnet_output_raw(ifp, PF_INET, m);
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// in_firewire_arp_input
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
inet_firewire_arp_input(
	mbuf_t m)
{
	IP1394_ARP *fwa;
	struct sockaddr_dl	sender_hw;
	struct sockaddr_in	sender_ip;
	struct sockaddr_in	target_ip;

	ifnet_t		  ifp		= mbuf_pkthdr_rcvif((mbuf_t)m);

	IOFWInterface *fwIf		= (IOFWInterface*)ifnet_softc(ifp);

	if(fwIf == NULL)
		return;
	
	IOFireWireIP  *fwIpObj	= (IOFireWireIP*)fwIf->getController();

	if(fwIpObj == NULL)
		return;

    if (mbuf_len(m) < (int)sizeof(IP1394_ARP) &&
	    mbuf_pullup(&m, sizeof(IP1394_ARP)) != 0) 
		return;

	fwa = (IP1394_ARP*)mbuf_data(m);
		
	// Verify this is an firewire/ip arp and address lengths are correct
    if (fwa->hardwareType != htons(ARP_HDW_TYPE) || fwa->protocolType != htons(FWTYPE_IP)
        || fwa->hwAddrLen != sizeof(IP1394_HDW_ADDR) || fwa->ipAddrLen != IPV4_ADDR_SIZE)
	{
        mbuf_free(m);
        return;
    }
		
	bzero(&sender_ip, sizeof(sender_ip));
	sender_ip.sin_len = sizeof(sender_ip);
	sender_ip.sin_family = AF_INET;
	sender_ip.sin_addr.s_addr = fwa->senderIpAddress;
	target_ip = sender_ip;
	target_ip.sin_addr.s_addr = fwa->targetIpAddress;
	
	bzero(&sender_hw, sizeof(sender_hw));
	sender_hw.sdl_len = sizeof(sender_hw);
	sender_hw.sdl_family = AF_LINK;
	sender_hw.sdl_type = IFT_IEEE1394;
	sender_hw.sdl_alen = FIREWIRE_ADDR_LEN;
	bcopy(&fwa->senderUniqueID, LLADDR(&sender_hw), FIREWIRE_ADDR_LEN);

	if(fwIpObj->arpCacheHandler(fwa))
		inet_arp_handle_input(ifp, ntohs(fwa->opcode), &sender_hw, &sender_ip, &target_ip);

	mbuf_free((mbuf_t)m);
}

void
firewire_inet_event(
	ifnet_t						ifp,
	__unused protocol_family_t	protocol,
	const struct kev_msg		*event)
{
	ifaddr_t	*addresses;
	
	if (event->vendor_code !=  KEV_VENDOR_APPLE ||
		event->kev_class != KEV_NETWORK_CLASS ||
		event->kev_subclass != KEV_DL_SUBCLASS ||
		event->event_code != KEV_DL_LINK_ADDRESS_CHANGED) 
	return;
	
	if (ifnet_get_address_list_family(ifp, &addresses, AF_INET) == 0) 
	{
		int i;
		
		for (i = 0; addresses[i] != NULL; i++) 
			inet_arp_init_ifaddr(ifp, addresses[i]);
		
		ifnet_free_address_list(addresses);
	}
}
#endif

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
