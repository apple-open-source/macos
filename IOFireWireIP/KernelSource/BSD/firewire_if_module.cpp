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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/dlil.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>	/* For M_LOOP */

#include <sys/socketvar.h>

#include <net/dlil.h>

#include "firewire.h"
#include <if_firewire.h>
}
#include "IOFireWireIP.h" 

// General stuff from if_ethersubr.c - may not need some of it
static u_long lo_dlt = 0;
static u_long ivedonethis = 0;

#define IFP2AC(IFP) ((struct arpcom *)IFP)

struct fw_desc {
    u_int16_t		type;		/* Type of protocol stored in data */
    struct if_proto *proto;		/* Protocol structure */
    u_long			data[2];	/* Protocol data */
};

#define FIREWIRE_DESC_BLK_SIZE (10)
#define MAX_INTERFACES 50

//
// Statics for demux module
//
struct firewire_desc_blk_str {
    u_long  n_max_used;
    u_long	n_count;
    struct fw_desc  *block_ptr;
};

static struct firewire_desc_blk_str firewire_desc_blk[MAX_INTERFACES];

extern void firewire_arpwhohas __P((struct arpcom *ac, struct in_addr *addr));
extern int  firewire_attach_inet6(struct ifnet *ifp, u_long *dl_tag);
extern void	firewire_arp_ifinit __P((struct arpcom *, struct ifaddr *));

////////////////////////////////////////////////////////////////////////////////
//
// firewire_del_proto
//
// IN: struct if_proto *proto, u_long dl_tag 
// 
// Invoked by : 
//  dlil_detach_protocol calls this funcion
// 
// Release all descriptor entries owned by this dl_tag (there may be several).
// Setting the type to 0 releases the entry. Eventually we should compact-out
// the unused entries.
//
////////////////////////////////////////////////////////////////////////////////
static
int  firewire_del_proto(struct if_proto *proto, u_long dl_tag)
{
    struct fw_desc*	ed = firewire_desc_blk[proto->ifp->family_cookie].block_ptr;
    u_long	current = 0;
    int found = 0;
    
    for (current = firewire_desc_blk[proto->ifp->family_cookie].n_max_used;
            current > 0; current--) {
        if (ed[current - 1].proto == proto) {
            found = 1;
            ed[current - 1].type = 0;
            
            if (current == firewire_desc_blk[proto->ifp->family_cookie].n_max_used) {
                firewire_desc_blk[proto->ifp->family_cookie].n_max_used--;
            }
        }
    }
    
    return found;
 }

////////////////////////////////////////////////////////////////////////////////
//
// firewire_add_proto
//
// IN: struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag
// 
// Invoked by : 
//  dlil_attach_protocol calls this funcion
// 
//
////////////////////////////////////////////////////////////////////////////////
static int
firewire_add_proto(struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag)
{
   struct dlil_demux_desc	*desc;
   struct fw_desc			*ed;
//   struct fw_desc			*last;
   
// char		*current_ptr;
// short	total_length;
   
// u_long	*bitmask;
// u_long	*proto_id;
   u_long	i;
// u_long	block_count;
   u_long	*tmp;

   
    TAILQ_FOREACH(desc, desc_head, next) {
        switch (desc->type) {
            case DLIL_DESC_RAW:
                if (desc->variants.bitmask.proto_id_length == 0)
                    break;
            
            default:
                firewire_del_proto(proto, dl_tag);
                return EINVAL;
        }
    
//    restart:
        ed = firewire_desc_blk[proto->ifp->family_cookie].block_ptr;
        
        /* Find a free entry */
        for (i = 0; i < firewire_desc_blk[proto->ifp->family_cookie].n_count; i++) {
            if (ed[i].type == 0) {
                break;
            }
        }
        
        if (i >= firewire_desc_blk[proto->ifp->family_cookie].n_count) {
            u_long	new_count = FIREWIRE_DESC_BLK_SIZE +
										firewire_desc_blk[proto->ifp->family_cookie].n_count;
            tmp = (u_long*)_MALLOC((new_count * (sizeof(*ed))), M_IFADDR, M_WAITOK);
			
            if (tmp  == 0) {
				//
				// Remove any previous descriptors set in the call.
				//
                firewire_del_proto(proto, dl_tag);
                return ENOMEM;
            }
            
            bzero(tmp, new_count * sizeof(*ed));
            bcopy(firewire_desc_blk[proto->ifp->family_cookie].block_ptr, 
									tmp, firewire_desc_blk[proto->ifp->family_cookie].n_count * sizeof(*ed));
									
            FREE(firewire_desc_blk[proto->ifp->family_cookie].block_ptr, M_IFADDR);
            firewire_desc_blk[proto->ifp->family_cookie].n_count = new_count;
            firewire_desc_blk[proto->ifp->family_cookie].block_ptr = (struct fw_desc*)tmp;
        }
        
        // Bump n_max_used if appropriate
        if (i + 1 > firewire_desc_blk[proto->ifp->family_cookie].n_max_used) {
            firewire_desc_blk[proto->ifp->family_cookie].n_max_used = i + 1;
        }
        
        ed[i].proto	= proto;
        ed[i].data[0] = 0;
        ed[i].data[1] = 0;
        
        switch (desc->type) {
            case DLIL_DESC_RAW:
                /* 2 byte ethernet raw protocol type is at native_type */
                /* protocol is not in network byte order */
                ed[i].type = DLIL_DESC_ETYPE2;
                ed[i].data[0] = htons(*(u_int16_t*)desc->native_type);
                break;
        }
    }
    
    return 0;
} 

////////////////////////////////////////////////////////////////////////////////
//
// firewire_shutdown
//
// IN: 
// 
// Invoked by : 
//  dlil_attach_protocol calls this funcion
// 
//
////////////////////////////////////////////////////////////////////////////////
static
int  firewire_shutdown()
{
	log(LOG_DEBUG, "firewire shutdown called\n");
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_demux
//
// IN: struct ifnet *ifp,struct mbuf  *m,char *frame_header,
//	   struct if_proto **proto
// 
// Invoked by : 
//  dlil_input_packet()
// 
////////////////////////////////////////////////////////////////////////////////
int firewire_demux(struct ifnet *ifp,struct mbuf  *m,char *frame_header,struct if_proto **proto)
{
    register struct firewire_header *eh = (struct firewire_header *)frame_header;
	u_short ether_type;
    u_int16_t type = DLIL_DESC_ETYPE2;
    u_long i = 0;
    u_long max = firewire_desc_blk[ifp->family_cookie].n_max_used;
    struct fw_desc *ed = firewire_desc_blk[ifp->family_cookie].block_ptr;

    if(eh == NULL)
    {
        // log(LOG_DEBUG, "firewire_demux %d \n", __LINE__);
        return EINVAL;
    }

	ether_type = eh->ether_type;
    
    /* 
     * Search through the connected protocols for a match. 
     */
    
    switch (type) {
        case DLIL_DESC_ETYPE2:
            for (i = 0; i < max; i++) {
                if ((ed[i].type == type) && (ed[i].data[0] == ether_type)) {
                    *proto = ed[i].proto;
                    return 0;
                }
            }
            break;
    }
    
    return ENOENT;
}			



////////////////////////////////////////////////////////////////////////////////
//
// firewire_frameout
//
// IN:	struct ifnet *ifp,struct mbuf **m
// IN:  struct sockaddr *ndest - contains the destination IP Address 
// IN:	char *edst - filled by firewire_arpresolve function in if_firewire.c
// IN:  char *ether_type 
//
// Invoked by : 
//  dlil.c for dlil_output, Its called after inet_firewire_pre_output
//
// Encapsulate a packet of type family for the local net.
// Use trailer local net encapsulation if enough data in first
// packet leaves a multiple of 512 bytes of data in remainder.
// Assumes that ifp is actually pointer to arpcom structure.
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_frameout(register struct ifnet	*ifp, struct mbuf **m, 
					struct sockaddr *ndest, char *edst, char *ether_type)
{
	register struct firewire_header *fwh;
	int hlen;	/* link layer header lenght */
	struct arpcom *ac = IFP2AC(ifp);

	hlen = FIREWIRE_HDR_LEN;

	// log(LOG_DEBUG,"fw: ac->ac_enaddr %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n",
    //        ac->ac_enaddr[0], ac->ac_enaddr[1], ac->ac_enaddr[2], ac->ac_enaddr[3],
    //        ac->ac_enaddr[4], ac->ac_enaddr[5], ac->ac_enaddr[6], ac->ac_enaddr[7]);

	// log(LOG_DEBUG,"fw: edst %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n",
    //          edst[0], edst[1], edst[2], edst[3],
    //          edst[4], edst[5], edst[6], edst[7]);


	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	if ((ifp->if_flags & IFF_SIMPLEX) &&
	    ((*m)->m_flags & M_LOOP)) 
	{
	    if (lo_dlt == 0) 
            dlil_find_dltag(APPLE_IF_FAM_LOOPBACK, 0, PF_INET, &lo_dlt);

	    if (lo_dlt) 
		{
            if ((*m)->m_flags & M_BCAST)
			{
                struct mbuf *n = m_copy(*m, 0, (int)M_COPYALL);
                if (n != NULL)
                    dlil_output(lo_dlt, n, 0, ndest, 0);
            } 
            else 
            {
				if (bcmp(edst,  ac->ac_enaddr, FIREWIRE_ADDR_LEN) == 0) 
				{
                    dlil_output(lo_dlt, *m, 0, ndest, 0);
                    return EJUSTRETURN;
                }
            }
	    }
	}

	//
	// Add local net header.  If no space in first mbuf,
	// allocate another.
	//
	M_PREPEND(*m, sizeof(struct firewire_header), M_DONTWAIT);
	if (*m == 0) {
	    return (EJUSTRETURN);
	}

	//
	// Lets put this intelligent here into the mbuf 
	// so we can demux on our output path
	//
	fwh = mtod(*m, struct firewire_header *);
	(void)memcpy(&fwh->ether_type, ether_type,sizeof(fwh->ether_type));
	memcpy(fwh->ether_dhost, edst, FIREWIRE_ADDR_LEN);
	(void)memcpy(fwh->ether_shost, ac->ac_enaddr, sizeof(fwh->ether_shost));
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_add_if
//
// IN:	struct ifnet *ifp
//
// Invoked by : 
//    dlil_if_attach calls this function
//
////////////////////////////////////////////////////////////////////////////////
static
int  firewire_add_if(struct ifnet *ifp)
{
    u_long  i;

    ifp->if_framer = firewire_frameout;
    ifp->if_demux  = firewire_demux;
    ifp->if_event  = 0;

    for (i=0; i < MAX_INTERFACES; i++)
        if (firewire_desc_blk[i].n_count == 0)
            break;

    if (i == MAX_INTERFACES)
        return ENOMEM;

    firewire_desc_blk[i].block_ptr = (struct fw_desc*)_MALLOC(FIREWIRE_DESC_BLK_SIZE * sizeof(struct fw_desc),
                                            M_IFADDR, M_WAITOK);
    if (firewire_desc_blk[i].block_ptr == 0)
        return ENOMEM;

    firewire_desc_blk[i].n_count = FIREWIRE_DESC_BLK_SIZE;
    bzero(firewire_desc_blk[i].block_ptr, FIREWIRE_DESC_BLK_SIZE * sizeof(struct fw_desc));

    ifp->family_cookie = i;
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_del_if
//
// IN:	struct ifnet *ifp
//
// Invoked by : 
//    dlil_if_dettach calls this function
//
////////////////////////////////////////////////////////////////////////////////
static
int  firewire_del_if(struct ifnet *ifp)
{
    if ((ifp->family_cookie < MAX_INTERFACES) &&
        (firewire_desc_blk[ifp->family_cookie].n_count))
    {
        FREE(firewire_desc_blk[ifp->family_cookie].block_ptr, M_IFADDR);
        firewire_desc_blk[ifp->family_cookie].block_ptr = NULL;
        firewire_desc_blk[ifp->family_cookie].n_count = 0;
        firewire_desc_blk[ifp->family_cookie].n_max_used = 0;
        return 0;
    }
    else
        return ENOENT;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_ifmod_ioctl
//
// IN:	struct ifnet *ifp
//
// Invoked by : 
//    dlil_ioctl calls this function, all ioctls are handled at 
//    firewire_inet_prmod_ioctl
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_ifmod_ioctl(struct ifnet *ifp, u_long cmd, caddr_t  data)
{
    struct ifaddr *ifa = (struct ifaddr *) data;
//	struct in_ifaddr *ia = (struct in_ifaddr *)data;
    struct arpcom *ac = (struct arpcom *) ifp;
//	u_long dl_tag;
    IOFireWireIP *fwIpObj = (IOFireWireIP*)ifp->if_softc;
	int err = EOPNOTSUPP;
	
    switch (cmd) 
	{
		//
		// Should be or will be moved after right fixes in the current
		// way of handling the protocol attach routines for
		// as soon as we resolve the boot-up panic related
		// to IONetworkingFamily panic.
		//
        case SIOCSIFADDR:
            switch (ifa->ifa_addr->sa_family) 
			{
                case AF_INET:
					//log(LOG_DEBUG, "IOFireWireIP firewire_ifmod_ioctl AF_INET \n");
                    // Attach to the protocol interface
#ifdef FIREWIRETODO 
                    firewire_attach_inet(ifp, &dl_tag);
					if(ia)
						ia->ia_ifa.ifa_dlt = dl_tag;
#endif						
                    if (IA_SIN(ifa)->sin_addr.s_addr != 0)
                    {
                        // don't bother for 0.0.0.0
                        ac->ac_ipaddr = IA_SIN(ifa)->sin_addr;
						// Set the ip address in the link control block
						fwIpObj->setIPAddress(&IA_SIN(ifa)->sin_addr);
                        firewire_arpwhohas(ac, &IA_SIN(ifa)->sin_addr);
                    }
                    firewire_arp_ifinit(IFP2AC(ifp), ifa);
					break;
				
				case AF_INET6:
					//IOLog("IOFireWireIP IPV6 protocol attach \n");
#ifdef FIREWIRETODO 
					firewire_attach_inet6(ifp, &dl_tag);
					if(ia)
						ia->ia_ifa.ifa_dlt = dl_tag;
#endif
					break;
			}
	}

	return err;
}

static int  firewire_init_if(struct ifnet *ifp)
{
    register struct ifaddr *ifa;
    register struct sockaddr_dl *sdl;

    ifa = ifnet_addrs[ifp->if_index - 1];
    if (ifa == 0) 
	{
		log(LOG_DEBUG, "firewire_ifattach: no lladdr!\n");
		return -1;
    }
	
    sdl = (struct sockaddr_dl *)ifa->ifa_addr;
    sdl->sdl_type = IFT_IEEE1394;
	ifp->if_addrlen = FIREWIRE_ADDR_LEN;
    sdl->sdl_alen = ifp->if_addrlen;
    bcopy((IFP2AC(ifp))->ac_enaddr, LLADDR(sdl), ifp->if_addrlen);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_family_init
//
// IN:	NULL
//
// Invoked by : 
//    IOFWInterface::attachToDataLinkLayer 
//
////////////////////////////////////////////////////////////////////////////////
int firewire_family_init()
{
    int i;
	int ret;
    struct dlil_ifmod_reg_str  ifmod_reg;

    if (ivedonethis)
        return 0;

    ivedonethis = 1;
	
	bzero(&ifmod_reg, sizeof(struct dlil_ifmod_reg_str));

    ifmod_reg.init_if 	= firewire_init_if;
    ifmod_reg.add_if	= firewire_add_if;
    ifmod_reg.del_if	= firewire_del_if;
    ifmod_reg.add_proto	= firewire_add_proto;
    ifmod_reg.del_proto	= firewire_del_proto;
    ifmod_reg.ifmod_ioctl = firewire_ifmod_ioctl;
    ifmod_reg.shutdown    = firewire_shutdown;
 
	
	ret = dlil_reg_if_modules(APPLE_IF_FAM_FIREWIRE, &ifmod_reg);
    if (ret != 0) 
	{
		log(LOG_DEBUG, "WARNING: firewire_family_init -- Can't register if family modules %d\n", ret);
        return EIO;
    }

    for (i=0; i < MAX_INTERFACES; i++)
        firewire_desc_blk[i].n_count = 0;

    return 0;
}
