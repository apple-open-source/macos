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


#ifndef __PPP_H__
#define __PPP_H__


/*
 * The basic PPP frame.
 */
#define PPP_HDRLEN	4	/* octets for standard ppp header */

/*
 * Packet sizes
 */
#define	PPP_MTU		1500	/* Default MTU (size of Info field) */
#define PPP_MAXMTU	65535 - PPP_HDRLEN
#define PPP_MINMTU	64
#define PPP_MRU		1500	/* default MRU = max length of info field */
#define PPP_MAXMRU	65000	/* Largest MRU we allow */
#define PPP_MINMRU	128

#define PPP_ADDRESS(p)	(((u_char *)(p))[0])
#define PPP_CONTROL(p)	(((u_char *)(p))[1])
#define PPP_PROTOCOL(p)	((((u_char *)(p))[2] << 8) + ((u_char *)(p))[3])

/*
 * Significant octet values.
 */
#define	PPP_ALLSTATIONS	0xff	/* All-Stations broadcast address */
#define	PPP_UI		0x03	/* Unnumbered Information */
#define	PPP_FLAG	0x7e	/* Flag Sequence */
#define	PPP_ESCAPE	0x7d	/* Asynchronous Control Escape */
#define	PPP_TRANS	0x20	/* Asynchronous transparency modifier */

/*
 * Protocol field values.
 */
#define PPP_IP		0x21	/* Internet Protocol */
#define PPP_AT		0x29	/* AppleTalk Protocol */
#define PPP_IPX		0x2b	/* IPX protocol */
#define	PPP_VJC_COMP	0x2d	/* VJ compressed TCP */
#define	PPP_VJC_UNCOMP	0x2f	/* VJ uncompressed TCP */
#define PPP_IPV6	0x57	/* Internet Protocol Version 6 */
#define PPP_COMP	0xfd	/* compressed packet */
#define PPP_IPCP	0x8021	/* IP Control Protocol */
#define PPP_ATCP	0x8029	/* AppleTalk Control Protocol */
#define PPP_IPXCP	0x802b	/* IPX Control Protocol */
#define PPP_IPV6CP	0x8057	/* IPv6 Control Protocol */
#define PPP_CCP		0x80fd	/* Compression Control Protocol */
#define PPP_LCP		0xc021	/* Link Control Protocol */
#define PPP_PAP		0xc023	/* Password Authentication Protocol */
#define PPP_LQR		0xc025	/* Link Quality Report protocol */
#define PPP_CHAP	0xc223	/* Cryptographic Handshake Auth. Protocol */
#define PPP_CBCP	0xc029	/* Callback Control Protocol */



/* Subtype for PPP family (family is build with subtype/type) */
/* APPLE_IF_SUBFAM_PPP_XXX / APPLE_IF_FAM_PPP */
/* Subtypes 0..127 are reserved for Apple */
#define APPLE_IF_FAM_PPP_SERIAL	0	/* Apple's Asynchonous Line Discipline */
#define APPLE_IF_FAM_PPP_PPPoE	2	/* Apple's PPP over Ethernet */

/* name used for the interface */
/* to avoid confict, each implementation should used different names */
#define APPLE_PPP_NAME_SERIAL	"ppp"
#define APPLE_PPP_NAME_PPPoE	"pppoe"


/* drivers must specify the physical layer they are running over */
#define PPP_PHYS_SERIAL		0	/* Asynchonous Line Discipline */
#define PPP_PHYS_PPPoE		2	/* PPP over Ethernet */
#define PPP_PHYS_PPPoA		3	/* PPP over ATM */
#define PPP_PHYS_PPTP		4	/* Point-to-Point Tunneling Protocol */
#define PPP_PHYS_L2TP		5	/* Layer 2 Tunneling Protocol */
#define PPP_PHYS_ISDN		6	/* Integrated Service Digital Network */


#define PPP_NAME_SIZE 		256		/* informationnal name, not interface name */
#define PPP_ADDR_SIZE 		256		/* remote address to dial */
#define PPP_INITPR_SIZE		256		/* init structure, private to protocol */

 /* ioctls */

#define PPP_CAPS_DIAL		0x00000001	/* support integrated dialing */
#define PPP_CAPS_LOOPBACK	0x00000002	/* support loopback (for testing purpose) */
//#define PPP_CAPS_DYNLINK	0x00000004	/* support dynamic allocation for links */
#define PPP_CAPS_RELIABLE	0x00000008	/* is the link reliable ? (i.e. no data loss) */
#define PPP_CAPS_ERRORDETECT	0x00000010	/* does the link support error detection ? (vj needs error detection) */
#define PPP_CAPS_DISCDETECT	0x00000020	/* link is always able to detect disconnection (don't need keep alive) */
#define PPP_CAPS_ASYNCFRAME	0x00000020	/* link uses asynchronous ppp framing */
#define PPP_CAPS_PCOMP		0x00000040	/* link supports protocol field compression */
#define PPP_CAPS_ACCOMP		0x00000080	/* link supports address/control field compression */


struct ppp_caps {
    char 	link_name[PPP_NAME_SIZE];	/* intormationnal name, for display purpose (like "PPPoE Apple") */
    u_int16_t	physical;			/* PPPoE, PPPoA, L2TP, SERIAL... */
    u_int16_t	max_mtu;			/* max mtu supported */
    u_int16_t	max_mru;			/* max mru supported */
    u_int32_t	flags;				/* flags for capabilities */
    //u_int16_t	max_links;			/* max links supported (useful if CAPS_DYNLINK flag present)  */
    u_int32_t	reserved[4];			/* reserved for future use */
};

/*
 * tcp von jacobson  header compression.
 */
struct ppp_ip_vj {
    u_char 	vj;				/* 1 = turn vj ON, 0 = turn vj off */
    u_char	cid;				/* 1 = turn cid ON, 0 = turn it OFF */
    u_char	max_cid;			/* maximum cid states  */
};

/*
 * get idle activity.
 */
struct ppp_idle {
    time_t 	xmit_idle;		/* time since last NP packet sent */
    time_t 	recv_idle;		/* time since last NP packet received */
};

/*
 * Extended asyncmap - allows any character to be escaped.
 */
typedef u_int32_t	ext_accm[8];

/*
 * Statistics.
 */
struct ppp_stats	{
    u_int32_t ibytes;	/* bytes received */
    u_int32_t ipackets;	/* packets received */
    u_int32_t ierrors;	/* receive errors */
    u_int32_t obytes;	/* bytes sent */
    u_int32_t opackets;	/* packets sent */
    u_int32_t oerrors;	/* transmit errors */
};

struct vjstat {
    unsigned int vjs_packets;	/* outbound packets */
    unsigned int vjs_compressed; /* outbound compressed packets */
    unsigned int vjs_searches;	/* searches for connection state */
    unsigned int vjs_misses;	/* times couldn't find conn. state */
    unsigned int vjs_uncompressedin; /* inbound uncompressed packets */
    unsigned int vjs_compressedin; /* inbound compressed packets */
    unsigned int vjs_errorin;	/* inbound unknown type packets */
    unsigned int vjs_tossed;	/* inbound packets tossed because of error */
};

//struct ppp_stats {
//    struct pppstat p;		/* basic PPP statistics */
//    struct vjstat vj;		/* VJ header compression statistics */
//};

struct compstat {
    unsigned int unc_bytes;	/* total uncompressed bytes */
    unsigned int unc_packets;	/* total uncompressed packets */
    unsigned int comp_bytes;	/* compressed bytes */
    unsigned int comp_packets;	/* compressed packets */
    unsigned int inc_bytes;	/* incompressible bytes */
    unsigned int inc_packets;	/* incompressible packets */
    unsigned int ratio;		/* recent compression ratio << 8 */
};

struct ppp_comp_stats {
    struct compstat c;		/* packet compression statistics */
    struct compstat d;		/* packet decompression statistics */
};


struct ifpppreq {
    char	ifr_name[IFNAMSIZ];	/* if name, e.g. "ppp0" */
    u_int32_t	ifr_code;
    union {
        // link specific 
        u_int32_t	ifru_mru;			/* MRU */
        u_int32_t	ifru_map;			/* send or receive async map */
        ext_accm	ifru_xmap;			/* extended async map */
        u_int32_t	ifru_eflags;			/* extended flags */
        u_char 		ifru_connect[PPP_ADDR_SIZE];	/* connection address */
        u_int32_t	ifru_disconnect;		/* disconnect info */
        u_char 		ifru_listen[PPP_ADDR_SIZE];	/* listen parameters */
        u_int32_t	ifru_accept;			/* accept info */
        u_int32_t	ifru_refuse;			/* refuse info */
        struct ppp_caps	ifru_caps;			/* capabilies of the link */
        u_int32_t	ifru_loopback;			/* set the interface in loopback mode, for testing purpose */
//        u_int32_t	ifru_nblinks;			/* number of interfaces of that kind */
        u_int32_t	ifru_abort;			/* abort info */
        struct ppp_idle	ifru_idle;			/* idle info */
        struct ppp_stats ifru_stats;			/* stats for ppp level */
        u_int32_t 	ifru_newif;			/* interface create */
        u_int32_t 	ifru_disposeif;			/* interface dispose */
        u_char 		ifru_device[PPP_NAME_SIZE];	/* device name (subset of the set config) */
        //
        u_int32_t	ifru_attach_ip;			/* ip proto attachmenent */
        u_int32_t	ifru_detach_ip;			/* ip proto detachement */
        struct ppp_ip_vj ifru_ip_vj;			/* vj parameters [IP protocol specific] */
       
        //char		ifru_filler[1024 - sizeof(u_int32_t) - IFNAMSIZ]; 	/* max size for data */
    } ifr_ifru;
#define	ifr_mru		ifr_ifru.ifru_mru
#define	ifr_map		ifr_ifru.ifru_map
#define	ifr_xmap	ifr_ifru.ifru_xmap
#define	ifr_eflags	ifr_ifru.ifru_eflags
#define	ifr_connect	ifr_ifru.ifru_connect
#define	ifr_disconnect	ifr_ifru.ifru_disconnect
#define	ifr_listen	ifr_ifru.ifru_listen
#define	ifr_accept	ifr_ifru.ifru_accept
#define	ifr_refuse	ifr_ifru.ifru_refuse
#define	ifr_caps	ifr_ifru.ifru_caps
#define	ifr_loopback	ifr_ifru.ifru_loopback
//#define	ifr_nblinks	ifr_ifru.ifru_nblinks
#define	ifr_abort	ifr_ifru.ifru_abort
#define	ifr_idle	ifr_ifru.ifru_idle
#define	ifr_stats	ifr_ifru.ifru_stats
#define	ifr_newif	ifr_ifru.ifru_newif
#define	ifr_disposeif	ifr_ifru.ifru_disposeif
#define	ifr_device	ifr_ifru.ifru_device
#define	ifr_attach_ip	ifr_ifru.ifru_attach_ip
#define	ifr_detach_ip	ifr_ifru.ifru_detach_ip
#define	ifr_ip_vj	ifr_ifru.ifru_ip_vj
};


#define SIOCGIFPPP		_IOWR('i', 131, struct ifpppreq)
#define SIOCSIFPPP		_IOWR('i', 132, struct ifpppreq)

#define IFPPP_MRU		1
#define IFPPP_ASYNCMAP		2	/* supported by link with asynchronous framing */
#define IFPPP_RASYNCMAP		3	/* supported by link with asynchronous framing */
#define IFPPP_XASYNCMAP 	4	/* supported by link with asynchronous framing */
#define IFPPP_EFLAGS 		5
#define IFPPP_CONNECT 		6
#define IFPPP_DISCONNECT	7
#define IFPPP_LISTEN 		8
#define IFPPP_ACCEPT 		9
#define IFPPP_REFUSE 		10
#define IFPPP_CAPS 		11
#define IFPPP_LOOPBACK 		12
#define IFPPP_NBLINKS		13
#define IFPPP_ABORT		14
#define IFPPP_IDLE		18
#define IFPPP_STATS		19
#define IFPPP_NEWIF		20
#define IFPPP_DISPOSEIF		21
#define IFPPP_DEVICE		22
#define IFPPP_ATTACH_IP		0x8001
#define IFPPP_DETACH_IP		0x8002
#define IFPPP_IP_VJ		0x8003


/* values for ifnet->if_flags */

#define IFF_PPP_INCOMING_CALL 	IFF_LINK0
#define IFF_PPP_CONNECTING	IFF_LINK1
#define IFF_PPP_DISCONNECTING 	IFF_LINK2
#define IFF_PPP_CONNECTED 	IFF_RUNNING


/* values for ifnet->if_eflags */

#define IFF_ELINK0		0x01000000
#define IFF_ELINK1		0x02000000
#define IFF_ELINK2		0x04000000
#define IFF_ELINK3		0x08000000
#define IFF_ELINK4		0x10000000

#define IFF_ELINKMASK		0xFF000000

// set by the mux, to inform others that it may do PCOMP and ACCOMP
// these flags should only concern the mux and may be use for
// informationnal purpose by the driver
#define IFF_PPP_DOES_ACCOMP	IFF_ELINK0		/* link does address and control field compression */
#define IFF_PPP_DOES_PCOMP	IFF_ELINK1		/* link does protocol field compression */
// set by the mux to inform driver that it MUST accept ACCOMP and PCOMP
// the driver is still free to ignore the flags and give the packet as-is
// but the driver is aware of what has been negociated in case it wants
// to do frame checking (in the case of async ppp, for example)
#define IFF_PPP_ACPT_ACCOMP	IFF_ELINK2		/* link accepts address and control field compression */
#define IFF_PPP_ACPT_PCOMP	IFF_ELINK3		/* link accepts protocol field compression */
// set by the driver to inform the mux that the driver doesn't want ALLSTATION/UI even if not negociated
// mux need to remove address/control field prior to send packet to the driver
// mux must expect packets from the driver without thos fields
#define IFF_PPP_DEL_AC		IFF_ELINK4		/* link doesn't need address and control and needs to remove it */


/* values for events */

/* struct used by user apps decoding the data portion of the event */
struct kev_ppp_data {
    struct net_event_data   link_data;
    u_int32_t  		error;                 /* error code */
};

/* struct used by ppp drivers providing the event */
struct kev_ppp_msg {
    u_long	       	total_size;      /* Size of entire event msg */
    u_long	       	vendor_code;     /* For non-Apple extensibility */
    u_long	       	kev_class;	/* Layer of event source */
    u_long	       	kev_subclass;    /* Component within layer    */
    u_long	       	id;	        /* Monotonically increasing value  */
    u_long            	event_code;      /* unique code */
    struct kev_ppp_data event_data;
};


/* Define PPP events, as subclass of NETWORK_CLASS events */

#define KEV_PPP_SUBCLASS 3

#define KEV_PPP_PACKET_LOST	1
#define KEV_PPP_CONNECTING	2
#define KEV_PPP_CONNECTED	3
#define KEV_PPP_DISCONNECTING	4
#define KEV_PPP_DISCONNECTED	5
#define KEV_PPP_LISTENING	6
#define KEV_PPP_RINGING		7
#define KEV_PPP_ACCEPTING	8
#define KEV_PPP_NEEDCONNECT	9

/* communication pppd/ppp_domain */


#define IOC_PPP_GETEFLAGS	_IOWR('P', 1, u_int32_t)
#define IOC_PPP_SETEFLAGS	_IOW('P', 2, u_int32_t)
#define IOC_PPP_ATTACHMUX	_IOW('P', 3, u_int32_t)
#define IOC_PPP_DETACHMUX	_IOW('P', 4, u_int32_t)


/*  We steal two bits in the mbuf m_flags, to mark high-priority packets
for output, and received packets following lost/corrupted packets. */
#define M_HIGHPRI	0x2000	/* output packet for sc_fastq */
#define M_ERRMARK	0x4000	/* steal a bit in mbuf m_flags */


#endif