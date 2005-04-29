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
 * Public definitions used for Shared IP support between
 *  Classic networking and the kernel
 *
 * Justin Walker, 991005
 *	@(#)sip.h	1.1 (Mac OS X) 6/10/43
 */

#ifndef _NET_SIP_H
#define _NET_SIP_H

#include <net/if.h>
#include <netat/appletalk.h>
#include <netinet/kpi_ipfilter.h>

#define SIP_DEBUG_FLOW  0	/* potentially lots of printfs */
#define SIP_DEBUG		0	/* important printfs only */
#define SIP_DEBUG_ERR   1	/* Important error printfs only */
#define SIP_DEBUG_INFO 	0	/* Interesting printfs only */

#if SIP_DEBUG
#define DEBUG_MSG printf
#else
#define DEBUG_MSG 1 ? (void)0 : (void)
#endif

#define DO_LOG	0

#define SharedIP_Handle 0xfeedface

/*
 * Min amount needed in contiguous array to filter packets
 * The smallest ones are ARP packets.
 * Needs to be updated if we look inside ICMP packets, or deal
 *  with IP options
 */
#define FILTER_LEN 28
/*
 * Y-adapter filter mechanism.
 * Declares the use of Atalk or IP from Classic.
 * If BF_ALLOC is set and BF_VALID is not, the corresponding
 * protocol type should be captured.
 * If BF_VALID is set, the structure specifies a protocol address
 *  [This is now limited to AppleTalk, and a single address at that
 *   (per interface)]
 * This is used to attempt binary compatibility during testing
 */

/* Eventually, this should probably use socket address structures
 * instead of an unsigned long and an unsigned char
 */
struct BlueFilter {
	unsigned long  BF_address;	/* IP address or Atalk Network # */
	unsigned char  BF_node;		/* Atalk node # */
	short BF_flags;
};

/*
 * Control block for support of Blue/Classic networking
 * TODO:
 *  clean up filter data structures
 */

/* Preallocate slots in blue_if to simplify filtering */
#define BFS_ATALK	0	/* The Atalk filter */
#define BFS_IP		1	/* The IP filter */
#define BFS_COUNT	2	/* number of BlueFilters per blue control block */

struct blueCtlBlock {
	int refcnt;
    struct blueCtlBlock *bcb_link; /* Chain `em up */
    socket_t ifb_so;
	ifnet_t ifp;
    struct BlueFilter filter[BFS_COUNT];	/* Only need to check IP, A/talk */
    struct sockaddr_at XAtalkAddr;	/* Atalk addr from X stack */
    int ClosePending;	/* 1 -> timer function should just free ifb */
    /* Media info - should be in ndrv_cb */
    unsigned char *dev_media_addr;	/* Media address for attached device (MAC address) */
    int media_addr_size;		/* Size (bytes) of media address */
	u_int8_t		ether_addr[6];
    /* For port sharing */
    unsigned char udp_blue_owned;	/* Client UDP port ownership sig */
    unsigned char tcp_blue_owned;	/* Client TCP port ownership sig */
	interface_filter_t	atalk_proto_filter;
	ipfilter_t			ip_filter;
    int ipv4_stopping;
    int atalk_stopping;
    /* Stats */
    int pkts_up;
    int pkts_out;
    int pkts_looped_r2b;
    int pkts_looped_b2r;
    int no_bufs1;		/* Input NKE got null mbuf */
    int no_bufs2;		/* ndrv_output couldn't dup mbuf */
    int full_sockbuf;
	int noifpnotify;
	mbuf_t	frag_head;
	mbuf_t	frag_last;
};

#define MDATA_INCLUDE_HEADER(_m, _size) { \
	mbuf_prepend(&(_m), (_size), M_WAITOK); \
}

#define MDATA_REMOVE_HEADER(_m, _size) { \
	mbuf_adj((_m), (_size)); \
}

#define MDATA_ETHER_START(_m) { \
	mbuf_prepend(&(_m), sizeof(struct ether_header), M_WAITOK); \
}

#define MDATA_ETHER_END(_m) { \
	mbuf_adj(_m, sizeof(struct ether_header)); \
}

#define mtodAtOffset(_m, _offset, _type) ((_type)(((unsigned char*)mbuf_data(_m)) + _offset))

#ifdef KERNEL
int enable_atalk(struct BlueFilter *, void *, struct blueCtlBlock *);
int ipv4_attach_protofltr(ifnet_t, struct blueCtlBlock *);
int enable_ipv4(struct BlueFilter *, void *, struct blueCtlBlock *);
int atalk_stop(struct blueCtlBlock *);
int ipv4_stop(struct blueCtlBlock *);
int ipv4_control(socket_t so, struct sopt_shared_port_param *psp,
				 struct blueCtlBlock *, int cmd);
int blue_inject(struct blueCtlBlock *, mbuf_t);
int my_frameout(mbuf_t *, struct blueCtlBlock *, char *, char *);
int ether_attach_ip(ifnet_t, unsigned long *, unsigned long *);
int ether_attach_at(ifnet_t);
int ether_detach_at(ifnet_t);
void ifb_release(struct blueCtlBlock *);
void ifb_reference(struct blueCtlBlock *);
int si_send_eth_atalk(mbuf_t *m_orig, struct blueCtlBlock *ifb, ifnet_t ifp);
int si_send_eth_ipv4(mbuf_t *m_orig, struct blueCtlBlock *ifb, ifnet_t ifp);
int si_send_ppp_ipv4(mbuf_t *m_orig, struct blueCtlBlock *ifb, ifnet_t ifp);
void timeout(timeout_fcn_t, void *, int);
errno_t sip_ifp(struct blueCtlBlock *ifb, ifnet_t *ifp);
int sip_get_ether_addr(struct blueCtlBlock *ifb, char* addr);
void sip_lock(void);
void sip_unlock(void);

#endif /* KERNEL */
#endif /* _NET_SIP_H */
