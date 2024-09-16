/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#import "SCTest.h"
#import "SCTestUtils.h"
#import <sys/ioctl.h>
#import <net/if.h>
#import <System/net/if_fake_var.h>
#import <arpa/inet.h>
#import <sys/socket.h>
#import <network_information.h>

#define	INTERFACES_KEY		@"State:/Network/Interface"
#define GLOBAL_IPV4		@"State:/Network/Global/IPv4"
#define FETH_PREFIX		"feth"
#define IFNAME1			"feth0"
#define IFNAME2 		"feth2"
#define ROUTER_IFNAME1		"feth1"
#define ROUTER_IFNAME2		"feth3"
#define FETH0_IP		"10.0.10.5"
#define FETH1_IP		"10.0.10.1"
#define FETH2_IP		"10.0.11.5"
#define FETH3_IP		"10.0.11.1"
#define NETMASK			"255.255.255.0"
#define INTERFACE_RANK		CFSTR("InterfaceRank")
#define SERVICE_RANK		CFSTR("ServiceRank")
#define	MAX_TRIES		3

@interface SCTestRankAssertion : SCTest
@property SCPreferencesRef prefs;
@property SCDynamicStoreRef store;
@property SCDynamicStoreRef interfaceSession;
@property SCDynamicStoreRef globalIPv4Session;
@property SCNetworkInterfaceRef ifEthernet1; //feth0
@property SCNetworkInterfaceRef ifEthernet2; //feth2
@property dispatch_semaphore_t interfaceSem;
@property dispatch_semaphore_t globalIPv4Sem;
@property dispatch_queue_t interfaceQueue;
@property dispatch_queue_t globalIPv4Queue;
@property NSString *feth0ServiceID;
@property NSString *feth2ServiceID;
@property NSString *serviceID;
@property NSArray *interfaces;
@end

#if !TARGET_OS_BRIDGE
@implementation SCTestRankAssertion

+ (NSString *)command
{
	return @"rank_assertion";
}

+ (NSString *)commandDescription
{
	return @"Tests the SCNetworkConfigurationPrivate code path";
}

- (instancetype)initWithOptions:(NSDictionary *)options
{
	self = [super initWithOptions:options];
	if (self) {
		_prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("SCTest"), NULL);
		_store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("SCTest"), NULL, NULL);
	}
	return self;
}

- (void)dealloc
{
	if (self.prefs != NULL) {
		CFRelease(self.prefs);
		self.prefs = NULL;
	}
	if (self.store != NULL) {
		CFRelease(self.store);
		self.store = NULL;
	}
	[self cleanupFethServicesAndInterfaces];
	[self watchForNewInterface:FALSE];
	[self watchForGlobalIPv4StateChange:FALSE];
}

- (void)start
{
	if (self.options[kSCTestInterfaceRankAssertion] != nil) {
		[self unitTestInterfaceRankAssertion];
	} else if (self.options[kSCTestServiceRankAssertion] != nil) {
		[self unitTestServiceRankAssertion];
	} else {
		SCTestLog("Usage: sctest rank_assertion -interface_rank");
		SCTestLog("Usage: sctest rank_assertion -service_rank");
		ERR_EXIT;
	}

	[self cleanupAndExitWithErrorCode:0];
}

- (void)cleanupAndExitWithErrorCode:(int)error
{
	[super cleanupAndExitWithErrorCode:error];
}

- (BOOL)unitTest
{
	BOOL allUnitTestsPassed = YES;
	allUnitTestsPassed &= [self unitTestServiceRankAssertion];
	allUnitTestsPassed &= [self unitTestInterfaceRankAssertion];
	return  allUnitTestsPassed;
}

#pragma mark -
#pragma mark Interface and Rank Assertion callbacks

static void
interfaceCallback(SCDynamicStoreRef session, CFArrayRef changedKeys, void * info)
{
#pragma unused(session)
	@autoreleasepool {
		NSDictionary	*dict;
		NSArray		*interfaces;
		SCTestRankAssertion	*test	= (__bridge SCTestRankAssertion *)info;
		if ([(__bridge NSArray *)changedKeys containsObject:INTERFACES_KEY]) {
			dict = (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(test.interfaceSession, (CFStringRef)INTERFACES_KEY);
			interfaces = [dict objectForKey:(__bridge NSString *)kSCPropNetInterfaces];
			if (!_SC_CFEqual((__bridge CFArrayRef)interfaces, (__bridge CFArrayRef)test.interfaces)) {
				test.interfaces = interfaces;
				if (test.interfaceSem != NULL) {
					dispatch_semaphore_signal(test.interfaceSem);
				}
			}
		}
	}
	return;
}

- (void)watchForNewInterface:(BOOL)enabled
{
	if (enabled) {
		SCDynamicStoreContext	context	= {0, NULL, CFRetain, CFRelease, NULL};
		BOOL ok = YES;

		self.interfaceQueue = dispatch_queue_create("SCTestNetworkInterface callback queue", NULL);
		self.interfaceSem = dispatch_semaphore_create(0);
		context.info = (__bridge void * _Nullable)self;
		self.interfaceSession = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("SCTest"), interfaceCallback, &context);

		ok = SCDynamicStoreSetNotificationKeys(self.interfaceSession,
						       (__bridge CFArrayRef)@[INTERFACES_KEY],
						       NULL);
		assert(ok);
		ok = SCDynamicStoreSetDispatchQueue(self.interfaceSession, self.interfaceQueue);
		assert(ok);
	} else {
		if (self.interfaceSession != NULL) {
			(void)SCDynamicStoreSetDispatchQueue(self.interfaceSession, NULL);
			CFRelease(self.interfaceSession);
			self.interfaceSession = NULL;
		}
		self.interfaceQueue = NULL;
		self.interfaceSem = NULL;
	}
}

static void
globalIPv4ChangeCallback(SCDynamicStoreRef session, CFArrayRef changedKeys, void *info)
{
#pragma unused(session)
	@autoreleasepool {
		SCTestRankAssertion *test = (__bridge SCTestRankAssertion *)info;

		if ([(__bridge NSArray *)changedKeys containsObject:GLOBAL_IPV4]) {
			if (test.globalIPv4Sem != NULL) {
				dispatch_semaphore_signal(test.globalIPv4Sem);
			}
		}
	}
	return;
}

- (void)watchForGlobalIPv4StateChange:(BOOL)enabled
{
	if (enabled) {
		SCDynamicStoreContext	context = {
			.version = 0,
			.info = (__bridge void * __nullable)self,
			.retain = NULL,
			.release = NULL,
			.copyDescription = NULL
		};
		CFMutableArrayRef keys = NULL;
		CFStringRef key = NULL;
		BOOL ok = YES;

		self.globalIPv4Sem = dispatch_semaphore_create(0);

		keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
								 kSCDynamicStoreDomainState,
								 kSCEntNetIPv4);
		CFArrayAppendValue(keys, key);
		CFRelease(key);

		self.globalIPv4Session = SCDynamicStoreCreate(NULL, CFSTR("SCTest"), globalIPv4ChangeCallback, &context);
		ok = SCDynamicStoreSetNotificationKeys(self.globalIPv4Session, keys, NULL);
		CFRelease(keys);
		assert(ok);

		self.globalIPv4Queue = dispatch_queue_create("SCTestInterfaceRankAssertion callback queue", NULL);
		ok = SCDynamicStoreSetDispatchQueue(self.globalIPv4Session, self.globalIPv4Queue);
		assert(ok);

	} else	{
		if (self.globalIPv4Session != NULL) {
			(void)SCDynamicStoreSetDispatchQueue(self.globalIPv4Session, NULL);
			CFRelease(self.globalIPv4Session);
			self.globalIPv4Session = NULL;
		}
		self.globalIPv4Queue = NULL;
		self.globalIPv4Sem = NULL;
	}
}

#pragma mark -
#pragma mark FETH INTERFACES

static int
inet_dgram_socket(void)
{
	int     s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		SCTestLog("socket() failed: %s", strerror(errno));
	}

	return s;
}

- (BOOL)destroyInterface:(NSString *)ifName
{
	int socketFD = -1;
	struct ifreq ifr = {0};
	BOOL status = NO;

	if (![ifName hasPrefix:@FETH_PREFIX]) {
		goto done;
	}

	socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD == -1) {
		goto done;
	}

	strlcpy(ifr.ifr_name, [ifName UTF8String], sizeof(ifr.ifr_name));
	if (ioctl(socketFD, SIOCIFDESTROY, &ifr) == -1) {
		goto done;
	}
	status = YES;

done:
	if (socketFD != -1) close(socketFD);
	return status;
}

#define SIFCREATE_RETRY 600
- (BOOL)createInterfaceAndMarkUP:(NSString *)interface
{
	struct ifreq ifr = {0};
	int socketFd = -1;
	int error = 0;
	BOOL status = NO;

	socketFd = inet_dgram_socket();
	if (socketFd == -1) {
		SCTestLog("createInterfaceAndMarkUP(): Cannot create socket");
		goto done;
	}

	(void)strlcpy(ifr.ifr_name, [interface UTF8String], sizeof(ifr.ifr_name));

	/* Create interface */
	for (int i = 0; i < SIFCREATE_RETRY; i++) {
		if (ioctl(socketFd, SIOCIFCREATE, &ifr) < 0) {
			error = errno;
			if (error == EBUSY) {
				/* interface is tearing down, try again */
				usleep(10000);
			} else if (error == EEXIST) {
				/* interface exists, try destroying it */
				[self destroyInterface:interface];
			} else {
				/* unexpected failure */
				break;
			}
		} else {
			error = 0;
			break;
		}
	}
	if (error == 0) {
		/* Mark interface UP */
		if ((ifr.ifr_flags & IFF_UP) == 0) {
			ifr.ifr_flags |= IFF_UP;
			if (ioctl(socketFd, SIOCSIFFLAGS, &ifr) == -1) {
				SCTestLog("SIOCSIFFLAGS failed \"%@\": %s",
					  interface,
					  strerror(errno));
				goto done;
			}
		}
	}
	status = YES;

done:
	if (socketFd != -1) close(socketFd);
	return status;
}

- (BOOL)setPeerInterface:(NSString *)clientIf
		  router:(NSString *)routerIf
{
	struct if_fake_request iffr = {0};
	struct ifdrv ifd = {0};
	int socketFd = -1;
	BOOL status = NO;

	if ((clientIf == NULL) || (routerIf == NULL)) {
		SCTestLog("Interface name is NULL : Client %@ and Router %@", clientIf, routerIf);
		goto done;
	}

	socketFd = inet_dgram_socket();
	if (socketFd == -1) {
		SCTestLog("Could not create Socket: %s", strerror(errno));
		goto done;
	}

	strlcpy(iffr.iffr_peer_name, [routerIf UTF8String], sizeof(iffr.iffr_peer_name));
	strlcpy(ifd.ifd_name, [clientIf UTF8String], sizeof(ifd.ifd_name));
	ifd.ifd_cmd = IF_FAKE_S_CMD_SET_PEER;
	ifd.ifd_len = sizeof(iffr);
	ifd.ifd_data = &iffr;
	if (ioctl(socketFd, SIOCSDRVSPEC, (caddr_t)&ifd) == -1) {
		SCTestLog("Could not set peer interface: \"%@\" to \"%@\": %s",
			  clientIf,
			  routerIf,
			  strerror(errno));
		goto done;
	}
	status = YES;

done:
    if (socketFd != -1) close(socketFd);
    return status;
}

- (BOOL)createClientRouterAndPeer:(NSString *)fethIf
			   router:(NSString *)routerIf
{
	const uint8_t waitTime = 1;
	if (![self createInterfaceAndMarkUP:fethIf]) {
		SCTestLog("Cannot create interface %@", (__bridge CFStringRef)fethIf);
		return NO;
	}
	dispatch_semaphore_wait(self.interfaceSem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));

	if (![self createInterfaceAndMarkUP:routerIf]) {
		return NO;
	}
	dispatch_semaphore_wait(self.interfaceSem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));

	if (![self setPeerInterface:fethIf router:routerIf]) {
		return NO;
	}

	return YES;
}

- (BOOL)interfaceAddIPv4Addr:(const char *)ifName
		   ipAddress:(const char *)addr
		     netmask:(const char *)netmask
{
	int socketFD = -1;
	struct sockaddr_in sockaddr = {0};
	struct in_addr inaddr = {0};
	struct sockaddr_in sockaddrNetmask = {0};
	struct in_addr inaddrNetmask = {0};
	struct ifaliasreq ifr = {0};
	BOOL status = NO;

	/* Set IPv4 address */
	if (inet_pton(AF_INET, addr, &inaddr) != 1) {
		goto done;
	}
	sockaddr.sin_len = sizeof(sockaddr);
	sockaddr.sin_family = AF_INET;
	memcpy(&sockaddr.sin_addr, &inaddr, sizeof(sockaddr.sin_addr));
	memcpy(&ifr.ifra_addr, &sockaddr, sizeof(ifr.ifra_addr));

	/* Set IPv4 netmask */
	if (inet_pton(AF_INET, netmask, &inaddrNetmask)!= 1) {
		goto done;
	}
	sockaddrNetmask.sin_len = sizeof(sockaddrNetmask);
	sockaddrNetmask.sin_family = AF_INET;
	memcpy(&sockaddrNetmask.sin_addr, &inaddrNetmask, sizeof(sockaddrNetmask.sin_addr));
	memcpy(&ifr.ifra_mask, &sockaddrNetmask, sizeof(ifr.ifra_mask));

	/* Set interface name */
	strlcpy(ifr.ifra_name, ifName, sizeof(ifr.ifra_name));

	socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD == -1) {
		goto done;
	}
	if (ioctl(socketFD, SIOCSIFADDR, &ifr) == -1) {
		goto done;
	}
	status = YES;

done:
	if (socketFD != -1) close(socketFD);
	return status;
}

- (void)cleanupFethServicesAndInterfaces
{
	/* Remove service over feth interfaces */
	if (self.prefs != NULL) {
		SCNetworkInterfaceRef   interface;
		SCNetworkServiceRef     service;
		NSArray *services;
		NSString *bsdName;

		services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
		for (id servicePtr in services) {
			service = (__bridge SCNetworkServiceRef)servicePtr;
			interface = SCNetworkServiceGetInterface(service);
			if (interface != NULL) {
				bsdName = (__bridge NSString *)SCNetworkInterfaceGetBSDName(interface);
				if ([bsdName hasPrefix:@FETH_PREFIX]) {
					[self removeService:service];
				}
			}
		}
	}
	/* Destroy feth interfaces */
	[self destroyInterface:@IFNAME1];
	[self destroyInterface:@ROUTER_IFNAME1];
	[self destroyInterface:@IFNAME2];
	[self destroyInterface:@ROUTER_IFNAME2];
}

- (CFArrayRef)copyActiveInterfaces
{
	/* Remove service over feth interfaces */
	CFMutableArrayRef activeInterfaces = NULL;
	if (self.prefs != NULL) {
		SCNetworkInterfaceRef   interface;
		SCNetworkServiceRef     service;
		NSArray *services;
		CFStringRef bsdName;

		activeInterfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
		for (id servicePtr in services) {
			service = (__bridge SCNetworkServiceRef)servicePtr;
			interface = SCNetworkServiceGetInterface(service);
			if (interface != NULL) {
				SCNetworkInterfaceRef currentInterface = NULL;
				bsdName = SCNetworkInterfaceGetBSDName(interface);
				if (bsdName != NULL) {
					if (CFEqual(bsdName, @IFNAME1) || CFEqual(bsdName, @IFNAME2)) {
						[self destroyInterface:(__bridge NSString *)bsdName];
						continue;
					}
					currentInterface = _SCNetworkInterfaceCreateWithBSDName(NULL, bsdName, kIncludeNoVirtualInterfaces);
					if (currentInterface != NULL) {
						CFArrayAppendValue(activeInterfaces, currentInterface);
						CFRelease(currentInterface);
					}
				}
			}
		}
	}
	if (activeInterfaces != NULL) {
		return CFRetain(activeInterfaces);
	}

	return NULL;
}

void
get_rank_from_string(CFStringRef str, SCNetworkServicePrimaryRank *ret_rank)
{
	SCNetworkServicePrimaryRank     rank;

	if (CFEqual(str, CFSTR("Default"))) {
		rank = kSCNetworkServicePrimaryRankDefault;
	} else if (CFEqual(str, CFSTR("First"))) {
		rank = kSCNetworkServicePrimaryRankFirst;
	} else if (CFEqual(str, CFSTR("Last"))) {
		rank = kSCNetworkServicePrimaryRankLast;
	} else if (CFEqual(str, CFSTR("Never"))) {
		rank = kSCNetworkServicePrimaryRankNever;
	} else if (CFEqual(str, CFSTR("Scoped"))) {
		rank = kSCNetworkServicePrimaryRankScoped;
	} else {
		rank = kSCNetworkServicePrimaryRankDefault;
	}
	*ret_rank = rank;
}

static const char *
get_rank_str(SCNetworkServicePrimaryRank rank)
{
	const char *        str = NULL;
	switch (rank) {
	case kSCNetworkServicePrimaryRankDefault:
		str = "Default";
		break;
	case kSCNetworkServicePrimaryRankFirst:
		str = "First";
		break;
	case kSCNetworkServicePrimaryRankLast:
		str = "Last";
		break;
	case kSCNetworkServicePrimaryRankNever:
		str = "Never";
		break;
	case kSCNetworkServicePrimaryRankScoped:
		str = "Scoped";
		break;
	default:
		str = "<unknown>";
		break;
	}
	return (str);
}

- (NSString *)getPrimaryInterfaceName
{
	const char *name;
	NSString *nsName;
	nwi_ifstate_t ifstate;
	nwi_state_t nwi;

	nwi = nwi_state_copy();
	ifstate = nwi_state_get_first_ifstate(nwi, AF_INET);
	if (ifstate == NULL) {
		return nil;
	}

	name = nwi_ifstate_get_ifname(ifstate);
	nsName = [NSString stringWithCString:name encoding:NSUTF8StringEncoding];
	nwi_state_release(nwi);

	return nsName;
}

- (SCNetworkServiceRef)createAndConfigureService:(SCNetworkInterfaceRef)interface
{
	SCNetworkServiceRef newService;
	SCNetworkSetRef currentSet;
	NSArray *services; 
	BOOL prefsLocked = NO;

	if (!SCPreferencesLock(self.prefs, true)) {
		return NULL;
	}
	prefsLocked = YES;

	/* Remove old services over feth0 and feth2, if any */
	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	if (services != nil) {
		SCNetworkInterfaceRef currentInterface;
		SCNetworkServiceRef service;
		for (id servicePtr in services) {
			service = (__bridge SCNetworkServiceRef)servicePtr;
			currentInterface = SCNetworkServiceGetInterface(service);
			if (_SC_CFEqual(currentInterface, interface)) {
				SCNetworkServiceRemove(service);
			}
		}
	}

	newService = SCNetworkServiceCreate(self.prefs, interface);
	if (newService != NULL) {
		NSArray *protocols = NULL;
		NSString *router = NULL;
		NSMutableDictionary *dnsConfig = nil;
		NSArray<NSString *> *dnsSearchDomains = nil;
		NSArray<NSString *> *dnsServerAddresses = nil;
		NSMutableDictionary *ipv4Config = nil;
		NSArray<NSString *> *ipv4Addresses = nil;
		NSArray<NSString *> *subnetMasks = nil;
		SCNetworkProtocolRef protocol = NULL;
		CFStringRef bsdName = NULL;

		SCNetworkServiceEstablishDefaultConfiguration(newService);
		SCNetworkServiceSetName(newService, CFSTR("Test Service"));

		protocols = (__bridge_transfer NSArray *)SCNetworkServiceCopyProtocols(newService);
		if ([protocols count] == 0) {
			SCTestLog("SCNetworkServiceCopyProtocols failed to copy protocols from a service. Error: %s", SCErrorString(SCError()));
			goto done;
		}

		ipv4Config = [[NSMutableDictionary alloc] init];
		dnsConfig = [[NSMutableDictionary alloc] init];
		bsdName = SCNetworkInterfaceGetBSDName(interface);

		for (NSUInteger i = 0; i < [protocols count]; i++) {
			protocol = (__bridge SCNetworkProtocolRef)[protocols objectAtIndex:i];
			if (protocol) {
				CFStringRef protocolType = SCNetworkProtocolGetProtocolType(protocol);
				if (CFEqual(protocolType, kSCNetworkProtocolTypeIPv4)) {
					if (CFEqual(bsdName, (__bridge CFStringRef)@IFNAME1)) {
						ipv4Addresses = [NSArray arrayWithObject:@FETH0_IP];
						subnetMasks = [NSArray arrayWithObject:@NETMASK];
						router = @FETH1_IP;
					} else {
						ipv4Addresses = [NSArray arrayWithObject:@FETH2_IP];
						subnetMasks = [NSArray arrayWithObject:@NETMASK];
						router = @FETH3_IP;
					}

					[ipv4Config setObject:(__bridge NSString*)kSCValNetIPv4ConfigMethodManual forKey:(__bridge NSString*)kSCPropNetIPv4ConfigMethod];
					[ipv4Config setObject:ipv4Addresses forKey:(__bridge NSString *)kSCPropNetIPv4Addresses];
					[ipv4Config setObject:subnetMasks forKey:(__bridge NSString *)kSCPropNetIPv4SubnetMasks];
					[ipv4Config setObject:router forKey:(__bridge NSString *)kSCPropNetIPv4Router];

					/* Set IPv4 protocol configuration */
					SCNetworkProtocolSetConfiguration(protocol, (__bridge CFMutableDictionaryRef)ipv4Config);
					SCNetworkProtocolSetEnabled(protocol, TRUE);

				} else if (CFEqual(protocolType, kSCNetworkProtocolTypeDNS)) {
					dnsSearchDomains = @[@"apple.com", @"icloud.com", @"itunes.com"];
					dnsServerAddresses = @[@"10.10.10.7", @"10.10.10.8", @"10.10.10.9"];

					[dnsConfig setObject:dnsSearchDomains forKey:(__bridge NSString *)kSCPropNetDNSSearchDomains];
					[dnsConfig setObject:dnsServerAddresses forKey:(__bridge NSString *)kSCPropNetDNSServerAddresses];

					/* Set DNS configuration */
					SCNetworkProtocolSetConfiguration(protocol, (__bridge CFMutableDictionaryRef)dnsConfig);
					SCNetworkProtocolSetEnabled(protocol, TRUE);
				}
			}
		}
		currentSet = SCNetworkSetCopyCurrent(self.prefs);
		if (currentSet != NULL) {
			SCNetworkServiceSetEnabled(newService, TRUE);
			SCNetworkSetAddService(currentSet, newService);
			CFRelease(currentSet);
		}
	}
	SCPreferencesCommitChanges(self.prefs);
	SCPreferencesApplyChanges(self.prefs);

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	return newService;
}

- (void)removeService:(SCNetworkServiceRef)service
{
	if (self.prefs != NULL && service != NULL) {
		SCNetworkServiceRemove(service);
		SCPreferencesCommitChanges(self.prefs);
		SCPreferencesApplyChanges(self.prefs);
	}
}

#define ROUNDUP32(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof (uint32_t) - 1))) : sizeof (uint32_t))

- (BOOL)checkRoutesFromRoutingTableAreScoped:(const char *)interfaceName
{
	const uint32_t interface_index = if_nametoindex(interfaceName);

	/* Set up sysctl arguments to get IPv4 routing table */
	char *table_buffer = NULL;
	char *current;
	char *table_end;
	BOOL parsed = NO;
	BOOL scoped = YES;
	struct rt_msghdr2 *rtm;
	size_t table_size = 0;
	int sysctl_args[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP2, 0};

	/* Try to get the routing table 3 times before giving up */
	for (int i = 0; i < 3; i++) {

		/* Get the routing table size estimate */
		table_size = 0;
		if (sysctl(sysctl_args, 6, NULL, &table_size, NULL, 0) < 0) {
			SCTestLog("sysctl: net.route.0.0.dump estimate");
			continue;
		}

		/* Increase estimate by 25% in case routing table grows between sysctl() calls */
		table_size = (table_size * 5) / 4;
		table_size = ROUNDUP32(table_size);

		/* Allocate memory using size estimate */
		if ((table_buffer = malloc(table_size)) == 0) {
			SCTestLog("malloc(%zu)", table_size);
			continue;
		}

		/* Get routing table */
		if (sysctl(sysctl_args, 6, table_buffer, &table_size, NULL, 0) < 0) {
			free(table_buffer);
			table_buffer = NULL;

			if (errno == ENOMEM) {
				SCTestLog("Unable to get routing table because allocated buffer was too small");
			} else {
				SCTestLog("Unable to get routing table");
			}
		} else {
			break;
		}
	}

	if (table_buffer == NULL) {
		SCTestLog("Table buffer is NULL");
		return NO;
	}

	/* Parse routing table looking for interface routes */
	current = table_buffer;
	table_end = table_buffer + table_size;

	while (current < table_end) {
		rtm = (struct rt_msghdr2 *) (void *) current;
		if ((current + sizeof(*rtm)) > table_end) {
			SCTestLog("Not enough data to read rtm");
			break;
		}
		if ((current + rtm->rtm_msglen) > table_end) {
			SCTestLog("Not enough data to read routing table message");
			break;
		}
		if (rtm->rtm_index == interface_index) {
			scoped &= ((rtm->rtm_flags & RTF_IFSCOPE) == RTF_IFSCOPE);
			parsed = YES;
		}
		current += rtm->rtm_msglen;
	}

	free(table_buffer);
	if (scoped && parsed) {
		return YES;
	}

	return NO;
}

- (BOOL)checkAssertedRankOnInterface:(SCNetworkInterfaceRef)interface
				rank:(SCNetworkServicePrimaryRank)rank
{
	CFArrayRef list = NULL;
	CFIndex count = 0;
	BOOL ok = NO;

	list = SCNetworkInterfaceCopyRankAssertionInfo(interface);
	if (list == NULL) {
		if (rank == kSCNetworkServicePrimaryRankDefault) {
			return YES;
		} else {
			SCTestLog("No rank assertions for interface %@", SCNetworkInterfaceGetBSDName(interface));
			return ok;
		}
	}
	count = CFArrayGetCount(list);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkInterfaceRankAssertionInfoRef 	info;
		SCNetworkServicePrimaryRank 		ifRank;
		info = (SCNetworkInterfaceRankAssertionInfoRef)CFArrayGetValueAtIndex(list, i);
		ifRank = SCNetworkInterfaceRankAssertionInfoGetPrimaryRank(info);
		if (ifRank == rank && CFEqual(SCNetworkInterfaceRankAssertionInfoGetProcessName(info), CFSTR("sctest"))) {
			ok = YES;
			break;
		}
	}
	CFRelease(list);

	return ok;
}

- (BOOL)checkAssertedRankOnService:(SCNetworkServiceRef)service
			      rank:(SCNetworkServicePrimaryRank)rank
{
	SCNetworkServicePrimaryRank serviceRank;

	if (service == NULL) {
		return NO;
	}
	serviceRank = SCNetworkServiceGetPrimaryRank(service);
	if (serviceRank == rank) {
		return YES;
	}

	return NO;
}

- (BOOL)rankAssertionValidation:(SCNetworkInterfaceRef)interface1
			  rank1:(SCNetworkServicePrimaryRank)rank1
		     interface2:(SCNetworkInterfaceRef)interface2
			  rank2:(SCNetworkServicePrimaryRank)rank2
	interfaceRankValidation:(BOOL)isInterfaceRankValidation
{
	CFStringRef primaryIfName = nil;
	CFStringRef ifName1 = nil;
	CFStringRef ifName2 = nil;
	char *if1 = NULL;
	char *if2 = NULL;
	BOOL ok = YES;

	if (interface1 == NULL || interface2 == NULL) {
		SCTestLog("Rank assertion validation failed as interfaces are invalid");
		return NO;
	}

	ifName1 = SCNetworkInterfaceGetBSDName(interface1);
	ifName2 = SCNetworkInterfaceGetBSDName(interface2);

	/* Validate rank assertion on feth0 and feth2 */
	/* When an interface and service has rank assertions, the most restrictive of the two is not asserted on an interface */
	if (isInterfaceRankValidation) {
		if (rank1 != kSCNetworkServicePrimaryRankDefault) {
			ok = [self checkAssertedRankOnInterface:interface1 rank:rank1];
			if (!ok) {
				SCTestLog("SCNetworkInterfaceSetPrimaryRank(interface, rank) validation failed for interface %@ rank %s", SCNetworkInterfaceGetBSDName(interface1),  get_rank_str(rank1));
				return ok;
			}
		}
		if (rank2 != kSCNetworkServicePrimaryRankDefault) {
			ok = [self checkAssertedRankOnInterface:interface2 rank:rank2];
			if (!ok) {
				SCTestLog("SCNetworkInterfaceSetPrimaryRank(interface, rank) validation failed for interface %@ rank %s", SCNetworkInterfaceGetBSDName(interface2),  get_rank_str(rank2));
				return ok;
			}
		}
	}

	/* Check primary interface on the system based on rank assertion */
	primaryIfName = (__bridge CFStringRef)[self getPrimaryInterfaceName];
	if (rank1 == rank2) {
		if (rank1 == kSCNetworkServicePrimaryRankNever || rank1 == kSCNetworkServicePrimaryRankScoped) {
			if (primaryIfName != NULL) {
				if (CFEqual(ifName1, primaryIfName) || CFEqual(ifName2, primaryIfName)) {
					SCTestLog("Rank assertion validation failed: feth0/feth2 is primary, when rank Never/Scoped is asserted");
					return NO;
				}
			}
		} else if (primaryIfName == NULL) {
			SCTestLog("Rank assertion validation failed: primary interface is NULL when the rank is NOT Never/Scoped");
			return NO;
		}
	} else if (rank1 < rank2) {
		if (rank1 == kSCNetworkServicePrimaryRankNever) {
			if (primaryIfName != NULL && (CFEqual(ifName1, primaryIfName) || CFEqual(ifName2, primaryIfName))) {
				SCTestLog("Rank assertion validation failed: primary interface is %@ when the rank is Never for %@ and Scoped for %@",  primaryIfName, ifName1, ifName2);
				return NO;
			}
		} else if (rank2 == kSCNetworkServicePrimaryRankFirst) {
			if (primaryIfName == NULL || CFEqual(ifName1, primaryIfName)) {
				SCTestLog("Rank assertion validation failed: primary interface is %@ when rank First is asserted on interface %@", primaryIfName, ifName2);
				return NO;
			}
		} else {
			if (primaryIfName != NULL && CFEqual(ifName2, primaryIfName)) {
				SCTestLog("Rank assertion validation failed: primary interface is %@ when rank %s is asserted on interface %@", primaryIfName,    get_rank_str(rank1), ifName1);
				return NO;
			}
		}
	} else {
		if (rank2 == kSCNetworkServicePrimaryRankNever) {
			if (primaryIfName != NULL && (CFEqual(ifName1, primaryIfName) || CFEqual(ifName2, primaryIfName))) {
				SCTestLog("Rank assertion validation failed: primary interface is %@ when the rank is Never for %@ and Scoped for %@",
					  primaryIfName, ifName2, ifName1);
				return NO;
			}
		} else if (rank1 == kSCNetworkServicePrimaryRankFirst) {
			if (primaryIfName == NULL || CFEqual(ifName2, primaryIfName)) {
				SCTestLog("Rank assertion validation failed: primary interface is %@ when rank First is asserted on interface %@", primaryIfName, ifName1);
				return NO;
			}
		} else {
			if (primaryIfName != NULL && CFEqual(ifName1, primaryIfName)) {
				SCTestLog("Rank assertion validation failed: primary interface is %@ when rank %s is asserted on interface %@", primaryIfName, get_rank_str(rank2), ifName2);
				return NO;
			}
		}
	}

	/* Validate Scoped rank assertion on feth0 and feth2 */
	if1 = _SC_cfstring_to_cstring(ifName1, NULL, 0, kCFStringEncodingUTF8);
	if (rank1 == kSCNetworkServicePrimaryRankScoped && ![self checkRoutesFromRoutingTableAreScoped:if1]) {
		SCTestLog("Primary rank Scoped validation for interface %s : checkRoutesFromRoutingTableAreScoped() failed", if1);
		CFAllocatorDeallocate(NULL, if1);
		return NO;
	}
	CFAllocatorDeallocate(NULL, if1);

	if2 = _SC_cfstring_to_cstring(ifName2, NULL, 0, kCFStringEncodingUTF8);
	if (rank2 == kSCNetworkServicePrimaryRankScoped && ![self checkRoutesFromRoutingTableAreScoped:if2]) {
		SCTestLog("Primary rank Scoped validation for interface %s : checkRoutesFromRoutingTableAreScoped() failed", if2);
		CFAllocatorDeallocate(NULL, if2);
		return NO;
	}
	CFAllocatorDeallocate(NULL, if2);

	return ok;
}

- (BOOL)assertRankAndValidate:(CFStringRef)assertionType
			rank1:(SCNetworkServicePrimaryRank)rank1
			rank2:(SCNetworkServicePrimaryRank)rank2
{
	CFStringRef primaryInterface = NULL;
	SCNetworkServiceRef feth0Service = NULL;
	SCNetworkServiceRef feth2Service = NULL;
	NSString *primaryIfName = nil;
	const uint8_t waitTime = 1;
	int count = 0;
	BOOL ifRankAssertion = YES;
	BOOL ok = NO;

	if (CFEqual(assertionType, INTERFACE_RANK)) {
		if (!SCNetworkInterfaceSetPrimaryRank(self.ifEthernet1, rank1)) {
			SCTestLog("Set rank %s for interface %@ failed", get_rank_str(rank1), SCNetworkInterfaceGetBSDName(self.ifEthernet1));
			return ok;
		}
		
		/* If rank2 is "Default", there is a change in rank1 */
		if (rank2 == kSCNetworkServicePrimaryRankDefault) {
			dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
			if (rank1 == rank2) {
				primaryIfName = [self getPrimaryInterfaceName];
				if (primaryIfName == nil) {
					dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
				}
			}
		}

		if (!SCNetworkInterfaceSetPrimaryRank(self.ifEthernet2, rank2)) {
			SCTestLog("Set rank %s for interface %@ failed", get_rank_str(rank2), SCNetworkInterfaceGetBSDName(self.ifEthernet2));
			return ok;
		}
	} else if (CFEqual(assertionType, SERVICE_RANK)) {
		ifRankAssertion = NO;
		/* Set rank on feth0 service */
		feth0Service = _SCNetworkServiceCopyActive(self.store, (__bridge CFStringRef)self.feth0ServiceID);
		if (feth0Service != NULL) {
			if (!SCNetworkServiceSetPrimaryRank(feth0Service, rank1)) {
				SCTestLog("Unable set primary rank for service %@", (__bridge CFStringRef)self.feth0ServiceID);
				goto done;
			}
			/* If rank2 is "Default", there is a change in rank1 */
			if (rank2 == kSCNetworkServicePrimaryRankDefault) {
				dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
				if (rank1 == rank2) {
					primaryIfName = [self getPrimaryInterfaceName];
					if (primaryIfName == nil) {
						dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
					}
				}
			}

			if (![self checkAssertedRankOnService:feth0Service rank:rank1]) {
				SCTestLog("Service rank assertion validation failed for %@", (__bridge CFStringRef)self.feth0ServiceID);
				goto done;
			}
		}

		/* Set rank on feth2 service */
		feth2Service = _SCNetworkServiceCopyActive(self.store, (__bridge CFStringRef)self.feth2ServiceID);
		if (feth2Service != NULL) {

			if (!SCNetworkServiceSetPrimaryRank(feth2Service, rank2)) {
				SCTestLog("Unable set primary rank for service %@", (__bridge CFStringRef)self.feth2ServiceID);
				goto done;
			}

			if (![self checkAssertedRankOnService:feth2Service rank:rank2]) {
				SCTestLog("Service rank assertion validation failed for %@", (__bridge CFStringRef)self.feth2ServiceID);
				goto done;
			}
		}
	}

retry :
	count++;
	if (count <= MAX_TRIES) {
		/* Wait for a signal when IPv4 global state change is expected */
		dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
	} else {
		goto done;
	}

	/* Check if primary interface is NULL, when the rank is "Default" for feth0 and feth2 */
	if (rank1 == kSCNetworkServicePrimaryRankDefault && rank1 == rank2) {
		//In a few cases, primary interface is NULL (as the notification is received later when rank is default)
		primaryInterface = (__bridge CFStringRef)[self getPrimaryInterfaceName];
		if (primaryInterface == NULL) {
			goto retry;
		}
	}

	/* Retry waiting for a signal when there is no expected primary interface */
	if (rank1 < kSCNetworkServicePrimaryRankNever && rank2 < kSCNetworkServicePrimaryRankNever) {
		primaryInterface = (__bridge CFStringRef)[self getPrimaryInterfaceName];
		if (rank1 < rank2) {
			if (primaryInterface == NULL ||
			    (rank2 == kSCNetworkServicePrimaryRankLast && !CFEqual(primaryInterface, (__bridge CFStringRef)@IFNAME1)) ||
			    (rank2 == kSCNetworkServicePrimaryRankFirst && !CFEqual(primaryInterface, (__bridge CFStringRef)@IFNAME2))) {
				goto retry;
			}
		} else if (rank2 < rank1) {
			if (primaryInterface == NULL ||
			    (rank1 == kSCNetworkServicePrimaryRankLast && !CFEqual(primaryInterface, (__bridge CFStringRef)@IFNAME2)) ||
			    (rank1 == kSCNetworkServicePrimaryRankFirst && !CFEqual(primaryInterface, (__bridge CFStringRef)@IFNAME1))) {
				goto retry;
			}
		}
	}
	/* Retry waiting for a signal when an interface rank is "Scoped", but the routes from routing table are not updated */
	if ((rank1 == kSCNetworkServicePrimaryRankScoped && ![self checkRoutesFromRoutingTableAreScoped:IFNAME1]) ||
	    (rank2 == kSCNetworkServicePrimaryRankScoped && ![self checkRoutesFromRoutingTableAreScoped:IFNAME2])) {
		goto retry;
	}

done:
	if (feth0Service != NULL) CFRelease (feth0Service);
	if (feth2Service != NULL) CFRelease (feth2Service);
	ok = [self rankAssertionValidation:self.ifEthernet1 rank1:rank1 interface2:self.ifEthernet2 rank2:rank2 interfaceRankValidation:ifRankAssertion];
	return ok;
}

- (BOOL)assertRankOnInterfaceServiceAndValidate:(SCNetworkInterfaceRef) interface
					 ifRank:(SCNetworkServicePrimaryRank) ifRank
					service:(NSString *) serviceID
				    serviceRank:(SCNetworkServicePrimaryRank) serviceRank
{
	SCNetworkServicePrimaryRank currentRank;
	SCNetworkServiceRef service = NULL;
	const uint8_t waitTime = 1;
	BOOL ok = NO;
	CFStringRef primaryInterface = NULL;

	/* Set rank on feth0 */
	if (!SCNetworkInterfaceSetPrimaryRank(interface, ifRank)) {
		SCTestLog("Set rank %s for interface %@ failed", get_rank_str(ifRank), SCNetworkInterfaceGetBSDName(interface));
		return ok;
	}

	/* Wait for a signal when an interface and service rank is "Default" */
	if (ifRank == kSCNetworkServicePrimaryRankDefault && ifRank == serviceRank) {
		dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
	}

	/* Assert rank on feth0 service */
	if (serviceID != nil) {
		service = _SCNetworkServiceCopyActive(self.store, (__bridge CFStringRef)serviceID);
		if (service != NULL) {
			if (!SCNetworkServiceSetPrimaryRank(service, serviceRank)) {
				SCTestLog("Unable set primary rank for service %@", (__bridge CFStringRef)self.serviceID);
				return ok;
			}
			if (![self checkAssertedRankOnService:service rank:serviceRank]) {
				SCTestLog("Service rank assertion validation failed for %@", (__bridge CFStringRef)self.serviceID);
				return ok;
			}
		}
	}

	/* Wait for a signal when IPv4 global state change is expected */
	if ((ifRank != kSCNetworkServicePrimaryRankScoped && (serviceRank == kSCNetworkServicePrimaryRankDefault || ifRank < serviceRank)) ||
	    (ifRank == kSCNetworkServicePrimaryRankScoped && serviceRank == kSCNetworkServicePrimaryRankDefault)) {
		dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
	}

	/* Interface and Service rank: Use the more restrictive of the two */
	currentRank = ifRank;
	if (ifRank < serviceRank) {
		currentRank = serviceRank;
	}

	/* Retry waiting for a signal when there is no expected primary interface */
	if (currentRank < kSCNetworkServicePrimaryRankNever) {
		primaryInterface = (__bridge CFStringRef)[self getPrimaryInterfaceName];
		if (currentRank == kSCNetworkServicePrimaryRankFirst) {
			if (primaryInterface == NULL || !CFEqual(primaryInterface, (__bridge CFStringRef)@IFNAME1)) {
				dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
			}
		} else if (currentRank == kSCNetworkServicePrimaryRankLast) {
			if (primaryInterface == NULL || !CFEqual(primaryInterface, (__bridge CFStringRef)@IFNAME2)) {
				dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
			}
		}
	}
	/* Retry waiting for a signal when an interface rank is "Scoped", but routes from the routing table are not updated */
	if (currentRank == kSCNetworkServicePrimaryRankScoped && ![self checkRoutesFromRoutingTableAreScoped:IFNAME1]) {
		dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
	}

	ok = [self rankAssertionValidation:interface rank1:currentRank interface2:self.ifEthernet2 rank2:kSCNetworkServicePrimaryRankDefault interfaceRankValidation:NO];

	if (service != NULL) CFRelease(service);

	return ok;
}

- (BOOL)interfaceRankAssertionTest:(SCNetworkInterfaceRef)if1
			interface2:(SCNetworkInterfaceRef)if2
{
	SCNetworkServicePrimaryRank if1Rank;
	SCNetworkServicePrimaryRank if2Rank;
	SCNetworkServicePrimaryRank serviceRank;
	const uint8_t waitTime = 1;
	BOOL status = YES;
	BOOL ifRankAssertion = YES;
	BOOL serviceRankAssertion = YES;
	NSArray *ranks = @[ @"Default", @"First", @"Last", @"Never", @"Scoped"];

	/* Rank Assertion tests on interfaces: feth0, feth2 */
	SCTestLog("Interface Rank Assertion");
	for (NSUInteger i = 0; i < [ranks count]; i++) {
		get_rank_from_string((__bridge CFStringRef)[ranks objectAtIndex:i], &if1Rank);
		for (NSUInteger j = 0; j < [ranks count]; j++) {
			get_rank_from_string((__bridge CFStringRef)[ranks objectAtIndex:j], &if2Rank);
			status = [self assertRankAndValidate:INTERFACE_RANK rank1:if1Rank rank2:if2Rank];
			SCTestLog("Interface : %@ Rank: %s Interface : %@ Rank: %s Validation: %s", SCNetworkInterfaceGetBSDName(if1),get_rank_str(if1Rank), SCNetworkInterfaceGetBSDName(if2), get_rank_str(if2Rank), status ? "Passed" : "Failed");
			ifRankAssertion &= status;
		}
	}

	/* Interface and Service rank assertion tests */
	SCTestLog("Interface and Service Rank Assertion");
	/* Assert rank Default on feth2 */
	if (!SCNetworkInterfaceSetPrimaryRank(self.ifEthernet2, kSCNetworkServicePrimaryRankDefault)) {
		SCTestLog("Set rank %s for interface %@ failed", get_rank_str(kSCNetworkServicePrimaryRankDefault), SCNetworkInterfaceGetBSDName(self.ifEthernet2));
		return NO;
	}

	/* Assert ranks on feth0 and serviceFeth0 */
	for (NSUInteger i = 0; i < [ranks count]; i++) {
		get_rank_from_string((__bridge CFStringRef)[ranks objectAtIndex:i], &if1Rank);
		for (NSUInteger j = 0; j < [ranks count]; j++) {
			get_rank_from_string((__bridge CFStringRef)[ranks objectAtIndex:j], &serviceRank);
			status = [self assertRankOnInterfaceServiceAndValidate:self.ifEthernet1 ifRank:if1Rank service:self.serviceID serviceRank:serviceRank];
			SCTestLog("Interface: %@ Rank: %s Service: %@ Rank: %s Validation: %s",
				  SCNetworkInterfaceGetBSDName(self.ifEthernet1), get_rank_str(if1Rank), self.serviceID, get_rank_str(serviceRank), status ? "Passed" : "Failed");
			serviceRankAssertion &= status;
		}
	}

	/* Check primary interface before exiting so that the reachability unit test does not fail */
	if (!SCNetworkInterfaceSetPrimaryRank(self.ifEthernet2, kSCNetworkServicePrimaryRankNever)) {
		SCTestLog("Set rank %s for interface %@ failed", get_rank_str(kSCNetworkServicePrimaryRankNever), SCNetworkInterfaceGetBSDName(self.ifEthernet2));
	}
	if ([self getPrimaryInterfaceName] == nil) {
		dispatch_semaphore_wait(self.globalIPv4Sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC));
	}

	if (ifRankAssertion && serviceRankAssertion) {
		return YES;
	}
	return NO;
}

- (BOOL)serviceRankAssertionTest
{
	SCNetworkServicePrimaryRank serviceRank1;
	SCNetworkServicePrimaryRank serviceRank2;
	BOOL status = YES;
	BOOL serviceRankAssertion = YES;
	NSArray *ranks = @[ @"Default", @"First", @"Last", @"Never", @"Scoped"];

	/* Service rank assertion tests */
	SCTestLog("Service Rank Assertion");
	/* Assert rank Default on feth0 and feth2 */
	if (!SCNetworkInterfaceSetPrimaryRank(self.ifEthernet1, kSCNetworkServicePrimaryRankDefault)) {
		SCTestLog("Set rank %s for interface %@ failed", get_rank_str(kSCNetworkServicePrimaryRankDefault), SCNetworkInterfaceGetBSDName(self.ifEthernet2));
		return NO;
	}
	if (!SCNetworkInterfaceSetPrimaryRank(self.ifEthernet2, kSCNetworkServicePrimaryRankDefault)) {
		SCTestLog("Set rank %s for interface %@ failed", get_rank_str(kSCNetworkServicePrimaryRankDefault), SCNetworkInterfaceGetBSDName(self.ifEthernet2));
		return NO;
	}

	/* Assert ranks on serviceFeth0 and serviceFeth2 */
	for (NSUInteger i = 0; i < [ranks count]; i++) {
		get_rank_from_string((__bridge CFStringRef)[ranks objectAtIndex:i], &serviceRank1);
		for (NSUInteger j = 0; j < [ranks count]; j++) {
			get_rank_from_string((__bridge CFStringRef)[ranks objectAtIndex:j], &serviceRank2);
			status = [self assertRankAndValidate:SERVICE_RANK rank1:serviceRank1 rank2:serviceRank2];
			SCTestLog("Service: %@ Rank: %s Service: %@ Rank: %s Validation: %s",
				  (__bridge CFStringRef)self.feth0ServiceID, get_rank_str(serviceRank1), (__bridge CFStringRef)self.feth2ServiceID, get_rank_str(serviceRank2), status ? "Passed" : "Failed");
			serviceRankAssertion &= status;
		}
	}

	if (serviceRankAssertion) {
		return YES;
	}
	return NO;
}

#pragma mark -
#pragma mark Service Rank Assertion test

- (BOOL)unitTestServiceRankAssertion
{
	CFArrayRef activeInterfaces = NULL;
	CFIndex i;
	CFIndex n;
	SCNetworkServiceRef serviceFeth0 = NULL;
	SCNetworkServiceRef serviceFeth2 = NULL;
	BOOL ok = NO;

	/* Assert rank Never on all interfaces */
	activeInterfaces = [self copyActiveInterfaces];
	if (activeInterfaces != NULL) {
		n = CFArrayGetCount(activeInterfaces);
		for (i = 0; i < n; i++) {
			SCNetworkInterfaceRef           interface;
			interface = CFArrayGetValueAtIndex(activeInterfaces, i);
			SCNetworkInterfaceSetPrimaryRank(interface, kSCNetworkServicePrimaryRankNever);
		}
	}

	[self watchForNewInterface:TRUE];

	/* Ethernet : feth0 and feth1 */
	ok = [self createClientRouterAndPeer:@IFNAME1 router:@ROUTER_IFNAME1];
	if (!ok) {
		goto done;
	}

	/* Ethernet interface: feth0 */
	self.ifEthernet1 = _SCNetworkInterfaceCreateWithBSDName(NULL, (__bridge CFStringRef)@IFNAME1, 0);
	if (self.ifEthernet1 == NULL) {
		SCTestLog("Could not create interface with BSD Name %@", (__bridge CFStringRef)@IFNAME1);
		goto done;
	}

	/* Create a new service for feth0 */
	serviceFeth0 = [self createAndConfigureService:self.ifEthernet1];
	self.feth0ServiceID = (__bridge NSString *) SCNetworkServiceGetServiceID(serviceFeth0);

	/* Configure IPv4 address for router interface feth1 */
	[self interfaceAddIPv4Addr:ROUTER_IFNAME1 ipAddress:FETH1_IP netmask:NETMASK];

	/* Ethernet: feth2 and feth3 */
	ok = [self createClientRouterAndPeer:@IFNAME2 router:@ROUTER_IFNAME2];
	if (!ok) {
		goto done;
	}

	/* Ethernet interface: feth2 */
	self.ifEthernet2 = _SCNetworkInterfaceCreateWithBSDName(NULL, (__bridge CFStringRef)@IFNAME2, 0);
	if (self.ifEthernet2 == NULL) {
		SCTestLog("Could not create interface with BSD Name %@", (__bridge CFStringRef)@IFNAME2);
		goto done;
	}

	/* Create a new service for feth2 */
	serviceFeth2 = [self createAndConfigureService:self.ifEthernet2];
	self.feth2ServiceID = (__bridge NSString *) SCNetworkServiceGetServiceID(serviceFeth2);

	/* Configure IPv4 address for router interface feth4 */
	[self interfaceAddIPv4Addr:ROUTER_IFNAME2 ipAddress:FETH3_IP netmask:NETMASK];

	[self watchForGlobalIPv4StateChange:TRUE];

	ok = [self serviceRankAssertionTest];

	[self watchForGlobalIPv4StateChange:FALSE];

	[self cleanupFethServicesAndInterfaces];

	/* Assert rank Default on all interfaces */
	if (activeInterfaces != NULL) {
		n = CFArrayGetCount(activeInterfaces);
		for (i = 0; i < n; i++) {
			SCNetworkInterfaceRef           interface;
			interface = CFArrayGetValueAtIndex(activeInterfaces, i);
			SCNetworkInterfaceSetPrimaryRank(interface, kSCNetworkServicePrimaryRankDefault);
		}
	}

done:
	if (activeInterfaces != NULL) CFRelease(activeInterfaces);
	if (serviceFeth0 != NULL) CFRelease(serviceFeth0);
	if (serviceFeth2 != NULL) CFRelease(serviceFeth2);
	if (self.ifEthernet1 != NULL) CFRelease(self.ifEthernet1);
	if (self.ifEthernet2 != NULL) CFRelease(self.ifEthernet2);
	[self watchForNewInterface:FALSE];
	[self cleanupFethServicesAndInterfaces];

	if (ok) {
		SCTestLog("Verified that SCNetworkInterface rank assertions and service rank assertions behave as expected");
	}

	return ok;
}

#pragma mark -
#pragma mark Interface Rank Assertion test

- (BOOL)unitTestInterfaceRankAssertion
{
	SCNetworkServiceRef serviceFeth0 = NULL;
	SCNetworkServiceRef serviceFeth2 = NULL;
	CFArrayRef activeInterfaces = NULL;
	CFIndex i;
	CFIndex n;
	BOOL ok = NO;

	/* Assert rank Never on all interfaces */
	activeInterfaces = [self copyActiveInterfaces];
	if (activeInterfaces != NULL) {
		n = CFArrayGetCount(activeInterfaces);
		for (i = 0; i < n; i++) {
			SCNetworkInterfaceRef           interface;
			interface = CFArrayGetValueAtIndex(activeInterfaces, i);
			SCNetworkInterfaceSetPrimaryRank(interface, kSCNetworkServicePrimaryRankNever);
		}
	}

	[self watchForNewInterface:TRUE];

	/* Ethernet : feth0 and feth1 */
	ok = [self createClientRouterAndPeer:@IFNAME1 router:@ROUTER_IFNAME1];
	if (!ok) {
		goto done;
	}

	/* Ethernet interface: feth0 */
	self.ifEthernet1 = _SCNetworkInterfaceCreateWithBSDName(NULL, (__bridge CFStringRef)@IFNAME1, 0);
	if (self.ifEthernet1 == NULL) {
		SCTestLog("Could not create interface with BSD Name %@", (__bridge CFStringRef)@IFNAME1);
		goto done;
	}

	/* Create a new service for feth0 */
	serviceFeth0 = [self createAndConfigureService:self.ifEthernet1];
	self.serviceID = (__bridge NSString *) SCNetworkServiceGetServiceID(serviceFeth0);

	/* Configure IPv4 address for router interface feth1 */
	[self interfaceAddIPv4Addr:ROUTER_IFNAME1 ipAddress:FETH1_IP netmask:NETMASK];

	/* Ethernet: feth2 and feth3 */
	ok = [self createClientRouterAndPeer:@IFNAME2 router:@ROUTER_IFNAME2];
	if (!ok) {
		goto done;
	}

	/* Ethernet interface: feth2 */
	self.ifEthernet2 = _SCNetworkInterfaceCreateWithBSDName(NULL, (__bridge CFStringRef)@IFNAME2, 0);
	if (self.ifEthernet2 == NULL) {
		SCTestLog("Could not create interface with BSD Name %@", (__bridge CFStringRef)@IFNAME2);
		goto done;
	}

	/* Create a new service for feth2 */
	serviceFeth2 = [self createAndConfigureService:self.ifEthernet2];

	/* Configure IPv4 address for router interface feth4 */
	[self interfaceAddIPv4Addr:ROUTER_IFNAME2 ipAddress:FETH3_IP netmask:NETMASK];

	[self watchForGlobalIPv4StateChange:TRUE];

	ok = [self interfaceRankAssertionTest:self.ifEthernet1 interface2:self.ifEthernet2];

	[self watchForGlobalIPv4StateChange:FALSE];

	/* Assert rank Default on all interfaces */
	if (activeInterfaces != NULL) {
		n = CFArrayGetCount(activeInterfaces);
		for (i = 0; i < n; i++) {
			SCNetworkInterfaceRef           interface;
			interface = CFArrayGetValueAtIndex(activeInterfaces, i);
			SCNetworkInterfaceSetPrimaryRank(interface, kSCNetworkServicePrimaryRankDefault);
		}
	}

done:
	if (activeInterfaces != NULL) CFRelease(activeInterfaces);
	if (serviceFeth0 != NULL) CFRelease(serviceFeth0);
	if (serviceFeth2 != NULL) CFRelease(serviceFeth2);
	if (self.ifEthernet1 != NULL) CFRelease(self.ifEthernet1);
	if (self.ifEthernet2 != NULL) CFRelease(self.ifEthernet2);
	[self watchForNewInterface:FALSE];
	[self cleanupFethServicesAndInterfaces];

	if (ok) {
		SCTestLog("Verified that SCNetworkInterface rank assertions and service rank assertions behave as expected");
	}

	return ok;
}

@end
#endif
