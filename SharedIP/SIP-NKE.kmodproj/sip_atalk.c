/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1997-2004 Apple Computer, Inc. All Rights Reserved */
/*
 * The Darwin (Apple Public Source) license specifies the terms
 * and conditions for redistribution.
 *
 * Support for networking from Classic:
 *  AppleTalk: OT and X have separate stacks and addresses
 *  IP: OT and X have separate stacks, but share the same IP address(es)
 * This is the AppleTalk support module
 */

#include <sys/kdebug.h>
#if KDEBUG

#define DBG_SPLT_BFCHK  DRVDBG_CODE(DBG_DRVSPLT, 0)
#define DBG_SPLT_APPND  DRVDBG_CODE(DBG_DRVSPLT, 1)
#define DBG_SPLT_MBUF   DRVDBG_CODE(DBG_DRVSPLT, 2)
#define DBG_SPLT_DUP    DRVDBG_CODE(DBG_DRVSPLT, 3)
#define DBG_SPLT_PAD    DRVDBG_CODE(DBG_DRVSPLT, 4)

#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/ndrv.h>
#include <net/kext_net.h>
#include <net/dlil.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <net/if_arp.h>
#include <net/kpi_interfacefilter.h>

#include "SharedIP.h"
#include "sip.h"

#include <netat/sysglue.h>
#include <netat/at_pcb.h>
#include <netat/at_var.h>
#include <machine/spl.h>

#include <sys/syslog.h>

static void atalk_detach(void*  cookie, ifnet_t ifp);
static int atalk_attach_protofltr(ifnet_t, struct blueCtlBlock *);


/*
 * Setup the filter (passed in) for the requested AppleTalk values.
 *  Verifying that it's OK to do this is done by the caller once
 *  we're done.  Nothing here changes real state.
 */
__private_extern__ int
enable_atalk(struct BlueFilter *bf, void *data, struct blueCtlBlock *ifb)
{
    int retval;
	struct sockaddr_at atalk_sockaddr;
	ifnet_t	ifp;

    retval = copyin(CAST_USER_ADDR_T(data), &atalk_sockaddr,
                    sizeof (struct sockaddr_at));
    if (retval) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "enable_atalk: copyin for data=%x to atalk_sockaddr failed ret=%x\n",
            data, retval);
#endif
        return (-retval);
    }
    
    /* Record client's address */
    bf->BF_address       = atalk_sockaddr.sat_addr.s_net;
    bf->BF_node          = atalk_sockaddr.sat_addr.s_node;
#if SIP_DEBUG
    log(LOG_WARNING, "enable_atalk: SIP_PROTO_ATALK  Flags: %x, AT address %x.%x\n",
            bf->BF_flags, atalk_sockaddr.sat_addr.s_net,
            atalk_sockaddr.sat_addr.s_node);
#endif
    /*
     * Record X-side address, if any, for this interface
     * NB: for this to be really correct, we need change of
     *  address notification, so we can update this value.
     *  Can't just hang on to the 'ifa' or sockaddr pointer,
     *  because that might get thrown away in an update.
     */
	retval = sip_ifp(ifb, &ifp);
	if (retval)
		return -retval;
	
    bzero((caddr_t)&ifb->XAtalkAddr, sizeof(ifb->XAtalkAddr));
    
    /* Assume the first one is it, since AppleTalk only uses one */
	{
		ifaddr_t	*addr_list;
		int i;
		
		retval = ifnet_get_address_list(ifb->ifp, &addr_list);
		if (retval)
			return -retval;
		
		for (i = 0; addr_list[i] != NULL; i++) {
			struct sockaddr_storage ss;
			errno_t	error;
			error = ifaddr_address(addr_list[i], (struct sockaddr*)&ss, sizeof(ss));
			if (error == 0 && ss.ss_family == AF_APPLETALK) {
				struct sockaddr_at *sat = (struct sockaddr_at*)&ss;
				ifb->XAtalkAddr = *sat;
			}
		}
		ifnet_free_address_list(addr_list);
	}

    /* Invoke protocol filter attachment */
    if ((retval = atalk_attach_protofltr(ifp, ifb)) != 0) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "SharedIP: enable_atalk: failed, ifp=%d retval=%d\n",
                    ifp, retval);
#endif
            return(retval);
    }
    
    return(BFS_ATALK);
}

/*
 * This filter function intercepts incoming packets being delivered to
 * AppleTalk (either from the interface or when a packet is sent by
 * MacOS X) and decide if they need to be sent up to the BlueBox.
 * Note: right now, handles both AT and AARP
 */

/* packets coming from interface? */
static int
atalk_infltr(
	void				*cookie,
	ifnet_t				ifnet_ptr,
	protocol_family_t   orig_protocol,
	mbuf_t				*m_orig,
	char				**etherheader)

{
    struct blueCtlBlock *ifb = cookie;
    struct BlueFilter *bf;
    unsigned char *p;
    unsigned short *s;
    mbuf_t m0, m = *m_orig;
	protocol_family_t protocol = orig_protocol;
	struct ether_header *eh = (struct ether_header*)*etherheader;
	
	/*
	 * The demux might not match this packet if AppleTalk isn't attached.
	 * If the demux didn't match the packet, lets see if we can.
	 */
	if (protocol == 0) {
		u_int32_t			*ulp;
		/*
		 * We need to verify this is AppleTalk.
		 * This could happen if AppleTalk is not enabled in X.
		 */
		if (ntohs(eh->ether_type) > ETHERMTU)
			return 0;
		
		if (mbuf_len(*m_orig) < 8) {
			return 0;
		}
		
		/* Check that this is a SNAP packet */
		ulp = mbuf_data(*m_orig);
		if (((ulp[0]) & htonl(0xFFFFFF00)) != ntohl(0xaaaa0300)) {
			return 0;
		}
		
		/* Check if this is AppleTalk or AARP */
		if (((ulp[0] & ntohl(0x000000ff)) == ntohl(0x00000008) &&
			 (ulp[1] == ntohl(0x0007809b))) || ((ulp[1] == ntohl(0x000080f3)) &&
			 (ulp[0] & ntohl(0x000000ff)) == ntohl(0x00000000)))
		{
			protocol = PF_APPLETALK;
		}
	}

	/* If this isn't an AppleTalk packet, we don't want it */
	if (protocol != PF_APPLETALK)
		return 0;
	
	/* If the packet is from the local address, this is something we injected, ignore it */
	if (bcmp(ifnet_lladdr(ifnet_ptr), eh->ether_shost, sizeof(eh->ether_shost)) == 0)
		return 0;

    MDATA_ETHER_START(m);

    s = (unsigned short *)*etherheader;
    p = (unsigned char *)*etherheader;

    /* Check SNAP header for EtherTalk packets */
	/* LOCK */
    bf = &ifb->filter[BFS_ATALK];  /* find AppleTalk filter */

#if SIP_DEBUG_FLOW
    if (!bf->BF_flags)
       	log(LOG_WARNING, "atalk_infltr: p0=%x %x, %x, ... %x, net/node %x.%x\n",
		 p[0],s[6], s[7], s[10], s[13], p[30]);
#endif

    if (bf->BF_flags & SIP_PROTO_ENABLE) {
        if (((p[0] & 0x01) == 0) &&
            (p[17] == 0x08) &&
            (ntohl(*((unsigned long*)&p[18])) == 0x0007809bL)) {
            if (bf->BF_flags & SIP_PROTO_RCV_FILT) {
                if ((ntohs(s[13]) == bf->BF_address &&
                        p[30] == bf->BF_node)) {
#if SIP_DEBUG_FLOW
                    log(LOG_WARNING, "atalk_infltr: Filter match %x.%x ! ifb=%x so=%x m=%x m_flags=%x\n",
                            s[13], p[30], ifb, ifb->ifb_so, m, m->m_flags);
#endif
                    blue_inject(ifb, m);
                    return(EJUSTRETURN); /* packet swallowed by bbox*/
                } else {
                    /* puts the packet back together as expected by X*/
                    MDATA_ETHER_END(m);
#if SIP_DEBUG_FLOW
                    log(LOG_WARNING, "atalk_infltr: direct packet not matched: ifb=%x m=%x %x.%x\n",
                                    ifb, m, s[13], p[30]);
#endif
                    return(0); /* filtering and no match, let MacOS X have it */
                }
            }
#if SIP_DEBUG
            log(LOG_WARNING, "atalk_infltr: SIP_PROTO_RCV_FILT == FALSE\n");
#endif
		}
	}
	/* UNLOCK */
    
    /*
     * Either a multicast, or AARP or we don't filter on node address.
     * duplicate so both X and Classic receive the packet.
     * This is important so both can defend their addresses.
     * NOTE: when no filter is set (ie startup) Blue will receive all
     *       AppleTalk traffic. Fixes problem when Blue AT is starting
     *	 up and has no node/net hint.
     */
	
	if (orig_protocol != 0) {
		
		if (mbuf_dup(m, M_NOWAIT, &m0) != 0) {
#if SIP_DEBUG_FLOW
			log(LOG_WARNING, "atalk_infltr: m_dup failed\n");
#endif
			ifb->no_bufs1++;
			/* puts the packet back together as expected */
			MDATA_ETHER_END(m);
			return(0); /* MacOS X will still get the packet if it needs to*/
		}
#if SIP_DEBUG_FLOW
		log(LOG_WARNING, "atalk_infltr: inject for bluebox p0=%x m0=%x m0->m_flags=%x so=%x \n",
		p[0], m0,  m0->m_flags, ifb->ifb_so);
#endif
		MDATA_ETHER_END(m);
	}
	else {
		m0 = m;
		m = NULL;
	}
    blue_inject(ifb, m0);

    /* this is for MacOS X, DLIL will hand the mbuf to AppleTalk */
    /* puts the packet back together as expected */

    return m ? 0 : EJUSTRETURN;
}

/* Packets coming from X AppleTalk stack */
static
int  atalk_outfltr(
	void					*cookie,
	ifnet_t					ifnet_ptr,
	protocol_family_t		protocol,
	mbuf_t					*m_orig)
{
    struct blueCtlBlock *ifb = cookie;
    struct BlueFilter *bf;
    unsigned char *p;
    mbuf_t m0, m = *m_orig;
    int total;
    int	reqlen = 17;
	
	if (protocol != PF_APPLETALK)
		return 0;

    /* the following is needed if the packet proto headers
     * are constructed using several mbufs (can happen with AppleTalk)
     */
    if (reqlen > mbuf_pkthdr_len(m))
        return(1);
    while ((reqlen > mbuf_len(m)) && mbuf_next(m)) {
        total = mbuf_len(m) + mbuf_len(mbuf_next(m));
		if (mbuf_pullup(m_orig, min(reqlen, total)) != 0)
			return -1;
		m = *m_orig;
    }
	
	/* LOCK */
    bf = &ifb->filter[BFS_ATALK];  /* find AppleTalk filter */
    p = (unsigned char *)mbuf_data(m);
    /*
	 * See if we're filtering Appletalk.
     * We already know it's an AppleTalk packet
     */
    if (bf->BF_flags & SIP_PROTO_ENABLE) {
        if ((p[0] & 0x01) == 0) {
            if (bf->BF_flags & SIP_PROTO_RCV_FILT)
            {
                /* Check for AppleTalk packet for Classic */
                u_int32_t	snap1 = ntohl(*(u_int32_t*)(p + 14));
                u_int32_t	snap2 = ntohl(*(u_int32_t*)(p + 18));
                if ((snap1 == 0xaaaa0308) && (snap2 == 0x0007809B))
                {
                    if ((ntohs(*(u_int16_t*)(p + 26)) == bf->BF_address &&
                        p[30] == bf->BF_node))
                    {
						/* UNLOCK */
						blue_inject(ifb, m);
                        *m_orig = 0;
                        return(EJUSTRETURN); /* packet swallowed by bbox*/
                    }
                }
                else if ((snap1 == 0xaaaa0300) && (snap2 == 0x000080F3) &&
                         (mbuf_len(m) >= 36))
                {
                    /* AARP */
                    /* Network number is at odd offset, copy it out */
                    u_int16_t	net = ((u_int16_t)p[47]) << 8 | p[48];
                    
                    if ((net == bf->BF_address) &&
                        (p[49] == bf->BF_node))
                    {
						/* UNLOCK */
						blue_inject(ifb, m);
                        *m_orig = 0;
                        return(EJUSTRETURN); /* packet swallowed by bbox*/
                    }
                }
            }
			/* UNLOCK */
            
            /* Unicast packet, not destined for Classic, just return 0 */
            return 0;
        }
		/* UNLOCK */
        /*
        * Either a multicast, or AARP or we don't filter on node address.
        * duplicate so both X and Classic receive the packet.
        */
        if (mbuf_dup(m, M_NOWAIT, &m0) != 0)
        {
#if SIP_DEBUG_FLOW
            log(LOG_WARNING, "atalk_outfltr: m_dup failed\n");
#endif
            *m_orig = m;
			
            /* Packet will still be sent */
            return(0);
        }
		blue_inject(ifb, m0);
    }
    /* this packet should continue out */
    *m_orig = m;
    return (0);
}


/*
 * This function attaches a protocol filter between the AppleTalk stack
 *  and the specified interface;
 * Note: use the blueCtlBlock pointer as the "cookie" passed when handling
 *  packets.
 */
static int
atalk_attach_protofltr(ifnet_t ifp, struct blueCtlBlock *ifb)
{
    int retval=0;
    struct iff_filter atalk_pfilter;
	bzero(&atalk_pfilter, sizeof(atalk_pfilter));
	ifb_reference(ifb);
	atalk_pfilter.iff_cookie = ifb;
	atalk_pfilter.iff_name = "com.apple.nke.SharedIP";
	atalk_pfilter.iff_protocol = 0; /* Allows us to capture AppleTalk even when X's AppleTalk isn't on */
	atalk_pfilter.iff_input = atalk_infltr;
	atalk_pfilter.iff_output = atalk_outfltr;
	atalk_pfilter.iff_detached = atalk_detach;

    /* Note: this assume the folowing here:
	- if AppleTalk X is already up and running, get it's home port and
  	  on it.
	- if not, we ourselves.
        - AT and AARP share the same DLIL tag (temporary)
     */
    if (ifb->atalk_proto_filter) {
		/* make sure to deregister the current filter first */
        retval= atalk_stop(ifb);
		if (ifb->atalk_proto_filter) {	
			// The old filter hasn't actually been detached yet :(
			retval = EEXIST;
		}
	}
    if (retval)
        return (retval);
	retval= iflt_attach(ifp, &atalk_pfilter, &ifb->atalk_proto_filter);
#if SIP_DEBUG
   log(LOG_WARNING, "atalk_attach_protofilter: filter_id=%d retval=%d\n",
	   ifb->atalk_proto_filter, retval);
#endif
   return (retval);

}

/*
 * TEMP! Disable the guts of this function; need another, non-global
 *  way of dealing with this interface's AppleTalk address.
 *
 * We use a global from X-side AppleTalk; if it's not running, flags
 *  are NULL.  It's only read, not written, so there's little chance
 *  of MP problems.  Exception: need to protect its use during this
 *  chunk of code, to avoid mid-operation surprises.
 */
 /* Packets from Blue to net & X stack?
  * When we exit, if we return 0, the mbuf will be sent to the driver.
  * If we return, EJUSTRETURN, no packet will be sent. To send the packet
  * to the X side, we have to use dlil_input. If the packet is
  * to be sent to both, we must duplicate the mbuf and send the duplicate
  * to dlil_input.
  */
__private_extern__ int
si_send_eth_atalk(
	mbuf_t *m_orig,
	struct blueCtlBlock *ifb,
	ifnet_t	ifp)
{
    mbuf_t m1 = NULL, m;
    unsigned char *p;
    unsigned short *s;
    unsigned long *l;
    struct sockaddr_at *sap;

    m = *m_orig;		/* Assumption: 'm' is not changed below! */
    p = mbuf_data(m);   /* Point to destination media addr */
    s = (unsigned short *)p;
    /*
     * flags entry is non-null if X's Appletalk is up (???)
     * Otherwise, it's null, and we just exit.
     */
    sap = &ifb->XAtalkAddr;
    if (sap->sat_family) {	/* X has an AppleTalk address */
        if (p[0] & 0x01) {  /* Multicast/broadcast, send to both */
			
#if SIP_DEBUG_FLOW
            log(LOG_WARNING, "si_send: broadcast! inject m=%x m1=%x..\n", m, m1);
#endif
            if (mbuf_dup(m, M_NOWAIT, &m1) == 0) {
                /* find the interface of the X AppleTalk stack to inject this packet on? */
				mbuf_pkthdr_setrcvif(m1, ifp);
				mbuf_pkthdr_setheader(m1, mbuf_data(m1));
                MDATA_ETHER_END(m1);
                ifnet_input(ifp, m1, NULL);
            } else
                ifb->no_bufs2++;
            return(0);
        }
        l = (unsigned long *)&s[8];
#if SIP_DEBUG_FLOW
        log(LOG_WARNING, "si_send packet: p0=%x len=%x, m_len=%x snap? %x %x:%x, Add:%x.%x\n",
                p[0], m->m_len, s[6], s[7], *l, s[10], s[13], p[30]);
#endif
        /* Verify SNAP header is AppleTalk */
        if (ntohl(*l) == 0x03080007 && ntohs(s[10]) == 0x809b) {
            if (ntohs(s[13]) == sap->sat_addr.s_net &&
                p[30] == sap->sat_addr.s_node)
            {
#if SIP_DEBUG_FLOW
                log(LOG_WARNING, "si_send: packet is for X side %x.%x m=%x\n",
                        s[13], p[30], m);
#endif
                MDATA_ETHER_END(m);
                /* send this packet to the X AT stack, not the network */
				mbuf_pkthdr_setrcvif(m, ifp);
				mbuf_pkthdr_setheader(m, mbuf_data(m));
                MDATA_ETHER_END(m);
                ifnet_input(ifp, m, NULL);
                return(EJUSTRETURN);
            } else
                return(0);
        } else if (ntohl(*l) == 0x03000000 && ntohs(s[10]) == 0x80f3) {
            /* AARP SNAP (0x00000080F3) */
            /* AARP pkts aren't net-addressed */
            /* Send to both X AT and network */
#if SIP_DEBUG_FLOW
            log(LOG_WARNING, "si_send:  AARP m=%x m1=%x send to X\n", m, m1);
#endif
            if (mbuf_dup(m, M_NOWAIT, &m1) == 0)
            {
				mbuf_pkthdr_setrcvif(m1, ifp);
				mbuf_pkthdr_setheader(m1, mbuf_data(m1));
                MDATA_ETHER_END(m1);
                ifnet_input(ifp, m1, NULL);
            } else
                ifb->no_bufs2++;
            
            return(0);
        }
#if SIP_DEBUG_FLOW
        log(LOG_WARNING, "si_send: not an Atalk nor AARP...\n");
#endif
        return(0);
    }
    return(0);
}

int
atalk_stop(struct blueCtlBlock *ifb)
{
    int retval = 0;

    if (ifb == NULL)
        return(0);

    ifb->atalk_stopping = 1;
    if (ifb->atalk_proto_filter) {
#if SIP_DEBUG
        log(LOG_WARNING, "atalk_stop: deregister AppleTalk proto filter tag=%d\n",
            ifb->atalk_proto_filter);
#endif
		if (ifb->atalk_proto_filter) {
			iflt_detach(ifb->atalk_proto_filter);
		}
     }
     
     ifb->atalk_stopping = 0;
     return(retval);
}

/* Handle our filter being detached */
static void
atalk_detach(
	void*	cookie,
	__unused ifnet_t	ifp)
{
    /* Assume the interface has been detached */
    struct blueCtlBlock *ifb = (struct blueCtlBlock*)cookie;
    
    ifb->atalk_proto_filter = 0;
	
	ifb_release(ifb);
}
