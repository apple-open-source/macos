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

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <sys/kern_event.h>
#include <errno.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

const char *inetEventName[] = {
	"",
	"INET address added",
	"INET address changed",
	"INET address deleted",
	"INET destination address changed",
	"INET broadcast address changed",
	"INET netmask changed"
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
	"KEV_DL_LINK_ON"
};


SCDSessionRef		session		= NULL;
int			sock;


void
logEvent(CFStringRef evStr, struct kern_event_msg *ev_msg)
{
	int	i;
	int	j;

	SCDLog(LOG_DEBUG, CFSTR("%@ event:"), evStr);
	SCDLog(LOG_DEBUG,
	       CFSTR("  Event size=%d, id=%d, vendor=%d, class=%d, subclass=%d, code=%d"),
	       ev_msg->total_size,
	       ev_msg->id,
	       ev_msg->vendor_code,
	       ev_msg->kev_class,
	       ev_msg->kev_subclass,
	       ev_msg->event_code);
	for (i=0, j=KEV_MSG_HEADER_SIZE; j<ev_msg->total_size; i++, j+=4) {
		SCDLog(LOG_DEBUG, CFSTR("  Event data[%2d] = %08lx"), i, ev_msg->event_data[i]);
	}
}


void
interface_update_status(const char *if_name, CFBooleanRef active)
{
	CFStringRef		interface;
	CFStringRef		key;
	SCDStatus		scd_status;
	SCDHandleRef		handle;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict;
	CFBooleanRef		state;

	/* get the current cache information */
	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						       interface,
						       kSCEntNetLink);
	CFRelease(interface);

	scd_status = SCDGet(session, key, &handle);
	switch (scd_status) {
		case SCD_OK :
			dict    = SCDHandleGetData(handle);
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			if (!CFDictionaryGetValueIfPresent(newDict,
							   kSCPropNetLinkActive,
							   (void **)&state)) {
				state = NULL;	/* we know about the link but not its status */
			}
			break;
		case SCD_NOKEY :
			handle  = SCDHandleInit();
			newDict = CFDictionaryCreateMutable(NULL,
							    0,
							    &kCFTypeDictionaryKeyCallBacks,
							    &kCFTypeDictionaryValueCallBacks);
			state = NULL;
			break;
		default :
			SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
			CFRelease(key);
			return;
	};

	if (active) {
		/* if new status available, update cache */
		CFDictionarySetValue(newDict, kSCPropNetLinkActive, active);
	} else {
		/* if new status not available */
		if (!state) {
			/* if old status was not recorded in the cache */
			goto done;
		}
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkActive);
	}

	/* update status */
	SCDHandleSetData(handle, newDict);
	scd_status = SCDSet(session, key, handle);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("SCDSet() failed: %s"), SCDError(scd_status));
	}

    done :

	SCDHandleRelease(handle);
	CFRelease(newDict);
	CFRelease(key);
	return;
}


void
link_update_status(const char *if_name)
{
	struct ifmediareq	ifm;
	CFBooleanRef		active;

	bzero((char *)&ifm, sizeof(ifm));
	(void) strncpy(ifm.ifm_name, if_name, sizeof(ifm.ifm_name));

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) == -1) {
		/* if media status not available for this interface */
		return;
	}

	if (ifm.ifm_count == 0) {
		/* no media types */
		return;
	}

	if (!(ifm.ifm_status & IFM_AVALID)) {
		/* if active bit not valid */
		return;
	}

	if (ifm.ifm_status & IFM_ACTIVE) {
		active = kCFBooleanTrue;
	} else {
		active = kCFBooleanFalse;
	}

	interface_update_status(if_name, active);

	return;
}


void
link_add(const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		cacheKey;
	SCDStatus		scd_status;
	SCDHandleRef		handle;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;
	CFArrayRef		ifList;
	CFMutableArrayRef	newIFList	= NULL;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	cacheKey  = SCDKeyCreateNetworkInterface(kSCCacheDomainState);

	scd_status = SCDGet(session, cacheKey, &handle);
	switch (scd_status) {
		case SCD_OK :
			dict      = SCDHandleGetData(handle);
			newDict   = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			ifList    = CFDictionaryGetValue(newDict, kSCCachePropNetInterfaces);
			newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			break;
		case SCD_NOKEY :
			handle    = SCDHandleInit();
			newDict   = CFDictionaryCreateMutable(NULL,
							    0,
							    &kCFTypeDictionaryKeyCallBacks,
							    &kCFTypeDictionaryValueCallBacks);
			newIFList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			break;
		default :
			SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
			goto done;
	};

	if (CFArrayContainsValue(newIFList,
				 CFRangeMake(0, CFArrayGetCount(newIFList)),
				 interface)) {
		/*
		 * we already know about this interface
		 */
		goto done;
	}

	CFArrayAppendValue(newIFList, interface);
	CFDictionarySetValue(newDict, kSCCachePropNetInterfaces, newIFList);
	SCDHandleSetData(handle, newDict);
	scd_status = SCDSet(session, cacheKey, handle);
	if (scd_status != SCD_OK)
		SCDLog(LOG_DEBUG, CFSTR("SCDSet() failed: %s"), SCDError(scd_status));
	SCDHandleRelease(handle);

	link_update_status(if_name);

    done:

	CFRelease(cacheKey);
	CFRelease(interface);
	if (newDict)	CFRelease(newDict);
	if (newIFList)	CFRelease(newIFList);

	return;
}


void
link_remove(const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		cacheKey;
	SCDStatus		scd_status;
	SCDHandleRef		handle;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;
	CFArrayRef		ifList;
	CFMutableArrayRef	newIFList	= NULL;
	CFIndex			i;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	cacheKey  = SCDKeyCreateNetworkInterface(kSCCacheDomainState);

	scd_status = SCDGet(session, cacheKey, &handle);
	switch (scd_status) {
		case SCD_OK :
			dict      = SCDHandleGetData(handle);
			newDict   = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			ifList    = CFDictionaryGetValue(newDict, kSCCachePropNetInterfaces);
			newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			break;
		case SCD_NOKEY :
			/* we're not tracking this interface */
			goto done;
		default :
			SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
			goto done;
	};

	if ((i = CFArrayGetFirstIndexOfValue(newIFList,
					     CFRangeMake(0, CFArrayGetCount(newIFList)),
					     interface)) == -1) {
		/* we're not tracking this interface */
		goto done;
	}

	CFArrayRemoveValueAtIndex(newIFList, i);
	CFDictionarySetValue(newDict, kSCCachePropNetInterfaces, newIFList);
	SCDHandleSetData(handle, newDict);
	scd_status = SCDSet(session, cacheKey, handle);
	if (scd_status != SCD_OK)
		SCDLog(LOG_DEBUG, CFSTR("SCDSet() failed: %s"), SCDError(scd_status));
	SCDHandleRelease(handle);

	interface_update_status(if_name, NULL);

    done:

	CFRelease(cacheKey);
	CFRelease(interface);
	if (newDict)	CFRelease(newDict);
	if (newIFList)	CFRelease(newIFList);

	return;
}


void
processEvent_Apple_Network(struct kern_event_msg *ev_msg)
{
	CFStringRef		evStr;
	int			dataLen = (ev_msg->total_size - KEV_MSG_HEADER_SIZE);
	struct net_event_data	*nEvent;
	struct kev_in_data	*iEvent;
	char			ifr_name[IFNAMSIZ];

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
					sprintf(ifr_name, "%s%ld", iEvent->link_data.if_name, iEvent->link_data.if_unit);
					evStr = CFStringCreateWithFormat(NULL,
									 NULL,
									 CFSTR("%s: %s"),
									 ifr_name,
									 inetEventName[ev_msg->event_code]);
					logEvent(evStr, ev_msg);
					CFRelease(evStr);
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
						sprintf(ifr_name, "%s%ld", nEvent->if_name, nEvent->if_unit);
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
						sprintf(ifr_name, "%s%ld", nEvent->if_name, nEvent->if_unit);
						link_remove(ifr_name);
					} else {
						logEvent(CFSTR("KEV_DL_IF_DETACHED"), ev_msg);
					}
					break;

				case KEV_DL_SIFFLAGS :
				case KEV_DL_SIFMETRICS :
				case KEV_DL_SIFMTU :
				case KEV_DL_SIFPHYS :
				case KEV_DL_SIFMEDIA :
				case KEV_DL_SIFGENERIC :
				case KEV_DL_ADDMULTI :
				case KEV_DL_DELMULTI :
				case KEV_DL_IF_DETACHING :
					sprintf(ifr_name, "%s%ld", nEvent->if_name, nEvent->if_unit);
					evStr = CFStringCreateWithFormat(NULL,
									 NULL,
									 CFSTR("%s: %s"),
									 ifr_name,
									 dlEventName[ev_msg->event_code]);
					logEvent(evStr, ev_msg);
					CFRelease(evStr);
					break;

				case KEV_DL_LINK_OFF :
				case KEV_DL_LINK_ON :
					/*
					 * update the link status in the cache
					 */
					if (dataLen == sizeof(struct net_event_data)) {
						sprintf(ifr_name, "%s%ld", nEvent->if_name, nEvent->if_unit);
						interface_update_status(ifr_name,
									(ev_msg->event_code == KEV_DL_LINK_ON)
									? kCFBooleanTrue
									: kCFBooleanFalse);
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

		default :
			logEvent(CFSTR("New Apple network subclass"), ev_msg);
			break;
	}

	return;
}


void
processEvent_Apple_IOKit(struct kern_event_msg *ev_msg)
{
	switch (ev_msg->kev_subclass) {
		default :
			logEvent(CFSTR("New Apple IOKit subclass"), ev_msg);
			break;
	}

	return;
}


void
eventCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
{
	SCDStatus		scd_status;
	int			so		= CFSocketGetNative(s);
	int			status;
	char			buf[1024];
	struct kern_event_msg	*ev_msg		= (struct kern_event_msg *)&buf[0];
	int			offset		= 0;

	status = recv(so, &buf, sizeof(buf), 0);
	if (status == -1) {
		SCDLog(LOG_ERR, CFSTR("recv() failed: %s"), strerror(errno));
		goto error;
	}

	scd_status = SCDLock(session);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDLock() failed: %s"), SCDError(scd_status));
		goto error;
	}

	while (offset < status) {
		if ((offset + ev_msg->total_size) > status) {
			SCDLog(LOG_NOTICE, CFSTR("missed SYSPROTO_EVENT event, buffer not big enough"));
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

	scd_status = SCDUnlock(session);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDUnlock() failed: %s"), SCDError(scd_status));
	}

	return;

    error :

	SCDLog(LOG_ERR, CFSTR("kernel event monitor disabled."));
	CFSocketInvalidate(s);
	return;

}


void
prime()
{
	SCDStatus		scd_status;
	boolean_t		haveLock = FALSE;
	struct ifconf		ifc;
	struct ifreq		*ifr;
	char			buf[1024];
	int			offset;

	SCDLog(LOG_DEBUG, CFSTR("prime() called"));

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		SCDLog(LOG_ERR, CFSTR("could not get interface list, socket() failed: %s"), strerror(errno));
		goto done;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0) {
		SCDLog(LOG_ERR, CFSTR("could not get interface list, ioctl() failed: %s"), strerror(errno));
		goto done;
	}

	scd_status = SCDLock(session);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDLock() failed: %s"), SCDError(scd_status));
		goto done;
	}
	haveLock = TRUE;

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

    done :

	if (haveLock) {
		scd_status = SCDUnlock(session);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_ERR, CFSTR("SCDUnlock() failed: %s"), SCDError(scd_status));
		}
	}

	return;
}


void
start(const char *bundleName, const char *bundleDir)
{
	SCDStatus		scd_status;
	int			so;
	int			status;
	struct kev_request	kev_req;
	CFSocketRef		es;
	CFSocketContext		context = { 0, NULL, NULL, NULL, NULL };
	CFRunLoopSourceRef	rls;

	SCDLog(LOG_DEBUG, CFSTR("start() called"));
	SCDLog(LOG_DEBUG, CFSTR("  bundle name      = %s"), bundleName);
	SCDLog(LOG_DEBUG, CFSTR("  bundle directory = %s"), bundleDir);

	/* open a "configd" session to allow cache updates */
	scd_status = SCDOpen(&session, CFSTR("Kernel/User Notification plug-in"));
	if(scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDOpen() failed: %s"), SCDError(scd_status));
		SCDLog(LOG_ERR, CFSTR("kernel event monitor disabled."));
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
			SCDLog(LOG_ERR, CFSTR("could not establish event filter, ioctl() failed: %s"), strerror(errno));
			(void) close(so);
			so = -1;
		}
	} else {
		SCDLog(LOG_ERR, CFSTR("could not open event socket, socket() failed: %s"), strerror(errno));
	}

	if (so != -1) {
		int	yes = 1;

		status = ioctl(so, FIONBIO, &yes);
		if (status) {
			SCDLog(LOG_ERR, CFSTR("could not set non-blocking io, ioctl() failed: %s"), strerror(errno));
			(void) close(so);
			so = -1;
		}
	}

	if (so == -1) {
		SCDLog(LOG_ERR, CFSTR("kernel event monitor disabled."));
		(void) SCDClose(&session);
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
