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

#define SIP_DEBUG_FLOW	0	/* potentially lots of printfs */
#define SIP_DEBUG	0	/* important printfs only */
#define SIP_DEBUG_ERR	1	/* Important error printfs only */
#define SIP_DEBUG_INFO 0	/* Interesting printfs only */

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

struct BlueFilter
{
#define	IFNAMSIZ	16
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	short BF_flags;
	unsigned long  BF_address;	/* IP address or Atalk Network # */
	unsigned char  BF_node;		/* Atalk node # */
//        struct ifnet *BF_if;            /* Destination of "passed" pkts */
};

#define BF_ALLOC	0x01	/* Entry in use */
#define BF_DEALLOC	0x02	/* Clear matching entry */
#define BF_VALID	0x04	/* Address is valid */
#define BF_ATALK	0x08	/* Appletalk capture turned on (w. address) */
#define BF_IP		0x10	/* IP capture turned on (no address) */

/*
 * Control block for support of Blue/Classic networking
 * TODO:
 *  clean up filter data structures
 */
struct blueCtlBlock
{	struct blueCtlBlock *bcb_link; /* Chain `em up */
	struct socket *ifb_so;
	struct BlueFilter filter[2];	/* Only need to check IP, A/talk */
	struct sockaddr_at XAtalkAddr;	/* Atalk addr from X stack */
	int ClosePending;	/* 1 -> timer function should just free ifb */
	/* Media info - should be in ndrv_cb */
	unsigned char *dev_media_addr;	/* Media address for attached device */
	int media_addr_size;		/* Size (bytes) of media address */
	/* For port sharing */
	unsigned char udp_blue_owned;	/* Client UDP port ownership sig */
	unsigned char tcp_blue_owned;	/* Client TCP port ownership sig */
	unsigned long atalk_proto_filter_id;	/* AppleTalk DLIL filter */
	unsigned long ipv4_proto_filter_id;	/* IPv4 DLIL filter id */
	unsigned long lo_proto_filter_id;	/* Loopback filter id */
	/* For IP fragment handling */
	TAILQ_HEAD(fraglist, fraghead) fraglist;
	int fraglist_timer_on;	/* Flag for timing out frags */
	/* Stats */
	int pkts_up;
	int pkts_out;
	int pkts_looped_r2b;
	int pkts_looped_b2r;
	int no_bufs1;		/* Input NKE got null mbuf */
	int no_bufs2;		/* ndrv_output couldn't dup mbuf */
	int full_sockbuf;
};

/* Preallocate slots in blue_if to simplify filtering */
#define BFS_ATALK	0x0	/* The Atalk filter */
#define BFS_IP		0x1	/* The IP filter */

#define SIOCSSPLITTER	_IOW('i', 123, struct ifreq)	/* set 'splitter' */
#define SIOCGSPLITTER	_IOR('i', 122, struct ifreq)	/* get 'splitter' */
#define SIOCGSPLTSTAT	_IOWR('i', 121, struct Ystats)
#define SIOCSSPLTFILT	_IOW('i', 120, struct BlueFilter)
#define SIOCZSPLTSTAT	_IO('i', 119)		/* Clear stats */

/*
 * Config structure for  - NYI
 */
struct if_splitter
{	char ifs_on;		/* 1=>on */
	char ifs_qmax;		/* !0 => maxqlen */
	short ifs_wait;		/* Time to wait for signal */
	short ifs_sig;		/* Signal to send */
	short ifs_pad;		/* Extra space */
};

#define MDATA_ETHER_START(m) {				\
    (m)->m_data -= sizeof(struct ether_header);		\
    (m)->m_len += sizeof (struct ether_header);		\
    (m)->m_pkthdr.len += sizeof(struct ether_header);	\
}

#define MDATA_ETHER_END(m) {				\
    (m)->m_data += sizeof(struct ether_header);		\
    (m)->m_len -= sizeof (struct ether_header);		\
    (m)->m_pkthdr.len -= sizeof(struct ether_header);	\
}

#ifdef KERNEL
extern int enable_atalk(struct BlueFilter *, void *, struct blueCtlBlock *);
extern int atalk_attach_protofltr(struct ifnet *, struct blueCtlBlock *);
extern int ipv4_attach_protofltr(struct ifnet *, struct blueCtlBlock *);
extern int enable_ipv4(struct BlueFilter *, void *, struct blueCtlBlock *);
extern int atalk_stop(struct blueCtlBlock *);
extern int ipv4_stop(struct blueCtlBlock *);
extern int ipv4_control(struct socket *, struct sopt_shared_port_param *,
			    struct kextcb *, int);
extern int blue_inject(struct blueCtlBlock *, struct mbuf *);
extern struct mbuf *m_dup(struct mbuf *, int);
extern int my_frameout(struct mbuf **, struct ifnet *, char *, char *);
extern int ether_attach_ip(struct ifnet *, unsigned long *, unsigned long *);
extern int ether_attach_at(struct ifnet *, unsigned long *, unsigned long *);
extern void release_ifb(struct blueCtlBlock *);
extern int si_send_eth_atalk(struct mbuf **, struct blueCtlBlock *);
extern int si_send_eth_ipv4(struct mbuf **, struct blueCtlBlock *);
extern int si_send_ppp_ipv4(struct mbuf **, struct blueCtlBlock *);
extern void init_ipv4(struct socket *, struct kextcb *);
extern void timeout(timeout_fcn_t, void *, int);     
#endif /* KERNEL */
#endif /* _NET_SIP_H */
