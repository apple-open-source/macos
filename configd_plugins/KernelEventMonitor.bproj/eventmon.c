/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
 *
 * December 3, 2002		Dieter Siegmund <dieter@apple.com>
 * - handle the new KEV_INET_ARPCOLLISION event
 * - format the event into a DynamicStore key
 *     State:/Network/Interface/ifname/IPv4Collision/ip_addr/hw_addr
 *   and send a notification on the key
 *
 * January 6, 2002		Jessica Vazquez <vazquez@apple.com>
 * - added handling for KEV_ATALK_xxx events
 *
 * July 2, 2001			Dieter Siegmund <dieter@apple.com>
 * - added handling for KEV_DL_PROTO_{ATTACHED, DETACHED}
 * - mark an interface up if the number of protocols remaining is not 0,
 *   mark an interface down if the number is zero
 * - allocate socket on demand instead of keeping it open all the time
 *
 * June 23, 2001		Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
 *
 * May 17, 2001			Allan Nathanson <ajn@apple.com>
 * - add/maintain per-interface address/netmask/destaddr information
 *   in the dynamic store.
 *
 * June 30, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <sys/kern_event.h>
#include <errno.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netat/appletalk.h>
#include <netat/at_var.h>

// from <netat/ddp.h>
#define DDP_MIN_NETWORK         0x0001
#define DDP_MAX_NETWORK         0xfffe

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#ifndef kSCEntNetIPv4ARPCollision
#define kSCEntNetIPv4ARPCollision	CFSTR("IPv4ARPCollision")
#endif kSCEntNetIPv4ARPCollision

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip, i)	(((u_char *)(ip))[i])
#define IP_LIST(ip)	IP_CH(ip,0),IP_CH(ip,1),IP_CH(ip,2),IP_CH(ip,3)

#ifndef kSCPropNetAppleTalkNetworkRange
#define kSCPropNetAppleTalkNetworkRange	SCSTR("NetworkRange") 	/* CFArray[CFNumber] */
#endif	/* kSCPropNetAppleTalkNetworkRange */

const char *inetEventName[] = {
	"",
	"INET address added",
	"INET address changed",
	"INET address deleted",
	"INET destination address changed",
	"INET broadcast address changed",
	"INET netmask changed",
	"INET ARP collision",
};

const char *dlEventName[] = {
	"",
	"KEV_DL_SIFFLAGS",
	"KEV_DL_SIFMETRICS",
	"KEV_DL_SIFMTU",
	"KEV_DL_SIFPHYS",
	"KEV_DL_SIFMEDIA",
	"KEV_DL_SIFGENERIC",
	"KEV_DL_ADDMULTI",
	"KEV_DL_DELMULTI",
	"KEV_DL_IF_ATTACHED",
	"KEV_DL_IF_DETACHING",
	"KEV_DL_IF_DETACHED",
	"KEV_DL_LINK_OFF",
	"KEV_DL_LINK_ON",
	"KEV_DL_PROTO_ATTACHED",
	"KEV_DL_PROTO_DETACHED",
};

const char *atalkEventName[] = {
	"",
	"KEV_ATALK_ENABLED",
	"KEV_ATALK_DISABLED",
	"KEV_ATALK_ZONEUPDATED",
	"KEV_ATALK_ROUTERUP",
	"KEV_ATALK_ROUTERUP_INVALID",
	"KEV_ATALK_ROUTERDOWN",
	"KEV_ATALK_ZONELISTCHANGED"
};


SCDynamicStoreRef	store		= NULL;
Boolean			_verbose	= FALSE;


#define ROUNDUP(a, size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) ap = (struct sockaddr *) \
	((caddr_t) ap + (ap->sa_len ? ROUNDUP(ap->sa_len, sizeof(u_long)) \
				    : sizeof(u_long)))


static int
inet_dgram_socket()
{
    return (socket(AF_INET, SOCK_DGRAM, 0));
}

static int
ifflags_set(int s, char * name, short flags)
{
    struct ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret < 0) {
		return (ret);
    }
    ifr.ifr_flags |= flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

static int
ifflags_clear(int s, char * name, short flags)
{
    struct ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret < 0) {
		return (ret);
    }
    ifr.ifr_flags &= ~flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

static void
mark_if_up(char * name)
{
	int s = inet_dgram_socket();
	if (s < 0)
		return;
	ifflags_set(s, name, IFF_UP);
	close(s);
}

static void
mark_if_down(char * name)
{
	int s = inet_dgram_socket();
	if (s < 0)
		return;
	ifflags_clear(s, name, IFF_UP);
	close(s);
}

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int     i;

	for (i=0; i<RTAX_MAX; i++) {
		if (addrs & (1<<i)) {
			rti_info[i] = sa;
			NEXT_SA(sa);
		} else {
			rti_info[i] = NULL;
		}
	}
}


static void
appendAddress(CFMutableDictionaryRef dict, CFStringRef key, struct in_addr *address)
{
	CFStringRef		addr;
	CFArrayRef		addrs;
	CFMutableArrayRef	newAddrs;

	addrs = CFDictionaryGetValue(dict, key);
	if (addrs) {
		newAddrs = CFArrayCreateMutableCopy(NULL, 0, addrs);
	} else {
		newAddrs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	addr = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(address));
	CFArrayAppendValue(newAddrs, addr);
	CFRelease(addr);

	CFDictionarySetValue(dict, key, newAddrs);
	CFRelease(newAddrs);
	return;
}


static int
get_atalk_interface_cfg(const char *if_name, at_if_cfg_t *cfg)
{
	int 	fd;

	/* open socket */
	if ((fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return -1;

	/* get config info for given interface */
	strncpy(cfg->ifr_name, if_name, sizeof(cfg->ifr_name));
	if (ioctl(fd, AIOCGETIFCFG, (caddr_t)cfg) < 0) {
		(void)close(fd);
		return -1;
	}

	(void)close(fd);
	return 0;
}


static void
logEvent(CFStringRef evStr, struct kern_event_msg *ev_msg)
{
	int	i;
	int	j;

	SCLog(_verbose, LOG_DEBUG, CFSTR("%@ event:"), evStr);
	SCLog(_verbose, LOG_DEBUG,
	      CFSTR("  Event size=%d, id=%d, vendor=%d, class=%d, subclass=%d, code=%d"),
	      ev_msg->total_size,
	      ev_msg->id,
	      ev_msg->vendor_code,
	      ev_msg->kev_class,
	      ev_msg->kev_subclass,
	      ev_msg->event_code);
	for (i=0, j=KEV_MSG_HEADER_SIZE; j<ev_msg->total_size; i++, j+=4) {
		SCLog(_verbose, LOG_DEBUG, CFSTR("  Event data[%2d] = %08lx"), i, ev_msg->event_data[i]);
	}
}


static void
interface_update_addresses(const char *if_name)
{
	char			*buf		= NULL;
	size_t			bufLen;
	CFDictionaryRef		dict		= NULL;
	int			entry;
	char			ifr_name[IFNAMSIZ+1];
	CFStringRef		interface;
	boolean_t		interfaceFound	= FALSE;
	CFStringRef		key		= NULL;
	int			mib[6];
	CFMutableDictionaryRef	newDict		= NULL;
	size_t			offset;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &bufLen, NULL, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() size failed: %s"), strerror(errno));
		goto error;
	}

	buf = (char *)CFAllocatorAllocate(NULL, bufLen, 0);

	if (sysctl(mib, 6, buf, &bufLen, NULL, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() failed: %s"), strerror(errno));
		goto error;
	}

	entry  = 0;
	offset = 0;
	while (offset < bufLen) {
		struct if_msghdr	*ifm;
		struct sockaddr_dl	*sdl;
		boolean_t		skip	= FALSE;

		ifm = (struct if_msghdr *)&buf[offset];

		if (ifm->ifm_type != RTM_IFINFO) {
			SCLog(TRUE, LOG_ERR, CFSTR("unexpected data from sysctl buffer"));
			break;
		}

		/* advance to next address/interface */
		offset += ifm->ifm_msglen;

		/* get interface name */
		sdl = (struct sockaddr_dl *)(ifm + 1);
		if (sdl->sdl_nlen > IFNAMSIZ) {
			SCLog(TRUE, LOG_ERR, CFSTR("sdl_nlen > IFNAMSIZ"));
			break;
		}
		bzero(ifr_name, sizeof(ifr_name));
		memcpy(ifr_name, sdl->sdl_data, sdl->sdl_nlen);

		/* check if this is the requested interface */
		if (if_name) {
			if (strncmp(if_name, ifr_name, IFNAMSIZ) == 0) {
				interfaceFound = TRUE;	/* yes, this is the one I want */
			} else {
				skip = TRUE;		/* sorry, not interested */
			}
		}

		if (!skip) {
			/* get the current cache information */
			interface = CFStringCreateWithCString(NULL, ifr_name, kCFStringEncodingMacRoman);
			key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
										  kSCDynamicStoreDomainState,
										  interface,
										  kSCEntNetIPv4);
			CFRelease(interface);

			dict = SCDynamicStoreCopyValue(store, key);
			if (isA_CFDictionary(dict)) {
				newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4Addresses);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4SubnetMasks);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4DestAddresses);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4BroadcastAddresses);
			}

			if (!newDict) {
				newDict = CFDictionaryCreateMutable(NULL,
								      0,
								      &kCFTypeDictionaryKeyCallBacks,
								      &kCFTypeDictionaryValueCallBacks);
			}
		}

		while (offset < bufLen) {
			struct ifa_msghdr	*ifam;
			struct sockaddr		*a_info[RTAX_MAX];
			struct sockaddr		*sa;

			ifam = (struct ifa_msghdr *)&buf[offset];

			if (ifam->ifam_type != RTM_NEWADDR) {
				/* if not an address for this interface */
				break;
			}

			/* advance to next address/interface */
			offset += ifam->ifam_msglen;

			if (skip) {
				/* if we are not interested in this interface */
				continue;
			}

			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, a_info);

			sa = a_info[RTAX_IFA];
			if (!sa) {
				break;
			}

			switch (sa->sa_family) {
				case AF_INET :
				{
					struct sockaddr_in	*sin;

					sin = (struct sockaddr_in *)a_info[RTAX_IFA];
					appendAddress(newDict, kSCPropNetIPv4Addresses, &sin->sin_addr);

					if (ifm->ifm_flags & IFF_POINTOPOINT) {
						struct sockaddr_in	*dst;

						dst = (struct sockaddr_in *)a_info[RTAX_BRD];
						appendAddress(newDict, kSCPropNetIPv4DestAddresses, &dst->sin_addr);
					} else {
						struct sockaddr_in	*brd;
						struct sockaddr_in	*msk;

						brd = (struct sockaddr_in *)a_info[RTAX_BRD];
						appendAddress(newDict, kSCPropNetIPv4BroadcastAddresses, &brd->sin_addr);
						msk = (struct sockaddr_in *)a_info[RTAX_NETMASK];
						appendAddress(newDict, kSCPropNetIPv4SubnetMasks, &msk->sin_addr);
					}
					break;
				}

				default :
				{
					SCLog(TRUE, LOG_DEBUG, CFSTR("sysctl() returned address w/family=%d"), sa->sa_family);
					break;
				}
			}
		}

		if (!skip) {
			if (!dict || !CFEqual(dict, newDict)) {
				if (CFDictionaryGetCount(newDict) > 0) {
					if (!SCDynamicStoreSetValue(store, key, newDict)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreSetValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				} else if (dict) {
					if (!SCDynamicStoreRemoveValue(store, key)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreRemoveValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				}
			}
			if (dict) {
				CFRelease(dict);
				dict = NULL;
			}
			CFRelease(newDict);
			newDict = NULL;
			CFRelease(key);
		}
	}

	/* if the last address[es] were removed from the target interface */
	if (if_name && !interfaceFound) {
		interface = CFStringCreateWithCString(NULL, ifr_name, kCFStringEncodingMacRoman);
		key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									  kSCDynamicStoreDomainState,
									  interface,
									  kSCEntNetIPv4);
		CFRelease(interface);

		dict = SCDynamicStoreCopyValue(store, key);
		if (dict) {
			if (isA_CFDictionary(dict)) {
				newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4Addresses);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4SubnetMasks);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4DestAddresses);
				CFDictionaryRemoveValue(newDict, kSCPropNetIPv4BroadcastAddresses);
				if (CFDictionaryGetCount(newDict) > 0) {
					if (!SCDynamicStoreSetValue(store, key, newDict)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreSetValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				} else {
					if (!SCDynamicStoreRemoveValue(store, key)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreRemoveValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				}
				CFRelease(newDict);		newDict = NULL;
			}
			CFRelease(dict);
		}
		CFRelease(key);
	}

    error :

	if (buf)	CFAllocatorDeallocate(NULL, buf);

	return;
}


void
interface_collision_ipv4(const char *if_name, struct in_addr ip_addr, int hw_len, const void * hw_addr)
{
	uint8_t	*		hw_addr_bytes = (uint8_t *)hw_addr;
	int			i;
	CFStringRef		if_name_cf;
	CFMutableStringRef	key;
	CFStringRef		prefix;

	if_name_cf = CFStringCreateWithCString(NULL, if_name, 
					       kCFStringEncodingASCII);
	prefix = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    if_name_cf,
							    kSCEntNetIPv4ARPCollision);
	key = CFStringCreateMutableCopy(NULL, 0, prefix);
	CFStringAppendFormat(key, NULL, CFSTR("/" IP_FORMAT),
			     IP_LIST(&ip_addr));
	for (i = 0; i < hw_len; i++) {
	    CFStringAppendFormat(key, NULL, CFSTR("%s%02x"),
				 (i == 0) ? "/" : ":", hw_addr_bytes[i]);
	}
	SCDynamicStoreNotifyValue(store, key);
	CFRelease(key);
	CFRelease(prefix);
	CFRelease(if_name_cf);
	return;
}

static void
interface_update_appletalk(const char *if_name)
{
	char			*buf		= NULL;
	size_t			bufLen;
	CFDictionaryRef		dict		= NULL;
	int			entry;
	char			ifr_name[IFNAMSIZ+1];
	CFStringRef		interface;
	boolean_t		interfaceFound	= FALSE;
	CFStringRef		key		= NULL;
	int			mib[6];
	CFMutableDictionaryRef	newDict	= NULL;
	size_t			offset;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_APPLETALK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &bufLen, NULL, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() size failed: %s"), strerror(errno));
		goto error;
	}

	buf = (char *)CFAllocatorAllocate(NULL, bufLen, 0);

	if (sysctl(mib, 6, buf, &bufLen, NULL, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sysctl() failed: %s"), strerror(errno));
		goto error;
	}

	entry  = 0;
	offset = 0;
	while (offset < bufLen) {
		struct if_msghdr	*ifm;
		struct sockaddr_dl	*sdl;
		boolean_t		skip	= FALSE;

		ifm = (struct if_msghdr *)&buf[offset];

		if (ifm->ifm_type != RTM_IFINFO) {
			SCLog(TRUE, LOG_ERR, CFSTR("unexpected data from sysctl buffer"));
			break;
		}

		/* advance to next address/interface */
		offset += ifm->ifm_msglen;

		/* get interface name */
		sdl = (struct sockaddr_dl *)(ifm + 1);
		if (sdl->sdl_nlen > IFNAMSIZ) {
			SCLog(TRUE, LOG_ERR, CFSTR("sdl_nlen > IFNAMSIZ"));
			break;
		}
		bzero(ifr_name, sizeof(ifr_name));
		memcpy(ifr_name, sdl->sdl_data, sdl->sdl_nlen);

		/* check if this is the requested interface */
		if (if_name) {
			if (strncmp(if_name, ifr_name, IFNAMSIZ) == 0) {
				interfaceFound = TRUE;	/* yes, this is the one I want */
			} else {
				skip = TRUE;		/* sorry, not interested */
			}
		}

		if (!skip) {
			/* get the current cache information */
			interface = CFStringCreateWithCString(NULL, ifr_name, kCFStringEncodingMacRoman);
			key     = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
										  kSCDynamicStoreDomainState,
										  interface,
										  kSCEntNetAppleTalk);
			CFRelease(interface);

			dict = SCDynamicStoreCopyValue(store, key);
			if (isA_CFDictionary(dict)) {
				newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNetworkID);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNodeID);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNetworkRange);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkDefaultZone);
			}

			if (!newDict) {
				newDict = CFDictionaryCreateMutable(NULL,
								      0,
								      &kCFTypeDictionaryKeyCallBacks,
								      &kCFTypeDictionaryValueCallBacks);
			}
		}

		while (offset < bufLen) {
			struct ifa_msghdr	*ifam;
			struct sockaddr		*a_info[RTAX_MAX];
			struct sockaddr		*sa;

			ifam = (struct ifa_msghdr *)&buf[offset];

			if (ifam->ifam_type != RTM_NEWADDR) {
				/* if not an address for this interface */
				break;
			}

			/* advance to next address/interface */
			offset += ifam->ifam_msglen;

			if (skip) {
				/* if we are not interested in this interface */
				continue;
			}

			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, a_info);

			sa = a_info[RTAX_IFA];
			if (!sa) {
				break;
			}

			switch (sa->sa_family) {
				case AF_APPLETALK :
				{
					at_if_cfg_t		cfg;
					int			iVal;
					CFNumberRef		num;
					struct sockaddr_at      *sat;

					sat = (struct sockaddr_at *)a_info[RTAX_IFA];

					iVal = (int)sat->sat_addr.s_net;
					num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
					CFDictionarySetValue(newDict, kSCPropNetAppleTalkNetworkID, num);
					CFRelease(num);

					iVal = (int)sat->sat_addr.s_node;
					num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
					CFDictionarySetValue(newDict, kSCPropNetAppleTalkNodeID, num);
					CFRelease(num);

					if (!(get_atalk_interface_cfg(ifr_name, &cfg))) {
						CFStringRef		zone;

						/*
						 * Set starting and ending net values
						 */
						if (!(((cfg.netStart == 0) && (cfg.netEnd == 0)) ||
						      ((cfg.netStart == DDP_MIN_NETWORK) && (cfg.netEnd == DDP_MAX_NETWORK)))) {
							CFMutableArrayRef	array;

							array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

							iVal = cfg.netStart;
							num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
							CFArrayAppendValue(array, num);
							CFRelease(num);

							iVal = cfg.netEnd;
							num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
							CFArrayAppendValue(array, num);
							CFRelease(num);

							CFDictionarySetValue(newDict, kSCPropNetAppleTalkNetworkRange, array);
							CFRelease(array);
						}

						/*
						 * Set the default zone
						 */
						zone = CFStringCreateWithPascalString(NULL,
										      (ConstStr255Param)&cfg.zonename,
										      kCFStringEncodingMacRoman);
						CFDictionarySetValue(newDict, kSCPropNetAppleTalkDefaultZone, zone);
						CFRelease(zone);
					}

					break;
				}

				default :
				{
					SCLog(TRUE, LOG_DEBUG, CFSTR("sysctl() returned address w/family=%d"), sa->sa_family);
					break;
				}
			}
		}

		if (!skip) {
			if (!dict || !CFEqual(dict, newDict)) {
				if (CFDictionaryGetCount(newDict) > 0) {
					if (!SCDynamicStoreSetValue(store, key, newDict)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreSetValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				} else if (dict) {
					if (!SCDynamicStoreRemoveValue(store, key)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreRemoveValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				}
			}
			if (dict) {
				CFRelease(dict);
				dict = NULL;
			}
			CFRelease(newDict);
			newDict = NULL;
			CFRelease(key);
		}
	}

	/* if the last address[es] were removed from the target interface */
	if (if_name && !interfaceFound) {
		interface = CFStringCreateWithCString(NULL, ifr_name, kCFStringEncodingMacRoman);
		key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									  kSCDynamicStoreDomainState,
									  interface,
									  kSCEntNetAppleTalk);
		CFRelease(interface);

		dict = SCDynamicStoreCopyValue(store, key);
		if (dict) {
			if (isA_CFDictionary(dict)) {
				newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNetworkID);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNodeID);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNetworkRange);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkDefaultZone);
				if (CFDictionaryGetCount(newDict) > 0) {
					if (!SCDynamicStoreSetValue(store, key, newDict)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreSetValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				} else {
					if (!SCDynamicStoreRemoveValue(store, key)) {
						SCLog(TRUE,
						      LOG_DEBUG,
						      CFSTR("SCDynamicStoreRemoveValue() failed: %s"),
						      SCErrorString(SCError()));
					}
				}
				CFRelease(newDict);		newDict = NULL;
			}
			CFRelease(dict);
		}
		CFRelease(key);
	}

    error :

	if (buf)	CFAllocatorDeallocate(NULL, buf);

	return;
}


static CFMutableDictionaryRef
copy_entity(CFStringRef key)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;

	dict = SCDynamicStoreCopyValue(store, key);
	if (dict != NULL) {
		if (isA_CFDictionary(dict) != NULL) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		}
		CFRelease(dict);
	}
	if (newDict == NULL) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}
	return (newDict);
}

static CFStringRef
create_interface_key(const char * if_name)
{
	CFStringRef		interface;
	CFStringRef		key;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  interface,
								  kSCEntNetLink);
	CFRelease(interface);
	return (key);
}

static void
interface_update_status(const char *if_name, CFBooleanRef active,
			boolean_t attach)
{
	CFStringRef		key		= NULL;
	CFMutableDictionaryRef	newDict		= NULL;
	CFBooleanRef		state		= NULL;

	key = create_interface_key(if_name);
	newDict = copy_entity(key);
	state = isA_CFBoolean(CFDictionaryGetValue(newDict,
						   kSCPropNetLinkActive));
	/* if new status available, update cache */
	if (active == NULL) {
	    CFDictionaryRemoveValue(newDict, kSCPropNetLinkActive);
	}
	else {
	    CFDictionarySetValue(newDict, kSCPropNetLinkActive, active);
	}
	if (attach == TRUE) {
		/* the interface was attached, remove stale state */
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkDetaching);
	}

	/* update status */
	if (CFDictionaryGetCount(newDict) == 0) {
		if (!SCDynamicStoreRemoveValue(store, key)) {
			if (SCError() != kSCStatusNoKey) {
				SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreRemoveValue() failed: %s"), SCErrorString(SCError()));
			}
		}
	}
	else if (!SCDynamicStoreSetValue(store, key, newDict)) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetValue() failed: %s"), SCErrorString(SCError()));
	}

	CFRelease(key);
	CFRelease(newDict);
	return;
}

static void
interface_detaching(const char *if_name)
{
	CFStringRef		key		= NULL;
	CFMutableDictionaryRef	newDict		= NULL;

	key = create_interface_key(if_name);
	newDict = copy_entity(key);
	CFDictionarySetValue(newDict, kSCPropNetLinkDetaching,
			     kCFBooleanTrue);
	if (!SCDynamicStoreSetValue(store, key, newDict)) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetValue() failed: %s"), SCErrorString(SCError()));
	}
	CFRelease(key);
	CFRelease(newDict);
	return;
}

static void
interface_remove(const char *if_name)
{
	CFStringRef		key		= NULL;

	key = create_interface_key(if_name);
	if (!SCDynamicStoreRemoveValue(store, key)) {
		if (SCError() != kSCStatusNoKey) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreRemoveValue() failed: %s"), SCErrorString(SCError()));
		}
	}
	CFRelease(key);
	return;
}


static void
interface_update_atalk_address(struct kev_atalk_data *aEvent, const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		key;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict = NULL;
	CFNumberRef		newNode, newNet;
	int			node;
	int			net;

	/* get the current cache information */
	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  interface,
								  kSCEntNetAppleTalk);
	CFRelease(interface);

	dict = SCDynamicStoreCopyValue(store, key);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		}
		CFRelease(dict);
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	/* Update node/net values in cache */
	node	= (int)aEvent->node_data.address.s_node;
	net	= (int)aEvent->node_data.address.s_net;

	newNode	= CFNumberCreate(NULL, kCFNumberIntType, &node);
	newNet 	= CFNumberCreate(NULL, kCFNumberIntType, &net);

	CFDictionarySetValue(newDict, kSCPropNetAppleTalkNodeID, newNode);
	CFDictionarySetValue(newDict, kSCPropNetAppleTalkNetworkID, newNet);

	CFRelease(newNode);
	CFRelease(newNet);

	/* update cache */
	if (!SCDynamicStoreSetValue(store, key, newDict)) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetValue() failed: %s"), SCErrorString(SCError()));
	}

	CFRelease(newDict);
	CFRelease(key);
	return;
}


static void
interface_update_atalk_zone(struct kev_atalk_data *aEvent, const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		key;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict	= NULL;
	CFStringRef		newZone;

	/* get the current cache information */
	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  interface,
								  kSCEntNetAppleTalk);
	CFRelease(interface);

	dict = SCDynamicStoreCopyValue(store, key);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		}
		CFRelease(dict);
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	/* Update zone value in cache */
	newZone = CFStringCreateWithPascalString(NULL, (ConstStr255Param)&(aEvent->node_data.zone), kCFStringEncodingMacRoman);

	CFDictionarySetValue(newDict, kSCPropNetAppleTalkDefaultZone, newZone);

	CFRelease(newZone);

	/* update cache */
	if (!SCDynamicStoreSetValue(store, key, newDict)) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetValue() failed: %s"), SCErrorString(SCError()));
	}

	CFRelease(newDict);
	CFRelease(key);
	return;
}


static void
interface_update_shutdown_atalk()
{
	CFStringRef			cacheKey;
	CFDictionaryRef			dict1;
	CFArrayRef			ifList = NULL;
	CFIndex 			count, index;
	CFStringRef 			interface;
	CFStringRef			key;

	cacheKey  = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							    kSCDynamicStoreDomainState);

	dict1 = SCDynamicStoreCopyValue(store, cacheKey);
	CFRelease(cacheKey);

	if (dict1) {
		if (isA_CFDictionary(dict1)) {
			/*get a list of the interfaces*/
			ifList  = isA_CFArray(CFDictionaryGetValue(dict1, kSCDynamicStorePropNetInterfaces));
			if (ifList) {
				count = CFArrayGetCount(ifList);

				/*iterate through list and remove AppleTalk data*/
				for (index = 0; index < count; index++) {
					interface = CFArrayGetValueAtIndex(ifList, index);
					key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
												  kSCDynamicStoreDomainState,
												  interface,
												  kSCEntNetAppleTalk);
					SCDynamicStoreRemoveValue(store, key);
					CFRelease(key);
				}
			}
		}
		CFRelease(dict1);
	}

	return;
}


static void
link_update_status(const char *if_name)
{
	struct ifmediareq	ifm;
	CFBooleanRef		active = NULL;
	int			sock = -1;

	sock = inet_dgram_socket();
	if (sock < 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("link_update_status: socket open failed,  %s"), strerror(errno));
		goto done;
	}
	bzero((char *)&ifm, sizeof(ifm));
	(void) strncpy(ifm.ifm_name, if_name, sizeof(ifm.ifm_name));

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) == -1) {
		/* if media status not available for this interface */
		goto done;
	}

	if (ifm.ifm_count == 0) {
		/* no media types */
		goto done;
	}

	if (!(ifm.ifm_status & IFM_AVALID)) {
		/* if active bit not valid */
		goto done;
	}

	if (ifm.ifm_status & IFM_ACTIVE) {
		active = kCFBooleanTrue;
	} else {
		active = kCFBooleanFalse;
	}

 done:
	interface_update_status(if_name, active, TRUE);
	if (sock >= 0)
		close(sock);
	return;
}


static void
link_add(const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		cacheKey;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;
	CFArrayRef		ifList;
	CFMutableArrayRef	newIFList	= NULL;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	cacheKey  = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							    kSCDynamicStoreDomainState);

	dict = SCDynamicStoreCopyValue(store, cacheKey);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			ifList  = CFDictionaryGetValue(newDict, kSCDynamicStorePropNetInterfaces);
			if (isA_CFArray(ifList)) {
				newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			}
		}
		CFRelease(dict);
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	if (!newIFList) {
		newIFList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	if (CFArrayContainsValue(newIFList,
				 CFRangeMake(0, CFArrayGetCount(newIFList)),
				 interface) == FALSE) {
		CFArrayAppendValue(newIFList, interface);
		CFDictionarySetValue(newDict,
				     kSCDynamicStorePropNetInterfaces,
				     newIFList);
	}
	if (!SCDynamicStoreSetValue(store, cacheKey, newDict)) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCDynamicStoreSetValue() failed: %s"), SCErrorString(SCError()));
	}
	link_update_status(if_name);
	CFRelease(cacheKey);
	CFRelease(interface);
	if (newDict)	CFRelease(newDict);
	if (newIFList)	CFRelease(newIFList);

	return;
}


static void
link_remove(const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		cacheKey;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;
	CFArrayRef		ifList;
	CFMutableArrayRef	newIFList	= NULL;
	CFIndex			i;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	cacheKey  = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							    kSCDynamicStoreDomainState);

	dict = SCDynamicStoreCopyValue(store, cacheKey);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			ifList  = CFDictionaryGetValue(newDict, kSCDynamicStorePropNetInterfaces);
			if (isA_CFArray(ifList)) {
				newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			}
		}
		CFRelease(dict);
	}

	if (!newIFList ||
	    ((i = CFArrayGetFirstIndexOfValue(newIFList,
					     CFRangeMake(0, CFArrayGetCount(newIFList)),
					     interface)) == -1)
	   ) {
		/* we're not tracking this interface */
		goto done;
	}

	CFArrayRemoveValueAtIndex(newIFList, i);
	CFDictionarySetValue(newDict, kSCDynamicStorePropNetInterfaces, newIFList);
	if (!SCDynamicStoreSetValue(store, cacheKey, newDict)) {
		SCLog(TRUE,
		      LOG_DEBUG,
		      CFSTR("SCDynamicStoreSetValue() failed: %s"),
		      SCErrorString(SCError()));
	}

	interface_remove(if_name);

    done:

	CFRelease(cacheKey);
	CFRelease(interface);
	if (newDict)	CFRelease(newDict);
	if (newIFList)	CFRelease(newIFList);

	return;
}


static void
processEvent_Apple_Network(struct kern_event_msg *ev_msg)
{
	CFStringRef		evStr;
	int			dataLen = (ev_msg->total_size - KEV_MSG_HEADER_SIZE);
	struct net_event_data	*nEvent;
	struct kev_in_collision *colEvent;
	struct kev_in_data	*iEvent;
	struct kev_atalk_data	*aEvent;
	char			ifr_name[IFNAMSIZ+1];

	switch (ev_msg->kev_subclass) {
		case KEV_INET_SUBCLASS :
			iEvent = (struct kev_in_data *)&ev_msg->event_data[0];
			switch (ev_msg->event_code) {
				case KEV_INET_NEW_ADDR :
				case KEV_INET_CHANGED_ADDR :
				case KEV_INET_ADDR_DELETED :
				case KEV_INET_SIFDSTADDR :
				case KEV_INET_SIFBRDADDR :
				case KEV_INET_SIFNETMASK :
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", iEvent->link_data.if_name, iEvent->link_data.if_unit);
					if (dataLen == sizeof(struct kev_in_data)) {
						interface_update_addresses(ifr_name);
					} else {
						evStr = CFStringCreateWithFormat(NULL,
										 NULL,
										 CFSTR("%s: %s"),
										 ifr_name,
										 inetEventName[ev_msg->event_code]);
						logEvent(evStr, ev_msg);
						CFRelease(evStr);
					}
					break;
				case KEV_INET_ARPCOLLISION :
					colEvent 
						= (struct kev_in_collision *)&ev_msg->event_data[0];
					if (dataLen >= sizeof(*colEvent)
						&& (dataLen == sizeof(*colEvent) + colEvent->hw_len)) {
						snprintf(ifr_name, sizeof(ifr_name), 
								 "%s%ld", 
								 colEvent->link_data.if_name, 
								 colEvent->link_data.if_unit);
						interface_collision_ipv4(ifr_name,
												 colEvent->ia_ipaddr,
												 colEvent->hw_len,
												 colEvent->hw_addr);
					}
					break;
				default :
					logEvent(CFSTR("New Apple network INET subcode"), ev_msg);
					break;
			}
			break;

		case KEV_DL_SUBCLASS :
			nEvent = (struct net_event_data *)&ev_msg->event_data[0];
			switch (ev_msg->event_code) {
				case KEV_DL_IF_ATTACHED :
					/*
					 * new interface added
					 */
					if (dataLen == sizeof(struct net_event_data)) {
						snprintf(ifr_name, sizeof(ifr_name), "%s%ld", nEvent->if_name, nEvent->if_unit);
						link_add(ifr_name);
					} else {
						logEvent(CFSTR("KEV_DL_IF_ATTACHED"), ev_msg);
					}
					break;

				case KEV_DL_IF_DETACHED :
					/*
					 * interface removed
					 */
					if (dataLen == sizeof(struct net_event_data)) {
						snprintf(ifr_name, sizeof(ifr_name), "%s%ld", nEvent->if_name, nEvent->if_unit);
						link_remove(ifr_name);
					} else {
						logEvent(CFSTR("KEV_DL_IF_DETACHED"), ev_msg);
					}
					break;

				case KEV_DL_IF_DETACHING :
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", nEvent->if_name, nEvent->if_unit);
					interface_detaching(ifr_name);
					break;

				case KEV_DL_SIFFLAGS :
				case KEV_DL_SIFMETRICS :
				case KEV_DL_SIFMTU :
				case KEV_DL_SIFPHYS :
				case KEV_DL_SIFMEDIA :
				case KEV_DL_SIFGENERIC :
				case KEV_DL_ADDMULTI :
				case KEV_DL_DELMULTI :
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", nEvent->if_name, nEvent->if_unit);
					evStr = CFStringCreateWithFormat(NULL,
									 NULL,
									 CFSTR("%s: %s"),
									 ifr_name,
									 dlEventName[ev_msg->event_code]);
					logEvent(evStr, ev_msg);
					CFRelease(evStr);
					break;
				case KEV_DL_PROTO_ATTACHED :
				case KEV_DL_PROTO_DETACHED : {
					struct kev_dl_proto_data *	protoEvent = (struct kev_dl_proto_data *)&ev_msg->event_data[0];
					if (dataLen == sizeof(*protoEvent)) {
						snprintf(ifr_name, sizeof(ifr_name), "%s%ld", nEvent->if_name, nEvent->if_unit);
						if (protoEvent->proto_remaining_count == 0) {
							mark_if_down(ifr_name);
						}
						else {
							mark_if_up(ifr_name);
						}
					}
					else {
						evStr = CFStringCreateWithFormat(NULL,
										 NULL,
										 CFSTR("%s"),
										 dlEventName[ev_msg->event_code]);
						logEvent(evStr, ev_msg);
						CFRelease(evStr);
					}
					break;
				}

				case KEV_DL_LINK_OFF :
				case KEV_DL_LINK_ON :
					/*
					 * update the link status in the cache
					 */
					if (dataLen == sizeof(struct net_event_data)) {
						snprintf(ifr_name, sizeof(ifr_name), "%s%ld", nEvent->if_name, nEvent->if_unit);
						interface_update_status(ifr_name,
									(ev_msg->event_code == KEV_DL_LINK_ON)
									? kCFBooleanTrue
									: kCFBooleanFalse, FALSE);
					} else {
						if (ev_msg->event_code == KEV_DL_LINK_OFF) {
							logEvent(CFSTR("KEV_DL_LINK_OFF"), ev_msg);
						} else {
							logEvent(CFSTR("KEV_DL_LINK_ON"),  ev_msg);
						}
					}
					break;

				default :
					logEvent(CFSTR("New Apple network DL subcode"), ev_msg);
					break;
			}
			break;

		case KEV_ATALK_SUBCLASS:
			aEvent = (struct kev_atalk_data *)&ev_msg->event_data[0];
			switch (ev_msg->event_code) {
				case KEV_ATALK_ENABLED:
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", aEvent->link_data.if_name, aEvent->link_data.if_unit);
					if (dataLen == sizeof(struct kev_atalk_data)) {
						interface_update_atalk_address(aEvent, ifr_name);
					} else {
						evStr = CFStringCreateWithFormat(NULL,
										 NULL,
										 CFSTR("%s: %s"),
										 ifr_name,
										 atalkEventName[ev_msg->event_code]);
						logEvent(evStr, ev_msg);
						CFRelease(evStr);
					}
					break;

				case KEV_ATALK_DISABLED:
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", aEvent->link_data.if_name, aEvent->link_data.if_unit);
					if (dataLen == sizeof(struct kev_atalk_data)) {
						interface_update_shutdown_atalk();
					} else {
						evStr = CFStringCreateWithFormat(NULL,
										 NULL,
										 CFSTR("%s: %s"),
										 ifr_name,
										 atalkEventName[ev_msg->event_code]);
						logEvent(evStr, ev_msg);
						CFRelease(evStr);
					}
					break;

				case KEV_ATALK_ZONEUPDATED:
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", aEvent->link_data.if_name, aEvent->link_data.if_unit);
					if (dataLen == sizeof(struct kev_atalk_data)) {
						interface_update_atalk_zone(aEvent, ifr_name);
					} else {
						evStr = CFStringCreateWithFormat(NULL,
										 NULL,
										 CFSTR("%s: %s"),
										 ifr_name,
										 atalkEventName[ev_msg->event_code]);
						logEvent(evStr, ev_msg);
						CFRelease(evStr);
					}
					break;

				case KEV_ATALK_ROUTERUP:
				case KEV_ATALK_ROUTERUP_INVALID:
				case KEV_ATALK_ROUTERDOWN:
					snprintf(ifr_name, sizeof(ifr_name), "%s%ld", aEvent->link_data.if_name, aEvent->link_data.if_unit);
					if (dataLen == sizeof(struct kev_atalk_data)) {
						interface_update_appletalk(ifr_name);
					} else {
						evStr = CFStringCreateWithFormat(NULL,
										 NULL,
										 CFSTR("%s: %s"),
										 ifr_name,
										 atalkEventName[ev_msg->event_code]);
						logEvent(evStr, ev_msg);
						CFRelease(evStr);
					}
					break;

				case KEV_ATALK_ZONELISTCHANGED:
					break;

				default :
					logEvent(CFSTR("New Apple network AT subcode"), ev_msg);
					break;
			}
			break;

		default :
			logEvent(CFSTR("New Apple network subclass"), ev_msg);
			break;
	}

	return;
}


static void
processEvent_Apple_IOKit(struct kern_event_msg *ev_msg)
{
	switch (ev_msg->kev_subclass) {
		default :
			logEvent(CFSTR("New Apple IOKit subclass"), ev_msg);
			break;
	}

	return;
}


static void
eventCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
{
	int			so		= CFSocketGetNative(s);
	int			status;
	char			buf[1024];
	struct kern_event_msg	*ev_msg		= (struct kern_event_msg *)&buf[0];
	int			offset		= 0;

	status = recv(so, &buf, sizeof(buf), 0);
	if (status == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("recv() failed: %s"), strerror(errno));
		goto error;
	}

	if (!SCDynamicStoreLock(store)) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("SCDynamicStoreLock() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	while (offset < status) {
		if ((offset + ev_msg->total_size) > status) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("missed SYSPROTO_EVENT event, buffer not big enough"));
			break;
		}

		switch (ev_msg->vendor_code) {
			case KEV_VENDOR_APPLE :
				switch (ev_msg->kev_class) {
					case KEV_NETWORK_CLASS :
						processEvent_Apple_Network(ev_msg);
						break;
					case KEV_IOKIT_CLASS :
						processEvent_Apple_IOKit(ev_msg);
						break;
					default :
						/* unrecognized (Apple) event class */
						logEvent(CFSTR("New (Apple) class"), ev_msg);
						break;
				}
				break;
			default :
				/* unrecognized vendor code */
				logEvent(CFSTR("New vendor"), ev_msg);
				break;
		}
		offset += ev_msg->total_size;
		ev_msg = (struct kern_event_msg *)&buf[offset];
	}

	if (!SCDynamicStoreUnlock(store)) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("SCDynamicStoreUnlock() failed: %s"),
		      SCErrorString(SCError()));
	}

	return;

    error :

	SCLog(TRUE, LOG_ERR, CFSTR("kernel event monitor disabled."));
	CFSocketInvalidate(s);
	return;

}


void
prime()
{
	boolean_t		haveLock = FALSE;
	struct ifconf		ifc;
	struct ifreq		*ifr;
	char			buf[1024];
	int			offset;
	int			sock = -1;

	SCLog(_verbose, LOG_DEBUG, CFSTR("prime() called"));

	sock = inet_dgram_socket();
	if (sock == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("could not get interface list, socket() failed: %s"), strerror(errno));
		goto done;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("could not get interface list, ioctl() failed: %s"),
		      strerror(errno));
		goto done;
	}

	if (!SCDynamicStoreLock(store)) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("SCDynamicStoreLock() failed: %s"),
		      SCErrorString(SCError()));
		goto done;
	}
	haveLock = TRUE;

	/* update list of interfaces & link status */
	offset = 0;
	while (offset <= (ifc.ifc_len - sizeof(*ifr))) {
		int			extra;

		/*
		 * grab this interface & bump offset to next
		 */
		ifr    = (struct ifreq *)(ifc.ifc_buf + offset);
		offset = offset + sizeof(*ifr);
		extra  = ifr->ifr_addr.sa_len - sizeof(ifr->ifr_addr);
		if (extra > 0)
			offset = offset + extra;

		/*
		 * get the per-interface link/media information
		 */
		link_add(ifr->ifr_name);
	}

	/*
	 * update IPv4 network addresses already assigned to
	 * the interfaces.
	 */
	interface_update_addresses(NULL);

	/*
	 * update AppleTalk network addresses already assigned
	 * to the interfaces.
	 */
	interface_update_appletalk(NULL);

 done:
	if (sock >= 0)
		close(sock);

	if (haveLock) {
		if (!SCDynamicStoreUnlock(store)) {
			SCLog(TRUE,
			      LOG_ERR,
			      CFSTR("SCDynamicStoreUnlock() failed: %s"),
			      SCErrorString(SCError()));
		}
	}

	return;
}


void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
	int			so;
	int			status;
	struct kev_request	kev_req;
	CFSocketRef		es;
	CFSocketContext		context = { 0, NULL, NULL, NULL, NULL };
	CFRunLoopSourceRef	rls;

	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* open a "configd" session to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("Kernel/User Notification plug-in"),
				     NULL,
				     NULL);
	if (!store) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCDnamicStoreCreate() failed: %s"), SCErrorString(SCError()));
		SCLog(TRUE, LOG_ERR, CFSTR("kernel event monitor disabled."));
		return;
	}

	/* Open an event socket */
	so = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (so != -1) {
		/* establish filter to return all events */
		kev_req.vendor_code  = 0;
		kev_req.kev_class    = 0;	/* Not used if vendor_code is 0 */
		kev_req.kev_subclass = 0;	/* Not used if either kev_class OR vendor_code are 0 */
		status = ioctl(so, SIOCSKEVFILT, &kev_req);
		if (status) {
			SCLog(TRUE, LOG_ERR, CFSTR("could not establish event filter, ioctl() failed: %s"), strerror(errno));
			(void) close(so);
			so = -1;
		}
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("could not open event socket, socket() failed: %s"), strerror(errno));
	}

	if (so != -1) {
		int	yes = 1;

		status = ioctl(so, FIONBIO, &yes);
		if (status) {
			SCLog(TRUE, LOG_ERR, CFSTR("could not set non-blocking io, ioctl() failed: %s"), strerror(errno));
			(void) close(so);
			so = -1;
		}
	}

	if (so == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("kernel event monitor disabled."));
		CFRelease(store);
		return;
	}

	/* Create a CFSocketRef for the PF_SYSTEM kernel event socket */
	es  = CFSocketCreateWithNative(NULL,
				       so,
				       kCFSocketReadCallBack,
				       eventCallback,
				       &context);

	/* Create and add a run loop source for the event socket */
	rls = CFSocketCreateRunLoopSource(NULL, es, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRelease(rls);
	CFRelease(es);

	return;
}

#ifdef	MAIN
int
main(int argc, char **argv)
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	prime();
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
