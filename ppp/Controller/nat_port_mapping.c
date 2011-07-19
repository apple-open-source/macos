/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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

#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#include <SystemConfiguration/SCPreferencesPathKey.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
#include <mach/task_special_ports.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <notify.h>
#include <sys/sysctl.h>
#include <ifaddrs.h>
#include <SystemConfiguration/SCNetworkSignature.h>

#include "scnc_mach_server.h"
#include "scnc_main.h"
#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "scnc_client.h"
#include "ipsec_manager.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_socket_server.h"
#include "scnc_utils.h"
#include "if_ppplink.h"
#include "PPPControllerPriv.h"
#include "pppd.h"

static void
startProbe (struct service *serv)
{
    if (serv->type == TYPE_IPSEC) {
        racoon_send_cmd_start_dpd(serv->u.ipsec.controlfd,
                                  serv->u.ipsec.peer_address.sin_addr.s_addr);
        serv->u.ipsec.awaiting_peer_resp = 1;
    }
}

static const char *
getPortMappingType (struct service *serv)
{
	if (serv) {
		switch (serv->type) {
			case TYPE_IPSEC: return (const char *)"IPSec";
		}
	}
	
	return (const char *)"INVALID";
}

static int
isServiceConnected (struct service *serv)
{
	switch (serv->type) {
		case TYPE_IPSEC: return (serv->u.ipsec.phase != IPSEC_IDLE);
	}
	return 0;
}

static const char *
getInterfaceName (struct service *serv)
{
	if (serv) {
		switch (serv->type) {
			case TYPE_IPSEC: return (const char*)serv->u.ipsec.lower_interface;
		}
	}
	return (const char *)"NULL";
}

static u_int32_t
getInterfaceNameSize (struct service *serv)
{
	if (serv) {
		switch (serv->type) {
			case TYPE_IPSEC: return sizeof(serv->u.ipsec.lower_interface);
		}
	}
	return 4; // "NULL"
}

static uint32_t
getInterfaceIndex (struct service *serv)
{
	const char *str = getInterfaceName(serv);
	
	if (str)
		return if_nametoindex(str);
	return 0;
}

static void
clearOnePortMapping (mdns_nat_mapping_t *mapping)
{
	if (mapping) {
		// what about mapping->mDNSRef_fd?
		if (mapping->mDNSRef != NULL) {
			DNSServiceRefDeallocate(mapping->mDNSRef);
		}
		bzero(mapping, sizeof(*mapping));
	}
}

static int
ignorePortMappingUpdate (DNSServiceRef       sdRef,
						 struct service     *serv,
						 char               *sd_name,
						 char               *if_name,
						 uint32_t            if_name_siz,
						 uint32_t            interfaceIndex,
						 uint32_t            publicAddress,
						 DNSServiceProtocol  protocol,
						 uint16_t            privatePort,
						 uint16_t            publicPort)
{
    int i = 0, vpn = 0, found = 0;
    struct ifaddrs *ifap = NULL;
	u_int8_t interfaceName[sizeof(serv->u.ipsec.lower_interface)];
	
	/* check if address still exist */
	if (getifaddrs(&ifap) == 0) {
		struct ifaddrs *ifa;
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_name  
				&& ifa->ifa_addr
				&& (!strncmp(ifa->ifa_name, "utun", 4) || !strncmp(ifa->ifa_name, "ppp", 3))
				&& ifa->ifa_addr->sa_family == AF_INET
				&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == htonl(publicAddress)) {
				SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping update for %s ignored: related to VPN interface. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
					   sd_name,
					   ifa->ifa_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
				vpn = 1;
			}
			if (ifa->ifa_name  
				&& ifa->ifa_addr
				&& !strncmp(ifa->ifa_name, if_name, if_name_siz)
				&& ifa->ifa_addr->sa_family == AF_INET
				&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == serv->u.ipsec.our_address.sin_addr.s_addr) {
				found = 1;
			}
		}
		freeifaddrs(ifap);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("%s port-mapping update for %s ignored: failed to get interface list\n"),
			   sd_name,
			   if_name);
		return 1;
	}
	
	if (vpn) {
		return 1;
	}
	
	if_indextoname(interfaceIndex, (char *)interfaceName);
	if (!strncmp((const char *)interfaceName, (const char *)if_name, if_name_siz)) {
		if (strstr(if_name, "ppp") || strstr(if_name, "utun")) {
			// change on PPP interface... we don't care
			SCLog(TRUE, LOG_NOTICE, CFSTR("%s port-mapping update for %s ignored: underlying interface is PPP/VPN. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
				   sd_name,
				   if_name,
				   publicAddress,
				   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
				   privatePort,
				   publicPort);
			return 1;
		} else {
			/* check if address still exist */
			if (found) {
				return 0;
			} else {
				if (!publicAddress || (!publicPort && privatePort)) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping update for %s ignored: underlying interface down. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
						   sd_name,
						   if_name,
						   publicAddress,
						   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
						   privatePort,
						   publicPort);
					for (i = 0; i < serv->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
						if (serv->nat_mapping[i].mDNSRef_tmp == sdRef) {
							serv->nat_mapping[i].up = 0;
							SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s flagged down because of no connectivity\n"), sd_name, if_name);
						}
					}
					return 1;
				}
				SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping update for %s ignored: underlying interface's address changed. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
					   sd_name,
					   if_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
				return 1;
			}
		}
	} else if (publicAddress && !publicPort && !privatePort && !protocol) {
		// an update on the NAT's public IP
		return 0;
	} else {
		// public interface disappeared?
		if (!publicAddress && !publicPort && !privatePort && !protocol) {
			return 0;
		}
		/* check if address still exist */
		if (serv->type == TYPE_IPSEC && serv->u.ipsec.our_address.sin_addr.s_addr == htonl(publicAddress) && found) {
			return 0;
		}
		// change due to another interface, ignore for now
		SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping update for %s ignored: not for interface %s. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
			   sd_name,
			   interfaceName,
			   if_name,
			   publicAddress,
			   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
			   privatePort,
			   publicPort);
		return 1;
	}
}

#define FAR_FUTURE          (60.0 * 60.0 * 24.0 * 365.0 * 1000.0)
#define PUBLIC_NAT_PORT_MAPPING_TIMEOUT 20

static void
stop_public_nat_port_mapping_timer (struct service *serv)
{
	if (serv->type == TYPE_IPSEC) {
		if (serv->u.ipsec.port_mapping_timerref) {
			CFRunLoopRemoveTimer(serv->u.ipsec.port_mapping_timerrun, serv->u.ipsec.port_mapping_timerref, kCFRunLoopCommonModes);
			my_CFRelease(&serv->u.ipsec.port_mapping_timerref);
		}
	}
}

static void
public_port_mapping_timeout (CFRunLoopTimerRef timer, void *info)
{
	struct service *serv = info;
	
	if (serv->type == TYPE_IPSEC) {
		SCLog(TRUE, LOG_ERR, CFSTR("NAT's public interface down for more than %d secs... starting faster probe.\n"),
			   PUBLIC_NAT_PORT_MAPPING_TIMEOUT);
		startProbe(serv);
	}
}

static void
start_public_nat_port_mapping_timer (struct service *serv)
{
    CFRunLoopTimerContext	context = { 0, serv, NULL, NULL, NULL };
	
	if (serv->type == TYPE_IPSEC) {
		if (serv->u.ipsec.interface_timerref || serv->u.ipsec.port_mapping_timerref) {
			return;
		}
		
		SCLog(TRUE, LOG_INFO, CFSTR("starting wait-port-mapping timer for IPSec: %d secs\n"), PUBLIC_NAT_PORT_MAPPING_TIMEOUT);
		serv->u.ipsec.port_mapping_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + PUBLIC_NAT_PORT_MAPPING_TIMEOUT, FAR_FUTURE, 0, 0, public_port_mapping_timeout, &context);
		if (!serv->u.ipsec.port_mapping_timerref) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: public interface down, cannot create RunLoop timer... starting faster probe.\n"));
			startProbe(serv);
			return;
		}
		CFRunLoopAddTimer(serv->u.ipsec.port_mapping_timerrun, serv->u.ipsec.port_mapping_timerref, kCFRunLoopCommonModes);
	}
}

static void clearPortMapping (struct service *serv);

static void
setPortMappingCallback (DNSServiceRef        sdRef,
						DNSServiceFlags      flags,
						uint32_t             interfaceIndex,
						DNSServiceErrorType  errorCode,
						uint32_t             nPublicAddress,	   /* four byte IPv4 address in network byte order */
						DNSServiceProtocol   protocol,
						uint16_t             nPrivatePort,
						uint16_t             nPublicPort,	   /* may be different than the requested port */
						uint32_t             ttl,			   /* may be different than the requested ttl */
						void                *context)
{
	uint32_t publicAddress = ntohl(nPublicAddress);
	uint16_t privatePort = ntohs(nPrivatePort);
	uint16_t publicPort = ntohs(nPublicPort);
	int i;
	struct service *serv = (__typeof__(serv))context;
	const char *sd_name = getPortMappingType(serv);
	int is_connected = isServiceConnected(serv);
	const char *if_name = getInterfaceName(serv);
	u_int32_t if_name_siz = getInterfaceNameSize(serv);

	if (serv->u.ipsec.modecfg_defaultroute) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("%s port-mapping update for %s ignored: VPN is the Primary interface. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
			  sd_name,
			  if_name,
			  publicAddress,
			  (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
			  privatePort,
			  publicPort);
		clearPortMapping(serv);
		return;
	}

	if (errorCode != kDNSServiceErr_NoError && errorCode != kDNSServiceErr_DoubleNAT) {
		SCLog(TRUE, LOG_ERR, CFSTR("%s failed to set port-mapping for %s, errorCode: %d\n"), sd_name, if_name, errorCode);
		if (errorCode == kDNSServiceErr_NATPortMappingUnsupported || errorCode == kDNSServiceErr_NATPortMappingDisabled) {
			for (i = 0; i < serv->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
				if (serv->nat_mapping[i].mDNSRef_tmp == sdRef) {
					SCLog(TRUE, LOG_ERR, CFSTR("%s port-mapping for %s became invalid. is Connected: %d, Protocol: %s, Private Port: %d, Previous publicAddress: (%x), Previous publicPort: (%d)\n"),
						   sd_name,
						   if_name,
						   is_connected,
						   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
						   privatePort,
						   serv->nat_mapping[i].reflexive.addr,
						   serv->nat_mapping[i].reflexive.port);
					if (serv->nat_mapping[i].up && is_connected) {
						SCLog(TRUE, LOG_NOTICE, CFSTR("%s public port-mapping for %s changed... starting faster probe.\n"), sd_name, if_name);
						startProbe(serv);
					} else {
						clearPortMapping(serv);
					}
					return;
				}
			}
		} else if (errorCode == kDNSServiceErr_ServiceNotRunning) {
			clearPortMapping(serv);
		}
		return;
	}
	
	if (ignorePortMappingUpdate(sdRef, serv, (char *)sd_name, (char *)if_name, if_name_siz, interfaceIndex, publicAddress, protocol, privatePort, publicPort)) {
		return;
	}
	
	SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s, Protocol: %s, Private Port: %d, Public Address: %x, Public Port: %d, TTL: %d%s\n"),
		   sd_name,
		   if_name,
		   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
		   privatePort,
		   publicAddress,
		   publicPort,
		   ttl,
		   (errorCode == kDNSServiceErr_DoubleNAT)? " (Double NAT)" : ".");
	
	// generate a disconnect if we were already connected and we just detected a change in publicAddress or publicPort
	for (i = 0; i < serv->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
		if (serv->nat_mapping[i].mDNSRef_tmp == sdRef) {
			if (serv->nat_mapping[i].up && !publicAddress && !publicPort) {
				SCLog(TRUE, LOG_NOTICE, CFSTR("%s port-mapping for %s indicates public interface down. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
					   sd_name,
					   if_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
				start_public_nat_port_mapping_timer(serv);
				return;
			} else if (serv->nat_mapping[i].up) {
				stop_public_nat_port_mapping_timer(serv);				
			}
			
			if (serv->type == TYPE_IPSEC && serv->u.ipsec.our_address.sin_addr.s_addr == htonl(publicAddress) &&
				privatePort == publicPort) {
				SCLog(TRUE, LOG_NOTICE, CFSTR("%s port-mapping update for %s indicates no NAT. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n"),
					   sd_name,
					   if_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
			}
			
			if (serv->nat_mapping[i].interfaceIndex == interfaceIndex &&
				serv->nat_mapping[i].protocol == protocol &&
				serv->nat_mapping[i].privatePort == privatePort) {
				SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s consistent. is Connected: %d, interface: %d, protocol: %d, privatePort: %d\n"),
					   sd_name, if_name, is_connected, interfaceIndex, protocol, privatePort);
			} else {
				// inconsistency (mostly because of mdns api only works for the primary interface): TODO revise
				if (serv->nat_mapping[i].interfaceIndex != interfaceIndex) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s inconsistent. is Connected: %d, Previous interface: %d, Current interface %d\n"),
						   sd_name, if_name, is_connected, serv->nat_mapping[i].interfaceIndex, interfaceIndex);
					serv->nat_mapping[i].interfaceIndex = interfaceIndex;
				}
				if (serv->nat_mapping[i].protocol != protocol) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s inconsistent. is Connected: %d, Previous protocol: %x, Current protocol %x\n"),
						   sd_name, if_name, is_connected, serv->nat_mapping[i].protocol, protocol);
					serv->nat_mapping[i].protocol = protocol;
				} else if (serv->nat_mapping[i].privatePort != privatePort) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s inconsistent. is Connected: %d, Previous privatePort: %d, Current privatePort %d\n"),
						   sd_name, if_name, serv->nat_mapping[i].privatePort, privatePort);
					serv->nat_mapping[i].privatePort = privatePort;
				}
			}
			// is mapping up?
			if (!serv->nat_mapping[i].up) {
				if (serv->nat_mapping[i].reflexive.addr != publicAddress) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s initialized. is Connected: %d, Previous publicAddress: (%x), Current publicAddress %x\n"),
						   sd_name, if_name, is_connected, serv->nat_mapping[i].reflexive.addr, publicAddress);
					serv->nat_mapping[i].reflexive.addr = publicAddress;
				}
				if (serv->nat_mapping[i].reflexive.port != publicPort) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s initialized. is Connected: %d, Previous publicPort: (%d), Current publicPort %d\n"),
						   sd_name, if_name, is_connected, serv->nat_mapping[i].reflexive.port, publicPort);
					serv->nat_mapping[i].reflexive.port = publicPort;
				}
				if (serv->nat_mapping[i].reflexive.addr && 
				    ((!privatePort && !serv->nat_mapping[i].reflexive.port) || (serv->nat_mapping[i].reflexive.port))) {
					// flag up mapping
					serv->nat_mapping[i].up = 1;
					SCLog(TRUE, LOG_INFO, CFSTR("%s port-mapping for %s fully initialized. Flagging up\n"), sd_name, if_name);
				}
				return;
			}
			
			if (serv->nat_mapping[i].reflexive.addr != publicAddress) {
				SCLog(TRUE, LOG_ERR, CFSTR("%s port-mapping for %s changed. is Connected: %d, Previous publicAddress: (%x), Current publicAddress %x\n"),
					   sd_name, if_name, is_connected, serv->nat_mapping[i].reflexive.addr, publicAddress);
				if (is_connected) {
					if (!privatePort || publicAddress) {
						SCLog(TRUE, LOG_ERR, CFSTR("NAT's public address down or changed... starting faster probe.\n"));
						startProbe(serv);
						return;
					}
					// let function that handles (KEV_INET_NEW_ADDR || KEV_INET_CHANGED_ADDR || KEV_INET_ADDR_DELETED) deal with the connection
					return;
				}
				serv->nat_mapping[i].reflexive.addr = publicAddress;
				return;
			}
			if (serv->nat_mapping[i].reflexive.port != publicPort) {
				SCLog(TRUE, LOG_ERR, CFSTR("%s port-mapping for %s changed. is Connected: %d, Previous publicPort: (%d), Current publicPort %d\n"),
					   sd_name, if_name, is_connected, serv->nat_mapping[i].reflexive.port, publicPort);
				if (is_connected) {
					if (!privatePort || publicPort) {
						SCLog(TRUE, LOG_ERR, CFSTR("NAT's public port down or changed... starting faster probe.\n"));
						startProbe(serv);
						return;
					}
					// let function that handles (KEV_INET_NEW_ADDR || KEV_INET_CHANGED_ADDR || KEV_INET_ADDR_DELETED) deal with the connection
					return;
				}
				serv->nat_mapping[i].reflexive.port = publicPort;
				return;
			}
			if (errorCode == kDNSServiceErr_DoubleNAT) {
				SCLog(TRUE, LOG_NOTICE, CFSTR("%s port-mapping for %s hasn't changed, however there's a Double NAT.  is Connected: %d\n"),
					   sd_name, if_name, is_connected);
			}
			return;
		}
	}
	return;
}

static void
clearPortMapping (struct service *serv)
{
	int i;
	const char *sd_name = getPortMappingType(serv);
	const char *if_name = getInterfaceName(serv);

	stop_public_nat_port_mapping_timer(serv);

	if (!serv->nat_mapping_cnt)
		return;
	
	SCLog(TRUE, LOG_INFO, CFSTR("%s clearing port-mapping for %s\n"), sd_name, if_name);
	
	for (i = 0; i < serv->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
		clearOnePortMapping(&serv->nat_mapping[i]);
	}
	serv->nat_mapping_cnt = 0;
}

static int
setPortMapping (struct service     *serv,
				mdns_nat_mapping_t *mapping,
				uint32_t            interfaceIndex,
				DNSServiceProtocol  protocol,
				uint16_t            privatePort,
				int                 probeOnly)
{
	DNSServiceErrorType err = kDNSServiceErr_NoError;
	uint16_t publicPort;
	uint32_t ttl;
	const char *sd_name = getPortMappingType(serv);
	const char *if_name = getInterfaceName(serv);
	
	if (mapping == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%s invalid mapping pointer for %s\n"), sd_name, if_name);
		return -1;
	}
	
	if (!probeOnly) {
		publicPort = 0;
		ttl = 2 * 3600; // 2 hours
	} else {
		publicPort = 0;
		ttl = 0;
		protocol = 0;
		privatePort = 0;
		interfaceIndex = 0;
	}
	
	if (mapping->mDNSRef == NULL) {
		err = DNSServiceCreateConnection(&mapping->mDNSRef);
		if (err != kDNSServiceErr_NoError || mapping->mDNSRef == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("%s Error calling DNSServiceCreateConnection for %s, error: %d\n"), sd_name, if_name, err);
			clearOnePortMapping(mapping);
			return -1;
		}
		err = DNSServiceSetDispatchQueue(mapping->mDNSRef, dispatch_get_current_queue());
		if (err != kDNSServiceErr_NoError) {
			SCLog(TRUE, LOG_ERR, CFSTR("%s Error calling DNSServiceSetDispatchQueue for %s, error: %d\n"), sd_name, if_name, err);
			clearOnePortMapping(mapping);
			return -1;
		}
		if (!serv->u.ipsec.port_mapping_timerrun) {
			serv->u.ipsec.port_mapping_timerrun = (__typeof__(serv->u.ipsec.port_mapping_timerrun))my_CFRetain(CFRunLoopGetCurrent());
		}
	}
	mapping->mDNSRef_tmp = mapping->mDNSRef;
	err = DNSServiceNATPortMappingCreate(&mapping->mDNSRef_tmp, kDNSServiceFlagsShareConnection, interfaceIndex, protocol, htons(privatePort), publicPort, ttl, setPortMappingCallback, serv);
	if (err != kDNSServiceErr_NoError) {
		SCLog(TRUE, LOG_ERR, CFSTR("%s Error calling DNSServiceNATPortMappingCreate for %s, error: %d\n"), sd_name, if_name, err);
		clearOnePortMapping(mapping);
		return -1;
	}
	mapping->interfaceIndex = interfaceIndex;
	mapping->protocol = protocol;
	mapping->privatePort = privatePort;
	bzero(&mapping->reflexive, sizeof(mapping->reflexive));
	
	SCLog(TRUE, LOG_INFO, CFSTR("%s set port-mapping for %s, interface: %d, protocol: %d, privatePort: %d\n"), sd_name, if_name, interfaceIndex, protocol, privatePort);
	return 0;
}

static void
ipsecSetPortMapping (struct service *serv)
{
	const char *if_name = getInterfaceName(serv);
	u_int32_t interfaceIndex = getInterfaceIndex(serv);
	
	// exit early if interface is PPP/VPN
	if (strstr(if_name, "ppp") || strstr(if_name, "utun")) {
		return;
	}
	
	if (setPortMapping(serv, &serv->nat_mapping[0], interfaceIndex, 0, 0, 1) == 0) {
		serv->nat_mapping_cnt++;
	}
#if 0 // <rdar://problem/8001582>
	if (setPortMapping(serv, &serv->nat_mapping[1], interfaceIndex, kDNSServiceProtocol_UDP, (u_int16_t)500, 0) == 0) {
		serv->nat_mapping_cnt++;
	}
	if (setPortMapping(serv, &serv->nat_mapping[2], interfaceIndex, kDNSServiceProtocol_UDP, (u_int16_t)4500, 0) == 0) {
		serv->nat_mapping_cnt++;
	}
#endif
}

void
nat_port_mapping_set (struct service *serv)
{
	if (!serv)
		return;

	if (serv->u.ipsec.modecfg_defaultroute) {
		const char *sd_name = getPortMappingType(serv);
		const char *if_name = getInterfaceName(serv);

		SCLog(TRUE, LOG_NOTICE, CFSTR("%s port-mapping API for %s ignored: VPN is the primary interface.\n"),
			  sd_name,
			  if_name);
		return;
	}

	if (serv->nat_mapping_cnt) {
		clearPortMapping(serv);
	}
	
	switch (serv->type) {
		case TYPE_PPP: break;
		case TYPE_IPSEC: ipsecSetPortMapping(serv); break;
	}	
}

void
nat_port_mapping_clear (struct service *serv)
{
	if (!serv)
		return;
	
	clearPortMapping(serv);
}
