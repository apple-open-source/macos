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
/* Copyright (c) 1997-2000 Apple Computer, Inc. All Rights Reserved */
/*
 * The Darwin (Apple Public Source) license specifies the terms
 * and conditions for redistribution.
 *
 * Shared IP address support for Classic/OT
 * Supports networking from Classic:
 *  for AppleTalk, OT and X have separate stacks and addresses
 *  for IP, OT and X have separate stacks, but share the same IP address(es)
 * This is the NKE that makes it all happen.
 *
 * Justin Walker, 991005
 * Laurent Dumont, 991112
 * 000824: Adding support for the PPP device type (IP only)
 */

/*
 * Theory of operation:
 * - init hook just registers
 * - kernel code finds this (find_nke()) when client executes
 *   SO_NKE setsockopt().
 * - invokes 'create' hook, to set up plumbing
 * - SO_PROTO_REGISTER ioctl induces filter registration, depending
 *   on filter definitions
 * - Output from client is filtered with socket NKE; output from X
 *   is filtered by data link protocol output filters
 * - Input from chosen devices is filtered by data link protocol
 *   input filters
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
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/ndrv.h>
#include <sys/kpi_socketfilter.h>
#include <net/dlil.h>
#include <net/kpi_interfacefilter.h>
#include <netinet/in.h>		/* Needed for (*&^%$#@ arpcom in if_arp.h */
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <kern/thread.h>
#include <kern/locks.h>

#include "SharedIP.h"
#include "sip.h"

#include <sys/syslog.h>

static int si_initted = 0;
static  lck_mtx_t   *sip_mtx = 0;
static  lck_grp_t   *sip_lock_group;

static errno_t enable_filters(socket_t so, int reg_flags, void *data, struct blueCtlBlock *ifb);

static void
sip_unregistered(
	sflt_handle handle)
{
	/* It is now safe to unload... */
	si_initted = 0;
}

static errno_t
sip_attach(
	void		**cookie,
	socket_t	socket)
{
	struct blueCtlBlock *ifb = _MALLOC(sizeof(*ifb), M_PCB, M_WAITOK);
	int size = 0;
	int error = 0;
	
	DEBUG_MSG("sip_attach\n");
	
	if (ifb == NULL) {
		log(LOG_WARNING, "SharedIP can't attach to socket, malloc failed\n");
		return ENOMEM;
	}
	
	bzero(ifb, sizeof(*ifb));

	ifb_reference(ifb);
	
	*(struct blueCtlBlock **)cookie = ifb;
	ifb->ifb_so = socket;
	
	/* Increase the receive socket buffer size - this should really be done by Classic */
	for (size = 131072; size >= 16384; size /= 2) {
		error = sock_setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
		if (error == 0)
			break;
	}
	
	if (error) {
		ifb_release(ifb);
		log(LOG_WARNING, "SharedIP couldn't increase the receive socket buffer size.\n");
		DEBUG_MSG("/sip_attach - error\n");
		return error;
	}
	
	DEBUG_MSG("/sip_attach\n");
	return 0;
}

static void
sip_detach(
	void		*cookie,
	socket_t	socket)
{
	struct blueCtlBlock *ifb = cookie;
	int error = 0;
	
	DEBUG_MSG("sip_detach\n");
	// What do we need to cleanup???
	error = atalk_stop(ifb);
	if (error) {
		log(LOG_WARNING, "SharedIP - atalk_stop returned %d\n", error);
	}
	error = ipv4_stop(ifb);
	if (error) {
		log(LOG_WARNING, "SharedIP - ipv4_stop returned %d\n", error);
	}
	ifb_release(ifb);
	DEBUG_MSG("/sip_detach\n");
}

__private_extern__ errno_t
sip_ifp(
	struct blueCtlBlock *ifb,
	ifnet_t *ifp_out)
{
	errno_t error = 0;
	
	/* Attempt to find the ifp the PF_NDRV socket is bound to */
	if (ifb->ifp == NULL) {
		sip_lock();
		if (ifb->ifp == NULL) {
			struct sockaddr_ndrv sandrv;
			
			error = sock_getsockname(ifb->ifb_so, (struct sockaddr*)&sandrv, sizeof(sandrv));
			
			if (error == 0) {
				error = ifnet_find_by_name(sandrv.snd_name, &ifb->ifp);
			}
			
			if (error) {
				if (ifb->noifpnotify == 0) {
					ifb->noifpnotify = 1;
					log(LOG_WARNING, "SharedIP - sip_sock_out couldn't find the ifp for the socket\n");
				}
			}
		}
		sip_unlock();
	}
	
	*ifp_out = ifb->ifp;
	
	if (ifb->ifp == NULL && error == 0)
		return ENOENT;
	
	return error;
}

static errno_t
sip_sock_out(
	void					*cookie,
	socket_t				so,
	const struct sockaddr   *to,
	mbuf_t					*data,
	mbuf_t					*control,
	sflt_data_flag_t		flags)
{
	ifnet_t				ifp;
	struct blueCtlBlock *ifb = cookie;
	errno_t				error = 0;
	static u_char		etherbroadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	
	if (data == NULL || *data == NULL) {
		return 0;
	}
	
	if (sip_ifp(ifb, &ifp) != 0)
		return 0;
	
	if (ifb->atalk_proto_filter == 0 &&
		ifb->ip_filter == 0) {
		return 0; /* Used to be EJUSTRETURN and we'd free the packet. Eeek! */
	}
	
	switch (ifnet_type(ifp)) {
		case IFT_ETHER: {
			struct ether_header *eh;
			
			if (mbuf_len(*data) < ETHER_HDR_LEN + 4)
				break;
			
			eh = mbuf_data(*data);
			
			if (eh->ether_dhost[0] & 0x01) {
				if (bcmp(eh->ether_dhost, etherbroadcast, sizeof(etherbroadcast)) == 0) {
					mbuf_setflags(*data, mbuf_flags(*data) | MBUF_BCAST);
				}
				else {
					mbuf_setflags(*data, mbuf_flags(*data) | MBUF_MCAST);
				}
			}
			
			if (eh->ether_type == ETHERTYPE_IP ||
				eh->ether_type == ETHERTYPE_ARP) {
				error = si_send_eth_ipv4(data, ifb, ifp);
			}
			else if (eh->ether_type <= ETHERMTU) {
				u_int32_t issnap = *(u_int32_t*)(eh + 1);
				
				if ((issnap & htonl(0xFFFFFF00)) == htonl(0xaaaa0300)) {
					/* This is a SNAP packet, it's probably AppleTalk */
					error = si_send_eth_atalk(data, ifb, ifp);
				}
			}
		}
		break;
		
		case IFT_PPP: {
			u_int16_t	*p;
			p = mbuf_data(*data);
			if (*p == htons(0x21)) {
				error = si_send_ppp_ipv4(data, ifb, ifp);
			}
		}
		break;
	}
	/* Unlock? */
	return error;
}

static errno_t
sip_setoption(
	void		*cookie,
	socket_t	so,
	sockopt_t   opt)
{
	int error;
	int handled = 0;
	
	DEBUG_MSG("sip_setoption\n");
	
	switch(sockopt_name(opt)) {
		case SO_PROTO_REGISTER: {
			struct sopt_proto_register proto_register;
			handled = 1;
			error = sockopt_copyin(opt, &proto_register, sizeof(proto_register));
			if (error)
				break;
			
			error = enable_filters(so, proto_register.reg_flags, proto_register.reg_sa, cookie);
			DEBUG_MSG("enable_filters(so = 0x%X, flags = 0x%x, blah, ifb = 0x%x) returned %d\n",
				so, proto_register.reg_flags, cookie, error);
		}
		break;
		
		case SO_PORT_RESERVE:
		case SO_PORT_RELEASE: {
			struct sopt_shared_port_param port_param;
			handled = 1;
			
			error = sockopt_copyin(opt, &port_param, sizeof(port_param));
			if (error)
				break;
			
			error = ipv4_control(so, &port_param, cookie, sockopt_name(opt));
			if (error == 0)
				error = sockopt_copyout(opt, &port_param, sizeof(port_param));
			
			if (error != 0) {
				// Try to cleanup
				ipv4_control(so, &port_param, cookie, SO_PORT_RELEASE);
			}
		}
		break;
		
		case SO_PORT_LOOKUP:
			handled = 1;	/* We own this socket option */
			error = EOPNOTSUPP;
			break;
		
		default:
			error = 0;
			break;
	}
	
	if (error == 0 && handled)
		error = EJUSTRETURN;
	
	DEBUG_MSG("/sip_setoption\n");
	return error;
}



int
SIP_start()
{
	int error = 0;
	struct sflt_filter  sock_filter;
	
	/* Allocate a lock */
	{
		lck_grp_attr_t	*grp_attrib = 0;
		lck_attr_t		*lck_attrib = 0;
		grp_attrib = lck_grp_attr_alloc_init();
		lck_grp_attr_setdefault(grp_attrib);
		sip_lock_group = lck_grp_alloc_init("SharedIP lock", grp_attrib);
		lck_grp_attr_free(grp_attrib);
		lck_attrib = lck_attr_alloc_init();
		lck_attr_setdefault(lck_attrib);
		sip_mtx = lck_mtx_alloc_init(sip_lock_group, lck_attrib);
		lck_attr_free(lck_attrib);
	}
	
	DEBUG_MSG("SIP_start\n");
	bzero(&sock_filter, sizeof(sock_filter));
	sock_filter.sf_handle = SharedIP_Handle;
	sock_filter.sf_flags = SFLT_PROG;
	sock_filter.sf_name = "com.apple.nke.SharedIP";
	sock_filter.sf_unregistered = sip_unregistered;
	sock_filter.sf_attach = sip_attach;
	sock_filter.sf_detach = sip_detach;
	sock_filter.sf_data_out = sip_sock_out;
	sock_filter.sf_setoption = sip_setoption;
	
	error = sflt_register(&sock_filter, PF_NDRV, SOCK_RAW, 0);
	if (error) {
		log(LOG_WARNING, "SharedIP can't attach socket filter - %d\n", error);
		return KERN_FAILURE;
	}

#if SIP_DEBUG
    log(LOG_WARNING, "SharedIP: SIP_start called successfully\n");
#endif

    si_initted = 1;
	DEBUG_MSG("/SIP_start\n");
	
    return KERN_SUCCESS;
}

static errno_t
enable_filters(
	socket_t so,
	int reg_flags,
	void *data,
	struct blueCtlBlock *ifb)
{
    struct BlueFilter Filter, *bf;
    int retval;

    Filter.BF_flags = reg_flags;
    if (reg_flags & SIP_PROTO_ATALK) {
		DEBUG_MSG("enable_filters - enabling AppleTalk for 0x%X\n", ifb);
        retval = enable_atalk(&Filter, data, ifb);
	}
    else if (reg_flags & SIP_PROTO_IPv4) {
		DEBUG_MSG("enable_filters - enabling IP for 0x%X\n", ifb);
        retval = enable_ipv4(&Filter, data, ifb);
	}
    else {
        log(LOG_WARNING, "SharedIP - enable_filter: unknown protocol requested so=%x kp=%x flags=%x\n",
                so, ifb, reg_flags);
        retval = -EPROTONOSUPPORT;
    }
    /*
        * 'retval' here is either:
        *  >= 0 (an index into the filter array in the ifb struct)
        *  <  0 (negative of a value from errno.h)
        */
    if (retval < 0) {
        return(-retval);
    }
    bf = &ifb->filter[retval];
    if (Filter.BF_flags & SIP_PROTO_ENABLE) {
        if ((bf->BF_flags & (SIP_PROTO_RCV_FILT)) ==
                SIP_PROTO_RCV_FILT) {
            log(LOG_WARNING, "enable_filters: SIP_PROTO_ENABLE -already set flags=%x\n",
                    bf->BF_flags);
            return(EBUSY);
        }
        *bf = Filter;
    } else if (Filter.BF_flags & SIP_PROTO_DISABLE) {
#if SIP_DEBUG
        log(LOG_WARNING, "enable_filters: SIP_PROTO_DISABLE flags=%x for addr %x.%x\n",
                bf->BF_flags, Filter.BF_address, Filter.BF_node);
#endif
        if (bf->BF_flags & SIP_PROTO_ENABLE)
            bf->BF_flags = 0;
        else {
            return(EINVAL);
        }
    }
    return(0);
}

/* Stop all Sip users, unload if that works */
int
SIP_stop()
{
	sflt_unregister(SharedIP_Handle);
	
	if (si_initted == 0) {
		lck_mtx_free(sip_mtx, sip_lock_group);
		sip_mtx = 0;
		lck_grp_free(sip_lock_group);
		sip_lock_group = 0;
	}
	
	/* if si_initted is not zero, the filter hasn't been unregistered yet */
    return si_initted ? KERN_FAILURE : KERN_SUCCESS;
}

/* blue_inject : append the filtered mbuf to the Blue Box socket and
                 notify BBox that there is something to read.
*/
static struct sockaddr_dl ndrvsrc = {sizeof (struct sockaddr_dl), AF_NDRV};

__private_extern__ int
blue_inject(struct blueCtlBlock *ifb, mbuf_t m)
{
	errno_t error;
	
    /* move packet from if queue to socket */
    /* !!!Fix this to work generically!!! */
    ndrvsrc.sdl_type = IFT_ETHER;
    ndrvsrc.sdl_nlen = 0;
    ndrvsrc.sdl_alen = 6;
    ndrvsrc.sdl_slen = 0;
    bcopy(mtodAtOffset(m, 6, void*), &ndrvsrc.sdl_data, ndrvsrc.sdl_alen);

    if (!ifb || !ifb->ifb_so) {
        log(LOG_WARNING, "SharedIP - blue_inject: no valid ifb for m=%x\n", m);
        mbuf_freem(m);
        return (-EINVAL); /* argh! */
    }
	error = sock_inject_data_in(ifb->ifb_so, (struct sockaddr*)&ndrvsrc, m, NULL, 0);
    if (error != 0) {
        ifb->full_sockbuf++;
#if SIP_DEBUG
        log(LOG_WARNING, "blue_inject: sock_inject_data_in returned %d for so=%x\n", error, ifb->ifb_so);
#endif
    } else {
#if SIP_DEBUG_FLOW
        log(LOG_WARNING, "blue_inject: call sorwakeup for so=%x\n",
                ifb->ifb_so);
#endif
        ifb->pkts_up++;
    }
	
	return error;
}

__private_extern__ int
sip_get_ether_addr(
	struct blueCtlBlock *ifb,
	char*   addr)
{
	errno_t error = 0;
	
	if (ifb->ifp == NULL || ifnet_type(ifb->ifp) != IFT_ETHER)
		return EINVAL;
	if (ifb->dev_media_addr == NULL) {
		ifaddr_t	*addr_list;
		int			i = 0;
		error = ifnet_get_address_list(ifb->ifp, &addr_list);
		if (error != 0)
			return error;
		
		for (i = 0; addr_list[i] != NULL; i++) {
			struct sockaddr_storage ss;
			error = ifaddr_address(addr_list[i], (struct sockaddr*)&ss, sizeof(ss));
			if (error == 0 && ss.ss_family == AF_LINK) {
				struct sockaddr_dl *sdl = (struct sockaddr_dl*)&ss;
				if (sdl->sdl_type == IFT_ETHER &&
					sdl->sdl_alen == 6) {
					bcopy(LLADDR(sdl), &ifb->ether_addr, 6);
					ifb->dev_media_addr = ifb->ether_addr;
					break;
				}
			}
		}
		
		ifnet_free_address_list(addr_list);
	}
	
	if (ifb->dev_media_addr) {
		bcopy(ifb->dev_media_addr, addr, 6);
		error = 0;
	}
	else {
		error = ENOENT;
	}
	
	return error;
}

__private_extern__ int
my_frameout(mbuf_t  *m0,
            struct blueCtlBlock *ifb,
            char *dest_linkaddr,
            char *ether_type)
{
    struct ether_header *eh;
    int hlen;

    if ((mbuf_flags(*m0) & M_PKTHDR) == 0) {
#if SIP_DEBUG
        log(LOG_WARNING, "my_frameout: m=%x m_flags=%x doesn't have PKTHDR set\n", *m0, mbuf_flags(*m0));
#endif
        mbuf_freem(*m0);
		*m0 = NULL;
        return (1);
    }
    hlen = ETHER_HDR_LEN;
    /*
     * Add local net header.  If no space in first mbuf,
     * allocate another.
     */
    if (mbuf_prepend(m0, sizeof(struct ether_header), M_WAITOK) != 0) {
#if SIP_DEBUG
         log(LOG_WARNING, "my_frameout: can't prepend\n");
#endif
         return (1);
    }
    eh = mbuf_data(*m0);
    if (ether_type != NULL)
    {
		eh->ether_type = *(u_int16_t*)ether_type;
    }
    else
    {
        // 802.3, eh->ether_type should be set to
        // the length of the packet
        eh->ether_type = htons(mbuf_pkthdr_len(*m0) - sizeof(struct ether_header));
    }
    memcpy(eh->ether_dhost, dest_linkaddr, 6);
	sip_get_ether_addr(ifb, eh->ether_shost);
	mbuf_setflags(*m0, mbuf_flags(*m0) | M_PROTO2); // why???
    return (0);
}

__private_extern__ void
sip_lock()
{
	lck_mtx_lock(sip_mtx);
}

__private_extern__ void
sip_unlock()
{
	lck_mtx_unlock(sip_mtx);
}

__private_extern__ void
ifb_release(struct blueCtlBlock *ifb)
{
    if (ifb == NULL)
        return;
	sip_lock();
	ifb->refcnt--;
	DEBUG_MSG("SharedIP - release ifb 0x%x\n", ifb);
	if (ifb->refcnt == 0) {
		if (ifb->ifp) {
			ifnet_release(ifb->ifp);
			ifb->ifp = NULL;
		}
		_FREE(ifb, M_PCB);
		DEBUG_MSG("SharedIP - freed ifb 0x%x\n", ifb);
	}
	sip_unlock();
}

__private_extern__ void
ifb_reference(struct blueCtlBlock *ifb)
{
	sip_lock();
	DEBUG_MSG("SharedIP - referenced ifb 0x%x\n", ifb);
	ifb->refcnt++;
	sip_unlock();
}
