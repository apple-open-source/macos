/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
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
 */


#include "scutil.h"
#include "nc.h"
#include "prefs.h"

#include <sys/time.h>


static	SCNetworkConnectionRef	connectionRef	= NULL;
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
static CFStringRef
nc_copy_serviceID(int argc, char **argv)
{
	CFStringRef		serviceIDRef	= NULL;

	if (argc == 0) {
		serviceIDRef = _copyStringFromSTDIN();
	} else {
		serviceIDRef = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
	}

	return serviceIDRef;
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
				SCPrint(TRUE, stdout, CFSTR("multiple services match\n"));
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
	CFStringRef			serviceIDRef;

	serviceIDRef = nc_copy_serviceID(argc, argv);
	if (serviceIDRef == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service identifier\n"));
		if (exit_on_failure)
			exit(1);
		return;
	}

	service = nc_copy_service(NULL, serviceIDRef);
	CFRelease(serviceIDRef);
	if (service == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service\n"));
		if (exit_on_failure)
			exit(1);
		return;
	}

	connectionRef = SCNetworkConnectionCreateWithService(NULL, service, nc_callback, &context);
	if (connectionRef == NULL) {
		SCPrint(TRUE, stderr, CFSTR("nc_create_connection SCNetworkConnectionCreateWithServiceID() failed to create connectionRef: %s\n"), SCErrorString(SCError()));
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
	my_CFRelease(&connectionRef);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_start(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	SCNetworkConnectionStart(connectionRef, 0, TRUE);

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_stop(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	SCNetworkConnectionStop(connectionRef, TRUE);

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
nc_suspend(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	SCNetworkConnectionSuspend(connectionRef);

	nc_release_connection();
	exit(0);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static void
nc_resume(int argc, char **argv)
{
	nc_create_connection(argc, argv, TRUE);

	SCNetworkConnectionResume(connectionRef);

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

	status = SCNetworkConnectionGetStatus(connectionRef);
	nc_callback(connectionRef, status, NULL);

	nc_release_connection();
	exit(0);
}

static void
nc_watch(int argc, char **argv)
{
	SCNetworkConnectionStatus	status;

	nc_create_connection(argc, argv, TRUE);

	status = SCNetworkConnectionGetStatus(connectionRef);

	// report initial status
	n_callback = 0;
	nc_callback(connectionRef, status, &n_callback);

	// setup watcher
	if (doDispatch) {
		if (!SCNetworkConnectionSetDispatchQueue(connectionRef, dispatch_get_current_queue())) {
			printf("SCNetworkConnectionSetDispatchQueue() failed: %s\n", SCErrorString(SCError()));
			exit(1);
		}
	} else {
		if (!SCNetworkConnectionScheduleWithRunLoop(connectionRef, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
			printf("SCNetworkConnectinScheduleWithRunLoop() failed: %s\n", SCErrorString(SCError()));
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

	stats_dict = SCNetworkConnectionCopyStatistics(connectionRef);

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
nc_ondemand(int argc, char **argv)
{
	int			exit_code	= 1;
	CFStringRef		key		= NULL;
	CFDictionaryRef		ondemand_dict	= NULL;
	SCDynamicStoreRef	store;

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil --nc"), NULL, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr, CFSTR("do_nc_ondemand SCDynamicStoreCreate() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetOnDemand);
	if (key == NULL) {
		SCPrint(TRUE, stderr, CFSTR("do_nc_ondemand SCDynamicStoreKeyCreateNetworkGlobalEntity() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}

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
 Given a string 'key' and a string prefix 'prefix',
 return the next component in the slash '/' separated
 key.  If no slash follows the prefix, return NULL.

 Examples:
 1. key = "a/b/c" prefix = "a/"    returns "b"
 2. key = "a/b/c" prefix = "a/b/"  returns NULL
----------------------------------------------------------------------------- */
CFStringRef parse_component(CFStringRef key, CFStringRef prefix)
{
	CFMutableStringRef	comp;
	CFRange			range;

	if (!CFStringHasPrefix(key, prefix))
		return NULL;

	comp = CFStringCreateMutableCopy(NULL, 0, key);
	CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
	range = CFStringFind(comp, CFSTR("/"), 0);
	if (range.location == kCFNotFound) {
		CFRelease(comp);
		return NULL;
	}
	range.length = CFStringGetLength(comp) - range.location;
	CFStringDelete(comp, range);
	return comp;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_list(int argc, char **argv)
{
	int			count;
	int			exit_code	= 1;
	int			i;
	CFStringRef		key		= NULL;
	CFMutableDictionaryRef	names		= NULL;
	CFArrayRef		services	= NULL;
	CFStringRef		setup		= NULL;
	SCDynamicStoreRef	store;

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil --nc"), NULL, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr, CFSTR("nc_list SCDynamicStoreCreate() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainSetup, kSCCompAnyRegex, kSCEntNetInterface);
	if (key == NULL ) {
		SCPrint(TRUE, stderr, CFSTR("nc_list SCDynamicStoreKeyCreateNetworkServiceEntity() failed to create key string\n"));
		goto done;
	}
	setup = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/"), kSCDynamicStoreDomainSetup, kSCCompNetwork, kSCCompService);
	if (setup == NULL) {
		SCPrint(TRUE, stderr, CFSTR("nc_list SCDynamicStoreKeyCreate() failed to create setup string\n"));
		goto done;
	}
	names = CFDictionaryCreateMutable(NULL,
					  0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	if (names == NULL) {
		SCPrint(TRUE, stderr, CFSTR("nc_list CFDictionaryCreateMutable() failed to create names dictionary\n"));
		goto done;
	}
	services = SCNetworkConnectionCopyAvailableServices(NULL);
	if (services != NULL) {
		count = CFArrayGetCount(services);

		for (i = 0; i < count; i++) {
			SCNetworkServiceRef	service;
			CFStringRef		serviceID;
			CFStringRef		serviceName;

			service = CFArrayGetValueAtIndex(services, i);
			serviceID = SCNetworkServiceGetServiceID(service);
			serviceName = SCNetworkServiceGetName(service);
			if (serviceName != NULL) {
				CFDictionarySetValue(names, serviceID, serviceName);
			}
		}

		CFRelease(services);
	}

	services = SCDynamicStoreCopyKeyList(store, key);
	if (services == NULL ) {
		SCPrint(TRUE, stderr, CFSTR("nc_list SCDynamicStoreCopyKeyList() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}

	count = CFArrayGetCount(services);
	for (i = 0; i < count; i++) {
		CFStringRef serviceID;

		serviceID = parse_component(CFArrayGetValueAtIndex(services, i), setup);
		if (serviceID) {
			CFStringRef	iftype;
			CFStringRef	ifsubtype;
			CFStringRef	interface_key	= NULL;
			CFDictionaryRef	interface_dict	= NULL;
			CFStringRef	service_name;

			interface_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceID, kSCEntNetInterface);
			if (!interface_key)  {
				SCPrint(TRUE, stderr, CFSTR("nc_list SCDynamicStoreKeyCreateNetworkServiceEntity() failed to interface key string\n"));
				goto endloop;
			}

			interface_dict = SCDynamicStoreCopyValue(store, interface_key);
			if (!interface_dict) {
				SCPrint(TRUE, stderr, CFSTR("nc_list SCDynamicStoreCopyValue() to copy interface dictionary: %s\n"), SCErrorString(SCError()));
				goto endloop;
			}

			iftype = CFDictionaryGetValue(interface_dict, kSCPropNetInterfaceType);
			if (!iftype) {
				// is that an error condition ???
				goto endloop;
			}

			if (!CFEqual(iftype, kSCEntNetPPP) &&
				!CFEqual(iftype, kSCEntNetIPSec) &&
				!CFEqual(iftype, kSCEntNetVPN))
				goto endloop;

			ifsubtype = CFDictionaryGetValue(interface_dict, kSCPropNetInterfaceSubType);

			service_name = CFDictionaryGetValue(names, serviceID);

			SCPrint(TRUE, stdout, CFSTR("[%@%@%@] %@%s%@\n"),
				iftype ? iftype : CFSTR("?"),
				ifsubtype ? CFSTR("/") : CFSTR(""),
				ifsubtype ? ifsubtype : CFSTR(""),
				serviceID,
				service_name ? " : " : "",
				service_name ? service_name : CFSTR(""));

		    endloop:
			my_CFRelease(&interface_key);
			my_CFRelease(&interface_dict);
			my_CFRelease(&serviceID);
		}
	}

	exit_code = 0;
done:
	my_CFRelease(&services);
	my_CFRelease(&names);
	my_CFRelease(&setup);
	my_CFRelease(&key);
	my_CFRelease(&store);
	exit(exit_code);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
nc_show(int argc, char **argv)
{
	SCDynamicStoreRef	store = NULL;
	int			exit_code = 1;
	CFStringRef		setup = NULL;
	CFStringRef		serviceIDRef = NULL;
	CFArrayRef		services = NULL;
	CFStringRef		iftype = NULL;
	CFStringRef		ifsubtype = NULL;
	CFStringRef		interface_key = NULL;
	CFDictionaryRef		interface_dict = NULL;
	CFStringRef		type_entity_key = NULL;
	CFStringRef		subtype_entity_key = NULL;
	CFDictionaryRef		type_entity_dict = NULL;
	CFDictionaryRef		subtype_entity_dict = NULL;

	serviceIDRef = nc_copy_serviceID(argc, argv);
	if (serviceIDRef == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service ID\n"));
		goto done;
	}

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil --nc"), NULL, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr, CFSTR("nc_show SCDynamicStoreCreate() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}

	interface_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceIDRef, kSCEntNetInterface);
	if (!interface_key) {
		SCPrint(TRUE, stderr, CFSTR("nc_show SCDynamicStoreKeyCreateNetworkServiceEntity() failed to create interface key\n"));
		goto done;
	}

	interface_dict = SCDynamicStoreCopyValue(store, interface_key);
	if (!interface_dict) {
		SCPrint(TRUE, stdout, CFSTR("Interface dictionary missing for service ID : %@\n"), serviceIDRef);
		goto done;
	}

	iftype = CFDictionaryGetValue(interface_dict, kSCPropNetInterfaceType);
	if (!iftype) {
		SCPrint(TRUE, stdout, CFSTR("Interface Type missing for service ID : %@\n"), serviceIDRef);
		goto done;
	}

	if (!CFEqual(iftype, kSCEntNetPPP) &&
		!CFEqual(iftype, kSCEntNetIPSec) &&
		!CFEqual(iftype, kSCEntNetVPN)) {
		SCPrint(TRUE, stdout, CFSTR("Interface Type [%@] invalid for service ID : %@\n"), iftype, serviceIDRef);
		goto done;
	}

	ifsubtype = CFDictionaryGetValue(interface_dict, kSCPropNetInterfaceSubType);
	SCPrint(TRUE, stdout, CFSTR("[%@%@%@] %@\n"),
		iftype ? iftype : CFSTR("?"),
		ifsubtype ? CFSTR("/") : CFSTR(""),
		ifsubtype ? ifsubtype : CFSTR(""),
		serviceIDRef);

	type_entity_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceIDRef, iftype);
	if (!type_entity_key) {
		SCPrint(TRUE, stderr, CFSTR("nc_show SCDynamicStoreKeyCreateNetworkServiceEntity() failed to create type entity key\n"));
		goto done;
	}
	type_entity_dict = SCDynamicStoreCopyValue(store, type_entity_key);
	if (!type_entity_dict) {
		SCPrint(TRUE, stdout, CFSTR("%@ dictionary missing for service ID : %@\n"), iftype, serviceIDRef);
	} else {
		SCPrint(TRUE, stdout, CFSTR("%@ %@\n"), iftype, type_entity_dict);
	}

	if (ifsubtype) {
		subtype_entity_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceIDRef, ifsubtype);
		if (!subtype_entity_key) {
			SCPrint(TRUE, stderr, CFSTR("nc_show SCDynamicStoreKeyCreateNetworkServiceEntity() failed to create subtype entity key\n"));
			goto done;
		}
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
	my_CFRelease(&serviceIDRef);
	my_CFRelease(&interface_key);
	my_CFRelease(&interface_dict);
	my_CFRelease(&type_entity_key);
	my_CFRelease(&type_entity_dict);
	my_CFRelease(&subtype_entity_key);
	my_CFRelease(&subtype_entity_dict);
	my_CFRelease(&services);
	my_CFRelease(&setup);
	my_CFRelease(&store);

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
	CFStringRef		service_id;
	Boolean			status;

	service_id = nc_copy_serviceID(argc, argv);
	if (service_id == NULL) {
		SCPrint(TRUE, stderr, CFSTR("No service identifier\n"));
		exit(exit_code);
	}

	do_prefs_init();	/* initialization */
	do_prefs_open(0, NULL);	/* open default prefs */

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		SCPrint(TRUE, stdout, CFSTR("nc_select SCNetworkSetCopyCurrent() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}

	service = nc_copy_service(current_set, service_id);
	if (service == NULL) {
		SCPrint(TRUE, stdout, CFSTR("No service\n"));
		goto done;
	}

#if !TARGET_OS_IPHONE
	status = SCNetworkServiceSetEnabled(service, TRUE);
	if (!status) {
		SCPrint(TRUE, stdout, CFSTR("nc_select SCNetworkServiceSetEnabled() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}
#else
	status = SCNetworkSetSetSelectedVPNService(current_set, service);
	if (!status) {
		SCPrint(TRUE, stdout, CFSTR("nc_select SCNetworkSetSetSelectedVPNService() failed: %s\n"), SCErrorString(SCError()));
		goto done;
	}
#endif

	_prefs_save();
	exit_code = 0;
done:

	my_CFRelease(&service_id);
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

