/*
 * flow_divert_controller.c
 *
 * Copyright (c) 2012-2013 Apple Inc.
 * All rights reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/VPNTunnelPrivate.h>
#include <SystemConfiguration/VPNAppLayerPrivate.h>
#include <CommonCrypto/CommonRandomSPI.h>

#include <sys/socket.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <net/if.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <netinet/flow_divert_proto.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "scnc_main.h"
#include "scnc_utils.h"
#include "flow_divert_controller.h"
#include "fd_exchange.h"

#define FLOW_DIVERT_TOKEN_KEY_SIZE		256 /* 256 bytes == 2048 bits */

#define FLOW_DIVERT_DNS_SERVER_ADDRESS	"fe80::1"
#define FLOW_DIVERT_DNS_PORT_BASE		6300

struct flow_divert {
	uint32_t	control_unit;
	uint8_t		token_key[FLOW_DIVERT_TOKEN_KEY_SIZE];
};

extern CFRunLoopRef	gControllerRunloop;

static struct flow_divert *
flow_divert_get(struct service *serv, int plugin_index)
{
#define MAX_PLUGINS (sizeof(serv->u.vpn.plugin) / sizeof(serv->u.vpn.plugin[0]))
	if (plugin_index < 0 || plugin_index >= MAX_PLUGINS) {
		int idx;
		for (idx = 0; idx < MAX_PLUGINS; idx++) {
			if (serv->u.vpn.plugin[idx].flow_divert != NULL) {
				plugin_index = idx;
				break;
			}
		}
	}

	if (plugin_index >= 0 && plugin_index < MAX_PLUGINS) {
		return serv->u.vpn.plugin[plugin_index].flow_divert;
	} else {
		return NULL;
	}
}

#define FLOW_DIVERT_DNS_IF_NETWORK_SIZE		64
#define FLOW_DIVERT_DNS_IF_HOST_SIZE		64

static void
flow_divert_ip6_len2mask(struct in6_addr *mask, int len)
{
	int i;
	bzero(mask, sizeof (*mask));
	for (i = 0; i < len / 8; i++) {
		mask->s6_addr[i] = 0xff;
	}
	if (len % 8) {
		mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
	}
}

static int
flow_divert_start_dns_interface(char if_name[IF_NAMESIZE])
{
	int error;
	int if_index = -1;
	int ipv6_sock = -1;
	int utun_sock = -1;
	Boolean success = TRUE;
	struct in6_aliasreq alias_request;
	uint8_t unique_id[FLOW_DIVERT_DNS_IF_HOST_SIZE / 8];

	SCLog(gSCNCDebug, LOG_INFO, CFSTR("flow_divert_start_dns_interface: starting a new utun interface"));

	utun_sock = create_tun_interface(if_name, IF_NAMESIZE, &if_index, 0, 0);
	if (utun_sock < 0 || if_index <= 0) {
		success = FALSE;
		goto done;
	}

	ipv6_sock = socket(AF_INET6, SOCK_DGRAM, 0);
	if (ipv6_sock < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("flow_divert_start_dns_interface: failed to open a IPv6 socket: %s"), strerror(errno));
		success = FALSE;
		goto done;
	}

	memset(&alias_request, 0, sizeof(alias_request));
	strlcpy(alias_request.ifra_name, if_name, sizeof(alias_request.ifra_name));

	if (ioctl(ipv6_sock, SIOCPROTOATTACH_IN6, (caddr_t)&alias_request)) {
		SCLog(TRUE, LOG_ERR, CFSTR("flow_divert_start_dns_interface: SIOCPROTOATTACH_IN6 failed: %s"), strerror(errno));
		success = FALSE;
		goto done;
	}

	alias_request.ifra_addr.sin6_family = AF_INET6;
	alias_request.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	alias_request.ifra_addr.sin6_scope_id = if_index;
	alias_request.ifra_addr.sin6_addr.s6_addr[0] = 0xfe;
	alias_request.ifra_addr.sin6_addr.s6_addr[1] = 0x80;
	error = CCRandomCopyBytes(kCCRandomDefault, unique_id, sizeof(unique_id));
	if (error != kCCSuccess) {
		SCLog(TRUE, LOG_ERR, CFSTR("flow_divert_start_dns_interface: CCRandomCopyBytes failed with error %d"), error);
		success = FALSE;
		goto done;
	}
	unique_id[0] &= ~0x02; /* Local scope (tunnel end-point), see RFC 4941 section 3.2 */
	memcpy(alias_request.ifra_addr.sin6_addr.s6_addr + (FLOW_DIVERT_DNS_IF_NETWORK_SIZE / 8), unique_id, sizeof(unique_id));

	alias_request.ifra_prefixmask.sin6_family = AF_INET6;
	alias_request.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	flow_divert_ip6_len2mask(&alias_request.ifra_prefixmask.sin6_addr, FLOW_DIVERT_DNS_IF_NETWORK_SIZE);

	alias_request.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	alias_request.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

	if (ioctl(ipv6_sock, SIOCLL_START, (caddr_t)&alias_request) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("flow_divert_start_dns_interface: SIOCLL_START failed: %s"), strerror(errno));
		success = FALSE;
		goto done;
	}

done:
	if (!success) {
		if (utun_sock >= 0) {
			close(utun_sock);
			utun_sock = -1;
 		}
 	}
 
	if (ipv6_sock >= 0) {
		close(ipv6_sock);
	}

	return utun_sock;
}

static int
flow_divert_ctl_open(void)
{
	int						sock;
	int						error			= 0;
	struct ctl_info			kernctl_info;
	struct sockaddr_ctl		kernctl_addr;

	sock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (sock < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("failed to open a SYSPROTO_CONTROL socket: %s"), strerror(errno));
		goto done;
	}

	memset(&kernctl_info, 0, sizeof(kernctl_info));
	strlcpy(kernctl_info.ctl_name, FLOW_DIVERT_CONTROL_NAME, sizeof(kernctl_info.ctl_name));

	error = ioctl(sock, CTLIOCGINFO, &kernctl_info);
	if (error) {
		SCLog(TRUE, LOG_ERR, CFSTR("Failed to get the control info for control named \"%s\": %s\n"), 
				FLOW_DIVERT_CONTROL_NAME, strerror(errno));
		goto done;
	}

	memset(&kernctl_addr, 0, sizeof(kernctl_addr));
	kernctl_addr.sc_len = sizeof(kernctl_addr);
	kernctl_addr.sc_family = AF_SYSTEM;
	kernctl_addr.ss_sysaddr = AF_SYS_CONTROL;
	kernctl_addr.sc_id = kernctl_info.ctl_id;
	kernctl_addr.sc_unit = 0;

	error = connect(sock, (struct sockaddr *)&kernctl_addr, sizeof(kernctl_addr));
	if (error) {
		SCLog(TRUE, LOG_ERR, CFSTR("Failed to connect to the flow divert control: %s"), strerror(errno));
		goto done;
	}

done:
	if (error && sock >= 0) {
		close(sock);
		sock = -1;
	}

	return sock;
}

CFDictionaryRef
flow_divert_create_dns_configuration(int control_unit, const char *if_name)
{
	CFDictionaryRef dns_dict = NULL;
	const void *server_strings[] = { CFSTR(FLOW_DIVERT_DNS_SERVER_ADDRESS) };
	int port = FLOW_DIVERT_DNS_PORT_BASE + control_unit;
	const void *keys[4] = {
		kSCPropNetDNSServerAddresses,
		kSCPropNetDNSServerPort,
		kSCPropNetDNSServiceIdentifier,
		kSCPropInterfaceName
	};
	const void *values[4];
	int idx;
  
	values[0] = CFArrayCreate(kCFAllocatorDefault,
	                          server_strings,
	                          sizeof(server_strings) / sizeof(server_strings[0]),
	                          &kCFTypeArrayCallBacks);
	values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &port);
	values[2] = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &control_unit);
	values[3] = CFStringCreateWithCString(kCFAllocatorDefault, if_name, kCFStringEncodingUTF8);
  
	dns_dict = CFDictionaryCreate(kCFAllocatorDefault,
	                              keys,
	                              values,
	                              sizeof(values) / sizeof(values[0]),
	                              &kCFTypeDictionaryKeyCallBacks,
	                              &kCFTypeDictionaryValueCallBacks);
	for (idx = 0; idx < sizeof(values) / sizeof(values[0]); idx++) {
		CFRelease(values[idx]);
  	}
  
	return dns_dict;
}

void
flow_divert_init(struct service *serv, int index)
{
	struct fd_exchange *exchange = serv->u.vpn.plugin[index].fd_exchange;

	if (exchange != NULL) {
		struct flow_divert *flow_divert = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(*flow_divert), 0);

		SCLog(gSCNCDebug, LOG_INFO, CFSTR("Initializing flow divert for plugin %d"), index);

		memset(flow_divert, 0, sizeof(*flow_divert));
		flow_divert->control_unit = FLOW_DIVERT_CTL_UNIT_INVALID;
		serv->u.vpn.plugin[index].flow_divert = flow_divert;

		fd_exchange_set_request_handler(exchange, kVPNFDTypeFlowDivertControl, ^(int fd, const void *ancilliary_data, size_t ancilliary_data_size) {
			int ctl_sock = flow_divert_ctl_open();

			if (ctl_sock >= 0 && flow_divert->control_unit == FLOW_DIVERT_CTL_UNIT_INVALID) {
				int							err;
				VPNFlowTLVIterator			iter;
				uint8_t						log_level		= LOG_NOTICE;
				uint8_t						*msg;
				size_t						msg_size		= 0;
				struct sockaddr_ctl			sac;
				socklen_t					sac_len			= sizeof(sac);
				bool						success			= true;

				SCLog(gSCNCDebug, LOG_INFO, CFSTR("Recevied a request for the flow divert control socket"));

				memset(&sac, 0, sizeof(sac));
				if (getpeername(ctl_sock, (struct sockaddr *)&sac, &sac_len) != 0) {
					SCLog(TRUE, LOG_ERR, CFSTR("Plugin %d: getpeername on control socket failed: %s"), index, strerror(errno));
					success = false;
					goto done;
				}

				flow_divert->control_unit = sac.sc_unit;

				err = CCRandomCopyBytes(kCCRandomDefault, flow_divert->token_key, FLOW_DIVERT_TOKEN_KEY_SIZE);
				if (err != kCCSuccess) {
					SCLog(TRUE, LOG_ERR, CFSTR("Plugin %d: CCRandomCopyBytes failed with error %d"), index, err);
					success = false;
					goto done;
				}

				msg = VPNFlowTLVMsgCreate(0,
				                          FLOW_DIVERT_PKT_GROUP_INIT,
				                          2,
				                          FLOW_DIVERT_TOKEN_KEY_SIZE + sizeof(log_level),
				                          &msg_size,
				                          &iter);

				VPNFlowTLVAdd(&iter, FLOW_DIVERT_TLV_TOKEN_KEY, FLOW_DIVERT_TOKEN_KEY_SIZE, flow_divert->token_key);

				if (gSCNCDebug) {
					log_level = LOG_INFO;
				}
				VPNFlowTLVAdd(&iter, FLOW_DIVERT_TLV_LOG_LEVEL, sizeof(log_level), &log_level);

				err = send(ctl_sock, msg, msg_size, 0);
				if (err < 0) {
					SCLog(TRUE, LOG_ERR, CFSTR("Plugin %d: failed to send the FLOW_DIVERT_PKT_GROUP_INIT message: %s"), index, strerror(errno));
					success = false;
				}

				CFAllocatorDeallocate(kCFAllocatorDefault, msg);
		done:
				if (!success) {
					close(ctl_sock);
					ctl_sock = -1;
					flow_divert->control_unit = FLOW_DIVERT_CTL_UNIT_INVALID;
				}
			}

			if (!fd_exchange_send_response(exchange, kVPNFDTypeFlowDivertControl, ctl_sock, NULL, 0)) {
				SCLog(TRUE, LOG_ERR, CFSTR("Plugin %d: failed to send a flow divert control socket to the plugin"), index);
			}
		});

		fd_exchange_set_request_handler(exchange, kVPNFDTypeFlowDivertDNSUTUN, ^(int fd, const void *ancilliary_data, size_t ancilliary_data_size) {
			int delegate_if_index = 0;
			char if_name[IF_NAMESIZE];
			struct sockaddr_in6 dns_server_addr;
			int ctl_sock = -1;

			memset(&dns_server_addr, 0, sizeof(dns_server_addr));

			if (ancilliary_data != NULL && ancilliary_data_size == sizeof(delegate_if_index)) {
				memcpy(&delegate_if_index, ancilliary_data, ancilliary_data_size);
			}

			if (fd < 0) {
				SCLog(gSCNCDebug, LOG_INFO, CFSTR("Recevied a request for the flow divert DNS TUN socket"));

				if (flow_divert->control_unit != FLOW_DIVERT_CTL_UNIT_INVALID && gDynamicStore != NULL) {
					ctl_sock = flow_divert_start_dns_interface(if_name);

					if (ctl_sock >= 0) {
						CFDictionaryRef dns_dict = flow_divert_create_dns_configuration(flow_divert->control_unit, if_name);
						if (dns_dict != NULL) {
							CFStringRef key =
								SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
								                                            kSCDynamicStoreDomainState,
							                                                serv->serviceID, kSCEntNetDNS);
							SCDynamicStoreSetValue(gDynamicStore, key, dns_dict);
							CFRelease(key);
							CFRelease(dns_dict);
						}

						dns_server_addr.sin6_family = AF_INET6;
						dns_server_addr.sin6_len = sizeof(dns_server_addr);
						inet_pton(AF_INET6, FLOW_DIVERT_DNS_SERVER_ADDRESS, &dns_server_addr.sin6_addr);
						dns_server_addr.sin6_port = htons(FLOW_DIVERT_DNS_PORT_BASE + flow_divert->control_unit);
					}
				}
			} else {
				SCLog(gSCNCDebug, LOG_INFO, CFSTR("Re-setting the delegate interface for the DNS TUN socket"));
				ctl_sock = fd;
			}

			if (ctl_sock >= 0 && delegate_if_index > 0) {
				if (if_indextoname(delegate_if_index, if_name) != NULL) {
					set_tun_delegate(ctl_sock, if_name);
				}
			}

			if (fd < 0) {
				if (!fd_exchange_send_response(exchange,
				                               kVPNFDTypeFlowDivertDNSUTUN,
				                               ctl_sock,
				                               &dns_server_addr,
				                               dns_server_addr.sin6_len))
				{
					SCLog(TRUE, LOG_ERR, CFSTR("Plugin %d: failed to send a flow divert DNS TUN socket to the plugin"), index);
				}
			} else {
				close(fd);
			}
		});
	}
}

void
flow_divert_dispose(struct service *serv, int index)
{
	SCLog(gSCNCDebug, LOG_INFO, CFSTR("Disposing flow divert for plugin %d"), index);
	struct flow_divert *flow_divert = flow_divert_get(serv, index);

	if (flow_divert != NULL) {
		memset(flow_divert, 0, sizeof(*flow_divert));
		CFAllocatorDeallocate(kCFAllocatorDefault, flow_divert);
	}

	serv->u.vpn.plugin[index].flow_divert = NULL;
}

#ifndef kVPNAppLayerTokenControlUnit
#define kVPNAppLayerTokenControlUnit	CFSTR("ControlUnit")
#endif /* kVPNAppLayerTokenControlUnit */

CFDictionaryRef
flow_divert_copy_token_parameters(struct service *serv)
{
	struct flow_divert		*flow_divert = flow_divert_get(serv, -1);
	CFDictionaryRef			parameters = NULL;

	if (flow_divert != NULL && flow_divert->control_unit != FLOW_DIVERT_CTL_UNIT_INVALID) {
		const void	*keys[3];
		const void	*values[3];
		int			log_level = LOG_NOTICE;
		
		if (gSCNCDebug) {
			log_level = LOG_INFO;
		}

		keys[0] = kVPNAppLayerTokenHMACKey;
		values[0] = CFDataCreate(kCFAllocatorDefault, flow_divert->token_key, sizeof(flow_divert->token_key));

		keys[1] = kVPNAppLayerTokenLogLevel;
		values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &log_level);

		keys[2] = kVPNAppLayerTokenControlUnit;
		values[2] = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &flow_divert->control_unit);

		parameters = CFDictionaryCreate(kCFAllocatorDefault,
		                                keys,
		                                values,
		                                sizeof(values) / sizeof(values[0]),
		                                &kCFTypeDictionaryKeyCallBacks,
		                                &kCFTypeDictionaryValueCallBacks);

		CFRelease(values[0]);
		CFRelease(values[1]);
		CFRelease(values[2]);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("flow_divert_copy_token_parameters: No valid flow divert service found"));
	}

	return parameters;
}

CFNumberRef
flow_divert_copy_service_identifier(struct service *serv)
{
	struct flow_divert *flow_divert = flow_divert_get(serv, -1);
	if (flow_divert != NULL && flow_divert->control_unit != FLOW_DIVERT_CTL_UNIT_INVALID) {
		return CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &flow_divert->control_unit);
	}
	return NULL;
}

CFDataRef
flow_divert_copy_match_rules_result(CFDataRef request)
{
	uint32_t flow_id;
	int result = 0;
	pid_t pid = -1;
	uuid_t uuid;
	audit_token_t null_audit = KERNEL_AUDIT_TOKEN_VALUE;
	CFDictionaryRef app_properties;
	uint8_t response[sizeof(flow_id) + sizeof(result)];

	uuid_clear(uuid);

	if (!isA_CFData(request) ||
	    (CFDataGetLength(request) != (sizeof(flow_id) + sizeof(pid) + sizeof(uuid))))
	{
		return NULL;
	}

	CFDataGetBytes(request, CFRangeMake(0, sizeof(flow_id)), (uint8_t *)&flow_id);
	CFDataGetBytes(request, CFRangeMake(sizeof(flow_id), sizeof(pid)), (uint8_t *)&pid);
	CFDataGetBytes(request, CFRangeMake(sizeof(flow_id) + sizeof(pid), sizeof(uuid)), (uint8_t *)uuid);

	app_properties = VPNAppLayerCopyMatchingApp(null_audit, pid, uuid, NULL, FALSE);
	if (app_properties != NULL) {
		result = 1;
		CFRelease(app_properties);
	}

	memcpy(response, &flow_id, sizeof(flow_id));
	memcpy(response + sizeof(flow_id), &result, sizeof(result));

	return CFDataCreate(kCFAllocatorDefault, response, sizeof(response));
}

static CFIndex
flow_divert_get_unique_prefix_count(CFArrayRef signing_ids)
{
	CFIndex id_count = (signing_ids != NULL ? CFArrayGetCount(signing_ids) : 0);
	CFMutableDictionaryRef prefixes = CFDictionaryCreateMutable(kCFAllocatorDefault,
	                                                            0,
	                                                            &kCFTypeDictionaryKeyCallBacks,
	                                                            &kCFTypeDictionaryValueCallBacks);
	CFIndex result;

	if (id_count > 0) {
		char **c_signing_ids;
		CFIndex idx1;
		CFIndex str_len1;

		/* Convert the signing identifiers to ASCII C strings */

		c_signing_ids = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(*c_signing_ids) * id_count, 0);

		for (idx1 = 0; idx1 < id_count; idx1++) {
			CFStringRef id1 = CFArrayGetValueAtIndex(signing_ids, idx1);
			str_len1 = CFStringGetLength(id1);
			CFIndex c_len = CFStringGetMaximumSizeForEncoding(str_len1, kCFStringEncodingASCII);
			CFIndex len = 0;

			c_signing_ids[idx1] = CFAllocatorAllocate(kCFAllocatorDefault, c_len + 1, 0);

			CFStringGetBytes(id1,
			                 CFRangeMake(0, str_len1),
			                 kCFStringEncodingASCII,
			                 '?',
			                 FALSE,
			                 (uint8_t *)c_signing_ids[idx1],
			                 c_len,
							 &len);

			c_signing_ids[idx1][len] = '\0';
		}

		for (idx1 = 0; idx1 < id_count; idx1++) {
			CFIndex idx2;
			str_len1 = strlen(c_signing_ids[idx1]);

			for (idx2 = 0; idx2 < id_count; idx2++) {
				CFIndex str_len2;
				CFIndex str_idx;

				if (idx2 == idx1) {
					continue;
				}

				str_len2 = strlen(c_signing_ids[idx2]);

				/* Find the first byte that differs between the two strings */
				for (str_idx = 0;
				     str_idx < str_len1 && str_idx < str_len2 && c_signing_ids[idx1][str_idx] == c_signing_ids[idx2][str_idx];
				     str_idx++);

				if ((str_idx < str_len1 || str_idx < str_len2) && str_idx > 0) {
					CFStringRef prefix = CFStringCreateWithBytes(kCFAllocatorDefault,
					                                             (const uint8_t *)c_signing_ids[idx1],
					                                             str_idx,
					                                             kCFStringEncodingASCII,
					                                             FALSE);
					if (!CFDictionaryContainsKey(prefixes, prefix)) {
						CFDictionarySetValue(prefixes, prefix, prefix);
					}
					CFRelease(prefix);
				} /* else the strings are equal or do not have a common prefix */
			}
		}

		for (idx1 = 0; idx1 < id_count; idx1++) {
			CFAllocatorDeallocate(kCFAllocatorDefault, c_signing_ids[idx1]);
		}
		CFAllocatorDeallocate(kCFAllocatorDefault, c_signing_ids);
	}

	result = CFDictionaryGetCount(prefixes);

	CFRelease(prefixes);

	return result;
}

void
flow_divert_set_signing_ids(CFArrayRef signing_ids)
{
	int					ctl_sock;
	VPNFlowTLVIterator	iter;
	uint8_t				*msg;
	size_t				msg_size		= 0;
	size_t				payload_size	= 0;
	CFIndex				idx;
	CFIndex				id_count		= (signing_ids != NULL ? CFArrayGetCount(signing_ids) : 0);
	int					prefix_count	= (int)flow_divert_get_unique_prefix_count(signing_ids);

	for (idx = 0; idx < id_count; idx++) {
		CFStringRef curr_id = CFArrayGetValueAtIndex(signing_ids, idx);
		payload_size += CFStringGetMaximumSizeForEncoding(CFStringGetLength(curr_id), kCFStringEncodingASCII);
	}

	payload_size += sizeof(prefix_count);

	msg = VPNFlowTLVMsgCreate(0,
	                          FLOW_DIVERT_PKT_APP_MAP_CREATE,
	                          id_count + 1,
	                          payload_size,
	                          &msg_size,
	                          &iter);

	VPNFlowTLVAdd(&iter, FLOW_DIVERT_TLV_PREFIX_COUNT, sizeof(prefix_count), &prefix_count);

	for (idx = 0; idx < id_count; idx++) {
		VPNFlowTLVAddCFString(&iter,
		                      FLOW_DIVERT_TLV_SIGNING_ID,
		                      (CFStringRef)CFArrayGetValueAtIndex(signing_ids, idx),
		                      kCFStringEncodingASCII);
	}

	ctl_sock = flow_divert_ctl_open();
	if (ctl_sock >= 0) {
		if (send(ctl_sock, msg, msg_size - iter.remaining, 0) < 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("Failed to send a FLOW_DIVERT_PKT_APP_MAP_CREATE message: %s"), strerror(errno));
		}
		close(ctl_sock);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("Failed to send a FLOW_DIVERT_PKT_APP_MAP_CREATE message: could not create a kernel control socket"));
	}

	CFAllocatorDeallocate(kCFAllocatorDefault, msg);
}
