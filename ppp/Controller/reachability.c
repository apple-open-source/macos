/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
 */
#include <arpa/inet.h>
#include <SystemConfiguration/SCPrivate.h>

#include "reachability.h"
#include "scnc_main.h"
#include "scnc_utils.h"

static ReachabilityChangedBlock	g_cb_block			= NULL;
static CFRunLoopRef				g_cb_runloop		= NULL;
static CFTypeRef				g_cb_runloop_mode	= NULL;
static dispatch_queue_t			g_reach_queue		= NULL;

static void
reachability_target_dispose(SCNetworkReachabilityRef target)
{
	dispatch_async(g_reach_queue, ^{
		SCNetworkReachabilitySetCallback(target, NULL, NULL);
		SCNetworkReachabilitySetDispatchQueue(target, NULL);
		CFRelease(target);
	});
}

static void
remote_address_reachability_callback(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags flags, void *info)
{
	CFStringRef service_id = (CFStringRef)info;
	int reach_if_index;

	CFRetain(target);
	CFRetain(service_id);

	SCNetworkReachabilityGetFlags(target, &flags);
	reach_if_index = SCNetworkReachabilityGetInterfaceIndex(target);

	CFRunLoopPerformBlock(g_cb_runloop, g_cb_runloop_mode, ^{
		struct service *serv = findbyserviceID(service_id);
		if (serv != NULL && serv->remote_address_reachability == target) {
			serv->remote_address_reach_flags = flags;
			serv->remote_address_reach_ifindex = reach_if_index;
			g_cb_block(serv);
		}
		CFRelease(target);
		CFRelease(service_id);
	});
	CFRunLoopWakeUp(g_cb_runloop);
}

void
reachability_init(CFRunLoopRef cb_runloop, CFTypeRef cb_runloop_mode, ReachabilityChangedBlock cb_block)
{
	static dispatch_once_t init_reachability = 0;

	dispatch_once(&init_reachability, ^{
		g_reach_queue = dispatch_queue_create("PPPController reachability dispatch queue", NULL);
		g_cb_runloop = cb_runloop;
		CFRetain(g_cb_runloop);
		g_cb_runloop_mode = cb_runloop_mode;
		CFRetain(g_cb_runloop_mode);
		g_cb_block = Block_copy(cb_block);
	});
}

void
reachability_clear(struct service *serv)
{
	if (serv->remote_address_reachability != NULL) {
		reachability_target_dispose(serv->remote_address_reachability);
	}
	serv->remote_address_reachability = NULL;
	serv->remote_address_reach_flags = 0;
	serv->remote_address_reach_ifindex = -1;
}

void
reachability_reset(struct service *serv)
{
	reachability_clear(serv);

	CFStringRef remote_address = scnc_copy_remote_server(serv, NULL);
	if (isA_CFString(remote_address) && CFStringGetLength(remote_address) > 0) {
		char *addr_cstr;
		CFIndex addr_cstr_len;
		struct sockaddr_storage sa_storage;
		CFMutableDictionaryRef reach_options;
		CFStringRef service_id;

		memset(&sa_storage, 0, sizeof(sa_storage));

		addr_cstr_len = CFStringGetLength(remote_address); /* Assume that the address is ASCII */
		addr_cstr = CFAllocatorAllocate(kCFAllocatorDefault, addr_cstr_len + 1, 0);

		CFStringGetCString(remote_address, addr_cstr, addr_cstr_len, kCFStringEncodingASCII);

		reach_options = CFDictionaryCreateMutable(kCFAllocatorDefault,
		                                          0,
		                                          &kCFTypeDictionaryKeyCallBacks,
		                                          &kCFTypeDictionaryValueCallBacks);

		if (inet_pton(AF_INET, addr_cstr, &((struct sockaddr_in *)&sa_storage)->sin_addr) == 1) {
			CFDataRef addr_data;
			struct sockaddr_in *sa_in = (struct sockaddr_in *)&sa_storage;
			sa_in->sin_family = AF_INET;
			sa_in->sin_len = sizeof(struct sockaddr_in);
			addr_data = CFDataCreate(kCFAllocatorDefault, (const uint8_t *)sa_in, sa_in->sin_len);
			CFDictionarySetValue(reach_options, kSCNetworkReachabilityOptionRemoteAddress, addr_data);
			CFRelease(addr_data);
		} else if (inet_pton(AF_INET6, addr_cstr, &((struct sockaddr_in6 *)&sa_storage)->sin6_addr) == 1) {
			CFDataRef addr_data;
			struct sockaddr_in6 *sa_in6 = (struct sockaddr_in6 *)&sa_storage;
			sa_in6->sin6_family = AF_INET6;
			sa_in6->sin6_len = sizeof(struct sockaddr_in6);
			addr_data = CFDataCreate(kCFAllocatorDefault, (const uint8_t *)sa_in6, sa_in6->sin6_len);
			CFDictionarySetValue(reach_options, kSCNetworkReachabilityOptionRemoteAddress, addr_data);
			CFRelease(addr_data);
		} else {
			CFDictionarySetValue(reach_options, kSCNetworkReachabilityOptionNodeName, remote_address);
		}

		CFRelease(remote_address);
		CFAllocatorDeallocate(kCFAllocatorDefault, addr_cstr);

		CFDictionarySetValue(reach_options, kSCNetworkReachabilityOptionConnectionOnDemandBypass, kCFBooleanTrue);

		service_id = serv->serviceID;
		CFRetain(service_id);

		dispatch_async(g_reach_queue, ^{
			SCNetworkReachabilityRef reach_ref = SCNetworkReachabilityCreateWithOptions(kCFAllocatorDefault, reach_options);
			CFRelease(reach_options);

			if (reach_ref != NULL) {
				SCNetworkReachabilityContext reach_ctx = { 0, (void *)service_id, CFRetain, CFRelease, CFCopyDescription };
				SCNetworkReachabilitySetCallback(reach_ref, remote_address_reachability_callback, &reach_ctx);
				SCNetworkReachabilitySetDispatchQueue(reach_ref, g_reach_queue);

				CFRunLoopPerformBlock(g_cb_runloop, g_cb_runloop_mode, ^{
					struct service *serv = findbyserviceID(service_id);
					Boolean retained = FALSE;
					if (serv != NULL) {
						if (serv->remote_address_reachability != NULL) {
							reachability_target_dispose(serv->remote_address_reachability);
							serv->remote_address_reachability = NULL;
						}
						serv->remote_address_reachability = reach_ref;
						retained = TRUE;
						dispatch_async(g_reach_queue, ^{
							remote_address_reachability_callback(reach_ref, 0, (void *)service_id);
						});
					}

					if (!retained) {
						reachability_target_dispose(reach_ref);
					}
				});
				CFRunLoopWakeUp(g_cb_runloop);
			} else {
				SCLog(TRUE, LOG_ERR, CFSTR("reset_reachability: failed to create a reachability target for %@"), remote_address);
			}
			CFRelease(service_id);
		});
	}
}

