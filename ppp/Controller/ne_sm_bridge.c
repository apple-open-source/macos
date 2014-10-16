/*
 * Copyright (c) 2014 Apple Inc.
 * All rights reserved.
 */
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mach/mach_time.h>
#include <netinet/in_var.h>
#include <xpc/private.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SNHelperPrivate.h>

#include "ne_sm_bridge_private.h"

#include "scnc_main.h"
#include "scnc_client.h"
#include "ipsec_manager.h"
#include "ppp_manager.h"

extern TAILQ_HEAD(, service) service_head; /* Defined in scnc_main.c */
extern CFBundleRef gBundleRef; /* Defined in scnc_main.c */

struct _ne_sm_bridge {
	ne_sm_bridge_type_t type;
	struct service serv;
	void *info;
	void (^disposable_callback)(void);
};

static struct ne_sm_bridge_callbacks *g_callbacks = NULL;

static void
ne_sm_bridge_log(int level, CFStringRef format, ...)
{
	va_list args;
	va_start(args, format);
	ne_sm_bridge_logv(level, format, args);
	va_end(args);
}

bool
ne_sm_bridge_logv(int level, CFStringRef format, va_list args)
{
	if (g_callbacks != NULL && g_callbacks->log != NULL) {
		g_callbacks->log(level, format, args);
		return true;
	} else {
		return false;
	}
}

bool
ne_sm_bridge_is_logging_at_level(int level)
{
	if (g_callbacks != NULL && g_callbacks->is_logging_at_level != NULL) {
		return g_callbacks->is_logging_at_level(level);
	} else {
		return true;
	}
}

CFDictionaryRef
ne_sm_bridge_copy_configuration(ne_sm_bridge_t bridge)
{
	if (g_callbacks != NULL && g_callbacks->copy_service_configuration != NULL) {
		return g_callbacks->copy_service_configuration(bridge->info);
	} else {
		return NULL;
	}
}

void
ne_sm_bridge_status_changed(ne_sm_bridge_t bridge)
{
	if (g_callbacks != NULL && g_callbacks->status_changed != NULL) {
		g_callbacks->status_changed(bridge->info, scnc_getstatus(&bridge->serv));
	}
}

void
ne_sm_bridge_acknowledge_sleep(ne_sm_bridge_t bridge)
{
	if (g_callbacks != NULL && g_callbacks->acknowledge_sleep != NULL) {
		g_callbacks->acknowledge_sleep(bridge->info);
	}
}

void
ne_sm_bridge_filter_state_dictionaries(ne_sm_bridge_t bridge, CFMutableArrayRef names, CFMutableArrayRef dictionaries)
{
	if (g_callbacks != NULL && g_callbacks->filter_state_dictionaries != NULL) {
		g_callbacks->filter_state_dictionaries(bridge->info, names, dictionaries);
	}
}

CFStringRef
ne_sm_bridge_copy_password_from_keychain(ne_sm_bridge_t bridge, CFStringRef type)
{
	if (g_callbacks != NULL && g_callbacks->copy_password_from_keychain != NULL) {
		return g_callbacks->copy_password_from_keychain(bridge->info, type);
	}
	return NULL;
}

void
ne_sm_bridge_allow_dispose(ne_sm_bridge_t bridge)
{
	if (bridge->disposable_callback != NULL) {
		bridge->disposable_callback();
		Block_release(bridge->disposable_callback);
		bridge->disposable_callback = NULL;
	}
}

uint64_t
ne_sm_bridge_get_connect_time(ne_sm_bridge_t bridge)
{
	if (g_callbacks != NULL && g_callbacks->get_connect_time != NULL) {
		return g_callbacks->get_connect_time(bridge->info);
	}
	return 0;
}

bool
ne_sm_bridge_request_install(ne_sm_bridge_t bridge, bool exclusive)
{
	if (g_callbacks != NULL && g_callbacks->request_install != NULL) {
		g_callbacks->request_install(bridge->info, exclusive);
		return true;
	}
	return false;
}

bool
ne_sm_bridge_request_uninstall(ne_sm_bridge_t bridge)
{
	if (g_callbacks != NULL && g_callbacks->request_uninstall != NULL) {
		g_callbacks->request_uninstall(bridge->info);
		return true;
	}
	return false;
}

bool
ne_sm_bridge_start_profile_janitor(ne_sm_bridge_t bridge, CFStringRef profileIdentifier)
{
	char profileIdentifierStr[256];
	if (profileIdentifier == NULL || !CFStringGetCString(profileIdentifier, profileIdentifierStr, sizeof(profileIdentifierStr), kCFStringEncodingUTF8)) {
		return false;
	}
	
	if (g_callbacks != NULL && g_callbacks->start_profile_janitor != NULL) {
		g_callbacks->start_profile_janitor(bridge->info, profileIdentifierStr);
		return true;
	}
	return false;
}

void
ne_sm_bridge_clear_saved_password(ne_sm_bridge_t bridge, CFStringRef type)
{
#if NE_SM_BRIDGE_VERSION > 2
	if (g_callbacks != NULL && g_callbacks->clear_saved_password != NULL) {
		g_callbacks->clear_saved_password(bridge->info, type);
	}
#else
#pragma unused(bridge, type)
#endif
}

static bool
init_controller(void)
{
	static dispatch_once_t controller_once = 0;
	static bool success = false;

	dispatch_once(&controller_once,
		^{
			mach_timebase_info_data_t timebaseInfo;
			const CFStringRef scdOptionsKeys[] = { kSCDynamicStoreUseSessionKeys };
			const CFBooleanRef scdOptionsValues[] = { kCFBooleanTrue };
			CFDictionaryRef scdOptions = NULL;

			scnc_init_resources(gBundleRef);

			/* Initialize time scale */
			if (mach_timebase_info(&timebaseInfo) != KERN_SUCCESS) {
				ne_sm_bridge_log(LOG_ERR, CFSTR("init_controller: mach_timebase_info failed"));
				goto fail;
			}
			gTimeScaleSeconds = ((double) timebaseInfo.numer / (double) timebaseInfo.denom) / 1000000000;

			scdOptions = CFDictionaryCreate(kCFAllocatorDefault,
			                                (const void **)scdOptionsKeys,
			                                (const void **)scdOptionsValues,
			                                sizeof(scdOptionsValues) / sizeof(scdOptionsValues[0]),
			                                &kCFTypeDictionaryKeyCallBacks,
			                                &kCFTypeDictionaryValueCallBacks);

			gDynamicStore = SCDynamicStoreCreateWithOptions(kCFAllocatorDefault, CFSTR("NE - SCNC bridge"), scdOptions, NULL, NULL);
			if (gDynamicStore == NULL) {
				ne_sm_bridge_log(LOG_ERR, CFSTR("init_controller: SCDynamicStoreCreateWithOptions failed: %s"), SCErrorString(SCError()));
				goto fail;
			}

			TAILQ_INIT(&service_head);
			client_init_all();
			ipsec_init_things();

			success = true;
fail:
			if (scdOptions != NULL) {
				CFRelease(scdOptions);
			}
		});

	return success;
}

static void
bridge_destroy(ne_sm_bridge_t bridge)
{
	if (bridge->type == NESMBridgeTypeIPSec) {
		ipsec_dispose_service(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		ppp_dispose_service(&bridge->serv);
	}

	CFRelease(bridge->serv.serviceID);
	CFRelease(bridge->serv.typeRef);
	if (bridge->serv.subtypeRef != NULL) {
		CFRelease(bridge->serv.subtypeRef);
	}
	TAILQ_REMOVE(&service_head, &bridge->serv, next);

	if (bridge->disposable_callback != NULL) {
		Block_release(bridge->disposable_callback);
	}

	free(bridge->serv.sid);
	free(bridge);
}

static ne_sm_bridge_t 
bridge_create(ne_sm_bridge_type_t type, CFStringRef serviceID, void *info)
{
	ne_sm_bridge_t new_bridge;
	CFIndex sid_len;
	int error = 0;

	if (!init_controller()) {
		return NULL;
	}

	new_bridge = (ne_sm_bridge_t)malloc(sizeof(*new_bridge));
	memset(new_bridge, 0, sizeof(*new_bridge));

	new_bridge->type = type;
	new_bridge->info = info;

	new_bridge->serv.serviceID = CFRetain(serviceID);
	sid_len = CFStringGetLength(serviceID) + 1;
	new_bridge->serv.sid = malloc(sid_len);
	CFStringGetCString(serviceID, (char*)new_bridge->serv.sid, sid_len, kCFStringEncodingUTF8);

	new_bridge->serv.ne_sm_bridge = new_bridge;

	if (type == NESMBridgeTypeIPSec) {
		new_bridge->serv.typeRef = CFRetain(kSCValNetInterfaceTypeIPSec);
		new_bridge->serv.type = TYPE_IPSEC;
		ipsec_new_service(&new_bridge->serv);
		error = ipsec_setup_service(&new_bridge->serv);
		if (error) {
			ne_sm_bridge_log(LOG_ERR, CFSTR("bridge_create: ipsec_setup_service failed: %d"), error);
		}
	} else if (type == NESMBridgeTypeL2TP || type == NESMBridgeTypePPTP) {
		new_bridge->serv.typeRef = CFRetain(kSCValNetInterfaceTypePPP);
		new_bridge->serv.type = TYPE_PPP;
		if (type == NESMBridgeTypeL2TP) {
			new_bridge->serv.subtypeRef = CFRetain(kSCValNetInterfaceSubTypeL2TP);
		} else {
			new_bridge->serv.subtypeRef = CFRetain(kSCValNetInterfaceSubTypePPTP);
		}
		new_bridge->serv.subtype = ppp_subtype(new_bridge->serv.subtypeRef);
		ppp_new_service(&new_bridge->serv);
		error = ppp_setup_service(&new_bridge->serv);
		if (error) {
			ne_sm_bridge_log(LOG_ERR, CFSTR("bridge_create: ppp_setup_service failed: %d"), error);
		}
	}

	new_bridge->serv.unit = findfreeunit(new_bridge->serv.type, new_bridge->serv.subtype);
	if (new_bridge->serv.unit == 0xFFFF) {
		ne_sm_bridge_log(LOG_ERR, CFSTR("bridge_create: findfreeunit failed"));
		error = ENOMEM;
	}

	TAILQ_INSERT_TAIL(&service_head, &new_bridge->serv, next);

	if (error) {
		bridge_destroy(new_bridge);
		return NULL;
	}

	new_bridge->serv.initialized = TRUE;

	return new_bridge;
}

static void
bridge_handle_network_change_event(ne_sm_bridge_t bridge, const char *ifname, nwi_ifstate_flags flags)
{
	if (bridge->type == NESMBridgeTypeIPSec && scnc_getstatus(&bridge->serv) != kSCNetworkConnectionDisconnected) {
		struct {
			struct kern_event_msg msg;
			uint8_t buffer[sizeof(struct kev_in_data)];
		} event;
		struct kev_in_data *inetdata = (struct kev_in_data *)event.msg.event_data;
		int unitIdx;

		memset(&event, 0, sizeof(event));

		if (flags & NWI_IFSTATE_FLAGS_HAS_IPV4) {
			int ifr_socket;
			struct ifreq ifr;
			int ioctl_result;

			memset(&ifr, 0, sizeof(ifr));
			ifr.ifr_addr.sa_family = AF_INET;
			strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);

			ifr_socket = socket(AF_INET, SOCK_DGRAM, 0);
			ioctl_result = ioctl(ifr_socket, SIOCGIFADDR, &ifr);
			close(ifr_socket);

			if (ioctl_result < 0) {
				ne_sm_bridge_log(LOG_ERR, CFSTR("ioctl(SIOCGIFADDR) failed: %s"), strerror(errno));
				return;
			}

			memcpy(&inetdata->ia_addr, &((struct sockaddr_in *)(void *)&ifr.ifr_addr)->sin_addr, sizeof(inetdata->ia_addr));
		}

		for (unitIdx = 0; ifname[unitIdx] != '\0' && !isdigit(ifname[unitIdx]); unitIdx++);

		strncpy(inetdata->link_data.if_name, ifname, unitIdx);

		if (isdigit(ifname[unitIdx])) {
			inetdata->link_data.if_unit = (uint32_t)strtol((ifname + unitIdx), NULL, 10);
		}

		if (!strcmp(inetdata->link_data.if_name, "ppp")) {
			inetdata->link_data.if_family = APPLE_IF_FAM_PPP;
		}

		if (flags & NWI_IFSTATE_FLAGS_HAS_IPV4) {
			event.msg.event_code = KEV_INET_NEW_ADDR;
		} else {
			event.msg.event_code = KEV_INET_ADDR_DELETED;
		}

		if (ne_sm_bridge_is_logging_at_level(LOG_DEBUG)) {
			if (event.msg.event_code == KEV_INET_NEW_ADDR) {
				char addr_str[INET_ADDRSTRLEN];
				memset(addr_str, 0, sizeof(addr_str));
				inet_ntop(AF_INET, &inetdata->ia_addr, addr_str, sizeof(addr_str));

				ne_sm_bridge_log(LOG_DEBUG, CFSTR("Network change event: added address %s to interface %s%d (family %d)"), addr_str, inetdata->link_data.if_name, inetdata->link_data.if_unit, inetdata->link_data.if_family);
			} else if (event.msg.event_code == KEV_INET_ADDR_DELETED) {
				ne_sm_bridge_log(LOG_DEBUG, CFSTR("Network change event: deleted address from interface %s%d (family %d)"), inetdata->link_data.if_name, inetdata->link_data.if_unit, inetdata->link_data.if_family);
			}
		}

		ipsec_network_event(&bridge->serv, &event.msg);
	}
}

static bool
bridge_handle_sleep(ne_sm_bridge_t bridge)
{
	bool result = false;

	if (bridge->type == NESMBridgeTypeIPSec) {
		result = ipsec_will_sleep(&bridge->serv, 0);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		result = (ppp_will_sleep(&bridge->serv, 0) > 0);
	}

	ne_sm_bridge_log(LOG_DEBUG, CFSTR("handle sleep for bridge type %d returning %d"), bridge->type, result);

	return result;
}

static bool
bridge_can_sleep(ne_sm_bridge_t bridge)
{
	bool result = true; 

	if (bridge->type == NESMBridgeTypeIPSec) {
		result = ipsec_can_sleep(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		result = ppp_can_sleep(&bridge->serv);
	}

	ne_sm_bridge_log(LOG_DEBUG, CFSTR("can sleep for bridge type %d returning %d"), bridge->type, result);

	return result;
}

static void
bridge_handle_sleep_time(ne_sm_bridge_t bridge, double sleep_time)
{
	ne_sm_bridge_log(LOG_INFO, CFSTR("System slept for %f secs"), sleep_time);
	if (bridge->serv.flags & FLAG_SETUP_DISCONNECTONWAKE) {
		double wake_timeout = 0;
#if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
		wake_timeout = scnc_getsleepwaketimeout(&bridge->serv);
#endif

		ne_sm_bridge_log(LOG_INFO, CFSTR("Session is configured to disconnect on wake if slept for more than %f seconds"), wake_timeout);
		bridge->serv.connectionslepttime += (uint32_t)sleep_time;

		if (sleep_time > wake_timeout) {
			scnc_idle_disconnect(&bridge->serv);
		}
	}
}

static void
bridge_handle_wakeup(ne_sm_bridge_t bridge)
{
	ne_sm_bridge_log(LOG_INFO, CFSTR("Handling wake up for bridge type %d"), bridge->type);
	if (bridge->type == NESMBridgeTypeIPSec) {
		ipsec_wake_up(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		ppp_wake_up(&bridge->serv);
	}
}

static bool
bridge_handle_start(ne_sm_bridge_t bridge, CFDictionaryRef options, uid_t uid, gid_t gid, mach_port_t bootstrap_port, mach_port_t audit_session_port, bool on_demand)
{
	if (bridge->type == NESMBridgeTypeIPSec) {
		return (ipsec_start(&bridge->serv, options, uid, gid, bootstrap_port, false, on_demand) == 0);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		return (ppp_start(&bridge->serv, options, uid, gid, bootstrap_port, audit_session_port, false, on_demand) == 0);
	}
	return false;
}

static bool
bridge_handle_stop(ne_sm_bridge_t bridge)
{
	ne_sm_bridge_log(LOG_INFO, CFSTR("Handling stop for bridge type %d"), bridge->type);
	if (bridge->type == NESMBridgeTypeIPSec) {
		return (ipsec_stop(&bridge->serv, 0) == 0);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		return (ppp_stop(&bridge->serv, SIGTERM) == 0);
	}
	return false;
}

static void
bridge_get_security_session_info(ne_sm_bridge_t bridge, xpc_object_t request, mach_port_t *bootstrap_port, mach_port_t *audit_session_port)
{
	xpc_connection_t connection = xpc_dictionary_get_remote_connection(request);
	xpc_object_t entitlement = xpc_connection_copy_entitlement_value(connection, NESessionManagerPrivilegedEntitlement);
    
	if ((bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) &&
	    isa_xpc_bool(entitlement) && xpc_bool_get_value(entitlement))
	{
		*bootstrap_port = bridge->serv.bootstrap;
		*audit_session_port = bridge->serv.au_session;
	} else {
		*bootstrap_port = MACH_PORT_NULL;
		*audit_session_port = MACH_PORT_NULL;
	}
    
	if (entitlement) {
		xpc_release(entitlement);
	}
}

static CFDictionaryRef
bridge_copy_configuration(ne_sm_bridge_t bridge, xpc_object_t request)
{
	xpc_connection_t connection = xpc_dictionary_get_remote_connection(request);
	CFDictionaryRef serviceConfig = ne_sm_bridge_copy_configuration(bridge);
	CFDictionaryRef userOptions = NULL;
	CFMutableDictionaryRef info = NULL;
	int error = 0;

	if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		xpc_object_t entitlement = xpc_connection_copy_entitlement_value(connection, NESessionManagerPrivilegedEntitlement);

		if (isa_xpc_bool(entitlement) && xpc_bool_get_value(entitlement))
		{
			error = ppp_getconnectdata(&bridge->serv, &userOptions, true);
		} else {
			error = ppp_getconnectdata(&bridge->serv, &userOptions, false);
		}
        
		if (entitlement) {
			xpc_release(entitlement);
		}
	} else if (bridge->type == NESMBridgeTypeIPSec) {
		error = ipsec_getconnectdata(&bridge->serv, &userOptions, false);
	}

	if (error == 0 && userOptions != NULL) {
		if (info == NULL) {
			info = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		}
		CFDictionarySetValue(info, CFSTR(NESMSessionLegacyUserConfigurationKey), userOptions);
	}

	if (userOptions != NULL) {
		CFRelease(userOptions);
	}

	if (serviceConfig != NULL) {
		if (info == NULL) {
			info = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		}
		CFDictionarySetValue(info, CFSTR(NESMSessionLegacyServiceConfigurationKey), serviceConfig);
		CFRelease(serviceConfig);
	}

	return info;
}

static void
bridge_set_disposable_callback(ne_sm_bridge_t bridge, void (^callback)(void))
{
	if (bridge->disposable_callback != NULL) {
		Block_release(bridge->disposable_callback);
	}
	bridge->disposable_callback = Block_copy(callback);
}

static CFDictionaryRef
bridge_copy_statistics(ne_sm_bridge_t bridge)
{
	int error = 0;
	CFDictionaryRef statsdict = NULL;

	if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		error = ppp_copystatistics(&bridge->serv, &statsdict);
	} else if (bridge->type == NESMBridgeTypeIPSec) {
		error = ipsec_copystatistics(&bridge->serv, &statsdict);
	}

	if (error) {
		ne_sm_bridge_log(LOG_NOTICE, CFSTR("Failed to copy statistics: %s"), strerror(error));
	}

	return statsdict;
}

static CFDictionaryRef
bridge_copy_extended_status(ne_sm_bridge_t bridge)
{
	int error = 0;
	CFDictionaryRef statusdict = NULL;

	if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		error = ppp_copyextendedstatus(&bridge->serv, &statusdict);
	} else if (bridge->type == NESMBridgeTypeIPSec) {
		error = ipsec_copyextendedstatus(&bridge->serv, &statusdict);
	}

	if (error) {
		ne_sm_bridge_log(LOG_NOTICE, CFSTR("Failed to copy extended status: %s"), strerror(error));
	}

	return statusdict;
}

static void
bridge_install(ne_sm_bridge_t bridge)
{
	int error = 0;
	
	if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		error = ppp_install(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeIPSec) {
		error = ipsec_install(&bridge->serv);
	}
	
	if (error) {
		ne_sm_bridge_log(LOG_NOTICE, CFSTR("Failed to install: %s"), strerror(error));
	}
}

static void
bridge_uninstall(ne_sm_bridge_t bridge)
{
	int error = 0;
	
	if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		error = ppp_uninstall(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeIPSec) {
		error = ipsec_uninstall(&bridge->serv);
	}
	
	if (error) {
		ne_sm_bridge_log(LOG_NOTICE, CFSTR("Failed to uninstall: %s"), strerror(error));
	}
}

static void
bridge_handle_configuration_change(ne_sm_bridge_t bridge)
{
	int error;

	if (bridge->type == NESMBridgeTypeIPSec) {
		error = ipsec_setup_service(&bridge->serv);
		if (error) {
			ne_sm_bridge_log(LOG_ERR, CFSTR("bridge_create: ipsec_setup_service failed: %d"), error);
		}
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		error = ppp_setup_service(&bridge->serv);
		if (error) {
			ne_sm_bridge_log(LOG_ERR, CFSTR("bridge_create: ppp_setup_service failed: %d"), error);
		}
	}
}

static void
bridge_handle_user_logout(ne_sm_bridge_t bridge)
{
	if (bridge->type == NESMBridgeTypeIPSec) {
		ipsec_log_out(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		ppp_log_out(&bridge->serv);
	}
}

static void
bridge_handle_user_switch(ne_sm_bridge_t bridge)
{
	if (bridge->type == NESMBridgeTypeIPSec) {
		ipsec_log_switch(&bridge->serv);
	} else if (bridge->type == NESMBridgeTypeL2TP || bridge->type == NESMBridgeTypePPTP) {
		ppp_log_switch(&bridge->serv);
	}
}

static void
bridge_handle_device_lock(ne_sm_bridge_t bridge)
{
	if (bridge->type == NESMBridgeTypeIPSec) {
		ipsec_device_lock(&bridge->serv);
	}
}

static void
bridge_handle_device_unlock(ne_sm_bridge_t bridge)
{
	if (bridge->type == NESMBridgeTypeIPSec) {
		ipsec_device_unlock(&bridge->serv);
	}
}

extern ne_sm_bridge_functions_t
ne_sm_bridge_copy_functions(struct ne_sm_bridge_callbacks *callbacks, CFBundleRef bundle)
{
	static dispatch_once_t copy_functions_once = 0;
	static ne_sm_bridge_functions_t functions = NULL;

	dispatch_once(&copy_functions_once,
		^{
			functions = (ne_sm_bridge_functions_t)malloc(sizeof(*functions));

			memset(functions, 0, sizeof(*functions));

			functions->create = bridge_create;
			functions->destroy = bridge_destroy;
			functions->handle_network_change_event = bridge_handle_network_change_event;
			functions->handle_sleep = bridge_handle_sleep;
			functions->can_sleep = bridge_can_sleep;
			functions->handle_sleep_time = bridge_handle_sleep_time;
			functions->handle_wakeup = bridge_handle_wakeup;
			functions->handle_start = bridge_handle_start;
			functions->handle_stop = bridge_handle_stop;
			functions->get_security_session_info = bridge_get_security_session_info;
			functions->set_disposable_callback = bridge_set_disposable_callback;
			functions->copy_statistics = bridge_copy_statistics;
			functions->copy_extended_status = bridge_copy_extended_status;
			functions->copy_configuration = bridge_copy_configuration;
			functions->install = bridge_install;
			functions->uninstall = bridge_uninstall;
			functions->handle_configuration_change = bridge_handle_configuration_change;
			functions->handle_user_logout = bridge_handle_user_logout;
			functions->handle_user_switch = bridge_handle_user_switch;
			functions->handle_device_lock = bridge_handle_device_lock;
			functions->handle_device_unlock = bridge_handle_device_unlock;

			g_callbacks = (struct ne_sm_bridge_callbacks *)malloc(sizeof(*g_callbacks));
			memcpy(g_callbacks, callbacks, sizeof(*g_callbacks));
			gBundleRef = (CFBundleRef)CFRetain(bundle);
		});

	return functions;
}

