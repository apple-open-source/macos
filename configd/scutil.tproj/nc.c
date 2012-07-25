/*
 * Copyright (c) 2010-2012 Apple Inc. All rights reserved.
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
 * March 1, 2010			Christophe Allie <callie@apple.com>
 * - initial revision
 * February 8, 2011			Kevin Wells <kcw@apple.com>
 * - added "select" command
 * January 2012				Kevin Wells <kcw@apple.com>
 * - added arguments to "start" command to pass authentication credentials
 * - "show" now takes a service name as an alternative to a service ID
 * - fixes a bug whereby "IPv4" was being displayed as a subtype to IPsec services
 * - improved format of "list" output
 * - general cleanup of error messages and some variable names
 */


#include "scutil.h"
#include "nc.h"
#include "prefs.h"

#include <sys/time.h>

CFStringRef			username = NULL;
CFStringRef			password = NULL;
CFStringRef			sharedsecret = NULL;

static	SCNetworkConnectionRef	connection	= NULL;
static	int			n_callback	= 0;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
my_CFRelease(void *t)
{
	void * * obj = (void * *)t;
	if (obj && *obj) {
		CFRelease(*obj);
		*obj = NULL;
	}
	return;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
nc_get_service_type_and_subtype(SCNetworkServiceRef service, CFStringRef *iftype, CFStringRef *ifsubtype) {
	SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
	SCNetworkInterfaceRef child = SCNetworkInterfaceGetInterface(interface);

	*iftype = SCNetworkInterfaceGetInterfaceType(interface);
	*ifsubtype = NULL;
	if (CFEqual(*iftype, kSCNetworkInterfaceTypePPP) ||
	    CFEqual(*iftype, kSCNetworkInterfaceTypeVPN)) {
	    *ifsubtype = (child != NULL) ? SCNetworkInterfaceGetInterfaceType(child) : NULL;
	}
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static SCNetworkServiceRef
nc_copy_service(SCNetworkSetRef set, CFStringRef identifier)
{
	CFIndex			i;
	CFIndex			n;
	SCNetworkServiceRef	selected	= NULL;
	CFArrayRef		services;

	services = SCNetworkConnectionCopyAvailableServices(set);
	if (services == NULL) {
		goto done;
	}

	n = CFArrayGetCount(services);

	// try to select the service by its serviceID
	for (i = 0; i < n; i++) {
		SCNetworkServiceRef	service		= NULL;
		CFStringRef		serviceID;

		service = CFArrayGetValueAtIndex(services, i);
		serviceID = SCNetworkServiceGetServiceID(service);
		if (CFEqual(identifier, serviceID)) {
			selected = service;
			goto done;
		}
	}

	// try to select the service by service name
	for (i = 0; i < n; i++) {
		SCNetworkServiceRef	service		= NULL;
		CFStringRef		serviceName;

		service = CFArrayGetValueAtIndex(services, i);
		serviceName = SCNetworkServiceGetName(service);
		if ((serviceName != NULL) && CFEqual(identifier, serviceName)) {
			if (selected == NULL) {
				selected = service;
			} else {
				// if multiple services match
				selected = NULL;
				SCPrint(TRUE, stderr, CFSTR("Multiple services match\n"));
				goto done;
			}
		}
	}

done :

	if (selected != NULL) CFRetain(selected);
	if (services != NULL) CFRelease(services);
	return selected;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static SCNetworkServiceRef
nc_copy_service_from_arguments(int argc, char **argv, SCNetworkSetRef set) {
	CFStringRef		serviceID	= NULL;
	SCNetworkServiceRef	service		= NULL;

	if (argc == 0) {
		serviceID = _copyStringFromSTDIN();
	} else {
		serviceID = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
	}
	if (serviceID == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service ID specified\n"));
		return NULL;
	}
	service = nc_copy_service(set, serviceID);
	my_CFRelease(&serviceID);
	return service;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static char *
nc_status_string(SCNetworkConnectionStatus status)
{
	switch (status) {
		case kSCNetworkConnectionInvalid:
			return "Invalid";
		case kSCNetworkConnectionDisconnected:
			return "Disconnected";
		case kSCNetworkConnectionConnecting:
			return "Connecting";
		case kSCNetworkConnectionConnected:
			return "Connected";
		case kSCNetworkConnectionDisconnecting:
			return "Disconnecting";
	}
	return "Unknown";
}

static void
nc_callback(SCNetworkConnectionRef connection, SCNetworkConnectionStatus status, void *info)
{
	int		*n		= (int *)info;
	CFDictionaryRef	status_dict;

	// report status
	if (n != NULL) {
		if (*n == 0) {
			SCPrint(TRUE, stdout, CFSTR("Current status = "));
		} else {
			struct tm	tm_now;
			struct timeval	tv_now;

			(void)gettimeofday(&tv_now, NULL);
			(void)localtime_r(&tv_now.tv_sec, &tm_now);

			SCPrint(TRUE, stdout, CFSTR("\n*** %2d:%02d:%02d.%03d\n\n"),
				tm_now.tm_hour,
				tm_now.tm_min,
				tm_now.tm_sec,
				tv_now.tv_usec / 1000);
			SCPrint(TRUE, stdout, CFSTR("Callback (%d) status = "), *n);
		}
		*n = *n + 1;
	}
	SCPrint(TRUE, stdout, CFSTR("%s%s%s\n"),
		nc_status_string(status),
		(status == kSCNetworkConnectionInvalid) ? ": "                     : "",
		(status == kSCNetworkConnectionInvalid) ? SCErrorString(SCError()) : "");

	// report extended status
	status_dict = SCNetworkConnectionCopyExtendedStatus(connection);
	if (status_dict) {
		SCPrint(TRUE, stdout, CFSTR("Extended Status %@\n"), status_dict);
		CFRelease(status_dict);
	}

	return;
}

static void
nc_create_connection(int argc, char **argv, Boolean exit_on_failure)
{
	SCNetworkConnectionContext	context	= { 0, &n_callback, NULL, NULL, NULL };
	SCNetworkServiceRef		service;

	service = nc_copy_service_from_arguments(argc, argv, NULL);
	if (service == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service\n"));
		if (exit_on_failure)
			exit(1);
		return;
	}

	connection = SCNetworkConnectionCreateWithService(NULL, service, nc_callback, &context);
	CFRelease(service);
	if (connection == NULL) {
		SCPrint(TRUE, stderr, CFSTR("Could not create connection: %s\n"), SCErrorString(SCError()));
		if (exit_on_failure)
			exit(1);
		return;
	}
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_release_connection()
{
	my_CFRelease(&connection);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_start(int argc, char **argv)
{
	CFMutableDictionaryRef		userOptions = NULL;
	CFStringRef			iftype = NULL;
	CFStringRef			ifsubtype = NULL;
	SCNetworkServiceRef		service = NULL;

	nc_create_connection(argc, argv, TRUE);

	service = SCNetworkConnectionGetService(connection);
	nc_get_service_type_and_subtype(service, &iftype, &ifsubtype);

	userOptions = CFDictionaryCreateMutable(NULL, 0,
						&kCFTypeDictionaryKeyCallBacks,
						&kCFTypeDictionaryValueCallBacks);

	Boolean isL2TP = (CFEqual(iftype, kSCEntNetPPP) &&
			  (ifsubtype != NULL) && CFEqual(ifsubtype, kSCValNetInterfaceSubTypeL2TP));

	if (CFEqual(iftype, kSCEntNetPPP)) {
		CFMutableDictionaryRef pppEntity  = CFDictionaryCreateMutable(NULL, 0,
									   &kCFTypeDictionaryKeyCallBacks,
									   &kCFTypeDictionaryValueCallBacks);

		if (username != NULL) {
			CFDictionarySetValue(pppEntity, kSCPropNetPPPAuthName, username);
		}
		if (password != NULL) {
			CFDictionarySetValue(pppEntity, kSCPropNetPPPAuthPassword, password);
		}
		CFDictionarySetValue(userOptions, kSCEntNetPPP, pppEntity);
		my_CFRelease(&pppEntity);
	}
	if (CFEqual(iftype, kSCEntNetIPSec) || isL2TP) {
		CFMutableDictionaryRef ipsecEntity  = CFDictionaryCreateMutable(NULL, 0,
									   &kCFTypeDictionaryKeyCallBacks,
									   &kCFTypeDictionaryValueCallBacks);
		if (!isL2TP) {
			if (username != NULL) {
				CFDictionarySetValue(ipsecEntity, kSCPropNetIPSecXAuthName, username);
			}
			if (password != NULL) {
				CFDictionarySetValue(ipsecEntity, kSCPropNetIPSecXAuthPassword, password);
			}
		}
		if (sharedsecret != NULL) {
			CFDictionarySetValue(ipsecEntity, kSCPropNetIPSecSharedSecret, sharedsecret);
		}
		CFDictionarySetValue(userOptions, kSCEntNetIPSec, ipsecEntity);
		my_CFRelease(&ipsecEntity);
	}
	if (CFEqual(iftype, kSCEntNetVPN)) {
		CFMutableDictionaryRef vpnEntity  = CFDictionaryCreateMutable(NULL, 0,
									   &kCFTypeDictionaryKeyCallBacks,
									   &kCFTypeDictionaryValueCallBacks);
		if (username != NULL) {
			CFDictionarySetValue(vpnEntity, kSCPropNetVPNAuthName, username);
		}
		if (password != NULL) {
			CFDictionarySetValue(vpnEntity, kSCPropNetVPNAuthPassword, password);
		}
		CFDictionarySetValue(userOptions, kSCEntNetVPN, vpnEntity);
		my_CFRelease(&vpnEntity);
	}
	// If it doesn't match any VPN type, fail silently

	if (!SCNetworkConnectionStart(connection, userOptions, TRUE)) {
		SCPrint(TRUE, stderr, CFSTR("Could not start connection: %s\n"), SCErrorString(SCError()));
		exit(1);
	};

	CFRelease(userOptions);
	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_stop(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	if (!SCNetworkConnectionStop(connection, TRUE)) {
		SCPrint(TRUE, stderr, CFSTR("Could not stop connection: %s\n"), SCErrorString(SCError()));
		exit(1);
	};

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
nc_suspend(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	SCNetworkConnectionSuspend(connection);

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
nc_resume(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	SCNetworkConnectionResume(connection);

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_status(int argc, char **argv)
{
	SCNetworkConnectionStatus	status;

	nc_create_connection(argc, argv, TRUE);

	status = SCNetworkConnectionGetStatus(connection);
	nc_callback(connection, status, NULL);

	nc_release_connection();
	exit(0);
}

static void
nc_watch(int argc, char **argv)
{
	SCNetworkConnectionStatus	status;

	nc_create_connection(argc, argv, TRUE);

	status = SCNetworkConnectionGetStatus(connection);

	// report initial status
	n_callback = 0;
	nc_callback(connection, status, &n_callback);

	// setup watcher
	if (doDispatch) {
		if (!SCNetworkConnectionSetDispatchQueue(connection, dispatch_get_current_queue())) {
			SCPrint(TRUE, stderr, CFSTR("Unable to schedule watch process: %s\n"), SCErrorString(SCError()));
			exit(1);
		}
	} else {
		if (!SCNetworkConnectionScheduleWithRunLoop(connection, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
			SCPrint(TRUE, stderr, CFSTR("Unable to schedule watch process: %s\n"), SCErrorString(SCError()));
			exit(1);
		}
	}

	// wait for changes
	CFRunLoopRun();

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_statistics(int argc, char **argv)
{
	CFDictionaryRef stats_dict;

	nc_create_connection(argc, argv, TRUE);

	stats_dict = SCNetworkConnectionCopyStatistics(connection);

	if (stats_dict) {
		SCPrint(TRUE, stdout, CFSTR("%@\n"), stats_dict);
	} else {
		SCPrint(TRUE, stdout, CFSTR("No statistics available\n"));
	}

	my_CFRelease(&stats_dict);

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
checkOnDemandHost(SCDynamicStoreRef store, CFStringRef nodeName, Boolean retry)
{
	Boolean				ok;
	CFStringRef			connectionServiceID	= NULL;
	SCNetworkConnectionStatus	connectionStatus	= 0;
	CFStringRef			vpnRemoteAddress	= NULL;

	SCPrint(TRUE, stdout, CFSTR("OnDemand host/domain check (%sretry)\n"), retry ? "" : "no ");

	ok = __SCNetworkConnectionCopyOnDemandInfoWithName(&store,
							   nodeName,
							   retry,
							   &connectionServiceID,
							   &connectionStatus,
							   &vpnRemoteAddress);

	if (ok) {
		SCPrint(TRUE, stdout, CFSTR("  serviceID      = %@\n"), connectionServiceID);
		SCPrint(TRUE, stdout, CFSTR("  remote address = %@\n"), vpnRemoteAddress);
	} else if (SCError() != kSCStatusOK) {
		SCPrint(TRUE, stdout, CFSTR("%sretry\n"), retry ? "" : "no ");
		SCPrint(TRUE, stdout,
			CFSTR("  Unable to copy OnDemand information for connection: %s\n"),
			SCErrorString(SCError()));
	} else {
		SCPrint(TRUE, stdout, CFSTR("  no match\n"));
	}

	if (connectionServiceID != NULL) {
		CFRelease(connectionServiceID);
		connectionServiceID = NULL;
	}
	if (vpnRemoteAddress != NULL) {
		CFRelease(vpnRemoteAddress);
		vpnRemoteAddress = NULL;
	}

	return;
}

static void
nc_ondemand(int argc, char **argv)
{
	int			exit_code	= 1;
	CFStringRef		key		= NULL;
	CFDictionaryRef		ondemand_dict	= NULL;
	SCDynamicStoreRef	store;

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil --nc"), NULL, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr, CFSTR("Unable to create dynamic store: %s\n"), SCErrorString(SCError()));
		goto done;
	}

	if (argc > 0) {
		CFStringRef	nodeName;

		nodeName = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
		checkOnDemandHost(store, nodeName, FALSE);
		checkOnDemandHost(store, nodeName, TRUE);
		goto done;
	}

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetOnDemand);

	ondemand_dict = SCDynamicStoreCopyValue(store, key);
	if (ondemand_dict) {
		SCPrint(TRUE, stdout, CFSTR("%@ %@\n"), kSCEntNetOnDemand, ondemand_dict);
	} else {
		SCPrint(TRUE, stdout, CFSTR("%@ not configured\n"), kSCEntNetOnDemand);
	}

	exit_code = 0;
done:
	my_CFRelease(&ondemand_dict);
	my_CFRelease(&key);
	my_CFRelease(&store);
	exit(exit_code);
}


/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

CFStringRef
copy_padded_string(CFStringRef original, int width)
{
	CFMutableStringRef	padded;

	padded = CFStringCreateMutableCopy(NULL, 0, original);
	CFStringPad(padded, CFSTR(" "), MAX(CFStringGetLength(original), width), 0);
	return padded;
}


static void
nc_print_VPN_service(SCNetworkServiceRef service)
{
	CFStringRef type = NULL;
	CFStringRef sub_type = NULL;

	nc_get_service_type_and_subtype(service, &type, &sub_type);

	CFStringRef service_name = SCNetworkServiceGetName(service);
	if (service_name == NULL)
		service_name = CFSTR("");
	CFStringRef service_name_quoted = CFStringCreateWithFormat(NULL, NULL, CFSTR("\"%@\""), service_name);
	if (service_name_quoted == NULL) {
		service_name_quoted = CFRetain(CFSTR(""));
	}
	CFStringRef service_name_padded = copy_padded_string(service_name, 30);

	CFStringRef service_id   = SCNetworkServiceGetServiceID(service);
	SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
	CFStringRef display_name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
	if (display_name == NULL)
		display_name = CFSTR("");
	CFStringRef display_name_padded = copy_padded_string(display_name, 18);


	SCPrint(TRUE,
		stdout,
		CFSTR("%@  %@ %@ %@ [%@%@%@]\n"),
		SCNetworkServiceGetEnabled(service) ? CFSTR("*") : CFSTR(" "),
		service_id,
		display_name_padded,
		service_name_padded,
		type,
		(sub_type == NULL) ? CFSTR("") : CFSTR(":"),
		(sub_type == NULL) ? CFSTR("") : sub_type);
	CFRelease(service_name_quoted);
	CFRelease(display_name_padded);
	CFRelease(service_name_padded);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_list(int argc, char **argv)
{
	int			count;
	int			i;
	CFArrayRef		services	= NULL;

	SCPrint(TRUE, stdout, CFSTR("Available network connection services in the current set (*=enabled):\n"));
	services = SCNetworkConnectionCopyAvailableServices(NULL);
	if (services != NULL) {
		count = CFArrayGetCount(services);

		for (i = 0; i < count; i++) {
			SCNetworkServiceRef	service;

			service = CFArrayGetValueAtIndex(services, i);
			nc_print_VPN_service(service);
		}

	}
	my_CFRelease(&services);
	exit(0);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_show(int argc, char **argv)
{
	SCNetworkServiceRef	service = NULL;
	SCDynamicStoreRef	store = NULL;
	int			exit_code = 1;
	CFStringRef		serviceID = NULL;
	CFStringRef		iftype = NULL;
	CFStringRef		ifsubtype = NULL;
	CFStringRef		type_entity_key = NULL;
	CFStringRef		subtype_entity_key = NULL;
	CFDictionaryRef		type_entity_dict = NULL;
	CFDictionaryRef		subtype_entity_dict = NULL;

	service = nc_copy_service_from_arguments(argc, argv, NULL);
	if (service == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service\n"));
		exit(exit_code);
	}

	serviceID = SCNetworkServiceGetServiceID(service);

	nc_get_service_type_and_subtype(service, &iftype, &ifsubtype);

	if (!CFEqual(iftype, kSCEntNetPPP) &&
	    !CFEqual(iftype, kSCEntNetIPSec) &&
	    !CFEqual(iftype, kSCEntNetVPN)) {
		SCPrint(TRUE, stderr, CFSTR("Not a connection oriented service: %@\n"), serviceID);
		goto done;
	}

	type_entity_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceID, iftype);

	nc_print_VPN_service(service);

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil --nc"), NULL, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr, CFSTR("Unable to create dynamic store: %s\n"), SCErrorString(SCError()));
		goto done;
	}
	type_entity_dict = SCDynamicStoreCopyValue(store, type_entity_key);

	if (!type_entity_dict) {
		SCPrint(TRUE, stderr, CFSTR("No \"%@\" configuration available\n"), iftype);
	} else {
		SCPrint(TRUE, stdout, CFSTR("%@ %@\n"), iftype, type_entity_dict);
	}

	if (ifsubtype) {
		subtype_entity_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceID, ifsubtype);
		subtype_entity_dict = SCDynamicStoreCopyValue(store, subtype_entity_key);
		if (!subtype_entity_dict) {
			//
		}
		else {
			SCPrint(TRUE, stdout, CFSTR("%@ %@\n"), ifsubtype, subtype_entity_dict);
		}
	}

	exit_code = 0;

done:
	my_CFRelease(&type_entity_key);
	my_CFRelease(&type_entity_dict);
	my_CFRelease(&subtype_entity_key);
	my_CFRelease(&subtype_entity_dict);
	my_CFRelease(&store);
	my_CFRelease(&service);
	exit(exit_code);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
nc_select(int argc, char **argv)
{
	SCNetworkSetRef		current_set;
	int			exit_code	= 1;
	SCNetworkServiceRef	service		= NULL;
	Boolean			status;

	do_prefs_init();	/* initialization */
	do_prefs_open(0, NULL);	/* open default prefs */

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No current location\n"), SCErrorString(SCError()));
		goto done;
	}

	service = nc_copy_service_from_arguments(argc, argv, current_set);
	if (service == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service\n"));
		goto done;
	}

#if !TARGET_OS_IPHONE
	status = SCNetworkServiceSetEnabled(service, TRUE);
	if (!status) {
		SCPrint(TRUE, stderr, CFSTR("Unable to enable service: %s\n"), SCErrorString(SCError()));
		goto done;
	}
#else
	status = SCNetworkSetSetSelectedVPNService(current_set, service);
	if (!status) {
		SCPrint(TRUE, stderr, CFSTR("Unable to select service: %s\n"), SCErrorString(SCError()));
		goto done;
	}
#endif

	_prefs_save();
	exit_code = 0;
done:
	my_CFRelease(&service);
	my_CFRelease(&current_set);
	_prefs_close();
	exit(exit_code);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
typedef void (*nc_func) (int argc, char **argv);

static const struct {
	char		*cmd;
	nc_func		func;
} nc_cmds[] = {
	{ "list",		nc_list		},
	{ "ondemand",		nc_ondemand	},
	{ "resume",		nc_resume	},
	{ "select",		nc_select	},
	{ "show",		nc_show		},
	{ "start",		nc_start	},
	{ "statistics",		nc_statistics	},
	{ "status",		nc_status	},
	{ "stop",		nc_stop		},
	{ "suspend",		nc_suspend	},
};
#define	N_NC_CMNDS	(sizeof(nc_cmds) / sizeof(nc_cmds[0]))


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
find_nc_cmd(char *cmd)
{
	int	i;

	for (i = 0; i < (int)N_NC_CMNDS; i++) {
		if (strcmp(cmd, nc_cmds[i].cmd) == 0) {
			return i;
		}
	}

	return -1;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void
do_nc_cmd(char *cmd, int argc, char **argv, Boolean watch)
{
	int	i;

	i = find_nc_cmd(cmd);
	if (i >= 0) {
		nc_func	func;

		func = nc_cmds[i].func;
		if (watch && (func == nc_status)) {
			func = nc_watch;
		}
		(*func)(argc, argv);
	}
	return;
}

