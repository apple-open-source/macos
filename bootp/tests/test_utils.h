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

#ifndef test_utils_h
#define test_utils_h

#import <Foundation/Foundation.h>
#import <AssertMacros.h>
#import <pthread.h>
#import <sys/ioctl.h>
#import <arpa/inet.h>
#import <net/ethernet.h>
#import <net/if.h>
#import <System/net/if_fake_var.h>
#import <netinet/in.h>
#import <netinet6/in6_var.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/network/IOUserEthernetController.h>
#import <IOKit/network/IOUserEthernetResourceUserClient.h>
#import <IOKit/storage/IOStorageDeviceCharacteristics.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <SystemConfiguration/SCPrivate.h>

#import "IPConfigurationService.h"
#import "DHCPv6PDService.h"
#import "cfutil.h"
#import "interfaces.h"

#define BRIDGED_ID(to_objc_object) ((__bridge id)(to_objc_object))
#define BRIDGED_NSSTRINGREF(cptr) ((__bridge NSString *)(cptr))
#define RELEASE_NULLSAFE(cptr)	\
do {			\
if ((cptr) != NULL) {	\
CFRelease((cptr));	\
(cptr) = NULL;	\
}			\
} while (0)
#define REQUIRE_EQUAL(val1, val2, lbl, str, ...)						\
do {											\
require_action_quiet((val1) == (val2), lbl, NSLog(@str "\n", ## __VA_ARGS__));	\
} while (0)
#define XCT_REQUIRE_EQUAL(val1, val2, lbl, str, ...)		\
do {							\
XCTAssertEqual((val1), (val2), str, ## __VA_ARGS__);	\
require_quiet((val1) == (val2), lbl);			\
} while (0)
#define REQUIRE_NEQUAL(val1, val2, lbl, str, ...)						\
do {											\
require_action_quiet((val1) != (val2), lbl, NSLog(@str "\n", ## __VA_ARGS__));	\
} while (0)
#define REQUIRE_NONNULL(ptr, lbl, str, ...) REQUIRE_NEQUAL((ptr), NULL, lbl, str, ## __VA_ARGS__)
#define XCT_REQUIRE_NONNULL(ptr, lbl, str, ...)			\
do {							\
XCTAssertNotNil((ptr), str, ## __VA_ARGS__);		\
require_quiet((ptr) != nil, lbl);			\
} while (0)
#define REQUIRE_TRUE(ok, lbl, str, ...) REQUIRE_EQUAL((ok), TRUE, lbl, str, ## __VA_ARGS__)
#define XCT_REQUIRE_TRUE(val, lbl, str, ...)			\
do {							\
XCTAssertTrue((val) == TRUE, str, ## __VA_ARGS__);	\
require_quiet((val) == TRUE, lbl);			\
} while (0)
#define REQUIRE_NERROR(err, lbl, str, ...) REQUIRE_EQUAL((err), 0, lbl, str, ## __VA_ARGS__)
#define XCT_REQUIRE_NERROR(err, lbl, str, ...)			\
do {							\
XCTAssertEqual((err), 0, str, ## __VA_ARGS__);		\
require_quiet((err) == 0, lbl);				\
} while (0)

#define _LABEL						"IPConfigurationFrameworkTests"
#define NETIF_TEST_USERDEFINEDNAME			"Test IOUserEthernet interface for " _LABEL
#define NETIF_TEST_PREFIX				"tst"
#define NETIF_FETH_PREFIX				"feth"

#define SCD_KEY_PATTERN_ALLBUTFORSLASH			"[^/]+"
#define SCD_KEY_DOMAIN_PLUGIN				"Plugin:"
#define SCD_KEY_PLUGIN_IPCONFIGSERV			SCD_KEY_DOMAIN_PLUGIN "IPConfigurationService:"
#define SCD_KEY_STATE_NETIF				"State:/Network/Interface/"
#define SCD_KEY_TEST_NETIF_PATTERN			SCD_KEY_STATE_NETIF NETIF_TEST_PREFIX "[0-9]+.*" // "State:/Network/Interface/tstX.*"
#define SCD_KEY_PLUGIN_IPCONFIGSERV_PATTERN		"Plugin:IPConfigurationService:" SCD_KEY_PATTERN_ALLBUTFORSLASH
#define SCD_KEY_PATTERNS_CFARRAYREF			(__bridge CFArrayRef)@[@SCD_KEY_TEST_NETIF_PATTERN, @SCD_KEY_PLUGIN_IPCONFIGSERV_PATTERN]
#define SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service)	(__bridge CFArrayRef)@[(__bridge NSString *)IPConfigurationServiceGetNotificationKey((service))]

#define SEMAPHORE_TIMEOUT_SEC 30
#define SEMAPHORE_TIMEOUT_SEC_SHORT 5
#define SEMAPHORE_TIMEOUT_NANOSEC (SEMAPHORE_TIMEOUT_SEC * NSEC_PER_SEC)
#define SEMAPHORE_TIMEOUT_NANOSEC_SHORT (SEMAPHORE_TIMEOUT_SEC_SHORT * NSEC_PER_SEC)


#define MANUAL_IPV6_ADDR "fc00::2"
#define MANUAL_IPV6_PREFIXLEN 7
#define MANUAL_IPV4_ADDR "10.0.0.2"
#define MANUAL_IPV4_SUBNET_MASK "255.255.255.0"

#define DHCPV6_MCAST_ADDR "ff02::1:2"

#define DHCPV4_SERVER_ADDR "10.0.10.1"
#define DHCPV4_SERVER_NETMASK "255.255.255.0"
#define DHCPV6_SERVER_ULA "fc00::" "10:0:10:1"
#define DHCPV6_SERVER_PREFIXMASK "ffff:ffff:ffff:ffff::"
#define DHCPV6_SERVER_ULA_PREFIXLEN 64
#define DHCPV6_SERVER_ULA_PREFIXLEN_STR "64"
#define DHCPV6_SERVER_ULA_WITH_PREFIXLEN DHCPV6_SERVER_ULA "/" DHCPV6_SERVER_ULA_PREFIXLEN_STR
#define DHCPV6_SERVER_PORT_STR "547"

#define DHCPV4_CLIENT_ADDR "10.0.11.1"
#define DHCPV4_CLIENT_NETMASK "255.255.255.0"
#define DHCPV6_CLIENT_ULA "fc00::" "10:0:11:1"
#define DHCPV6_CLIENT_PREFIXMASK "ffff:ffff:ffff:ffff::"
#define DHCPV6_CLIENT_ULA_PREFIXLEN 64
#define DHCPV6_CLIENT_ULA_PREFIXLEN_STR "64"
#define DHCPV6_CLIENT_ULA_WITH_PREFIXLEN DHCPV6_CLIENT_ULA "/" DHCPV6_CLIENT_ULA_PREFIXLEN_STR
#define DHCPV6_CLIENT_PORT_STR "546"

#define FETH_IF_MTU 1500

#define SCD_KEY_DOMAIN_STATE		"State:"
#define SCD_KEY_STATE_NETSERV		SCD_KEY_DOMAIN_STATE "/Network/Service"
#define SCD_KEY_SUFFIX_INTERFACE	"/Interface"



static __inline__ void
NSLogDebug(NSString * str, ...) {
#if DEBUG
	va_list args;
	va_start(args, str);
	NSLogv(str, args);
	va_end(args);
#endif
}

#pragma mark -
#pragma mark Interface management ioctls

static __inline__ NSString *
createInterface(const char * ifname)
{
	NSString *ret = nil;
	int sockfd = -1;
	struct ifreq ifr = {0};

	require_quiet((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, done);
	(void)strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	require_action_quiet(ioctl(sockfd, SIOCIFCREATE, &ifr) >= 0,
			     done, NSLogDebug(@"ioctl: SIOCIFCREATE failed"));
	ret = [NSString stringWithFormat:@"%s", ifr.ifr_name];
	NSLogDebug(@"ioctl: created interface '%@'", ret);

done:
	close(sockfd);
	return ret;
}

static __inline__ NSString *
createFethInterface(void)
{
	return createInterface(NETIF_FETH_PREFIX);
}

static __inline__ NSString *
destroyFethInterface(NSString * ifName)
{
	NSString *ret = nil;
	int sockfd = -1;
	struct ifreq ifr = {0};

	require_quiet([ifName hasPrefix:@NETIF_FETH_PREFIX], done);
	require_quiet((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, done);
	(void)strlcpy(ifr.ifr_name, [ifName UTF8String], sizeof(ifr.ifr_name));
	require_quiet(ioctl(sockfd, SIOCIFDESTROY, &ifr) >= 0, done);
	ret = [NSString stringWithUTF8String:ifr.ifr_name];
	NSLogDebug(@"ioctl: destroyed interface '%@'", ret);

done:
	close(sockfd);
	return ret;
}

static __inline__ NSString *
setInterfaceUp(NSString * ifName)
{
	NSString *ret = @"";
	int sockfd = -1;
	struct ifreq ifr = {0};

	require_quiet((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, done);
	(void)strlcpy(ifr.ifr_name, [ifName UTF8String], sizeof(ifr.ifr_name));
	ifr.ifr_flags |= IFF_UP;
	require_quiet(ioctl(sockfd, SIOCSIFFLAGS, &ifr) >= 0, done);
	ret = [NSString stringWithUTF8String:ifr.ifr_name];
	NSLogDebug(@"ioctl: set interface '%@' up", ret);

done:
	close(sockfd);
	return ret;
}

static __inline__ NSString *
setInterfaceDown(NSString * ifName)
{
	NSString *ret = @"";
	int sockfd = -1;
	struct ifreq ifr = {0};

	require_quiet((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, done);
	(void)strlcpy(ifr.ifr_name, [ifName UTF8String], sizeof(ifr.ifr_name));
	ifr.ifr_flags &= ~IFF_UP;
	require_quiet(ioctl(sockfd, SIOCSIFFLAGS, &ifr) >= 0, done);
	ret = [NSString stringWithUTF8String:ifr.ifr_name];
	NSLogDebug(@"ioctl: set interface '%@' down", ret);

done:
	close(sockfd);
	return ret;
}

/*
 * Some commands need root user privileges to execute.
 */

static __inline__ NSString *
listInterfaces(void)
{
	NSMutableString *ret = [NSMutableString string];
	interface_list_t *ifl = ifl_init();

	for (int i = 0; i < ifl_count(ifl); i++) {
		interface_t *if_ = ifl_at_index(ifl, i);
		if (i < ifl_count(ifl) - 1) {
			ret = (NSMutableString *)[ret stringByAppendingFormat:@"%s ", if_->name];
		} else {
			ret = (NSMutableString *)[ret stringByAppendingFormat:@"%s", if_->name];
		}
	}

	ifl_free(&ifl);
	return (NSString *)ret;
}

static __inline__ void
destroyInterfaces(NSString * ifname1, NSString * ifname2)
{
	NSString *listStr = listInterfaces();
	NSArray *list = [listStr componentsSeparatedByString:@" "];

	if ([list containsObject:ifname1]) {
		destroyFethInterface(ifname1);
	}
	if ([list containsObject:ifname2]) {
		destroyFethInterface(ifname2);
	}

	return;
}

static __inline__ BOOL
interfaceAddIPv4Addr(const char * ifName, const char * addr, const char * netmask)
{
	BOOL ret = FALSE;
	int sockfd = -1;
	struct sockaddr_in sockaddr = {0};
	struct in_addr inaddr = {0};
	struct sockaddr_in sockaddrNetmask = {0};
	struct in_addr inaddrNetmask = {0};
	struct ifaliasreq ifr = {0};

	// sets ipv4 addr
	require_quiet(inet_pton(AF_INET, addr, &inaddr) == 1, done);
	sockaddr.sin_len = sizeof(sockaddr);
	sockaddr.sin_family = AF_INET;
	memcpy(&sockaddr.sin_addr, &inaddr, sizeof(sockaddr.sin_addr));
	memcpy(&ifr.ifra_addr, &sockaddr, sizeof(ifr.ifra_addr));

	// sets ipv4 netmask
	require_quiet(inet_pton(AF_INET, netmask, &inaddrNetmask) == 1, done);
	sockaddrNetmask.sin_len = sizeof(sockaddrNetmask);
	sockaddrNetmask.sin_family = AF_INET;
	memcpy(&sockaddrNetmask.sin_addr, &inaddrNetmask, sizeof(sockaddrNetmask.sin_addr));
	memcpy(&ifr.ifra_mask, &sockaddrNetmask, sizeof(ifr.ifra_mask));

	// sets interface name
	(void)strlcpy(ifr.ifra_name, ifName, sizeof(ifr.ifra_name));

	// makes ioctl call
	require_quiet((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, done);
	require_quiet(ioctl(sockfd, SIOCSIFADDR, &ifr) >= 0, done);
	ret = TRUE;

done:
	close(sockfd);
	return ret;
}

static __inline__ BOOL
interfaceAddIPv6Addr(const char * ifName, const char * addr, const char * prefixmask)
{
	BOOL ret = FALSE;
	int sockfd = -1;
	struct sockaddr_in6 sockaddr = {0};
	struct in6_addr in6addr = {0};
	struct sockaddr_in6 sockaddrPrefixmask = {0};
	struct in6_addr in6addrPrefixmask = {0};
	struct in6_addrlifetime in6addrLifetime = {0};
	struct in6_aliasreq ifr = {0};

	// sets ipv6 addr
	require_quiet(inet_pton(AF_INET6, addr, &in6addr) == 1, done);
	sockaddr.sin6_len = sizeof(sockaddr);
	sockaddr.sin6_family = AF_INET6;
	memcpy(&sockaddr.sin6_addr, &in6addr, sizeof(sockaddr.sin6_addr));
	memcpy(&ifr.ifra_addr, &sockaddr, sizeof(ifr.ifra_addr));

	// sets ipv6 prefixmask
	require_quiet(inet_pton(AF_INET6, prefixmask, &in6addrPrefixmask) == 1, done);
	sockaddrPrefixmask.sin6_len = sizeof(sockaddrPrefixmask);
	sockaddrPrefixmask.sin6_family = AF_INET6;
	memcpy(&sockaddrPrefixmask.sin6_addr, &in6addrPrefixmask, sizeof(sockaddrPrefixmask.sin6_addr));
	memcpy(&ifr.ifra_prefixmask, &sockaddrPrefixmask, sizeof(ifr.ifra_prefixmask));

	// sets ipv6 addr lifetime
	in6addrLifetime.ia6t_expire = 0;
	in6addrLifetime.ia6t_preferred = 0;
	in6addrLifetime.ia6t_vltime = UINT32_MAX;
	in6addrLifetime.ia6t_pltime = UINT32_MAX;
	memcpy(&ifr.ifra_lifetime, &in6addrLifetime, sizeof(ifr.ifra_lifetime));

	// sets interface name
	(void)strlcpy(ifr.ifra_name, ifName, sizeof(ifr.ifra_name));

	// makes ioctl call
	require_quiet((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0, done);
	require_quiet(ioctl(sockfd, SIOCAIFADDR_IN6, &ifr) >= 0, done);
	ret = TRUE;

done:
	close(sockfd);
	return ret;
}

#pragma mark -
#pragma mark DHCPv4/v6 helpers

static __inline__ BOOL
fethSetPeer(const char * feth, const char * fethPeer)
{
	struct if_fake_request iffr = {0};
	struct ifdrv ifd = {0};
	int sockfd = -1;
	BOOL ret = FALSE;

	require_quiet(feth != NULL, done);
	require_quiet(fethPeer != NULL, done);
	strlcpy(iffr.iffr_peer_name, fethPeer, sizeof(iffr.iffr_peer_name));
	require_quiet((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, done);
	strlcpy(ifd.ifd_name, feth, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = IF_FAKE_S_CMD_SET_PEER;
	ifd.ifd_len = sizeof(iffr);
	ifd.ifd_data = &iffr;
	require_quiet(ioctl(sockfd, SIOCSDRVSPEC, &ifd) >= 0, done);
	ret = TRUE;

done:
	close(sockfd);
	return ret;
}

static __inline__ void
setupForMockDHCPExchange(NSString ** outServerIfname, NSString **outClientIfname)
{
	NSString *serverIfname = nil;
	NSString *clientIfname = nil;

	// creates a couple of feth interfaces
	require_action_quiet((serverIfname = createFethInterface()) != nil,
			     done, NSLogDebug(@"%s: Failed to create server if", __func__));
	require_action_quiet((clientIfname = createFethInterface()) != nil,
			     done, NSLogDebug(@"%s: Failed to create client if", __func__));
	while (if_nametoindex([serverIfname UTF8String]) == 0
	       || if_nametoindex([clientIfname UTF8String]) == 0) {
		// This ensures that the required interfaces are attached.
		// Otherwise, ipconfig_add_service(feth[01]) fails
		// with "interface doesn't exist" within tests.
		sleep(1);
	}

	// peers the 2 interfaces
	require_action_quiet(fethSetPeer([serverIfname UTF8String], [clientIfname UTF8String]),
			     done, NSLogDebug(@"%s: Failed to peer interfaces", __func__));

	// assigns ipv4 addresses
	require_action_quiet(interfaceAddIPv4Addr([serverIfname UTF8String], DHCPV4_SERVER_ADDR, DHCPV4_SERVER_NETMASK),
			     done, NSLogDebug(@"%s: Failed to set server ipv4 addr", __func__));
	require_action_quiet(interfaceAddIPv4Addr([clientIfname UTF8String], DHCPV4_CLIENT_ADDR, DHCPV4_CLIENT_NETMASK),
			     done, NSLogDebug(@"%s: Failed to set client ipv4 addr", __func__));

	// assigns ipv6 ula addresses
	require_action_quiet(interfaceAddIPv6Addr([serverIfname UTF8String], DHCPV6_SERVER_ULA, DHCPV6_SERVER_PREFIXMASK),
			     done, NSLogDebug(@"%s: Failed to set server ipv6 addr", __func__));
	require_action_quiet(interfaceAddIPv6Addr([clientIfname UTF8String], DHCPV6_CLIENT_ULA, DHCPV6_CLIENT_PREFIXMASK),
			     done, NSLogDebug(@"%s: Failed to set client ipv6 addr"));
	*outServerIfname = serverIfname;
	*outClientIfname = clientIfname;

done:
	return;
}

static __inline__ void
cleanupFromMockDHCPExchange(NSString * server, NSString * client)
{
	destroyInterfaces(server, client);
	return;
}

static __inline__ NSString *
pdServiceStringFromPrefixAndLength(const struct in6_addr * prefix, uint8_t len)
{
	return [NSString stringWithFormat:@"%@/%u",
		(__bridge_transfer NSString *)my_CFStringCreateWithIPv6Address(prefix), len];
}

#endif /* test_utils_h */
