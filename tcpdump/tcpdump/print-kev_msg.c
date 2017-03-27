/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <sys/types.h>
#include <sys/kern_event.h>
#include <sys/kern_control.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <stdio.h>
#include <stddef.h>
#include <arpa/inet.h>

#include <netdissect-stdinc.h>

#include "netdissect.h"
#include "interface.h"

static const char *str_kev_class[] = {
	"0",
	"net",
	"iokit",
	"systm",
	"afp",
	"fw",
	"ieee80211",
	NULL
};

static const char *str_kev_systm_subclass[] = {
	"0",
	"1",
	"ctl",
	"memory status",
	NULL
};

static const char *str_kev_net_subclass[] = {
	"0",
	"inet",
	"dl",
	"3",
	"4",
	"atalk",
	"inet6",
	"nd6",
	"8",
	"9",
	"log",
	NULL
};

static const char *str_kev_net_dl[] = {
	"0",
	"if flags changed",
	"if metric changed",
	"if mtu changed",
	"if phys changed",
	"if media changed",
	"if generic changed",
	"add multi",
	"del multi",
	"if attached",
	"if detaching",
	"if detached",
	"link off",
	"link on",
	"attached",
	"detach",
	"laddr changed",
	"wake flags changed",
	"idle route refcnt changed",
	"ifcap changed",
	"lqm",
	"node presence",
	"node absence",
	"master elected",
	"dl issue",
	"if delegate changed",
	"awdl restricted",
	"awdl unrestricted",
	"rrc state",
	NULL
};

static const char *str_kev_net_inet[] = {
	"0",
	"new addr",
	"changed addr",
	"addr deleted",
	"set if dst addr",
	"set if brd addr",
	"set if netmask",
	"arp collision",
	"port inuse",
	"arp rtr failure",
	"arp rtr alive",
	NULL
};

static const char *str_kev_net_inet6[] = {
	"0",
	"new user addr",
	"changed addr",
	"addr deleted",
	"new lladdr",
	"new rtavd addr",
	"def router",
	NULL
};

static const char *str_kev_net_nd6[] = {
	"0",
	"ra",
	NULL
};

static const char *str_if_family[] = {
	"any",
	"loopback",
	"ethernet",
	"slip",
	"tun",
	"vlan",
	"ppp",
	"pvc",
	"disc",
	"mdecap",
	"gif",
	"faith",
	"sft",
	"firewire",
	"bond",
	"cellular",
	NULL
};

#define IFF_STR(x) { x, #x }

struct iff_str {
	uint32_t flag;
	const char *str;
};

static struct iff_str in6_iff_strs[] = {
	{ IN6_IFF_ANYCAST, "anycast" },
	{ IN6_IFF_TENTATIVE, "tentative" },
	{ IN6_IFF_DUPLICATED, "tentative" },
	{ IN6_IFF_DETACHED, "tentative" },
	{ IN6_IFF_DEPRECATED, "tentative" },
	{ IN6_IFF_NODAD, "no dad" },
	{ IN6_IFF_AUTOCONF, "autoconf" },
	{ IN6_IFF_TEMPORARY, "temporary" },
	{ IN6_IFF_DYNAMIC, "dynamic" },
	{ IN6_IFF_OPTIMISTIC, "optimistic" },
	{ IN6_IFF_SECURED, "secured" },
	{ IN6_IFF_SWIFTDAD, "swiftdad" },
	{ IN6_IFF_NOPFX, "nopfx" },
	{ 0, NULL },
};

static const char *
get_str_from_index(const char **array, int index)
{
	int		i= 0;
	const char	*s;

	while ((s = array[i]) != NULL && i < index) {
		i++;
	}
	return (s);
}

static const char *
get_str_from_ifflag(struct iff_str *array, uint32_t flag)
{
	int i = 0;
	const char *s = "";

	while (array[i].str != NULL) {
		if (flag == array[i].flag) {
			s = array[i].str;
			break;
		}
		i++;
	}
	return (s);
}

static void
print_in6_ifflags(struct netdissect_options *ndo, uint32_t flags)
{
	int bit = 0x1;
	size_t len = 0;

	while (bit) {
		const char *s = get_str_from_ifflag(in6_iff_strs, bit & flags);
		if (s != NULL && *s != 0) {
			ND_PRINT((ndo, "%s%s",len > 0 ? "|" : "", s));
		}
		bit = bit << 1;
	}
}

static void
print_lqm(struct netdissect_options *ndo, int lqm)
{
	ND_PRINT((ndo, " %d ", lqm));
	if (lqm == IFNET_LQM_THRESH_OFF) {
		ND_PRINT((ndo, "(off)"));
	} else if (lqm == IFNET_LQM_THRESH_UNKNOWN) {
		ND_PRINT((ndo, "(unknown)"));
	} else if (lqm == IFNET_LQM_THRESH_BAD) {
		ND_PRINT((ndo, "(bad)"));
	} else if (lqm == IFNET_LQM_THRESH_POOR) {
		ND_PRINT((ndo, "(poor)"));
	} else if (lqm == IFNET_LQM_THRESH_GOOD) {
		ND_PRINT((ndo, "(good)"));
	} else {
		ND_PRINT((ndo, "(?)"));
	}
}

static void
print_node_presence(struct netdissect_options *ndo, struct kev_dl_node_presence *kev)
{
	char sin6_node_address[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, &kev->sin6_node_address.sin6_addr.s6_addr,
		  sin6_node_address, sizeof(sin6_node_address));

	ND_PRINT((ndo, " inet6 %d", sin6_node_address));

	switch (kev->sdl_node_address.sdl_type)  {
		case IFT_ETHER:
		case IFT_FDDI:
			ND_PRINT((ndo, " lladdr %s",
				  ether_ntoa((struct ether_addr *)
					     LLADDR(&kev->sdl_node_address))));
			break;
		default:
			ND_PRINT((ndo, " lladdr type %u",
				  kev->sdl_node_address.sdl_type));
			break;
	}
	ND_PRINT((ndo,
		  "rrsi %d lqm %d proximity %d",
		  kev->rssi,
		  kev->link_quality_metric,
		  kev->node_proximity_metric));
}

static void
print_node_absence(struct netdissect_options *ndo, struct kev_dl_node_absence *kev)
{
	char sin6_node_address[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, &kev->sin6_node_address.sin6_addr.s6_addr,
		  sin6_node_address, sizeof(sin6_node_address));

	ND_PRINT((ndo, " inet6 %d", sin6_node_address));

	switch (kev->sdl_node_address.sdl_type)  {
		case IFT_ETHER:
		case IFT_FDDI:
			ND_PRINT((ndo, " lladdr %s",
				  ether_ntoa((struct ether_addr *)
					     LLADDR(&kev->sdl_node_address))));
			break;
		default:
			ND_PRINT((ndo, " lladdr type %u",
				  kev->sdl_node_address.sdl_type));
			break;
	}
}

void
print_kev_msg(struct netdissect_options *ndo, struct kern_event_msg *kev_msg)
{
	const char *event_code_str;

	if (kev_msg->vendor_code != KEV_VENDOR_APPLE) {
		ND_PRINT((ndo, "kev size %u vendor %u class %u subclass %u code %u",
			  kev_msg->total_size,
			  kev_msg->vendor_code,
			  kev_msg->kev_class,
			  kev_msg->kev_subclass,
			  kev_msg->event_code));
		return;
	}

	if (kev_msg->kev_class == KEV_NETWORK_CLASS) {
		struct net_event_data *net_data = (struct net_event_data *) &kev_msg->event_data[0];
		const char *if_family_str = get_str_from_index(str_if_family, net_data->if_family);

		ND_PRINT((ndo, "kev %s%d ", net_data->if_name, net_data->if_unit));

		if (ndo->ndo_vflag > 1) {
			ND_PRINT((ndo, "%s", if_family_str ? if_family_str : "?"));
		}
		if (kev_msg->kev_subclass == KEV_INET_SUBCLASS) {
			struct kev_in_data   *in_data = (struct kev_in_data *) &kev_msg->event_data[0];
			char ia_addr[INET_ADDRSTRLEN];
			char ia_net[INET_ADDRSTRLEN];
			char ia_netmask[INET_ADDRSTRLEN];
			char ia_subnet[INET_ADDRSTRLEN];
			char ia_subnetmask[INET_ADDRSTRLEN];
			char ia_netbroadcast[INET_ADDRSTRLEN];
			char ia_dstaddr[INET_ADDRSTRLEN];
			in_addr_t addr;

			event_code_str = get_str_from_index(str_kev_net_inet, kev_msg->event_code);

			inet_ntop(AF_INET, &in_data->ia_addr.s_addr, ia_addr, sizeof(ia_addr));
			addr = htonl(in_data->ia_net);
			inet_ntop(AF_INET, &addr, ia_net, sizeof(ia_net));
			addr = htonl(in_data->ia_netmask);
			inet_ntop(AF_INET, &addr, ia_netmask, sizeof(ia_netmask));
			addr = htonl(in_data->ia_subnet);
			inet_ntop(AF_INET, &addr, ia_subnet, sizeof(ia_subnet));
			addr = htonl(in_data->ia_subnetmask);
			inet_ntop(AF_INET, &addr, ia_subnetmask, sizeof(ia_subnetmask));
			inet_ntop(AF_INET, &in_data->ia_netbroadcast.s_addr, ia_netbroadcast, sizeof(ia_netbroadcast));
			inet_ntop(AF_INET, &in_data->ia_dstaddr.s_addr, ia_dstaddr, sizeof(ia_dstaddr));

			ND_PRINT((ndo,
				  "inet %s %s net %s netmask %s subnet %s subnetmask %s netbroadcast %s dstaddr %s",
				  event_code_str ? event_code_str : "?",
				  ia_addr,
				  ia_net,
				  ia_netmask,
				  ia_subnet,
				  ia_subnetmask,
				  ia_netbroadcast,
				  ia_dstaddr));
		} else if (kev_msg->kev_subclass == KEV_INET6_SUBCLASS) {
			struct kev_in6_data   *in6_data = (struct kev_in6_data *) &kev_msg->event_data[0];
			char ia_addr[INET6_ADDRSTRLEN];
			char ia_net[INET6_ADDRSTRLEN];
			char ia_dstaddr[INET6_ADDRSTRLEN];
			char ia_prefixmask[INET6_ADDRSTRLEN];

			event_code_str = get_str_from_index(str_kev_net_inet6, kev_msg->event_code);

			inet_ntop(AF_INET6, &in6_data->ia_addr.sin6_addr.s6_addr, ia_addr, sizeof(ia_addr));
			inet_ntop(AF_INET6, &in6_data->ia_net.sin6_addr.s6_addr, ia_net, sizeof(ia_net));
			inet_ntop(AF_INET6, &in6_data->ia_dstaddr.sin6_addr.s6_addr, ia_dstaddr, sizeof(ia_dstaddr));
			inet_ntop(AF_INET6, &in6_data->ia_prefixmask.sin6_addr.s6_addr, ia_prefixmask, sizeof(ia_prefixmask));

			ND_PRINT((ndo,
				  "kev inet6 %s %s net %s dstaddr %s prefixmask %s plen %u",
				  event_code_str ? event_code_str : "?",
				  ia_addr,
				  ia_net,
				  ia_dstaddr,
				  ia_prefixmask,
				  in6_data->ia_plen));
			print_in6_ifflags(ndo, in6_data->ia6_flags);
			ND_PRINT((ndo,
				  " expire %u preferred %u vltime %u pltime %u",
			       in6_data->ia_lifetime.ia6t_expire,
			       in6_data->ia_lifetime.ia6t_preferred,
			       in6_data->ia_lifetime.ia6t_vltime,
			       in6_data->ia_lifetime.ia6t_pltime));
		} else if (kev_msg->kev_subclass == KEV_DL_SUBCLASS) {
			event_code_str = get_str_from_index(str_kev_net_dl, kev_msg->event_code);

			ND_PRINT((ndo,
				  "kev %s",
				  event_code_str ? event_code_str : "?"));

			switch (kev_msg->event_code) {
				case KEV_DL_PROTO_ATTACHED:
				case KEV_DL_PROTO_DETACHED: {
					struct kev_dl_proto_data *ev_pr_data;

					ev_pr_data = (struct kev_dl_proto_data *)net_data;

					ND_PRINT((ndo,
						  " proto %u remaining %u",
						  ev_pr_data->proto_family,
						  ev_pr_data->proto_remaining_count));
					break;
				}
				case KEV_DL_LINK_QUALITY_METRIC_CHANGED: {
					struct kev_dl_link_quality_metric_data *ev_lqm_data;

					ev_lqm_data = (struct kev_dl_link_quality_metric_data *)net_data;

					print_lqm(ndo, ev_lqm_data->link_quality_metric);
					break;
				}
				case KEV_DL_NODE_PRESENCE: {
					print_node_presence(ndo, (struct kev_dl_node_presence *)net_data);
					break;
				}
				case KEV_DL_NODE_ABSENCE: {
					print_node_absence(ndo, (struct kev_dl_node_absence *)net_data);
					break;
				}
				default:
					break;
			}
		} else if (kev_msg->event_code == KEV_ND6_SUBCLASS) {
			event_code_str = get_str_from_index(str_kev_net_nd6, kev_msg->event_code);

			ND_PRINT((ndo,
				  "kev nd6 %s",
				  event_code_str ? event_code_str : "?"));
		} else {
			const char *str_subclass = get_str_from_index(str_kev_net_subclass, kev_msg->kev_subclass);

			if (str_subclass != NULL) {
				ND_PRINT((ndo, "kev net %s", str_subclass));
			} else {
				ND_PRINT((ndo, "kev net subclass %u (?)", kev_msg->kev_subclass));
			}
		}
	} else if (kev_msg->kev_class == KEV_SYSTEM_CLASS) {
		const char *str_subclass = get_str_from_index(str_kev_systm_subclass, kev_msg->kev_subclass);

		if (str_subclass != NULL) {
			ND_PRINT((ndo, "kev systm %s", str_subclass));
		} else {
			ND_PRINT((ndo, "kev systm subclass %u (?)", kev_msg->kev_subclass));
		}
	} else {
		const char *str_class = get_str_from_index(str_kev_class, kev_msg->kev_class);

		if (str_class != NULL) {
			ND_PRINT((ndo, "kev %s", str_class));
		} else {
			ND_PRINT((ndo, "kev class %u (?)", kev_msg->kev_class));
		}
	}
}
