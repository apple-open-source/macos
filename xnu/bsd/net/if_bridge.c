/*
 * Copyright (c) 2004-2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*	$NetBSD: if_bridge.c,v 1.31 2005/06/01 19:45:34 jdc Exp $	*/
/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * OpenBSD: if_bridge.c,v 1.60 2001/06/15 03:38:33 itojun Exp
 */

/*
 * Network interface bridge support.
 *
 * TODO:
 *
 *	- Currently only supports Ethernet-like interfaces (Ethernet,
 *	  802.11, VLANs on Ethernet, etc.)  Figure out a nice way
 *	  to bridge other types of interfaces (FDDI-FDDI, and maybe
 *	  consider heterogenous bridges).
 *
 *	- GIF isn't handled due to the lack of IPPROTO_ETHERIP support.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h> /* for net/if.h */
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mcache.h>

#include <sys/kauth.h>

#include <kern/thread_call.h>

#include <libkern/libkern.h>

#include <kern/zalloc.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/net_api_stats.h>

#include <netinet/in.h> /* for struct arpcom */
#include <netinet/tcp.h> /* for struct tcphdr */
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#define _IP_VHL
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/if_ether.h> /* for struct arpcom */
#include <net/bridgestp.h>
#include <net/if_bridgevar.h>
#include <net/if_llc.h>
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

#include <net/if_ether.h>
#include <net/dlil.h>
#include <net/kpi_interfacefilter.h>
#include <net/pfvar.h>

#include <net/route.h>
#include <dev/random/randomdev.h>

#include <netinet/bootp.h>
#include <netinet/dhcp.h>

#if SKYWALK
#include <skywalk/nexus/netif/nx_netif.h>
#endif /* SKYWALK */

#include <net/sockaddr_utils.h>
#include <net/mblist.h>

#include <os/log.h>

static struct in_addr inaddr_any = { .s_addr = INADDR_ANY };


#define __M_FLAGS_ARE_SET(m, flags)     (((m)->m_flags & (flags)) != 0)
#define IS_BCAST(m)                     __M_FLAGS_ARE_SET(m, M_BCAST)
#define IS_MCAST(m)                     __M_FLAGS_ARE_SET(m, M_MCAST)
#define IS_BCAST_MCAST(m)               __M_FLAGS_ARE_SET(m, M_BCAST | M_MCAST)

#define HTONS_ETHERTYPE_ARP             htons(ETHERTYPE_ARP)
#define HTONS_ETHERTYPE_IP              htons(ETHERTYPE_IP)
#define HTONS_ETHERTYPE_IPV6            htons(ETHERTYPE_IPV6)
#define HTONS_ARPHRD_ETHER              htons(ARPHRD_ETHER)
#define HTONS_ARPOP_REQUEST             htons(ARPOP_REQUEST)
#define HTONS_ARPOP_REPLY               htons(ARPOP_REPLY)
#define HTONS_IPPORT_BOOTPC             htons(IPPORT_BOOTPC)
#define HTONS_IPPORT_BOOTPS             htons(IPPORT_BOOTPS)
#define HTONS_DHCP_FLAGS_BROADCAST      htons(DHCP_FLAGS_BROADCAST)

/*
 * if_bridge_debug, BR_DBGF_*
 * - 'if_bridge_debug' is a bitmask of BR_DBGF_* flags that can be set
 *   to enable additional logs for the corresponding bridge function
 * - "sysctl net.link.bridge.debug" controls the value of
 *   'if_bridge_debug'
 */
static uint32_t if_bridge_debug = 0;
#define BR_DBGF_LIFECYCLE       0x0001
#define BR_DBGF_INPUT           0x0002
#define BR_DBGF_OUTPUT          0x0004
#define BR_DBGF_RT_TABLE        0x0008
#define BR_DBGF_DELAYED_CALL    0x0010
#define BR_DBGF_IOCTL           0x0020
#define BR_DBGF_MBUF            0x0040
#define BR_DBGF_MCAST           0x0080
#define BR_DBGF_HOSTFILTER      0x0100
#define BR_DBGF_CHECKSUM        0x0200
#define BR_DBGF_MAC_NAT         0x0400
#define BR_DBGF_INPUT_LIST      0x0800

/*
 * if_bridge_log_level
 * - 'if_bridge_log_level' ensures that by default important logs are
 *   logged regardless of if_bridge_debug by comparing the log level
 *   in BRIDGE_LOG to if_bridge_log_level
 * - use "sysctl net.link.bridge.log_level" controls the value of
 *   'if_bridge_log_level'
 * - the default value of 'if_bridge_log_level' is LOG_NOTICE; important
 *   logs must use LOG_NOTICE to ensure they appear by default
 */
static int if_bridge_log_level = LOG_NOTICE;

#define BRIDGE_DBGF_ENABLED(__flag)     ((if_bridge_debug & __flag) != 0)

/*
 * BRIDGE_LOG, BRIDGE_LOG_SIMPLE
 * - macros to generate the specified log conditionally based on
 *   the specified log level and debug flags
 * - BRIDGE_LOG_SIMPLE does not include the function name in the log
 */
#define BRIDGE_LOG(__level, __dbgf, __string, ...)              \
	do {                                                            \
	        if (__level <= if_bridge_log_level ||                   \
	            BRIDGE_DBGF_ENABLED(__dbgf)) {                      \
	                os_log(OS_LOG_DEFAULT, "%s: " __string, \
	                       __func__, ## __VA_ARGS__);       \
	        }                                                       \
	} while (0)
#define BRIDGE_LOG_SIMPLE(__level, __dbgf, __string, ...)               \
	do {                                                    \
	        if (__level <= if_bridge_log_level ||           \
	            BRIDGE_DBGF_ENABLED(__dbgf)) {                      \
	                os_log(OS_LOG_DEFAULT, __string, ## __VA_ARGS__); \
	        }                                                               \
	} while (0)

#define _BRIDGE_LOCK(_sc)               lck_mtx_lock(&(_sc)->sc_mtx)
#define _BRIDGE_UNLOCK(_sc)             lck_mtx_unlock(&(_sc)->sc_mtx)
#define BRIDGE_LOCK_ASSERT_HELD(_sc)            \
	LCK_MTX_ASSERT(&(_sc)->sc_mtx, LCK_MTX_ASSERT_OWNED)
#define BRIDGE_LOCK_ASSERT_NOTHELD(_sc)         \
	LCK_MTX_ASSERT(&(_sc)->sc_mtx, LCK_MTX_ASSERT_NOTOWNED)

#define BRIDGE_LOCK_DEBUG      1
#if BRIDGE_LOCK_DEBUG

#define BR_LCKDBG_MAX                   4

#define BRIDGE_LOCK(_sc)                bridge_lock(_sc)
#define BRIDGE_UNLOCK(_sc)              bridge_unlock(_sc)
#define BRIDGE_LOCK2REF(_sc, _err)      _err = bridge_lock2ref(_sc)
#define BRIDGE_UNREF(_sc)               bridge_unref(_sc)
#define BRIDGE_XLOCK(_sc)               bridge_xlock(_sc)
#define BRIDGE_XDROP(_sc)               bridge_xdrop(_sc)

#else /* !BRIDGE_LOCK_DEBUG */

#define BRIDGE_LOCK(_sc)                _BRIDGE_LOCK(_sc)
#define BRIDGE_UNLOCK(_sc)              _BRIDGE_UNLOCK(_sc)
#define BRIDGE_LOCK2REF(_sc, _err)      do {                            \
	BRIDGE_LOCK_ASSERT_HELD(_sc);                                   \
	if ((_sc)->sc_iflist_xcnt > 0)                                  \
	        (_err) = EBUSY;                                         \
	else {                                                          \
	        (_sc)->sc_iflist_ref++;                                 \
	        (_err) = 0;                                             \
	}                                                               \
	_BRIDGE_UNLOCK(_sc);                                            \
} while (0)
#define BRIDGE_UNREF(_sc)               do {                            \
	_BRIDGE_LOCK(_sc);                                              \
	(_sc)->sc_iflist_ref--;                                         \
	if (((_sc)->sc_iflist_xcnt > 0) && ((_sc)->sc_iflist_ref == 0))	{ \
	        _BRIDGE_UNLOCK(_sc);                                    \
	        wakeup(&(_sc)->sc_cv);                                  \
	} else                                                          \
	        _BRIDGE_UNLOCK(_sc);                                    \
} while (0)
#define BRIDGE_XLOCK(_sc)               do {                            \
	BRIDGE_LOCK_ASSERT_HELD(_sc);                                   \
	(_sc)->sc_iflist_xcnt++;                                        \
	while ((_sc)->sc_iflist_ref > 0)                                \
	        msleep(&(_sc)->sc_cv, &(_sc)->sc_mtx, PZERO,            \
	            "BRIDGE_XLOCK", NULL);                              \
} while (0)
#define BRIDGE_XDROP(_sc)               do {                            \
	BRIDGE_LOCK_ASSERT_HELD(_sc);                                   \
	(_sc)->sc_iflist_xcnt--;                                        \
} while (0)

#endif /* BRIDGE_LOCK_DEBUG */

#define BRIDGE_BPF_TAP_IN(ifp, m) \
	do {                                                            \
	        if (ifp->if_bpf != NULL) {                              \
	                bpf_tap_in(ifp, DLT_EN10MB, m, NULL, 0);        \
	        }                                                       \
	} while(0)

#define BRIDGE_BPF_TAP_OUT(ifp, m)                                      \
	do {                                                            \
	        if (ifp->if_bpf != NULL) {                              \
	                bpf_tap_out(ifp, DLT_EN10MB, m, NULL, 0);       \
	        }                                                       \
	} while(0)


/*
 * Initial size of the route hash table.  Must be a power of two.
 */
#ifndef BRIDGE_RTHASH_SIZE
#define BRIDGE_RTHASH_SIZE              16
#endif

/*
 * Maximum size of the routing hash table
 */
#define BRIDGE_RTHASH_SIZE_MAX          2048

#define BRIDGE_RTHASH_MASK(sc)          ((sc)->sc_rthash_size - 1)

/*
 * Maximum number of addresses to cache.
 */
#ifndef BRIDGE_RTABLE_MAX
#define BRIDGE_RTABLE_MAX               100
#endif

/*
 * Timeout (in seconds) for entries learned dynamically.
 */
#ifndef BRIDGE_RTABLE_TIMEOUT
#define BRIDGE_RTABLE_TIMEOUT           (20 * 60)       /* same as ARP */
#endif

/*
 * Number of seconds between walks of the route list.
 */
#ifndef BRIDGE_RTABLE_PRUNE_PERIOD
#define BRIDGE_RTABLE_PRUNE_PERIOD      (5 * 60)
#endif

/*
 * Number of MAC NAT entries
 * - sized based on 16 clients (including MAC NAT interface)
 *   each with 4 addresses
 */
#ifndef BRIDGE_MAC_NAT_ENTRY_MAX
#define BRIDGE_MAC_NAT_ENTRY_MAX        64
#endif /* BRIDGE_MAC_NAT_ENTRY_MAX */

/*
 * List of capabilities to possibly mask on the member interface.
 */
#define BRIDGE_IFCAPS_MASK              (IFCAP_TSO | IFCAP_TXCSUM)
/*
 * List of capabilities to disable on the member interface.
 */
#define BRIDGE_IFCAPS_STRIP             IFCAP_LRO

/*
 * Bridge interface list entry.
 */
struct bridge_iflist {
	TAILQ_ENTRY(bridge_iflist) bif_next;
	struct ifnet            *bif_ifp;       /* member if */
	struct bstp_port        bif_stp;        /* STP state */
	uint32_t                bif_ifflags;    /* member if flags */
	int                     bif_savedcaps;  /* saved capabilities */
	uint32_t                bif_addrmax;    /* max # of addresses */
	uint32_t                bif_addrcnt;    /* cur. # of addresses */
	uint32_t                bif_addrexceeded; /* # of address violations */

	interface_filter_t      bif_iff_ref;
	struct bridge_softc     *bif_sc;
	uint32_t                bif_flags;

	/* host filter */
	struct in_addr          bif_hf_ipsrc;
	uint8_t                 bif_hf_hwsrc[ETHER_ADDR_LEN];

	struct ifbrmstats       bif_stats;
};

static inline bool
bif_ifflags_are_set(struct bridge_iflist * bif, uint32_t flags)
{
	return (bif->bif_ifflags & flags) != 0;
}

static inline bool
bif_has_checksum_offload(struct bridge_iflist * bif)
{
	return bif_ifflags_are_set(bif, IFBIF_CHECKSUM_OFFLOAD);
}

static inline bool
bif_has_mac_nat(struct bridge_iflist * bif)
{
	return bif_ifflags_are_set(bif, IFBIF_MAC_NAT);
}

/* fake errors to make the code clearer */
#define _EBADIP                 EJUSTRETURN
#define _EBADIPCHECKSUM         EJUSTRETURN
#define _EBADIPV6               EJUSTRETURN
#define _EBADUDP                EJUSTRETURN
#define _EBADTCP                EJUSTRETURN
#define _EBADUDPCHECKSUM        EJUSTRETURN
#define _EBADTCPCHECKSUM        EJUSTRETURN

#define BIFF_PROMISC            0x01    /* promiscuous mode set */
#define BIFF_PROTO_ATTACHED     0x02    /* protocol attached */
#define BIFF_FILTER_ATTACHED    0x04    /* interface filter attached */
#define BIFF_MEDIA_ACTIVE       0x08    /* interface media active */
#define BIFF_HOST_FILTER        0x10    /* host filter enabled */
#define BIFF_HF_HWSRC           0x20    /* host filter source MAC is set */
#define BIFF_HF_IPSRC           0x40    /* host filter source IP is set */
#define BIFF_INPUT_BROADCAST    0x80    /* send broadcast packets in */
#define BIFF_IN_MEMBER_LIST     0x100   /* added to the member list */
#define BIFF_WIFI_INFRA         0x200   /* interface is Wi-Fi infra */
#define BIFF_ALL_MULTI          0x400   /* allmulti set */
#define BIFF_LRO_DISABLED       0x800   /* LRO was disabled */
#if SKYWALK
#define BIFF_FLOWSWITCH_ATTACHED 0x1000   /* we attached the flowswitch */
#define BIFF_NETAGENT_REMOVED    0x2000   /* we removed the netagent */
#endif /* SKYWALK */

/*
 * mac_nat_entry
 * - translates between an IP address and MAC address on a specific
 *   bridge interface member
 */
struct mac_nat_entry {
	LIST_ENTRY(mac_nat_entry) mne_list;     /* list linkage */
	struct bridge_iflist    *mne_bif;       /* originating interface */
	unsigned long           mne_expire;     /* expiration time */
	union {
		struct in_addr  mneu_ip;        /* originating IPv4 address */
		struct in6_addr mneu_ip6;       /* originating IPv6 address */
	} mne_u;
	uint8_t                 mne_mac[ETHER_ADDR_LEN];
	uint8_t                 mne_flags;
	uint8_t                 mne_reserved;
};
#define mne_ip  mne_u.mneu_ip
#define mne_ip6 mne_u.mneu_ip6

#define MNE_FLAGS_IPV6          0x01    /* IPv6 address */

LIST_HEAD(mac_nat_entry_list, mac_nat_entry);

/*
 * mac_nat_record
 * - used by bridge_mac_nat_output() to convey the translation that needs
 *   to take place in bridge_mac_nat_translate
 * - holds enough information so that the translation can be done later
 *   when the destination interface is the MAC-NAT interface
 */
struct mac_nat_record {
	uint16_t                mnr_ether_type;
	union {
		uint16_t        mnru_arp_offset;
		struct {
			uint16_t mnruip_dhcp_flags;
			uint16_t mnruip_udp_csum;
			uint8_t  mnruip_header_len;
		} mnru_ip;
		struct {
			uint16_t mnruip6_icmp6_len;
			uint16_t mnruip6_lladdr_offset;
			uint8_t mnruip6_icmp6_type;
			uint8_t mnruip6_header_len;
		} mnru_ip6;
	} mnr_u;
};

#define mnr_arp_offset  mnr_u.mnru_arp_offset

#define mnr_ip_header_len       mnr_u.mnru_ip.mnruip_header_len
#define mnr_ip_dhcp_flags       mnr_u.mnru_ip.mnruip_dhcp_flags
#define mnr_ip_udp_csum         mnr_u.mnru_ip.mnruip_udp_csum

#define mnr_ip6_icmp6_len       mnr_u.mnru_ip6.mnruip6_icmp6_len
#define mnr_ip6_icmp6_type      mnr_u.mnru_ip6.mnruip6_icmp6_type
#define mnr_ip6_header_len      mnr_u.mnru_ip6.mnruip6_header_len
#define mnr_ip6_lladdr_offset   mnr_u.mnru_ip6.mnruip6_lladdr_offset

/*
 * Bridge route node.
 */
struct bridge_rtnode {
	LIST_ENTRY(bridge_rtnode) brt_hash;     /* hash table linkage */
	LIST_ENTRY(bridge_rtnode) brt_list;     /* list linkage */
	struct bridge_iflist    *brt_dst;       /* destination if */
	unsigned long           brt_expire;     /* expiration time */
	uint8_t                 brt_flags;      /* address flags */
	uint8_t                 brt_addr[ETHER_ADDR_LEN];
	uint16_t                brt_vlan;       /* vlan id */
};

#define brt_ifp                 brt_dst->bif_ifp

/*
 * Bridge delayed function call context
 */
typedef void (*bridge_delayed_func_t)(struct bridge_softc *);

struct bridge_delayed_call {
	struct bridge_softc     *bdc_sc;
	bridge_delayed_func_t   bdc_func; /* Function to call */
	struct timespec         bdc_ts; /* Time to call */
	u_int32_t               bdc_flags;
	thread_call_t           bdc_thread_call;
};

#define BDCF_OUTSTANDING        0x01    /* Delayed call has been scheduled */
#define BDCF_CANCELLING         0x02    /* May be waiting for call completion */

/*
 * Software state for each bridge.
 */
LIST_HEAD(_bridge_rtnode_list, bridge_rtnode);

struct bridge_softc {
	struct ifnet            *sc_ifp;        /* make this an interface */
	uint32_t                sc_flags;
	LIST_ENTRY(bridge_softc) sc_list;
	decl_lck_mtx_data(, sc_mtx);
	struct _bridge_rtnode_list * __counted_by(sc_rthash_size) sc_rthash;  /* our forwarding table */
	struct _bridge_rtnode_list sc_rtlist;   /* list version of above */
	uint32_t                sc_rthash_key;  /* key for hash */
	uint32_t                sc_rthash_size; /* size of the hash table */
	struct bridge_delayed_call sc_aging_timer;
	struct bridge_delayed_call sc_resize_call;
	TAILQ_HEAD(, bridge_iflist) sc_spanlist;        /* span ports list */
	struct bstp_state       sc_stp;         /* STP state */
	void                    *sc_cv;
	uint32_t                sc_brtmax;      /* max # of addresses */
	uint32_t                sc_brtcnt;      /* cur. # of addresses */
	uint32_t                sc_brttimeout;  /* rt timeout in seconds */
	uint32_t                sc_iflist_ref;  /* refcount for sc_iflist */
	uint32_t                sc_iflist_xcnt; /* refcount for sc_iflist */
	TAILQ_HEAD(, bridge_iflist) sc_iflist;  /* member interface list */
	uint32_t                sc_brtexceeded; /* # of cache drops */
	uint32_t                sc_filter_flags; /* ipf and flags */
	struct ifnet            *sc_ifaddr;     /* member mac copied from */
	u_char                  sc_defaddr[6];  /* Default MAC address */
	char                    sc_if_xname[IFNAMSIZ];

	struct bridge_iflist    *sc_mac_nat_bif; /* single MAC NAT interface */
	struct mac_nat_entry_list sc_mne_list;  /* MAC NAT IPv4 */
	struct mac_nat_entry_list sc_mne_list_v6;/* MAC NAT IPv6 */
	uint32_t                sc_mne_max;      /* max # of entries */
	uint32_t                sc_mne_count;    /* cur. # of entries */
	uint32_t                sc_mne_allocation_failures;
#if BRIDGE_LOCK_DEBUG
	/*
	 * Locking and unlocking calling history
	 */
	void                    *lock_lr[BR_LCKDBG_MAX];
	int                     next_lock_lr;
	void                    *unlock_lr[BR_LCKDBG_MAX];
	int                     next_unlock_lr;
#endif /* BRIDGE_LOCK_DEBUG */
};

#define SCF_DETACHING            0x01
#define SCF_RESIZING             0x02
#define SCF_MEDIA_ACTIVE         0x04
#define SCF_ADDRESS_ASSIGNED     0x08

typedef enum {
	CHECKSUM_OPERATION_NONE = 0,
	CHECKSUM_OPERATION_CLEAR_OFFLOAD = 1,
	CHECKSUM_OPERATION_FINALIZE = 2,
	CHECKSUM_OPERATION_COMPUTE = 3,
} ChecksumOperation;

typedef struct {
	u_int           ip_hlen;        /* IP header length */
	u_int           ip_pay_len;     /* length of payload (exclusive of ip_hlen) */
	u_int           ip_m0_len;      /* bytes available at ip_hdr (without jumping mbufs) */
	u_int           ip_opt_len;     /* IPv6 options headers length */
	uint8_t         ip_proto;       /* IPPROTO_TCP, IPPROTO_UDP, etc. */
	bool            ip_is_ipv4;
	bool            ip_is_fragmented;
	uint8_t         *__sized_by(ip_m0_len) ip_hdr;   /* pointer to IP header */
	uint8_t         *__indexable ip_proto_hdr;   /* ptr to protocol header (TCP) */
} ip_packet_info, *ip_packet_info_t;

struct bridge_hostfilter_stats bridge_hostfilter_stats;

static LCK_GRP_DECLARE(bridge_lock_grp, "if_bridge");
#if BRIDGE_LOCK_DEBUG
static LCK_ATTR_DECLARE(bridge_lock_attr, 0, 0);
#else
static LCK_ATTR_DECLARE(bridge_lock_attr, LCK_ATTR_DEBUG, 0);
#endif
static LCK_MTX_DECLARE_ATTR(bridge_list_mtx, &bridge_lock_grp, &bridge_lock_attr);

static int      bridge_rtable_prune_period = BRIDGE_RTABLE_PRUNE_PERIOD;

static KALLOC_TYPE_DEFINE(bridge_rtnode_pool, struct bridge_rtnode, NET_KT_DEFAULT);
static KALLOC_TYPE_DEFINE(bridge_mne_pool, struct mac_nat_entry, NET_KT_DEFAULT);

static int      bridge_clone_create(struct if_clone *, uint32_t, void *);
static int      bridge_clone_destroy(struct ifnet *);

static errno_t  bridge_ioctl(struct ifnet *, u_long cmd, void *__sized_by(IOCPARM_LEN(cmd)));
#if HAS_IF_CAP
static void     bridge_mutecaps(struct bridge_softc *);
static void     bridge_set_ifcap(struct bridge_softc *, struct bridge_iflist *,
    int);
#endif
static errno_t bridge_set_tso(struct bridge_softc *);
static void     bridge_proto_attach_changed(struct ifnet *);
static int      bridge_init(struct ifnet *);
static void     bridge_ifstop(struct ifnet *, int);
static int      bridge_output(struct ifnet *, struct mbuf *);
static void     bridge_finalize_cksum(struct ifnet *, struct mbuf *);
static void     bridge_start(struct ifnet *);
static errno_t  bridge_input(struct ifnet *, mbuf_t *);
static errno_t  bridge_iff_input(void *, ifnet_t, protocol_family_t,
    mbuf_t *, char **);
static errno_t  bridge_iff_output(void *, ifnet_t, protocol_family_t,
    mbuf_t *);
static errno_t  bridge_member_output(struct bridge_softc *sc, ifnet_t ifp,
    mbuf_t *m);

static int      bridge_enqueue(ifnet_t, struct ifnet *,
    struct ifnet *, struct mbuf *, ChecksumOperation);
static void     bridge_rtdelete(struct bridge_softc *, struct ifnet *ifp, int);

static void     bridge_forward(struct bridge_softc *, struct bridge_iflist *,
    struct mbuf *);

static void     bridge_aging_timer(struct bridge_softc *sc);

static void     bridge_broadcast(struct bridge_softc *, struct bridge_iflist *,
    struct mbuf *);
static void     bridge_span(struct bridge_softc *, struct mbuf *);

static int      bridge_rtupdate(struct bridge_softc *, const uint8_t[ETHER_ADDR_LEN],
    uint16_t, struct bridge_iflist *, int, uint8_t);
static struct ifnet *bridge_rtlookup(struct bridge_softc *, const uint8_t[ETHER_ADDR_LEN],
    uint16_t);
static void     bridge_rttrim(struct bridge_softc *);
static void     bridge_rtage(struct bridge_softc *);
static void     bridge_rtflush(struct bridge_softc *, int);
static int      bridge_rtdaddr(struct bridge_softc *, const uint8_t[ETHER_ADDR_LEN],
    uint16_t);

static int      bridge_rtable_init(struct bridge_softc *);
static void     bridge_rtable_fini(struct bridge_softc *);

static void     bridge_rthash_resize(struct bridge_softc *);

static int      bridge_rtnode_addr_cmp(const uint8_t[ETHER_ADDR_LEN], const uint8_t[ETHER_ADDR_LEN]);
static struct bridge_rtnode *bridge_rtnode_lookup(struct bridge_softc *,
    const uint8_t[ETHER_ADDR_LEN], uint16_t);
static int      bridge_rtnode_hash(struct bridge_softc *,
    struct bridge_rtnode *);
static int      bridge_rtnode_insert(struct bridge_softc *,
    struct bridge_rtnode *);
static void     bridge_rtnode_destroy(struct bridge_softc *,
    struct bridge_rtnode *);
#if BRIDGESTP
static void     bridge_rtable_expire(struct ifnet *, int);
static void     bridge_state_change(struct ifnet *, int);
#endif /* BRIDGESTP */

static struct bridge_iflist *bridge_lookup_member(struct bridge_softc *,
    char * __sized_by(IFNAMSIZ) name);
static struct bridge_iflist *bridge_lookup_member_if(struct bridge_softc *,
    struct ifnet *ifp);
static void     bridge_delete_member(struct bridge_softc *,
    struct bridge_iflist *);
static void     bridge_delete_span(struct bridge_softc *,
    struct bridge_iflist *);

static int      bridge_ioctl_add(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_del(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifflags(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sifflags(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_scache(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gcache(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifs32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifs64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_rts32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_rts64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_saddr32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_saddr64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sto(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gto(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_daddr32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_daddr64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_flush(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gpri(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_spri(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_ght(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sht(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gfd(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sfd(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gma(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sma(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sifprio(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sifcost(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sifmaxaddr(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_addspan(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_delspan(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gbparam32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gbparam64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_grte(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifsstp32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifsstp64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sproto(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_stxhc(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_purge(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gfilt(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_sfilt(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_ghostfilter(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_shostfilter(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gmnelist32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gmnelist64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifstats32(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);
static int      bridge_ioctl_gifstats64(struct bridge_softc *, void *__sized_by(arg_len) arg, size_t arg_len);

static int      bridge_pf(struct mbuf **, struct ifnet *,
    uint32_t sc_filter_flags, bool input);
static int bridge_ip_checkbasic(struct mbuf **);
static int bridge_ip6_checkbasic(struct mbuf **);

static void bridge_detach(ifnet_t);
static void bridge_link_event(struct ifnet *, u_int32_t);
static void bridge_iflinkevent(struct ifnet *);
static u_int32_t bridge_updatelinkstatus(struct bridge_softc *);
static int interface_media_active(struct ifnet *);
static void bridge_schedule_delayed_call(struct bridge_delayed_call *);
static void bridge_cancel_delayed_call(struct bridge_delayed_call *);
static void bridge_cleanup_delayed_call(struct bridge_delayed_call *);
static int bridge_host_filter(struct bridge_iflist *, mbuf_t *);

static errno_t bridge_mac_nat_enable(struct bridge_softc *,
    struct bridge_iflist *);
static void bridge_mac_nat_disable(struct bridge_softc *sc);
static void bridge_mac_nat_age_entries(struct bridge_softc *sc, unsigned long);
static void bridge_mac_nat_populate_entries(struct bridge_softc *sc);
static void bridge_mac_nat_flush_entries(struct bridge_softc *sc,
    struct bridge_iflist *);
static mbuf_t bridge_mac_nat_input(struct bridge_softc *, ifnet_t, mbuf_t,
    ifnet_t * dst_if);
static boolean_t bridge_mac_nat_output(struct bridge_softc *,
    struct bridge_iflist *, mbuf_t *, struct mac_nat_record *);
static void bridge_mac_nat_translate(mbuf_t *, struct mac_nat_record *,
    const char[ETHER_ADDR_LEN]);

static mblist bridge_mac_nat_input_list(struct bridge_softc *sc,
    ifnet_t external_ifp, mbuf_t m, mbuf_t * forward_head);
static mbuf_t bridge_mac_nat_translate_list(struct bridge_softc * sc,
    struct bridge_iflist *sbif, ifnet_t dst_if, mbuf_t m);
static mbuf_t bridge_mac_nat_copy_and_translate_list(struct bridge_softc * sc,
    struct bridge_iflist *sbif, ifnet_t dst_if, mbuf_t m);

static mbuf_t   bridge_pf_list(mbuf_t m, ifnet_t ifp,
    uint32_t sc_filter_flags, bool input);

static bool is_broadcast_ip_packet(mbuf_t *);
static bool in_addr_is_ours(const struct in_addr);
static bool in6_addr_is_ours(const struct in6_addr *, uint32_t);

#define m_copypacket(m, how) m_copym(m, 0, M_COPYALL, how)

static mblist
gso_tcp(ifnet_t ifp, mbuf_t m, u_int mac_hlen, bool is_ipv4, bool is_tx);

static inline mblist
gso_tcp_receive(ifnet_t ifp, mbuf_t m, u_int mac_hlen, bool is_ipv4)
{
	return gso_tcp(ifp, m, mac_hlen, is_ipv4, false);
}

static inline mblist
gso_tcp_transmit(ifnet_t ifp, mbuf_t m, u_int mac_hlen, bool is_ipv4)
{
	return gso_tcp(ifp, m, mac_hlen, is_ipv4, true);
}

/* The default bridge vlan is 1 (IEEE 802.1Q-2003 Table 9-2) */
#define VLANTAGOF(_m)   0

#define BSTP_ETHERADDR_RANGE_FIRST      0x00
#define BSTP_ETHERADDR_RANGE_LAST       0x0f

u_int8_t bstp_etheraddr[ETHER_ADDR_LEN] =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, BSTP_ETHERADDR_RANGE_FIRST };


static u_int8_t ethernulladdr[ETHER_ADDR_LEN] =
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#if BRIDGESTP
static struct bstp_cb_ops bridge_ops = {
	.bcb_state = bridge_state_change,
	.bcb_rtage = bridge_rtable_expire
};
#endif /* BRIDGESTP */

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_BRIDGE, bridge, CTLFLAG_RW | CTLFLAG_LOCKED, 0,
    "Bridge");

static int bridge_inherit_mac = 0;   /* share MAC with first bridge member */
SYSCTL_INT(_net_link_bridge, OID_AUTO, inherit_mac,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &bridge_inherit_mac, 0,
    "Inherit MAC address from the first bridge member");

SYSCTL_INT(_net_link_bridge, OID_AUTO, rtable_prune_period,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &bridge_rtable_prune_period, 0,
    "Interval between pruning of routing table");

static unsigned int bridge_rtable_hash_size_max = BRIDGE_RTHASH_SIZE_MAX;
SYSCTL_UINT(_net_link_bridge, OID_AUTO, rtable_hash_size_max,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &bridge_rtable_hash_size_max, 0,
    "Maximum size of the routing hash table");

#if BRIDGE_DELAYED_CALLBACK_DEBUG
static int bridge_delayed_callback_delay = 0;
SYSCTL_INT(_net_link_bridge, OID_AUTO, delayed_callback_delay,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &bridge_delayed_callback_delay, 0,
    "Delay before calling delayed function");
#endif

SYSCTL_STRUCT(_net_link_bridge, OID_AUTO,
    hostfilterstats, CTLFLAG_RD | CTLFLAG_LOCKED,
    &bridge_hostfilter_stats, bridge_hostfilter_stats, "");

#if BRIDGESTP
static int log_stp   = 0;   /* log STP state changes */
SYSCTL_INT(_net_link_bridge, OID_AUTO, log_stp, CTLFLAG_RW,
    &log_stp, 0, "Log STP state changes");
#endif /* BRIDGESTP */

struct bridge_control {
	int             (*bc_func)(struct bridge_softc *, void *__sized_by(arg_len) args, size_t arg_len);
	unsigned int    bc_argsize;
	unsigned int    bc_flags;
};

#define BC_F_COPYIN             0x01    /* copy arguments in */
#define BC_F_COPYOUT            0x02    /* copy arguments out */
#define BC_F_SUSER              0x04    /* do super-user check */

static const struct bridge_control bridge_control_table32[] = {
	{ .bc_func = bridge_ioctl_add, .bc_argsize = sizeof(struct ifbreq),             /* 0 */
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_del, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gifflags, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sifflags, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_scache, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_gcache, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_gifs32, .bc_argsize = sizeof(struct ifbifconf32),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_rts32, .bc_argsize = sizeof(struct ifbaconf32),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_saddr32, .bc_argsize = sizeof(struct ifbareq32),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sto, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_gto, .bc_argsize = sizeof(struct ifbrparam),           /* 10 */
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_daddr32, .bc_argsize = sizeof(struct ifbareq32),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_flush, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gpri, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_spri, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_ght, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sht, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gfd, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sfd, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gma, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sma, .bc_argsize = sizeof(struct ifbrparam),           /* 20 */
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sifprio, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sifcost, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gfilt, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sfilt, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_purge, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_addspan, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_delspan, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gbparam32, .bc_argsize = sizeof(struct ifbropreq32),
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_grte, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_gifsstp32, .bc_argsize = sizeof(struct ifbpstpconf32),     /* 30 */
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_sproto, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_stxhc, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sifmaxaddr, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_ghostfilter, .bc_argsize = sizeof(struct ifbrhostfilter),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_shostfilter, .bc_argsize = sizeof(struct ifbrhostfilter),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gmnelist32,
	  .bc_argsize = sizeof(struct ifbrmnelist32),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_gifstats32,
	  .bc_argsize = sizeof(struct ifbrmreq32),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
};

static const struct bridge_control bridge_control_table64[] = {
	{ .bc_func = bridge_ioctl_add, .bc_argsize = sizeof(struct ifbreq),           /* 0 */
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_del, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gifflags, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sifflags, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_scache, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_gcache, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_gifs64, .bc_argsize = sizeof(struct ifbifconf64),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_rts64, .bc_argsize = sizeof(struct ifbaconf64),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_saddr64, .bc_argsize = sizeof(struct ifbareq64),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sto, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_gto, .bc_argsize = sizeof(struct ifbrparam),           /* 10 */
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_daddr64, .bc_argsize = sizeof(struct ifbareq64),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_flush, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gpri, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_spri, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_ght, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sht, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gfd, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sfd, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gma, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sma, .bc_argsize = sizeof(struct ifbrparam),           /* 20 */
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sifprio, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sifcost, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gfilt, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_sfilt, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_purge, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_addspan, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },
	{ .bc_func = bridge_ioctl_delspan, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gbparam64, .bc_argsize = sizeof(struct ifbropreq64),
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_grte, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_gifsstp64, .bc_argsize = sizeof(struct ifbpstpconf64),     /* 30 */
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },

	{ .bc_func = bridge_ioctl_sproto, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_stxhc, .bc_argsize = sizeof(struct ifbrparam),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_sifmaxaddr, .bc_argsize = sizeof(struct ifbreq),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_ghostfilter, .bc_argsize = sizeof(struct ifbrhostfilter),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_shostfilter, .bc_argsize = sizeof(struct ifbrhostfilter),
	  .bc_flags = BC_F_COPYIN | BC_F_SUSER },

	{ .bc_func = bridge_ioctl_gmnelist64,
	  .bc_argsize = sizeof(struct ifbrmnelist64),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
	{ .bc_func = bridge_ioctl_gifstats64,
	  .bc_argsize = sizeof(struct ifbrmreq64),
	  .bc_flags = BC_F_COPYIN | BC_F_COPYOUT },
};

static const unsigned int bridge_control_table_size =
    sizeof(bridge_control_table32) / sizeof(bridge_control_table32[0]);

static LIST_HEAD(, bridge_softc) bridge_list =
    LIST_HEAD_INITIALIZER(bridge_list);

#define BRIDGENAME      "bridge"
#define BRIDGES_MAX     IF_MAXUNIT
#define BRIDGE_ZONE_MAX_ELEM    MIN(IFNETS_MAX, BRIDGES_MAX)

static struct if_clone bridge_cloner =
    IF_CLONE_INITIALIZER(BRIDGENAME, bridge_clone_create, bridge_clone_destroy,
    0, BRIDGES_MAX);

static int if_bridge_txstart = 0;
SYSCTL_INT(_net_link_bridge, OID_AUTO, txstart, CTLFLAG_RW | CTLFLAG_LOCKED,
    &if_bridge_txstart, 0, "Bridge interface uses TXSTART model");

SYSCTL_INT(_net_link_bridge, OID_AUTO, debug, CTLFLAG_RW | CTLFLAG_LOCKED,
    &if_bridge_debug, 0, "Bridge debug flags");

SYSCTL_INT(_net_link_bridge, OID_AUTO, log_level,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &if_bridge_log_level, 0, "Bridge log level");

static int if_bridge_segmentation = 1;
SYSCTL_INT(_net_link_bridge, OID_AUTO, segmentation,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &if_bridge_segmentation, 0, "Bridge interface enable segmentation");

static int if_bridge_output_skip_filters = 1;
SYSCTL_INT(_net_link_bridge, OID_AUTO, output_skip_filters,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &if_bridge_output_skip_filters, 0, "Bridge skip output filters");

int bridge_enable_early_input = 1;   /* DLIL early input */
SYSCTL_INT(_net_link_bridge, OID_AUTO, enable_early_input,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &bridge_enable_early_input, 0,
    "Bridge enable early input");

#define BRIDGE_TSO_REDUCE_MSS_FORWARDING_MAX            256
#define BRIDGE_TSO_REDUCE_MSS_FORWARDING_DEFAULT        110
#define BRIDGE_TSO_REDUCE_MSS_TX_MAX                    256
#define BRIDGE_TSO_REDUCE_MSS_TX_DEFAULT                0

static u_int if_bridge_tso_reduce_mss_forwarding
        = BRIDGE_TSO_REDUCE_MSS_FORWARDING_DEFAULT;
static u_int if_bridge_tso_reduce_mss_tx
        = BRIDGE_TSO_REDUCE_MSS_TX_DEFAULT;

static int
bridge_tso_reduce_mss(struct sysctl_req *req, u_int * val, u_int val_max)
{
	int     changed;
	int     error;
	u_int   new_value;

	error = sysctl_io_number(req, *val, sizeof(*val), &new_value,
	    &changed);
	if (error == 0 && changed != 0) {
		if (new_value > val_max) {
			return EINVAL;
		}
		*val = new_value;
	}
	return error;
}

static int
bridge_tso_reduce_mss_forwarding_sysctl SYSCTL_HANDLER_ARGS
{
	return bridge_tso_reduce_mss(req, &if_bridge_tso_reduce_mss_forwarding,
    BRIDGE_TSO_REDUCE_MSS_FORWARDING_MAX);
}

SYSCTL_PROC(_net_link_bridge, OID_AUTO, tso_reduce_mss_forwarding,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, bridge_tso_reduce_mss_forwarding_sysctl, "IU",
    "Bridge tso reduce mss when forwarding");

static int
bridge_tso_reduce_mss_tx_sysctl SYSCTL_HANDLER_ARGS
{
	return bridge_tso_reduce_mss(req, &if_bridge_tso_reduce_mss_tx,
    BRIDGE_TSO_REDUCE_MSS_TX_MAX);
}

SYSCTL_PROC(_net_link_bridge, OID_AUTO, tso_reduce_mss_tx,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, bridge_tso_reduce_mss_tx_sysctl, "IU",
    "Bridge tso reduce mss on transmit");

#if DEBUG || DEVELOPMENT
/*
 * net.link.bridge.reduce_tso_mtu
 * - when non-zero, the bridge overrides the interface TSO MTU to a lower
 *   value (i.e. 16K) to enable testing the "use GSO instead" path
 */
static int if_bridge_reduce_tso_mtu = 0;
SYSCTL_INT(_net_link_bridge, OID_AUTO, reduce_tso_mtu,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &if_bridge_reduce_tso_mtu, 0, "Bridge interface reduce TSO MTU");

#endif /* DEBUG || DEVELOPMENT */

static void brlog_ether_header(struct ether_header *);
static void brlog_mbuf_data(mbuf_t, size_t, size_t);
static void brlog_mbuf_pkthdr(mbuf_t, const char *, const char *);
static void brlog_mbuf(mbuf_t, const char *, const char *);
static void brlog_link(struct bridge_softc * sc);

#if BRIDGE_LOCK_DEBUG
static void bridge_lock(struct bridge_softc *);
static void bridge_unlock(struct bridge_softc *);
static int bridge_lock2ref(struct bridge_softc *);
static void bridge_unref(struct bridge_softc *);
static void bridge_xlock(struct bridge_softc *);
static void bridge_xdrop(struct bridge_softc *);

#define DECL_RETURN_ADDR(v) void * __single v = __unsafe_forge_single(void *, __builtin_return_address(0))

static void
bridge_lock(struct bridge_softc *sc)
{
	DECL_RETURN_ADDR(lr_saved);

	BRIDGE_LOCK_ASSERT_NOTHELD(sc);

	_BRIDGE_LOCK(sc);

	sc->lock_lr[sc->next_lock_lr] = lr_saved;
	sc->next_lock_lr = (sc->next_lock_lr + 1) % SO_LCKDBG_MAX;
}

static void
bridge_unlock(struct bridge_softc *sc)
{
	DECL_RETURN_ADDR(lr_saved);

	BRIDGE_LOCK_ASSERT_HELD(sc);

	sc->unlock_lr[sc->next_unlock_lr] = lr_saved;
	sc->next_unlock_lr = (sc->next_unlock_lr + 1) % SO_LCKDBG_MAX;

	_BRIDGE_UNLOCK(sc);
}

static int
bridge_lock2ref(struct bridge_softc *sc)
{
	int error = 0;
	DECL_RETURN_ADDR(lr_saved);

	BRIDGE_LOCK_ASSERT_HELD(sc);

	if (sc->sc_iflist_xcnt > 0) {
		error = EBUSY;
	} else {
		sc->sc_iflist_ref++;
	}

	sc->unlock_lr[sc->next_unlock_lr] = lr_saved;
	sc->next_unlock_lr = (sc->next_unlock_lr + 1) % SO_LCKDBG_MAX;

	_BRIDGE_UNLOCK(sc);

	return error;
}

static void
bridge_unref(struct bridge_softc *sc)
{
	DECL_RETURN_ADDR(lr_saved);

	BRIDGE_LOCK_ASSERT_NOTHELD(sc);

	_BRIDGE_LOCK(sc);
	sc->lock_lr[sc->next_lock_lr] = lr_saved;
	sc->next_lock_lr = (sc->next_lock_lr + 1) % SO_LCKDBG_MAX;

	sc->sc_iflist_ref--;

	sc->unlock_lr[sc->next_unlock_lr] = lr_saved;
	sc->next_unlock_lr = (sc->next_unlock_lr + 1) % SO_LCKDBG_MAX;
	if ((sc->sc_iflist_xcnt > 0) && (sc->sc_iflist_ref == 0)) {
		_BRIDGE_UNLOCK(sc);
		wakeup(&sc->sc_cv);
	} else {
		_BRIDGE_UNLOCK(sc);
	}
}

static void
bridge_xlock(struct bridge_softc *sc)
{
	DECL_RETURN_ADDR(lr_saved);

	BRIDGE_LOCK_ASSERT_HELD(sc);

	sc->sc_iflist_xcnt++;
	while (sc->sc_iflist_ref > 0) {
		sc->unlock_lr[sc->next_unlock_lr] = lr_saved;
		sc->next_unlock_lr = (sc->next_unlock_lr + 1) % SO_LCKDBG_MAX;

		msleep(&sc->sc_cv, &sc->sc_mtx, PZERO, "BRIDGE_XLOCK", NULL);

		sc->lock_lr[sc->next_lock_lr] = lr_saved;
		sc->next_lock_lr = (sc->next_lock_lr + 1) % SO_LCKDBG_MAX;
	}
}

#undef DECL_RETURN_ADDR

static void
bridge_xdrop(struct bridge_softc *sc)
{
	BRIDGE_LOCK_ASSERT_HELD(sc);

	sc->sc_iflist_xcnt--;
}

#endif /* BRIDGE_LOCK_DEBUG */

static void
brlog_mbuf_pkthdr(mbuf_t m, const char *prefix, const char *suffix)
{
	if (m) {
		BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0,
		    "%spktlen: %u rcvif: 0x%llx header: 0x%llx nextpkt: 0x%llx%s",
		    prefix ? prefix : "", (unsigned int)mbuf_pkthdr_len(m),
		    (uint64_t)VM_KERNEL_ADDRPERM(mbuf_pkthdr_rcvif(m)),
		    (uint64_t)VM_KERNEL_ADDRPERM(mbuf_pkthdr_header(m)),
		    (uint64_t)VM_KERNEL_ADDRPERM(mbuf_nextpkt(m)),
		    suffix ? suffix : "");
	} else {
		BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0, "%s<NULL>%s", prefix, suffix);
	}
}

static void
brlog_mbuf(mbuf_t m, const char *prefix, const char *suffix)
{
	if (m) {
		BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0,
		    "%s0x%llx type: %u flags: 0x%x len: %u data: 0x%llx "
		    "maxlen: %u datastart: 0x%llx next: 0x%llx%s",
		    prefix ? prefix : "", (uint64_t)VM_KERNEL_ADDRPERM(m),
		    mbuf_type(m), mbuf_flags(m), (unsigned int)mbuf_len(m),
		    (uint64_t)VM_KERNEL_ADDRPERM(mtod(m, void *)),
		    (unsigned int)mbuf_maxlen(m),
		    (uint64_t)VM_KERNEL_ADDRPERM(mbuf_datastart(m)),
		    (uint64_t)VM_KERNEL_ADDRPERM(mbuf_next(m)),
		    !suffix || (mbuf_flags(m) & MBUF_PKTHDR) ? "" : suffix);
		if ((mbuf_flags(m) & MBUF_PKTHDR)) {
			brlog_mbuf_pkthdr(m, "", suffix);
		}
	} else {
		BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0, "%s<NULL>%s", prefix, suffix);
	}
}

static void
brlog_mbuf_data(mbuf_t m, size_t offset, size_t len)
{
	mbuf_t                  n;
	size_t                  i, j;
	size_t                  pktlen, mlen, maxlen;
	unsigned char   *ptr;

	pktlen = mbuf_pkthdr_len(m);

	if (offset > pktlen) {
		return;
	}

	maxlen = (pktlen - offset > len) ? len : pktlen - offset;
	n = m;
	mlen = mbuf_len(n);
	ptr = mtod(n, unsigned char *);
	for (i = 0, j = 0; i < maxlen; i++, j++) {
		if (j >= mlen) {
			n = mbuf_next(n);
			if (n == 0) {
				break;
			}
			ptr = mtod(n, unsigned char *);
			mlen = mbuf_len(n);
			j = 0;
		}
		if (i >= offset) {
			BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0,
			    "%02x%s", ptr[j], i % 2 ? " " : "");
		}
	}
}

static void
brlog_ether_header(struct ether_header *eh)
{
	BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0,
	    "%02x:%02x:%02x:%02x:%02x:%02x > "
	    "%02x:%02x:%02x:%02x:%02x:%02x 0x%04x ",
	    eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2],
	    eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5],
	    eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
	    eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5],
	    ntohs(eh->ether_type));
}

static char *
ether_ntop(char * __sized_by(len) buf, size_t len, const u_char ap[ETHER_ADDR_LEN])
{
	snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
	    ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);

	return buf;
}

static void
brlog_link(struct bridge_softc * sc)
{
	int i;
	uint32_t sdl_buffer[(offsetof(struct sockaddr_dl, sdl_data) +
	IFNAMSIZ + ETHER_ADDR_LEN)];
	struct sockaddr_dl *sdl = SDL((uint8_t*)&sdl_buffer); /* SDL requires byte pointer */
	const u_char * lladdr;
	char lladdr_str[48];

	memset(sdl_buffer, 0, sizeof(sdl_buffer));
	sdl->sdl_family = AF_LINK;
	sdl->sdl_nlen = strbuflen(sc->sc_if_xname);
	sdl->sdl_alen = ETHER_ADDR_LEN;
	sdl->sdl_len = offsetof(struct sockaddr_dl, sdl_data);
	memcpy(sdl->sdl_data, sc->sc_if_xname, sdl->sdl_nlen);
	memcpy(LLADDR(sdl), sc->sc_defaddr, ETHER_ADDR_LEN);
	lladdr_str[0] = '\0';
	for (i = 0, lladdr = CONST_LLADDR(sdl);
	    i < sdl->sdl_alen;
	    i++, lladdr++) {
		char    byte_str[4];

		snprintf(byte_str, sizeof(byte_str), "%s%x", i ? ":" : "",
		    *lladdr);
		strbufcat(lladdr_str, byte_str);
	}
	BRIDGE_LOG_SIMPLE(LOG_NOTICE, 0,
	    "%s sdl len %d index %d family %d type 0x%x nlen %d alen %d"
	    " slen %d addr %s", sc->sc_if_xname,
	    sdl->sdl_len, sdl->sdl_index,
	    sdl->sdl_family, sdl->sdl_type, sdl->sdl_nlen,
	    sdl->sdl_alen, sdl->sdl_slen, lladdr_str);
}


/*
 * bridgeattach:
 *
 *	Pseudo-device attach routine.
 */
__private_extern__ int
bridgeattach(int n)
{
#pragma unused(n)
	int error;

	LIST_INIT(&bridge_list);

#if BRIDGESTP
	bstp_sys_init();
#endif /* BRIDGESTP */

	error = if_clone_attach(&bridge_cloner);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_clone_attach failed %d", error);
	}
	return error;
}

static void
_mbuf_adjust_pkthdr_and_data(mbuf_t m, int len)
{
	mbuf_setdata(m, mtodo(m, len), mbuf_len(m) - len);
	mbuf_pkthdr_adjustlen(m, -len);
}

static errno_t
bridge_ifnet_set_attrs(struct ifnet * ifp)
{
	errno_t         error;

	error = ifnet_set_mtu(ifp, ETHERMTU);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_mtu failed %d", error);
		goto done;
	}
	error = ifnet_set_addrlen(ifp, ETHER_ADDR_LEN);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_addrlen failed %d", error);
		goto done;
	}
	error = ifnet_set_hdrlen(ifp, ETHER_HDR_LEN);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_hdrlen failed %d", error);
		goto done;
	}
	error = ifnet_set_flags(ifp,
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST,
	    0xffff);

	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_flags failed %d", error);
		goto done;
	}
done:
	return error;
}

/*
 * bridge_clone_create:
 *
 *	Create a new bridge instance.
 */
static int
bridge_clone_create(struct if_clone *ifc, uint32_t unit, void *params)
{
#pragma unused(params)
	ifnet_ref_t ifp = NULL;
	struct bridge_softc *sc = NULL;
	struct bridge_softc *sc2 = NULL;
	struct ifnet_init_eparams init_params;
	errno_t error = 0;
	uint8_t eth_hostid[ETHER_ADDR_LEN];
	int fb, retry, has_hostid;

	sc = kalloc_type(struct bridge_softc, Z_WAITOK_ZERO_NOFAIL);
	lck_mtx_init(&sc->sc_mtx, &bridge_lock_grp, &bridge_lock_attr);
	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_mne_max = BRIDGE_MAC_NAT_ENTRY_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
	sc->sc_filter_flags = 0;

	TAILQ_INIT(&sc->sc_iflist);

	/* use the interface name as the unique id for ifp recycle */
	snprintf(sc->sc_if_xname, sizeof(sc->sc_if_xname), "%s%d",
	    ifc->ifc_name, unit);
	bzero(&init_params, sizeof(init_params));
	init_params.ver                 = IFNET_INIT_CURRENT_VERSION;
	init_params.len                 = sizeof(init_params);
	/* Initialize our routing table. */
	error = bridge_rtable_init(sc);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "bridge_rtable_init failed %d", error);
		goto done;
	}
	TAILQ_INIT(&sc->sc_spanlist);
	if (if_bridge_txstart) {
		init_params.start = bridge_start;
	} else {
		init_params.flags = IFNET_INIT_LEGACY;
		init_params.output = bridge_output;
	}
	init_params.uniqueid_len        = strbuflen(sc->sc_if_xname);
	init_params.uniqueid            = sc->sc_if_xname;
	init_params.sndq_maxlen         = IFQ_MAXLEN;
	init_params.name                = __unsafe_null_terminated_from_indexable(ifc->ifc_name);
	init_params.unit                = unit;
	init_params.family              = IFNET_FAMILY_ETHERNET;
	init_params.type                = IFT_BRIDGE;
	init_params.demux               = ether_demux;
	init_params.add_proto           = ether_add_proto;
	init_params.del_proto           = ether_del_proto;
	init_params.check_multi         = ether_check_multi;
	init_params.framer_extended     = ether_frameout_extended;
	init_params.softc               = sc;
	init_params.ioctl               = bridge_ioctl;
	init_params.detach              = bridge_detach;
	init_params.broadcast_addr      = etherbroadcastaddr;
	init_params.broadcast_len       = ETHER_ADDR_LEN;

	error = ifnet_allocate_extended(&init_params, &ifp);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_allocate failed %d", error);
		goto done;
	}
	LIST_INIT(&sc->sc_mne_list);
	LIST_INIT(&sc->sc_mne_list_v6);
	sc->sc_ifp = ifp;
	error = bridge_ifnet_set_attrs(ifp);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "bridge_ifnet_set_attrs failed %d",
		    error);
		goto done;
	}
	/*
	 * Generate an ethernet address with a locally administered address.
	 *
	 * Since we are using random ethernet addresses for the bridge, it is
	 * possible that we might have address collisions, so make sure that
	 * this hardware address isn't already in use on another bridge.
	 * The first try uses the "hostid" and falls back to read_frandom();
	 * for "hostid", we use the MAC address of the first-encountered
	 * Ethernet-type interface that is currently configured.
	 */
	fb = 0;
	has_hostid = (uuid_get_ethernet(&eth_hostid[0]) == 0);
	for (retry = 1; retry != 0;) {
		if (fb || has_hostid == 0) {
			read_frandom(&sc->sc_defaddr, ETHER_ADDR_LEN);
			sc->sc_defaddr[0] &= ~1; /* clear multicast bit */
			sc->sc_defaddr[0] |= 2;  /* set the LAA bit */
		} else {
			bcopy(&eth_hostid[0], &sc->sc_defaddr,
			    ETHER_ADDR_LEN);
			sc->sc_defaddr[0] &= ~1; /* clear multicast bit */
			sc->sc_defaddr[0] |= 2;  /* set the LAA bit */
			sc->sc_defaddr[3] =     /* stir it up a bit */
			    ((sc->sc_defaddr[3] & 0x0f) << 4) |
			    ((sc->sc_defaddr[3] & 0xf0) >> 4);
			/*
			 * Mix in the LSB as it's actually pretty significant,
			 * see rdar://14076061
			 */
			sc->sc_defaddr[4] =
			    (((sc->sc_defaddr[4] & 0x0f) << 4) |
			    ((sc->sc_defaddr[4] & 0xf0) >> 4)) ^
			    sc->sc_defaddr[5];
			sc->sc_defaddr[5] = ifp->if_unit & 0xff;
		}

		fb = 1;
		retry = 0;
		lck_mtx_lock(&bridge_list_mtx);
		LIST_FOREACH(sc2, &bridge_list, sc_list) {
			if (_ether_cmp(sc->sc_defaddr,
			    IF_LLADDR(sc2->sc_ifp)) == 0) {
				retry = 1;
			}
		}
		lck_mtx_unlock(&bridge_list_mtx);
	}

	sc->sc_flags &= ~SCF_MEDIA_ACTIVE;

	if (BRIDGE_DBGF_ENABLED(BR_DBGF_LIFECYCLE)) {
		brlog_link(sc);
	}
	error = ifnet_attach(ifp, NULL);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_attach failed %d", error);
		goto done;
	}

	error = ifnet_set_lladdr_and_type(ifp, sc->sc_defaddr, ETHER_ADDR_LEN,
	    IFT_ETHER);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_lladdr_and_type failed %d",
		    error);
		goto done;
	}

	ifnet_set_offload(ifp,
	    IFNET_CSUM_IP | IFNET_CSUM_TCP | IFNET_CSUM_UDP |
	    IFNET_CSUM_TCPIPV6 | IFNET_CSUM_UDPIPV6 | IFNET_MULTIPAGES);
	error = bridge_set_tso(sc);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "bridge_set_tso failed %d", error);
		goto done;
	}
#if BRIDGESTP
	bstp_attach(&sc->sc_stp, &bridge_ops);
#endif /* BRIDGESTP */

	lck_mtx_lock(&bridge_list_mtx);
	LIST_INSERT_HEAD(&bridge_list, sc, sc_list);
	lck_mtx_unlock(&bridge_list_mtx);

	/* attach as ethernet */
	error = bpf_attach(ifp, DLT_EN10MB, sizeof(struct ether_header),
	    NULL, NULL);

done:
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "failed error %d", error);
		/* TBD: Clean up: sc, sc_rthash etc */
	}

	return error;
}

/*
 * bridge_clone_destroy:
 *
 *	Destroy a bridge instance.
 */
static int
bridge_clone_destroy(struct ifnet *ifp)
{
	struct bridge_softc * __single sc = ifp->if_softc;
	struct bridge_iflist *bif;
	errno_t error;

	BRIDGE_LOCK(sc);
	if ((sc->sc_flags & SCF_DETACHING)) {
		BRIDGE_UNLOCK(sc);
		return 0;
	}
	sc->sc_flags |= SCF_DETACHING;

	bridge_ifstop(ifp, 1);

	bridge_cancel_delayed_call(&sc->sc_resize_call);

	bridge_cleanup_delayed_call(&sc->sc_resize_call);
	bridge_cleanup_delayed_call(&sc->sc_aging_timer);

	error = ifnet_set_flags(ifp, 0, IFF_UP);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_flags failed %d", error);
	}

	while ((bif = TAILQ_FIRST(&sc->sc_iflist)) != NULL) {
		bridge_delete_member(sc, bif);
	}

	while ((bif = TAILQ_FIRST(&sc->sc_spanlist)) != NULL) {
		bridge_delete_span(sc, bif);
	}
	BRIDGE_UNLOCK(sc);

	error = ifnet_detach(ifp);
	if (error != 0) {
		panic("%s (%d): ifnet_detach(%p) failed %d",
		    __func__, __LINE__, ifp, error);
	}
	return 0;
}

#define DRVSPEC do { \
	if (ifd->ifd_cmd >= bridge_control_table_size) {                \
	        error = EINVAL;                                         \
	        break;                                                  \
	}                                                               \
	bc = &bridge_control_table[ifd->ifd_cmd];                       \
                                                                        \
	if (cmd == SIOCGDRVSPEC &&                                      \
	    (bc->bc_flags & BC_F_COPYOUT) == 0) {                       \
	        error = EINVAL;                                         \
	        break;                                                  \
	} else if (cmd == SIOCSDRVSPEC &&                               \
	    (bc->bc_flags & BC_F_COPYOUT) != 0) {                       \
	        error = EINVAL;                                         \
	        break;                                                  \
	}                                                               \
                                                                        \
	if (bc->bc_flags & BC_F_SUSER) {                                \
	        error = kauth_authorize_generic(kauth_cred_get(),       \
	            KAUTH_GENERIC_ISSUSER);                             \
	        if (error)                                              \
	                break;                                          \
	}                                                               \
                                                                        \
	if (ifd->ifd_len != bc->bc_argsize ||                           \
	    ifd->ifd_len > sizeof (args)) {                             \
	        error = EINVAL;                                         \
	        break;                                                  \
	}                                                               \
                                                                        \
	bzero(&args, sizeof (args));                                    \
	if (bc->bc_flags & BC_F_COPYIN) {                               \
	        error = copyin(ifd->ifd_data, &args, ifd->ifd_len);     \
	        if (error)                                              \
	                break;                                          \
	}                                                               \
                                                                        \
	BRIDGE_LOCK(sc);                                                \
	error = (*bc->bc_func)(sc, &args, sizeof(args));                \
	BRIDGE_UNLOCK(sc);                                              \
	if (error)                                                      \
	        break;                                                  \
                                                                        \
	if (bc->bc_flags & BC_F_COPYOUT)                                \
	        error = copyout(&args, ifd->ifd_data, ifd->ifd_len);    \
} while (0)

static boolean_t
interface_needs_input_broadcast(struct ifnet * ifp)
{
	/*
	 * Selectively enable input broadcast only when necessary.
	 * The bridge interface itself attaches a fake protocol
	 * so checking for at least two protocols means that the
	 * interface is being used for something besides bridging
	 * and needs to see broadcast packets from other members.
	 */
	return if_get_protolist(ifp, NULL, 0) >= 2;
}

static boolean_t
bif_set_input_broadcast(struct bridge_iflist * bif, boolean_t input_broadcast)
{
	boolean_t       old_input_broadcast;

	old_input_broadcast = (bif->bif_flags & BIFF_INPUT_BROADCAST) != 0;
	if (input_broadcast) {
		bif->bif_flags |= BIFF_INPUT_BROADCAST;
	} else {
		bif->bif_flags &= ~BIFF_INPUT_BROADCAST;
	}
	return old_input_broadcast != input_broadcast;
}

/*
 * bridge_ioctl:
 *
 *	Handle a control request from the operator.
 */
static errno_t
bridge_ioctl(struct ifnet *ifp, u_long cmd, void *__sized_by(IOCPARM_LEN(cmd)) data)
{
	struct bridge_softc * __single sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct bridge_iflist *bif;
	int error = 0;

	BRIDGE_LOCK_ASSERT_NOTHELD(sc);

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_IOCTL,
	    "ifp %s cmd 0x%08lx (%c%c [%lu] %c %lu)",
	    ifp->if_xname, cmd, (cmd & IOC_IN) ? 'I' : ' ',
	    (cmd & IOC_OUT) ? 'O' : ' ', IOCPARM_LEN(cmd),
	    (char)IOCGROUP(cmd), cmd & 0xff);

	switch (cmd) {
	case SIOCAIFADDR_IN6_32:
	case SIOCAIFADDR_IN6_64:
	case SIOCSIFADDR:
	case SIOCAIFADDR:
		ifnet_set_flags(ifp, IFF_UP, IFF_UP);
		BRIDGE_LOCK(sc);
		sc->sc_flags |= SCF_ADDRESS_ASSIGNED;
		BRIDGE_UNLOCK(sc);
		BRIDGE_LOG(LOG_NOTICE, 0,
		    "ifp %s has address", ifp->if_xname);
		break;

	case SIOCGIFMEDIA32:
	case SIOCGIFMEDIA64: {
		// cast to 32bit version to work within bounds with 32bit userspace
		struct ifmediareq32 *ifmr = (struct ifmediareq32 *)data;
		user_addr_t user_addr;

		user_addr = (cmd == SIOCGIFMEDIA64) ?
		    ((struct ifmediareq64 *)data)->ifmu_ulist :
		    CAST_USER_ADDR_T(((struct ifmediareq32 *)data)->ifmu_ulist);

		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_mask = 0;
		ifmr->ifm_count = 1;

		BRIDGE_LOCK(sc);
		if (!(sc->sc_flags & SCF_DETACHING) &&
		    (sc->sc_flags & SCF_MEDIA_ACTIVE)) {
			ifmr->ifm_status |= IFM_ACTIVE;
			ifmr->ifm_active = ifmr->ifm_current =
			    IFM_ETHER | IFM_AUTO;
		} else {
			ifmr->ifm_active = ifmr->ifm_current = IFM_NONE;
		}
		BRIDGE_UNLOCK(sc);

		if (user_addr != USER_ADDR_NULL) {
			error = copyout(&ifmr->ifm_current, user_addr,
			    sizeof(int));
		}
		break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSDRVSPEC32:
	case SIOCGDRVSPEC32: {
		union {
			struct ifbreq ifbreq;
			struct ifbifconf32 ifbifconf;
			struct ifbareq32 ifbareq;
			struct ifbaconf32 ifbaconf;
			struct ifbrparam ifbrparam;
			struct ifbropreq32 ifbropreq;
		} args;
		struct ifdrv32 *ifd = (struct ifdrv32 *)data;
		const struct bridge_control *bridge_control_table =
		    bridge_control_table32, *bc;

		DRVSPEC;

		break;
	}
	case SIOCSDRVSPEC64:
	case SIOCGDRVSPEC64: {
		union {
			struct ifbreq ifbreq;
			struct ifbifconf64 ifbifconf;
			struct ifbareq64 ifbareq;
			struct ifbaconf64 ifbaconf;
			struct ifbrparam ifbrparam;
			struct ifbropreq64 ifbropreq;
		} args;
		struct ifdrv64 *ifd = (struct ifdrv64 *)data;
		const struct bridge_control *bridge_control_table =
		    bridge_control_table64, *bc;

		DRVSPEC;

		break;
	}

	case SIOCSIFFLAGS:
		if (!(ifp->if_flags & IFF_UP) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			BRIDGE_LOCK(sc);
			bridge_ifstop(ifp, 1);
			BRIDGE_UNLOCK(sc);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_flags & IFF_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			BRIDGE_LOCK(sc);
			error = bridge_init(ifp);
			BRIDGE_UNLOCK(sc);
		}
		break;

	case SIOCSIFLLADDR:
		error = ifnet_set_lladdr(ifp, ifr->ifr_addr.sa_data,
		    ifr->ifr_addr.sa_len);
		if (error != 0) {
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_IOCTL,
			    "%s SIOCSIFLLADDR error %d", ifp->if_xname,
			    error);
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		BRIDGE_LOCK(sc);
		if (TAILQ_EMPTY(&sc->sc_iflist)) {
			sc->sc_ifp->if_mtu = ifr->ifr_mtu;
			BRIDGE_UNLOCK(sc);
			break;
		}
		TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
			if (bif->bif_ifp->if_mtu != (unsigned)ifr->ifr_mtu) {
				BRIDGE_LOG(LOG_NOTICE, 0,
				    "%s invalid MTU: %u(%s) != %d",
				    sc->sc_ifp->if_xname,
				    bif->bif_ifp->if_mtu,
				    bif->bif_ifp->if_xname, ifr->ifr_mtu);
				error = EINVAL;
				break;
			}
		}
		if (!error) {
			sc->sc_ifp->if_mtu = ifr->ifr_mtu;
		}
		BRIDGE_UNLOCK(sc);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error != 0 && error != EOPNOTSUPP) {
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_IOCTL,
			    "ifp %s cmd 0x%08lx "
			    "(%c%c [%lu] %c %lu) failed error: %d",
			    ifp->if_xname, cmd,
			    (cmd & IOC_IN) ? 'I' : ' ',
			    (cmd & IOC_OUT) ? 'O' : ' ',
			    IOCPARM_LEN(cmd), (char)IOCGROUP(cmd),
			    cmd & 0xff, error);
		}
		break;
	}
	BRIDGE_LOCK_ASSERT_NOTHELD(sc);

	return error;
}

#if HAS_IF_CAP
/*
 * bridge_mutecaps:
 *
 *	Clear or restore unwanted capabilities on the member interface
 */
static void
bridge_mutecaps(struct bridge_softc *sc)
{
	struct bridge_iflist *bif;
	int enabled, mask;

	/* Initial bitmask of capabilities to test */
	mask = BRIDGE_IFCAPS_MASK;

	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		/* Every member must support it or its disabled */
		mask &= bif->bif_savedcaps;
	}

	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		enabled = bif->bif_ifp->if_capenable;
		enabled &= ~BRIDGE_IFCAPS_STRIP;
		/* strip off mask bits and enable them again if allowed */
		enabled &= ~BRIDGE_IFCAPS_MASK;
		enabled |= mask;

		bridge_set_ifcap(sc, bif, enabled);
	}
}

static void
bridge_set_ifcap(struct bridge_softc *sc, struct bridge_iflist *bif, int set)
{
	struct ifnet *ifp = bif->bif_ifp;
	struct ifreq ifr;
	int error;

	bzero(&ifr, sizeof(ifr));
	ifr.ifr_reqcap = set;

	if (ifp->if_capenable != set) {
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, SIOCSIFCAP, (caddr_t)&ifr);
		IFF_UNLOCKGIANT(ifp);
		if (error) {
			BRIDGE_LOG(LOG_NOTICE, 0,
			    "%s error setting interface capabilities on %s",
			    sc->sc_ifp->if_xname, ifp->if_xname);
		}
	}
}
#endif /* HAS_IF_CAP */

static errno_t
siocsifcap(struct ifnet * ifp, uint32_t cap_enable)
{
	struct ifreq    ifr;

	bzero(&ifr, sizeof(ifr));
	ifr.ifr_reqcap = cap_enable;
	return ifnet_ioctl(ifp, 0, SIOCSIFCAP, &ifr);
}

static const char *
enable_disable_str(boolean_t enable)
{
	return (const char * __null_terminated)(enable ? "enable" : "disable");
}

static boolean_t
bridge_set_lro(struct ifnet * ifp, boolean_t enable)
{
	uint32_t        cap_enable;
	uint32_t        cap_supported;
	boolean_t       changed = FALSE;
	boolean_t       lro_enabled;

	cap_supported = ifnet_capabilities_supported(ifp);
	if ((cap_supported & IFCAP_LRO) == 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_LIFECYCLE,
		    "%s doesn't support LRO",
		    ifp->if_xname);
		goto done;
	}
	cap_enable = ifnet_capabilities_enabled(ifp);
	lro_enabled = (cap_enable & IFCAP_LRO) != 0;
	if (lro_enabled != enable) {
		errno_t         error;

		if (enable) {
			cap_enable |= IFCAP_LRO;
		} else {
			cap_enable &= ~IFCAP_LRO;
		}
		error = siocsifcap(ifp, cap_enable);
		if (error != 0) {
			BRIDGE_LOG(LOG_NOTICE, 0,
			    "%s %s failed (cap 0x%x) %d",
			    ifp->if_xname,
			    enable_disable_str(enable),
			    cap_enable,
			    error);
		} else {
			changed = TRUE;
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_LIFECYCLE,
			    "%s %s success (cap 0x%x)",
			    ifp->if_xname,
			    enable_disable_str(enable),
			    cap_enable);
		}
	}
done:
	return changed;
}

static errno_t
bridge_set_tso(struct bridge_softc *sc)
{
	struct bridge_iflist *bif;
	u_int32_t tso_v4_mtu;
	u_int32_t tso_v6_mtu;
	ifnet_offload_t offload;
	errno_t error = 0;

	/* By default, support TSO */
	offload = sc->sc_ifp->if_hwassist | IFNET_TSO_IPV4 | IFNET_TSO_IPV6;
	tso_v4_mtu = IP_MAXPACKET;
	tso_v6_mtu = IP_MAXPACKET;

	/* Use the lowest common denominator of the members */
	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		ifnet_t ifp = bif->bif_ifp;

		if (ifp == NULL) {
			continue;
		}

		if (offload & IFNET_TSO_IPV4) {
			if (ifp->if_hwassist & IFNET_TSO_IPV4) {
				if (tso_v4_mtu > ifp->if_tso_v4_mtu) {
					tso_v4_mtu = ifp->if_tso_v4_mtu;
				}
			} else {
				offload &= ~IFNET_TSO_IPV4;
				tso_v4_mtu = 0;
			}
		}
		if (offload & IFNET_TSO_IPV6) {
			if (ifp->if_hwassist & IFNET_TSO_IPV6) {
				if (tso_v6_mtu > ifp->if_tso_v6_mtu) {
					tso_v6_mtu = ifp->if_tso_v6_mtu;
				}
			} else {
				offload &= ~IFNET_TSO_IPV6;
				tso_v6_mtu = 0;
			}
		}
	}

	if (offload != sc->sc_ifp->if_hwassist) {
		error = ifnet_set_offload(sc->sc_ifp, offload);
		if (error != 0) {
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_LIFECYCLE,
			    "ifnet_set_offload(%s, 0x%x) failed %d",
			    sc->sc_ifp->if_xname, offload, error);
			goto done;
		}
		/*
		 * For ifnet_set_tso_mtu() sake, the TSO MTU must be at least
		 * as large as the interface MTU
		 */
		if (sc->sc_ifp->if_hwassist & IFNET_TSO_IPV4) {
			if (tso_v4_mtu < sc->sc_ifp->if_mtu) {
				tso_v4_mtu = sc->sc_ifp->if_mtu;
			}
			error = ifnet_set_tso_mtu(sc->sc_ifp, AF_INET,
			    tso_v4_mtu);
			if (error != 0) {
				BRIDGE_LOG(LOG_NOTICE, BR_DBGF_LIFECYCLE,
				    "ifnet_set_tso_mtu(%s, "
				    "AF_INET, %u) failed %d",
				    sc->sc_ifp->if_xname,
				    tso_v4_mtu, error);
				goto done;
			}
		}
		if (sc->sc_ifp->if_hwassist & IFNET_TSO_IPV6) {
			if (tso_v6_mtu < sc->sc_ifp->if_mtu) {
				tso_v6_mtu = sc->sc_ifp->if_mtu;
			}
			error = ifnet_set_tso_mtu(sc->sc_ifp, AF_INET6,
			    tso_v6_mtu);
			if (error != 0) {
				BRIDGE_LOG(LOG_NOTICE, BR_DBGF_LIFECYCLE,
				    "ifnet_set_tso_mtu(%s, "
				    "AF_INET6, %u) failed %d",
				    sc->sc_ifp->if_xname,
				    tso_v6_mtu, error);
				goto done;
			}
		}
	}
done:
	return error;
}

static const char *
sanitize_ifname(char * __sized_by(IFNAMSIZ) ifname)
{
	ifname[IFNAMSIZ - 1] = '\0';
	return __unsafe_null_terminated_from_indexable(ifname, &ifname[IFNAMSIZ - 1]);
}

/*
 * bridge_lookup_member:
 *
 *	Lookup a bridge member interface.
 */
static struct bridge_iflist *
bridge_lookup_member(struct bridge_softc *sc, char * __sized_by(IFNAMSIZ) name_unsanitized)
{
	struct bridge_iflist *bif;
	struct ifnet *ifp;
	const char * __null_terminated name = sanitize_ifname(name_unsanitized);

	BRIDGE_LOCK_ASSERT_HELD(sc);

	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		ifp = bif->bif_ifp;
		if (strcmp(ifp->if_xname, name) == 0) {
			return bif;
		}
	}

	return NULL;
}

/*
 * bridge_lookup_member_if:
 *
 *	Lookup a bridge member interface by ifnet*.
 */
static struct bridge_iflist *
bridge_lookup_member_if(struct bridge_softc *sc, struct ifnet *member_ifp)
{
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (bif->bif_ifp == member_ifp) {
			return bif;
		}
	}

	return NULL;
}

static errno_t
bridge_iff_input(void *cookie, ifnet_t ifp, protocol_family_t protocol,
    mbuf_t *data, char **frame_ptr)
{
#pragma unused(protocol)
	errno_t error = 0;
	struct bridge_iflist *bif = (struct bridge_iflist *)cookie;
	struct bridge_softc *sc = bif->bif_sc;
	int included = 0;
	size_t frmlen = 0;
	mbuf_t m = *data;

	if ((m->m_flags & M_PROTO1)) {
		goto out;
	}

	if (*frame_ptr >= (char *)mbuf_datastart(m) &&
	    *frame_ptr <= mtod(m, char *)) {
		included = 1;
		frmlen = mtod(m, char *) - *frame_ptr;
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
	    "%s from %s m 0x%llx data 0x%llx frame 0x%llx %s "
	    "frmlen %lu", sc->sc_ifp->if_xname,
	    ifp->if_xname, (uint64_t)VM_KERNEL_ADDRPERM(m),
	    (uint64_t)VM_KERNEL_ADDRPERM(mtod(m, void *)),
	    (uint64_t)VM_KERNEL_ADDRPERM(*frame_ptr),
	    included ? "inside" : "outside", frmlen);
	if (BRIDGE_DBGF_ENABLED(BR_DBGF_MBUF)) {
		brlog_mbuf(m, "bridge_iff_input[", "");
		brlog_ether_header((struct ether_header *)
		    (void *)*frame_ptr);
		brlog_mbuf_data(m, 0, 20);
	}
	if (included == 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT, "frame_ptr outside mbuf");
		goto out;
	}

	/* Move data pointer to start of frame to the link layer header */
	_mbuf_adjust_pkthdr_and_data(m, -frmlen);

	/* make sure we can access the ethernet header */
	if (mbuf_pkthdr_len(m) < sizeof(struct ether_header)) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "short frame %lu < %lu",
		    mbuf_pkthdr_len(m), sizeof(struct ether_header));
		goto out;
	}
	if (mbuf_len(m) < sizeof(struct ether_header)) {
		error = mbuf_pullup(data, sizeof(struct ether_header));
		if (error != 0) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
			    "mbuf_pullup(%lu) failed %d",
			    sizeof(struct ether_header),
			    error);
			error = EJUSTRETURN;
			goto out;
		}
		if (m != *data) {
			m = *data;
			*frame_ptr = mtod(m, char *);
		}
	}

	error = bridge_input(ifp, data);

	/* Adjust packet back to original */
	if (error == 0) {
		/* bridge_input might have modified *data */
		if (*data != m) {
			m = *data;
			*frame_ptr = mtod(m, char *);
		}
		_mbuf_adjust_pkthdr_and_data(m, frmlen);
	}

	if (BRIDGE_DBGF_ENABLED(BR_DBGF_MBUF) &&
	    BRIDGE_DBGF_ENABLED(BR_DBGF_INPUT)) {
		brlog_mbuf(m, "bridge_iff_input]", "");
	}

out:
	BRIDGE_LOCK_ASSERT_NOTHELD(sc);

	return error;
}

static errno_t
bridge_iff_output(void *cookie, ifnet_t ifp, protocol_family_t protocol,
    mbuf_t *data)
{
#pragma unused(protocol)
	errno_t error = 0;
	struct bridge_iflist *bif = (struct bridge_iflist *)cookie;
	struct bridge_softc *sc = bif->bif_sc;
	mbuf_t m = *data;

	if ((m->m_flags & M_PROTO1)) {
		goto out;
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_OUTPUT,
	    "%s from %s m 0x%llx data 0x%llx",
	    sc->sc_ifp->if_xname, ifp->if_xname,
	    (uint64_t)VM_KERNEL_ADDRPERM(m),
	    (uint64_t)VM_KERNEL_ADDRPERM(mtod(m, void *)));

	error = bridge_member_output(sc, ifp, data);
	if (error != 0 && error != EJUSTRETURN) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_OUTPUT,
		    "bridge_member_output failed error %d",
		    error);
	}
out:
	BRIDGE_LOCK_ASSERT_NOTHELD(sc);

	return error;
}

static void
bridge_iff_event(void *cookie, ifnet_t ifp, protocol_family_t protocol,
    const struct kev_msg *event_msg)
{
#pragma unused(protocol)
	struct bridge_iflist *bif = (struct bridge_iflist *)cookie;
	struct bridge_softc *sc = bif->bif_sc;

	if (event_msg->vendor_code == KEV_VENDOR_APPLE &&
	    event_msg->kev_class == KEV_NETWORK_CLASS &&
	    event_msg->kev_subclass == KEV_DL_SUBCLASS) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE,
		    "%s event_code %u - %s",
		    ifp->if_xname, event_msg->event_code,
		    dlil_kev_dl_code_str(event_msg->event_code));

		switch (event_msg->event_code) {
		case KEV_DL_LINK_OFF:
		case KEV_DL_LINK_ON: {
			bridge_iflinkevent(ifp);
#if BRIDGESTP
			bstp_linkstate(ifp, event_msg->event_code);
#endif /* BRIDGESTP */
			break;
		}
		case KEV_DL_SIFFLAGS: {
			if ((ifp->if_flags & IFF_UP) == 0) {
				break;
			}
			if ((bif->bif_flags & BIFF_PROMISC) == 0) {
				errno_t error;

				error = ifnet_set_promiscuous(ifp, 1);
				if (error != 0) {
					BRIDGE_LOG(LOG_NOTICE, 0,
					    "ifnet_set_promiscuous (%s)"
					    " failed %d", ifp->if_xname,
					    error);
				} else {
					bif->bif_flags |= BIFF_PROMISC;
				}
			}
			if ((bif->bif_flags & BIFF_WIFI_INFRA) != 0 &&
			    (bif->bif_flags & BIFF_ALL_MULTI) == 0) {
				errno_t error;

				error = if_allmulti(ifp, 1);
				if (error != 0) {
					BRIDGE_LOG(LOG_NOTICE, 0,
					    "if_allmulti (%s)"
					    " failed %d", ifp->if_xname,
					    error);
				} else {
					bif->bif_flags |= BIFF_ALL_MULTI;
#ifdef XNU_PLATFORM_AppleTVOS
					ip6_forwarding = 1;
#endif /* XNU_PLATFORM_AppleTVOS */
				}
			}
			break;
		}
		case KEV_DL_IFCAP_CHANGED: {
			BRIDGE_LOCK(sc);
			bridge_set_tso(sc);
			BRIDGE_UNLOCK(sc);
			break;
		}
		case KEV_DL_PROTO_DETACHED:
		case KEV_DL_PROTO_ATTACHED: {
			bridge_proto_attach_changed(ifp);
			break;
		}
		default:
			break;
		}
	}
}

/*
 * bridge_iff_detached:
 *
 *      Called when our interface filter has been detached from a
 *      member interface.
 */
static void
bridge_iff_detached(void *cookie, ifnet_t ifp)
{
#pragma unused(cookie)
	struct bridge_iflist *bif;
	struct bridge_softc * __single sc = ifp->if_bridge;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE, "%s", ifp->if_xname);

	/* Check if the interface is a bridge member */
	if (sc != NULL) {
		BRIDGE_LOCK(sc);
		bif = bridge_lookup_member_if(sc, ifp);
		if (bif != NULL) {
			bridge_delete_member(sc, bif);
		}
		BRIDGE_UNLOCK(sc);
		return;
	}
	/* Check if the interface is a span port */
	lck_mtx_lock(&bridge_list_mtx);
	LIST_FOREACH(sc, &bridge_list, sc_list) {
		BRIDGE_LOCK(sc);
		TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next)
		if (ifp == bif->bif_ifp) {
			bridge_delete_span(sc, bif);
			break;
		}
		BRIDGE_UNLOCK(sc);
	}
	lck_mtx_unlock(&bridge_list_mtx);
}

static errno_t
bridge_proto_input(ifnet_t ifp, protocol_family_t protocol, mbuf_t packet,
    char *header)
{
#pragma unused(protocol, packet, header)
	BRIDGE_LOG(LOG_NOTICE, 0, "%s unexpected packet",
	    ifp->if_xname);
	return 0;
}

static int
bridge_attach_protocol(struct ifnet *ifp)
{
	int     error;
	struct ifnet_attach_proto_param reg;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE, "%s", ifp->if_xname);
	bzero(&reg, sizeof(reg));
	reg.input = bridge_proto_input;

	error = ifnet_attach_protocol(ifp, PF_BRIDGE, &reg);
	if (error) {
		BRIDGE_LOG(LOG_NOTICE, 0,
		    "ifnet_attach_protocol(%s) failed, %d",
		    ifp->if_xname, error);
	}

	return error;
}

static int
bridge_detach_protocol(struct ifnet *ifp)
{
	int     error;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE, "%s", ifp->if_xname);
	error = ifnet_detach_protocol(ifp, PF_BRIDGE);
	if (error) {
		BRIDGE_LOG(LOG_NOTICE, 0,
		    "ifnet_detach_protocol(%s) failed, %d",
		    ifp->if_xname, error);
	}

	return error;
}

/*
 * bridge_delete_member:
 *
 *	Delete the specified member interface.
 */
static void
bridge_delete_member(struct bridge_softc *sc, struct bridge_iflist *bif)
{
#if SKYWALK
	boolean_t add_netagent = FALSE;
#endif /* SKYWALK */
	uint32_t    bif_flags;
	struct ifnet *ifs = bif->bif_ifp, *bifp = sc->sc_ifp;
	int lladdr_changed = 0, error;
	uint8_t eaddr[ETHER_ADDR_LEN];
	u_int32_t event_code = 0;

	BRIDGE_LOCK_ASSERT_HELD(sc);
	VERIFY(ifs != NULL);

	/*
	 * Remove the member from the list first so it cannot be found anymore
	 * when we release the bridge lock below
	 */
	if ((bif->bif_flags & BIFF_IN_MEMBER_LIST) != 0) {
		bif->bif_flags &= ~BIFF_IN_MEMBER_LIST;
		BRIDGE_XLOCK(sc);
		TAILQ_REMOVE(&sc->sc_iflist, bif, bif_next);
		BRIDGE_XDROP(sc);
	}
	if (sc->sc_mac_nat_bif != NULL) {
		if (bif == sc->sc_mac_nat_bif) {
			bridge_mac_nat_disable(sc);
		} else {
			bridge_mac_nat_flush_entries(sc, bif);
		}
	}
#if BRIDGESTP
	if ((bif->bif_ifflags & IFBIF_STP) != 0) {
		bstp_disable(&bif->bif_stp);
	}
#endif /* BRIDGESTP */

	/*
	 * If removing the interface that gave the bridge its mac address, set
	 * the mac address of the bridge to the address of the next member, or
	 * to its default address if no members are left.
	 */
	if (bridge_inherit_mac && sc->sc_ifaddr == ifs) {
		ifnet_release(sc->sc_ifaddr);
		if (TAILQ_EMPTY(&sc->sc_iflist)) {
			bcopy(sc->sc_defaddr, eaddr, ETHER_ADDR_LEN);
			sc->sc_ifaddr = NULL;
		} else {
			struct ifnet *fif =
			    TAILQ_FIRST(&sc->sc_iflist)->bif_ifp;
			bcopy(IF_LLADDR(fif), eaddr, ETHER_ADDR_LEN);
			sc->sc_ifaddr = fif;
			ifnet_reference(fif);   /* for sc_ifaddr */
		}
		lladdr_changed = 1;
	}

#if HAS_IF_CAP
	bridge_mutecaps(sc);    /* recalculate now this interface is removed */
#endif /* HAS_IF_CAP */

	error = bridge_set_tso(sc);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "bridge_set_tso failed %d", error);
	}

	bridge_rtdelete(sc, ifs, IFBF_FLUSHALL);

	KASSERT(bif->bif_addrcnt == 0,
	    ("%s: %d bridge routes referenced", __func__, bif->bif_addrcnt));

	/*
	 * Update link status of the bridge based on its remaining members
	 */
	event_code = bridge_updatelinkstatus(sc);
	bif_flags = bif->bif_flags;
	BRIDGE_UNLOCK(sc);

	/* only perform these steps if the interface is still attached */
	if (ifnet_is_attached(ifs, 1)) {
#if SKYWALK
		add_netagent = (bif_flags & BIFF_NETAGENT_REMOVED) != 0;

		if ((bif_flags & BIFF_FLOWSWITCH_ATTACHED) != 0) {
			ifnet_detach_flowswitch_nexus(ifs);
		}
#endif /* SKYWALK */
		/* disable promiscuous mode */
		if ((bif_flags & BIFF_PROMISC) != 0) {
			(void) ifnet_set_promiscuous(ifs, 0);
		}
		/* disable all multi */
		if ((bif_flags & BIFF_ALL_MULTI) != 0) {
			(void)if_allmulti(ifs, 0);
		}
#if HAS_IF_CAP
		/* re-enable any interface capabilities */
		bridge_set_ifcap(sc, bif, bif->bif_savedcaps);
#endif
		/* detach bridge "protocol" */
		if ((bif_flags & BIFF_PROTO_ATTACHED) != 0) {
			(void)bridge_detach_protocol(ifs);
		}
		/* detach interface filter */
		if ((bif_flags & BIFF_FILTER_ATTACHED) != 0) {
			iflt_detach(bif->bif_iff_ref);
		}
		/* re-enable LRO */
		if ((bif_flags & BIFF_LRO_DISABLED) != 0) {
			(void)bridge_set_lro(ifs, TRUE);
		}
		ifnet_decr_iorefcnt(ifs);
	}

	if (lladdr_changed &&
	    (error = ifnet_set_lladdr(bifp, eaddr, ETHER_ADDR_LEN)) != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_lladdr failed %d", error);
	}

	if (event_code != 0) {
		bridge_link_event(bifp, event_code);
	}

#if BRIDGESTP
	bstp_destroy(&bif->bif_stp);    /* prepare to free */
#endif /* BRIDGESTP */

	kfree_type(struct bridge_iflist, bif);
	ifs->if_bridge = NULL;
#if SKYWALK
	if (add_netagent && ifnet_is_attached(ifs, 1)) {
		(void)ifnet_add_netagent(ifs);
		ifnet_decr_iorefcnt(ifs);
	}
#endif /* SKYWALK */

	ifnet_release(ifs);

	BRIDGE_LOCK(sc);
}

/*
 * bridge_delete_span:
 *
 *	Delete the specified span interface.
 */
static void
bridge_delete_span(struct bridge_softc *sc, struct bridge_iflist *bif)
{
	BRIDGE_LOCK_ASSERT_HELD(sc);

	KASSERT(bif->bif_ifp->if_bridge == NULL,
	    ("%s: not a span interface", __func__));

	ifnet_release(bif->bif_ifp);

	TAILQ_REMOVE(&sc->sc_spanlist, bif, bif_next);
	kfree_type(struct bridge_iflist, bif);
}

static int
bridge_ioctl_add(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif = NULL;
	struct ifnet *ifs, *bifp = sc->sc_ifp;
	int error = 0, lladdr_changed = 0;
	uint8_t eaddr[ETHER_ADDR_LEN];
	struct iff_filter iff;
	u_int32_t event_code = 0;
	boolean_t input_broadcast;
	int media_active;
	boolean_t wifi_infra = FALSE;

	ifs = ifunit(sanitize_ifname(req->ifbr_ifsname));
	if (ifs == NULL) {
		return ENOENT;
	}
	if (ifs->if_ioctl == NULL) {    /* must be supported */
		return EINVAL;
	}

	if (IFNET_IS_INTCOPROC(ifs) || IFNET_IS_MANAGEMENT(ifs)) {
		return EINVAL;
	}

	/* If it's in the span list, it can't be a member. */
	TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next) {
		if (ifs == bif->bif_ifp) {
			return EBUSY;
		}
	}

	if (ifs->if_bridge == sc) {
		return EEXIST;
	}

	if (ifs->if_bridge != NULL) {
		return EBUSY;
	}

	switch (ifs->if_type) {
	case IFT_ETHER:
		if (strcmp(ifs->if_name, "en") == 0 &&
		    ifs->if_subfamily == IFNET_SUBFAMILY_WIFI &&
		    (ifs->if_eflags & IFEF_IPV4_ROUTER) == 0) {
			/* XXX is there a better way to identify Wi-Fi STA? */
			wifi_infra = TRUE;
		}
		break;
	case IFT_L2VLAN:
	case IFT_IEEE8023ADLAG:
		break;
	default:
		return EINVAL;
	}

	/* fail to add the interface if the MTU doesn't match */
	if (!TAILQ_EMPTY(&sc->sc_iflist) && sc->sc_ifp->if_mtu != ifs->if_mtu) {
		BRIDGE_LOG(LOG_NOTICE, 0, "%s invalid MTU for %s",
		    sc->sc_ifp->if_xname,
		    ifs->if_xname);
		return EINVAL;
	}

	if (wifi_infra && sc->sc_mac_nat_bif != NULL) {
		/* there's already an interface that's doing MAC NAT */
		return EBUSY;
	}

	/* prevent the interface from detaching while we add the member */
	if (!ifnet_is_attached(ifs, 1)) {
		return ENXIO;
	}

	/* allocate a new member */
	bif = kalloc_type(struct bridge_iflist, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	bif->bif_ifp = ifs;
	ifnet_reference(ifs);
	bif->bif_ifflags |= IFBIF_LEARNING | IFBIF_DISCOVER;
#if HAS_IF_CAP
	bif->bif_savedcaps = ifs->if_capenable;
#endif /* HAS_IF_CAP */
	bif->bif_sc = sc;
	if (wifi_infra) {
		(void)bridge_mac_nat_enable(sc, bif);
	}

	/* Allow the first Ethernet member to define the MTU */
	if (TAILQ_EMPTY(&sc->sc_iflist)) {
		sc->sc_ifp->if_mtu = ifs->if_mtu;
	}

	/*
	 * Assign the interface's MAC address to the bridge if it's the first
	 * member and the MAC address of the bridge has not been changed from
	 * the default (randomly) generated one.
	 */
	if (bridge_inherit_mac && TAILQ_EMPTY(&sc->sc_iflist) &&
	    _ether_cmp(IF_LLADDR(sc->sc_ifp), sc->sc_defaddr) == 0) {
		bcopy(IF_LLADDR(ifs), eaddr, ETHER_ADDR_LEN);
		sc->sc_ifaddr = ifs;
		ifnet_reference(ifs);   /* for sc_ifaddr */
		lladdr_changed = 1;
	}

	ifs->if_bridge = sc;
#if BRIDGESTP
	bstp_create(&sc->sc_stp, &bif->bif_stp, bif->bif_ifp);
#endif /* BRIDGESTP */

#if HAS_IF_CAP
	/* Set interface capabilities to the intersection set of all members */
	bridge_mutecaps(sc);
#endif /* HAS_IF_CAP */

	/*
	 * Respect lock ordering with DLIL lock for the following operations
	 */
	BRIDGE_UNLOCK(sc);

	/* enable promiscuous mode */
	error = ifnet_set_promiscuous(ifs, 1);
	switch (error) {
	case 0:
		bif->bif_flags |= BIFF_PROMISC;
		break;
	case ENETDOWN:
	case EPWROFF:
		BRIDGE_LOG(LOG_NOTICE, 0,
		    "ifnet_set_promiscuous(%s) failed %d, ignoring",
		    ifs->if_xname, error);
		/* Ignore error when device is not up */
		error = 0;
		break;
	default:
		BRIDGE_LOG(LOG_NOTICE, 0,
		    "ifnet_set_promiscuous(%s) failed %d",
		    ifs->if_xname, error);
		BRIDGE_LOCK(sc);
		goto out;
	}
	if (wifi_infra) {
		int this_error;

		/* Wi-Fi doesn't really support promiscuous, set allmulti */
		bif->bif_flags |= BIFF_WIFI_INFRA;
		this_error = if_allmulti(ifs, 1);
		if (this_error == 0) {
			bif->bif_flags |= BIFF_ALL_MULTI;
#ifdef XNU_PLATFORM_AppleTVOS
			ip6_forwarding = 1;
#endif /* XNU_PLATFORM_AppleTVOS */
		} else {
			BRIDGE_LOG(LOG_NOTICE, 0,
			    "if_allmulti(%s) failed %d, ignoring",
			    ifs->if_xname, this_error);
		}
	}
#if SKYWALK
	/* ensure that the flowswitch is present for native interface */
	if (SKYWALK_NATIVE(ifs)) {
		if (ifnet_attach_flowswitch_nexus(ifs)) {
			bif->bif_flags |= BIFF_FLOWSWITCH_ATTACHED;
		}
	}
	/* remove the netagent on the flowswitch (rdar://75050182) */
	if (if_is_fsw_netagent_enabled()) {
		(void)ifnet_remove_netagent(ifs);
		bif->bif_flags |= BIFF_NETAGENT_REMOVED;
	}
#endif /* SKYWALK */

	/*
	 * install an interface filter
	 */
	memset(&iff, 0, sizeof(struct iff_filter));
	iff.iff_cookie = bif;
	iff.iff_name = "com.apple.kernel.bsd.net.if_bridge";
	iff.iff_input = bridge_iff_input;
	iff.iff_output = bridge_iff_output;
	iff.iff_event = bridge_iff_event;
	iff.iff_detached = bridge_iff_detached;
	error = dlil_attach_filter(ifs, &iff, &bif->bif_iff_ref,
	    DLIL_IFF_TSO | DLIL_IFF_INTERNAL | DLIL_IFF_BRIDGE);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "iflt_attach failed %d", error);
		BRIDGE_LOCK(sc);
		goto out;
	}
	bif->bif_flags |= BIFF_FILTER_ATTACHED;

	/*
	 * install a dummy "bridge" protocol
	 */
	if ((error = bridge_attach_protocol(ifs)) != 0) {
		if (error != 0) {
			BRIDGE_LOG(LOG_NOTICE, 0,
			    "bridge_attach_protocol failed %d", error);
			BRIDGE_LOCK(sc);
			goto out;
		}
	}
	bif->bif_flags |= BIFF_PROTO_ATTACHED;

	if (lladdr_changed &&
	    (error = ifnet_set_lladdr(bifp, eaddr, ETHER_ADDR_LEN)) != 0) {
		BRIDGE_LOG(LOG_NOTICE, 0, "ifnet_set_lladdr failed %d", error);
	}

	media_active = interface_media_active(ifs);

	/* disable LRO */
	if (bridge_set_lro(ifs, FALSE)) {
		bif->bif_flags |= BIFF_LRO_DISABLED;
	}

	/*
	 * No failures past this point. Add the member to the list.
	 */
	BRIDGE_LOCK(sc);
	bif->bif_flags |= BIFF_IN_MEMBER_LIST;
	BRIDGE_XLOCK(sc);
	TAILQ_INSERT_TAIL(&sc->sc_iflist, bif, bif_next);
	BRIDGE_XDROP(sc);

	/* cache the member link status */
	if (media_active != 0) {
		bif->bif_flags |= BIFF_MEDIA_ACTIVE;
	} else {
		bif->bif_flags &= ~BIFF_MEDIA_ACTIVE;
	}

	/* the new member may change the link status of the bridge interface */
	event_code = bridge_updatelinkstatus(sc);

	/* check whether we need input broadcast or not */
	input_broadcast = interface_needs_input_broadcast(ifs);
	bif_set_input_broadcast(bif, input_broadcast);
	BRIDGE_UNLOCK(sc);

	if (event_code != 0) {
		bridge_link_event(bifp, event_code);
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE,
	    "%s input broadcast %s", ifs->if_xname,
	    input_broadcast ? "ENABLED" : "DISABLED");

	BRIDGE_LOCK(sc);
	bridge_set_tso(sc);

out:
	/* allow the interface to detach */
	ifnet_decr_iorefcnt(ifs);

	if (error != 0) {
		if (bif != NULL) {
			bridge_delete_member(sc, bif);
		}
	} else if (IFNET_IS_VMNET(ifs)) {
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_vmnet_total);
	}

	return error;
}

static int
bridge_ioctl_del(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	bridge_delete_member(sc, bif);

	return 0;
}

static int
bridge_ioctl_purge(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#pragma unused(sc, arg, arg_len)
	return 0;
}

static int
bridge_ioctl_gifflags(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	struct bstp_port *bp;

	bp = &bif->bif_stp;
	req->ifbr_state = bp->bp_state;
	req->ifbr_priority = bp->bp_priority;
	req->ifbr_path_cost = bp->bp_path_cost;
	req->ifbr_proto = bp->bp_protover;
	req->ifbr_role = bp->bp_role;
	req->ifbr_stpflags = bp->bp_flags;
	req->ifbr_ifsflags = bif->bif_ifflags;

	/* Copy STP state options as flags */
	if (bp->bp_operedge) {
		req->ifbr_ifsflags |= IFBIF_BSTP_EDGE;
	}
	if (bp->bp_flags & BSTP_PORT_AUTOEDGE) {
		req->ifbr_ifsflags |= IFBIF_BSTP_AUTOEDGE;
	}
	if (bp->bp_ptp_link) {
		req->ifbr_ifsflags |= IFBIF_BSTP_PTP;
	}
	if (bp->bp_flags & BSTP_PORT_AUTOPTP) {
		req->ifbr_ifsflags |= IFBIF_BSTP_AUTOPTP;
	}
	if (bp->bp_flags & BSTP_PORT_ADMEDGE) {
		req->ifbr_ifsflags |= IFBIF_BSTP_ADMEDGE;
	}
	if (bp->bp_flags & BSTP_PORT_ADMCOST) {
		req->ifbr_ifsflags |= IFBIF_BSTP_ADMCOST;
	}

	req->ifbr_portno = bif->bif_ifp->if_index & 0xfff;
	req->ifbr_addrcnt = bif->bif_addrcnt;
	req->ifbr_addrmax = bif->bif_addrmax;
	req->ifbr_addrexceeded = bif->bif_addrexceeded;

	return 0;
}

static int
bridge_ioctl_sifflags(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif;
#if BRIDGESTP
	struct bstp_port *bp;
#endif /* BRIDGESTP */
	errno_t error;
	uint32_t ifsflags;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	ifsflags = req->ifbr_ifsflags;
	if (ifsflags & IFBIF_SPAN) {
		/* SPAN is readonly */
		return EINVAL;
	}

#define CHECKSUM_OR_MAC_NAT     (IFBIF_CHECKSUM_OFFLOAD | IFBIF_MAC_NAT)
	if ((ifsflags & CHECKSUM_OR_MAC_NAT) == CHECKSUM_OR_MAC_NAT) {
		/* can't specify both MAC-NAT and checksum offload */
		return EINVAL;
	}
	if ((ifsflags & IFBIF_MAC_NAT) != 0) {
		if ((bif->bif_flags & BIFF_HOST_FILTER) != 0) {
			/* no host filter with MAC-NAT */
			return EINVAL;
		}
		error = bridge_mac_nat_enable(sc, bif);
		if (error != 0) {
			return error;
		}
	} else if (sc->sc_mac_nat_bif == bif) {
		bridge_mac_nat_disable(sc);
	}

#if BRIDGESTP
	if (ifsflags & IFBIF_STP) {
		if ((bif->bif_ifflags & IFBIF_STP) == 0) {
			error = bstp_enable(&bif->bif_stp);
			if (error) {
				return error;
			}
		}
	} else {
		if ((bif->bif_ifflags & IFBIF_STP) != 0) {
			bstp_disable(&bif->bif_stp);
		}
	}

	/* Pass on STP flags */
	bp = &bif->bif_stp;
	bstp_set_edge(bp, ifsflags & IFBIF_BSTP_EDGE ? 1 : 0);
	bstp_set_autoedge(bp, ifsflags & IFBIF_BSTP_AUTOEDGE ? 1 : 0);
	bstp_set_ptp(bp, ifsflags & IFBIF_BSTP_PTP ? 1 : 0);
	bstp_set_autoptp(bp, ifsflags & IFBIF_BSTP_AUTOPTP ? 1 : 0);
#else /* !BRIDGESTP */
	if (ifsflags & IFBIF_STP) {
		return EOPNOTSUPP;
	}
#endif /* !BRIDGESTP */

	/* Save the bits relating to the bridge */
	bif->bif_ifflags = ifsflags & IFBIFMASK;

	return 0;
}

static int
bridge_ioctl_scache(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	sc->sc_brtmax = param->ifbrp_csize;
	bridge_rttrim(sc);
	return 0;
}

static int
bridge_ioctl_gcache(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	param->ifbrp_csize = sc->sc_brtmax;

	return 0;
}

#define BRIDGE_IOCTL_GIFS do { \
	struct bridge_iflist *bif;                                      \
	struct ifbreq breq;                                             \
	char *buf, *outbuf;                                             \
	unsigned int count, buflen, len;                                \
                                                                        \
	count = 0;                                                      \
	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next)                    \
	        count++;                                                \
	TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next)                  \
	        count++;                                                \
                                                                        \
	buflen = sizeof (breq) * count;                                 \
	if (bifc->ifbic_len == 0) {                                     \
	        bifc->ifbic_len = buflen;                               \
	        return (0);                                             \
	}                                                               \
	BRIDGE_UNLOCK(sc);                                              \
	outbuf = kalloc_data(buflen, Z_WAITOK | Z_ZERO);                \
	BRIDGE_LOCK(sc);                                                \
                                                                        \
	count = 0;                                                      \
	buf = outbuf;                                                   \
	len = min(bifc->ifbic_len, buflen);                             \
	bzero(&breq, sizeof (breq));                                    \
	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {                  \
	        if (len < sizeof (breq))                                \
	                break;                                          \
                                                                        \
	        snprintf(breq.ifbr_ifsname, sizeof (breq.ifbr_ifsname), \
	            "%s", bif->bif_ifp->if_xname);                      \
	/* Fill in the ifbreq structure */                      \
	        error = bridge_ioctl_gifflags(sc, &breq, sizeof(breq)); \
	        if (error)                                              \
	                break;                                          \
	        memcpy(buf, &breq, sizeof (breq));                      \
	        count++;                                                \
	        buf += sizeof (breq);                                   \
	        len -= sizeof (breq);                                   \
	}                                                               \
	TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next) {                \
	        if (len < sizeof (breq))                                \
	                break;                                          \
                                                                        \
	        snprintf(breq.ifbr_ifsname,                             \
	                 sizeof (breq.ifbr_ifsname),                    \
	                 "%s", bif->bif_ifp->if_xname);                 \
	        breq.ifbr_ifsflags = bif->bif_ifflags;                  \
	        breq.ifbr_portno                                        \
	                = bif->bif_ifp->if_index & 0xfff;               \
	        memcpy(buf, &breq, sizeof (breq));                      \
	        count++;                                                \
	        buf += sizeof (breq);                                   \
	        len -= sizeof (breq);                                   \
	}                                                               \
                                                                        \
	BRIDGE_UNLOCK(sc);                                              \
	bifc->ifbic_len = sizeof (breq) * count;                        \
	if (bifc->ifbic_len > 0) {                                      \
	        error = copyout(outbuf, bifc->ifbic_req, bifc->ifbic_len);\
	}                                                               \
	BRIDGE_LOCK(sc);                                                \
	kfree_data(outbuf, buflen);                                     \
} while (0)

static int
bridge_ioctl_gifs64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbifconf64 * __single bifc = arg;
	int error = 0;

	BRIDGE_IOCTL_GIFS;

	return error;
}

static int
bridge_ioctl_gifs32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbifconf32 * __single bifc = arg;
	int error = 0;

	BRIDGE_IOCTL_GIFS;

	return error;
}

#define BRIDGE_IOCTL_RTS do {                                               \
	struct bridge_rtnode *brt;                                          \
	char *buf;                                                          \
	char *outbuf = NULL;                                                \
	unsigned int count, buflen, len;                                    \
	unsigned long now;                                                  \
                                                                            \
	if (bac->ifbac_len == 0)                                            \
	        return (0);                                                 \
                                                                            \
	bzero(&bareq, sizeof (bareq));                                      \
	count = 0;                                                          \
	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list)                         \
	        count++;                                                    \
	buflen = sizeof (bareq) * count;                                    \
                                                                            \
	BRIDGE_UNLOCK(sc);                                                  \
	outbuf = kalloc_data(buflen, Z_WAITOK | Z_ZERO);                    \
	BRIDGE_LOCK(sc);                                                    \
                                                                            \
	count = 0;                                                          \
	buf = outbuf;                                                       \
	len = min(bac->ifbac_len, buflen);                                  \
	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {                       \
	        if (len < sizeof (bareq))                                   \
	                goto out;                                           \
	        snprintf(bareq.ifba_ifsname, sizeof (bareq.ifba_ifsname),   \
	                 "%s", brt->brt_ifp->if_xname);                     \
	        memcpy(bareq.ifba_dst, brt->brt_addr, sizeof (brt->brt_addr)); \
	        bareq.ifba_vlan = brt->brt_vlan;                            \
	        if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {   \
	                now = (unsigned long) net_uptime();                 \
	                if (now < brt->brt_expire)                          \
	                        bareq.ifba_expire =                         \
	                            brt->brt_expire - now;                  \
	        } else                                                      \
	                bareq.ifba_expire = 0;                              \
	        bareq.ifba_flags = brt->brt_flags;                          \
                                                                            \
	        memcpy(buf, &bareq, sizeof (bareq));                        \
	        count++;                                                    \
	        buf += sizeof (bareq);                                      \
	        len -= sizeof (bareq);                                      \
	}                                                                   \
out:                                                                        \
	bac->ifbac_len = sizeof (bareq) * count;                            \
	if (outbuf != NULL) {                                               \
	        BRIDGE_UNLOCK(sc);                                          \
	        if (bac->ifbac_len > 0) {                                   \
	                error = copyout(outbuf, bac->ifbac_req, bac->ifbac_len);\
	        }                                                           \
	        kfree_data(outbuf, buflen);                                 \
	        BRIDGE_LOCK(sc);                                            \
	}                                                                   \
	return (error);                                                     \
} while (0)

static int
bridge_ioctl_rts64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbaconf64 * __single bac = arg;
	struct ifbareq64 bareq;
	int error = 0;

	BRIDGE_IOCTL_RTS;
	return error;
}

static int
bridge_ioctl_rts32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbaconf32 * __single bac = arg;
	struct ifbareq32 bareq;
	int error = 0;

	BRIDGE_IOCTL_RTS;
	return error;
}

static int
bridge_ioctl_saddr32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbareq32 * __single req = arg;
	struct bridge_iflist *bif;
	int error;

	bif = bridge_lookup_member(sc, req->ifba_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	error = bridge_rtupdate(sc, req->ifba_dst, req->ifba_vlan, bif, 1,
	    req->ifba_flags);

	return error;
}

static int
bridge_ioctl_saddr64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbareq64 * __single req = arg;
	struct bridge_iflist *bif;
	int error;

	bif = bridge_lookup_member(sc, req->ifba_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	error = bridge_rtupdate(sc, req->ifba_dst, req->ifba_vlan, bif, 1,
	    req->ifba_flags);

	return error;
}

static int
bridge_ioctl_sto(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	sc->sc_brttimeout = param->ifbrp_ctime;
	return 0;
}

static int
bridge_ioctl_gto(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	param->ifbrp_ctime = sc->sc_brttimeout;
	return 0;
}

static int
bridge_ioctl_daddr32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbareq32 * __single req = arg;

	return bridge_rtdaddr(sc, req->ifba_dst, req->ifba_vlan);
}

static int
bridge_ioctl_daddr64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbareq64 * __single req = arg;

	return bridge_rtdaddr(sc, req->ifba_dst, req->ifba_vlan);
}

static int
bridge_ioctl_flush(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;

	bridge_rtflush(sc, req->ifbr_ifsflags);
	return 0;
}

static int
bridge_ioctl_gpri(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;
	struct bstp_state *bs = &sc->sc_stp;

	param->ifbrp_prio = bs->bs_bridge_priority;
	return 0;
}

static int
bridge_ioctl_spri(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbrparam *param = arg;

	return bstp_set_priority(&sc->sc_stp, param->ifbrp_prio);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_ght(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;
	struct bstp_state *bs = &sc->sc_stp;

	param->ifbrp_hellotime = bs->bs_bridge_htime >> 8;
	return 0;
}

static int
bridge_ioctl_sht(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbrparam *param = arg;

	return bstp_set_htime(&sc->sc_stp, param->ifbrp_hellotime);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_gfd(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param;
	struct bstp_state *bs;

	param = arg;
	bs = &sc->sc_stp;
	param->ifbrp_fwddelay = bs->bs_bridge_fdelay >> 8;
	return 0;
}

static int
bridge_ioctl_sfd(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbrparam *param = arg;

	return bstp_set_fdelay(&sc->sc_stp, param->ifbrp_fwddelay);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_gma(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param;
	struct bstp_state *bs;

	param = arg;
	bs = &sc->sc_stp;
	param->ifbrp_maxage = bs->bs_bridge_max_age >> 8;
	return 0;
}

static int
bridge_ioctl_sma(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbrparam *param = arg;

	return bstp_set_maxage(&sc->sc_stp, param->ifbrp_maxage);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_sifprio(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	return bstp_set_port_priority(&bif->bif_stp, req->ifbr_priority);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_sifcost(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	return bstp_set_path_cost(&bif->bif_stp, req->ifbr_path_cost);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_gfilt(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	param->ifbrp_filter = sc->sc_filter_flags;

	return 0;
}

static int
bridge_ioctl_sfilt(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	if (param->ifbrp_filter & ~IFBF_FILT_MASK) {
		return EINVAL;
	}

	if (param->ifbrp_filter & IFBF_FILT_USEIPF) {
		return EINVAL;
	}

	sc->sc_filter_flags = param->ifbrp_filter;

	return 0;
}

static int
bridge_ioctl_sifmaxaddr(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	bif->bif_addrmax = req->ifbr_addrmax;
	return 0;
}

static int
bridge_ioctl_addspan(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif = NULL;
	struct ifnet *ifs;

	ifs = ifunit(sanitize_ifname(req->ifbr_ifsname));
	if (ifs == NULL) {
		return ENOENT;
	}

	if (IFNET_IS_INTCOPROC(ifs) || IFNET_IS_MANAGEMENT(ifs)) {
		return EINVAL;
	}

	TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next)
	if (ifs == bif->bif_ifp) {
		return EBUSY;
	}

	if (ifs->if_bridge != NULL) {
		return EBUSY;
	}

	switch (ifs->if_type) {
	case IFT_ETHER:
	case IFT_L2VLAN:
	case IFT_IEEE8023ADLAG:
		break;
	default:
		return EINVAL;
	}

	bif = kalloc_type(struct bridge_iflist, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	bif->bif_ifp = ifs;
	bif->bif_ifflags = IFBIF_SPAN;

	ifnet_reference(bif->bif_ifp);

	TAILQ_INSERT_HEAD(&sc->sc_spanlist, bif, bif_next);

	return 0;
}

static int
bridge_ioctl_delspan(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbreq * __single req = arg;
	struct bridge_iflist *bif;
	struct ifnet *ifs;

	ifs = ifunit(sanitize_ifname(req->ifbr_ifsname));
	if (ifs == NULL) {
		return ENOENT;
	}

	TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next)
	if (ifs == bif->bif_ifp) {
		break;
	}

	if (bif == NULL) {
		return ENOENT;
	}

	bridge_delete_span(sc, bif);

	return 0;
}

#define BRIDGE_IOCTL_GBPARAM do {                                       \
	struct bstp_state *bs = &sc->sc_stp;                            \
	struct bstp_port *root_port;                                    \
                                                                        \
	req->ifbop_maxage = bs->bs_bridge_max_age >> 8;                 \
	req->ifbop_hellotime = bs->bs_bridge_htime >> 8;                \
	req->ifbop_fwddelay = bs->bs_bridge_fdelay >> 8;                \
                                                                        \
	root_port = bs->bs_root_port;                                   \
	if (root_port == NULL)                                          \
	        req->ifbop_root_port = 0;                               \
	else                                                            \
	        req->ifbop_root_port = root_port->bp_ifp->if_index;     \
                                                                        \
	req->ifbop_holdcount = bs->bs_txholdcount;                      \
	req->ifbop_priority = bs->bs_bridge_priority;                   \
	req->ifbop_protocol = bs->bs_protover;                          \
	req->ifbop_root_path_cost = bs->bs_root_pv.pv_cost;             \
	req->ifbop_bridgeid = bs->bs_bridge_pv.pv_dbridge_id;           \
	req->ifbop_designated_root = bs->bs_root_pv.pv_root_id;         \
	req->ifbop_designated_bridge = bs->bs_root_pv.pv_dbridge_id;    \
	req->ifbop_last_tc_time.tv_sec = bs->bs_last_tc_time.tv_sec;    \
	req->ifbop_last_tc_time.tv_usec = bs->bs_last_tc_time.tv_usec;  \
} while (0)

static int
bridge_ioctl_gbparam32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbropreq32 * __single req = arg;

	BRIDGE_IOCTL_GBPARAM;
	return 0;
}

static int
bridge_ioctl_gbparam64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbropreq64 * __single req = arg;

	BRIDGE_IOCTL_GBPARAM;
	return 0;
}

static int
bridge_ioctl_grte(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrparam * __single param = arg;

	param->ifbrp_cexceeded = sc->sc_brtexceeded;
	return 0;
}

#define BRIDGE_IOCTL_GIFSSTP do {                                       \
	struct bridge_iflist *bif;                                      \
	struct bstp_port *bp;                                           \
	struct ifbpstpreq bpreq;                                        \
	char *buf, *outbuf;                                             \
	unsigned int count, buflen, len;                                \
                                                                        \
	count = 0;                                                      \
	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {                  \
	        if ((bif->bif_ifflags & IFBIF_STP) != 0)                \
	                count++;                                        \
	}                                                               \
                                                                        \
	buflen = sizeof (bpreq) * count;                                \
	if (bifstp->ifbpstp_len == 0) {                                 \
	        bifstp->ifbpstp_len = buflen;                           \
	        return (0);                                             \
	}                                                               \
                                                                        \
	BRIDGE_UNLOCK(sc);                                              \
	outbuf = kalloc_data(buflen, Z_WAITOK | Z_ZERO);                \
	BRIDGE_LOCK(sc);                                                \
                                                                        \
	count = 0;                                                      \
	buf = outbuf;                                                   \
	len = min(bifstp->ifbpstp_len, buflen);                         \
	bzero(&bpreq, sizeof (bpreq));                                  \
	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {                  \
	        if (len < sizeof (bpreq))                               \
	                break;                                          \
                                                                        \
	        if ((bif->bif_ifflags & IFBIF_STP) == 0)                \
	                continue;                                       \
                                                                        \
	        bp = &bif->bif_stp;                                     \
	        bpreq.ifbp_portno = bif->bif_ifp->if_index & 0xfff;     \
	        bpreq.ifbp_fwd_trans = bp->bp_forward_transitions;      \
	        bpreq.ifbp_design_cost = bp->bp_desg_pv.pv_cost;        \
	        bpreq.ifbp_design_port = bp->bp_desg_pv.pv_port_id;     \
	        bpreq.ifbp_design_bridge = bp->bp_desg_pv.pv_dbridge_id; \
	        bpreq.ifbp_design_root = bp->bp_desg_pv.pv_root_id;     \
                                                                        \
	        memcpy(buf, &bpreq, sizeof (bpreq));                    \
	        count++;                                                \
	        buf += sizeof (bpreq);                                  \
	        len -= sizeof (bpreq);                                  \
	}                                                               \
                                                                        \
	BRIDGE_UNLOCK(sc);                                              \
	bifstp->ifbpstp_len = sizeof (bpreq) * count;                   \
	if (bifstp->ifbpstp_len > 0) {                                  \
	        error = copyout(outbuf, bifstp->ifbpstp_req, bifstp->ifbpstp_len);\
	}                                                               \
	BRIDGE_LOCK(sc);                                                \
	kfree_data(outbuf, buflen);                                     \
	return (error);                                                 \
} while (0)

static int
bridge_ioctl_gifsstp32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbpstpconf32 * __single bifstp = arg;
	int error = 0;

	BRIDGE_IOCTL_GIFSSTP;
	return error;
}

static int
bridge_ioctl_gifsstp64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbpstpconf64 * __single bifstp = arg;
	int error = 0;

	BRIDGE_IOCTL_GIFSSTP;
	return error;
}

static int
bridge_ioctl_sproto(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbrparam *param = arg;

	return bstp_set_protocol(&sc->sc_stp, param->ifbrp_proto);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}

static int
bridge_ioctl_stxhc(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
#if BRIDGESTP
	struct ifbrparam *param = arg;

	return bstp_set_holdcount(&sc->sc_stp, param->ifbrp_txhc);
#else /* !BRIDGESTP */
#pragma unused(sc, arg)
	return EOPNOTSUPP;
#endif /* !BRIDGESTP */
}


static int
bridge_ioctl_ghostfilter(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrhostfilter * __single req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbrhf_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}

	bzero(req, sizeof(struct ifbrhostfilter));
	if (bif->bif_flags & BIFF_HOST_FILTER) {
		req->ifbrhf_flags |= IFBRHF_ENABLED;
		bcopy(bif->bif_hf_hwsrc, req->ifbrhf_hwsrca,
		    ETHER_ADDR_LEN);
		req->ifbrhf_ipsrc = bif->bif_hf_ipsrc.s_addr;
	}
	return 0;
}

static int
bridge_ioctl_shostfilter(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrhostfilter * __single req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbrhf_ifsname);
	if (bif == NULL) {
		return ENOENT;
	}
	if (bif_has_mac_nat(bif)) {
		/* no host filter with MAC-NAT */
		return EINVAL;
	}
	if (req->ifbrhf_flags & IFBRHF_ENABLED) {
		bif->bif_flags |= BIFF_HOST_FILTER;

		if (req->ifbrhf_flags & IFBRHF_HWSRC) {
			bcopy(req->ifbrhf_hwsrca, bif->bif_hf_hwsrc,
			    ETHER_ADDR_LEN);
			if (bcmp(req->ifbrhf_hwsrca, ethernulladdr,
			    ETHER_ADDR_LEN) != 0) {
				bif->bif_flags |= BIFF_HF_HWSRC;
			} else {
				bif->bif_flags &= ~BIFF_HF_HWSRC;
			}
		}
		if (req->ifbrhf_flags & IFBRHF_IPSRC) {
			bif->bif_hf_ipsrc.s_addr = req->ifbrhf_ipsrc;
			if (bif->bif_hf_ipsrc.s_addr != INADDR_ANY) {
				bif->bif_flags |= BIFF_HF_IPSRC;
			} else {
				bif->bif_flags &= ~BIFF_HF_IPSRC;
			}
		}
	} else {
		bif->bif_flags &= ~(BIFF_HOST_FILTER | BIFF_HF_HWSRC |
		    BIFF_HF_IPSRC);
		bzero(bif->bif_hf_hwsrc, ETHER_ADDR_LEN);
		bif->bif_hf_ipsrc.s_addr = INADDR_ANY;
	}

	return 0;
}

static char *__indexable
bridge_mac_nat_entry_out(struct mac_nat_entry_list * list,
    unsigned int * count_p, char *__indexable buf,
    unsigned int * len_p)
{
	unsigned int            count = *count_p;
	struct ifbrmne          ifbmne;
	unsigned int            len = *len_p;
	struct mac_nat_entry    *mne;
	unsigned long           now;

	bzero(&ifbmne, sizeof(ifbmne));
	LIST_FOREACH(mne, list, mne_list) {
		if (len < sizeof(ifbmne)) {
			break;
		}
		snprintf(ifbmne.ifbmne_ifname, sizeof(ifbmne.ifbmne_ifname),
		    "%s", mne->mne_bif->bif_ifp->if_xname);
		memcpy(ifbmne.ifbmne_mac, mne->mne_mac,
		    sizeof(ifbmne.ifbmne_mac));
		now = (unsigned long) net_uptime();
		if (now < mne->mne_expire) {
			ifbmne.ifbmne_expire = mne->mne_expire - now;
		} else {
			ifbmne.ifbmne_expire = 0;
		}
		if ((mne->mne_flags & MNE_FLAGS_IPV6) != 0) {
			ifbmne.ifbmne_af = AF_INET6;
			ifbmne.ifbmne_ip6_addr = mne->mne_ip6;
		} else {
			ifbmne.ifbmne_af = AF_INET;
			ifbmne.ifbmne_ip_addr = mne->mne_ip;
		}
		memcpy(buf, &ifbmne, sizeof(ifbmne));
		count++;
		buf += sizeof(ifbmne);
		len -= sizeof(ifbmne);
	}
	*count_p = count;
	*len_p = len;
	return buf;
}

/*
 * bridge_ioctl_gmnelist()
 *   Perform the get mac_nat_entry list ioctl.
 *
 * Note:
 *   The struct ifbrmnelist32 and struct ifbrmnelist64 have the same
 *   field size/layout except for the last field ifbml_buf, the user-supplied
 *   buffer pointer. That is passed in separately via the 'user_addr'
 *   parameter from the respective 32-bit or 64-bit ioctl routine.
 */
static int
bridge_ioctl_gmnelist(struct bridge_softc *sc, struct ifbrmnelist32 *mnl,
    user_addr_t user_addr)
{
	unsigned int            count;
	char                    *buf;
	int                     error = 0;
	char                    *outbuf = NULL;
	struct mac_nat_entry    *mne;
	unsigned int            buflen;
	unsigned int            len;

	mnl->ifbml_elsize = sizeof(struct ifbrmne);
	count = 0;
	LIST_FOREACH(mne, &sc->sc_mne_list, mne_list) {
		count++;
	}
	LIST_FOREACH(mne, &sc->sc_mne_list_v6, mne_list) {
		count++;
	}
	buflen = sizeof(struct ifbrmne) * count;
	if (buflen == 0 || mnl->ifbml_len == 0) {
		mnl->ifbml_len = buflen;
		return error;
	}
	BRIDGE_UNLOCK(sc);
	outbuf = kalloc_data(buflen, Z_WAITOK | Z_ZERO);
	BRIDGE_LOCK(sc);
	count = 0;
	buf = outbuf;
	len = min(mnl->ifbml_len, buflen);
	buf = bridge_mac_nat_entry_out(&sc->sc_mne_list, &count, buf, &len);
	buf = bridge_mac_nat_entry_out(&sc->sc_mne_list_v6, &count, buf, &len);
	mnl->ifbml_len = count * sizeof(struct ifbrmne);
	BRIDGE_UNLOCK(sc);
	if (mnl->ifbml_len > 0) {
		error = copyout(outbuf, user_addr, mnl->ifbml_len);
	}
	kfree_data(outbuf, buflen);
	BRIDGE_LOCK(sc);
	return error;
}

static int
bridge_ioctl_gmnelist64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrmnelist64 * __single mnl = arg;

	return bridge_ioctl_gmnelist(sc, arg, mnl->ifbml_buf);
}

static int
bridge_ioctl_gmnelist32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrmnelist32 * __single mnl = arg;

	return bridge_ioctl_gmnelist(sc, arg,
	           CAST_USER_ADDR_T(mnl->ifbml_buf));
}

/*
 * bridge_ioctl_gifstats()
 *   Return per-member stats.
 *
 * Note:
 *   The ifbrmreq32 and ifbrmreq64 structures have the same
 *   field size/layout except for the last field brmr_buf, the user-supplied
 *   buffer pointer. That is passed in separately via the 'user_addr'
 *   parameter from the respective 32-bit or 64-bit ioctl routine.
 */
static int
bridge_ioctl_gifstats(struct bridge_softc *sc, struct ifbrmreq32 *mreq,
    user_addr_t user_addr)
{
	struct bridge_iflist    *bif;
	int                     error = 0;
	unsigned int            buflen;

	bif = bridge_lookup_member(sc, mreq->brmr_ifname);
	if (bif == NULL) {
		error = ENOENT;
		goto done;
	}

	buflen = mreq->brmr_elsize = sizeof(struct ifbrmstats);
	if (buflen == 0 || mreq->brmr_len == 0) {
		mreq->brmr_len = buflen;
		goto done;
	}
	if (mreq->brmr_len != 0 && mreq->brmr_len < buflen) {
		error = ENOBUFS;
		goto done;
	}
	mreq->brmr_len = buflen;
	error = copyout(&bif->bif_stats, user_addr, buflen);
done:
	return error;
}

static int
bridge_ioctl_gifstats32(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrmreq32 * __single mreq = arg;

	return bridge_ioctl_gifstats(sc, arg, mreq->brmr_buf);
}

static int
bridge_ioctl_gifstats64(struct bridge_softc *sc, void *__sized_by(arg_len) arg, size_t arg_len __unused)
{
	struct ifbrmreq64 * __single mreq = arg;

	return bridge_ioctl_gifstats(sc, arg, mreq->brmr_buf);
}

/*
 * bridge_proto_attach_changed
 *
 *	Called when protocol attachment on the interface changes.
 */
static void
bridge_proto_attach_changed(struct ifnet *ifp)
{
	boolean_t changed = FALSE;
	struct bridge_iflist *bif;
	boolean_t input_broadcast;
	struct bridge_softc * __single sc = ifp->if_bridge;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE, "%s", ifp->if_xname);
	if (sc == NULL) {
		return;
	}
	input_broadcast = interface_needs_input_broadcast(ifp);
	BRIDGE_LOCK(sc);
	bif = bridge_lookup_member_if(sc, ifp);
	if (bif != NULL) {
		changed = bif_set_input_broadcast(bif, input_broadcast);
	}
	BRIDGE_UNLOCK(sc);
	if (changed) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE,
		    "%s input broadcast %s", ifp->if_xname,
		    input_broadcast ? "ENABLED" : "DISABLED");
	}
	return;
}

/*
 * interface_media_active:
 *
 *	Tells if an interface media is active.
 */
static int
interface_media_active(struct ifnet *ifp)
{
	struct ifmediareq   ifmr;
	int status = 0;

	bzero(&ifmr, sizeof(ifmr));
	if (ifnet_ioctl(ifp, 0, SIOCGIFMEDIA, &ifmr) == 0) {
		if ((ifmr.ifm_status & IFM_AVALID) && ifmr.ifm_count > 0) {
			status = ifmr.ifm_status & IFM_ACTIVE ? 1 : 0;
		}
	}

	return status;
}

/*
 * bridge_updatelinkstatus:
 *
 *      Update the media active status of the bridge based on the
 *	media active status of its member.
 *	If changed, return the corresponding onf/off link event.
 */
static u_int32_t
bridge_updatelinkstatus(struct bridge_softc *sc)
{
	struct bridge_iflist *bif;
	int active_member = 0;
	u_int32_t event_code = 0;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	/*
	 * Find out if we have an active interface
	 */
	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (bif->bif_flags & BIFF_MEDIA_ACTIVE) {
			active_member = 1;
			break;
		}
	}

	if (active_member && !(sc->sc_flags & SCF_MEDIA_ACTIVE)) {
		sc->sc_flags |= SCF_MEDIA_ACTIVE;
		event_code = KEV_DL_LINK_ON;
	} else if (!active_member && (sc->sc_flags & SCF_MEDIA_ACTIVE)) {
		sc->sc_flags &= ~SCF_MEDIA_ACTIVE;
		event_code = KEV_DL_LINK_OFF;
	}

	return event_code;
}

/*
 * bridge_iflinkevent:
 */
static void
bridge_iflinkevent(struct ifnet *ifp)
{
	struct bridge_softc * __single sc = ifp->if_bridge;
	struct bridge_iflist *bif;
	u_int32_t event_code = 0;
	int media_active;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE, "%s", ifp->if_xname);

	/* Check if the interface is a bridge member */
	if (sc == NULL) {
		return;
	}

	media_active = interface_media_active(ifp);
	BRIDGE_LOCK(sc);
	bif = bridge_lookup_member_if(sc, ifp);
	if (bif != NULL) {
		if (media_active) {
			bif->bif_flags |= BIFF_MEDIA_ACTIVE;
		} else {
			bif->bif_flags &= ~BIFF_MEDIA_ACTIVE;
		}
		if (sc->sc_mac_nat_bif != NULL) {
			bridge_mac_nat_flush_entries(sc, bif);
		}

		event_code = bridge_updatelinkstatus(sc);
	}
	BRIDGE_UNLOCK(sc);

	if (event_code != 0) {
		bridge_link_event(sc->sc_ifp, event_code);
	}
}

/*
 * bridge_delayed_callback:
 *
 *	Makes a delayed call
 */
static void
bridge_delayed_callback(void *param, __unused void *param2)
{
	struct bridge_delayed_call *call = (struct bridge_delayed_call *)param;
	struct bridge_softc *sc = call->bdc_sc;

#if BRIDGE_DELAYED_CALLBACK_DEBUG
	if (bridge_delayed_callback_delay > 0) {
		struct timespec ts;

		ts.tv_sec = bridge_delayed_callback_delay;
		ts.tv_nsec = 0;

		BRIDGE_LOG(LOG_NOTICE, 0,
		    "sleeping for %d seconds",
		    bridge_delayed_callback_delay);

		msleep(&bridge_delayed_callback_delay, NULL, PZERO,
		    __func__, &ts);

		BRIDGE_LOG(LOG_NOTICE, 0, "awoken");
	}
#endif /* BRIDGE_DELAYED_CALLBACK_DEBUG */

	BRIDGE_LOCK(sc);

#if BRIDGE_DELAYED_CALLBACK_DEBUG
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_DELAYED_CALL,
	    "%s call 0x%llx flags 0x%x",
	    sc->sc_if_xname, (uint64_t)VM_KERNEL_ADDRPERM(call),
	    call->bdc_flags);
}
#endif /* BRIDGE_DELAYED_CALLBACK_DEBUG */

	if (call->bdc_flags & BDCF_CANCELLING) {
		wakeup(call);
	} else {
		if ((sc->sc_flags & SCF_DETACHING) == 0) {
			(*call->bdc_func)(sc);
		}
	}
	call->bdc_flags &= ~BDCF_OUTSTANDING;
	BRIDGE_UNLOCK(sc);
}

/*
 * bridge_schedule_delayed_call:
 *
 *	Schedule a function to be called on a separate thread
 *      The actual call may be scheduled to run at a given time or ASAP.
 */
static void
bridge_schedule_delayed_call(struct bridge_delayed_call *call)
{
	uint64_t deadline = 0;
	struct bridge_softc *sc = call->bdc_sc;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	if ((sc->sc_flags & SCF_DETACHING) ||
	    (call->bdc_flags & (BDCF_OUTSTANDING | BDCF_CANCELLING))) {
		return;
	}

	if (call->bdc_ts.tv_sec || call->bdc_ts.tv_nsec) {
		nanoseconds_to_absolutetime(
			(uint64_t)call->bdc_ts.tv_sec * NSEC_PER_SEC +
			call->bdc_ts.tv_nsec, &deadline);
		clock_absolutetime_interval_to_deadline(deadline, &deadline);
	}

	call->bdc_flags = BDCF_OUTSTANDING;

#if BRIDGE_DELAYED_CALLBACK_DEBUG
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_DELAYED_CALL,
	    "%s call 0x%llx flags 0x%x",
	    sc->sc_if_xname, (uint64_t)VM_KERNEL_ADDRPERM(call),
	    call->bdc_flags);
}
#endif /* BRIDGE_DELAYED_CALLBACK_DEBUG */

	if (call->bdc_ts.tv_sec || call->bdc_ts.tv_nsec) {
		thread_call_func_delayed(
			(thread_call_func_t)bridge_delayed_callback,
			call, deadline);
	} else {
		if (call->bdc_thread_call == NULL) {
			call->bdc_thread_call = thread_call_allocate(
				(thread_call_func_t)bridge_delayed_callback,
				call);
		}
		thread_call_enter(call->bdc_thread_call);
	}
}

/*
 * bridge_cancel_delayed_call:
 *
 *	Cancel a queued or running delayed call.
 *	If call is running, does not return until the call is done to
 *	prevent race condition with the brigde interface getting destroyed
 */
static void
bridge_cancel_delayed_call(struct bridge_delayed_call *call)
{
	boolean_t result;
	struct bridge_softc *sc = call->bdc_sc;

	/*
	 * The call was never scheduled
	 */
	if (sc == NULL) {
		return;
	}

	BRIDGE_LOCK_ASSERT_HELD(sc);

	call->bdc_flags |= BDCF_CANCELLING;

	while (call->bdc_flags & BDCF_OUTSTANDING) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_DELAYED_CALL,
		    "%s call 0x%llx flags 0x%x",
		    sc->sc_if_xname, (uint64_t)VM_KERNEL_ADDRPERM(call),
		    call->bdc_flags);
		result = thread_call_func_cancel(
			(thread_call_func_t)bridge_delayed_callback, call, FALSE);

		if (result) {
			/*
			 * We managed to dequeue the delayed call
			 */
			call->bdc_flags &= ~BDCF_OUTSTANDING;
		} else {
			/*
			 * Wait for delayed call do be done running
			 */
			msleep(call, &sc->sc_mtx, PZERO, __func__, NULL);
		}
	}
	call->bdc_flags &= ~BDCF_CANCELLING;
}

/*
 * bridge_cleanup_delayed_call:
 *
 *	Dispose resource allocated for a delayed call
 *	Assume the delayed call is not queued or running .
 */
static void
bridge_cleanup_delayed_call(struct bridge_delayed_call *call)
{
	boolean_t result;
	struct bridge_softc *sc = call->bdc_sc;

	/*
	 * The call was never scheduled
	 */
	if (sc == NULL) {
		return;
	}

	BRIDGE_LOCK_ASSERT_HELD(sc);

	VERIFY((call->bdc_flags & BDCF_OUTSTANDING) == 0);
	VERIFY((call->bdc_flags & BDCF_CANCELLING) == 0);

	if (call->bdc_thread_call != NULL) {
		result = thread_call_free(call->bdc_thread_call);
		if (result == FALSE) {
			panic("%s thread_call_free() failed for call %p",
			    __func__, call);
		}
		call->bdc_thread_call = NULL;
	}
}

/*
 * bridge_init:
 *
 *	Initialize a bridge interface.
 */
static int
bridge_init(struct ifnet *ifp)
{
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_softc;
	errno_t error;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	if ((ifnet_flags(ifp) & IFF_RUNNING)) {
		return 0;
	}

	error = ifnet_set_flags(ifp, IFF_RUNNING, IFF_RUNNING);

	/*
	 * Calling bridge_aging_timer() is OK as there are no entries to
	 * age so we're just going to arm the timer
	 */
	bridge_aging_timer(sc);
#if BRIDGESTP
	if (error == 0) {
		bstp_init(&sc->sc_stp); /* Initialize Spanning Tree */
	}
#endif /* BRIDGESTP */
	return error;
}

/*
 * bridge_ifstop:
 *
 *	Stop the bridge interface.
 */
static void
bridge_ifstop(struct ifnet *ifp, int disable)
{
#pragma unused(disable)
	struct bridge_softc * __single sc = ifp->if_softc;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	if ((ifnet_flags(ifp) & IFF_RUNNING) == 0) {
		return;
	}

	bridge_cancel_delayed_call(&sc->sc_aging_timer);

#if BRIDGESTP
	bstp_stop(&sc->sc_stp);
#endif /* BRIDGESTP */

	bridge_rtflush(sc, IFBF_FLUSHDYN);
	(void) ifnet_set_flags(ifp, 0, IFF_RUNNING);
}

/*
 * bridge_compute_cksum:
 *
 *	If the packet has checksum flags, compare the hardware checksum
 *	capabilities of the source and destination interfaces. If they
 *	are the same, there's nothing to do. If they are different,
 *	finalize the checksum so that it can be sent on the destination
 *	interface.
 */
static void
bridge_compute_cksum(struct ifnet *src_if, struct ifnet *dst_if, struct mbuf *m)
{
	uint32_t csum_flags;
	uint16_t dst_hw_csum;
	uint32_t did_sw = 0;
	struct ether_header *eh;
	uint16_t src_hw_csum;

	if (src_if == dst_if) {
		return;
	}
	csum_flags = m->m_pkthdr.csum_flags & IF_HWASSIST_CSUM_MASK;
	if (csum_flags == 0) {
		/* no checksum offload */
		return;
	}

	/*
	 * if destination/source differ in checksum offload
	 * capabilities, finalize/compute the checksum
	 */
	dst_hw_csum = IF_HWASSIST_CSUM_FLAGS(dst_if->if_hwassist);
	src_hw_csum = IF_HWASSIST_CSUM_FLAGS(src_if->if_hwassist);
	if (dst_hw_csum == src_hw_csum) {
		return;
	}
	eh = mtod(m, struct ether_header *);
	switch (eh->ether_type) {
	case HTONS_ETHERTYPE_IP:
		did_sw = in_finalize_cksum(m, sizeof(*eh), csum_flags);
		break;
	case HTONS_ETHERTYPE_IPV6:
		did_sw = in6_finalize_cksum(m, sizeof(*eh), -1, -1, csum_flags);
		break;
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "[%s -> %s] before 0x%x did 0x%x after 0x%x",
	    src_if->if_xname, dst_if->if_xname, csum_flags, did_sw,
	    m->m_pkthdr.csum_flags);
}

static inline errno_t
bridge_transmit(ifnet_t ifp, mbuf_t m)
{
	struct flowadv  adv = { .code = FADV_SUCCESS };
	errno_t         error;
	int             flags = DLIL_OUTPUT_FLAGS_RAW;

	flags = (if_bridge_output_skip_filters != 0)
	    ? (DLIL_OUTPUT_FLAGS_RAW | DLIL_OUTPUT_FLAGS_SKIP_IF_FILTERS)
	    : DLIL_OUTPUT_FLAGS_RAW;
	error = dlil_output(ifp, 0, m, NULL, NULL, flags, &adv);
	if (error == 0) {
		if (adv.code == FADV_FLOW_CONTROLLED) {
			error = EQFULL;
		} else if (adv.code == FADV_SUSPENDED) {
			error = EQSUSPENDED;
		}
	}
	return error;
}

static int
get_last_ip6_hdr(struct mbuf *m, int off, int proto, int * nxtp,
    bool *is_fragmented)
{
	int newoff;

	*is_fragmented = false;
	while (1) {
		newoff = ip6_nexthdr(m, off, proto, nxtp);
		if (newoff < 0) {
			return off;
		} else if (newoff < off) {
			return -1;    /* invalid */
		} else if (newoff == off) {
			return newoff;
		}
		off = newoff;
		proto = *nxtp;
		if (proto == IPPROTO_FRAGMENT) {
			*is_fragmented = true;
		}
	}
}

#define __ATOMIC_INC(s) os_atomic_inc(&s, relaxed)

static int
bridge_get_ip_proto(struct mbuf * * mp, u_int mac_hlen, bool is_ipv4,
    ip_packet_info_t info_p, struct bripstats * stats_p)
{
	int             error = 0;
	u_int           hlen;
	u_int           ip_hlen;
	u_int           ip_pay_len;
	struct mbuf *   m0 = *mp;
	int             off;
	int             opt_len = 0;
	int             proto = 0;

	bzero(info_p, sizeof(*info_p));
	if (is_ipv4) {
		struct ip *     ip;
		u_int           ip_total_len;

		/* IPv4 */
		hlen = mac_hlen + sizeof(struct ip);
		if (m0->m_pkthdr.len < hlen) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "Short IP packet %d < %d",
			    m0->m_pkthdr.len, hlen);
			error = _EBADIP;
			__ATOMIC_INC(stats_p->bips_bad_ip);
			goto done;
		}
		if (m0->m_len < hlen) {
			*mp = m0 = m_pullup(m0, hlen);
			if (m0 == NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "m_pullup failed hlen %d",
				    hlen);
				error = ENOBUFS;
				__ATOMIC_INC(stats_p->bips_bad_ip);
				goto done;
			}
		}
		ip = (struct ip *)mtodo(m0, mac_hlen);
		if (IP_VHL_V(ip->ip_vhl) != IPVERSION) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "bad IP version");
			error = _EBADIP;
			__ATOMIC_INC(stats_p->bips_bad_ip);
			goto done;
		}
		ip_hlen = IP_VHL_HL(ip->ip_vhl) << 2;
		if (ip_hlen < sizeof(struct ip)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "bad IP header length %d < %d",
			    ip_hlen,
			    (int)sizeof(struct ip));
			error = _EBADIP;
			__ATOMIC_INC(stats_p->bips_bad_ip);
			goto done;
		}
		hlen = mac_hlen + ip_hlen;
		if (m0->m_len < hlen) {
			*mp = m0 = m_pullup(m0, hlen);
			if (m0 == NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "m_pullup failed hlen %d",
				    hlen);
				error = ENOBUFS;
				__ATOMIC_INC(stats_p->bips_bad_ip);
				goto done;
			}
			ip = (struct ip *)mtodo(m0, mac_hlen);
		}

		ip_total_len = ntohs(ip->ip_len);
		if (ip_total_len < ip_hlen) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "IP total len %d < header len %d",
			    ip_total_len, ip_hlen);
			error = _EBADIP;
			__ATOMIC_INC(stats_p->bips_bad_ip);
			goto done;
		}
		if (ip_total_len > (m0->m_pkthdr.len - mac_hlen)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "invalid IP payload length %d > %d",
			    ip_total_len,
			    (m0->m_pkthdr.len - mac_hlen));
			error = _EBADIP;
			__ATOMIC_INC(stats_p->bips_bad_ip);
			goto done;
		}
		ip_pay_len = ip_total_len - ip_hlen;
		info_p->ip_proto = ip->ip_p;
		info_p->ip_hdr = mtodo(m0, mac_hlen);
		info_p->ip_m0_len = m0->m_len - mac_hlen;
		info_p->ip_hlen = ip_hlen;
#define FRAG_BITS       (IP_OFFMASK | IP_MF)
		if ((ntohs(ip->ip_off) & FRAG_BITS) != 0) {
			info_p->ip_is_fragmented = true;
		}
		__ATOMIC_INC(stats_p->bips_ip);
	} else {
		struct ip6_hdr *ip6;

		/* IPv6 */
		hlen = mac_hlen + sizeof(struct ip6_hdr);
		if (m0->m_pkthdr.len < hlen) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "short IPv6 packet %d < %d",
			    m0->m_pkthdr.len, hlen);
			error = _EBADIPV6;
			__ATOMIC_INC(stats_p->bips_bad_ip6);
			goto done;
		}
		if (m0->m_len < hlen) {
			*mp = m0 = m_pullup(m0, hlen);
			if (m0 == NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "m_pullup failed hlen %d",
				    hlen);
				error = ENOBUFS;
				__ATOMIC_INC(stats_p->bips_bad_ip6);
				goto done;
			}
		}
		ip6 = (struct ip6_hdr *)(mtodo(m0, mac_hlen));
		if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "bad IPv6 version");
			error = _EBADIPV6;
			__ATOMIC_INC(stats_p->bips_bad_ip6);
			goto done;
		}
		off = get_last_ip6_hdr(m0, mac_hlen, IPPROTO_IPV6, &proto,
		    &info_p->ip_is_fragmented);
		if (off < 0 || m0->m_pkthdr.len < off) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "ip6_lasthdr() returned %d",
			    off);
			error = _EBADIPV6;
			__ATOMIC_INC(stats_p->bips_bad_ip6);
			goto done;
		}
		ip_hlen = sizeof(*ip6);
		opt_len = off - mac_hlen - ip_hlen;
		if (opt_len < 0) {
			error = _EBADIPV6;
			__ATOMIC_INC(stats_p->bips_bad_ip6);
			goto done;
		}
		ip_pay_len = ntohs(ip6->ip6_plen);
		if (ip_pay_len > (m0->m_pkthdr.len - mac_hlen - ip_hlen)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "invalid IPv6 payload length %d > %d",
			    ip_pay_len,
			    (m0->m_pkthdr.len - mac_hlen - ip_hlen));
			error = _EBADIPV6;
			__ATOMIC_INC(stats_p->bips_bad_ip6);
			goto done;
		}
		info_p->ip_proto = proto;
		info_p->ip_hdr = mtodo(m0, mac_hlen);
		info_p->ip_m0_len = m0->m_len - mac_hlen;
		info_p->ip_hlen = ip_hlen;
		__ATOMIC_INC(stats_p->bips_ip6);
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "IPv%c proto %d ip %u pay %u opt %u pkt %u%s",
	    is_ipv4 ? '4' : '6',
	    proto, ip_hlen, ip_pay_len, opt_len,
	    m0->m_pkthdr.len, info_p->ip_is_fragmented ? " frag" : "");
	info_p->ip_pay_len = ip_pay_len;
	info_p->ip_opt_len = opt_len;
	info_p->ip_is_ipv4 = is_ipv4;
done:
	return error;
}

static int
bridge_get_tcp_header(struct mbuf * * mp, u_int mac_hlen, bool is_ipv4,
    ip_packet_info_t info_p, struct bripstats * stats_p)
{
	int             error;
	u_int           hlen;

	error = bridge_get_ip_proto(mp, mac_hlen, is_ipv4, info_p, stats_p);
	if (error != 0) {
		goto done;
	}
	if (info_p->ip_proto != IPPROTO_TCP) {
		/* not a TCP frame, not an error, just a bad guess */
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "non-TCP (%d) IPv%c frame %d bytes",
		    info_p->ip_proto, is_ipv4 ? '4' : '6',
		    (*mp)->m_pkthdr.len);
		goto done;
	}
	if (info_p->ip_is_fragmented) {
		/* both TSO and IP fragmentation don't make sense */
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_CHECKSUM,
		    "fragmented TSO packet?");
		__ATOMIC_INC(stats_p->bips_bad_tcp);
		error = _EBADTCP;
		goto done;
	}
	hlen = mac_hlen + info_p->ip_hlen + sizeof(struct tcphdr) +
	    info_p->ip_opt_len;
	if ((*mp)->m_len < hlen) {
		*mp = m_pullup(*mp, hlen);
		if (*mp == NULL) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "m_pullup %d failed",
			    hlen);
			__ATOMIC_INC(stats_p->bips_bad_tcp);
			error = _EBADTCP;
			goto done;
		}
	}
	info_p->ip_proto_hdr = info_p->ip_hdr + info_p->ip_hlen +
	    info_p->ip_opt_len;
done:
	return error;
}

static inline void
proto_csum_stats_increment(uint8_t proto, struct brcsumstats * stats_p)
{
	if (proto == IPPROTO_TCP) {
		__ATOMIC_INC(stats_p->brcs_tcp_checksum);
	} else {
		__ATOMIC_INC(stats_p->brcs_udp_checksum);
	}
	return;
}

#define ETHER_TYPE_FLAG_NONE    0x00
#define ETHER_TYPE_FLAG_IPV4    0x01
#define ETHER_TYPE_FLAG_IPV6    0x02
#define ETHER_TYPE_FLAG_ARP     0x04
#define ETHER_TYPE_FLAG_IP      (ETHER_TYPE_FLAG_IPV4 | ETHER_TYPE_FLAG_IPV6)
#define ETHER_TYPE_FLAG_IP_ARP  (ETHER_TYPE_FLAG_IP | ETHER_TYPE_FLAG_ARP)

typedef uint8_t ether_type_flag_t;

static inline bool
ether_type_flag_is_ip(ether_type_flag_t flag)
{
	return (flag & ETHER_TYPE_FLAG_IP) != 0;
}

static inline ether_type_flag_t
ether_type_flag_get(uint16_t ether_type)
{
	ether_type_flag_t flag = ETHER_TYPE_FLAG_NONE;

	switch (ether_type) {
	case HTONS_ETHERTYPE_IP:
		flag = ETHER_TYPE_FLAG_IPV4;
		break;
	case HTONS_ETHERTYPE_IPV6:
		flag = ETHER_TYPE_FLAG_IPV6;
		break;
	case HTONS_ETHERTYPE_ARP:
		flag = ETHER_TYPE_FLAG_ARP;
		break;
	default:
		break;
	}
	return flag;
}

static bool
ether_header_type_is_ip(struct ether_header * eh, bool *is_ipv4)
{
	uint16_t        ether_type;
	bool            is_ip = TRUE;

	ether_type = ntohs(eh->ether_type);
	switch (ether_type) {
	case ETHERTYPE_IP:
		*is_ipv4 = TRUE;
		break;
	case ETHERTYPE_IPV6:
		*is_ipv4 = FALSE;
		break;
	default:
		is_ip = FALSE;
		break;
	}
	return is_ip;
}

static errno_t
bridge_verify_checksum(struct mbuf * * mp, struct ifbrmstats *stats_p)
{
	struct brcsumstats *csum_stats_p;
	struct ether_header     *eh;
	errno_t         error = 0;
	ip_packet_info  info;
	bool            is_ipv4;
	struct mbuf *   m;
	u_int           mac_hlen = sizeof(struct ether_header);
	uint16_t        sum;
	bool            valid;

	eh = mtod(*mp, struct ether_header *);
	if (!ether_header_type_is_ip(eh, &is_ipv4)) {
		goto done;
	}
	error = bridge_get_ip_proto(mp, mac_hlen, is_ipv4, &info,
	    &stats_p->brms_out_ip);
	m = *mp;
	if (error != 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "bridge_get_ip_proto failed %d",
		    error);
		goto done;
	}
	if (is_ipv4) {
		if ((m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) != 0) {
			/* hardware offloaded IP header checksum */
			valid = (m->m_pkthdr.csum_flags & CSUM_IP_VALID) != 0;
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "IP checksum HW %svalid",
			    valid ? "" : "in");
			if (!valid) {
				__ATOMIC_INC(stats_p->brms_out_cksum_bad_hw.brcs_ip_checksum);
				error = _EBADIPCHECKSUM;
				goto done;
			}
			__ATOMIC_INC(stats_p->brms_out_cksum_good_hw.brcs_ip_checksum);
		} else {
			/* verify */
			sum = inet_cksum(m, 0, mac_hlen, info.ip_hlen);
			valid = (sum == 0);
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "IP checksum SW %svalid",
			    valid ? "" : "in");
			if (!valid) {
				__ATOMIC_INC(stats_p->brms_out_cksum_bad.brcs_ip_checksum);
				error = _EBADIPCHECKSUM;
				goto done;
			}
			__ATOMIC_INC(stats_p->brms_out_cksum_good.brcs_ip_checksum);
		}
	}
	if (info.ip_is_fragmented) {
		/* can't verify checksum on fragmented packets */
		goto done;
	}
	switch (info.ip_proto) {
	case IPPROTO_TCP:
		__ATOMIC_INC(stats_p->brms_out_ip.bips_tcp);
		break;
	case IPPROTO_UDP:
		__ATOMIC_INC(stats_p->brms_out_ip.bips_udp);
		break;
	default:
		goto done;
	}
	/* check for hardware offloaded UDP/TCP checksum */
#define HW_CSUM         (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)
	if ((m->m_pkthdr.csum_flags & HW_CSUM) == HW_CSUM) {
		/* checksum verified by hardware */
		valid = (m->m_pkthdr.csum_rx_val == 0xffff);
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "IPv%c %s checksum HW 0x%x %svalid",
		    is_ipv4 ? '4' : '6',
		    (info.ip_proto == IPPROTO_TCP)
		    ? "TCP" : "UDP",
		    m->m_pkthdr.csum_data,
		    valid ? "" : "in" );
		if (!valid) {
			/* bad checksum */
			csum_stats_p = &stats_p->brms_out_cksum_bad_hw;
			error = (info.ip_proto == IPPROTO_TCP) ? _EBADTCPCHECKSUM
			    : _EBADTCPCHECKSUM;
		} else {
			/* good checksum */
			csum_stats_p = &stats_p->brms_out_cksum_good_hw;
		}
		proto_csum_stats_increment(info.ip_proto, csum_stats_p);
		goto done;
	}
	/* adjust frame to skip mac-layer header */
	_mbuf_adjust_pkthdr_and_data(m, mac_hlen);
	if (is_ipv4) {
		sum = inet_cksum(m, info.ip_proto,
		    info.ip_hlen,
		    info.ip_pay_len);
	} else {
		sum = inet6_cksum(m, info.ip_proto,
		    info.ip_hlen + info.ip_opt_len,
		    info.ip_pay_len - info.ip_opt_len);
	}
	valid = (sum == 0);
	if (valid) {
		csum_stats_p = &stats_p->brms_out_cksum_good;
	} else {
		csum_stats_p = &stats_p->brms_out_cksum_bad;
		error = (info.ip_proto == IPPROTO_TCP)
		    ? _EBADTCPCHECKSUM : _EBADUDPCHECKSUM;
	}
	proto_csum_stats_increment(info.ip_proto, csum_stats_p);
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "IPv%c %s checksum SW %svalid (0x%x) hlen %d paylen %d",
	    is_ipv4 ? '4' : '6',
	    (info.ip_proto == IPPROTO_TCP) ? "TCP" : "UDP",
	    valid ? "" : "in",
	    sum, info.ip_hlen, info.ip_pay_len);
	/* adjust frame back to start of mac-layer header */
	_mbuf_adjust_pkthdr_and_data(m, -mac_hlen);

done:
	return error;
}

static mbuf_t
bridge_verify_checksum_list(mbuf_t in_list, struct bridge_iflist * dbif)
{
	mbuf_t          next_packet;
	mblist          ret;

	mblist_init(&ret);
	for (mbuf_ref_t scan = in_list; scan != NULL; scan = next_packet) {
		errno_t         error;

		/* take packet out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;

		/* verify checksum */
		error = bridge_verify_checksum(&scan, &dbif->bif_stats);
		if (error != 0) {
			if (scan != NULL) {
				m_freem(scan);
				scan = NULL;
			}
		}

		/* add it back to the list */
		if (scan != NULL) {
			mblist_append(&ret, scan);
		}
	}
	return ret.head;
}


static errno_t
bridge_offload_checksum(struct mbuf * * mp, ip_packet_info * info_p,
    struct ifbrmstats * stats_p)
{
	uint16_t *      csum_p;
	errno_t         error = 0;
	u_int           hlen;
	struct mbuf *   m0 = *mp;
	u_int           mac_hlen = sizeof(struct ether_header);
	u_int           pkt_hdr_len;
	struct tcphdr * tcp;
	u_int           tcp_hlen;
	struct udphdr * udp;

	if (info_p->ip_is_ipv4) {
		/* compute IP header checksum */
		struct ip *ip = (struct ip *)info_p->ip_hdr;
		ip->ip_sum = 0;
		ip->ip_sum = inet_cksum(m0, 0, mac_hlen, info_p->ip_hlen);
		__ATOMIC_INC(stats_p->brms_in_computed_cksum.brcs_ip_checksum);
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "IPv4 checksum 0x%x",
		    ntohs(ip->ip_sum));
	}
	if (info_p->ip_is_fragmented) {
		/* can't compute checksum on fragmented packets */
		goto done;
	}
	pkt_hdr_len = m0->m_pkthdr.len;
	switch (info_p->ip_proto) {
	case IPPROTO_TCP:
		hlen = mac_hlen + info_p->ip_hlen + info_p->ip_opt_len
		    + sizeof(struct tcphdr);
		if (m0->m_len < hlen) {
			*mp = m0 = m_pullup(m0, hlen);
			if (m0 == NULL) {
				__ATOMIC_INC(stats_p->brms_in_ip.bips_bad_tcp);
				error = _EBADTCP;
				goto done;
			}
		}
		tcp = (struct tcphdr *)(info_p->ip_hdr + info_p->ip_hlen
		    + info_p->ip_opt_len);
		tcp_hlen = tcp->th_off << 2;
		hlen = mac_hlen + info_p->ip_hlen + info_p->ip_opt_len + tcp_hlen;
		if (hlen > pkt_hdr_len) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "bad tcp header length %u",
			    tcp_hlen);
			__ATOMIC_INC(stats_p->brms_in_ip.bips_bad_tcp);
			error = _EBADTCP;
			goto done;
		}
		csum_p = &tcp->th_sum;
		__ATOMIC_INC(stats_p->brms_in_ip.bips_tcp);
		break;
	case IPPROTO_UDP:
		hlen = mac_hlen + info_p->ip_hlen + info_p->ip_opt_len + sizeof(*udp);
		if (m0->m_len < hlen) {
			*mp = m0 = m_pullup(m0, hlen);
			if (m0 == NULL) {
				__ATOMIC_INC(stats_p->brms_in_ip.bips_bad_udp);
				error = ENOBUFS;
				goto done;
			}
		}
		udp = (struct udphdr *)(info_p->ip_hdr + info_p->ip_hlen
		    + info_p->ip_opt_len);
		csum_p = &udp->uh_sum;
		__ATOMIC_INC(stats_p->brms_in_ip.bips_udp);
		break;
	default:
		/* not TCP or UDP */
		goto done;
	}
	*csum_p = 0;
	/* adjust frame to skip mac-layer header */
	_mbuf_adjust_pkthdr_and_data(m0, mac_hlen);
	if (info_p->ip_is_ipv4) {
		*csum_p = inet_cksum(m0, info_p->ip_proto, info_p->ip_hlen,
		    info_p->ip_pay_len);
	} else {
		*csum_p = inet6_cksum(m0, info_p->ip_proto,
		    info_p->ip_hlen + info_p->ip_opt_len,
		    info_p->ip_pay_len - info_p->ip_opt_len);
	}
	if (info_p->ip_proto == IPPROTO_UDP && *csum_p == 0) {
		/* RFC 1122 4.1.3.4 */
		*csum_p = 0xffff;
	}
	/* adjust frame back to start of mac-layer header */
	_mbuf_adjust_pkthdr_and_data(m0, -mac_hlen);
	proto_csum_stats_increment(info_p->ip_proto,
	    &stats_p->brms_in_computed_cksum);

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "IPv%c %s set checksum 0x%x",
	    info_p->ip_is_ipv4 ? '4' : '6',
	    (info_p->ip_proto == IPPROTO_TCP) ? "TCP" : "UDP",
	    ntohs(*csum_p));
done:
	return error;
}

static inline void
bridge_handle_checksum_op(ifnet_t src_ifp, ifnet_t dst_ifp,
    mbuf_t m, ChecksumOperation cksum_op)
{
	switch (cksum_op) {
	case CHECKSUM_OPERATION_CLEAR_OFFLOAD:
		m->m_pkthdr.csum_flags &= ~CSUM_TX_FLAGS;
		break;
	case CHECKSUM_OPERATION_FINALIZE:
		/* the checksum might not be correct, finalize now */
		VERIFY(dst_ifp != NULL);
		bridge_finalize_cksum(dst_ifp, m);
		break;
	case CHECKSUM_OPERATION_COMPUTE:
		VERIFY(dst_ifp != NULL && src_ifp != NULL);
		bridge_compute_cksum(src_ifp, dst_ifp, m);
		break;
	default:
		break;
	}
	return;
}

static errno_t
bridge_send(ifnet_t src_ifp, ifnet_t dst_ifp,
    mbuf_t m, ChecksumOperation cksum_op)
{
	bridge_handle_checksum_op(src_ifp, dst_ifp, m, cksum_op);
#if HAS_IF_CAP
	/*
	 * If underlying interface can not do VLAN tag insertion itself
	 * then attach a packet tag that holds it.
	 */
	if ((m->m_flags & M_VLANTAG) &&
	    (dst_ifp->if_capenable & IFCAP_VLAN_HWTAGGING) == 0) {
		m = ether_vlanencap(m, m->m_pkthdr.ether_vtag);
		if (m == NULL) {
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_CHECKSUM,
			    "%s: unable to prepend VLAN header",
			    dst_ifp->if_xname);
			(void) ifnet_stat_increment_out(dst_ifp,
			    0, 0, 1);
			return 0;
		}
		m->m_flags &= ~M_VLANTAG;
	}
#endif /* HAS_IF_CAP */
	return bridge_transmit(dst_ifp, m);
}

static errno_t
bridge_send_tso(struct ifnet *dst_ifp, struct mbuf *m, bool is_ipv4)
{
	errno_t                 error = 0;
	u_int                   mac_hlen;
	mblist                  segs;

	mac_hlen = sizeof(struct ether_header);

#if HAS_IF_CAP
	/*
	 * If underlying interface can not do VLAN tag insertion itself
	 * then attach a packet tag that holds it.
	 */
	if ((m->m_flags & M_VLANTAG) &&
	    (dst_ifp->if_capenable & IFCAP_VLAN_HWTAGGING) == 0) {
		m = ether_vlanencap(m, m->m_pkthdr.ether_vtag);
		if (m == NULL) {
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_CHECKSUM,
			    "%s: unable to prepend VLAN header",
			    dst_ifp->if_xname);
			(void) ifnet_stat_increment_out(dst_ifp,
			    0, 0, 1);
			error = ENOBUFS;
			goto done;
		}
		m->m_flags &= ~M_VLANTAG;
		mac_hlen += ETHER_VLAN_ENCAP_LEN;
	}
#endif /* HAS_IF_CAP */
	segs = gso_tcp_transmit(dst_ifp, m, mac_hlen, is_ipv4);
	if (segs.head != NULL) {
		error = bridge_transmit(dst_ifp, segs.head);
	}
	return error;
}

static uint32_t
get_if_tso_mtu(struct ifnet * ifp, bool is_ipv4)
{
	uint32_t tso_mtu;

	tso_mtu = is_ipv4 ? ifp->if_tso_v4_mtu : ifp->if_tso_v6_mtu;
	if (tso_mtu == 0) {
		tso_mtu = IP_MAXPACKET;
	}

#if DEBUG || DEVELOPMENT
#define REDUCED_TSO_MTU         (16 * 1024)
	if (if_bridge_reduce_tso_mtu != 0 && tso_mtu > REDUCED_TSO_MTU) {
		tso_mtu = REDUCED_TSO_MTU;
	}
#endif /* DEBUG || DEVELOPMENT */
	return tso_mtu;
}

/*
 * tso_hwassist:
 * - determine whether the destination interface supports TSO offload
 * - if the packet is already marked for offload and the hardware supports
 *   it, just allow the packet to continue on
 * - if not, parse the packet headers to verify that this is a large TCP
 *   packet requiring segmentation; if the hardware doesn't support it
 *   set need_sw_tso; otherwise, mark the packet for TSO offload
 */
static int
tso_hwassist(struct mbuf **mp, bool is_ipv4, struct ifnet * ifp, u_int mac_hlen,
    bool * need_sw_tso, bool * is_large_tcp)
{
	int             error = 0;
	u_int32_t       if_csum;
	u_int32_t       if_tso;
	u_int32_t       mbuf_tso;
	bool            supports_cksum = false;

	*need_sw_tso = false;
	*is_large_tcp = false;
	if (is_ipv4) {
		/*
		 * Enable both TCP and IP offload if the hardware supports it.
		 * If the hardware doesn't support TCP offload, supports_cksum
		 * will be false so we won't set either offload.
		 */
		if_csum = ifp->if_hwassist & (CSUM_TCP | CSUM_IP);
		supports_cksum = (if_csum & CSUM_TCP) != 0;
		if_tso = IFNET_TSO_IPV4;
		mbuf_tso = CSUM_TSO_IPV4;
	} else {
		supports_cksum = (ifp->if_hwassist & CSUM_TCPIPV6) != 0;
		if_csum = CSUM_TCPIPV6;
		if_tso = IFNET_TSO_IPV6;
		mbuf_tso = CSUM_TSO_IPV6;
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "%s: does%s support checksum 0x%x if_csum 0x%x",
	    ifp->if_xname, supports_cksum ? "" : " not",
	    ifp->if_hwassist, if_csum);
	if ((ifp->if_hwassist & if_tso) != 0 &&
	    ((*mp)->m_pkthdr.csum_flags & mbuf_tso) != 0) {
		/* hardware TSO, mbuf already marked */
	} else {
		/* verify that this is a large TCP frame */
		uint32_t                csum_flags;
		ip_packet_info          info;
		int                     mss;
		uint32_t                pkt_mtu;
		struct bripstats        stats;
		struct tcphdr *         tcp;
		uint32_t                tso_mtu;

		error = bridge_get_tcp_header(mp, mac_hlen, is_ipv4,
		    &info, &stats);
		if (error != 0) {
			/* bad packet */
			goto done;
		}
		if (info.ip_proto_hdr == NULL) {
			/* not a TCP packet */
			goto done;
		}
		pkt_mtu = info.ip_hlen + info.ip_pay_len + info.ip_opt_len;
		if (pkt_mtu <= ifp->if_mtu) {
			/* not actually a large packet */
			goto done;
		}
		if ((ifp->if_hwassist & if_tso) == 0) {
			/* hardware does not support TSO, enable sw tso */
			*need_sw_tso = if_bridge_segmentation != 0;
			goto done;
		}
		tso_mtu = get_if_tso_mtu(ifp, is_ipv4);
		if (pkt_mtu > tso_mtu) {
			/* hardware can't segment this, enable sw tso */
			*need_sw_tso = if_bridge_segmentation != 0;
			goto done;
		}

		/* use hardware TSO */
		(*mp)->m_pkthdr.pkt_proto = IPPROTO_TCP;
		tcp = (struct tcphdr *)info.ip_proto_hdr;
		mss = ifp->if_mtu - info.ip_hlen - info.ip_opt_len
		    - (tcp->th_off << 2) - if_bridge_tso_reduce_mss_tx;
		assert(mss > 0);
		csum_flags = mbuf_tso;
		if (supports_cksum) {
			csum_flags |= if_csum;
		}
		(*mp)->m_pkthdr.tso_segsz = mss;
		(*mp)->m_pkthdr.csum_flags |= csum_flags;
		(*mp)->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
		*is_large_tcp = true;
	}
done:
	return error;
}

/*
 * bridge_enqueue:
 *
 *	Enqueue a packet on a bridge member interface.
 *
 */
static errno_t
bridge_enqueue(ifnet_t bridge_ifp, struct ifnet *src_ifp,
    struct ifnet *dst_ifp, struct mbuf *m, ChecksumOperation cksum_op)
{
	errno_t         error = 0;
	int             len;

	VERIFY(dst_ifp != NULL);

	for (struct mbuf *next_m = NULL; m != NULL; m = next_m) {
		bool            need_sw_tso = false;
		bool            is_ipv4 = false;
		bool            is_large_pkt;
		errno_t         _error = 0;

		len = m->m_pkthdr.len;
		m->m_flags |= M_PROTO1; /* set to avoid loops */
		next_m = m->m_nextpkt;
		m->m_nextpkt = NULL;
		/*
		 * Need to segment the packet if it is a large frame
		 * and the destination interface does not support TSO.
		 *
		 * Note that with trailers, it's possible for a packet to
		 * be large but not actually require segmentation.
		 */
		is_large_pkt = (len > (bridge_ifp->if_mtu + ETHER_HDR_LEN));
		if (is_large_pkt) {
			struct ether_header     *eh;
			bool                    is_large_tcp = false;

			eh = mtod(m, struct ether_header *);
			if (ether_header_type_is_ip(eh, &is_ipv4)) {
				_error = tso_hwassist(&m, is_ipv4,
				    dst_ifp, sizeof(struct ether_header),
				    &need_sw_tso, &is_large_tcp);
				if (is_large_tcp) {
					cksum_op = CHECKSUM_OPERATION_NONE;
				}
			} else {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "large non IP packet");
			}
		}
		if (_error != 0) {
			if (m != NULL) {
				m_freem(m);
			}
		} else if (need_sw_tso) {
			_error = bridge_send_tso(dst_ifp, m, is_ipv4);
		} else {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "%s bridge_send(%s) len %d op %d",
			    bridge_ifp->if_xname,
			    dst_ifp->if_xname,
			    len, cksum_op);
			_error = bridge_send(src_ifp, dst_ifp, m, cksum_op);
		}

		/* Preserve first error value */
		if (error == 0 && _error != 0) {
			error = _error;
		}
		if (_error == 0) {
			(void) ifnet_stat_increment_out(bridge_ifp, 1, len, 0);
		} else {
			(void) ifnet_stat_increment_out(bridge_ifp, 0, 0, 1);
		}
	}

	return error;
}

static void
bridge_enqueue_multi(ifnet_t bridge_ifp, ifnet_t src_if, ifnet_t dst_if,
    ether_type_flag_t etypef, mbuf_t in_list, ChecksumOperation orig_cksum_op)
{
	mbuf_t          next_packet;
	uint32_t        out_errors = 0;
	mblist          out_list;

	VERIFY(dst_if != NULL);

	mblist_init(&out_list);
	for (mbuf_ref_t scan = in_list; scan != NULL; scan = next_packet) {
		ChecksumOperation cksum_op = orig_cksum_op;
		errno_t         error = 0;
		bool            is_ipv4 = false;
		bool            is_large_pkt;
		int             len;
		bool            need_sw_tso = false;

		scan->m_flags |= M_PROTO1; /* set to avoid loops */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;
		len = mbuf_pkthdr_len(scan);

		/*
		 * Need to segment the packet if it is a large frame
		 * and the destination interface does not support TSO.
		 *
		 * Note that with trailers, it's possible for a packet to
		 * be large but not actually require segmentation.
		 */
		is_large_pkt = (len > (bridge_ifp->if_mtu + ETHER_HDR_LEN));
		if (is_large_pkt) {
			bool    is_large_tcp = false;

			if (ether_type_flag_is_ip(etypef)) {
				is_ipv4 = (etypef == ETHER_TYPE_FLAG_IPV4);
				error = tso_hwassist(&scan, is_ipv4,
				    dst_if, sizeof(struct ether_header),
				    &need_sw_tso, &is_large_tcp);
				if (is_large_tcp) {
					cksum_op = CHECKSUM_OPERATION_NONE;
				}
			} else {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "large non IP packet");
			}
		}
		if (error != 0) {
			if (scan != NULL) {
				m_freem(scan);
				scan = NULL;
			}
			out_errors++;
		} else if (need_sw_tso) {
			int             mac_hlen = sizeof(struct ether_header);
			mblist          segs;

			/* segment packets, add to list */
			segs = gso_tcp_transmit(dst_if, scan, mac_hlen,
			    is_ipv4);
			if (segs.head != NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "%s (%s) append gso #segs %u bytes %u",
				    bridge_ifp->if_xname,
				    dst_if->if_xname,
				    segs.count, segs.bytes);
				mblist_append_list(&out_list, segs);
			} else {
				out_errors++;
			}
		} else {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
			    "%s (%s) append %d bytes op %d",
			    bridge_ifp->if_xname,
			    dst_if->if_xname,
			    len, cksum_op);
			bridge_handle_checksum_op(src_if, dst_if,
			    scan, cksum_op);
			mblist_append(&out_list, scan);
		}
	}
	if (out_list.head != NULL) {
		errno_t error;

		error = bridge_transmit(dst_if, out_list.head);
		if (error != 0) {
			out_errors++;
		}
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "%s (%s) bridge_transmit packets %u bytes %u error %d",
		    bridge_ifp->if_xname,
		    dst_if->if_xname,
		    out_list.count, out_list.bytes, error);
	}
	if (out_list.count != 0 || out_errors != 0) {
		ifnet_stat_increment_out(bridge_ifp, out_list.count,
		    out_list.bytes, out_errors);
	}
	return;
}

/*
 * bridge_member_output:
 *
 *	Send output from a bridge member interface.  This
 *	performs the bridging function for locally originated
 *	packets.
 *
 *	The mbuf has the Ethernet header already attached.
 */
static errno_t
bridge_member_output(struct bridge_softc *sc, ifnet_t ifp, mbuf_t *data)
{
	ifnet_t bridge_ifp;
	struct ether_header *eh;
	struct ifnet *dst_if;
	uint16_t vlan;
	struct bridge_iflist *mac_nat_bif;
	ifnet_t mac_nat_ifp;
	mbuf_t m = *data;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_OUTPUT,
	    "ifp %s", ifp->if_xname);
	if (m->m_len < ETHER_HDR_LEN) {
		m = m_pullup(m, ETHER_HDR_LEN);
		if (m == NULL) {
			*data = NULL;
			return EJUSTRETURN;
		}
	}

	eh = mtod(m, struct ether_header *);
	vlan = VLANTAGOF(m);

	BRIDGE_LOCK(sc);
	mac_nat_bif = sc->sc_mac_nat_bif;
	mac_nat_ifp = (mac_nat_bif != NULL) ? mac_nat_bif->bif_ifp : NULL;
	if (mac_nat_ifp == ifp) {
		/* record the IP address used by the MAC NAT interface */
		(void)bridge_mac_nat_output(sc, mac_nat_bif, data, NULL);
		m = *data;
		if (m == NULL) {
			/* packet was deallocated */
			BRIDGE_UNLOCK(sc);
			return EJUSTRETURN;
		}
	}
	bridge_ifp = sc->sc_ifp;

	/*
	 * APPLE MODIFICATION
	 * If the packet is an 802.1X ethertype, then only send on the
	 * original output interface.
	 */
	if (eh->ether_type == htons(ETHERTYPE_PAE)) {
		dst_if = ifp;
		goto sendunicast;
	}

	/*
	 * If bridge is down, but the original output interface is up,
	 * go ahead and send out that interface.  Otherwise, the packet
	 * is dropped below.
	 */
	if ((bridge_ifp->if_flags & IFF_RUNNING) == 0) {
		dst_if = ifp;
		goto sendunicast;
	}

	/*
	 * If the packet is a multicast, or we don't know a better way to
	 * get there, send to all interfaces.
	 */
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		dst_if = NULL;
	} else {
		dst_if = bridge_rtlookup(sc, eh->ether_dhost, vlan);
	}
	if (dst_if == NULL) {
		struct bridge_iflist *bif;
		struct mbuf *mc;
		errno_t error;


		bridge_span(sc, m);

		BRIDGE_LOCK2REF(sc, error);
		if (error != 0) {
			m_freem(m);
			return EJUSTRETURN;
		}

		/*
		 * Duplicate and send the packet across all member interfaces
		 * except the originating interface.
		 */
		TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
			dst_if = bif->bif_ifp;
			if (dst_if == ifp) {
				/* skip the originating interface */
				continue;
			}
			/* skip interface with inactive link status */
			if ((bif->bif_flags & BIFF_MEDIA_ACTIVE) == 0) {
				continue;
			}

			/* skip interface that isn't running */
			if ((dst_if->if_flags & IFF_RUNNING) == 0) {
				continue;
			}
			/*
			 * If the interface is participating in spanning
			 * tree, make sure the port is in a state that
			 * allows forwarding.
			 */
			if ((bif->bif_ifflags & IFBIF_STP) &&
			    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
				continue;
			}
			/*
			 * If the destination is the MAC NAT interface,
			 * skip sending the packet. The packet can't be sent
			 * if the source MAC is incorrect.
			 */
			if (dst_if == mac_nat_ifp) {
				continue;
			}

			/* make a deep copy to send on this member interface */
			mc = m_dup(m, M_DONTWAIT);
			if (mc == NULL) {
				(void)ifnet_stat_increment_out(bridge_ifp,
				    0, 0, 1);
				continue;
			}
			(void)bridge_enqueue(bridge_ifp, ifp, dst_if,
			    mc, CHECKSUM_OPERATION_COMPUTE);
		}
		BRIDGE_UNREF(sc);

		if ((ifp->if_flags & IFF_RUNNING) == 0) {
			m_freem(m);
			return EJUSTRETURN;
		}
		/* allow packet to continue on the originating interface */
		return 0;
	}

sendunicast:
	/*
	 * XXX Spanning tree consideration here?
	 */

	bridge_span(sc, m);
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		BRIDGE_UNLOCK(sc);
		return EJUSTRETURN;
	}

	BRIDGE_UNLOCK(sc);
	if (dst_if == ifp) {
		/* allow packet to continue on the originating interface */
		return 0;
	}
	if (dst_if != mac_nat_ifp) {
		(void) bridge_enqueue(bridge_ifp, ifp, dst_if, m,
		    CHECKSUM_OPERATION_COMPUTE);
	} else {
		/*
		 * This is not the original output interface
		 * and the destination is the MAC NAT interface.
		 * Drop the packet because the packet can't be sent
		 * if the source MAC is incorrect.
		 */
		m_freem(m);
	}
	return EJUSTRETURN;
}

/*
 * Output callback.
 *
 * This routine is called externally from above only when if_bridge_txstart
 * is disabled; otherwise it is called internally by bridge_start().
 */
static int
bridge_output(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc * __single sc = ifnet_softc(ifp);
	struct ether_header *eh;
	struct ifnet *dst_if = NULL;
	int error = 0;

	eh = mtod(m, struct ether_header *);

	BRIDGE_LOCK(sc);

	if (!IS_BCAST_MCAST(m)) {
		dst_if = bridge_rtlookup(sc, eh->ether_dhost, 0);
	}

	(void) ifnet_stat_increment_out(ifp, 1, m->m_pkthdr.len, 0);

	BRIDGE_BPF_TAP_OUT(ifp, m);

	if (dst_if == NULL) {
		/* callee will unlock */
		bridge_broadcast(sc, NULL, m);
	} else {
		ifnet_t bridge_ifp;

		bridge_ifp = sc->sc_ifp;
		BRIDGE_UNLOCK(sc);

		error = bridge_enqueue(bridge_ifp, NULL, dst_if, m,
		    CHECKSUM_OPERATION_FINALIZE);
	}

	return error;
}

static void
bridge_finalize_cksum(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	bool is_ipv4;
	uint32_t sw_csum, hwcap;
	uint32_t did_sw;
	uint32_t csum_flags;

	eh = mtod(m, struct ether_header *);
	if (!ether_header_type_is_ip(eh, &is_ipv4)) {
		return;
	}

	/* do in software what the hardware cannot */
	hwcap = (ifp->if_hwassist | CSUM_DATA_VALID);
	csum_flags = m->m_pkthdr.csum_flags;
	sw_csum = csum_flags & ~IF_HWASSIST_CSUM_FLAGS(hwcap);
	sw_csum &= IF_HWASSIST_CSUM_MASK;

	if (is_ipv4) {
		if ((hwcap & CSUM_PARTIAL) && !(sw_csum & CSUM_DELAY_DATA) &&
		    (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA)) {
			if (m->m_pkthdr.csum_flags & CSUM_TCP) {
				uint16_t start =
				    sizeof(*eh) + sizeof(struct ip);
				uint16_t ulpoff =
				    m->m_pkthdr.csum_data & 0xffff;
				m->m_pkthdr.csum_flags |=
				    (CSUM_DATA_VALID | CSUM_PARTIAL);
				m->m_pkthdr.csum_tx_stuff = (ulpoff + start);
				m->m_pkthdr.csum_tx_start = start;
			} else {
				sw_csum |= (CSUM_DELAY_DATA &
				    m->m_pkthdr.csum_flags);
			}
		}
		did_sw = in_finalize_cksum(m, sizeof(*eh), sw_csum);
	} else {
		if ((hwcap & CSUM_PARTIAL) &&
		    !(sw_csum & CSUM_DELAY_IPV6_DATA) &&
		    (m->m_pkthdr.csum_flags & CSUM_DELAY_IPV6_DATA)) {
			if (m->m_pkthdr.csum_flags & CSUM_TCPIPV6) {
				uint16_t start =
				    sizeof(*eh) + sizeof(struct ip6_hdr);
				uint16_t ulpoff =
				    m->m_pkthdr.csum_data & 0xffff;
				m->m_pkthdr.csum_flags |=
				    (CSUM_DATA_VALID | CSUM_PARTIAL);
				m->m_pkthdr.csum_tx_stuff = (ulpoff + start);
				m->m_pkthdr.csum_tx_start = start;
			} else {
				sw_csum |= (CSUM_DELAY_IPV6_DATA &
				    m->m_pkthdr.csum_flags);
			}
		}
		did_sw = in6_finalize_cksum(m, sizeof(*eh), -1, -1, sw_csum);
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "[%s] before 0x%x hwcap 0x%x sw_csum 0x%x did 0x%x after 0x%x",
	    ifp->if_xname, csum_flags, hwcap, sw_csum,
	    did_sw, m->m_pkthdr.csum_flags);
}

/*
 * bridge_start:
 *
 *	Start output on a bridge.
 *
 * This routine is invoked by the start worker thread; because we never call
 * it directly, there is no need do deploy any serialization mechanism other
 * than what's already used by the worker thread, i.e. this is already single
 * threaded.
 *
 * This routine is called only when if_bridge_txstart is enabled.
 */
static void
bridge_start(struct ifnet *ifp)
{
	mbuf_ref_t m;

	for (;;) {
		if (ifnet_dequeue(ifp, &m) != 0) {
			break;
		}

		(void) bridge_output(ifp, m);
	}
}

/*
 * bridge_forward:
 *
 *	The forwarding function of the bridge.
 *
 *	NOTE: Releases the lock on return.
 */
static void
bridge_forward(struct bridge_softc *sc, struct bridge_iflist *sbif,
    struct mbuf *m)
{
	struct bridge_iflist *dbif;
	ifnet_t bridge_ifp;
	struct ifnet *src_if, *dst_if;
	struct ether_header *eh;
	uint16_t vlan;
	uint8_t *dst;
	int error;
	struct mac_nat_record mnr;
	bool translate_mac = FALSE;
	uint32_t sc_filter_flags;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	bridge_ifp = sc->sc_ifp;
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_OUTPUT,
	    "%s m 0x%llx", bridge_ifp->if_xname,
	    (uint64_t)VM_KERNEL_ADDRPERM(m));

	src_if = sbif->bif_ifp;
	VERIFY(src_if == m->m_pkthdr.rcvif);

	(void) ifnet_stat_increment_in(bridge_ifp, 1, m->m_pkthdr.len, 0);
	vlan = VLANTAGOF(m);

	eh = mtod(m, struct ether_header *);
	dst = eh->ether_dhost;

	/* If the interface is learning, record the address. */
	if (sbif->bif_ifflags & IFBIF_LEARNING) {
		error = bridge_rtupdate(sc, eh->ether_shost, vlan,
		    sbif, 0, IFBAF_DYNAMIC);
		/*
		 * If the interface has addresses limits then deny any source
		 * that is not in the cache.
		 */
		if (error && sbif->bif_addrmax) {
			goto drop;
		}
	}

	if ((sbif->bif_ifflags & IFBIF_STP) != 0 &&
	    sbif->bif_stp.bp_state == BSTP_IFSTATE_LEARNING) {
		goto drop;
	}

	/*
	 * At this point, the port either doesn't participate
	 * in spanning tree or it is in the forwarding state.
	 */

	/*
	 * If the packet is unicast, destined for someone on
	 * "this" side of the bridge, drop it.
	 */
	if (!IS_BCAST_MCAST(m)) {
		/* unicast */
		dst_if = bridge_rtlookup(sc, dst, vlan);
		if (src_if == dst_if) {
			goto drop;
		}
	} else {
		/* broadcast/multicast */

		/*
		 * Check if it's a reserved multicast address, any address
		 * listed in 802.1D section 7.12.6 may not be forwarded by the
		 * bridge.
		 * This is currently 01-80-C2-00-00-00 to 01-80-C2-00-00-0F
		 */
		if (dst[0] == 0x01 && dst[1] == 0x80 &&
		    dst[2] == 0xc2 && dst[3] == 0x00 &&
		    dst[4] == 0x00 && dst[5] <= 0x0f) {
			goto drop;
		}
		/* ...forward it to all interfaces. */
		os_atomic_inc(&bridge_ifp->if_imcasts, relaxed);
		dst_if = NULL;
	}

	/*
	 * If we have a destination interface which is a member of our bridge,
	 * OR this is a unicast packet, push it through the bpf(4) machinery.
	 * For broadcast or multicast packets, don't bother because it will
	 * be reinjected into ether_input. We do this before we pass the packets
	 * through the pfil(9) framework, as it is possible that pfil(9) will
	 * drop the packet, or possibly modify it, making it difficult to debug
	 * firewall issues on the bridge.
	 */
#if NBPFILTER > 0
	if (eh->ether_type == htons(ETHERTYPE_RSN_PREAUTH) ||
	    dst_if != NULL || !IS_BCAST_MCAST(m)) {
		m->m_pkthdr.rcvif = bridge_ifp;
		BRIDGE_BPF_TAP_IN(bridge_ifp, m);
	}
#endif /* NBPFILTER */

	if (dst_if == NULL) {
		/* bridge_broadcast will unlock */
		bridge_broadcast(sc, sbif, m);
		return;
	}

	/*
	 * Unicast.
	 */
	/*
	 * At this point, we're dealing with a unicast frame
	 * going to a different interface.
	 */
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		goto drop;
	}

	dbif = bridge_lookup_member_if(sc, dst_if);
	if (dbif == NULL) {
		/* Not a member of the bridge (anymore?) */
		goto drop;
	}

	/* Private segments can not talk to each other */
	if (sbif->bif_ifflags & dbif->bif_ifflags & IFBIF_PRIVATE) {
		goto drop;
	}

	if ((dbif->bif_ifflags & IFBIF_STP) &&
	    dbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
		goto drop;
	}
	if (dbif == sc->sc_mac_nat_bif) {
		/* determine how to translate the packet */
		translate_mac
		        = bridge_mac_nat_output(sc, sbif, &m, &mnr);
		if (m == NULL) {
			/* packet was deallocated */
			BRIDGE_UNLOCK(sc);
			return;
		}
	} else if (bif_has_checksum_offload(dbif) &&
	    !bif_has_checksum_offload(sbif)) {
		/*
		 * If the destination interface has checksum offload enabled,
		 * verify the checksum now, unless the source interface also has
		 * checksum offload enabled. The checksum in that case has
		 * already just been computed and verifying it is unnecessary.
		 */
		error = bridge_verify_checksum(&m, &dbif->bif_stats);
		if (error != 0) {
			BRIDGE_UNLOCK(sc);
			if (m != NULL) {
				m_freem(m);
			}
			return;
		}
	}

	sc_filter_flags = sc->sc_filter_flags;

	BRIDGE_UNLOCK(sc);

	if (translate_mac) {
		/* if we need to, translate the MAC address */
		bridge_mac_nat_translate(&m, &mnr, IF_LLADDR(dst_if));
	}

	if (m != NULL && PF_IS_ENABLED && (sc_filter_flags & IFBF_FILT_MEMBER)) {
		if (bridge_pf(&m, dst_if, sc_filter_flags, false) != 0) {
			return;
		}
		if (m == NULL) {
			return;
		}
	}

	/*
	 * We're forwarding an inbound packet in which the checksum must
	 * already have been computed and if required, verified.
	 */
	if (m != NULL) {
		(void) bridge_enqueue(bridge_ifp, src_if, dst_if, m,
		    CHECKSUM_OPERATION_CLEAR_OFFLOAD);
	}
	return;

drop:
	BRIDGE_UNLOCK(sc);
	m_freem(m);
}

static void
prepare_input_packet(ifnet_t ifp, mbuf_t m)
{
	mbuf_pkthdr_setrcvif(m, ifp);
	mbuf_pkthdr_setheader(m, mtod(m, void *));
	/* adjust frame to skip mac-layer header */
	_mbuf_adjust_pkthdr_and_data(m, ETHER_HDR_LEN);
}

static void
inject_input_packet(ifnet_t ifp, mbuf_t m)
{
	prepare_input_packet(ifp, m);
	m->m_flags |= M_PROTO1; /* set to avoid loops */
	dlil_input_packet_list(ifp, m);
	return;
}

static void
inject_input_packet_list(ifnet_t ifp, mbuf_t in_list, bool m_proto1)
{
	for (mbuf_t scan = in_list; scan != NULL; scan = scan->m_nextpkt) {
		/* mark the packets as arriving on the interface */
		BRIDGE_BPF_TAP_IN(ifp, scan);
		if (m_proto1) {
			scan->m_flags |= M_PROTO1; /* set to avoid loops */
		}
		prepare_input_packet(ifp, scan);
	}
	dlil_input_packet_list(ifp, in_list);
	return;
}

static void
adjust_input_packet_list(mbuf_t in_list)
{
	for (mbuf_t scan = in_list; scan != NULL; scan = scan->m_nextpkt) {
		mbuf_pkthdr_setheader(scan, mtod(scan, void *));
		_mbuf_adjust_pkthdr_and_data(scan, ETHER_HDR_LEN);
	}
}

static bool
in_addr_is_ours(struct in_addr ip)
{
	struct in_ifaddr *ia;
	bool             ours = false;

	lck_rw_lock_shared(&in_ifaddr_rwlock);
	TAILQ_FOREACH(ia, INADDR_HASH(ip.s_addr), ia_hash) {
		if (ia->ia_addr.sin_addr.s_addr == ip.s_addr) {
			ours = true;
			break;
		}
	}
	lck_rw_done(&in_ifaddr_rwlock);
	return ours;
}

static bool
in6_addr_is_ours(const struct in6_addr * ip6_p, uint32_t ifscope)
{
	struct in6_addr         dst_ip;
	struct in6_ifaddr       *ia6;
	bool                    ours = false;

	if (in6_embedded_scope && IN6_IS_ADDR_LINKLOCAL(ip6_p)) {
		/* need to embed scope ID for comparison */
		bcopy(ip6_p, &dst_ip, sizeof(dst_ip));
		dst_ip.s6_addr16[1] = htons(ifscope);
		ip6_p = &dst_ip;
	}
	lck_rw_lock_shared(&in6_ifaddr_rwlock);
	TAILQ_FOREACH(ia6, IN6ADDR_HASH(ip6_p), ia6_hash) {
		if (in6_are_addr_equal_scoped(&ia6->ia_addr.sin6_addr, ip6_p,
		    ia6->ia_addr.sin6_scope_id, ifscope)) {
			ours = true;
			break;
		}
	}
	lck_rw_done(&in6_ifaddr_rwlock);
	return ours;
}


static void
bridge_interface_input(ifnet_t bridge_ifp, mbuf_t m, bool has_address)
{
	bool                    do_segment;
	struct ether_header     *eh;
	uint32_t                in_bytes;
	uint32_t                in_count;
	uint32_t                in_errors = 0;
	bool                    is_ipv4;
	u_int                   mac_hlen;
	mblist                  seg;

	/* segment large packets before sending them up */
	in_count = 1;
	in_bytes = mbuf_pkthdr_len(m);
	if (!has_address) {
		/* bridge has no address, so we're not forwarding */
		goto done;
	}
	if (if_bridge_segmentation == 0) {
		goto done;
	}
	if (in_bytes <= (bridge_ifp->if_mtu + ETHER_HDR_LEN)) {
		goto done;
	}
	eh = mtod(m, struct ether_header *);
	if (!ether_header_type_is_ip(eh, &is_ipv4)) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "large non IPv4/IPv6 packet");
		goto done;
	}
	/* no need to segment if the packet will not be forwarded */
	if (is_ipv4) {
		do_segment = (ipforwarding != 0);
	} else {
		do_segment = (ip6_forwarding != 0);
	}
	if (!do_segment) {
		goto done;
	}

	/*
	 * We have a large IPv4/IPv6 TCP packet. Segment it if required.
	 *
	 * If gso_tcp_receive() returns non-NULL packets, the packet(s) are
	 * ready to be passed up. If the destination is a local IP address,
	 * the packet will not be segmented.
	 */
	mac_hlen = sizeof(*eh);
	seg = gso_tcp_receive(bridge_ifp, m, mac_hlen, is_ipv4);
	if (seg.head == NULL) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
		    "gso_tcp returned no packets");
		in_errors++;
		in_bytes = in_count = 0;
		m = NULL;
	} else {
		m = seg.head;
		in_count = seg.count;
		in_bytes = seg.bytes;
	}

done:
	if (m != NULL) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s packets %d bytes %d",
		    bridge_ifp->if_xname, in_count, in_bytes);
		/* Mark the packets as arriving on the bridge interface */
		inject_input_packet_list(bridge_ifp, m, false);
	}
	ifnet_stat_increment_in(bridge_ifp, in_count, in_bytes, in_errors);
	return;
}

static void
bridge_interface_input_list(ifnet_t bridge_ifp, ether_type_flag_t etypef,
    mblist list, bool has_address)
{
	bool            do_segment = false;
	uint32_t        in_errors = 0;
	bool            is_ipv4;
	mblist          in_list;
	u_int           mac_hlen;
	mbuf_t          next_packet;

	if (has_address && if_bridge_segmentation != 0 &&
	    ether_type_flag_is_ip(etypef)) {
		if (etypef == ETHER_TYPE_FLAG_IPV4) {
			is_ipv4 = true;
			do_segment = (ipforwarding != 0);
		} else {
			is_ipv4 = false;
			do_segment = (ip6_forwarding != 0);
		}
	}
	if (!do_segment) {
		in_list = list;
		goto done;
	}

	mblist_init(&in_list);
	mac_hlen = sizeof(struct ether_header);
	for (mbuf_t scan = list.head; scan != NULL; scan = next_packet) {
		uint32_t        len;
		mblist          seg;

		/* take it out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;

		/* check whether it might need segmentation */
		len = mbuf_pkthdr_len(scan);
		if (len <= (bridge_ifp->if_mtu + ETHER_HDR_LEN)) {
			mblist_append(&in_list, scan);
		} else {
			/* segment before sending up */
			seg = gso_tcp_receive(bridge_ifp, scan, mac_hlen,
			    is_ipv4);
			if (seg.head == NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
				    "gso_tcp returned no packets");
				in_errors++;
			} else {
				mblist_append_list(&in_list, seg);
			}
		}
	}

done:
	if (in_list.head != NULL) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s packets %d bytes %d",
		    bridge_ifp->if_xname,
		    in_list.count, in_list.bytes);
		/* Mark the packets as arriving on the bridge interface */
		inject_input_packet_list(bridge_ifp, in_list.head, false);
		ifnet_stat_increment_in(bridge_ifp, in_list.count,
		    in_list.bytes, in_errors);
	}
	return;
}

/*
 * bridge_input:
 *
 *	Filter input from a member interface.  Queue the packet for
 *	bridging if it is not for us.
 */
errno_t
bridge_input(struct ifnet *ifp, mbuf_t *data)
{
	struct bridge_softc * __single sc = ifp->if_bridge;
	struct bridge_iflist *bif, *bif2;
	uint8_t *dhost;
	struct ether_header eh_in;
	bool is_ip = false;
	bool is_ipv4 = false;
	ifnet_t bridge_ifp;
	unsigned int mac_hlen = sizeof(struct ether_header);
	uint16_t vlan;
	errno_t error;
	ip_packet_info info;
	bool is_broadcast;
	bool is_ip_broadcast = false;
	bool is_ifp_mac = false;
	mbuf_t m;
	uint32_t sc_filter_flags;
	bool bridge_has_address;

	if (sc == NULL) {
		return 0;
	}
	m = *data;
	is_broadcast = IS_BCAST_MCAST(m);

	bridge_ifp = sc->sc_ifp;
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
	    "%s %sfrom %s m 0x%llx data 0x%llx",
	    bridge_ifp->if_xname, is_broadcast ? "bcast_mcast " : "",
	    ifp->if_xname,
	    (uint64_t)VM_KERNEL_ADDRPERM(m),
	    (uint64_t)VM_KERNEL_ADDRPERM(mtod(m, void *)));
	if ((bridge_ifp->if_flags & IFF_RUNNING) == 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s not running passing along",
		    bridge_ifp->if_xname);
		return 0;
	}

	vlan = VLANTAGOF(m);

	if (!is_broadcast) {
		/*
		 * Need to clear the promiscuous flag otherwise it will be
		 * dropped by DLIL after processing filters
		 */
		if ((m->m_flags & M_PROMISC) != 0) {
			m->m_flags &= ~M_PROMISC;
		} else {
			is_ifp_mac = true;
		}
	}


	/* copy the ethernet header */
	eh_in = *(mtod(m, struct ether_header *));
	dhost = eh_in.ether_dhost;

	is_ip = ether_header_type_is_ip(&eh_in, &is_ipv4);

	sc_filter_flags = sc->sc_filter_flags;
	if (PF_IS_ENABLED && (sc_filter_flags & IFBF_FILT_MEMBER)) {
		error = bridge_pf(data, ifp, sc_filter_flags, true);
		m = *data;
		if (error != 0 || m == NULL) {
			return EJUSTRETURN;
		}
	}

	BRIDGE_LOCK(sc);
	bif = bridge_lookup_member_if(sc, ifp);
	if (bif == NULL) {
		BRIDGE_UNLOCK(sc);
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s bridge_lookup_member_if failed",
		    bridge_ifp->if_xname);
		return 0;
	}
	if (is_ip && bif_has_checksum_offload(bif)) {
		error = bridge_get_ip_proto(data, mac_hlen, is_ipv4,
		    &info, &bif->bif_stats.brms_in_ip);
		if (error != 0) {
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_CHECKSUM,
			    "%s(%s) bridge_get_ip_proto failed %d",
			    bridge_ifp->if_xname,
			    bif->bif_ifp->if_xname, error);
		} else {
			/* need to compute IP/UDP/TCP/checksums */
			error = bridge_offload_checksum(data, &info,
			    &bif->bif_stats);
			if (error != 0) {
				BRIDGE_LOG(LOG_NOTICE, BR_DBGF_CHECKSUM,
				    "%s(%s) bridge_offload_checksum failed %d",
				    bridge_ifp->if_xname,
				    bif->bif_ifp->if_xname, error);
			}
		}
		if (error != 0) {
			BRIDGE_UNLOCK(sc);
			if (*data != NULL) {
				m_freem(*data);
				*data = NULL;
			}
			return EJUSTRETURN;
		}
		m = *data;
	}

	if (bif->bif_flags & BIFF_HOST_FILTER) {
		error = bridge_host_filter(bif, data);
		if (error != 0) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
			    "%s bridge_host_filter failed",
			    bif->bif_ifp->if_xname);
			BRIDGE_UNLOCK(sc);
			return EJUSTRETURN;
		}
		m = *data;
	}

	/*
	 * A link-layer unicast packet directed at the MAC-NAT interface
	 * could be an IP broadcast packet. An IP broadcast packet needs to
	 * be forwarded like regular link-layer broadcast.
	 */
	if (sc->sc_mac_nat_bif == bif && is_ifp_mac && is_ip) {
		is_ip_broadcast = is_broadcast_ip_packet(data);
		m = *data;
		if (m == NULL) {
			BRIDGE_UNLOCK(sc);
			return EJUSTRETURN;
		}
	}

	bridge_span(sc, m);

	if (is_broadcast || is_ip_broadcast) {
		struct mbuf *mc;

		if (IS_MCAST(m)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MCAST,
			    " multicast: "
			    "%02x:%02x:%02x:%02x:%02x:%02x",
			    dhost[0], dhost[1],
			    dhost[2], dhost[3],
			    dhost[4], dhost[5]);
			/* Tap off 802.1D packets; they do not get forwarded. */
			if (_ether_cmp(dhost, bstp_etheraddr) == 0) {
#if BRIDGESTP
				bstp_input(&bif->bif_stp, m);
				m = NULL;
#else /* !BRIDGESTP */
				m_freem(m);
				*data = m = NULL;
#endif /* !BRIDGESTP */
				if (m == NULL) {
					BRIDGE_UNLOCK(sc);
					return EJUSTRETURN;
				}
			}
		} else if (is_broadcast) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MCAST,
			    " broadcast: "
			    "%02x:%02x:%02x:%02x:%02x:%02x",
			    dhost[0], dhost[1],
			    dhost[2], dhost[3],
			    dhost[4], dhost[5]);
		}

		if ((bif->bif_ifflags & IFBIF_STP) &&
		    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
			BRIDGE_UNLOCK(sc);
			return 0;
		}
		/*
		 * Make a deep copy of the packet and enqueue the copy
		 * for bridge processing.
		 */
		mc = m_dup(m, M_DONTWAIT);
		if (mc == NULL) {
			BRIDGE_UNLOCK(sc);
			return 0;
		}
		/*
		 * Perform the bridge forwarding function with the copy.
		 *
		 * Note that bridge_forward calls BRIDGE_UNLOCK
		 */
		if (is_ip_broadcast) {
			struct ether_header *eh;

			/* make the copy look like it is actually broadcast */
			mc->m_flags |= M_BCAST;
			eh = mtod(mc, struct ether_header *);
			bcopy(etherbroadcastaddr, eh->ether_dhost,
			    ETHER_ADDR_LEN);
		}

		bridge_forward(sc, bif, mc);

		/*
		 * Reinject the mbuf as arriving on the bridge so we have a
		 * chance at claiming multicast packets. We can not loop back
		 * here from ether_input as a bridge is never a member of a
		 * bridge.
		 */
		if (is_broadcast) {
			VERIFY(bridge_ifp->if_bridge == NULL);
			mc = m_dup(m, M_DONTWAIT);
			if (mc != NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MCAST,
				    "%s mcast for us", bridge_ifp->if_xname);
				BRIDGE_BPF_TAP_IN(ifp, mc);
				prepare_input_packet(bridge_ifp, mc);
				ifnet_stat_increment_in(bridge_ifp, 1,
				    mbuf_pkthdr_len(mc), 0);
				dlil_input_packet_list(bridge_ifp, mc);
			}
		}

		/* Return the original packet for local processing. */
		return 0;
	}

	if ((bif->bif_ifflags & IFBIF_STP) &&
	    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
		BRIDGE_UNLOCK(sc);
		return 0;
	}

#define GRAB_OUR_PACKETS(iface)                                         \
	/* It is destined for us. */                                    \
	if (_ether_cmp(IF_LLADDR((iface)), dhost) == 0) {               \
	        BRIDGE_BPF_TAP_IN(iface, m);                            \
	        if (bif->bif_ifflags & IFBIF_LEARNING) {                \
	                error = bridge_rtupdate(sc, eh_in.ether_shost,  \
	                    vlan, bif, 0, IFBAF_DYNAMIC);               \
	                if (error && bif->bif_addrmax) {                \
	                        BRIDGE_UNLOCK(sc);                      \
	                        m_freem(m);                             \
	                        return (EJUSTRETURN);                   \
	                }                                               \
	        }                                                       \
	        BRIDGE_UNLOCK(sc);                                      \
	        inject_input_packet(iface, m);                          \
	        return (EJUSTRETURN);                                   \
	}                                                               \
                                                                        \
	/* We just received a packet that we sent out. */               \
	if (_ether_cmp(IF_LLADDR((iface)), eh_in.ether_shost) == 0) {   \
	        BRIDGE_UNLOCK(sc);                                      \
	        m_freem(m);                                             \
	        return (EJUSTRETURN);                                   \
	}

	/*
	 * Unicast.
	 */

	/* handle MAC-NAT if enabled */
	if (is_ifp_mac && sc->sc_mac_nat_bif == bif) {
		ifnet_ref_t dst_if = NULL;

		m = *data = bridge_mac_nat_input(sc, ifp, m, &dst_if);
		if (m == NULL) {
			/* packet was deallocated */
			BRIDGE_UNLOCK(sc);
			return EJUSTRETURN;
		}
		if (dst_if != NULL) {
			/* need to forward the packet */
			BRIDGE_UNLOCK(sc);
			(void)bridge_enqueue(bridge_ifp, NULL,
			    dst_if, m,
			    CHECKSUM_OPERATION_CLEAR_OFFLOAD);
			return EJUSTRETURN;
		}
	}

	/*
	 * If the destination MAC matches the bridge interface, and
	 * the bridge has an address assigned, or the address is
	 * distinct from the receive interface, claim the packets
	 * for the bridge interface.
	 */
	bridge_has_address = (sc->sc_flags & SCF_ADDRESS_ASSIGNED) != 0;
	if (_ether_cmp(dhost, IF_LLADDR(bridge_ifp)) == 0 &&
	    (bridge_has_address || _ether_cmp(dhost, IF_LLADDR(ifp)) != 0)) {
		/*
		 * If the interface is learning, and the source
		 * address is valid and not multicast, record
		 * the address.
		 */
		if (bif->bif_ifflags & IFBIF_LEARNING) {
			(void) bridge_rtupdate(sc, eh_in.ether_shost,
			    vlan, bif, 0, IFBAF_DYNAMIC);
		}
		BRIDGE_UNLOCK(sc);
		bridge_interface_input(bridge_ifp, m, bridge_has_address);
		return EJUSTRETURN;
	}

	/*
	 * if the destination of the packet is for the MAC address of
	 * the member interface itself, then we don't need to forward
	 * it -- just pass it back.  Note that it'll likely just be
	 * dropped by the stack, but if something else is bound to
	 * the interface directly (for example, the wireless stats
	 * protocol -- although that actually uses BPF right now),
	 * then it will consume the packet
	 *
	 * ALSO, note that we do this check AFTER checking for the
	 * bridge's own MAC address, because the bridge may be
	 * using the SAME MAC address as one of its interfaces
	 */
	if (is_ifp_mac) {
		BRIDGE_UNLOCK(sc);
		return 0;
	}

	/* Now check the remaining bridge members. */
	TAILQ_FOREACH(bif2, &sc->sc_iflist, bif_next) {
		if (bif2->bif_ifp != ifp) {
			GRAB_OUR_PACKETS(bif2->bif_ifp);
		}
	}

#undef GRAB_OUR_PACKETS

	/*
	 * Perform the bridge forwarding function.
	 *
	 * Note that bridge_forward calls BRIDGE_UNLOCK
	 */
	bridge_forward(sc, bif, m);

	return EJUSTRETURN;
}

/*
 * bridge_broadcast:
 *
 *	Send a frame to all interfaces that are members of
 *	the bridge, except for the one on which the packet
 *	arrived.
 *
 *	NOTE: Releases the lock on return.
 */
static void
bridge_broadcast(struct bridge_softc *sc, struct bridge_iflist * sbif,
    struct mbuf *m)
{
	ifnet_t bridge_ifp;
	struct bridge_iflist *dbif;
	struct ifnet * src_if;
	mbuf_ref_t mc;
	struct mbuf *mc_in;
	int error = 0, used = 0;
	ChecksumOperation cksum_op;
	struct mac_nat_record mnr;
	struct bridge_iflist *mac_nat_bif = sc->sc_mac_nat_bif;
	boolean_t translate_mac = FALSE;
	uint32_t sc_filter_flags;
	bool is_bcast_mcast;

	bridge_ifp = sc->sc_ifp;
	if (sbif != NULL) {
		src_if = sbif->bif_ifp;
		cksum_op = CHECKSUM_OPERATION_CLEAR_OFFLOAD;
		if (mac_nat_bif != NULL && sbif != mac_nat_bif) {
			/* get the translation record */
			translate_mac
			        = bridge_mac_nat_output(sc, sbif, &m, &mnr);
			if (m == NULL) {
				/* packet was deallocated */
				BRIDGE_UNLOCK(sc);
				return;
			}
		}
	} else {
		/*
		 * sbif is NULL when the bridge interface calls
		 * bridge_broadcast().
		 */
		cksum_op = CHECKSUM_OPERATION_FINALIZE;
		src_if = NULL;
	}

	BRIDGE_LOCK2REF(sc, error);
	if (error) {
		m_freem(m);
		return;
	}
	is_bcast_mcast = IS_BCAST_MCAST(m);
	sc_filter_flags = sc->sc_filter_flags;
	TAILQ_FOREACH(dbif, &sc->sc_iflist, bif_next) {
		ifnet_t         dst_if;

		dst_if = dbif->bif_ifp;
		if (dst_if == src_if) {
			/* skip the interface that the packet came in on */
			continue;
		}

		/* Private segments can not talk to each other */
		if (sbif != NULL &&
		    (sbif->bif_ifflags & dbif->bif_ifflags & IFBIF_PRIVATE)) {
			continue;
		}

		if ((dbif->bif_ifflags & IFBIF_STP) &&
		    dbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
			continue;
		}

		if ((dbif->bif_ifflags & IFBIF_DISCOVER) == 0 &&
		    !is_bcast_mcast) {
			continue;
		}

		if ((dst_if->if_flags & IFF_RUNNING) == 0) {
			continue;
		}

		if ((dbif->bif_flags & BIFF_MEDIA_ACTIVE) == 0) {
			continue;
		}

		if (TAILQ_NEXT(dbif, bif_next) == NULL) {
			mc = m;
			used = 1;
		} else {
			mc = m_dup(m, M_DONTWAIT);
			if (mc == NULL) {
				(void) ifnet_stat_increment_out(bridge_ifp,
				    0, 0, 1);
				continue;
			}
		}

		/*
		 * If broadcast input is enabled, do so only if this
		 * is an input packet.
		 */
		if (sbif != NULL && is_bcast_mcast &&
		    (dbif->bif_flags & BIFF_INPUT_BROADCAST) != 0) {
			mc_in = m_dup(mc, M_DONTWAIT);
			/* this could fail, but we continue anyways */
		} else {
			mc_in = NULL;
		}

		/* out */
		if (translate_mac && mac_nat_bif == dbif) {
			/* translate the packet */
			bridge_mac_nat_translate(&mc, &mnr, IF_LLADDR(dst_if));
		}

		if (mc != NULL && sbif != NULL &&
		    PF_IS_ENABLED && (sc_filter_flags & IFBF_FILT_MEMBER)) {
			if (used == 0) {
				/* Keep the layer3 header aligned */
				int i = min(mc->m_pkthdr.len, max_protohdr);
				mc = m_copyup(mc, i, ETHER_ALIGN);
				if (mc == NULL) {
					(void) ifnet_stat_increment_out(
						sc->sc_ifp, 0, 0, 1);
					if (mc_in != NULL) {
						m_freem(mc_in);
						mc_in = NULL;
					}
					continue;
				}
			}
			if (bridge_pf(&mc, dst_if, sc_filter_flags, false) != 0) {
				if (mc_in != NULL) {
					m_freem(mc_in);
					mc_in = NULL;
				}
				continue;
			}
			if (mc == NULL) {
				if (mc_in != NULL) {
					m_freem(mc_in);
					mc_in = NULL;
				}
				continue;
			}
		}

		if (mc != NULL) {
			/* verify checksum if necessary */
			if (bif_has_checksum_offload(dbif) && sbif != NULL &&
			    !bif_has_checksum_offload(sbif)) {
				error = bridge_verify_checksum(&mc,
				    &dbif->bif_stats);
				if (error != 0) {
					if (mc != NULL) {
						m_freem(mc);
					}
					mc = NULL;
				}
			}
			if (mc != NULL) {
				(void) bridge_enqueue(bridge_ifp,
				    NULL, dst_if, mc, cksum_op);
			}
		}

		/* in */
		if (mc_in == NULL) {
			continue;
		}
		BRIDGE_BPF_TAP_IN(dst_if, mc_in);
		prepare_input_packet(dst_if, mc_in);
		mc_in->m_flags |= M_PROTO1; /* set to avoid loops */
		dlil_input_packet_list(dst_if, mc_in);
	}
	if (used == 0) {
		m_freem(m);
	}


	BRIDGE_UNREF(sc);
}

static mbuf_t
copy_packet_list(mbuf_t m)
{
	mblist  ret;
	mbuf_t  next_packet;

	mblist_init(&ret);
	for (mbuf_t scan = m; scan != NULL; scan = next_packet) {
		mbuf_t  copy_m;

		/* take it out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;

		/* create a copy and add it to the new list */
		copy_m = m_dup(scan, M_DONTWAIT);
		if (copy_m != NULL) {
			mblist_append(&ret, copy_m);
		}

		/* put it back in the original list */
		scan->m_nextpkt = next_packet;
	}
	return ret.head;
}

/*
 * bridge_broadcast_list:
 *
 *      Broadcast a list of packets to all members except `sbif`.
 *      If `need_copy` is false, consumes `m` before returning.
 *
 *	NOTE: Releases the lock on return.
 */
static void
bridge_broadcast_list(struct bridge_softc *sc, struct bridge_iflist * sbif,
    ether_type_flag_t etypef, mbuf_t m, bool need_copy)
{
	bool                    bridge_has_address;
	ifnet_t                 bridge_ifp;
	struct bridge_iflist *  dbif;
	bool                    is_bcast_mcast;
	errno_t                 error = 0;
	ChecksumOperation       cksum_op;
	struct bridge_iflist *  mac_nat_bif = sc->sc_mac_nat_bif;
	ifnet_t                 mac_nat_if = NULL;
	bool                    need_mac_nat = false;
	mbuf_t                  out_mac_nat = NULL;
	ifnet_t                 src_if;
	uint32_t                sc_filter_flags;
	bool                    used = false;

	bridge_ifp = sc->sc_ifp;
	if (sbif != NULL) {
		src_if = sbif->bif_ifp;
		cksum_op = CHECKSUM_OPERATION_CLEAR_OFFLOAD;

		/*
		 * If MAC-NAT is enabled and we'll be sending the packets
		 * over it, verify that it is up and active before
		 * deciding to make a translated copy.
		 */
		if (mac_nat_bif != NULL && sbif != mac_nat_bif) {
			mac_nat_if = mac_nat_bif->bif_ifp;
			if ((mac_nat_if->if_flags & IFF_RUNNING) != 0 &&
			    (mac_nat_bif->bif_flags & BIFF_MEDIA_ACTIVE) != 0) {
				need_mac_nat = true;
			}
		}
	} else {
		/*
		 * sbif is NULL when the bridge interface calls
		 * bridge_broadcast().
		 */
		cksum_op = CHECKSUM_OPERATION_FINALIZE;
		src_if = NULL;
	}

	/*
	 * Create a translated copy for packets destined to MAC-NAT interface.
	 */
	if (need_mac_nat) {
		out_mac_nat
		        = bridge_mac_nat_copy_and_translate_list(sc, sbif,
		    mac_nat_if, m);
	}
	sc_filter_flags = sc->sc_filter_flags;
	bridge_has_address = (sc->sc_flags & SCF_ADDRESS_ASSIGNED) != 0;
	BRIDGE_LOCK2REF(sc, error);
	if (error) {
		goto done;
	}
	is_bcast_mcast = IS_BCAST_MCAST(m);

	/* make a copy for the bridge interface */
	if (is_bcast_mcast && bridge_has_address) {
		mbuf_t  in_list;

		in_list = copy_packet_list(m);
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MCAST,
		    "%s mcast for us in_m %p",
		    bridge_ifp->if_xname, in_list);
		if (in_list != NULL) {
			inject_input_packet_list(bridge_ifp, in_list, false);
		}
	}

	TAILQ_FOREACH(dbif, &sc->sc_iflist, bif_next) {
		ifnet_t         dst_if;
		mbuf_t          in_m = NULL;
		mbuf_t          out_m = NULL;

		dst_if = dbif->bif_ifp;
		if (dst_if == src_if) {
			/* skip the interface that the packet came in on */
			continue;
		}

		/* Private segments can not talk to each other */
		if (sbif != NULL &&
		    (sbif->bif_ifflags & dbif->bif_ifflags & IFBIF_PRIVATE)) {
			continue;
		}

		if ((dbif->bif_ifflags & IFBIF_STP) &&
		    dbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
			continue;
		}

		if ((dbif->bif_ifflags & IFBIF_DISCOVER) == 0 &&
		    !is_bcast_mcast) {
			continue;
		}

		if ((dst_if->if_flags & IFF_RUNNING) == 0) {
			continue;
		}

		if ((dbif->bif_flags & BIFF_MEDIA_ACTIVE) == 0) {
			continue;
		}
		if (dbif == mac_nat_bif) {
			/* translated copy was created above, use that */
			out_m = out_mac_nat;
			out_mac_nat = NULL;
		} else if (!need_copy && TAILQ_NEXT(dbif, bif_next) == NULL) {
			/* consume `m` */
			out_m = m;
			used = true;
		} else {
			/* needs a copy */
			out_m = copy_packet_list(m);
		}

		if (out_m == NULL) {
			ifnet_stat_increment_out(bridge_ifp, 0, 0, 1);
			continue;
		}
		/*
		 * If broadcast input is enabled, do so only if this
		 * is an input packet.
		 */
		if (sbif != NULL && is_bcast_mcast &&
		    (dbif->bif_flags & BIFF_INPUT_BROADCAST) != 0) {
			in_m = copy_packet_list(m);
			/* this could fail, but we continue anyways */
		} else {
			in_m = NULL;
		}

		if (sbif != NULL &&
		    PF_IS_ENABLED && (sc_filter_flags & IFBF_FILT_MEMBER)) {
			out_m = bridge_pf_list(out_m, dst_if,
			    sc_filter_flags, false);
		}
		if (out_m != NULL) {
			/* verify checksum if necessary */
			if (sbif != NULL &&
			    ether_type_flag_is_ip(etypef) &&
			    bif_has_checksum_offload(dbif) &&
			    !bif_has_checksum_offload(sbif)) {
				out_m = bridge_verify_checksum_list(out_m, dbif);
			}
			if (out_m != NULL) {
				bridge_enqueue_multi(bridge_ifp, src_if, dst_if,
				    etypef, out_m, cksum_op);
			}
		}

		/* in */
		if (in_m != NULL) {
			inject_input_packet_list(dst_if, in_m, true);
		}
	}

	BRIDGE_UNREF(sc);

done:
	if (out_mac_nat != NULL) {
		m_freem_list(out_mac_nat);
	}
	if (!need_copy && !used) {
		m_freem_list(m);
	}
	return;
}

static void
bridge_forward_list(struct bridge_softc *sc, struct bridge_iflist * sbif,
    ifnet_t dst_if, ether_type_flag_t etypef, mbuf_t m)
{
	ifnet_t                 bridge_ifp;
	struct bridge_iflist *  dbif;
	uint32_t                sc_filter_flags;
	ifnet_t                 src_if;

	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		goto drop;
	}
	dbif = bridge_lookup_member_if(sc, dst_if);
	if (dbif == NULL) {
		/* Not a member of the bridge (anymore?) */
		goto drop;
	}

	/* Private segments can not talk to each other */
	if ((sbif->bif_ifflags & dbif->bif_ifflags & IFBIF_PRIVATE) != 0) {
		goto drop;
	}
	if (dbif == sc->sc_mac_nat_bif) {
		/* translate the packets before forwarding them */
		if ((etypef & ETHER_TYPE_FLAG_IP_ARP) != 0) {
			m = bridge_mac_nat_translate_list(sc, sbif, dst_if, m);
		}
	} else if (ether_type_flag_is_ip(etypef) &&
	    bif_has_checksum_offload(dbif) &&
	    !bif_has_checksum_offload(sbif)) {
		/*
		 * If the destination interface has checksum offload enabled,
		 * verify the checksum now, unless the source interface also has
		 * checksum offload enabled. The checksum in that case has
		 * already just been computed and verifying it is unnecessary.
		 */
		m = bridge_verify_checksum_list(m, dbif);
	}
	sc_filter_flags = sc->sc_filter_flags;
	bridge_ifp = sc->sc_ifp;
	src_if = sbif->bif_ifp;
	BRIDGE_UNLOCK(sc);
	if (PF_IS_ENABLED && (sc_filter_flags & IFBF_FILT_MEMBER)) {
		m = bridge_pf_list(m, dst_if, sc_filter_flags, false);
	}

	/*
	 * We're forwarding inbound packets for which the checksums must
	 * already have been computed and if required, verified.
	 */
	if (m != NULL) {
		bridge_enqueue_multi(bridge_ifp, src_if, dst_if, etypef, m,
		    CHECKSUM_OPERATION_CLEAR_OFFLOAD);
	}
	return;

drop:
	BRIDGE_UNLOCK(sc);
	m_freem_list(m);
	return;
}

/*
 * bridge_span:
 *
 *	Duplicate a packet out one or more interfaces that are in span mode,
 *	the original mbuf is unmodified.
 */
static void
bridge_span(struct bridge_softc *sc, struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct ifnet *dst_if;
	struct mbuf *mc;

	if (TAILQ_EMPTY(&sc->sc_spanlist)) {
		return;
	}

	TAILQ_FOREACH(bif, &sc->sc_spanlist, bif_next) {
		dst_if = bif->bif_ifp;

		if ((dst_if->if_flags & IFF_RUNNING) == 0) {
			continue;
		}

		mc = m_copypacket(m, M_DONTWAIT);
		if (mc == NULL) {
			(void) ifnet_stat_increment_out(sc->sc_ifp, 0, 0, 1);
			continue;
		}

		(void) bridge_enqueue(sc->sc_ifp, NULL, dst_if, mc,
		    CHECKSUM_OPERATION_NONE);
	}
}

/*
 * bridge_rtupdate:
 *
 *	Add a bridge routing entry.
 */
static int
bridge_rtupdate(struct bridge_softc *sc, const uint8_t dst[ETHER_ADDR_LEN], uint16_t vlan,
    struct bridge_iflist *bif, int setflags, uint8_t flags)
{
	struct bridge_rtnode *brt;
	int error;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	/* Check the source address is valid and not multicast. */
	if (ETHER_IS_MULTICAST(dst) ||
	    (dst[0] == 0 && dst[1] == 0 && dst[2] == 0 &&
	    dst[3] == 0 && dst[4] == 0 && dst[5] == 0) != 0) {
		return EINVAL;
	}

	/* 802.1p frames map to vlan 1 */
	if (vlan == 0) {
		vlan = 1;
	}

	/*
	 * A route for this destination might already exist.  If so,
	 * update it, otherwise create a new one.
	 */
	if ((brt = bridge_rtnode_lookup(sc, dst, vlan)) == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax) {
			sc->sc_brtexceeded++;
			return ENOSPC;
		}
		/* Check per interface address limits (if enabled) */
		if (bif->bif_addrmax && bif->bif_addrcnt >= bif->bif_addrmax) {
			bif->bif_addrexceeded++;
			return ENOSPC;
		}

		/*
		 * Allocate a new bridge forwarding node, and
		 * initialize the expiration time and Ethernet
		 * address.
		 */
		brt = zalloc_noblock(bridge_rtnode_pool);
		if (brt == NULL) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_RT_TABLE,
			    "zalloc_nolock failed");
			return ENOMEM;
		}
		bzero(brt, sizeof(struct bridge_rtnode));

		if (bif->bif_ifflags & IFBIF_STICKY) {
			brt->brt_flags = IFBAF_STICKY;
		} else {
			brt->brt_flags = IFBAF_DYNAMIC;
		}

		memcpy(brt->brt_addr, dst, ETHER_ADDR_LEN);
		brt->brt_vlan = vlan;

		if ((error = bridge_rtnode_insert(sc, brt)) != 0) {
			zfree(bridge_rtnode_pool, brt);
			return error;
		}
		brt->brt_dst = bif;
		bif->bif_addrcnt++;
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_RT_TABLE,
		    "added %02x:%02x:%02x:%02x:%02x:%02x "
		    "on %s count %u hashsize %u",
		    dst[0], dst[1], dst[2], dst[3], dst[4], dst[5],
		    sc->sc_ifp->if_xname, sc->sc_brtcnt,
		    sc->sc_rthash_size);
	}

	if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC &&
	    brt->brt_dst != bif) {
		brt->brt_dst->bif_addrcnt--;
		brt->brt_dst = bif;
		brt->brt_dst->bif_addrcnt++;
	}

	if ((flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
		unsigned long now;

		now = (unsigned long) net_uptime();
		brt->brt_expire = now + sc->sc_brttimeout;
	}
	if (setflags) {
		brt->brt_flags = flags;
	}

	return 0;
}

/*
 * bridge_rtlookup:
 *
 *	Lookup the destination interface for an address.
 */
static struct ifnet *
bridge_rtlookup(struct bridge_softc *sc, const uint8_t addr[ETHER_ADDR_LEN], uint16_t vlan)
{
	struct bridge_rtnode *brt;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	if ((brt = bridge_rtnode_lookup(sc, addr, vlan)) == NULL) {
		return NULL;
	}

	return brt->brt_ifp;
}

/*
 * bridge_rttrim:
 *
 *	Trim the routine table so that we have a number
 *	of routing entries less than or equal to the
 *	maximum number.
 */
static void
bridge_rttrim(struct bridge_softc *sc)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	/* Make sure we actually need to do this. */
	if (sc->sc_brtcnt <= sc->sc_brtmax) {
		return;
	}

	/* Force an aging cycle; this might trim enough addresses. */
	bridge_rtage(sc);
	if (sc->sc_brtcnt <= sc->sc_brtmax) {
		return;
	}

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			bridge_rtnode_destroy(sc, brt);
			if (sc->sc_brtcnt <= sc->sc_brtmax) {
				return;
			}
		}
	}
}

/*
 * bridge_aging_timer:
 *
 *	Aging periodic timer for the bridge routing table.
 */
static void
bridge_aging_timer(struct bridge_softc *sc)
{
	BRIDGE_LOCK_ASSERT_HELD(sc);

	bridge_rtage(sc);
	if ((sc->sc_ifp->if_flags & IFF_RUNNING) &&
	    (sc->sc_flags & SCF_DETACHING) == 0) {
		sc->sc_aging_timer.bdc_sc = sc;
		sc->sc_aging_timer.bdc_func = bridge_aging_timer;
		sc->sc_aging_timer.bdc_ts.tv_sec = bridge_rtable_prune_period;
		bridge_schedule_delayed_call(&sc->sc_aging_timer);
	}
}

/*
 * bridge_rtage:
 *
 *	Perform an aging cycle.
 */
static void
bridge_rtage(struct bridge_softc *sc)
{
	struct bridge_rtnode *brt, *nbrt;
	unsigned long now;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	now = (unsigned long) net_uptime();

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			if (now >= brt->brt_expire) {
				bridge_rtnode_destroy(sc, brt);
			}
		}
	}
	if (sc->sc_mac_nat_bif != NULL) {
		bridge_mac_nat_age_entries(sc, now);
	}
}

/*
 * bridge_rtflush:
 *
 *	Remove all dynamic addresses from the bridge.
 */
static void
bridge_rtflush(struct bridge_softc *sc, int full)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if (full || (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			bridge_rtnode_destroy(sc, brt);
		}
	}
}

/*
 * bridge_rtdaddr:
 *
 *	Remove an address from the table.
 */
static int
bridge_rtdaddr(struct bridge_softc *sc, const uint8_t addr[ETHER_ADDR_LEN], uint16_t vlan)
{
	struct bridge_rtnode *brt;
	int found = 0;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	/*
	 * If vlan is zero then we want to delete for all vlans so the lookup
	 * may return more than one.
	 */
	while ((brt = bridge_rtnode_lookup(sc, addr, vlan)) != NULL) {
		bridge_rtnode_destroy(sc, brt);
		found = 1;
	}

	return found ? 0 : ENOENT;
}

/*
 * bridge_rtdelete:
 *
 *	Delete routes to a specific member interface.
 */
static void
bridge_rtdelete(struct bridge_softc *sc, struct ifnet *ifp, int full)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if (brt->brt_ifp == ifp && (full ||
		    (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)) {
			bridge_rtnode_destroy(sc, brt);
		}
	}
}

/*
 * bridge_rtable_init:
 *
 *	Initialize the route table for this bridge.
 */
static int
bridge_rtable_init(struct bridge_softc *sc)
{
	u_int32_t i;

	sc->sc_rthash = kalloc_type(struct _bridge_rtnode_list,
	    BRIDGE_RTHASH_SIZE, Z_WAITOK_ZERO_NOFAIL);
	sc->sc_rthash_size = BRIDGE_RTHASH_SIZE;

	for (i = 0; i < sc->sc_rthash_size; i++) {
		LIST_INIT(&sc->sc_rthash[i]);
	}

	sc->sc_rthash_key = RandomULong();

	LIST_INIT(&sc->sc_rtlist);

	return 0;
}

/*
 * bridge_rthash_delayed_resize:
 *
 *	Resize the routing table hash on a delayed thread call.
 */
static void
bridge_rthash_delayed_resize(struct bridge_softc *sc)
{
	u_int32_t new_rthash_size = 0;
	u_int32_t old_rthash_size = 0;
	struct _bridge_rtnode_list *new_rthash = NULL;
	struct _bridge_rtnode_list *old_rthash = NULL;
	u_int32_t i;
	struct bridge_rtnode *brt;
	int error = 0;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	/*
	 * Four entries per hash bucket is our ideal load factor
	 */
	if (sc->sc_brtcnt < sc->sc_rthash_size * 4) {
		goto out;
	}

	/*
	 * Doubling the number of hash buckets may be too simplistic
	 * especially when facing a spike of new entries
	 */
	new_rthash_size = sc->sc_rthash_size * 2;

	sc->sc_flags |= SCF_RESIZING;
	BRIDGE_UNLOCK(sc);

	new_rthash = kalloc_type(struct _bridge_rtnode_list, new_rthash_size,
	    Z_WAITOK | Z_ZERO);

	BRIDGE_LOCK(sc);
	sc->sc_flags &= ~SCF_RESIZING;

	if (new_rthash == NULL) {
		error = ENOMEM;
		goto out;
	}
	if ((sc->sc_flags & SCF_DETACHING)) {
		error = ENODEV;
		goto out;
	}
	/*
	 * Fail safe from here on
	 */
	old_rthash = sc->sc_rthash;
	old_rthash_size = sc->sc_rthash_size;
	sc->sc_rthash = new_rthash;
	sc->sc_rthash_size = new_rthash_size;

	/*
	 * Get a new key to force entries to be shuffled around to reduce
	 * the likelihood they will land in the same buckets
	 */
	sc->sc_rthash_key = RandomULong();

	for (i = 0; i < sc->sc_rthash_size; i++) {
		LIST_INIT(&sc->sc_rthash[i]);
	}

	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
		LIST_REMOVE(brt, brt_hash);
		(void) bridge_rtnode_hash(sc, brt);
	}
out:
	if (error == 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_RT_TABLE,
		    "%s new size %u",
		    sc->sc_ifp->if_xname, sc->sc_rthash_size);
		kfree_type(struct _bridge_rtnode_list, old_rthash_size, old_rthash);
	} else {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_RT_TABLE,
		    "%s failed %d", sc->sc_ifp->if_xname, error);
		kfree_type(struct _bridge_rtnode_list, new_rthash_size, new_rthash);
	}
}

/*
 * Resize the number of hash buckets based on the load factor
 * Currently only grow
 * Failing to resize the hash table is not fatal
 */
static void
bridge_rthash_resize(struct bridge_softc *sc)
{
	BRIDGE_LOCK_ASSERT_HELD(sc);

	if ((sc->sc_flags & SCF_DETACHING) || (sc->sc_flags & SCF_RESIZING)) {
		return;
	}

	/*
	 * Four entries per hash bucket is our ideal load factor
	 */
	if (sc->sc_brtcnt < sc->sc_rthash_size * 4) {
		return;
	}
	/*
	 * Hard limit on the size of the routing hash table
	 */
	if (sc->sc_rthash_size >= bridge_rtable_hash_size_max) {
		return;
	}

	sc->sc_resize_call.bdc_sc = sc;
	sc->sc_resize_call.bdc_func = bridge_rthash_delayed_resize;
	bridge_schedule_delayed_call(&sc->sc_resize_call);
}

/*
 * bridge_rtable_fini:
 *
 *	Deconstruct the route table for this bridge.
 */
static void
bridge_rtable_fini(struct bridge_softc *sc)
{
	KASSERT(sc->sc_brtcnt == 0,
	    ("%s: %d bridge routes referenced", __func__, sc->sc_brtcnt));
	kfree_type_counted_by(struct _bridge_rtnode_list, sc->sc_rthash_size,
	    sc->sc_rthash);
	sc->sc_rthash = NULL;
	sc->sc_rthash_size = 0;
}

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 */
#define mix(a, b, c)                                                    \
do {                                                                    \
	a -= b; a -= c; a ^= (c >> 13);                                 \
	b -= c; b -= a; b ^= (a << 8);                                  \
	c -= a; c -= b; c ^= (b >> 13);                                 \
	a -= b; a -= c; a ^= (c >> 12);                                 \
	b -= c; b -= a; b ^= (a << 16);                                 \
	c -= a; c -= b; c ^= (b >> 5);                                  \
	a -= b; a -= c; a ^= (c >> 3);                                  \
	b -= c; b -= a; b ^= (a << 10);                                 \
	c -= a; c -= b; c ^= (b >> 15);                                 \
} while ( /*CONSTCOND*/ 0)

static __inline uint32_t
bridge_rthash(struct bridge_softc *sc, const uint8_t addr[ETHER_ADDR_LEN])
{
	uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = sc->sc_rthash_key;

	b += addr[5] << 8;
	b += addr[4];
	a += addr[3] << 24;
	a += addr[2] << 16;
	a += addr[1] << 8;
	a += addr[0];

	mix(a, b, c);

	return c & BRIDGE_RTHASH_MASK(sc);
}

#undef mix

static int
bridge_rtnode_addr_cmp(const uint8_t a[ETHER_ADDR_LEN], const uint8_t b[ETHER_ADDR_LEN])
{
	int i, d;

	for (i = 0, d = 0; i < ETHER_ADDR_LEN && d == 0; i++) {
		d = ((int)a[i]) - ((int)b[i]);
	}

	return d;
}

/*
 * bridge_rtnode_lookup:
 *
 *	Look up a bridge route node for the specified destination. Compare the
 *	vlan id or if zero then just return the first match.
 */
static struct bridge_rtnode *
bridge_rtnode_lookup(struct bridge_softc *sc, const uint8_t addr[ETHER_ADDR_LEN],
    uint16_t vlan)
{
	struct bridge_rtnode *brt;
	uint32_t hash;
	int dir;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	hash = bridge_rthash(sc, addr);
	LIST_FOREACH(brt, &sc->sc_rthash[hash], brt_hash) {
		dir = bridge_rtnode_addr_cmp(addr, brt->brt_addr);
		if (dir == 0 && (brt->brt_vlan == vlan || vlan == 0)) {
			return brt;
		}
		if (dir > 0) {
			return NULL;
		}
	}

	return NULL;
}

/*
 * bridge_rtnode_hash:
 *
 *	Insert the specified bridge node into the route hash table.
 *	This is used when adding a new node or to rehash when resizing
 *	the hash table
 */
static int
bridge_rtnode_hash(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	struct bridge_rtnode *lbrt;
	uint32_t hash;
	int dir;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	hash = bridge_rthash(sc, brt->brt_addr);

	lbrt = LIST_FIRST(&sc->sc_rthash[hash]);
	if (lbrt == NULL) {
		LIST_INSERT_HEAD(&sc->sc_rthash[hash], brt, brt_hash);
		goto out;
	}

	do {
		dir = bridge_rtnode_addr_cmp(brt->brt_addr, lbrt->brt_addr);
		if (dir == 0 && brt->brt_vlan == lbrt->brt_vlan) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_RT_TABLE,
			    "%s EEXIST %02x:%02x:%02x:%02x:%02x:%02x",
			    sc->sc_ifp->if_xname,
			    brt->brt_addr[0], brt->brt_addr[1],
			    brt->brt_addr[2], brt->brt_addr[3],
			    brt->brt_addr[4], brt->brt_addr[5]);
			return EEXIST;
		}
		if (dir > 0) {
			LIST_INSERT_BEFORE(lbrt, brt, brt_hash);
			goto out;
		}
		if (LIST_NEXT(lbrt, brt_hash) == NULL) {
			LIST_INSERT_AFTER(lbrt, brt, brt_hash);
			goto out;
		}
		lbrt = LIST_NEXT(lbrt, brt_hash);
	} while (lbrt != NULL);

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_RT_TABLE,
	    "%s impossible %02x:%02x:%02x:%02x:%02x:%02x",
	    sc->sc_ifp->if_xname,
	    brt->brt_addr[0], brt->brt_addr[1], brt->brt_addr[2],
	    brt->brt_addr[3], brt->brt_addr[4], brt->brt_addr[5]);
out:
	return 0;
}

/*
 * bridge_rtnode_insert:
 *
 *	Insert the specified bridge node into the route table.  We
 *	assume the entry is not already in the table.
 */
static int
bridge_rtnode_insert(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	int error;

	error = bridge_rtnode_hash(sc, brt);
	if (error != 0) {
		return error;
	}

	LIST_INSERT_HEAD(&sc->sc_rtlist, brt, brt_list);
	sc->sc_brtcnt++;

	bridge_rthash_resize(sc);

	return 0;
}

/*
 * bridge_rtnode_destroy:
 *
 *	Destroy a bridge rtnode.
 */
static void
bridge_rtnode_destroy(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	BRIDGE_LOCK_ASSERT_HELD(sc);

	LIST_REMOVE(brt, brt_hash);

	LIST_REMOVE(brt, brt_list);
	sc->sc_brtcnt--;
	brt->brt_dst->bif_addrcnt--;
	zfree(bridge_rtnode_pool, brt);
}

#if BRIDGESTP
/*
 * bridge_rtable_expire:
 *
 *	Set the expiry time for all routes on an interface.
 */
static void
bridge_rtable_expire(struct ifnet *ifp, int age)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct bridge_rtnode *brt;

	BRIDGE_LOCK(sc);

	/*
	 * If the age is zero then flush, otherwise set all the expiry times to
	 * age for the interface
	 */
	if (age == 0) {
		bridge_rtdelete(sc, ifp, IFBF_FLUSHDYN);
	} else {
		unsigned long now;

		now = (unsigned long) net_uptime();

		LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
			/* Cap the expiry time to 'age' */
			if (brt->brt_ifp == ifp &&
			    brt->brt_expire > now + age &&
			    (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
				brt->brt_expire = now + age;
			}
		}
	}
	BRIDGE_UNLOCK(sc);
}

/*
 * bridge_state_change:
 *
 *	Callback from the bridgestp code when a port changes states.
 */
static void
bridge_state_change(struct ifnet *ifp, int state)
{
	struct bridge_softc *sc = ifp->if_bridge;
	static const char *stpstates[] = {
		"disabled",
		"listening",
		"learning",
		"forwarding",
		"blocking",
		"discarding"
	};

	if (log_stp) {
		log(LOG_NOTICE, "%s: state changed to %s on %s",
		    sc->sc_ifp->if_xname,
		    stpstates[state], ifp->if_xname);
	}
}
#endif /* BRIDGESTP */

/*
 * bridge_detach:
 *
 *	Callback when interface has been detached.
 */
static void
bridge_detach(ifnet_t ifp)
{
	struct bridge_softc *sc = (struct bridge_softc *)ifnet_softc(ifp);

#if BRIDGESTP
	bstp_detach(&sc->sc_stp);
#endif /* BRIDGESTP */

	/* Tear down the routing table. */
	bridge_rtable_fini(sc);

	lck_mtx_lock(&bridge_list_mtx);
	LIST_REMOVE(sc, sc_list);
	lck_mtx_unlock(&bridge_list_mtx);

	ifnet_release(ifp);

	lck_mtx_destroy(&sc->sc_mtx, &bridge_lock_grp);
	kfree_type(struct bridge_softc, sc);
}

/*
 * bridge_link_event:
 *
 *	Report a data link event on an interface
 */
static void
bridge_link_event(struct ifnet *ifp, u_int32_t event_code)
{
	struct event {
		u_int32_t ifnet_family;
		u_int32_t unit;
		char if_name[IFNAMSIZ];
	};
	_Alignas(struct kern_event_msg) char message[sizeof(struct kern_event_msg) + sizeof(struct event)] = { 0 };
	struct kern_event_msg *header = (struct kern_event_msg*)message;
	struct event *data = (struct event *)(message + KEV_MSG_HEADER_SIZE);

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_LIFECYCLE,
	    "%s event_code %u - %s", ifp->if_xname,
	    event_code, dlil_kev_dl_code_str(event_code));
	header->total_size   = sizeof(message);
	header->vendor_code  = KEV_VENDOR_APPLE;
	header->kev_class    = KEV_NETWORK_CLASS;
	header->kev_subclass = KEV_DL_SUBCLASS;
	header->event_code   = event_code;
	data->ifnet_family   = ifnet_family(ifp);
	data->unit           = (u_int32_t)ifnet_unit(ifp);
	strlcpy(data->if_name, ifnet_name(ifp), IFNAMSIZ);
	ifnet_event(ifp, header);
}

#define BRIDGE_HF_DROP(reason, func, line) {                            \
	        bridge_hostfilter_stats.reason++;                       \
	        BRIDGE_LOG(LOG_DEBUG, BR_DBGF_HOSTFILTER,               \
	                   "%s.%d" #reason, func, line);                \
	        error = EINVAL;                                         \
	}

/*
 * Make sure this is a DHCP or Bootp request that match the host filter
 */
static int
bridge_dhcp_filter(struct bridge_iflist *bif, struct mbuf *m, size_t offset)
{
	int error = EINVAL;
	struct dhcp dhcp;

	/*
	 * Note: We use the dhcp structure because bootp structure definition
	 * is larger and some vendors do not pad the request
	 */
	error = mbuf_copydata(m, offset, sizeof(struct dhcp), &dhcp);
	if (error != 0) {
		BRIDGE_HF_DROP(brhf_dhcp_too_small, __func__, __LINE__);
		goto done;
	}
	if (dhcp.dp_op != BOOTREQUEST) {
		BRIDGE_HF_DROP(brhf_dhcp_bad_op, __func__, __LINE__);
		goto done;
	}
	/*
	 * The hardware address must be an exact match
	 */
	if (dhcp.dp_htype != ARPHRD_ETHER) {
		BRIDGE_HF_DROP(brhf_dhcp_bad_htype, __func__, __LINE__);
		goto done;
	}
	if (dhcp.dp_hlen != ETHER_ADDR_LEN) {
		BRIDGE_HF_DROP(brhf_dhcp_bad_hlen, __func__, __LINE__);
		goto done;
	}
	if (bcmp(dhcp.dp_chaddr, bif->bif_hf_hwsrc,
	    ETHER_ADDR_LEN) != 0) {
		BRIDGE_HF_DROP(brhf_dhcp_bad_chaddr, __func__, __LINE__);
		goto done;
	}
	/*
	 * Client address must match the host address or be not specified
	 */
	if (dhcp.dp_ciaddr.s_addr != bif->bif_hf_ipsrc.s_addr &&
	    dhcp.dp_ciaddr.s_addr != INADDR_ANY) {
		BRIDGE_HF_DROP(brhf_dhcp_bad_ciaddr, __func__, __LINE__);
		goto done;
	}
	error = 0;
done:
	return error;
}

static int
bridge_host_filter_arp(struct bridge_iflist *bif, mbuf_t *data)
{
	struct ether_arp *ea;
	struct ether_header *eh;
	int error = EINVAL;
	mbuf_t m = *data;
	size_t minlen = sizeof(struct ether_header) + sizeof(struct ether_arp);

	/*
	 * Make the Ethernet and ARP headers contiguous
	 */
	if (mbuf_pkthdr_len(m) < minlen) {
		BRIDGE_HF_DROP(brhf_arp_too_small, __func__, __LINE__);
		goto done;
	}
	if (mbuf_len(m) < minlen && mbuf_pullup(data, minlen) != 0) {
		BRIDGE_HF_DROP(brhf_arp_pullup_failed,
		    __func__, __LINE__);
		goto done;
	}
	m = *data;

	/*
	 * Restrict Ethernet protocols to ARP and IP/IPv6
	 */
	eh = mtod(m, struct ether_header *);
	ea = (struct ether_arp *)(eh + 1);
	if (ea->arp_hrd != HTONS_ARPHRD_ETHER) {
		BRIDGE_HF_DROP(brhf_arp_bad_hw_type,
		    __func__, __LINE__);
		goto done;
	}
	if (ea->arp_pro != HTONS_ETHERTYPE_IP) {
		BRIDGE_HF_DROP(brhf_arp_bad_pro_type,
		    __func__, __LINE__);
		goto done;
	}
	/*
	 * Verify the address lengths are correct
	 */
	if (ea->arp_hln != ETHER_ADDR_LEN) {
		BRIDGE_HF_DROP(brhf_arp_bad_hw_len, __func__, __LINE__);
		goto done;
	}
	if (ea->arp_pln != sizeof(struct in_addr)) {
		BRIDGE_HF_DROP(brhf_arp_bad_pro_len,
		    __func__, __LINE__);
		goto done;
	}
	/*
	 * Allow only ARP request or ARP reply
	 */
	if (ea->arp_op != HTONS_ARPOP_REQUEST &&
	    ea->arp_op != HTONS_ARPOP_REPLY) {
		BRIDGE_HF_DROP(brhf_arp_bad_op, __func__, __LINE__);
		goto done;
	}
	if ((bif->bif_flags & BIFF_HF_HWSRC) != 0) {
		/*
		 * Verify source hardware address matches
		 */
		if (bcmp(ea->arp_sha, bif->bif_hf_hwsrc,
		    ETHER_ADDR_LEN) != 0) {
			BRIDGE_HF_DROP(brhf_arp_bad_sha, __func__, __LINE__);
			goto done;
		}
	}
	if ((bif->bif_flags & BIFF_HF_IPSRC) != 0) {
		/*
		 * Verify source protocol address:
		 * May be null for an ARP probe
		 */
		if (bcmp(ea->arp_spa, &bif->bif_hf_ipsrc.s_addr,
		    sizeof(struct in_addr)) != 0 &&
		    bcmp(ea->arp_spa, &inaddr_any,
		    sizeof(struct in_addr)) != 0) {
			BRIDGE_HF_DROP(brhf_arp_bad_spa, __func__, __LINE__);
			goto done;
		}
	}
	bridge_hostfilter_stats.brhf_arp_ok += 1;
	error = 0;
done:
	return error;
}

static int
bridge_host_filter(struct bridge_iflist *bif, mbuf_t *data)
{
	int error = EINVAL;
	struct ether_header *eh;
	mbuf_t m = *data;

	eh = mtod(m, struct ether_header *);

	/*
	 * Restrict the source hardware address
	 */
	if ((bif->bif_flags & BIFF_HF_HWSRC) != 0 &&
	    bcmp(eh->ether_shost, bif->bif_hf_hwsrc,
	    ETHER_ADDR_LEN) != 0) {
		BRIDGE_HF_DROP(brhf_bad_ether_srchw_addr, __func__, __LINE__);
		goto done;
	}

	/*
	 * Restrict Ethernet protocols to ARP and IP/IPv6
	 */
	if (eh->ether_type == htons(ETHERTYPE_ARP)) {
		error = bridge_host_filter_arp(bif, data);
		if (error != 0) {
			goto done;
		}
	} else if (eh->ether_type == htons(ETHERTYPE_IP)) {
		size_t minlen = sizeof(struct ether_header) + sizeof(struct ip);
		struct ip iphdr;
		size_t offset;

		/*
		 * Make the Ethernet and IP headers contiguous
		 */
		if (mbuf_pkthdr_len(m) < minlen) {
			BRIDGE_HF_DROP(brhf_ip_too_small, __func__, __LINE__);
			goto done;
		}
		offset = sizeof(struct ether_header);
		error = mbuf_copydata(m, offset, sizeof(struct ip), &iphdr);
		if (error != 0) {
			BRIDGE_HF_DROP(brhf_ip_too_small, __func__, __LINE__);
			goto done;
		}
		if ((bif->bif_flags & BIFF_HF_IPSRC) != 0) {
			/*
			 * Verify the source IP address
			 */
			if (iphdr.ip_p == IPPROTO_UDP) {
				struct udphdr udp;

				minlen += sizeof(struct udphdr);
				if (mbuf_pkthdr_len(m) < minlen) {
					BRIDGE_HF_DROP(brhf_ip_too_small,
					    __func__, __LINE__);
					goto done;
				}

				/*
				 * Allow all zero addresses for DHCP requests
				 */
				if (iphdr.ip_src.s_addr != bif->bif_hf_ipsrc.s_addr &&
				    iphdr.ip_src.s_addr != INADDR_ANY) {
					BRIDGE_HF_DROP(brhf_ip_bad_srcaddr,
					    __func__, __LINE__);
					goto done;
				}
				offset = sizeof(struct ether_header) +
				    (IP_VHL_HL(iphdr.ip_vhl) << 2);
				error = mbuf_copydata(m, offset,
				    sizeof(struct udphdr), &udp);
				if (error != 0) {
					BRIDGE_HF_DROP(brhf_ip_too_small,
					    __func__, __LINE__);
					goto done;
				}
				/*
				 * Either it's a Bootp/DHCP packet that we like or
				 * it's a UDP packet from the host IP as source address
				 */
				if (udp.uh_sport == htons(IPPORT_BOOTPC) &&
				    udp.uh_dport == htons(IPPORT_BOOTPS)) {
					minlen += sizeof(struct dhcp);
					if (mbuf_pkthdr_len(m) < minlen) {
						BRIDGE_HF_DROP(brhf_ip_too_small,
						    __func__, __LINE__);
						goto done;
					}
					offset += sizeof(struct udphdr);
					error = bridge_dhcp_filter(bif, m, offset);
					if (error != 0) {
						goto done;
					}
				} else if (iphdr.ip_src.s_addr == INADDR_ANY) {
					BRIDGE_HF_DROP(brhf_ip_bad_srcaddr,
					    __func__, __LINE__);
					goto done;
				}
			} else if (iphdr.ip_src.s_addr != bif->bif_hf_ipsrc.s_addr) {
				assert(bif->bif_hf_ipsrc.s_addr != INADDR_ANY);
				BRIDGE_HF_DROP(brhf_ip_bad_srcaddr, __func__, __LINE__);
				goto done;
			}
		}
		/*
		 * Allow only boring IP protocols
		 */
		if (iphdr.ip_p != IPPROTO_TCP &&
		    iphdr.ip_p != IPPROTO_UDP &&
		    iphdr.ip_p != IPPROTO_ICMP &&
		    iphdr.ip_p != IPPROTO_IGMP) {
			BRIDGE_HF_DROP(brhf_ip_bad_proto, __func__, __LINE__);
			goto done;
		}
		bridge_hostfilter_stats.brhf_ip_ok += 1;
		error = 0;
	} else if (eh->ether_type == htons(ETHERTYPE_IPV6)) {
		size_t minlen = sizeof(struct ether_header) + sizeof(struct ip6_hdr);
		struct ip6_hdr ip6hdr;
		size_t offset;

		/*
		 * Make the Ethernet and IP headers contiguous
		 */
		if (mbuf_pkthdr_len(m) < minlen) {
			BRIDGE_HF_DROP(brhf_ip_too_small, __func__, __LINE__);
			goto done;
		}
		offset = sizeof(struct ether_header);
		error = mbuf_copydata(m, offset, sizeof(struct ip6_hdr), &ip6hdr);
		if (error != 0) {
			BRIDGE_HF_DROP(brhf_ip_too_small, __func__, __LINE__);
			goto done;
		}
		/*
		 * Allow only boring IPv6 protocols
		 */
		if (ip6hdr.ip6_nxt != IPPROTO_TCP &&
		    ip6hdr.ip6_nxt != IPPROTO_UDP &&
		    ip6hdr.ip6_nxt != IPPROTO_ICMPV6) {
			BRIDGE_HF_DROP(brhf_ip_bad_proto, __func__, __LINE__);
			goto done;
		}
		bridge_hostfilter_stats.brhf_ip_ok += 1;
		error = 0;
	} else {
		BRIDGE_HF_DROP(brhf_bad_ether_type, __func__, __LINE__);
		goto done;
	}
done:
	if (error != 0) {
		if (BRIDGE_DBGF_ENABLED(BR_DBGF_HOSTFILTER)) {
			if (m) {
				brlog_mbuf_data(m, 0,
				    sizeof(struct ether_header) +
				    sizeof(struct ip));
			}
		}

		if (m != NULL) {
			m_freem(m);
			*data = NULL;
		}
	}
	return error;
}

/*
 * MAC NAT
 */

static errno_t
bridge_mac_nat_enable(struct bridge_softc *sc, struct bridge_iflist *bif)
{
	errno_t         error = 0;

	BRIDGE_LOCK_ASSERT_HELD(sc);

	if (IFNET_IS_VMNET(bif->bif_ifp)) {
		error = EINVAL;
		goto done;
	}
	if (sc->sc_mac_nat_bif != NULL) {
		if (sc->sc_mac_nat_bif != bif) {
			error = EBUSY;
		}
		goto done;
	}
	sc->sc_mac_nat_bif = bif;
	bif->bif_ifflags |= IFBIF_MAC_NAT;
	bridge_mac_nat_populate_entries(sc);

done:
	return error;
}

static void
bridge_mac_nat_disable(struct bridge_softc *sc)
{
	struct bridge_iflist *mac_nat_bif = sc->sc_mac_nat_bif;

	assert(mac_nat_bif != NULL);
	bridge_mac_nat_flush_entries(sc, mac_nat_bif);
	mac_nat_bif->bif_ifflags &= ~IFBIF_MAC_NAT;
	sc->sc_mac_nat_bif = NULL;
	return;
}

static void
mac_nat_entry_print2(struct mac_nat_entry *mne,
    const char ifname[IFNAMSIZ], const char *msg1, const char *msg2)
{
	int             af;
	char            etopbuf[24];
	char            ntopbuf[MAX_IPv6_STR_LEN];
	const char      *space;

	af = ((mne->mne_flags & MNE_FLAGS_IPV6) != 0) ? AF_INET6 : AF_INET;
	ether_ntop(etopbuf, sizeof(etopbuf), mne->mne_mac);
	(void)inet_ntop(af, &mne->mne_u, ntopbuf, sizeof(ntopbuf));
	if (msg2 == NULL) {
		msg2 = "";
		space = "";
	} else {
		space = " ";
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
	    "%.*s %s%s%s %p (%s, %s, %s)", IFNAMSIZ, ifname, msg1, space, msg2, mne,
	    mne->mne_bif->bif_ifp->if_xname, ntopbuf, etopbuf);
}

static void
mac_nat_entry_print(struct mac_nat_entry *mne,
    const char ifname[IFNAMSIZ], const char *msg)
{
	mac_nat_entry_print2(mne, ifname, msg, NULL);
}

static struct mac_nat_entry *
bridge_lookup_mac_nat_entry_ipv4(const struct bridge_softc *sc, const struct in_addr *ip)
{
	struct mac_nat_entry    *mne;
	struct mac_nat_entry    *ret_mne = NULL;

	LIST_FOREACH(mne, &sc->sc_mne_list, mne_list) {
		if (mne->mne_ip.s_addr == ip->s_addr) {
			if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
				mac_nat_entry_print(mne, sc->sc_if_xname,
				    "found");
			}
			ret_mne = mne;
			break;
		}
	}

	return ret_mne;
}

static struct mac_nat_entry *
bridge_lookup_mac_nat_entry_ipv6(const struct bridge_softc *sc, const struct in6_addr *ip6)
{
	struct mac_nat_entry    *mne;
	struct mac_nat_entry    *ret_mne = NULL;

	LIST_FOREACH(mne, &sc->sc_mne_list_v6, mne_list) {
		if (IN6_ARE_ADDR_EQUAL(&mne->mne_ip6, ip6)) {
			if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
				mac_nat_entry_print(mne, sc->sc_if_xname,
				    "found");
			}
			ret_mne = mne;
			break;
		}
	}

	return ret_mne;
}

static void
bridge_destroy_mac_nat_entry(struct bridge_softc *sc,
    struct mac_nat_entry *mne, const char *reason)
{
	LIST_REMOVE(mne, mne_list);
	if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
		mac_nat_entry_print(mne, sc->sc_if_xname, reason);
	}
	zfree(bridge_mne_pool, mne);
	sc->sc_mne_count--;
}

static struct mac_nat_entry *
bridge_create_mac_nat_entry_common(struct bridge_softc *sc,
    struct bridge_iflist *bif, const char eaddr[ETHER_ADDR_LEN])
{
	struct mac_nat_entry *mne;

	if (sc->sc_mne_count >= sc->sc_mne_max) {
		sc->sc_mne_allocation_failures++;
		return NULL;
	}

	mne = zalloc_noblock(bridge_mne_pool);
	if (mne == NULL) {
		sc->sc_mne_allocation_failures++;
		return NULL;
	}

	sc->sc_mne_count++;
	bzero(mne, sizeof(*mne));
	bcopy(eaddr, mne->mne_mac, sizeof(mne->mne_mac));

	mne->mne_bif = bif;
	mne->mne_expire = (unsigned long)net_uptime() + sc->sc_brttimeout;

	if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
		mac_nat_entry_print(mne, sc->sc_if_xname, "created");
	}

	return mne;
}

static struct mac_nat_entry *
bridge_create_mac_nat_entry_ipv4(struct bridge_softc *sc,
    struct bridge_iflist *bif, const struct in_addr *ip, const char eaddr[ETHER_ADDR_LEN])
{
	struct mac_nat_entry *mne;

	mne = bridge_create_mac_nat_entry_common(sc, bif, eaddr);
	if (mne == NULL) {
		return NULL;
	}

	bcopy(ip, &mne->mne_ip, sizeof(mne->mne_ip));
	LIST_INSERT_HEAD(&sc->sc_mne_list, mne, mne_list);

	return mne;
}

static struct mac_nat_entry *
bridge_create_mac_nat_entry_ipv6(struct bridge_softc *sc,
    struct bridge_iflist *bif, const struct in6_addr *ip6, const char eaddr[ETHER_ADDR_LEN])
{
	struct mac_nat_entry *mne;

	mne = bridge_create_mac_nat_entry_common(sc, bif, eaddr);
	if (mne == NULL) {
		return NULL;
	}

	bcopy(ip6, &mne->mne_ip6, sizeof(mne->mne_ip6));
	mne->mne_flags |= MNE_FLAGS_IPV6;
	LIST_INSERT_HEAD(&sc->sc_mne_list_v6, mne, mne_list);

	return mne;
}

static struct mac_nat_entry *
bridge_update_mac_nat_entry_common(struct bridge_softc *sc, struct bridge_iflist *bif,
    struct mac_nat_entry *mne, const char eaddr[ETHER_ADDR_LEN])
{
	struct bridge_iflist *mac_nat_bif = sc->sc_mac_nat_bif;

	if (mne->mne_bif == mac_nat_bif) {
		/* the MAC NAT interface takes precedence */
		if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
			if (mne->mne_bif != bif) {
				mac_nat_entry_print2(mne,
				    sc->sc_if_xname, "reject",
				    bif->bif_ifp->if_xname);
			}
		}
	} else if (mne->mne_bif != bif) {
		const char *__null_terminated old_if = mne->mne_bif->bif_ifp->if_xname;

		mne->mne_bif = bif;
		if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
			mac_nat_entry_print2(mne,
			    sc->sc_if_xname, "replaced",
			    old_if);
		}
		bcopy(eaddr, mne->mne_mac, sizeof(mne->mne_mac));
	}

	mne->mne_expire = (unsigned long)net_uptime() + sc->sc_brttimeout;

	return mne;
}

static struct mac_nat_entry *
bridge_update_mac_nat_entry_ipv4(struct bridge_softc *sc,
    struct bridge_iflist *bif, struct in_addr *ip, const char eaddr[ETHER_ADDR_LEN])
{
	struct mac_nat_entry *mne;

	mne = bridge_lookup_mac_nat_entry_ipv4(sc, ip);
	if (mne != NULL) {
		return bridge_update_mac_nat_entry_common(sc, bif, mne, eaddr);
	}

	mne = bridge_create_mac_nat_entry_ipv4(sc, bif, ip, eaddr);
	return mne;
}

static struct mac_nat_entry *
bridge_update_mac_nat_entry_ipv6(struct bridge_softc *sc,
    struct bridge_iflist *bif, struct in6_addr *ip6, const char eaddr[ETHER_ADDR_LEN])
{
	struct mac_nat_entry *mne;

	mne = bridge_lookup_mac_nat_entry_ipv6(sc, ip6);
	if (mne != NULL) {
		return bridge_update_mac_nat_entry_common(sc, bif, mne, eaddr);
	}

	mne = bridge_create_mac_nat_entry_ipv6(sc, bif, ip6, eaddr);
	return mne;
}

static void
bridge_mac_nat_flush_entries_common(struct bridge_softc *sc,
    struct mac_nat_entry_list *list, struct bridge_iflist *bif)
{
	struct mac_nat_entry *mne;
	struct mac_nat_entry *tmne;

	LIST_FOREACH_SAFE(mne, list, mne_list, tmne) {
		if (bif != NULL && mne->mne_bif != bif) {
			continue;
		}
		bridge_destroy_mac_nat_entry(sc, mne, "flushed");
	}
}

/*
 * bridge_mac_nat_flush_entries:
 *
 * Flush MAC NAT entries for the specified member. Flush all entries if
 * the member is the one that requires MAC NAT, otherwise just flush the
 * ones for the specified member.
 */
static void
bridge_mac_nat_flush_entries(struct bridge_softc *sc, struct bridge_iflist * bif)
{
	struct bridge_iflist *flush_bif;

	flush_bif = (bif == sc->sc_mac_nat_bif) ? NULL : bif;
	bridge_mac_nat_flush_entries_common(sc, &sc->sc_mne_list, flush_bif);
	bridge_mac_nat_flush_entries_common(sc, &sc->sc_mne_list_v6, flush_bif);
}

static void
bridge_mac_nat_populate_entries(struct bridge_softc *sc)
{
	errno_t                 error;
	ifnet_t                 ifp;
	uint16_t                addresses_count = 0;
	ifaddr_t                * __counted_by(addresses_count) list;
	struct bridge_iflist    *mac_nat_bif = sc->sc_mac_nat_bif;

	assert(mac_nat_bif != NULL);
	ifp = mac_nat_bif->bif_ifp;
	error = ifnet_get_address_list_family_with_count(ifp, &list, &addresses_count, 0);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "ifnet_get_address_list(%s) failed %d",
		    ifp->if_xname, error);
		return;
	}

	for (uint16_t i = 0; i < addresses_count; ++i) {
		sa_family_t af;

		af = ifaddr_address_family(list[i]);
		switch (af) {
		case AF_INET: {
			struct sockaddr_in sin;

			error = ifaddr_address(list[i], (struct sockaddr *)&sin, sizeof(sin));
			if (error != 0) {
				BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
				    "ifaddr_address failed %d",
				    error);
				break;
			}

			bridge_create_mac_nat_entry_ipv4(sc, mac_nat_bif, &sin.sin_addr, IF_LLADDR(ifp));
			break;
		}

		case AF_INET6: {
			struct sockaddr_in6 sin6;

			error = ifaddr_address(list[i], (struct sockaddr *)&sin6, sizeof(sin6));
			if (error != 0) {
				BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
				    "ifaddr_address failed %d",
				    error);
				break;
			}

			if (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr)) {
				/* remove scope ID */
				sin6.sin6_addr.s6_addr16[1] = 0;
			}

			bridge_create_mac_nat_entry_ipv6(sc, mac_nat_bif, &sin6.sin6_addr, IF_LLADDR(ifp));
			break;
		}

		default:
			BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
			    "ifaddr_address_family unknown %d",
			    af);
			break;
		}
	}

	ifnet_address_list_free_counted_by(list, addresses_count);
	return;
}

static void
bridge_mac_nat_age_entries_common(struct bridge_softc *sc,
    struct mac_nat_entry_list *list, unsigned long now)
{
	struct mac_nat_entry *mne;
	struct mac_nat_entry *tmne;

	LIST_FOREACH_SAFE(mne, list, mne_list, tmne) {
		if (now >= mne->mne_expire) {
			bridge_destroy_mac_nat_entry(sc, mne, "aged out");
		}
	}
}

static void
bridge_mac_nat_age_entries(struct bridge_softc *sc, unsigned long now)
{
	if (sc->sc_mac_nat_bif == NULL) {
		return;
	}
	bridge_mac_nat_age_entries_common(sc, &sc->sc_mne_list, now);
	bridge_mac_nat_age_entries_common(sc, &sc->sc_mne_list_v6, now);
}

static const char *
get_in_out_string(boolean_t is_output)
{
	return (const char * __null_terminated)(is_output ? "OUT" : "IN");
}

/*
 * is_valid_arp_packet:
 *	Verify that this is a valid ARP packet.
 *
 *	Returns TRUE if the packet is valid, FALSE otherwise.
 */
static boolean_t
is_valid_arp_packet(mbuf_t *data, bool is_output,
    struct ether_header **eh_p, struct ether_arp **ea_p)
{
	struct ether_arp *ea;
	struct ether_header *eh;
	size_t minlen = sizeof(struct ether_header) + sizeof(struct ether_arp);
	boolean_t is_valid = FALSE;
	int flags = is_output ? BR_DBGF_OUTPUT : BR_DBGF_INPUT;

	if (mbuf_pkthdr_len(*data) < minlen) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "ARP %s short frame %lu < %lu",
		    get_in_out_string(is_output),
		    mbuf_pkthdr_len(*data), minlen);
		goto done;
	}
	if (mbuf_len(*data) < minlen && mbuf_pullup(data, minlen) != 0) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "ARP %s size %lu mbuf_pullup fail",
		    get_in_out_string(is_output),
		    minlen);
		*data = NULL;
		goto done;
	}

	/* validate ARP packet */
	eh = mtod(*data, struct ether_header *);
	ea = (struct ether_arp *)(eh + 1);
	if (ea->arp_hrd != HTONS_ARPHRD_ETHER) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "ARP %s htype not ethernet",
		    get_in_out_string(is_output));
		goto done;
	}
	if (ea->arp_hln != ETHER_ADDR_LEN) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "ARP %s hlen not ethernet",
		    get_in_out_string(is_output));
		goto done;
	}
	if (ea->arp_pro != HTONS_ETHERTYPE_IP) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "ARP %s ptype not IP",
		    get_in_out_string(is_output));
		goto done;
	}
	if (ea->arp_pln != sizeof(struct in_addr)) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "ARP %s plen not IP",
		    get_in_out_string(is_output));
		goto done;
	}
	is_valid = TRUE;
	*ea_p = ea;
	*eh_p = eh;
done:
	return is_valid;
}

static struct mac_nat_entry *
bridge_mac_nat_arp_input(struct bridge_softc *sc, mbuf_t *data)
{
	struct ether_arp        * __single ea;
	struct ether_header     * __single eh;
	struct mac_nat_entry    *mne = NULL;
	u_short                 op;
	struct in_addr          tpa;

	if (!is_valid_arp_packet(data, FALSE, &eh, &ea)) {
		goto done;
	}
	op = ea->arp_op;
	switch (op) {
	case HTONS_ARPOP_REQUEST:
	case HTONS_ARPOP_REPLY:
		/* only care about REQUEST and REPLY */
		break;
	default:
		goto done;
	}

	/* check the target IP address for a NAT entry */
	bcopy(ea->arp_tpa, &tpa, sizeof(tpa));
	if (tpa.s_addr != 0) {
		mne = bridge_lookup_mac_nat_entry_ipv4(sc, &tpa);
	}
	if (mne != NULL) {
		if (op == HTONS_ARPOP_REPLY) {
			/* translate the MAC address */
			if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
				char    mac_src[24];
				char    mac_dst[24];

				ether_ntop(mac_src, sizeof(mac_src),
				    ea->arp_tha);
				ether_ntop(mac_dst, sizeof(mac_dst),
				    mne->mne_mac);
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
				    "%s %s ARP %s -> %s",
				    sc->sc_if_xname,
				    mne->mne_bif->bif_ifp->if_xname,
				    mac_src, mac_dst);
			}
			bcopy(mne->mne_mac, ea->arp_tha, sizeof(ea->arp_tha));
		}
	} else {
		/* handle conflicting ARP (sender matches mne) */
		struct in_addr spa;

		bcopy(ea->arp_spa, &spa, sizeof(spa));
		if (spa.s_addr != 0 && spa.s_addr != tpa.s_addr) {
			/* check the source IP for a NAT entry */
			mne = bridge_lookup_mac_nat_entry_ipv4(sc, &spa);
		}
	}

done:
	return mne;
}

static boolean_t
bridge_mac_nat_arp_output(struct bridge_softc *sc,
    struct bridge_iflist *bif, mbuf_t *data, struct mac_nat_record *mnr)
{
	struct ether_arp        * __single ea;
	struct ether_header     * __single eh;
	struct in_addr          ip;
	struct mac_nat_entry    *mne = NULL;
	u_short                 op;
	boolean_t               translate = FALSE;

	if (!is_valid_arp_packet(data, TRUE, &eh, &ea)) {
		goto done;
	}
	op = ea->arp_op;
	switch (op) {
	case HTONS_ARPOP_REQUEST:
	case HTONS_ARPOP_REPLY:
		/* only care about REQUEST and REPLY */
		break;
	default:
		goto done;
	}

	bcopy(ea->arp_spa, &ip, sizeof(ip));
	if (ip.s_addr == 0) {
		goto done;
	}
	/* XXX validate IP address: no multicast/broadcast */
	mne = bridge_update_mac_nat_entry_ipv4(sc, bif, &ip,
	    (const char *)ea->arp_sha);
	if (mnr != NULL && mne != NULL) {
		/* record the offset to do the replacement */
		translate = TRUE;
		mnr->mnr_arp_offset = (char *)ea->arp_sha - (char *)eh;
	}

done:
	return translate;
}

#define ETHER_IPV4_HEADER_LEN   (sizeof(struct ether_header) +  \
	                         + sizeof(struct ip))
static uint8_t * __indexable
get_ether_ip_header_ptr(mbuf_t *data, boolean_t is_output)
{
	uint8_t         *header = NULL;
	int             flags = is_output ? BR_DBGF_OUTPUT : BR_DBGF_INPUT;
	size_t          minlen = ETHER_IPV4_HEADER_LEN;

	if (mbuf_pkthdr_len(*data) < minlen) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "IP %s short frame %lu < %lu",
		    get_in_out_string(is_output),
		    mbuf_pkthdr_len(*data), minlen);
		goto done;
	}
	if (mbuf_len(*data) < minlen && mbuf_pullup(data, minlen) != 0) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "IP %s size %lu mbuf_pullup fail",
		    get_in_out_string(is_output),
		    minlen);
		*data = NULL;
		goto done;
	}
	header = mtod(*data, uint8_t *);
done:
	return header;
}

static bool
is_broadcast_ip_packet(mbuf_t *data)
{
	uint8_t                 *header;
	struct ether_header     *eh;
	bool                    is_broadcast = FALSE;

	eh = mtod(*data, struct ether_header *);
	switch (eh->ether_type) {
	case HTONS_ETHERTYPE_IP:
		header = get_ether_ip_header_ptr(data, FALSE);
		if (header != NULL) {
			struct in_addr  dst;
			struct ip       *iphdr;

			iphdr = (struct ip *)(header + sizeof(struct ether_header));
			bcopy(&iphdr->ip_dst, &dst, sizeof(dst));
			is_broadcast = (dst.s_addr == INADDR_BROADCAST);
		}
		break;
	default:
		break;
	}
	return is_broadcast;
}

static struct mac_nat_entry *
bridge_mac_nat_ip_input(struct bridge_softc *sc, mbuf_t *data)
{
	struct in_addr          dst;
	uint8_t                 *header;
	struct ip               *iphdr;
	struct mac_nat_entry    *mne = NULL;

	header = get_ether_ip_header_ptr(data, FALSE);
	if (header == NULL) {
		goto done;
	}
	iphdr = (struct ip *)(void *)(header + sizeof(struct ether_header));
	bcopy(&iphdr->ip_dst, &dst, sizeof(dst));
	/* XXX validate IP address */
	if (dst.s_addr == 0) {
		goto done;
	}
	mne = bridge_lookup_mac_nat_entry_ipv4(sc, &dst);
done:
	return mne;
}

static void
bridge_mac_nat_udp_output(struct bridge_softc *sc,
    struct bridge_iflist *bif, mbuf_t m,
    uint8_t ip_header_len, struct mac_nat_record *mnr)
{
	uint16_t        dp_flags;
	errno_t         error;
	size_t          offset;
	struct udphdr   udphdr;

	/* copy the UDP header */
	offset = sizeof(struct ether_header) + ip_header_len;
	error = mbuf_copydata(m, offset, sizeof(struct udphdr), &udphdr);
	if (error != 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
		    "mbuf_copydata udphdr failed %d",
		    error);
		return;
	}
	if (udphdr.uh_sport != HTONS_IPPORT_BOOTPC ||
	    udphdr.uh_dport != HTONS_IPPORT_BOOTPS) {
		/* not a BOOTP/DHCP packet */
		return;
	}
	/* check whether the broadcast bit is already set */
	offset += sizeof(struct udphdr) + offsetof(struct dhcp, dp_flags);
	error = mbuf_copydata(m, offset, sizeof(dp_flags), &dp_flags);
	if (error != 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
		    "mbuf_copydata dp_flags failed %d",
		    error);
		return;
	}
	if ((dp_flags & HTONS_DHCP_FLAGS_BROADCAST) != 0) {
		/* it's already set, nothing to do */
		return;
	}
	/* broadcast bit needs to be set */
	mnr->mnr_ip_dhcp_flags = dp_flags | htons(DHCP_FLAGS_BROADCAST);
	mnr->mnr_ip_header_len = ip_header_len;
	if (udphdr.uh_sum != 0) {
		uint16_t        delta;

		/* adjust checksum to take modified dp_flags into account */
		delta = dp_flags - mnr->mnr_ip_dhcp_flags;
		mnr->mnr_ip_udp_csum = udphdr.uh_sum + delta;
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
	    "%s %s DHCP dp_flags 0x%x UDP cksum 0x%x",
	    sc->sc_if_xname,
	    bif->bif_ifp->if_xname,
	    ntohs(mnr->mnr_ip_dhcp_flags),
	    ntohs(mnr->mnr_ip_udp_csum));
	return;
}

static boolean_t
bridge_mac_nat_ip_output(struct bridge_softc *sc,
    struct bridge_iflist *bif, mbuf_t *data, struct mac_nat_record *mnr)
{
#pragma unused(mnr)
	uint8_t                 *header;
	struct ether_header     *eh;
	struct in_addr          ip;
	struct ip               *iphdr;
	uint8_t                 ip_header_len;
	struct mac_nat_entry    *mne = NULL;
	boolean_t               translate = FALSE;

	header = get_ether_ip_header_ptr(data, TRUE);
	if (header == NULL) {
		goto done;
	}

	eh = (struct ether_header *)header;
	iphdr = (struct ip *)(header + sizeof(*eh));
	ip_header_len = IP_VHL_HL(iphdr->ip_vhl) << 2;
	if (ip_header_len < sizeof(ip)) {
		/* bogus IP header */
		goto done;
	}
	bcopy(&iphdr->ip_src, &ip, sizeof(ip));
	/* XXX validate the source address */
	if (ip.s_addr != 0) {
		mne = bridge_update_mac_nat_entry_ipv4(sc, bif, &ip,
		    (const char *)eh->ether_shost);
	}
	if (mnr != NULL) {
		if (ip.s_addr == 0 && iphdr->ip_p == IPPROTO_UDP) {
			/* handle DHCP must broadcast */
			bridge_mac_nat_udp_output(sc, bif, *data,
			    ip_header_len, mnr);
		}
		translate = TRUE;
	}
done:
	return translate;
}

#define ETHER_IPV6_HEADER_LEN   (sizeof(struct ether_header) +  \
	                         + sizeof(struct ip6_hdr))
static uint8_t * __indexable
get_ether_ipv6_header_ptr(mbuf_t *data, size_t plen, boolean_t is_output)
{
	uint8_t         *header = NULL;
	int             flags = is_output ? BR_DBGF_OUTPUT : BR_DBGF_INPUT;
	size_t          minlen = ETHER_IPV6_HEADER_LEN + plen;

	if (mbuf_pkthdr_len(*data) < minlen) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "IP %s short frame %lu < %lu",
		    get_in_out_string(is_output),
		    mbuf_pkthdr_len(*data), minlen);
		goto done;
	}
	if (mbuf_len(*data) < minlen && mbuf_pullup(data, minlen) != 0) {
		BRIDGE_LOG(LOG_DEBUG, flags,
		    "IP %s size %lu mbuf_pullup fail",
		    get_in_out_string(is_output),
		    minlen);
		*data = NULL;
		goto done;
	}
	header = mtod(*data, uint8_t *);
done:
	return header;
}

#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#define ETHER_ND_LLADDR_LEN     (ETHER_ADDR_LEN + sizeof(struct nd_opt_hdr))

static void
bridge_mac_nat_icmpv6_output(struct bridge_softc *sc,
    struct bridge_iflist *bif,
    mbuf_t *data, struct ip6_hdr *ip6h,
    struct in6_addr *saddrp,
    struct mac_nat_record *mnr)
{
	uint8_t *header;
	struct ether_header *eh;
	struct icmp6_hdr *icmp6;
	uint8_t         icmp6_type;
	uint32_t        icmp6len;
	int             lladdrlen = 0;
	char            *lladdr = NULL;
	unsigned int    off = sizeof(*ip6h);

	icmp6len = (u_int32_t)ntohs(ip6h->ip6_plen);
	if (icmp6len < sizeof(*icmp6)) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "short IPv6 payload length %d < %lu",
		    icmp6len, sizeof(*icmp6));
		return;
	}

	/* pullup IP6 header + ICMPv6 header */
	header = get_ether_ipv6_header_ptr(data, sizeof(*icmp6), TRUE);
	if (header == NULL) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "failed to pullup icmp6 header");
		return;
	}
	eh = (struct ether_header *)header;
	ip6h = (struct ip6_hdr *)(header + sizeof(*eh));
	icmp6 = (struct icmp6_hdr *)(header + sizeof(*eh) + off);
	icmp6_type = icmp6->icmp6_type;
	switch (icmp6_type) {
	case ND_NEIGHBOR_SOLICIT:
	case ND_NEIGHBOR_ADVERT:
	case ND_ROUTER_ADVERT:
	case ND_ROUTER_SOLICIT:
		break;
	default:
		return;
	}

	/* pullup IP6 header + payload */
	header = get_ether_ipv6_header_ptr(data, icmp6len, TRUE);
	if (header == NULL) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "failed to pullup icmp6 + payload");
		return;
	}
	eh = (struct ether_header *)header;
	ip6h = (struct ip6_hdr *)(header + sizeof(*eh));
	icmp6 = (struct icmp6_hdr *)(header + sizeof(*eh) + off);

	switch (icmp6_type) {
	case ND_NEIGHBOR_SOLICIT: {
		struct nd_neighbor_solicit *nd_ns;
		union nd_opts ndopts;
		boolean_t is_dad_probe;
		struct in6_addr taddr;

		if (icmp6len < sizeof(*nd_ns)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "short nd_ns %d < %lu",
			    icmp6len, sizeof(*nd_ns));
			return;
		}

		nd_ns = (struct nd_neighbor_solicit *)(void *)icmp6;
		bcopy(&nd_ns->nd_ns_target, &taddr, sizeof(taddr));
		if (IN6_IS_ADDR_MULTICAST(&taddr) ||
		    IN6_IS_ADDR_UNSPECIFIED(&taddr)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "invalid target ignored");
			return;
		}

		/* parse options */
		nd6_option_init(nd_ns + 1, icmp6len - sizeof(*nd_ns), &ndopts);
		if (nd6_options(&ndopts) < 0) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "invalid ND6 NS option");
			return;
		}
		if (ndopts.nd_opts_src_lladdr != NULL) {
			ND_OPT_LLADDR(ndopts.nd_opts_src_lladdr, nd_opt_len,
			    lladdr, lladdrlen);
		}
		is_dad_probe = IN6_IS_ADDR_UNSPECIFIED(saddrp);
		if (lladdr != NULL) {
			if (is_dad_probe) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
				    "bad ND6 DAD packet");
				return;
			}
			if (lladdrlen != ETHER_ND_LLADDR_LEN) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
				    "source lladdrlen %d != %lu",
				    lladdrlen, ETHER_ND_LLADDR_LEN);
				return;
			}
		}
		if (is_dad_probe) {
			/* node is trying use taddr, create an mne for taddr */
			*saddrp = taddr;
		}
		break;
	}
	case ND_NEIGHBOR_ADVERT: {
		struct nd_neighbor_advert *nd_na;
		union nd_opts ndopts;
		struct in6_addr taddr;


		nd_na = (struct nd_neighbor_advert *)(void *)icmp6;

		if (icmp6len < sizeof(*nd_na)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "short nd_na %d < %lu",
			    icmp6len, sizeof(*nd_na));
			return;
		}

		bcopy(&nd_na->nd_na_target, &taddr, sizeof(taddr));
		if (IN6_IS_ADDR_MULTICAST(&taddr) ||
		    IN6_IS_ADDR_UNSPECIFIED(&taddr)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "invalid target ignored");
			return;
		}

		/* parse options */
		nd6_option_init(nd_na + 1, icmp6len - sizeof(*nd_na), &ndopts);
		if (nd6_options(&ndopts) < 0) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "invalid ND6 NA option");
			return;
		}
		if (ndopts.nd_opts_tgt_lladdr == NULL) {
			/* target linklayer, nothing to do */
			return;
		}

		ND_OPT_LLADDR(ndopts.nd_opts_tgt_lladdr, nd_opt_len, lladdr, lladdrlen);
		if (lladdrlen != ETHER_ND_LLADDR_LEN) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "target lladdrlen %d != %lu",
			    lladdrlen, ETHER_ND_LLADDR_LEN);
			return;
		}
		break;
	}
	case ND_ROUTER_ADVERT:
	case ND_ROUTER_SOLICIT: {
		union nd_opts ndopts;
		uint32_t type_length;
		const char *description;

		if (icmp6_type == ND_ROUTER_ADVERT) {
			type_length = sizeof(struct nd_router_advert);
			description = "RA";
		} else {
			type_length = sizeof(struct nd_router_solicit);
			description = "RS";
		}
		if (icmp6len < type_length) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "short ND6 %s %d < %d",
			    description, icmp6len, type_length);
			return;
		}

		/* parse options */
		nd6_option_init(((uint8_t *)icmp6) + type_length,
		    icmp6len - type_length, &ndopts);
		if (nd6_options(&ndopts) < 0) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "invalid ND6 %s option", description);
			return;
		}
		if (ndopts.nd_opts_src_lladdr != NULL) {
			ND_OPT_LLADDR(ndopts.nd_opts_src_lladdr, nd_opt_len, lladdr, lladdrlen);

			if (lladdrlen != ETHER_ND_LLADDR_LEN) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
				    "source lladdrlen %d != %lu",
				    lladdrlen, ETHER_ND_LLADDR_LEN);
				return;
			}
		}
		break;
	}
	default:
		break;
	}

	if (lladdr != NULL) {
		mnr->mnr_ip6_lladdr_offset = (uint16_t)
		    ((uintptr_t)lladdr - (uintptr_t)eh);
		mnr->mnr_ip6_icmp6_len = icmp6len;
		mnr->mnr_ip6_icmp6_type = icmp6_type;
		mnr->mnr_ip6_header_len = off;
		if (BRIDGE_DBGF_ENABLED(BR_DBGF_MAC_NAT)) {
			const char *str;

			switch (mnr->mnr_ip6_icmp6_type) {
			case ND_ROUTER_ADVERT:
				str = "ROUTER ADVERT";
				break;
			case ND_ROUTER_SOLICIT:
				str = "ROUTER SOLICIT";
				break;
			case ND_NEIGHBOR_ADVERT:
				str = "NEIGHBOR ADVERT";
				break;
			case ND_NEIGHBOR_SOLICIT:
				str = "NEIGHBOR SOLICIT";
				break;
			default:
				str = "";
				break;
			}
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "%s %s %s ip6len %d icmp6len %d lladdr offset %d",
			    sc->sc_if_xname, bif->bif_ifp->if_xname, str,
			    mnr->mnr_ip6_header_len,
			    mnr->mnr_ip6_icmp6_len, mnr->mnr_ip6_lladdr_offset);
		}
	}
}

static struct mac_nat_entry *
bridge_mac_nat_ipv6_input(struct bridge_softc *sc, mbuf_t *data)
{
	struct in6_addr         dst;
	uint8_t                 *header;
	struct ether_header     *eh;
	struct ip6_hdr          *ip6h;
	struct mac_nat_entry    *mne = NULL;

	header = get_ether_ipv6_header_ptr(data, 0, FALSE);
	if (header == NULL) {
		goto done;
	}
	eh = (struct ether_header *)header;
	ip6h = (struct ip6_hdr *)(header + sizeof(*eh));
	bcopy(&ip6h->ip6_dst, &dst, sizeof(dst));
	/* XXX validate IPv6 address */
	if (IN6_IS_ADDR_UNSPECIFIED(&dst)) {
		goto done;
	}
	mne = bridge_lookup_mac_nat_entry_ipv6(sc, &dst);

done:
	return mne;
}

static boolean_t
bridge_mac_nat_ipv6_output(struct bridge_softc *sc,
    struct bridge_iflist *bif, mbuf_t *data, struct mac_nat_record *mnr)
{
	uint8_t                 *header;
	struct ether_header     *eh;
	ether_addr_t            ether_shost;
	struct ip6_hdr          *ip6h;
	struct in6_addr         saddr;
	boolean_t               translate;

	translate = (bif == sc->sc_mac_nat_bif) ? FALSE : TRUE;
	header = get_ether_ipv6_header_ptr(data, 0, TRUE);
	if (header == NULL) {
		translate = FALSE;
		goto done;
	}
	eh = (struct ether_header *)header;
	bcopy(eh->ether_shost, &ether_shost, sizeof(ether_shost));
	ip6h = (struct ip6_hdr *)(header + sizeof(*eh));
	bcopy(&ip6h->ip6_src, &saddr, sizeof(saddr));
	if (mnr != NULL && ip6h->ip6_nxt == IPPROTO_ICMPV6) {
		bridge_mac_nat_icmpv6_output(sc, bif, data, ip6h, &saddr, mnr);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&saddr)) {
		goto done;
	}
	(void)bridge_update_mac_nat_entry_ipv6(sc, bif, &saddr,
	    (const char *)ether_shost.octet);

done:
	return translate;
}

/*
 * Function: bridge_mac_nat_input:
 *
 * Purpose:
 *   Process a unicast packet arriving on the external interface `external_ifp`.
 *
 *   If the packet is ARP, IPv4, or IPv6, lookup the address from the packet in
 *   the mac_nat_entry table. If an entry is found, and the interface is
 *   not `external_ifp`, replace the destination MAC address in the
 *   ethernet header with the corresponding internal MAC address, and return
 *   the interface via `*dst_if`.
 *
 * Returns:
 *   NULL if the packet was deallocated during processing.
 *
 *   Otherwise, returns non-NULL packet that should:
 *   1) if `*dst_if` is NULL, continue on as an input packet
 *      over `external_ifp`, OR
 *   2) if `*dst_if` is not NULL, be delivered as an output packet
 *      over `*dst_if`.
 */
static mbuf_t
bridge_mac_nat_input(struct bridge_softc *sc, ifnet_t external_ifp,
    mbuf_t m, ifnet_t * dst_if)
{
	struct ether_header     *eh;
	mbuf_t                  m0 = m;
	struct mac_nat_entry    *mne = NULL;

	BRIDGE_LOCK_ASSERT_HELD(sc);
	*dst_if = NULL;
	eh = mtod(m, struct ether_header *);
	switch (eh->ether_type) {
	case HTONS_ETHERTYPE_ARP:
		mne = bridge_mac_nat_arp_input(sc, &m);
		break;
	case HTONS_ETHERTYPE_IP:
		mne = bridge_mac_nat_ip_input(sc, &m);
		break;
	case HTONS_ETHERTYPE_IPV6:
		mne = bridge_mac_nat_ipv6_input(sc, &m);
		break;
	default:
		break;
	}
	if (m != NULL & mne != NULL) {
		*dst_if = mne->mne_bif->bif_ifp;
		if (*dst_if == external_ifp) {
			/* receive packet for ifp */
			*dst_if = NULL;
		} else {
			/* replace the destination MAC with internal one */
			if (m != m0) {
				/* it may have changed */
				eh = mtod(m, struct ether_header *);
			}
			bcopy(mne->mne_mac, eh->ether_dhost,
			    sizeof(eh->ether_dhost));
		}
	}
	return m;
}


static mblist
bridge_mac_nat_input_list(struct bridge_softc *sc, ifnet_t external_ifp,
    mbuf_t m, mbuf_t * forward_head)
{
	mblist          forward;
	mbuf_t          next_packet;
	mblist          ret;

	mblist_init(&ret);
	mblist_init(&forward);
	for (mbuf_t scan = m; scan != NULL; scan = next_packet) {
		ifnet_ref_t     dst_if;

		/* take packet out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;

		scan = bridge_mac_nat_input(sc, external_ifp, scan, &dst_if);
		if (scan != NULL) {
			if (dst_if != NULL) {
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
				    "%s MAC-NAT input translate to %s",
				    sc->sc_if_xname, dst_if->if_xname);
				/* use rcvif to store the egress interface */
				mbuf_pkthdr_setrcvif(scan, dst_if);
				/* add it to the forwarding list */
				mblist_append(&forward, scan);
			} else {
				/* add it to the "continue on as input" list */
				BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
				    "%s MAC-NAT input for %s",
				    sc->sc_if_xname,
				    external_ifp->if_xname);
				mblist_append(&ret, scan);
			}
		}
	}
	*forward_head = forward.head;
	return ret;
}

/*
 * bridge_mac_nat_translate_list:
 * Process a list of packets destined to the MAC-NAT interface `dst_if`
 * from the bridge member `sbif`.
 *
 * For each packet in the list, update the MAC-NAT record, and if
 * translation is required, translate it.
 *
 * Returns the list of packets that should be delivered to the MAC-NAT
 * interface.
 */
static mbuf_t
bridge_mac_nat_translate_list(struct bridge_softc * sc,
    struct bridge_iflist *sbif, ifnet_t dst_if, mbuf_t m)
{
	mbuf_t          next_packet;
	mblist          ret;

	mblist_init(&ret);
	for (mbuf_ref_t scan = m; scan != NULL; scan = next_packet) {
		struct mac_nat_record   mnr;
		bool                    translate_mac;

		/* take packet out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;
		translate_mac = bridge_mac_nat_output(sc, sbif, &scan, &mnr);
		if (scan != NULL) {
			if (translate_mac) {
				bridge_mac_nat_translate(&scan, &mnr,
				    IF_LLADDR(dst_if));
			}
			if (scan != NULL) {
				/* add it back to the list */
				mblist_append(&ret, scan);
			}
		}
	}
	return ret.head;
}

/*
 * bridge_mac_nat_copy_and_translate_list:
 * Same as bridge_mac_nat_translate_list() except that a copy of the
 * packet list is returned instead.
 *
 * The packet list `m` is left unaltered.
 */
static mbuf_t
bridge_mac_nat_copy_and_translate_list(struct bridge_softc * sc,
    struct bridge_iflist *sbif, ifnet_t dst_if, mbuf_t m)
{
	mbuf_t          next_packet;
	mblist          ret;

	mblist_init(&ret);
	for (mbuf_t scan = m; scan != NULL; scan = next_packet) {
		mbuf_ref_t              mc = NULL;
		struct mac_nat_record   mnr;
		bool                    translate_mac;

		/* take packet out of the list, make a copy, put it back */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;
		mc = m_dup(scan, M_DONTWAIT);
		scan->m_nextpkt = next_packet;
		if (mc == NULL) {
			continue;
		}
		translate_mac = bridge_mac_nat_output(sc, sbif, &mc, &mnr);
		if (mc != NULL) {
			if (translate_mac) {
				bridge_mac_nat_translate(&mc, &mnr,
				    IF_LLADDR(dst_if));
			}
			if (mc != NULL) {
				/* add it to the new list */
				mblist_append(&ret, mc);
			}
		}
	}
	return ret.head;
}

static void
bridge_mac_nat_forward_list(ifnet_t bridge_ifp, ether_type_flag_t etypef,
    mbuf_t m)
{
	int             count = 0;
	ifnet_t         dst_if;
	mblist          list;
	int             n_lists = 0;
	mbuf_t          next_packet;

	mblist_init(&list);
	for (mbuf_t scan = m; scan != NULL; scan = next_packet) {
		ifnet_t         this_if;

		next_packet = scan->m_nextpkt;
		this_if = mbuf_pkthdr_rcvif(scan);
		mbuf_pkthdr_setrcvif(scan, NULL);
		if (list.head == NULL) {
			/* start a new list */
			list.head = list.tail = scan;
			count = 1;
			dst_if = this_if;
		} else if (dst_if != this_if) {
			/* send up the previous chain */
			if (list.tail != NULL) {
				/* terminate the list */
				list.tail->m_nextpkt = NULL;
			}
			n_lists++;
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "(%s): sublist %u pkts %u",
			    dst_if->if_xname, n_lists, count);
			bridge_enqueue_multi(bridge_ifp, NULL,
			    dst_if, etypef, list.head,
			    CHECKSUM_OPERATION_CLEAR_OFFLOAD);

			/* start new list */
			list.head = list.tail = scan;
			count = 1;
			dst_if = this_if;
		} else {
			count++;
			list.tail = scan;
		}
		if (next_packet == NULL) {
			/* last list */
			n_lists++;
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MAC_NAT,
			    "(%s): sublist %u pkts %u",
			    dst_if->if_xname, n_lists, count);
			bridge_enqueue_multi(bridge_ifp, NULL,
			    dst_if, etypef, list.head,
			    CHECKSUM_OPERATION_CLEAR_OFFLOAD);
		}
	}
	return;
}

/*
 * bridge_mac_nat_output:
 * Process a packet destined to the MAC NAT interface (sc_mac_nat_bif)
 * from the interface 'bif'.
 *
 * Create a mac_nat_entry containing the source IP address and MAC address
 * from the packet. Populate a mac_nat_record with information detailing
 * how to translate the packet. Translation takes place later by calling
 * `bridge_mac_nat_translate()`.
 *
 * If 'bif' == sc_mac_nat_bif, the stack over the MAC NAT
 * interface is generating an output packet. No translation is required in this
 * case, we just record the IP address used to prevent another bif from
 * claiming our IP address.
 *
 * Returns:
 * TRUE if the packet should be translated (*mnr updated as well),
 * FALSE otherwise.
 *
 * *data may be updated to point at a different mbuf chain or NULL if
 * the chain was deallocated during processing.
 */

static boolean_t
bridge_mac_nat_output(struct bridge_softc *sc,
    struct bridge_iflist *bif, mbuf_t *data, struct mac_nat_record *mnr)
{
	struct ether_header     *eh;
	boolean_t               translate = FALSE;

	BRIDGE_LOCK_ASSERT_HELD(sc);
	assert(sc->sc_mac_nat_bif != NULL);

	eh = mtod(*data, struct ether_header *);
	if (mnr != NULL) {
		bzero(mnr, sizeof(*mnr));
		mnr->mnr_ether_type = eh->ether_type;
	}
	switch (eh->ether_type) {
	case HTONS_ETHERTYPE_ARP:
		translate = bridge_mac_nat_arp_output(sc, bif, data, mnr);
		break;
	case HTONS_ETHERTYPE_IP:
		translate = bridge_mac_nat_ip_output(sc, bif, data, mnr);
		break;
	case HTONS_ETHERTYPE_IPV6:
		translate = bridge_mac_nat_ipv6_output(sc, bif, data, mnr);
		break;
	default:
		break;
	}
	return translate;
}

static void
bridge_mac_nat_arp_translate(mbuf_t *data, struct mac_nat_record *mnr,
    const char eaddr[ETHER_ADDR_LEN])
{
	errno_t                 error;

	if (mnr->mnr_arp_offset == 0) {
		return;
	}
	/* replace the source hardware address */
	error = mbuf_copyback(*data, mnr->mnr_arp_offset,
	    ETHER_ADDR_LEN, eaddr,
	    MBUF_DONTWAIT);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "mbuf_copyback failed");
		m_freem(*data);
		*data = NULL;
	}
	return;
}

static void
bridge_mac_nat_ip_translate(mbuf_t *data, struct mac_nat_record *mnr)
{
	errno_t         error;
	size_t          offset;

	if (mnr->mnr_ip_header_len == 0) {
		return;
	}
	/* update the UDP checksum */
	offset = sizeof(struct ether_header) + mnr->mnr_ip_header_len;
	error = mbuf_copyback(*data, offset + offsetof(struct udphdr, uh_sum),
	    sizeof(mnr->mnr_ip_udp_csum),
	    &mnr->mnr_ip_udp_csum,
	    MBUF_DONTWAIT);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "mbuf_copyback uh_sum failed");
		m_freem(*data);
		*data = NULL;
	}
	/* update the DHCP must broadcast flag */
	offset += sizeof(struct udphdr);
	error = mbuf_copyback(*data, offset + offsetof(struct dhcp, dp_flags),
	    sizeof(mnr->mnr_ip_dhcp_flags),
	    &mnr->mnr_ip_dhcp_flags,
	    MBUF_DONTWAIT);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "mbuf_copyback dp_flags failed");
		m_freem(*data);
		*data = NULL;
	}
}

static void
bridge_mac_nat_ipv6_translate(mbuf_t *data, struct mac_nat_record *mnr,
    const char eaddr[ETHER_ADDR_LEN])
{
	uint16_t        cksum;
	errno_t         error;
	mbuf_t          m = *data;

	if (mnr->mnr_ip6_header_len == 0) {
		return;
	}
	switch (mnr->mnr_ip6_icmp6_type) {
	case ND_ROUTER_ADVERT:
	case ND_ROUTER_SOLICIT:
	case ND_NEIGHBOR_SOLICIT:
	case ND_NEIGHBOR_ADVERT:
		if (mnr->mnr_ip6_lladdr_offset == 0) {
			/* nothing to do */
			return;
		}
		break;
	default:
		return;
	}

	/*
	 * replace the lladdr
	 */
	error = mbuf_copyback(m, mnr->mnr_ip6_lladdr_offset,
	    ETHER_ADDR_LEN, eaddr,
	    MBUF_DONTWAIT);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "mbuf_copyback lladdr failed");
		m_freem(m);
		*data = NULL;
		return;
	}

	/*
	 * recompute the icmp6 checksum
	 */

	/* skip past the ethernet header */
	_mbuf_adjust_pkthdr_and_data(m, ETHER_HDR_LEN);

#define CKSUM_OFFSET_ICMP6      offsetof(struct icmp6_hdr, icmp6_cksum)
	/* set the checksum to zero */
	cksum = 0;
	error = mbuf_copyback(m, mnr->mnr_ip6_header_len + CKSUM_OFFSET_ICMP6,
	    sizeof(cksum), &cksum, MBUF_DONTWAIT);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "mbuf_copyback cksum=0 failed");
		m_freem(m);
		*data = NULL;
		return;
	}
	/* compute and set the new checksum */
	cksum = in6_cksum(m, IPPROTO_ICMPV6, mnr->mnr_ip6_header_len,
	    mnr->mnr_ip6_icmp6_len);
	error = mbuf_copyback(m, mnr->mnr_ip6_header_len + CKSUM_OFFSET_ICMP6,
	    sizeof(cksum), &cksum, MBUF_DONTWAIT);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, BR_DBGF_MAC_NAT,
		    "mbuf_copyback cksum failed");
		m_freem(m);
		*data = NULL;
		return;
	}
	/* restore the ethernet header */
	_mbuf_adjust_pkthdr_and_data(m, -ETHER_HDR_LEN);
	return;
}

static void
bridge_mac_nat_translate(mbuf_t *data, struct mac_nat_record *mnr,
    const char eaddr[ETHER_ADDR_LEN])
{
	struct ether_header     *eh;

	/* replace the source ethernet address with the single MAC */
	eh = mtod(*data, struct ether_header *);
	bcopy(eaddr, eh->ether_shost, sizeof(eh->ether_shost));
	switch (mnr->mnr_ether_type) {
	case HTONS_ETHERTYPE_ARP:
		bridge_mac_nat_arp_translate(data, mnr, eaddr);
		break;

	case HTONS_ETHERTYPE_IP:
		bridge_mac_nat_ip_translate(data, mnr);
		break;

	case HTONS_ETHERTYPE_IPV6:
		bridge_mac_nat_ipv6_translate(data, mnr, eaddr);
		break;

	default:
		break;
	}
	return;
}

/*
 * bridge packet filtering
 */

/*
 * Perform basic checks on header size since
 * pfil assumes ip_input has already processed
 * it for it.  Cut-and-pasted from ip_input.c.
 * Given how simple the IPv6 version is,
 * does the IPv4 version really need to be
 * this complicated?
 *
 * XXX Should we update ipstat here, or not?
 * XXX Right now we update ipstat but not
 * XXX csum_counter.
 */
static int
bridge_ip_checkbasic(struct mbuf **mp)
{
	struct mbuf *m = *mp;
	struct ip *ip;
	int len, hlen;
	u_short sum;

	if (*mp == NULL) {
		return -1;
	}

	if (IP_HDR_ALIGNED_P(mtod(m, caddr_t)) == 0) {
		/* max_linkhdr is already rounded up to nearest 4-byte */
		if ((m = m_copyup(m, sizeof(struct ip),
		    max_linkhdr)) == NULL) {
			/* XXXJRT new stat, please */
			ipstat.ips_toosmall++;
			goto bad;
		}
	} else if (OS_EXPECT((size_t)m->m_len < sizeof(struct ip), 0)) {
		if ((m = m_pullup(m, sizeof(struct ip))) == NULL) {
			ipstat.ips_toosmall++;
			goto bad;
		}
	}
	ip = mtod(m, struct ip *);
	if (ip == NULL) {
		goto bad;
	}

	if (IP_VHL_V(ip->ip_vhl) != IPVERSION) {
		ipstat.ips_badvers++;
		goto bad;
	}
	hlen = IP_VHL_HL(ip->ip_vhl) << 2;
	if (hlen < (int)sizeof(struct ip)) {  /* minimum header length */
		ipstat.ips_badhlen++;
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			ipstat.ips_badhlen++;
			goto bad;
		}
		ip = mtod(m, struct ip *);
		if (ip == NULL) {
			goto bad;
		}
	}

	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		sum = !(m->m_pkthdr.csum_flags & CSUM_IP_VALID);
	} else {
		if (hlen == sizeof(struct ip)) {
			sum = in_cksum_hdr(ip);
		} else {
			sum = in_cksum(m, hlen);
		}
	}
	if (sum) {
		ipstat.ips_badsum++;
		goto bad;
	}

	/* Retrieve the packet length. */
	len = ntohs(ip->ip_len);

	/*
	 * Check for additional length bogosity
	 */
	if (len < hlen) {
		ipstat.ips_badlen++;
		goto bad;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		ipstat.ips_tooshort++;
		goto bad;
	}

	/* Checks out, proceed */
	*mp = m;
	return 0;

bad:
	*mp = m;
	return -1;
}

/*
 * Same as above, but for IPv6.
 * Cut-and-pasted from ip6_input.c.
 * XXX Should we update ip6stat, or not?
 */
static int
bridge_ip6_checkbasic(struct mbuf **mp)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;

	/*
	 * If the IPv6 header is not aligned, slurp it up into a new
	 * mbuf with space for link headers, in the event we forward
	 * it.  Otherwise, if it is aligned, make sure the entire base
	 * IPv6 header is in the first mbuf of the chain.
	 */
	if (IP6_HDR_ALIGNED_P(mtod(m, caddr_t)) == 0) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		/* max_linkhdr is already rounded up to nearest 4-byte */
		if ((m = m_copyup(m, sizeof(struct ip6_hdr),
		    max_linkhdr)) == NULL) {
			/* XXXJRT new stat, please */
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	} else if (OS_EXPECT((size_t)m->m_len < sizeof(struct ip6_hdr), 0)) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6stat.ip6s_badvers++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
		goto bad;
	}

	/* Checks out, proceed */
	*mp = m;
	return 0;

bad:
	*mp = m;
	return -1;
}

/*
 * the PF routines expect to be called from ip_input, so we
 * need to do and undo here some of the same processing.
 *
 * XXX : this is heavily inspired on bridge_pfil()
 */
static int
bridge_pf(struct mbuf **mp, struct ifnet *ifp, uint32_t sc_filter_flags,
    bool input)
{
	/*
	 * XXX : mpetit : heavily inspired by bridge_pfil()
	 */

	int snap, error, i, hlen;
	struct ether_header *eh1, eh2;
	struct ip *ip;
	struct llc llc1;
	u_int16_t ether_type;

	snap = 0;
	error = -1;     /* Default error if not error == 0 */

	if ((sc_filter_flags & IFBF_FILT_MEMBER) == 0) {
		return 0; /* filtering is disabled */
	}
	i = min((*mp)->m_pkthdr.len, max_protohdr);
	if ((*mp)->m_len < i) {
		*mp = m_pullup(*mp, i);
		if (*mp == NULL) {
			BRIDGE_LOG(LOG_NOTICE, 0, "m_pullup failed");
			return -1;
		}
	}

	eh1 = mtod(*mp, struct ether_header *);
	ether_type = ntohs(eh1->ether_type);

	/*
	 * Check for SNAP/LLC.
	 */
	if (ether_type < ETHERMTU) {
		struct llc *llc2 = (struct llc *)(eh1 + 1);

		if ((*mp)->m_len >= ETHER_HDR_LEN + 8 &&
		    llc2->llc_dsap == LLC_SNAP_LSAP &&
		    llc2->llc_ssap == LLC_SNAP_LSAP &&
		    llc2->llc_control == LLC_UI) {
			ether_type = htons(llc2->llc_un.type_snap.ether_type);
			snap = 1;
		}
	}

	/*
	 * If we're trying to filter bridge traffic, don't look at anything
	 * other than IP and ARP traffic.  If the filter doesn't understand
	 * IPv6, don't allow IPv6 through the bridge either.  This is lame
	 * since if we really wanted, say, an AppleTalk filter, we are hosed,
	 * but of course we don't have an AppleTalk filter to begin with.
	 * (Note that since pfil doesn't understand ARP it will pass *ALL*
	 * ARP traffic.)
	 */
	switch (ether_type) {
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		return 0;         /* Automatically pass */

	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6:
		break;
	default:
		/*
		 * Check to see if the user wants to pass non-ip
		 * packets, these will not be checked by pf and
		 * passed unconditionally so the default is to drop.
		 */
		if ((sc_filter_flags & IFBF_FILT_ONLYIP)) {
			goto bad;
		}
		break;
	}

	/* Strip off the Ethernet header and keep a copy. */
	m_copydata(*mp, 0, ETHER_HDR_LEN, (caddr_t)&eh2);
	m_adj(*mp, ETHER_HDR_LEN);

	/* Strip off snap header, if present */
	if (snap) {
		m_copydata(*mp, 0, sizeof(struct llc), (caddr_t)&llc1);
		m_adj(*mp, sizeof(struct llc));
	}

	/*
	 * Check the IP header for alignment and errors
	 */
	switch (ether_type) {
	case ETHERTYPE_IP:
		error = bridge_ip_checkbasic(mp);
		break;
	case ETHERTYPE_IPV6:
		error = bridge_ip6_checkbasic(mp);
		break;
	default:
		error = 0;
		break;
	}
	if (error) {
		goto bad;
	}

	error = 0;

	/*
	 * Run the packet through pf rules
	 */
	switch (ether_type) {
	case ETHERTYPE_IP:
		/*
		 * before calling the firewall, swap fields the same as
		 * IP does. here we assume the header is contiguous
		 */
		ip = mtod(*mp, struct ip *);

		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);

		if (ifp != NULL) {
			error = pf_af_hook(ifp, 0, mp, AF_INET, input, NULL);
		}

		if (*mp == NULL || error != 0) { /* filter may consume */
			break;
		}

		/* Recalculate the ip checksum and restore byte ordering */
		ip = mtod(*mp, struct ip *);
		hlen = IP_VHL_HL(ip->ip_vhl) << 2;
		if (hlen < (int)sizeof(struct ip)) {
			goto bad;
		}
		if (hlen > (*mp)->m_len) {
			if ((*mp = m_pullup(*mp, hlen)) == 0) {
				goto bad;
			}
			ip = mtod(*mp, struct ip *);
			if (ip == NULL) {
				goto bad;
			}
		}
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		ip->ip_sum = 0;
		if (hlen == sizeof(struct ip)) {
			ip->ip_sum = in_cksum_hdr(ip);
		} else {
			ip->ip_sum = in_cksum(*mp, hlen);
		}
		break;

	case ETHERTYPE_IPV6:
		if (ifp != NULL) {
			error = pf_af_hook(ifp, 0, mp, AF_INET6, input, NULL);
		}

		if (*mp == NULL || error != 0) { /* filter may consume */
			break;
		}
		break;
	default:
		error = 0;
		break;
	}

	if (*mp == NULL) {
		return error;
	}
	if (error != 0) {
		goto bad;
	}

	error = -1;

	/*
	 * Finally, put everything back the way it was and return
	 */
	if (snap) {
		M_PREPEND(*mp, sizeof(struct llc), M_DONTWAIT, 0);
		if (*mp == NULL) {
			return error;
		}
		bcopy(&llc1, mtod(*mp, caddr_t), sizeof(struct llc));
	}

	M_PREPEND(*mp, ETHER_HDR_LEN, M_DONTWAIT, 0);
	if (*mp == NULL) {
		return error;
	}
	bcopy(&eh2, mtod(*mp, caddr_t), ETHER_HDR_LEN);

	return 0;

bad:
	m_freem(*mp);
	*mp = NULL;
	return error;
}

#if BRIDGESTP
static void
bridge_bstp_input_list(struct bstp_port *bp, struct mbuf *head)
{
	mbuf_t  next_packet = NULL;

	for (mbuf_t scan = head; scan != NULL; scan = next_packet) {
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;
		bstp_input(bp, scan);
	}
}
#endif /* BRIDGESTP */

static mblist
bridge_filter_arp_list(struct bridge_iflist * bif, mbuf_t m)
{
	mbuf_t          next_packet = NULL;
	mblist          ret;

	mblist_init(&ret);
	for (mbuf_ref_t scan = m; scan != NULL; scan = next_packet) {
		errno_t                 error;

		/* take packet out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;
		/* filter the ARP packet */
		error = bridge_host_filter_arp(bif, &scan);
		if (error != 0 && scan != NULL) {
			if (BRIDGE_DBGF_ENABLED(BR_DBGF_HOSTFILTER)) {
				brlog_mbuf_data(scan, 0,
				    sizeof(struct ether_header) +
				    sizeof(struct ip));
			}
			m_freem(scan);
			scan = NULL;
		}
		if (scan != NULL) {
			/* add it to the list */
			mblist_append(&ret, scan);
		}
	}
	return ret;
}

static mbuf_t
bridge_filter_checksum(ifnet_t bridge_ifp, struct bridge_iflist * bif, mbuf_t m,
    bool is_ipv4, bool host_filter, bool checksum)
{
	uint32_t                dbgf = 0;
	errno_t                 error;
	ip_packet_info          info;
	u_int                   mac_hlen = sizeof(struct ether_header);

	if (host_filter) {
		dbgf |= BR_DBGF_HOSTFILTER;
	}
	if (checksum) {
		dbgf |= BR_DBGF_CHECKSUM;
	}

	/* get the IP protocol header */
	error = bridge_get_ip_proto(&m, mac_hlen, is_ipv4, &info,
	    &bif->bif_stats.brms_in_ip);
	if (error != 0) {
		BRIDGE_LOG(LOG_NOTICE, dbgf,
		    "%s(%s) bridge_get_ip_proto failed %d",
		    bridge_ifp->if_xname,
		    bif->bif_ifp->if_xname, error);
		goto drop;
	}
	if (host_filter) {
		bool            drop = true;

		/* restrict IP protocols */
		switch (info.ip_proto) {
		case IPPROTO_ICMP:
		case IPPROTO_IGMP:
			drop = !is_ipv4;
			break;
		case IPPROTO_TCP:
		case IPPROTO_UDP:
			drop = false;
			break;
		case IPPROTO_ICMPV6:
			drop = is_ipv4;
			break;
		default:
			break;
		}
		if (drop) {
			BRIDGE_HF_DROP(brhf_ip_bad_proto, __func__, __LINE__);
			goto drop;
		}
		bridge_hostfilter_stats.brhf_ip_ok += 1;
	}
	if (checksum) {
		/* need to compute IP/UDP/TCP/checksums */
		error = bridge_offload_checksum(&m, &info, &bif->bif_stats);
		if (error != 0) {
			BRIDGE_LOG(LOG_NOTICE, dbgf,
			    "%s(%s) bridge_offload_checksum failed %d",
			    bridge_ifp->if_xname,
			    bif->bif_ifp->if_xname, error);
			goto drop;
		}
	}
	return m;

drop:
	/* toss the packet */
	if (m != NULL) {
		if (host_filter &&
		    BRIDGE_DBGF_ENABLED(BR_DBGF_HOSTFILTER)) {
			brlog_mbuf_data(m, 0,
			    sizeof(struct ether_header) +
			    sizeof(struct ip));
		}
		m_freem(m);
		m = NULL;
	}
	return NULL;
}

static mblist
bridge_filter_checksum_list(ifnet_t bridge_ifp, struct bridge_iflist * bif,
    mbuf_t in_list, ether_type_flag_t etypef, bool host_filter, bool checksum)
{
	bool                    is_ipv4 = (etypef == ETHER_TYPE_FLAG_IPV4);
	mbuf_t                  next_packet = NULL;
	mblist                  ret;

	mblist_init(&ret);
	for (mbuf_t scan = in_list; scan != NULL; scan = next_packet) {
		/* take packet out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;
		scan = bridge_filter_checksum(bridge_ifp, bif,
		    scan, is_ipv4, host_filter, checksum);
		if (scan != NULL) {
			/* add packet to the list */
			mblist_append(&ret, scan);
		}
	}
	return ret;
}

static mbuf_t
copy_broadcast_packet(mbuf_t m)
{
	mbuf_t  mc;

	/* make a copy of the packet */
	mc = m_dup(m, M_DONTWAIT);
	if (mc != NULL) {
		struct ether_header *eh;

		/* make copy look like it is broadcast */
		mc->m_flags |= M_BCAST;
		eh = mtod(mc, struct ether_header *);
		bcopy(etherbroadcastaddr, eh->ether_dhost, ETHER_ADDR_LEN);
	}
	return mc;
}

static mblist
bridge_find_broadcast_ipv4(mbuf_t in_list, mbuf_t * ip_bcast_head)
{
	mblist          ip_bcast;
	mbuf_t          next_packet = NULL;
	mblist          ret;

	mblist_init(&ret);
	mblist_init(&ip_bcast);
	for (mbuf_ref_t scan = in_list; scan != NULL; scan = next_packet) {
		mbuf_t  bcast_pkt = NULL;
		uint8_t *header;

		/* take packet out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;

		header = get_ether_ip_header_ptr(&scan, FALSE);
		if (header != NULL) {
			struct in_addr  dst;
			struct ip       *iphdr;

			iphdr = (struct ip *)(header + sizeof(struct ether_header));
			bcopy(&iphdr->ip_dst, &dst, sizeof(dst));
			if (dst.s_addr == INADDR_BROADCAST) {
				bcast_pkt = copy_broadcast_packet(scan);
			}
		}
		if (bcast_pkt != NULL) {
			/* add packet to broadcast list */
			mblist_append(&ip_bcast, bcast_pkt);
		}
		if (scan != NULL) {
			/* add packet back into the list */
			mblist_append(&ret, scan);
		}
	}
	*ip_bcast_head = ip_bcast.head;
	return ret;
}

static ifnet_t
bridge_find_member(struct bridge_softc * sc, uint8_t * lladdr,
    struct bridge_iflist * sbif)
{
	struct bridge_iflist * bif;

	TAILQ_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (bif == sbif) {
			/* skip the input member */
			continue;
		}
		if (_ether_cmp(IF_LLADDR(bif->bif_ifp), lladdr) == 0) {
			return bif->bif_ifp;
		}
	}
	return NULL;
}


/*
 * Function: bridge_input_list
 *
 * Purpose:
 *   Process a list of input packets through the bridge.
 *   The caller ensures that all of the packets in the list
 *  `list_head` .. `list_tail` have the same ethernet header.
 *
 * Returns:
 *    Non-NULL head of the chain of packets that were not consumed/freed,
 *    *tail_p set to the tail of that chain.
 *
 *    NULL if all of the packets were consumed.
 */
static mblist
bridge_input_list(struct bridge_softc * sc, ifnet_t ifp,
    struct ether_header * eh_in_p, mblist list, bool is_promisc)
{
	struct bridge_iflist *  bif;
	bool                    bridge_has_address;
	ifnet_t                 bridge_ifp;
	bool                    checksum_offload;
	uint8_t *               dhost;
#if BRIDGESTP
	bool                    discarding = false;
#endif /* BRIDGESTP */
	ifnet_t                 dst_if = NULL;
	errno_t                 error;
	ether_type_flag_t       etypef;
	bool                    host_filter;
	bool                    host_filter_drop = false;
	mbuf_ref_t              ip_bcast = NULL;
	bool                    is_bridge_mac = false;
	bool                    is_broadcast;
	bool                    is_ifp_mac;
	ifnet_t                 member_input = NULL;
	uint8_t *               shost;
	uint16_t                vlan;

	if (ifp->if_bridge == NULL) {
		/* no longer part of bridge */
		goto done;
	}
	bridge_ifp = sc->sc_ifp;
	is_broadcast = IS_BCAST_MCAST(list.head);
	is_ifp_mac = (!is_broadcast && !is_promisc);
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
	    "%s from %s count %d head 0x%llx.0x%llx tail 0x%llx.0x%llx",
	    bridge_ifp->if_xname, ifp->if_xname, list.count,
	    (uint64_t)VM_KERNEL_ADDRPERM(list.head),
	    (uint64_t)VM_KERNEL_ADDRPERM(mtod(list.head, void *)),
	    (uint64_t)VM_KERNEL_ADDRPERM(list.tail),
	    (uint64_t)VM_KERNEL_ADDRPERM(mtod(list.tail, void *)));

	/* assume we'll return all packets */
	if ((bridge_ifp->if_flags & IFF_RUNNING) == 0) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s not running passing along",
		    bridge_ifp->if_xname);
		goto done;
	}

	vlan = VLANTAGOF(m);

	/* lookup the bridge member */
	BRIDGE_LOCK(sc);
	bif = bridge_lookup_member_if(sc, ifp);
	if (bif == NULL) {
		BRIDGE_UNLOCK(sc);
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s bridge_lookup_member_if failed",
		    bridge_ifp->if_xname);
		goto done;
	}

	/*
	 * host filter drops packets that:
	 * - are not ARP, IPv4, or IPv6
	 * - have incorrect source MAC address
	 */
	host_filter = (bif->bif_flags & BIFF_HOST_FILTER) != 0;
	etypef = ether_type_flag_get(eh_in_p->ether_type);
	if (host_filter
	    && (etypef & ETHER_TYPE_FLAG_IP_ARP) == 0) {
		/* ether type not one of ARP, IPv4, or IPv6 */
		BRIDGE_HF_DROP(brhf_bad_ether_type, __func__, __LINE__);
		host_filter_drop = true;
	} else if ((bif->bif_flags & BIFF_HF_HWSRC) != 0 &&
	    bcmp(eh_in_p->ether_shost, bif->bif_hf_hwsrc, ETHER_ADDR_LEN)
	    != 0) {
		/* only allow the single source MAC address */
		BRIDGE_HF_DROP(brhf_bad_ether_srchw_addr,
		    __func__, __LINE__);
		host_filter_drop = true;
	}
	if (host_filter_drop) {
		BRIDGE_UNLOCK(sc);
		m_freem_list(list.head);
		list.head = list.tail = NULL;
		goto done;
	}

#if BRIDGESTP
	discarding = (bif->bif_ifflags & IFBIF_STP) != 0 &&
	    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING;
#endif /* BRIDGESTP */

	dhost = eh_in_p->ether_dhost;
	shost = eh_in_p->ether_shost;
	/*
	 * Reserved multicast address listed in 802.1D section 7.12.6
	 * must not be forwarded by the bridge.
	 * This is currently 01-80-C2-00-00-00 to 01-80-C2-00-00-0F
	 */
	if (is_broadcast) {
		if (IS_MCAST(list.head)) {
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_MCAST,
			    " multicast: "
			    "%02x:%02x:%02x:%02x:%02x:%02x",
			    dhost[0], dhost[1],
			    dhost[2], dhost[3],
			    dhost[4], dhost[5]);
		}
		if (bcmp(dhost, bstp_etheraddr, (ETHER_ADDR_LEN - 1)) == 0) {
			if (dhost[5] == BSTP_ETHERADDR_RANGE_FIRST) {
				/* multicast for spanning tree */
#if BRIDGESTP
				bridge_bstp_input_list(&bif->bif_stp, list.head);
#else /* BRIDGESTP */
				m_freem_list(list.head);
#endif /* BRIDGESTP */
				list.head = list.tail = NULL;
				BRIDGE_UNLOCK(sc);
				goto done;
			}
			if (dhost[5] <= BSTP_ETHERADDR_RANGE_LAST) {
				/* allow packet to continue up the stack */
				BRIDGE_UNLOCK(sc);
				goto done;
			}
		}
		/* broadcast to all members */
		os_atomic_add(&bridge_ifp->if_imcasts, list.count, relaxed);
	}

#if BRIDGESTP
	if (discarding) {
		BRIDGE_UNLOCK(sc);
		goto done;
	}
#endif /* BRIDGESTP */

	/* If the interface is learning, record the address. */
	if ((bif->bif_ifflags & IFBIF_LEARNING) != 0) {
		error = bridge_rtupdate(sc, shost, vlan, bif, 0, IFBAF_DYNAMIC);
		/*
		 * If the interface has addresses limits then deny any source
		 * that is not in the cache.
		 */
		if (error != 0 && bif->bif_addrmax) {
			BRIDGE_UNLOCK(sc);
			goto done;
		}
	}
#if BRIDGESTP
	if ((bif->bif_ifflags & IFBIF_STP) != 0 &&
	    bif->bif_stp.bp_state == BSTP_IFSTATE_LEARNING) {
		BRIDGE_UNLOCK(sc);
		goto done;
	}
#endif /* BRIDGESTP */

	/*
	 * If the packet is not IP, let the host filter drop ARP packets.
	 * Otherwise, if the host filter is enabled or we need to compute
	 * checksums, do that.
	 * Otherwise, if MAC-NAT is enabled and this is an IPv4 packet,
	 * check for IPv4 broadcast packets. Accumulate those in a separate
	 * list `ip_bcast`.
	 */
	checksum_offload = bif_has_checksum_offload(bif);
	if (!ether_type_flag_is_ip(etypef)) {
		/* host filter process ARP */
		if (host_filter) {
			/* host filter check earlier means this must be ARP */
			VERIFY(etypef == ETHER_TYPE_FLAG_ARP);
			list = bridge_filter_arp_list(bif, list.head);
			if (list.head == NULL) {
				VERIFY(list.tail == NULL);
				BRIDGE_UNLOCK(sc);
				goto done;
			}
		}
	} else if (host_filter || checksum_offload) {
		/* host filter and/or checksum */
		list = bridge_filter_checksum_list(bridge_ifp, bif,
		    list.head, etypef, host_filter, checksum_offload);
		if (list.head == NULL) {
			VERIFY(list.tail == NULL);
			BRIDGE_UNLOCK(sc);
			goto done;
		}
	} else if (is_ifp_mac && bif == sc->sc_mac_nat_bif &&
	    etypef == ETHER_TYPE_FLAG_IPV4) {
		/* look for broadcast IPv4 packet */
		list = bridge_find_broadcast_ipv4(list.head, &ip_bcast);
		if (list.head == NULL && ip_bcast == NULL) {
			/* all packets were consumed */
			BRIDGE_UNLOCK(sc);
			goto done;
		}
	}

	/*
	 * If the destination MAC matches the bridge interface, and
	 * the bridge has an address assigned, or the address is
	 * distinct from the receive interface, claim the packets
	 * for the bridge interface.
	 */
	bridge_has_address = (sc->sc_flags & SCF_ADDRESS_ASSIGNED) != 0;
	if (!is_broadcast &&
	    _ether_cmp(dhost, IF_LLADDR(bridge_ifp)) == 0 &&
	    (bridge_has_address || _ether_cmp(dhost, IF_LLADDR(ifp)) != 0)) {
		is_bridge_mac = true;
	}
	if (is_ifp_mac) {
		/* unicast to the interface */
		if (sc->sc_mac_nat_bif == bif) {
			mbuf_ref_t  forward = NULL;

			if (list.head != NULL) {
				/* handle MAC-NAT if enabled */
				list = bridge_mac_nat_input_list(sc, ifp,
				    list.head, &forward);
			}
			if (ip_bcast != NULL) {
				/* forward to all members except this one */
				/* bridge_broadcast_list unlocks */
				bridge_broadcast_list(sc, bif, etypef, ip_bcast,
				    false);
			} else {
				BRIDGE_UNLOCK(sc);
			}
			if (forward != NULL) {
				bridge_mac_nat_forward_list(bridge_ifp, etypef,
				    forward);
			}
		} else {
			BRIDGE_UNLOCK(sc);
		}
		/* unicast packets for this interface do not get forwarded */
		goto done;
	}
	if (is_bridge_mac || list.head == NULL) {
		BRIDGE_UNLOCK(sc);
		goto done;
	}
	if (!is_broadcast) {
		/* find where to send the packet */
		dst_if = bridge_rtlookup(sc, dhost, vlan);
		if (ifp == dst_if) {
			/* nothing to forward */
			BRIDGE_UNLOCK(sc);
			goto done;
		}
		if (dst_if == NULL) {
			/* if a member is the dhost, deliver as input */
			member_input = bridge_find_member(sc, dhost, bif);
			if (member_input != NULL) {
				/* grab packets destined to member */
				BRIDGE_UNLOCK(sc);
				goto done;
			}
			/* if a member is shost, there's a loop, drop it */
			if (bridge_find_member(sc, shost, bif) != NULL) {
				BRIDGE_UNLOCK(sc);
				m_freem_list(list.head);
				list.head = list.tail = NULL;
				goto done;
			}
		}
	}
	if (dst_if == NULL) {
		/* bridge_broadcast_list() copies list and unlocks */
		bridge_broadcast_list(sc, bif, etypef, list.head, true);
	} else {
		/* bridge_forward_list() consumes list and unlocks */
		bridge_forward_list(sc, bif, dst_if, etypef, list.head);
		list.head = list.tail = NULL;
	}

done:
	if (list.head != NULL) {
		if (member_input != NULL) {
			/* member gets the packets */
			inject_input_packet_list(member_input, list.head, true);
			list.head = list.tail = NULL;
		} else if (is_bridge_mac) {
			/* bridge consumes all the unicast packets */
			bridge_interface_input_list(bridge_ifp, etypef, list,
			    bridge_has_address);
			list.head = list.tail = NULL;
		} else {
			adjust_input_packet_list(list.head);
		}
	}
	return list;
}

static inline void
update_mbuf_flags(struct ifnet * ifp, mbuf_t m, struct ether_header * eh)
{
	/* duplicate some of the work done in ether_demux */
	if ((eh->ether_dhost[0] & 1) == 0) {
		if (_ether_cmp(eh->ether_dhost, IF_LLADDR(ifp)) != 0) {
			m->m_flags |= M_PROMISC;
		}
	} else {
		/* Check for broadcast */
		if (_ether_cmp(etherbroadcastaddr, eh->ether_dhost) == 0) {
			m->m_flags |= M_BCAST;
		} else {
			m->m_flags |= M_MCAST;
		}
	}
	if (m->m_flags & M_HASFCS) {
		/*
		 * If the M_HASFCS is set by the driver we want to make sure
		 * that we strip off the trailing FCS data before handing it
		 * up the stack.
		 */
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}
	return;
}

static mbuf_t
bridge_pf_list(mbuf_t m, ifnet_t ifp, uint32_t sc_filter_flags, bool input)
{
	mbuf_t  next_packet = NULL;
	mblist  ret;

	mblist_init(&ret);
	for (mbuf_ref_t scan = m; scan != NULL; scan = next_packet) {
		next_packet = scan->m_nextpkt;

		/* remove packet from list, and pass through PF */
		scan->m_nextpkt = NULL;
		MBUF_INPUT_CHECK(scan, ifp);
		bridge_pf(&scan, ifp, sc_filter_flags, input);
		if (scan != NULL) {
			/* add packet back to the list */
			mblist_append(&ret, scan);
		}
	}
	return ret.head;
}

static inline bool
get_and_clear_promisc(mbuf_t m)
{
	bool    is_promisc;

	/*
	 * Need to clear the promiscuous flag otherwise the packet will be
	 * dropped by DLIL after processing filters
	 */
	is_promisc = (mbuf_flags(m) & MBUF_PROMISC) != 0;
	if (is_promisc) {
		mbuf_setflags_mask(m, 0, MBUF_PROMISC);
	}
	return is_promisc;
}

static inline bool
bridge_check_frame_header(struct bridge_softc * sc, ifnet_t ifp, mbuf_t m)
{
	bool                    included = false;
	char * __single         header;
	size_t                  header_length = 0;

	header = m->m_pkthdr.pkt_hdr;
	if (header >= (char *)mbuf_datastart(m) &&
	    header <= mtod(m, char *)) {
		header_length = mtod(m, char *) - header;
		if (header_length >= ETHER_HDR_LEN) {
			included = true;
		}
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
	    "%s from %s m 0x%llx data 0x%llx frame 0x%llx %s "
	    "header length %lu", sc->sc_ifp->if_xname,
	    ifp->if_xname, (uint64_t)VM_KERNEL_ADDRPERM(m),
	    (uint64_t)VM_KERNEL_ADDRPERM(mtod(m, void *)),
	    (uint64_t)VM_KERNEL_ADDRPERM(header),
	    included ? "inside" : "outside", header_length);
	if (!included) {
		BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT,
		    "%s: frame_header outside mbuf", ifp->if_xname);
	}
	return included;
}


mbuf_t
bridge_early_input(struct ifnet *ifp, mbuf_t in_list, u_int32_t cnt)
{
	struct ether_header eh;
	mblist          list;
	volatile bool   list_is_promisc;
	int             n_lists = 0;
	mbuf_t          next_packet = NULL;
	mblist          ret;
	struct bridge_softc * __single sc = ifp->if_bridge;
	uint32_t        sc_filter_flags;

	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT_LIST,
	    "(%s): count %u", ifp->if_xname, cnt);

	/* run packet list through PF first */
	sc_filter_flags = sc->sc_filter_flags;
	if (PF_IS_ENABLED && (sc_filter_flags & IFBF_FILT_MEMBER)) {
		in_list = bridge_pf_list(in_list, ifp, sc_filter_flags, true);
	}

	/* form sublists with the same ethernet header */
	mblist_init(&list);
	mblist_init(&ret);
	for (mbuf_t scan = in_list; scan != NULL; scan = next_packet) {
		struct ether_header *   eh_p;
		volatile bool           is_promisc;
		mblist                  resid;

		/* take it out of the list */
		next_packet = scan->m_nextpkt;
		scan->m_nextpkt = NULL;

		/* don't loop the packet */
		if ((scan->m_flags & M_PROTO1) != 0) {
			mblist_append(&ret, scan);
			continue;
		}
		/* Check if this mbuf looks valid */
		MBUF_INPUT_CHECK(scan, ifp);

		/* if the frame header isn't in the first mbuf, ignore */
		if (!bridge_check_frame_header(sc, ifp, scan)) {
			mblist_append(&ret, scan);
			continue;
		}
		eh_p = __unsafe_forge_single(struct ether_header *,
		    scan->m_pkthdr.pkt_hdr);
		update_mbuf_flags(ifp, scan, eh_p);

		/* set start back to include ether header */
		_mbuf_adjust_pkthdr_and_data(scan, -ETHER_HDR_LEN);

		is_promisc = get_and_clear_promisc(scan);
		if (list.head == NULL) {
			/* start a new list */
			mblist_append(&list, scan);
			bcopy(eh_p, &eh, sizeof(eh));
			list_is_promisc = is_promisc;
		} else if (bcmp(eh_p, &eh, sizeof(eh)) != 0) {
			n_lists++;
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT_LIST,
			    "(%s): sublist %u pkts %u",
			    ifp->if_xname, n_lists, list.count);
			if (BRIDGE_DBGF_ENABLED(BR_DBGF_INPUT_LIST)) {
				brlog_ether_header(&eh);
			}
			resid = bridge_input_list(sc, ifp, &eh, list,
			    list_is_promisc);
			if (resid.head != NULL) {
				/* add to the packets to be returned */
				mblist_append_list(&ret, resid);
			}
			/* start new list */
			mblist_init(&list);
			mblist_append(&list, scan);
			list_is_promisc = is_promisc;
			bcopy(eh_p, &eh, sizeof(eh));
		} else {
			mblist_append(&list, scan);
			VERIFY(is_promisc == list_is_promisc);
		}
		if (next_packet == NULL) {
			/* last list */
			n_lists++;
			BRIDGE_LOG(LOG_DEBUG, BR_DBGF_INPUT_LIST,
			    "(%s): sublist %u pkts %u",
			    ifp->if_xname, n_lists, list.count);
			if (BRIDGE_DBGF_ENABLED(BR_DBGF_INPUT_LIST)) {
				brlog_ether_header(&eh);
			}
			resid = bridge_input_list(sc, ifp, &eh, list,
			    list_is_promisc);
			if (resid.head != NULL) {
				/* add to the packets to be returned */
				mblist_append_list(&ret, resid);
			}
		}
	}
	return ret.head;
}

/*
 * Copyright (C) 2014, Stefano Garzarella - Universita` di Pisa.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * XXX-ste: Maybe this function must be moved into kern/uipc_mbuf.c
 *
 * Create a queue of packets/segments which fit the given mss + hdr_len.
 * m0 points to mbuf chain to be segmented.
 * This function splits the payload (m0-> m_pkthdr.len - hdr_len)
 * into segments of length MSS bytes and then copy the first hdr_len bytes
 * from m0 at the top of each segment.
 * If hdr2_buf is not NULL (hdr2_len is the buf length), it is copied
 * in each segment after the first hdr_len bytes
 *
 * Return the new queue with the segments on success, NULL on failure.
 * (the mbuf queue is freed in this case).
 */

static mblist
m_seg(struct mbuf *m0, int hdr_len, int mss, char * hdr2_buf, int hdr2_len)
{
	int off = 0, n, firstlen;
	struct mbuf *mseg;
	int total_len = m0->m_pkthdr.len;
	mblist ret;

	mblist_init(&ret);
	mblist_append(&ret, m0);

	/*
	 * Segmentation useless
	 */
	if (total_len <= hdr_len + mss) {
		n = 1;
		goto done;
	}

	if (hdr2_buf == NULL || hdr2_len <= 0) {
		hdr2_buf = NULL;
		hdr2_len = 0;
	}

	off = hdr_len + mss;
	firstlen = mss; /* first segment stored in the original mbuf */

	for (n = 1; off < total_len; off += mss, n++) {
		struct mbuf *m;
		/*
		 * Copy the header from the original packet
		 * and create a new mbuf chain
		 */
		if (MHLEN < hdr_len) {
			m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		} else {
			m = m_gethdr(M_NOWAIT, MT_DATA);
		}

		if (m == NULL) {
#ifdef GSO_DEBUG
			D("MGETHDR error\n");
#endif
			goto err;
		}

		m_copydata(m0, 0, hdr_len, mtod(m, caddr_t));

		m->m_len = hdr_len;
		/*
		 * if the optional header is present, copy it
		 */
		if (hdr2_buf != NULL) {
			m_copyback(m, hdr_len, hdr2_len, hdr2_buf);
		}

		m->m_flags |= (m0->m_flags & M_COPYFLAGS);
		if (off + mss >= total_len) {           /* last segment */
			mss = total_len - off;
		}
		/*
		 * Copy the payload from original packet
		 */
		mseg = m_copym(m0, off, mss, M_NOWAIT);
		if (mseg == NULL) {
			m_freem(m);
#ifdef GSO_DEBUG
			D("m_copym error\n");
#endif
			goto err;
		}
		m_cat(m, mseg);

		m->m_pkthdr.len = hdr_len + hdr2_len + mss;
		m->m_pkthdr.rcvif = m0->m_pkthdr.rcvif;
		/*
		 * Copy the checksum flags and data (in_cksum() need this)
		 */
		m->m_pkthdr.csum_flags = m0->m_pkthdr.csum_flags;
		m->m_pkthdr.csum_data = m0->m_pkthdr.csum_data;
		m->m_pkthdr.tso_segsz = m0->m_pkthdr.tso_segsz;

		mblist_append(&ret, m);
	}

	/*
	 * Update first segment.
	 * If the optional header is present, is necessary
	 * to insert it into the first segment.
	 */
	if (hdr2_buf == NULL) {
		m_adj(m0, hdr_len + firstlen - total_len);
		m0->m_pkthdr.len = hdr_len + firstlen;
	} else {
		mseg = m_copym(m0, hdr_len, firstlen, M_NOWAIT);
		if (mseg == NULL) {
#ifdef GSO_DEBUG
			D("m_copym error\n");
#endif
			goto err;
		}
		m_adj(m0, hdr_len - total_len);
		m_copyback(m0, hdr_len, hdr2_len, hdr2_buf);
		m_cat(m0, mseg);
		m0->m_pkthdr.len = hdr_len + hdr2_len + firstlen;
	}

done:
	return ret;

err:
	if (ret.head != NULL) {
		m_freem_list(ret.head);
		mblist_init(&ret);
	}
	return ret;
}

/*
 * Wrappers of IPv4 checksum functions
 */
static inline void
gso_ipv4_data_cksum(struct mbuf *m, struct ip *ip, int mac_hlen)
{
	m->m_data += mac_hlen;
	m->m_len -= mac_hlen;
	m->m_pkthdr.len -= mac_hlen;
#if __FreeBSD_version < 1000000
	ip->ip_len = ntohs(ip->ip_len); /* needed for in_delayed_cksum() */
#endif

	in_delayed_cksum(m);

#if __FreeBSD_version < 1000000
	ip->ip_len = htons(ip->ip_len);
#endif
	m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	m->m_len += mac_hlen;
	m->m_pkthdr.len += mac_hlen;
	m->m_data -= mac_hlen;
}

static inline void
gso_ipv4_hdr_cksum(struct mbuf *m, struct ip *ip, int mac_hlen, int ip_hlen)
{
	m->m_data += mac_hlen;

	ip->ip_sum = in_cksum(m, ip_hlen);

	m->m_pkthdr.csum_flags &= ~CSUM_IP;
	m->m_data -= mac_hlen;
}

/*
 * Structure that contains the state during the TCP segmentation
 */
struct gso_ip_tcp_state {
	void    (*update)
	(struct gso_ip_tcp_state*, struct mbuf*);
	void    (*internal)
	(struct gso_ip_tcp_state*, struct mbuf*);
	u_int ip_m0_len;
	uint8_t * __counted_by(ip_m0_len) hdr;
	struct tcphdr *tcp;
	int mac_hlen;
	int ip_hlen;
	int tcp_hlen;
	int hlen;
	int pay_len;
	int sw_csum;
	uint32_t tcp_seq;
	uint16_t ip_id;
	boolean_t is_tx;
};

/*
 * Update the pointers to TCP and IPv4 headers
 */
static inline void
gso_ipv4_tcp_update(struct gso_ip_tcp_state *state, struct mbuf *m)
{
	state->hdr = mtodo(m, state->mac_hlen);
	state->ip_m0_len = m->m_len - state->mac_hlen;
	state->ip_hlen = state->ip_hlen;
	state->tcp = (struct tcphdr *)(state->hdr + state->ip_hlen);
	state->pay_len = m->m_pkthdr.len - state->hlen;
}

/*
 * Set properly the TCP and IPv4 headers
 */
static inline void
gso_ipv4_tcp_internal(struct gso_ip_tcp_state *state, struct mbuf *m)
{
	struct ip *ip;
	/*
	 * Update IP header
	 */
	ip = (struct ip *)state->hdr;
	ip->ip_id = htons((state->ip_id)++);
	ip->ip_len = htons(m->m_pkthdr.len - state->mac_hlen);
	/*
	 * TCP Checksum
	 */
	state->tcp->th_sum = 0;
	state->tcp->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
	    htons(state->tcp_hlen + IPPROTO_TCP + state->pay_len));
	/*
	 * Checksum HW not supported (TCP)
	 */
	if (state->sw_csum & CSUM_DELAY_DATA) {
		gso_ipv4_data_cksum(m, ip, state->mac_hlen);
	}

	state->tcp_seq += state->pay_len;
	/*
	 * IP Checksum
	 */
	ip->ip_sum = 0;
	/*
	 * Checksum HW not supported (IP)
	 */
	if (state->sw_csum & CSUM_IP) {
		gso_ipv4_hdr_cksum(m, ip, state->mac_hlen, state->ip_hlen);
	}
}


/*
 * Updates the pointers to TCP and IPv6 headers
 */
static inline void
gso_ipv6_tcp_update(struct gso_ip_tcp_state *state, struct mbuf *m)
{
	state->hdr = mtodo(m, state->mac_hlen);
	state->ip_m0_len = m->m_len - state->mac_hlen;
	state->ip_hlen = state->ip_hlen;
	state->tcp = (struct tcphdr *)(state->hdr + state->ip_hlen);
	state->pay_len = m->m_pkthdr.len - state->hlen;
}

/*
 * Sets properly the TCP and IPv6 headers
 */
static inline void
gso_ipv6_tcp_internal(struct gso_ip_tcp_state *state, struct mbuf *m)
{
	struct ip6_hdr *ip6;

	ip6 = (struct ip6_hdr *)state->hdr;
	ip6->ip6_plen = htons(m->m_pkthdr.len - state->mac_hlen - state->ip_hlen);
	/*
	 * TCP Checksum
	 */
	state->tcp->th_sum = 0;
	state->tcp->th_sum = in6_pseudo(&ip6->ip6_src, &ip6->ip6_dst,
	    htonl(state->tcp_hlen + state->pay_len + IPPROTO_TCP));
	/*
	 * Checksum HW not supported (TCP)
	 */
	if (state->sw_csum & CSUM_DELAY_IPV6_DATA) {
		(void)in6_finalize_cksum(m, state->mac_hlen, -1, -1, state->sw_csum);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_IPV6_DATA;
	}
	state->tcp_seq += state->pay_len;
}

/*
 * Init the state during the TCP segmentation
 */
static void
gso_ip_tcp_init_state(struct gso_ip_tcp_state *state, struct ifnet *ifp,
    bool is_ipv4, int mac_hlen, int ip_hlen,
    uint8_t *__counted_by(ip_m0_len) ip_hdr, u_int ip_m0_len,
    struct tcphdr * tcp_hdr)
{
#pragma unused(ifp)

	state->hdr = ip_hdr;
	state->ip_m0_len = ip_m0_len;
	state->ip_hlen = ip_hlen;
	state->tcp = tcp_hdr;
	if (is_ipv4) {
		state->ip_id = ntohs(((struct ip *)state->hdr)->ip_id);
		state->update = gso_ipv4_tcp_update;
		state->internal = gso_ipv4_tcp_internal;
		state->sw_csum = CSUM_DELAY_DATA | CSUM_IP; /* XXX */
	} else {
		state->update = gso_ipv6_tcp_update;
		state->internal = gso_ipv6_tcp_internal;
		state->sw_csum = CSUM_DELAY_IPV6_DATA; /* XXX */
	}
	state->mac_hlen = mac_hlen;
	state->tcp_hlen = state->tcp->th_off << 2;
	state->hlen = mac_hlen + ip_hlen + state->tcp_hlen;
	state->tcp_seq = ntohl(state->tcp->th_seq);
	//state->sw_csum = m->m_pkthdr.csum_flags & ~IF_HWASSIST_CSUM_FLAGS(ifp->if_hwassist);
	return;
}

/*
 * GSO on TCP/IP (v4 or v6)
 *
 * Segment the given mbuf and return the list of packets.
 *
 */
static mblist
gso_ip_tcp(ifnet_t ifp, mbuf_t m0, struct gso_ip_tcp_state *state, bool is_tx)
{
	struct mbuf *m;
	int mss = 0;
#ifdef GSO_STATS
	int total_len = m0->m_pkthdr.len;
#endif /* GSO_STATS */
	mblist seg;

#if 1
	/* hack: we don't get MSS via packet metadata */
	u_int reduce_mss;

	reduce_mss = is_tx ? if_bridge_tso_reduce_mss_tx
	    : if_bridge_tso_reduce_mss_forwarding;
	mss = ifp->if_mtu - state->ip_hlen - state->tcp_hlen - reduce_mss;
	assert(mss > 0);
#else
	if (m0->m_pkthdr.csum_flags & ifp->if_hwassist & CSUM_TSO) {/* TSO with GSO */
		mss = ifp->if_hw_tsomax - state->ip_hlen - state->tcp_hlen;
	} else {
		mss = m0->m_pkthdr.tso_segsz;
	}
#endif
	seg = m_seg(m0, state->hlen, mss, 0, 0);
	if (seg.head == NULL || seg.head->m_nextpkt == NULL) {
		return seg;
	}
	BRIDGE_LOG(LOG_DEBUG, BR_DBGF_CHECKSUM,
	    "%s %s mss %d nsegs %d",
	    ifp->if_xname,
	    is_tx ? "TX" : "RX",
	    mss, seg.count);
#ifdef GSO_STATS
	GSOSTAT_SET_MAX(tcp.gsos_max_mss, mss);
	GSOSTAT_SET_MIN(tcp.gsos_min_mss, mss);
	GSOSTAT_ADD(tcp.gsos_osegments, seg.count);
#endif /* GSO_STATS */

	/* first pkt */
	VERIFY(seg.head == m0);
	m = m0;

	state->update(state, m);

	do {
		state->tcp->th_flags &= ~(TH_FIN | TH_PUSH);

		state->internal(state, m);
		m = m->m_nextpkt;
		state->update(state, m);
		state->tcp->th_flags &= ~TH_CWR;
		state->tcp->th_seq = htonl(state->tcp_seq);
	} while (m->m_nextpkt);

	/* last pkt */
	state->internal(state, m);

#ifdef GSO_STATS
	if (!error) {
		GSOSTAT_INC(tcp.gsos_segmented);
		GSOSTAT_SET_MAX(tcp.gsos_maxsegmented, total_len);
		GSOSTAT_SET_MIN(tcp.gsos_minsegmented, total_len);
		GSOSTAT_ADD(tcp.gsos_totalbyteseg, total_len);
	}
#endif /* GSO_STATS */
	return seg;
}

/*
 * GSO for TCP/IPv[46]
 */
static mblist
gso_tcp(ifnet_t ifp, mbuf_t m, u_int mac_hlen, bool is_ipv4, bool is_tx)
{
	int error;
	ip_packet_info  info;
	uint32_t csum_flags;
	struct gso_ip_tcp_state state;
	struct bripstats stats; /* XXX ignored */
	struct tcphdr *tcp;
	mblist ret;

	mblist_init(&ret);
	error = bridge_get_tcp_header(&m, mac_hlen, is_ipv4, &info, &stats);
	if (error != 0) {
		if (m != NULL) {
			m_freem(m);
			m = NULL;
		}
		goto no_segment;
	}
	if (info.ip_proto_hdr == NULL) {
		/* not actually a TCP packet, no segmentation */
		goto no_segment;
	}
	tcp = (struct tcphdr *)(void *)info.ip_proto_hdr;
	gso_ip_tcp_init_state(&state, ifp, is_ipv4, mac_hlen,
	    info.ip_hlen + info.ip_opt_len, info.ip_hdr, info.ip_m0_len, tcp);
	if (is_ipv4) {
		csum_flags = CSUM_DELAY_DATA; /* XXX */
		if (!is_tx) {
			/* if RX to our local IP address, don't segment */
			struct in_addr  dst_ip;

			bcopy(&((struct ip *)state.hdr)->ip_dst, &dst_ip, sizeof(dst_ip));
			if (in_addr_is_ours(dst_ip)) {
				goto no_segment;
			}
		}
	} else {
		csum_flags = CSUM_DELAY_IPV6_DATA; /* XXX */
		if (!is_tx) {
			/* if RX to our local IP address, don't segment */
			if (in6_addr_is_ours(&((struct ip6_hdr *)state.hdr)->ip6_dst,
			    ifp->if_index)) {
				/* local IP address, no need to segment */
				goto no_segment;
			}
		}
	}
	m->m_pkthdr.csum_flags = csum_flags;
	m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
	return gso_ip_tcp(ifp, m, &state, is_tx);

no_segment:
	if (m != NULL) {
		mblist_append(&ret, m);
	}
	return ret;
}
