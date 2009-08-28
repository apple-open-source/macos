/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/*
 * Modification History
 *
 * December 3, 2002		Dieter Siegmund <dieter@apple.com>
 * - handle the new KEV_INET_ARPCOLLISION event
 * - format the event into a DynamicStore key
 *     State:/Network/Interface/ifname/IPv4Collision/ip_addr/hw_addr
 *   and send a notification on the key
 *
 * August 8, 2002		Allan Nathanson <ajn@apple.com>
 * - added support for KEV_INET6_xxx events
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

#include "eventmon.h"
#include "cache.h"
#include "ev_dlil.h"
#include "ev_ipv4.h"
#include "ev_ipv6.h"
#include <notify.h>

#if	!TARGET_OS_IPHONE && INCLUDE_APPLETALK
#include "ev_appletalk.h"
#endif	/* !TARGET_OS_IPHONE && INCLUDE_APPLETALK */

static const char *inetEventName[] = {
	"",
	"INET address added",
	"INET address changed",
	"INET address deleted",
	"INET destination address changed",
	"INET broadcast address changed",
	"INET netmask changed",
	"INET ARP collision",
};

static const char *dlEventName[] = {
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

#if	!TARGET_OS_IPHONE && INCLUDE_APPLETALK
static const char *atalkEventName[] = {
	"",
	"KEV_ATALK_ENABLED",
	"KEV_ATALK_DISABLED",
	"KEV_ATALK_ZONEUPDATED",
	"KEV_ATALK_ROUTERUP",
	"KEV_ATALK_ROUTERUP_INVALID",
	"KEV_ATALK_ROUTERDOWN",
	"KEV_ATALK_ZONELISTCHANGED"
};
#endif	/* !TARGET_OS_IPHONE && INCLUDE_APPLETALK */

static const char *inet6EventName[] = {
	"",
	"KEV_INET6_NEW_USER_ADDR",
	"KEV_INET6_CHANGED_ADDR",
	"KEV_INET6_ADDR_DELETED",
	"KEV_INET6_NEW_LL_ADDR",
	"KEV_INET6_NEW_RTADV_ADDR",
	"KEV_INET6_DEFROUTER"
};

__private_extern__ Boolean		network_changed	= FALSE;
__private_extern__ SCDynamicStoreRef	store		= NULL;
__private_extern__ Boolean		_verbose	= FALSE;

__private_extern__
int
dgram_socket(int domain)
{
    return (socket(domain, SOCK_DGRAM, 0));
}

static int
ifflags_set(int s, char * name, short flags)
{
    struct ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret == -1) {
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
    if (ret == -1) {
		return (ret);
    }
    ifr.ifr_flags &= ~flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

static void
mark_if_up(char * name)
{
	int s = dgram_socket(AF_INET);
	if (s == -1)
		return;
	ifflags_set(s, name, IFF_UP);
	close(s);
}

static void
mark_if_down(char * name)
{
	int s = dgram_socket(AF_INET);
	if (s == -1)
		return;
	ifflags_clear(s, name, IFF_UP);
	close(s);
}

static void
post_network_changed(void)
{
	if (network_changed) {
		uint32_t	status;

		status = notify_post("com.apple.system.config.network_change");
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_post() failed: error=%ld"), status);
		}

		network_changed = FALSE;
	}

	return;
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
	for (i = 0, j = KEV_MSG_HEADER_SIZE; j < ev_msg->total_size; i++, j+=4) {
		SCLog(_verbose, LOG_DEBUG, CFSTR("  Event data[%2d] = %08lx"), i, ev_msg->event_data[i]);
	}
}

static const char *
inetEventNameString(uint32_t event_code)
{
	if (event_code <= KEV_INET_ARPCOLLISION) {
		return (inetEventName[event_code]);
	}
	return ("New Apple network INET subcode");
}

static const char *
inet6EventNameString(uint32_t event_code)
{
	if (event_code <= KEV_INET6_DEFROUTER) {
		return (inet6EventName[event_code]);
	}
	return ("New Apple network INET6 subcode");
}

static const char *
dlEventNameString(uint32_t event_code)
{
	if (event_code <= KEV_DL_PROTO_DETACHED) {
		return (dlEventName[event_code]);
	}
	return ("New Apple network DL subcode");
}

#if	!TARGET_OS_IPHONE && INCLUDE_APPLETALK
static const char *
atalkEventNameString(uint32_t event_code)
{
	if (event_code <= KEV_ATALK_ZONELISTCHANGED) {
		return (atalkEventName[event_code]);
	}
	return ("New Apple network AppleTalk subcode");
}
#endif	/* !TARGET_OS_IPHONE && INCLUDE_APPLETALK */

static void
copy_if_name(struct net_event_data * ev, char * ifr_name, int ifr_len)
{
	snprintf(ifr_name, ifr_len, "%s%d", ev->if_name, ev->if_unit);
	return;
}

static void
processEvent_Apple_Network(struct kern_event_msg *ev_msg)
{
	const char *			eventName = NULL;
	int				dataLen = (ev_msg->total_size - KEV_MSG_HEADER_SIZE);
	void *				event_data = &ev_msg->event_data[0];
	Boolean				handled = TRUE;
	char				ifr_name[IFNAMSIZ + 1];

	switch (ev_msg->kev_subclass) {
		case KEV_INET_SUBCLASS : {
			eventName = inetEventNameString(ev_msg->event_code);
			switch (ev_msg->event_code) {
				case KEV_INET_NEW_ADDR :
				case KEV_INET_CHANGED_ADDR :
				case KEV_INET_ADDR_DELETED :
				case KEV_INET_SIFDSTADDR :
				case KEV_INET_SIFBRDADDR :
				case KEV_INET_SIFNETMASK : {
					struct kev_in_data * ev;

					ev = (struct kev_in_data *)event_data;
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					copy_if_name(&ev->link_data, ifr_name, sizeof(ifr_name));
					interface_update_ipv4(NULL, ifr_name);
					break;
				}
				case KEV_INET_ARPCOLLISION : {
					struct kev_in_collision * ev;

					ev = (struct kev_in_collision *)event_data;
					if ((dataLen < sizeof(*ev))
					    || (dataLen < (sizeof(*ev) + ev->hw_len))) {
						handled = FALSE;
						break;
					}
					copy_if_name(&ev->link_data, ifr_name, sizeof(ifr_name));
					interface_collision_ipv4(ifr_name,
								 ev->ia_ipaddr,
								 ev->hw_len,
								 ev->hw_addr);
					break;
				}
#if	!TARGET_OS_IPHONE
				case KEV_INET_PORTINUSE : {
					struct kev_in_portinuse	* ev;
					ev = (struct kev_in_portinuse *)event_data;
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					port_in_use_ipv4(ev->port, ev->req_pid);
					break;
				}
#endif	/* !TARGET_OS_IPHONE */
				default :
					handled = FALSE;
					break;
			}
			break;
		}
		case KEV_INET6_SUBCLASS : {
			struct kev_in6_data * ev;

			eventName = inet6EventNameString(ev_msg->event_code);
			ev = (struct kev_in6_data *)event_data;
			switch (ev_msg->event_code) {
				case KEV_INET6_NEW_USER_ADDR :
				case KEV_INET6_CHANGED_ADDR :
				case KEV_INET6_ADDR_DELETED :
				case KEV_INET6_NEW_LL_ADDR :
				case KEV_INET6_NEW_RTADV_ADDR :
				case KEV_INET6_DEFROUTER :
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					copy_if_name(&ev->link_data, ifr_name, sizeof(ifr_name));
					interface_update_ipv6(NULL, ifr_name);
					break;

				default :
					handled = FALSE;
					break;
			}
			break;
		}
		case KEV_DL_SUBCLASS : {
			struct net_event_data * ev;

			eventName = dlEventNameString(ev_msg->event_code);
			ev = (struct net_event_data *)event_data;
			switch (ev_msg->event_code) {
				case KEV_DL_IF_ATTACHED :
					/*
					 * new interface added
					 */
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					copy_if_name(ev, ifr_name, sizeof(ifr_name));
					link_add(ifr_name);
					break;

				case KEV_DL_IF_DETACHED :
					/*
					 * interface removed
					 */
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					copy_if_name(ev, ifr_name, sizeof(ifr_name));
					link_remove(ifr_name);
					break;

				case KEV_DL_IF_DETACHING :
					/*
					 * interface detaching
					 */
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					copy_if_name(ev, ifr_name, sizeof(ifr_name));
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
					handled = FALSE;
					break;

				case KEV_DL_PROTO_ATTACHED :
				case KEV_DL_PROTO_DETACHED : {
					struct kev_dl_proto_data * protoEvent;

					protoEvent = (struct kev_dl_proto_data *)event_data;
					if (dataLen < sizeof(*protoEvent)) {
						handled = FALSE;
						break;
					}
					copy_if_name(&protoEvent->link_data,
						     ifr_name, sizeof(ifr_name));
					if (protoEvent->proto_remaining_count == 0) {
						mark_if_down(ifr_name);
					} else {
						mark_if_up(ifr_name);
					}
					break;
				}

				case KEV_DL_LINK_OFF :
				case KEV_DL_LINK_ON :
					/*
					 * update the link status in the store
					 */
					if (dataLen < sizeof(*ev)) {
						handled = FALSE;
						break;
					}
					copy_if_name(ev, ifr_name, sizeof(ifr_name));
					link_update_status(ifr_name, FALSE);
					break;

				default :
					handled = FALSE;
					break;
			}
			break;
		}
#if	!TARGET_OS_IPHONE && INCLUDE_APPLETALK
		case KEV_ATALK_SUBCLASS: {
			struct kev_atalk_data * ev;

			eventName = atalkEventNameString(ev_msg->event_code);
			ev = (struct kev_atalk_data *)event_data;
			if (dataLen < sizeof(*ev)) {
				handled = FALSE;
				break;
			}
			copy_if_name(&ev->link_data, ifr_name, sizeof(ifr_name));
			switch (ev_msg->event_code) {
				case KEV_ATALK_ENABLED:
					interface_update_atalk_address(ev, ifr_name);
					break;

				case KEV_ATALK_DISABLED:
					interface_update_shutdown_atalk();
					break;

				case KEV_ATALK_ZONEUPDATED:
					interface_update_atalk_zone(ev, ifr_name);
					break;

				case KEV_ATALK_ROUTERUP:
				case KEV_ATALK_ROUTERUP_INVALID:
				case KEV_ATALK_ROUTERDOWN:
					interface_update_appletalk(NULL, ifr_name);
					break;

				case KEV_ATALK_ZONELISTCHANGED:
					break;

				default :
					handled = FALSE;
					break;
			}
			break;
		}
#endif	/* !TARGET_OS_IPHONE && INCLUDE_APPLETALK */
		default :
			handled = FALSE;
			break;
	}

	if (handled == FALSE) {
		CFStringRef	evStr;

		evStr = CFStringCreateWithCString(NULL,
						  (eventName != NULL) ? eventName : "New Apple network subclass",
						  kCFStringEncodingASCII);
		logEvent(evStr, ev_msg);
		CFRelease(evStr);
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
processEvent_Apple_System(struct kern_event_msg *ev_msg)
{
	switch (ev_msg->kev_subclass) {
		default :
			logEvent(CFSTR("New Apple System subclass"), ev_msg);
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

	cache_open();

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
					case KEV_SYSTEM_CLASS :
						processEvent_Apple_System(ev_msg);
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

	cache_write(store);
	cache_close();
	post_network_changed();

	return;

    error :

	SCLog(TRUE, LOG_ERR, CFSTR("kernel event monitor disabled."));
	CFSocketInvalidate(s);
	return;

}

__private_extern__
void
prime_KernelEventMonitor()
{
	struct ifaddrs	*ifap	= NULL;
	struct ifaddrs	*scan;
	int		sock	= -1;

	SCLog(_verbose, LOG_DEBUG, CFSTR("prime() called"));

	cache_open();

	sock = dgram_socket(AF_INET);
	if (sock == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("could not get interface list, socket() failed: %s"), strerror(errno));
		goto done;
	}

	if (getifaddrs(&ifap) == -1) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("could not get interface info, getifaddrs() failed: %s"),
		      strerror(errno));
		goto done;
	}

	/* update list of interfaces & link status */
	for (scan = ifap; scan != NULL; scan = scan->ifa_next) {
		if (scan->ifa_addr == NULL
		    || scan->ifa_addr->sa_family != AF_LINK) {
			continue;
		}
		/* get the per-interface link/media information */
		link_add(scan->ifa_name);
	}

	/*
	 * update IPv4 network addresses already assigned to
	 * the interfaces.
	 */
	interface_update_ipv4(ifap, NULL);

	/*
	 * update IPv6 network addresses already assigned to
	 * the interfaces.
	 */
	interface_update_ipv6(ifap, NULL);

#if	!TARGET_OS_IPHONE && INCLUDE_APPLETALK
	/*
	 * update AppleTalk network addresses already assigned
	 * to the interfaces.
	 */
	interface_update_appletalk(ifap, NULL);
#endif	/* !TARGET_OS_IPHONE && INCLUDE_APPLETALK */

	freeifaddrs(ifap);

 done:
	if (sock != -1)
		close(sock);

	cache_write(store);
	cache_close();

	network_changed = TRUE;
	post_network_changed();

	return;
}

static CFStringRef
kevSocketCopyDescription(const void *info)
{
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("<kernel event socket>"));
}

__private_extern__
void
load_KernelEventMonitor(CFBundleRef bundle, Boolean bundleVerbose)
{
	CFSocketContext		context = { 0
					  , (void *)1
					  , NULL
					  , NULL
					  , kevSocketCopyDescription
					  };
	CFSocketRef		es;
	struct kev_request	kev_req;
	CFRunLoopSourceRef	rls;
	int			so;
	int			status;

	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* open a "configd" session to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("Kernel Event Monitor plug-in"),
				     NULL,
				     NULL);
	if (store == NULL) {
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
	es = CFSocketCreateWithNative(NULL,
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

#include "ev_dlil.c"

#define appendAddress	appendAddress_v4
#define getIF		getIF_v4
#define updateStore	updateStore_v4
#include "ev_ipv4.c"
#undef appendAddress
#undef getIF
#undef updateStore

#define appendAddress	appendAddress_v6
#define getIF		getIF_v6
#define updateStore	updateStore_v6
#include "ev_ipv6.c"
#undef appendAddress
#undef getIF
#undef updateStore

#if	!TARGET_OS_IPHONE && INCLUDE_APPLETALK
#define getIF		getIF_at
#define updateStore	updateStore_at
#include "ev_appletalk.c"
#undef getIF
#undef updateStore
#endif	/* !TARGET_OS_IPHONE && INCLUDE_APPLETALK */

int
main(int argc, char **argv)
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load_KernelEventMonitor(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	prime_KernelEventMonitor();
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
