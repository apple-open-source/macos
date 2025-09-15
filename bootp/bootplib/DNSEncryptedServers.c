/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <mach/boolean.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCSchemaDefinitionsPrivate.h>
#include "symbol_scope.h"
#include "cfutil.h"
#include "util.h"
#include "nbo.h"
#include "DNSNameList.h"
#include "DNSEncryptedServers.h"

/*
 * ref: RFC 9463
 *
 * DHCPv4
 *
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | OPTION_V4_DNR |     Length    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ~      DNR Instance Data #1     ~
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   ---
 .              ...              .    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ optional
 ~      DNR Instance Data #n     ~    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   ---
 *
 * where an instance is defined as
 *
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |    DNR Instance Data Length   |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |       Service Priority        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   ADN Length  |               |
 +-+-+-+-+-+-+-+-+               |
 ~  authentication-domain-name   ~
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  Addr Length  |               |
 +-+-+-+-+-+-+-+-+               |
 ~        IPv4 Address(es)       ~
 |               +-+-+-+-+-+-+-+-+
 |               |               |
 +-+-+-+-+-+-+-+-+               |
 ~Service Parameters (SvcParams) ~
 |                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * DHCPv6
 *
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |       OPTION_V6_DNR           |         Option-length         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |       Service Priority        |         ADN Length            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ~                   authentication-domain-name                  ~
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |         Addr Length           |                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 ~                        ipv6-address(es)                       ~
 |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                               |                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 ~                 Service Parameters (SvcParams)                ~
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * RA
 *
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     Type      |     Length    |        Service Priority       |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Lifetime                            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |          ADN Length           |                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 ~                   authentication-domain-name                  ~
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |         Addr Length           |                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 ~                        ipv6-address(es)                       ~
 |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                               |     SvcParams Length          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ~                 Service Parameters (SvcParams)                ~
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * note: no ADN-only mode support yet
 *
 */

#define DNR_INSTANCE_LEN_SIZE_dhcp 	2
#define SERVICE_PRIORITY_SIZE 		2
#define ADN_LEN_SIZE_v4 		1
#define ADN_LEN_MIN 			1 // min FQDN len
#define ADN_LEN_MAX 			255 // max FQDN len
#define ADDR_LEN_SIZE_v4 		1
#define ADDR_BYTES_LEN_v4		4
#define DNR_INSTANCE_MIN_LEN_dhcpv4 	\
(SERVICE_PRIORITY_SIZE			\
+ ADN_LEN_SIZE_v4			\
+ ADN_LEN_MIN				\
+ ADDR_LEN_SIZE_v4			\
+ ADDR_BYTES_LEN_v4) // 9

#define ADN_LEN_SIZE_v6 		2
#define ADDR_LEN_SIZE_v6 		2
#define ADDR_BYTES_LEN_v6		16
#define DNR_INSTANCE_MIN_LEN_dhcpv6 	\
(SERVICE_PRIORITY_SIZE			\
+ ADN_LEN_SIZE_v6			\
+ ADN_LEN_MIN				\
+ ADDR_LEN_SIZE_v6			\
+ ADDR_BYTES_LEN_v6) // 23

#define ND_OPT_TYPE_SIZE 		1
#define DNR_INSTANCE_LEN_SIZE_ra 	1
#define LIFETIME_SIZE			4
#define SVC_PARAMS_LEN_SIZE 		2
#define DNR_INSTANCE_MIN_LEN_ra 	\
(SERVICE_PRIORITY_SIZE			\
+ LIFETIME_SIZE				\
+ ADN_LEN_SIZE_v6			\
+ ADN_LEN_MIN				\
+ ADDR_LEN_SIZE_v6			\
+ ADDR_BYTES_LEN_v6			\
+ SVC_PARAMS_LEN_SIZE) // 29

typedef enum {
	kDNRInstanceTypeDHCPv4 = 0,
	kDNRInstanceTypeDHCPv6,
	kDNRInstanceTypeRA
} DNRInstanceType_t;

typedef struct DNSEncryptedServer {
	DNRInstanceType_t type;
	uint16_t service_priority;
	CFStringRef adn;
	/*
	 * Either a struct in_addr *
	 * or a struct in6_addr *.
	 */
	void * addr_list;
	int addr_list_count;
	uint8_t * svc_params_data;
	uint16_t svc_params_data_len;
} DNSEncryptedServer, * DNSEncryptedServerRef;

STATIC void
DNSEncryptedServerFree(DNSEncryptedServerRef dnr_instance)
{
	if (dnr_instance == NULL) {
		goto done;
	}
	my_CFRelease(&dnr_instance->adn);
	if (dnr_instance->addr_list != NULL) {
		free(dnr_instance->addr_list);
		dnr_instance->addr_list = NULL;
	}
	if (dnr_instance->svc_params_data != NULL) {
		free(dnr_instance->svc_params_data);
		dnr_instance->svc_params_data = NULL;
	}
	free(dnr_instance);
done:
	return;
}

STATIC bool
DNSEncryptedServerCreateWithDHCPv4Data(DNSEncryptedServerRef * dnr_instance_p,
				       const uint8_t * dnr_data,
				       int dnr_data_len,
				       int * offset_p)
{
	uint8_t 		addr_len = 0;
	struct in_addr *	addr_list = NULL;
	int 			addr_list_count = 0;
	int			addr_list_count_max = 0;
	CFStringRef 		adn = NULL;
	uint8_t 		adn_len = 0;
	DNSEncryptedServerRef 	dnr_instance = NULL;
	uint16_t 		instance_len = 0;
	int			offset = 0;
	int 			remaining_data_bytes = 0;
	int 			remaining_instance_bytes = 0;
	uint16_t 		service_priority = 0;
	uint8_t *		svc_params_data = NULL;
	uint16_t 		svc_params_data_len = 0;
	const uint8_t *		scan = 0;
	bool 			success = FALSE;

	offset = *offset_p;
	if (offset < 0 || offset >= dnr_data_len) {
		goto done;
	}
	remaining_data_bytes = dnr_data_len - offset;
	if (remaining_data_bytes < DNR_INSTANCE_MIN_LEN_dhcpv4) {
		/* not enough bytes for an instance */
		goto done;
	}
	scan = dnr_data + offset;
	instance_len = net_uint16_get(scan);
	remaining_instance_bytes = instance_len + DNR_INSTANCE_LEN_SIZE_dhcp;
	if (remaining_data_bytes < remaining_instance_bytes) {
		/* instance vs data len mismatch */
		goto done;
	}
	scan += DNR_INSTANCE_LEN_SIZE_dhcp;
	remaining_instance_bytes -= DNR_INSTANCE_LEN_SIZE_dhcp;
	if (remaining_instance_bytes
	    < (SERVICE_PRIORITY_SIZE + ADN_LEN_SIZE_v4)) {
		/* truncated */
		goto done;
	}
	service_priority = net_uint16_get(scan);
	scan += SERVICE_PRIORITY_SIZE;
	remaining_instance_bytes -= SERVICE_PRIORITY_SIZE;
	adn_len = *scan;
	scan += ADN_LEN_SIZE_v4;
	remaining_instance_bytes -= ADN_LEN_SIZE_v4;
	if (adn_len < ADN_LEN_MIN || adn_len > ADN_LEN_MAX) {
		goto done;
	}
	if (remaining_instance_bytes < adn_len) {
		goto done;
	}
	adn = DNSNameStringCreate(scan, adn_len, false);
	if (adn == NULL) {
		/* bad ADN */
		goto done;
	}
	scan += adn_len;
	remaining_instance_bytes -= adn_len;
	if (remaining_instance_bytes < ADDR_LEN_SIZE_v4) {
		goto done;
	}
	addr_len = *scan;
	scan += ADDR_LEN_SIZE_v4;
	remaining_instance_bytes -= ADDR_LEN_SIZE_v4;
	if (remaining_instance_bytes < addr_len
	    || addr_len % sizeof(*addr_list) != 0) {
		goto done;
	}
	addr_list_count_max = addr_len / sizeof(*addr_list);
	if (remaining_instance_bytes
	    < (addr_list_count_max * sizeof(*addr_list))) {
		/* bad addrs section */
		goto done;
	}
	addr_list = (struct in_addr *)
	malloc(addr_list_count_max * sizeof(*addr_list));
	for (int i = 0; i < addr_list_count_max; i++) {
		struct in_addr ip = { 0 };
		uint32_t ip_addr = 0;

		memcpy(&ip.s_addr, scan, sizeof(ip));
		scan += sizeof(ip);
		remaining_instance_bytes -= sizeof(ip);
		ip_addr = ntohl(ip.s_addr);
		if (ip_addr == INADDR_ANY
		    || ip_addr == INADDR_BROADCAST
		    || ip_addr == INADDR_LOOPBACK
		    || IN_MULTICAST(ip_addr)) {
			/*
			 * ignores 0.0.0.0, 255.255.255.255,
			 * 127.0.0.1, 224.0.0.0/4
			 */
			continue;
		}
		addr_list[addr_list_count++] = ip;
	}
	if (addr_list_count == 0) {
		free(addr_list);
		addr_list = NULL;
		goto done;
	}
	svc_params_data_len = remaining_instance_bytes;
	if (svc_params_data_len > 0) {
		svc_params_data = (uint8_t *)malloc(svc_params_data_len);
		memcpy(svc_params_data, scan, svc_params_data_len);
		scan += svc_params_data_len;
	}
	offset = (scan - dnr_data);
	*offset_p = offset;

	dnr_instance = (DNSEncryptedServerRef)malloc(sizeof(*dnr_instance));
	bzero(dnr_instance, sizeof(*dnr_instance));
	dnr_instance->type = kDNRInstanceTypeDHCPv4;
	dnr_instance->service_priority = service_priority;
	dnr_instance->adn = adn;
	dnr_instance->addr_list = addr_list;
	dnr_instance->addr_list_count = addr_list_count;
	dnr_instance->svc_params_data = svc_params_data;
	dnr_instance->svc_params_data_len = svc_params_data_len;
	*dnr_instance_p = dnr_instance;
	success = TRUE;

done:
	if (!success) {
		my_CFRelease(&adn);
		if (addr_list != NULL) {
			free(addr_list);
		}
		if (svc_params_data != NULL) {
			free(svc_params_data);
		}
		if (dnr_instance != NULL) {
			free(dnr_instance);
		}
	}
	return success;
}

STATIC bool
DNSEncryptedServerCreateWithDHCPv6Data(DNSEncryptedServerRef * dnr_instance_p,
				       const uint8_t * dnr_data,
				       int dnr_data_len)
{
	uint8_t 		addr_len = 0;
	struct in6_addr *	addr_list = NULL;
	int 			addr_list_count = 0;
	int			addr_list_count_max = 0;
	CFStringRef 		adn = NULL;
	uint16_t 		adn_len = 0;
	DNSEncryptedServerRef 	dnr_instance = NULL;
	int 			remaining_instance_bytes = 0;
	uint16_t 		service_priority = 0;
	uint8_t *		svc_params_data = NULL;
	uint16_t 		svc_params_data_len = 0;
	const uint8_t *		scan = 0;
	bool 			success = FALSE;

	/* dnr instance len is the dhcpv6 opt len */
	if (dnr_data_len < DNR_INSTANCE_MIN_LEN_dhcpv6) {
		/* not enough bytes for an instance */
		goto done;
	}
	scan = dnr_data;
	remaining_instance_bytes = dnr_data_len;
	service_priority = net_uint16_get(scan);
	scan += SERVICE_PRIORITY_SIZE;
	remaining_instance_bytes -= SERVICE_PRIORITY_SIZE;
	if (remaining_instance_bytes < ADN_LEN_SIZE_v6) {
		goto done;
	}
	adn_len = net_uint16_get(scan);
	scan += ADN_LEN_SIZE_v6;
	remaining_instance_bytes -= ADN_LEN_SIZE_v6;
	if (adn_len < ADN_LEN_MIN || adn_len > ADN_LEN_MAX) {
		goto done;
	}
	if (remaining_instance_bytes < adn_len) {
		goto done;
	}
	adn = DNSNameStringCreate(scan, adn_len, false);
	if (adn == NULL) {
		goto done;
	}
	scan += adn_len;
	remaining_instance_bytes -= adn_len;
	if (remaining_instance_bytes < ADDR_LEN_SIZE_v6) {
		goto done;
	}
	addr_len = net_uint16_get(scan);
	scan += ADDR_LEN_SIZE_v6;
	remaining_instance_bytes -= ADDR_LEN_SIZE_v6;
	if (remaining_instance_bytes < addr_len
	    || addr_len % sizeof(*addr_list) != 0) {
		goto done;
	}
	addr_list_count_max = addr_len / sizeof(*addr_list);
	if (remaining_instance_bytes
	    < (addr_list_count_max * sizeof(*addr_list))) {
		goto done;
	}
	addr_list = (struct in6_addr *)
	malloc(addr_list_count_max * sizeof(*addr_list));
	for (int i = 0; i < addr_list_count_max; i++) {
		struct in6_addr ip6 = { 0 };

		memcpy(&ip6, scan, sizeof(ip6));
		scan += sizeof(ip6);
		remaining_instance_bytes -= sizeof(ip6);
		if (IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)&ip6)
		    || IN6_IS_ADDR_MULTICAST((struct in6_addr *)&ip6)
		    || IN6_IS_ADDR_LOOPBACK((struct in6_addr *)&ip6)) {
			/* ignores ::/128, ff::/8 and ::1/128 */
			continue;
		}
		addr_list[addr_list_count++] = ip6;
	}
	if (addr_list_count == 0) {
		goto done;
	}
	svc_params_data_len = remaining_instance_bytes;
	if (svc_params_data_len > 0) {
		svc_params_data = (uint8_t *)malloc(svc_params_data_len);
		memcpy(svc_params_data, scan, svc_params_data_len);
	}

	dnr_instance = (DNSEncryptedServerRef)malloc(sizeof(*dnr_instance));
	bzero(dnr_instance, sizeof(*dnr_instance));
	dnr_instance->type = kDNRInstanceTypeDHCPv6;
	dnr_instance->service_priority = service_priority;
	dnr_instance->adn = adn;
	dnr_instance->addr_list = addr_list;
	dnr_instance->addr_list_count = addr_list_count;
	dnr_instance->svc_params_data = svc_params_data;
	dnr_instance->svc_params_data_len = svc_params_data_len;
	*dnr_instance_p = dnr_instance;
	success = TRUE;

done:
	if (!success) {
		my_CFRelease(&adn);
		if (addr_list != NULL) {
			free(addr_list);
		}
		if (svc_params_data != NULL) {
			free(svc_params_data);
		}
		if (dnr_instance != NULL) {
			free(dnr_instance);
		}
	}
	return success;
}

STATIC bool
DNSEncryptedServerCreateWithRAData(DNSEncryptedServerRef * dnr_instance_p,
				   const uint8_t * dnr_data,
				   int dnr_data_len)
{
	uint8_t 		addr_len = 0;
	struct in6_addr *	addr_list = NULL;
	int 			addr_list_count = 0;
	int			addr_list_count_max = 0;
	CFStringRef 		adn = NULL;
	uint16_t 		adn_len = 0;
	DNSEncryptedServerRef 	dnr_instance = NULL;
	int 			remaining_instance_bytes = 0;
	uint16_t 		service_priority = 0;
	uint8_t *		svc_params_data = NULL;
	uint16_t 		svc_params_data_len = 0;
	const uint8_t *		scan = 0;
	bool 			success = FALSE;

	if (dnr_data_len < DNR_INSTANCE_MIN_LEN_ra) {
		goto done;
	}
	/* this points at the svc priority field already */
	scan = dnr_data;
	remaining_instance_bytes = dnr_data_len;
	service_priority = net_uint16_get(scan);
	scan += SERVICE_PRIORITY_SIZE;
	remaining_instance_bytes -= SERVICE_PRIORITY_SIZE;
	/* have to account for 'lifetime' field */
	/* this has been processes by the caller already, skip */
	scan += LIFETIME_SIZE;
	remaining_instance_bytes -= LIFETIME_SIZE;
	if (remaining_instance_bytes < ADN_LEN_SIZE_v6) {
		goto done;
	}
	adn_len = net_uint16_get(scan);
	scan += ADN_LEN_SIZE_v6;
	remaining_instance_bytes -= ADN_LEN_SIZE_v6;
	if (adn_len < ADN_LEN_MIN || adn_len > ADN_LEN_MAX
	    || remaining_instance_bytes < adn_len) {
		goto done;
	}
	adn = DNSNameStringCreate(scan, adn_len, false);
	if (adn == NULL) {
		goto done;
	}
	scan += adn_len;
	remaining_instance_bytes -= adn_len;
	if (remaining_instance_bytes < ADDR_LEN_SIZE_v6) {
		goto done;
	}
	addr_len = net_uint16_get(scan);
	scan += ADDR_LEN_SIZE_v6;
	remaining_instance_bytes -= ADDR_LEN_SIZE_v6;
	if (remaining_instance_bytes < addr_len
	    || addr_len % sizeof(*addr_list) != 0) {
		goto done;
	}
	addr_list_count_max = addr_len / sizeof(*addr_list);
	if (remaining_instance_bytes
	    < (addr_list_count_max * sizeof(*addr_list))) {
		goto done;
	}
	addr_list = (struct in6_addr *)
	malloc(addr_list_count_max * sizeof(*addr_list));
	for (int i = 0; i < addr_list_count_max; i++) {
		struct in6_addr ip6 = { 0 };

		memcpy(&ip6, scan, sizeof(ip6));
		scan += sizeof(ip6);
		remaining_instance_bytes -= sizeof(ip6);
		if (IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)&ip6)
		    || IN6_IS_ADDR_MULTICAST((struct in6_addr *)&ip6)
		    || IN6_IS_ADDR_LOOPBACK((struct in6_addr *)&ip6)) {
			/* ignores ::/128, ff::/8 and ::1/128 */
			continue;
		}
		addr_list[addr_list_count++] = ip6;
	}
	if (addr_list_count == 0) {
		goto done;
	}
	if (remaining_instance_bytes < SVC_PARAMS_LEN_SIZE) {
		goto done;
	}
	svc_params_data_len = net_uint16_get(scan);
	scan += SVC_PARAMS_LEN_SIZE;
	remaining_instance_bytes -= SVC_PARAMS_LEN_SIZE;
	if (svc_params_data_len > remaining_instance_bytes) {
		goto done;
	}
	if (svc_params_data_len > 0) {
		svc_params_data = (uint8_t *)malloc(svc_params_data_len);
		memcpy(svc_params_data, scan, svc_params_data_len);
		remaining_instance_bytes -= svc_params_data_len;
	}
	if (remaining_instance_bytes > 0) {
		/* zero padding, ignore */
	}

	dnr_instance = (DNSEncryptedServerRef)malloc(sizeof(*dnr_instance));
	bzero(dnr_instance, sizeof(*dnr_instance));
	dnr_instance->type = kDNRInstanceTypeRA;
	dnr_instance->service_priority = service_priority;
	dnr_instance->adn = adn;
	dnr_instance->addr_list = addr_list;
	dnr_instance->addr_list_count = addr_list_count;
	dnr_instance->svc_params_data = svc_params_data;
	dnr_instance->svc_params_data_len = svc_params_data_len;
	*dnr_instance_p = dnr_instance;
	success = TRUE;

done:
	if (success) {
		return dnr_instance;
	}
	my_CFRelease(&adn);
	if (addr_list != NULL) {
		free(addr_list);
	}
	if (svc_params_data != NULL) {
		free(svc_params_data);
	}
	if (dnr_instance != NULL) {
		free(dnr_instance);
	}
	return NULL;
}

STATIC CFDictionaryRef
DNSEncryptedServerEntryCreate(DNSEncryptedServerRef dnr_instance,
			      DNRInstanceType_t	type)
{
	bool success = FALSE;
	CFMutableDictionaryRef encrypted_server = NULL;
	CFNumberRef service_priority = NULL;
	CFStringRef authentication_domain_name = NULL;
	CFMutableArrayRef ip_addresses = NULL;
	CFDataRef service_parameters = NULL;
	CFIndex svc_params_len = 0;
	CFIndex signed_long_mask = 0xffff;

	encrypted_server = CFDictionaryCreateMutable(NULL, 0,
						     &kCFTypeDictionaryKeyCallBacks,
						     &kCFTypeDictionaryValueCallBacks);
	service_priority = CFNumberCreate(NULL,
					  kCFNumberShortType,
					  &dnr_instance->service_priority);
	if (service_priority == NULL) {
		goto done;
	}
	CFDictionarySetValue(encrypted_server,
			     kSCPropNetDNSEncryptedServerServicePriority,
			     service_priority);
	authentication_domain_name = CFRetain(dnr_instance->adn);
	if (authentication_domain_name == NULL) {
		goto done;
	}
	CFDictionarySetValue(encrypted_server,
			     kSCPropNetDNSEncryptedServerAuthenticationDomainName,
			     authentication_domain_name);
	ip_addresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (ip_addresses == NULL) {
		goto done;
	}
	if (type == kDNRInstanceTypeDHCPv4) {
		const struct in_addr * addr = NULL;
		int increment = sizeof(struct in_addr);

		for (int i = 0;
		     i < dnr_instance->addr_list_count * increment;
		     i += increment) {
			CFStringRef addr_str = NULL;

			addr = (struct in_addr *)&dnr_instance->addr_list[i];
			addr_str = my_CFStringCreateWithIPAddress(*addr);
			if (addr_str == NULL) {
				continue;
			}
			CFArrayAppendValue(ip_addresses, addr_str);
			CFRelease(addr_str);
		}
	} else {
		const struct in6_addr * addr = NULL;
		int increment = sizeof(struct in6_addr);

		for (int i = 0;
		     i < dnr_instance->addr_list_count * increment;
		     i += increment) {
			CFStringRef addr_str = NULL;

			addr = (struct in6_addr *)&dnr_instance->addr_list[i];
			addr_str =
			my_CFStringCreateWithIPv6Address(addr);
			if (addr_str == NULL) {
				continue;
			}
			CFArrayAppendValue(ip_addresses, addr_str);
			CFRelease(addr_str);
		}
	}
	CFDictionarySetValue(encrypted_server,
			     kSCPropNetDNSEncryptedServerAddresses,
			     ip_addresses);
	/* unsigned short to signed long conversion */
	svc_params_len = signed_long_mask & dnr_instance->svc_params_data_len;
	service_parameters = CFDataCreate(NULL,
					  dnr_instance->svc_params_data,
					  svc_params_len);
	if (service_parameters == NULL) {
		goto done;
	}
	CFDictionarySetValue(encrypted_server,
			     kSCPropNetDNSEncryptedServerServiceParameters,
			     service_parameters);
	success = TRUE;

done:
	if (!success) {
		my_CFRelease(encrypted_server);
	}
	my_CFRelease(&service_priority);
	my_CFRelease(&authentication_domain_name);
	my_CFRelease(&ip_addresses);
	my_CFRelease(&service_parameters);
	return (CFDictionaryRef)encrypted_server;
}

PRIVATE_EXTERN CFComparisonResult
DNSEncryptedServerListSortByServicePriority(const void * val1,
					    const void * val2,
					    void * context)
{
	CFDictionaryRef entry1 = (CFDictionaryRef)val1;
	CFNumberRef priority1 = NULL;
	CFDictionaryRef entry2 = (CFDictionaryRef)val2;
	CFNumberRef priority2 = NULL;
#define _UNKNOWN_RESULT -2
#define _DEPRIORITIZATION_RESULT(v) (((v) == 1) \
? kCFCompareGreaterThan \
: kCFCompareLessThan)
	CFComparisonResult result = _UNKNOWN_RESULT;

	if (!isA_CFDictionary(entry1)) {
		result = _DEPRIORITIZATION_RESULT(1);
		goto done;
	}
	priority1 = (CFNumberRef)
	CFDictionaryGetValue(entry1,
			     kSCPropNetDNSEncryptedServerServicePriority);
	if (!isA_CFNumber(priority1)) {
		result = _DEPRIORITIZATION_RESULT(1);
		goto done;
	}
	if (!isA_CFDictionary(entry2)) {
		result = _DEPRIORITIZATION_RESULT(2);
		goto done;
	}
	priority2 = (CFNumberRef)
	CFDictionaryGetValue(entry2,
			     kSCPropNetDNSEncryptedServerServicePriority);
	if (!isA_CFNumber(priority2)) {
		result = _DEPRIORITIZATION_RESULT(2);
		goto done;
	}
	result = CFNumberCompare(priority1, priority2, NULL);
#undef _UNKNOWN_RESULT
#undef _DEPRIORITIZATION_RESULT

done:
	return result;
}

PRIVATE_EXTERN void
DNSEncryptedServerListAppendUniqueEntry(CFMutableArrayRef server_entries,
					CFDictionaryRef encrypted_server)
{
	CFMutableArrayRef single_server_container = NULL;

	single_server_container =
	CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(single_server_container, encrypted_server);
	my_CFMutableArrayMergeArray(server_entries,
				    single_server_container,
				    DNSEncryptedServerListCompareEntries);
	CFRelease(single_server_container);

	return;
}

PRIVATE_EXTERN CFArrayRef
DNSEncryptedServerListCreateWithDHCPv4Data(const uint8_t * opt_dnr_data,
					   int opt_dnr_data_len)
{
	CFMutableArrayRef server_entries = NULL;
	int remaining_data_bytes = opt_dnr_data_len;
	int current_offset = 0;
	bool success = FALSE;

	server_entries = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	while (remaining_data_bytes > 0) {
		DNSEncryptedServerRef dnr_instance = NULL;
		CFDictionaryRef encrypted_server = NULL;

		success =
		DNSEncryptedServerCreateWithDHCPv4Data(&dnr_instance,
						   opt_dnr_data,
						   opt_dnr_data_len,
						   &current_offset);
		if (success == FALSE || dnr_instance == NULL) {
			break;
		}
		encrypted_server =
		DNSEncryptedServerEntryCreate(dnr_instance,
					      kDNRInstanceTypeDHCPv4);
		DNSEncryptedServerFree(dnr_instance);
		dnr_instance = NULL;
		if (encrypted_server == NULL) {
			success = FALSE;
			break;
		}
		/* filters out dupes */
		DNSEncryptedServerListAppendUniqueEntry(server_entries,
							encrypted_server);
		CFRelease(encrypted_server);

		/* next dnr instance */
		remaining_data_bytes = opt_dnr_data_len - current_offset;
	}
	if (!success) {
		/* ignores all DNR instances if any failed parsing */
		my_CFRelease(&server_entries);
		goto done;
	}
	/* sort servers by their stated service priority */
	CFArraySortValues(server_entries,
			  CFRangeMake(0, CFArrayGetCount(server_entries)),
			  DNSEncryptedServerListSortByServicePriority,
			  NULL);

done:
	return (CFArrayRef)server_entries;
}

PRIVATE_EXTERN CFDictionaryRef
DNSEncryptedServerEntryCreateWithDHCPv6Data(const uint8_t * opt_dnr_data,
					    int opt_dnr_data_len)
{
	CFDictionaryRef server_entry = NULL;
	DNSEncryptedServerRef dnr_instance = NULL;
	bool success = FALSE;

	success = DNSEncryptedServerCreateWithDHCPv6Data(&dnr_instance,
							 opt_dnr_data,
							 opt_dnr_data_len);
	if (success == FALSE || dnr_instance == NULL) {
		goto done;
	}
	server_entry = DNSEncryptedServerEntryCreate(dnr_instance,
						     kDNRInstanceTypeDHCPv6);
	DNSEncryptedServerFree(dnr_instance);
	dnr_instance = NULL;
	if (server_entry == NULL) {
		goto done;
	}
	success = TRUE;

done:
	if (!success) {
		my_CFRelease(&server_entry);
	}
	return server_entry;
}

PRIVATE_EXTERN CFDictionaryRef
DNSEncryptedServerEntryCreateWithRAData(const uint8_t * opt_dnr_data,
					int opt_dnr_data_len)
{
	CFDictionaryRef server_entry = NULL;
	DNSEncryptedServerRef dnr_instance = NULL;
	bool success = FALSE;

	success = DNSEncryptedServerCreateWithRAData(&dnr_instance,
						     opt_dnr_data,
						     opt_dnr_data_len);
	if (success == FALSE || dnr_instance == NULL) {
		goto done;
	}
	server_entry = DNSEncryptedServerEntryCreate(dnr_instance,
						     kDNRInstanceTypeRA);
	DNSEncryptedServerFree(dnr_instance);
	dnr_instance = NULL;
	if (server_entry == NULL) {
		goto done;
	}
	success = TRUE;

done:
	if (!success) {
		my_CFRelease(&server_entry);
	}
	return server_entry;
}

PRIVATE_EXTERN CFComparisonResult
DNSEncryptedServerListCompareEntries(const void * entry1, const void * entry2,
				     void * context)
{
	CFComparisonResult res = kCFCompareLessThan;
	CFDictionaryRef d1 = (CFDictionaryRef)entry1;
	CFDictionaryRef d2 = (CFDictionaryRef)entry2;
	CFStringRef adn1 = NULL;
	CFStringRef adn2 = NULL;
	bool adn_only = FALSE;
	CFNumberRef priority1 = NULL;
	CFNumberRef priority2 = NULL;
	CFArrayRef addrs1 = NULL;
	CFMutableSetRef addrs1_set = NULL;
	CFArrayRef addrs2 = NULL;
	CFDataRef params1 = NULL;
	CFDataRef params2 = NULL;
	CFRange params_range = { 0 };

	if (CFDictionaryGetCount(d1) != CFDictionaryGetCount(d2)) {
		goto done;
	}
	if (CFDictionaryGetCount(d1) == 1
	    && CFDictionaryGetValue(d1, kSCPropNetDNSEncryptedServerAuthenticationDomainName) != NULL
	    && CFDictionaryGetValue(d2, kSCPropNetDNSEncryptedServerAuthenticationDomainName) != NULL) {
		/* ADN-only mode */
		adn_only = TRUE;
	}
	adn1 =
	CFDictionaryGetValue(d1, kSCPropNetDNSEncryptedServerAuthenticationDomainName);
	adn2 =
	CFDictionaryGetValue(d2, kSCPropNetDNSEncryptedServerAuthenticationDomainName);
	res = CFStringCompare(adn1, adn2, 0);
	if (res != kCFCompareEqualTo) {
		goto done;
	}
	if (adn_only) {
		goto done;
	}
	res = kCFCompareLessThan;
	priority1 = CFDictionaryGetValue(d1, kSCPropNetDNSEncryptedServerServicePriority);
	priority2 = CFDictionaryGetValue(d2, kSCPropNetDNSEncryptedServerServicePriority);
	if (priority1 == NULL || priority2 == NULL) {
		goto done;
	}
	res = CFNumberCompare(priority1, priority2, NULL);
	if (res != kCFCompareEqualTo) {
		goto done;
	}
	res = kCFCompareLessThan;
	addrs1 = CFDictionaryGetValue(d1, kSCPropNetDNSEncryptedServerAddresses);
	addrs2 = CFDictionaryGetValue(d2, kSCPropNetDNSEncryptedServerAddresses);
	if (addrs1 == NULL || addrs2 == NULL
	    || (CFArrayGetCount(addrs1) != CFArrayGetCount(addrs2))) {
		goto done;
	}
	addrs1_set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	for (CFIndex i = 0; i < CFArrayGetCount(addrs1); i++) {
		CFSetAddValue(addrs1_set, CFArrayGetValueAtIndex(addrs1, i));
	}
	for (CFIndex j = 0; j < CFArrayGetCount(addrs2); j++) {
		CFStringRef s2 = CFArrayGetValueAtIndex(addrs2, j);

		if (!CFSetContainsValue(addrs1_set, s2)) {
			goto done;
		}
	}
	res = kCFCompareLessThan;
	params1 = CFDictionaryGetValue(d1, kSCPropNetDNSEncryptedServerServiceParameters);
	params2 = CFDictionaryGetValue(d2, kSCPropNetDNSEncryptedServerServiceParameters);
	if (params1 == NULL && params2 == NULL) {
		/* we allow empty svc params */
		res = kCFCompareEqualTo;
		goto done;
	}
	else if (params1 == NULL || params2 == NULL
	    || CFDataGetLength(params1) != CFDataGetLength(params2)) {
		goto done;
	}
	params_range = CFDataFind(params1,
				  params2,
				  CFRangeMake(0, CFDataGetLength(params1)),
				  0);
	if (params_range.length != CFDataGetLength(params1)) {
		goto done;
	}
	res = kCFCompareEqualTo;

done:
	my_CFRelease(&addrs1_set);
	return res;
}

#ifdef TEST_DNSDNRINSTANCE

STATIC void
DNSEncryptedServerListPrint(CFArrayRef entries, DNRInstanceType_t type)
{
	CFIndex count = 0;

	printf("%s = {\n", CFStringGetCStringPtr(kSCPropNetDNSEncryptedServers,
						 kCFStringEncodingUTF8));
	if (entries == NULL) {
		goto done;
	}
	count = CFArrayGetCount(entries);
	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef instance = NULL;
		CFNumberRef svc_priority = NULL;
		int svc_priority_int = 0;
		CFStringRef adn = NULL;
		char adn_buf[ADN_LEN_MAX] = { 0 };
		CFArrayRef addrs = NULL;
		CFIndex addrs_count = 0;

		CFDataRef svc_params = NULL;

		printf("\t{\n");
		instance = CFArrayGetValueAtIndex(entries, i);
		svc_priority =
		CFDictionaryGetValue(instance,
				     kSCPropNetDNSEncryptedServerServicePriority);
		CFNumberGetValue(svc_priority, kCFNumberIntType, &svc_priority_int);
		printf("\t\t%s : <number> '%hu',\n",
		       CFStringGetCStringPtr(kSCPropNetDNSEncryptedServerServicePriority,
					     kCFStringEncodingUTF8),
		       svc_priority_int);
		adn =
		CFDictionaryGetValue(instance,
				     kSCPropNetDNSEncryptedServerAuthenticationDomainName);
		printf("\t\t%s : <string> ",
		       CFStringGetCStringPtr(kSCPropNetDNSEncryptedServerAuthenticationDomainName,
					     kCFStringEncodingUTF8));
		if (CFStringGetCString(adn,
				       adn_buf,
				       ADN_LEN_MAX,
				       kCFStringEncodingUTF8)) {
			printf("'%s'", adn_buf);
		} else {
			printf("error");
		}
		printf(",\n");
		addrs =
		CFDictionaryGetValue(instance,
				     kSCPropNetDNSEncryptedServerAddresses);
		printf("\t\t%s : <array> {\n",
		       CFStringGetCStringPtr(kSCPropNetDNSEncryptedServerAddresses,
					     kCFStringEncodingUTF8));
		addrs_count = CFArrayGetCount(addrs);
		for (CFIndex j = 0; j < addrs_count; j++) {
			CFStringRef addr = NULL;
			char * addr_buf;
			int addr_buf_size = 0;
			bool good = FALSE;

			addr = CFArrayGetValueAtIndex(addrs, j);
			addr_buf_size =
			(type == kDNRInstanceTypeDHCPv4)
			? INET_ADDRSTRLEN + 1
			: INET6_ADDRSTRLEN + 1;
			addr_buf = (char *)malloc(addr_buf_size);
			good = CFStringGetCString(addr,
						  addr_buf,
						  addr_buf_size,
						  kCFStringEncodingUTF8);
			printf("\t\t\t%s,\n", good ? addr_buf : "error");
			free(addr_buf);
		}
		printf("\t\t},\n");
		svc_params =
		CFDictionaryGetValue(instance,
				     kSCPropNetDNSEncryptedServerServiceParameters);
		printf("\t\t%s : <data> {\n",
		       CFStringGetCStringPtr(kSCPropNetDNSEncryptedServerServiceParameters,
					     kCFStringEncodingUTF8));
		printf("\n\t\t\t");
		print_data(CFDataGetBytePtr(svc_params),
			   CFDataGetLength(svc_params));
		printf("\n\t\t}\n");

		printf("\t},\n");
	}
done:
	printf("}\n");
}


#include <netinet/icmp6.h>
#include "dhcp_options.h"
#include "DHCPv6Options.h"
#include "RouterAdvertisement.h"

static const uint8_t dhcpv4_good1[] = {
	0xa2, 0x26, // DHCP opt 162, optlen
	0x00, 0x24, // instance_len
	0x00, 0x00, // service_priority
	0x0c, 0x04, // adn_len, adn
	't', 'e', // adn cont'd
	's', 't',
	0x05, 'l',
	'o', 'c',
	'a', 'l', 0x00,
	0x04, 0x4b, // ip_addr_len, ip_addr
	0x4b, 0x4c, // ip_addr cont'd
	0x4c, 0x02, // ip_addr cont'd, svc_params
	0x01, 0x06, // svc_params cont'd
	0x40, 0xc0,
	0xa8, 0x20,
	0x10, 0xff,
	0x00, 0x00,
	0x49, 0x4e,
	0x46, 0x4f,
	0x00, 0x00,
};

static const uint8_t dhcpv4_good2[] = {
	// dhcp dnr opt header
	0xa2, 0x31,
	// instance #1
	0x00, 0x16,
	// priority 3 should sort
	// below priority 0
	0x00, 0x03,
	0x0b, 0x03,
	'o', 'n',
	'e',
	0x05, 'l',
	'o', 'c',
	'a', 'l', 0x00,
	0x04, 0x11,
	0x07, 0x07,
	0x07, 0x02,
	0x01, 0x06,
	// instance #2
	0x00, 0x17,
	// priority 0
	0x00, 0x00,
	0x0b, 0x03,
	't', 'w',
	'o',
	0x05, 'l',
	'o', 'c',
	'a', 'l', 0x00,
	0x08, 0x01,
	0x01, 0x01,
	0x01, 0x08,
	0x08, 0x08,
	0x08, // no svc params
};

static const uint8_t dhcpv4_bad1[] = {
	0xa2, 0x28,
	0x00, 0x26,
	0x00, 0x00,
	0x0d, 0x04, // bad adn len
	't', 'e',
	's', 't',
	0x05, 'l',
	'o', 'c',
	'a', 'l', 0x00,
	0x04, 0x4b, // overread in addrs
	0x4b, 0x4c,
	0x4c, 0x02,
	0x01, 0x06,
	0x40, 0xc0,
	0xa8, 0x20,
	0x10, 0xff,
	0x00, 0x00,
	0x49, 0x4e,
	0x46, 0x4f,
	0x00, 0x00,
	0x00, 0x00,
};

static const uint8_t dhcpv6_good1[] = {
	0x0, 0x17, 0x0, 0x20, // some other option
	0x20, 0x1, 0x5, 0x58,
	0xfe, 0xed, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x1,
	0x20, 0x1, 0x5, 0x58,
	0xfe, 0xed, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x2,
	0x00, 0x90, 0x00, 0x27, // DHCPv6 opt 144, optlen
	0x00, 0x01, 0x00, 0x0c, // svc prio, adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x10, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x11, 0x11,
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00,
};

/* merge within same optlist and merge with ra_good2 */
static const uint8_t dhcpv6_good2[] = {
	0x00, 0x90, 0x00, 0x28, // DHCPv6 opt 144, optlen
	0x00, 0x03, 0x00, 0x0c, // svc prio, adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x10, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x00, 0x03,
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00,
	0x00, 0x90, 0x00, 0x28, // DHCPv6 opt 144, optlen
	0x00, 0x03, 0x00, 0x0c, // svc prio, adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x10, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x00, 0x03,
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00,
};

/* this DHCPv6 DNR merges with below RA DNR */
static const uint8_t dhcpv6_good3[] = {
	0x00, 0x90, 0x00, 0x38,
	0x00, 0x00, 0x00, 0x0c,
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x20, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x11, 0x01,
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x11, 0x02,
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00
};

static const uint8_t ra_good1[] = {
	0x90, 0x08, 0x00, 0x00, // ND opt 144, optlen, svc priority
	0x00, 0x00, 0x04, 0x00, // lifetime
	0x00, 0x0c, // adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x20, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x11, 0x01,
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x11, 0x02,
	0x00, 0x04, // svc params len
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00 // padding
};

static const uint8_t ra_bad1[] = {
	0x90, 0x30, 0x00, 0x00, // ND opt 144, optlen (bad), svc priority
	0x00, 0x00, 0x04, 0x00, // lifetime
	0x00, 0x0c, // adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x10, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x11, 0x01,
	0x00, 0x06, // svc params len
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00
};

/* merge within same RA optlist and merge with dhcpv6_good2 */
static const uint8_t ra_good2[] = {
	0x90, 0x06, 0x00, 0x03, // ND opt 144, optlen, svc priority
	0x00, 0x00, 0x08, 0x00, // lifetime
	0x00, 0x0c, // adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x10, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x00, 0x03,
	0x00, 0x06, // svc params len
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00,
	0x90, 0x06, 0x00, 0x03, // ND opt 144, optlen, svc priority
	0x00, 0x00, 0x08, 0x00, // lifetime
	0x00, 0x0c, // adn len
	6, 't', 'e', 's', 't', 'v', '6', 3, 'c', 'o', 'm', 0, // adn
	0x00, 0x10, // addr len
	0xfd, 0x88, 0x77, 0x77, 0x66, 0x66, 0x55, 0x55,
	0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x00, 0x03,
	0x00, 0x06, // svc params len
	0x49, 0x4e, // svc params
	0x46, 0x4f,
	0x00, 0x00,
};

static const struct buf {
	const uint8_t *buf;
	const size_t buf_size;
	const bool ra;
} bufsv4[] = {
	{ dhcpv4_good1, sizeof(dhcpv4_good1), FALSE },
	{ dhcpv4_good2, sizeof(dhcpv4_good2), FALSE },
	{ dhcpv4_bad1, sizeof(dhcpv4_bad1), FALSE },
	{ NULL, 0 }
};

static const struct buf bufsv6[] = {
	{ dhcpv6_good1, sizeof(dhcpv6_good1), FALSE },
	{ dhcpv6_good2, sizeof(dhcpv6_good2), FALSE },
	{ dhcpv6_good3, sizeof(dhcpv6_good3), FALSE },
	{ ra_good1, sizeof(ra_good1), TRUE },
	{ ra_bad1, sizeof(ra_bad1), TRUE },
	{ ra_good2, sizeof(ra_good2), TRUE },
	{ NULL, 0 }
};

#ifndef ND_OPT_DNR

struct nd_opt_dnr {
	u_int8_t	nd_opt_dnr_type;
	u_int8_t	nd_opt_dnr_len;
	u_int8_t	nd_opt_dnr_svc_priority[2];
	u_int8_t	nd_opt_dnr_lifetime[4];
	u_int8_t	nd_opt_dnr_adn_len[2];
	u_int8_t        nd_opt_dnr_continuation[1];
} __attribute__((__packed__));

#define ND_OPT_DNR 144 /* RFC 9463 */
#define ND_OPT_DNR_MIN_LENGTH offsetof(struct nd_opt_dnr, nd_opt_dnr_continuation)

#endif /* ND_OPT_DNR */

#ifndef ND_OPT_ALIGN
#define ND_OPT_ALIGN 8
#endif

int
main(int argc, char * argv[])
{
	CFMutableArrayRef entries_dhcpv6 = NULL;
	CFMutableArrayRef entries_ra = NULL;
	int ntest = 1;

	/* test dhcpv4 */
	for (const struct buf *b = bufsv4; b->buf != NULL; b++) {
		dhcpol_t optlist = { 0 };
		uint8_t *dnr_data = NULL;
		int dnr_data_len = 0;
		CFArrayRef entries = NULL;

		printf("\n\n~~~ test %d - v4 ~~~\n\n", ntest++);
		dhcpol_init(&optlist);
		dhcpol_parse_buffer(&optlist,
				    (void *)b->buf,
				    b->buf_size,
				    NULL);
		dnr_data = (uint8_t *)
		dhcpol_option_copy(&optlist,
				   dhcptag_encrypted_dns_server_e,
				   &dnr_data_len);
		entries =
		DNSEncryptedServerListCreateWithDHCPv4Data(dnr_data,
							   dnr_data_len);
		free(dnr_data);
		dnr_data = NULL;
		DNSEncryptedServerListPrint(entries, kDNRInstanceTypeDHCPv4);
		my_CFRelease(&entries);
	}

	/* test dhcpv6 and ra */
	entries_dhcpv6 = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	entries_ra = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (const struct buf *b = bufsv6; b->buf != NULL; b++) {
		const uint8_t *dnr_data = NULL;
		int dnr_data_len = 0;
		CFArrayRef entries = NULL;
		DNRInstanceType_t type = (b->ra)
		? kDNRInstanceTypeRA
		: kDNRInstanceTypeDHCPv6;


		printf("\n\n~~~ test %d - %s ~~~\n\n", ntest++,
		       (type == kDNRInstanceTypeRA) ? "RA" : "DHCPv6");
		if (type == kDNRInstanceTypeDHCPv6) {
			DHCPv6OptionListRef optlist = { 0 };
			int optlist_count = 0;
			int optlist_i = 0;

			optlist = DHCPv6OptionListCreate(b->buf, b->buf_size, NULL);
			entries = DHCPv6OptionDNRCopyAllDNSEncryptedServers(optlist);
			free(optlist);
			DNSEncryptedServerListPrint(entries, type);
			if (entries == NULL) {
				continue;
			}
			my_CFMutableArrayMergeArray(entries_dhcpv6,
						    entries,
						    DNSEncryptedServerListCompareEntries);
			my_CFRelease(&entries);
		} else if (type == kDNRInstanceTypeRA) {
			struct __RouterAdvertisementTester fake_ra_struct = { 0 };
			RouterAdvertisementRef fake_ra = &fake_ra_struct;
			ptrlist_t ptrlist = { 0 };
			const uint8_t *dnr_data = NULL;
			uint8_t dnr_data_len = 0;
			uint32_t lifetime = 0;
			int start_index = 0;

			parse_nd_options(&ptrlist, (void *)b->buf, b->buf_size);
			fake_ra->options = ptrlist;
			entries =
			RouterAdvertisementCopyAllDNSEncryptedServers(fake_ra);
			DNSEncryptedServerListPrint(entries, type);
			if (entries == NULL) {
				continue;
			}
			my_CFMutableArrayMergeArray(entries_ra,
						    entries,
						    DNSEncryptedServerListCompareEntries);
			my_CFRelease(&entries);
		}
	}
	CFArraySortValues(entries_dhcpv6,
			  CFRangeMake(0, CFArrayGetCount(entries_dhcpv6)),
			  DNSEncryptedServerListSortByServicePriority,
			  NULL);
	CFArraySortValues(entries_ra,
			  CFRangeMake(0, CFArrayGetCount(entries_ra)),
			  DNSEncryptedServerListSortByServicePriority,
			  NULL);

	/* test merge for v6 */
	printf("\n\n~~~ test %d - dhcpv6-ra merge test ~~~\n\n", ntest++);
	printf("DHCPv6 list before merge:\n\n");
	DNSEncryptedServerListPrint(entries_dhcpv6, kDNRInstanceTypeDHCPv6);
	printf("\n\nRA list before merge:\n\n");
	DNSEncryptedServerListPrint(entries_ra, kDNRInstanceTypeDHCPv6);
	printf("\n\nlist after merge:\n\n");
	my_CFMutableArrayMergeArray(entries_dhcpv6, entries_ra,
				    DNSEncryptedServerListCompareEntries);
	CFArraySortValues(entries_dhcpv6,
			  CFRangeMake(0, CFArrayGetCount(entries_dhcpv6)),
			  DNSEncryptedServerListSortByServicePriority,
			  NULL);
	DNSEncryptedServerListPrint(entries_dhcpv6, kDNRInstanceTypeDHCPv6);
	my_CFRelease(&entries_dhcpv6);
	my_CFRelease(&entries_ra);

	return 0;
}

#endif /* TEST_DNSDNRINSTANCE */
