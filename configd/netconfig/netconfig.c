/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * November 11, 2020		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */

/*
 * netconfig
 * - a command-line tool to view and set network configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/errno.h>
#include <sysexits.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dispatch/dispatch.h>
#include <SystemConfiguration/SystemConfiguration.h>
#define __SC_CFRELEASE_NEEDED	1
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCNetworkCategory.h>
#include <SystemConfiguration/SCNetworkSettingsManager.h>

static void command_specific_help(void) __dead2;

static void
help_create_vlan(void) __dead2;

static void
help_create_bridge(void) __dead2;

#define countof(array)	(sizeof(array) / sizeof(array[0]))

#define kOptActiveState		"active-state"
#define OPT_ACTIVE_STATE	0

#define kOptAutoConfigure	"auto-configure"
#define OPT_AUTO_CONFIGURE	'a'

#define kOptAddress		"address"
#define OPT_ADDRESS		'A'

#define kOptCategory		"category"
#define OPT_CATEGORY		0

#define kOptCategoryValue	"category-value"
#define OPT_CATEGORY_VALUE	0

#define kOptConfigMethod	"config-method"
#define OPT_CONFIG_METHOD	'c'

#define kOptDHCPClientID	"dhcp-client-id"
#define OPT_DHCP_CLIENT_ID	'C'

#define kOptDNSDomainName	"dns-domain-name"
#define OPT_DNS_DOMAIN_NAME	'n'

#define kOptDNSSearchDomains	"dns-search-domains"
#define OPT_DNS_SEARCH_DOMAINS	's'

#define kOptDefaultConfig	"default-config"
#define OPT_DEFAULT_CONFIG	'D'

#define kOptFile		"file"
#define OPT_FILE		'f'

#define kOptHelp		"help"
#define OPT_HELP		'h'

#define kOptInterface		"interface"
#define OPT_INTERFACE		'i'

#define kOptBridgeMember	"bridge-member"
#define OPT_BRIDGE_MEMBER	'b'

#define kOptInterfaceType	"interface-type"
#define OPT_INTERFACE_TYPE	't'

#define kOptRouter		"router"
#define OPT_ROUTER		'r'

#define kOptSubnetMask		"subnet-mask"
#define OPT_SUBNET_MASK		'm'

#define kOptName		"name"
#define OPT_NAME		'N'

#define kOptProtocol		"protocol"
#define OPT_PROTOCOL		'p'

#define kOptQoSMarkingEnable	"qos-marking-enable"
#define OPT_QOS_MARKING_ENABLE	0

#define kOptQoSMarkingAppleAV	"qos-marking-apple-av"
#define OPT_QOS_MARKING_APPLE_AV	0

#define kOptQoSMarkingAppID	"qos-marking-app-id"
#define OPT_QOS_MARKING_APP_ID	0

#define kOptService		"service"
#define OPT_SERVICE		'S'

#define kOptSet			"set"
#define OPT_SET			'e'

#define kOptSSID		"ssid"
#define OPT_SSID		0

#define kOptUseSettingsManager	"use-settings-manager"
#define OPT_USE_SETTINGS_MANAGER 0

#define kOptVerbose		"verbose"
#define OPT_VERBOSE		'v'

#define kOptVLANID		"vlan-id"
#define OPT_VLAN_ID		'I'

#define kOptVLANDevice		"vlan-device"
#define OPT_VLAN_DEVICE		'P'

static struct option longopts[] = {
	{ kOptActiveState,	no_argument,	   NULL, OPT_ACTIVE_STATE },
	{ kOptAddress,		required_argument, NULL, OPT_ADDRESS },
	{ kOptCategory,		required_argument, NULL, OPT_CATEGORY },
	{ kOptCategoryValue,	required_argument, NULL, OPT_CATEGORY_VALUE },
	{ kOptConfigMethod,	required_argument, NULL, OPT_CONFIG_METHOD },
	{ kOptDHCPClientID, 	required_argument, NULL, OPT_DHCP_CLIENT_ID },
	{ kOptDNSDomainName,	required_argument, NULL, OPT_DNS_DOMAIN_NAME },
	{ kOptDNSSearchDomains,	required_argument, NULL, OPT_DNS_SEARCH_DOMAINS},
	{ kOptDefaultConfig,	no_argument,       NULL, OPT_DEFAULT_CONFIG },
	{ kOptFile,		required_argument, NULL, OPT_FILE },
	{ kOptInterface,	required_argument, NULL, OPT_INTERFACE },
	{ kOptHelp,		no_argument, 	   NULL, OPT_HELP },
	{ kOptInterfaceType,	required_argument, NULL, OPT_INTERFACE_TYPE },
	{ kOptName,		required_argument, NULL, OPT_NAME },
	{ kOptRouter,		required_argument, NULL, OPT_ROUTER },
	{ kOptSubnetMask,	required_argument, NULL, OPT_SUBNET_MASK },
	{ kOptProtocol,		required_argument, NULL, OPT_PROTOCOL },
	{ kOptQoSMarkingEnable,	required_argument, NULL,
	  OPT_QOS_MARKING_ENABLE },
	{ kOptQoSMarkingAppleAV,required_argument, NULL,
	  OPT_QOS_MARKING_APPLE_AV },
	{ kOptQoSMarkingAppID,	required_argument, NULL,
	  OPT_QOS_MARKING_APP_ID },
	{ kOptSet,		required_argument, NULL, OPT_SET},
	{ kOptService,		required_argument, NULL, OPT_SERVICE },
	{ kOptSSID,		required_argument, NULL, OPT_SSID },
	{ kOptUseSettingsManager,no_argument,	   NULL,
	  OPT_USE_SETTINGS_MANAGER },
	{ kOptVerbose,		no_argument, 	   NULL, OPT_VERBOSE },
	{ kOptVLANID,		required_argument, NULL, OPT_VLAN_ID },
	{ kOptVLANDevice,	required_argument, NULL, OPT_VLAN_DEVICE },
	{ NULL,			0,		   NULL, 0 }
};

static const char * 	G_argv0;

static SCPreferencesRef
prefs_create(void);

static SCNetworkInterfaceRef
copy_available_interface(CFStringRef name_cf, const char * name);

/*
 * Utility functions
 */
static Boolean
array_contains_value(CFArrayRef array, CFTypeRef val)
{
	CFRange	r = CFRangeMake(0, CFArrayGetCount(array));
	return (CFArrayContainsValue(array, r, val));
}

static CFMutableArrayRef
array_create(void)
{
	return (CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
}

static CFMutableDictionaryRef
dict_create(void)
{
	return (CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks));
}

static void
dict_set_val(CFMutableDictionaryRef dict, CFStringRef key, CFTypeRef val)
{
	if (val == NULL) {
		return;
	}
	CFDictionarySetValue(dict, key, val);
}

static CFStringRef
my_CFStringCreate(const char * str)
{
	return CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
}

static CFStringRef
my_CFStringCreateWithIPAddress(uint8_t af, const void * addr)
{
	char 		ntopbuf[INET6_ADDRSTRLEN];
	const char *	c_str;

	c_str = inet_ntop(af, addr, ntopbuf, sizeof(ntopbuf));
	return (CFStringCreateWithCString(NULL, c_str, kCFStringEncodingUTF8));
}

static void
my_CFDictionarySetCString(CFMutableDictionaryRef dict,
			  CFStringRef prop, const char * str)
{
	CFStringRef	cfstr;

	cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
	CFDictionarySetValue(dict, prop, cfstr);
	CFRelease(cfstr);
	return;
}

static void
my_CFDictionarySetTypeAsArrayValue(CFMutableDictionaryRef dict,
				   CFStringRef prop, CFTypeRef val)
{
	CFArrayRef	array;

	array = CFArrayCreate(NULL, (const void **)&val, 1,
			      &kCFTypeArrayCallBacks);
	if (array != NULL) {
		CFDictionarySetValue(dict, prop, array);
		CFRelease(array);
	}
	return;
}

static void
my_CFDictionarySetIPAddressAsArrayValue(CFMutableDictionaryRef dict,
					CFStringRef prop,
					uint8_t af,
					const void * addr)
{
	CFStringRef		str;

	str = my_CFStringCreateWithIPAddress(af, addr);
	my_CFDictionarySetTypeAsArrayValue(dict, prop, str);
	CFRelease(str);
	return;
}

static void
my_CFDictionarySetNumberAsArrayValue(CFMutableDictionaryRef dict,
				     CFStringRef prop,
				     int val)
{
	CFNumberRef		num;

	num = CFNumberCreate(NULL, kCFNumberIntType, &val);
	my_CFDictionarySetTypeAsArrayValue(dict, prop, num);
	CFRelease(num);
	return;
}

static void
my_CFDictionarySetIPAddress(CFMutableDictionaryRef dict,
			    CFStringRef prop,
			    uint8_t af,
			    const void * addr)
{
	CFStringRef		str;

	str = my_CFStringCreateWithIPAddress(af, addr);
	CFDictionarySetValue(dict, prop, str);
	CFRelease(str);
	return;
}

static Boolean
file_exists(const char * filename)
{
	struct stat	b;

	if (stat(filename, &b) != 0) {
		fprintf(stderr, "stat(%s) failed, %s\n",
			filename, strerror(errno));
		return (FALSE);
	}
	if ((b.st_mode & S_IFREG) == 0) {
		fprintf(stderr, "%s: not a file\n", filename);
		return (FALSE);
	}
	return (TRUE);
}

static Boolean
get_bool_from_string(const char * str, Boolean * ret_val_p,
		     char opt, const char * opt_string)
{
	Boolean		success = FALSE;

	if (str == NULL) {
		return (FALSE);
	}
	if (strcasecmp(str, "true") == 0
	    || strcasecmp(str, "yes") == 0
	    || strcmp(str, "1") == 0) {
		*ret_val_p = TRUE;
		success = TRUE;
	}
	else if (strcasecmp(str, "false") == 0
		 || strcasecmp(str, "no") == 0
		 || strcmp(str, "0") == 0) {
		*ret_val_p = FALSE;
		success = TRUE;
	}
	else if (opt != 0) {
		fprintf(stderr,
			"invalid value for %-c/--%s, must be "
			"( true | yes | 1 ) or ( false | no | 0 )\n",
			opt, opt_string);
	}
	else {
		fprintf(stderr,
			"invalid value for --%s, must be "
			"( true | yes | 1 ) or ( false | no | 0 )\n",
			opt_string);
	}
	return (success);
}

static Boolean
arg_present(const char * optname, const char * optarg)
{
	Boolean 	ok = TRUE;

	if (optarg == NULL || optarg[0] == '-') {
		fprintf(stderr, "--%s requires an argument\n", optname);
		ok = FALSE;
	}
	return (ok);
}

/*
 * categoryOptions
 */
typedef struct {
	Boolean			have_category;
	Boolean			have_category_value;
	Boolean			have_ssid;
	CFStringRef		category_id;
	CFStringRef		category_value;
} categoryOptions, *categoryOptionsRef;

static Boolean
categoryOptionsAdd(categoryOptionsRef opt,
		   const char * optname, const char * optarg,
		   Boolean * ret_success)
{
	Boolean		handled = FALSE;
	Boolean		success = FALSE;

	if (strcmp(optname, kOptSSID) == 0) {
		handled = TRUE;
		if (!arg_present(optname, optarg)) {
			goto done;
		}
		if (opt->have_ssid) {
			fprintf(stderr, "--%s specified multiple times\n",
				kOptSSID);
			goto done;
		}
		opt->have_ssid = TRUE;
		if (opt->have_category) {
			fprintf(stderr,	"--%s can't be specified with --%s\n",
				kOptSSID, kOptCategory);
			goto done;
		}
		if (opt->have_category_value) {
			fprintf(stderr, "--%s can't be specified with --%s\n",
				kOptSSID, kOptCategoryValue);
			goto done;
		}
		opt->category_id = CFRetain(kSCNetworkCategoryWiFiSSID);
		opt->category_value = my_CFStringCreate(optarg);
		success = TRUE;
	}
	else if (strcmp(optname, kOptCategory) == 0) {
		handled = TRUE;
		if (!arg_present(optname, optarg)) {
			goto done;
		}
		if (opt->have_ssid) {
			fprintf(stderr,	"--%s can't be specified with --%s\n",
				kOptCategory, kOptSSID);
			goto done;
		}
		if (opt->have_category) {
			fprintf(stderr,	"--%s specified multiple times\n",
				kOptCategory);
			goto done;
		}
		opt->category_id = my_CFStringCreate(optarg);
		success = TRUE;
	}
	else if (strcmp(optname, kOptCategoryValue) == 0) {
		handled = TRUE;
		if (!arg_present(optname, optarg)) {
			goto done;
		}
		if (opt->have_ssid) {
			fprintf(stderr,	"--%s can't be specified with --%s\n",
				kOptCategoryValue, kOptSSID);
			goto done;
		}
		if (opt->have_category_value) {
			fprintf(stderr,	"--%s specified multiple times\n",
				kOptCategoryValue);
			goto done;
		}
		opt->category_value = my_CFStringCreate(optarg);
		success = TRUE;
	}
 done:
	*ret_success = success;
	return (handled);
}

static Boolean
categoryOptionsAreValid(categoryOptionsRef opt)
{
	Boolean		valid;

	valid = (opt->category_id == NULL && opt->category_value == NULL)
		|| (opt->category_id != NULL && opt->category_value != NULL);
	if (!valid) {
		fprintf(stderr,
			"--%s and --%s must both be specified\n",
			kOptCategory, kOptCategoryValue);
	}
	return (valid);
}

static void
categoryOptionsInit(categoryOptionsRef opt)
{
	bzero(opt, sizeof(*opt));
}

static void
categoryOptionsFree(categoryOptionsRef opt)
{
	__SC_CFRELEASE(opt->category_id);
	__SC_CFRELEASE(opt->category_value);
}

/*
 * networkSettings
 */
static SCNSManagerRef
manager_create(void)
{
	return (SCNSManagerCreate(CFSTR("netconfig")));
}

typedef struct {
	SCNSManagerRef		manager;
	SCNSServiceRef		service;
} managerService, *managerServiceRef;

typedef struct {
	SCPreferencesRef	prefs;
	SCNetworkServiceRef	service;
} prefsService, *prefsServiceRef;

typedef struct {
	Boolean			use_manager;
	union {
		managerService	manager;
		prefsService	prefs;
	};
	SCNetworkInterfaceRef	netif;
} networkSetup, *networkSetupRef;

static void
networkSetupClear(networkSetupRef settings)
{
	bzero(settings, sizeof(*settings));
}

static void
networkSetupInitialize(networkSetupRef setup, Boolean use_manager)
{
	setup->use_manager = use_manager;
	if (use_manager) {
		setup->manager.manager = manager_create();
	}
	else {
		setup->prefs.prefs = prefs_create();
	}
}

static void
networkSetupRelease(networkSetupRef setup)
{
	if (setup->use_manager) {
		__SC_CFRELEASE(setup->manager.manager);
		__SC_CFRELEASE(setup->manager.service);
	}
	else {
		__SC_CFRELEASE(setup->prefs.prefs);
		__SC_CFRELEASE(setup->prefs.service);
	}
	__SC_CFRELEASE(setup->netif);
}


/*
 * QoSMarking options
 */
typedef struct {
	CFBooleanRef		enable;
	CFBooleanRef		apple_av;
	CFMutableArrayRef	app_ids;
} QoSMarkingOptions, *QoSMarkingOptionsRef;

static void
QoSMarkingOptionsInit(QoSMarkingOptionsRef opt)
{
	bzero(opt, sizeof(*opt));
}

static void
QoSMarkingOptionsFree(QoSMarkingOptionsRef opt)
{
	__SC_CFRELEASE(opt->app_ids);
}

static Boolean
QoSMarkingOptionsAdd(QoSMarkingOptionsRef opt,
		     const char * optname, const char * optarg,
		     Boolean * ret_success)
{
	Boolean		handled = FALSE;
	Boolean		success = FALSE;

	if (strcmp(optname, kOptQoSMarkingEnable) == 0) {
		Boolean		enable;

		handled = TRUE;
		if (!arg_present(optname, optarg)) {
			goto done;
		}
		if (!get_bool_from_string(optarg, &enable,
					  OPT_QOS_MARKING_ENABLE,
					  kOptQoSMarkingEnable)) {
			goto done;
		}
		opt->enable = enable ? kCFBooleanTrue : kCFBooleanFalse;
		success = TRUE;
	}
	else if (strcmp(optname, kOptQoSMarkingAppleAV) == 0) {
		Boolean		enable;

		handled = TRUE;
		if (!arg_present(optname, optarg)) {
			goto done;
		}
		if (!get_bool_from_string(optarg, &enable,
					  OPT_QOS_MARKING_APPLE_AV,
					  kOptQoSMarkingAppleAV)) {
			goto done;
		}
		opt->apple_av = enable ? kCFBooleanTrue : kCFBooleanFalse;
		success = TRUE;
	}
	else if (strcmp(optname, kOptQoSMarkingAppID) == 0) {
		static CFStringRef	appID;

		handled = TRUE;
		if (!arg_present(optname, optarg)) {
			goto done;
		}
		if (opt->app_ids == NULL) {
			opt->app_ids = array_create();
		}
		appID = my_CFStringCreate(optarg);
		CFArrayAppendValue(opt->app_ids, appID);
		CFRelease(appID);
		success = TRUE;
	}
 done:
	*ret_success = success;
	return (handled);
}

static Boolean
QoSMarkingOptionsSpecified(QoSMarkingOptionsRef qos)
{
	return (qos->enable != NULL
		|| qos->apple_av != NULL
		|| qos->app_ids != NULL);
}

static Boolean
setQoSMarkingPolicy(networkSetup * setup, categoryOptionsRef cat_opt,
		    CFDictionaryRef __nullable policy)
{
	Boolean			ok = FALSE;

	if (setup->use_manager) {
		SCNSServiceRef	service = setup->manager.service;

		ok = SCNSServiceSetQoSMarkingPolicy(service, policy);
	}
	else if (cat_opt->category_id != NULL) {
		SCNetworkCategoryRef	category;
		SCNetworkServiceRef	service = setup->prefs.service;
		CFStringRef		value = cat_opt->category_value;

		category = SCNetworkCategoryCreate(setup->prefs.prefs,
						   cat_opt->category_id);
		ok = SCNetworkCategorySetServiceQoSMarkingPolicy(category,
								 value,
								 service,
								 policy);
		CFRelease(category);
	}
	else {
		SCNetworkInterfaceRef	netif;
		SCNetworkServiceRef	service = setup->prefs.service;

		netif = SCNetworkServiceGetInterface(service);
		ok = SCNetworkInterfaceSetQoSMarkingPolicy(netif, policy);
	}
	return (ok);
}

static Boolean
QoSMarkingOptionsApply(QoSMarkingOptionsRef qos,
		       networkSetup * setup,
		       categoryOptionsRef cat_opt)
{
	CFMutableDictionaryRef	policy;
	Boolean			ok = FALSE;

	policy = dict_create();
	if (qos->enable != NULL) {
		CFDictionarySetValue(policy,
				     kSCPropNetQoSMarkingEnabled,
				     qos->enable);
	}
	if (qos->apple_av != NULL) {
		CFDictionarySetValue(policy,
				     kSCPropNetQoSMarkingAppleAudioVideoCalls,
				     qos->apple_av);
	}
	if (qos->app_ids != NULL) {
		CFDictionarySetValue(policy,
				     kSCPropNetQoSMarkingAllowListAppIdentifiers,
				     qos->app_ids);
	}
	ok = setQoSMarkingPolicy(setup, cat_opt, policy);
	CFRelease(policy);
	return (ok);
}

/*
 * StringMap
 * - map between C string and CFString
 */
typedef struct {
	const char *		string;
	const CFStringRef *	cfstring;
} StringMap, *StringMapRef;

static const char *
StringMapGetString(StringMapRef map, unsigned int count,
		   unsigned int index)
{
	if (index >= count) {
		return (NULL);
	}
	return (map[index].string);
}

static CFStringRef
StringMapGetCFString(StringMapRef map, unsigned int count,
		     unsigned int index)
{
	if (index >= count) {
		return (NULL);
	}
	return (*(map[index].cfstring));
}

static int
StringMapGetIndexOfString(StringMapRef map, unsigned int count,
			  const char * str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		if (strcasecmp(str, map[i].string) == 0) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

static int
StringMapGetIndexOfCFString(StringMapRef map, unsigned int count,
			    CFStringRef str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		if (CFEqual(str, *map[i].cfstring)) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

#define STRINGMAP_GET_INDEX_OF_STRING(map, str)			\
	StringMapGetIndexOfString(map, countof(map), str)

#define STRINGMAP_GET_INDEX_OF_CFSTRING(map, str)			\
	StringMapGetIndexOfCFString(map, countof(map), str)

#define STRINGMAP_GET_STRING(map, index) \
	StringMapGetString(map, countof(map), index)

#define STRINGMAP_GET_CFSTRING(map, index) \
	StringMapGetCFString(map, countof(map), index)

static const char *
string_array_get(const char * * strlist, unsigned int count,
		 unsigned int index)
{
	if (index >= count) {
		return (NULL);
	}
	return (strlist[index]);
}

static int
string_array_get_index_of_value(const char * * strlist, unsigned int count,
				const char * str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		if (strcasecmp(str, strlist[i]) == 0) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

static int
string_array_get_index_of_prefix(const char * * strlist, unsigned int count,
				 const char * str)
{
	int	where = -1;

	for (unsigned int i = 0; i < count; i++) {
		size_t	len = strlen(strlist[i]);

		if (strncasecmp(str, strlist[i], len) == 0) {
			where = (int)i;
			break;
		}
	}
	return (where);
}

#define STRING_ARRAY_GET_INDEX_OF_VALUE(array, str) \
	string_array_get_index_of_value(array, countof(array), str)

#define STRING_ARRAY_GET_INDEX_OF_PREFIX(array, str) \
	string_array_get_index_of_prefix(array, countof(array), str)

#define STRING_ARRAY_GET(array, index)			\
	string_array_get(array, countof(array), index)

typedef enum {
      kInterfaceTypeNone = 0,
      kInterfaceTypeVLAN = 1,
      kInterfaceTypeBridge = 2,
} InterfaceType;

static const char * interface_type_strings[] = {
	"vlan",
	"bridge",
};

static InterfaceType
InterfaceTypeFromString(const char * str)
{
	InterfaceType	type;

	type = (InterfaceType)
		(STRING_ARRAY_GET_INDEX_OF_VALUE(interface_type_strings, str)
		 + 1);
	return (type);
}

static InterfaceType
InterfaceTypeFromPrefix(const char * str)
{
	InterfaceType	type;

	type = (InterfaceType)
		(STRING_ARRAY_GET_INDEX_OF_PREFIX(interface_type_strings, str)
		 + 1);
	return (type);
}

/*
 * Commands
 */
typedef enum {
	kCommandNone		= 0,
	kCommandAdd		= 1,	/* add a service */
	kCommandSet		= 2,	/* set service configuration */
	kCommandRemove		= 3,	/* remove service/protocol */
	kCommandEnable		= 4,	/* enable service/protocol */
	kCommandDisable		= 5,	/* disable service/protocol */
	kCommandShow		= 6,	/* show information */
	kCommandCreate		= 7,	/* create an interface */
	kCommandDestroy 	= 8,	/* destroy an interface */
	kCommandSetVLAN		= 9,	/* set VLAN params */
	kCommandSetBridge	= 10,	/* set bridge params */
} Command;

static Command 		G_command;

static const char * command_strings[] = {
	"add",
	"set",
	"remove",
	"enable",
	"disable",
	"show",
	"create",
	"destroy",
	"setvlan",
	"setbridge",
};

static const char *
CommandGetString(Command command)
{
	return STRING_ARRAY_GET(command_strings, command - 1);
}

static Command
CommandFromString(const char * str)
{
	Command	command;

	command = (Command)
		(STRING_ARRAY_GET_INDEX_OF_VALUE(command_strings, str) + 1);
	return (command);
}

/*
 * Protocols
 */
typedef
enum {
	kProtocolNone			= 0,
	kProtocolIPv4			= 1,
	kProtocolIPv6			= 2,
	kProtocolDNS			= 3,
} Protocol;

static StringMap  protocol_strings[] = {
	{ "ipv4", &kSCNetworkProtocolTypeIPv4 },
	{ "ipv6", &kSCNetworkProtocolTypeIPv6 },
	{ "dns", &kSCNetworkProtocolTypeDNS },
};

static const char *
ProtocolGetString(Protocol protocol)
{
	return STRINGMAP_GET_STRING(protocol_strings, protocol - 1);
}

static Protocol
ProtocolFromString(const char * str)
{
	Protocol	proto;

	proto = (Protocol)(STRINGMAP_GET_INDEX_OF_STRING(protocol_strings,
							 str) + 1);
	return (proto);
}

static CFStringRef
ProtocolGetCFString(Protocol protocol)
{
	return STRINGMAP_GET_CFSTRING(protocol_strings, protocol - 1);
}

/*
 * ConfigMethod map
 */

typedef
enum {
	kIPv4ConfigMethodNone		= 0,
	kIPv4ConfigMethodDHCP		= 1,
	kIPv4ConfigMethodInform		= 2,
	kIPv4ConfigMethodManual		= 3,
	kIPv4ConfigMethodLinkLocal	= 4,
} IPv4ConfigMethod;

static StringMap ipv4_config_method_map[] = {
	{ "dhcp", &kSCValNetIPv4ConfigMethodDHCP },
	{ "inform", &kSCValNetIPv4ConfigMethodINFORM },
	{ "manual", &kSCValNetIPv4ConfigMethodManual },
	{ "linklocal", &kSCValNetIPv4ConfigMethodLinkLocal },
};

typedef
enum {
	kIPv6ConfigMethodNone		= 0,
	kIPv6ConfigMethodAutomatic	= 1,
	kIPv6ConfigMethodManual		= 2,
	kIPv6ConfigMethodLinkLocal	= 3,
} IPv6ConfigMethod;

static StringMap ipv6_config_method_map[] = {
	{ "automatic", 	&kSCValNetIPv6ConfigMethodAutomatic },
	{ "manual", &kSCValNetIPv6ConfigMethodManual },
	{ "linklocal", &kSCValNetIPv6ConfigMethodLinkLocal },
};

static IPv4ConfigMethod
IPv4ConfigMethodFromString(const char * config_method)
{
	IPv4ConfigMethod	m;

	m = (IPv4ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_STRING(ipv4_config_method_map,
					       config_method) + 1);
	return (m);
}

static IPv4ConfigMethod
IPv4ConfigMethodFromCFString(CFStringRef config_method)
{
	IPv4ConfigMethod	m;

	m = (IPv4ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_CFSTRING(ipv4_config_method_map,
						config_method) + 1);
	return (m);
}

static CFStringRef
IPv4ConfigMethodGetCFString(IPv4ConfigMethod method)
{
	return STRINGMAP_GET_CFSTRING(ipv4_config_method_map, method - 1);
}

static const char *
IPv4ConfigMethodGetString(IPv4ConfigMethod method)
{
	return STRINGMAP_GET_STRING(ipv4_config_method_map, method - 1);
}

static CFStringRef
IPv6ConfigMethodGetCFString(IPv6ConfigMethod method)
{
	return STRINGMAP_GET_CFSTRING(ipv6_config_method_map, method - 1);
}

static const char *
IPv6ConfigMethodGetString(IPv6ConfigMethod method)
{
	return STRINGMAP_GET_STRING(ipv6_config_method_map, method - 1);
}

static IPv6ConfigMethod
IPv6ConfigMethodFromString(const char * config_method)
{
	IPv6ConfigMethod	m;

	m = (IPv6ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_STRING(ipv6_config_method_map,
					       config_method) + 1);
	return (m);
}

static IPv6ConfigMethod
IPv6ConfigMethodFromCFString(CFStringRef config_method)
{
	IPv6ConfigMethod	m;

	m = (IPv6ConfigMethod)
		(STRINGMAP_GET_INDEX_OF_CFSTRING(ipv6_config_method_map,
						config_method) + 1);
	return (m);
}

/*
 * SCNetworkConfiguration utility functions
 */
static SCPreferencesRef
prefs_create_with_file(CFStringRef file)
{
	SCPreferencesRef prefs;

	prefs = SCPreferencesCreate(NULL, CFSTR("netconfig"), file);
	if (prefs == NULL) {
		SCPrint(TRUE, stderr,
			CFSTR("SCPreferencesCreate(%@) failed, %s\n"),
			file, SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	return (prefs);
}

static SCPreferencesRef
prefs_create(void)
{
	return (prefs_create_with_file(NULL));
}

static CFStringRef
interface_copy_summary(SCNetworkInterfaceRef netif)
{
	CFStringRef		bsd_name;
	SCNetworkInterfaceRef	child;
	CFStringRef		name;
	CFMutableStringRef	str;
	CFStringRef		type;

	str = CFStringCreateMutable(NULL, 0);
	type = SCNetworkInterfaceGetInterfaceType(netif);
	if (type != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR("%@"), type);
	}
	bsd_name = SCNetworkInterfaceGetBSDName(netif);
	if (bsd_name != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" (%@)"), bsd_name);
		name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
		if (name != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR(" %@"), name);
		}
	}
	child = SCNetworkInterfaceGetInterface(netif);
	if (child != NULL) {
		bsd_name = SCNetworkInterfaceGetBSDName(child);
		if (bsd_name != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR(" (%@)"),
					     bsd_name);
		}
		name = SCNetworkInterfaceGetLocalizedDisplayName(child);
		if (name != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR(" %@"), name);
		}
	}
	return (str);
}

static Boolean
append_first_value(CFMutableStringRef str, CFDictionaryRef dict,
		   CFStringRef key)
{
	CFArrayRef	list = CFDictionaryGetValue(dict, key);
	CFTypeRef	val = NULL;

	if (isA_CFArray(list) != NULL && CFArrayGetCount(list) != 0) {
		val = CFArrayGetValueAtIndex(list, 0);
	}
	if (val == NULL) {
		return (FALSE);
	}
	CFStringAppendFormat(str, NULL, CFSTR("%@"), val);
	return (TRUE);
}

static void
append_ipv4_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef		client_id;
	IPv4ConfigMethod	method;
	CFStringRef		method_str;
	CFStringRef		router;

	method_str = CFDictionaryGetValue(dict, kSCPropNetIPv4ConfigMethod);
	if (method_str == NULL) {
		return;
	}
	CFStringAppendFormat(str, NULL, CFSTR(" %@ "), method_str);
	method = IPv4ConfigMethodFromCFString(method_str);
	switch (method) {
	case kIPv4ConfigMethodDHCP:
		/* DHCP client id */
		client_id = CFDictionaryGetValue(dict,
						 kSCPropNetIPv4DHCPClientID);
		if (client_id != NULL) {
			CFStringAppendFormat(str, NULL, CFSTR("clientID=%@ "),
					     client_id);
		}
		break;
	case kIPv4ConfigMethodInform:
	case kIPv4ConfigMethodManual:
		/* address, subnet mask, router */
		if (append_first_value(str, dict, kSCPropNetIPv4Addresses)) {
			if (CFDictionaryContainsKey(dict,
						    kSCPropNetIPv4SubnetMasks)) {
				CFStringAppend(str, CFSTR("/"));
				append_first_value(str, dict,
						   kSCPropNetIPv4SubnetMasks);
			}
		}
		router = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
		if (router != NULL) {
			CFStringAppendFormat(str, NULL,
					     CFSTR(" router=%@"), router);
		}
		break;
	case kIPv4ConfigMethodLinkLocal:
	case kIPv4ConfigMethodNone:
		break;
	}
	return;
}

static void
append_ipv6_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	IPv6ConfigMethod	method;
	CFStringRef		method_str;
	CFStringRef		router;

	method_str = CFDictionaryGetValue(dict, kSCPropNetIPv6ConfigMethod);
	if (method_str == NULL) {
		return;
	}
	CFStringAppendFormat(str, NULL, CFSTR(" %@ "), method_str);
	method = IPv6ConfigMethodFromCFString(method_str);
	switch (method) {
	case kIPv6ConfigMethodManual:
		/* address, subnet mask, router */
		if (append_first_value(str, dict, kSCPropNetIPv6Addresses)) {
			if (CFDictionaryContainsKey(dict,
						    kSCPropNetIPv6PrefixLength)) {
				CFStringAppend(str, CFSTR("/"));
				append_first_value(str, dict,
						   kSCPropNetIPv6PrefixLength);
			}
		}
		router = CFDictionaryGetValue(dict, kSCPropNetIPv6Router);
		if (router != NULL) {
			CFStringAppendFormat(str, NULL,
					     CFSTR(" router=%@"), router);
		}
		break;
	case kIPv6ConfigMethodAutomatic:
	case kIPv6ConfigMethodLinkLocal:
	case kIPv6ConfigMethodNone:
		break;
	}
	return;
}

static Boolean
append_array_values(CFMutableStringRef str, CFDictionaryRef dict,
		    CFStringRef key)
{
	CFArrayRef list;	

	list = CFDictionaryGetValue(dict, key);
	if (isA_CFArray(list) == NULL || CFArrayGetCount(list) == 0) {
		return (FALSE);
	}
	for (CFIndex i = 0, count = CFArrayGetCount(list); i < count; i++) {
		CFTypeRef	val = CFArrayGetValueAtIndex(list, i);

		CFStringAppendFormat(str, NULL, CFSTR("%s%@"),
				    i == 0 ? "" : ",",
				    val);
	}
	return (TRUE);
}

static void
append_dns_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef	domain;

	if (CFDictionaryContainsKey(dict, kSCPropNetDNSServerAddresses)) {
		CFStringAppend(str, CFSTR(" "));
	}
	if (!append_array_values(str, dict, kSCPropNetDNSServerAddresses)) {
		return;
	}
	domain = CFDictionaryGetValue(dict, kSCPropNetDNSDomainName);
	if (domain != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" domain=%@"), domain);
	}
	if (CFDictionaryContainsKey(dict, kSCPropNetDNSSearchDomains)) {
		CFStringAppend(str, CFSTR(" search="));
	}
	append_array_values(str, dict, kSCPropNetDNSSearchDomains);
}

static void
append_proto_descr(CFMutableStringRef str, Protocol proto,
		   CFDictionaryRef config)
{
	switch (proto) {
	case kProtocolIPv4:
		append_ipv4_descr(str, config);
		break;
	case kProtocolIPv6:
		append_ipv6_descr(str, config);
		break;
	case kProtocolDNS:
		append_dns_descr(str, config);
		break;
	case kProtocolNone:
		break;
	}
}

static CFStringRef
service_copy_protocol_summary(SCNetworkServiceRef service)
{
	unsigned int		i;
	StringMapRef		map;
	CFMutableStringRef	str = NULL;

	for (i = 0, map = protocol_strings;
	     i < countof(protocol_strings); i++, map++) {
		CFDictionaryRef		config;
		SCNetworkProtocolRef	p;
		Protocol		proto = i + 1;
		CFStringRef		type = *map->cfstring;

		p = SCNetworkServiceCopyProtocol(service, type);
		if (p == NULL) {
			continue;
		}
		config = SCNetworkProtocolGetConfiguration(p);
		if (config != NULL) {
			if (str == NULL) {
				str = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(str, NULL,
						     CFSTR("%@={"),
						     type);
			}
			else {
				CFStringAppendFormat(str, NULL, CFSTR(" %@={"),
						     type);
			}
			if (!SCNetworkProtocolGetEnabled(p)) {
				CFStringAppendFormat(str, NULL,
						     CFSTR(" [DISABLED]"));
			}
			append_proto_descr(str, proto, config);
			CFStringAppend(str, CFSTR("}"));
		}
		CFRelease(p);
	}
	return (str);
}

static CFStringRef
settings_service_copy_protocol_summary(SCNSServiceRef service)
{
	unsigned int		i;
	StringMapRef		map;
	CFMutableStringRef	str = NULL;

	for (i = 0, map = protocol_strings;
	     i < countof(protocol_strings); i++, map++) {
		CFDictionaryRef		config;
		Protocol		proto = i + 1;
		CFStringRef		type = *map->cfstring;

		config = SCNSServiceCopyProtocolEntity(service, type);
		if (config != NULL) {
			if (str == NULL) {
				str = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(str, NULL,
						     CFSTR("%@={"),
						     type);
			}
			else {
				CFStringAppendFormat(str, NULL, CFSTR(" %@={"),
						     type);
			}
			append_proto_descr(str, proto, config);
			CFStringAppend(str, CFSTR("}"));
			CFRelease(config);
		}
	}
	return (str);
}

static CFStringRef
service_copy_summary(SCNetworkServiceRef service)
{
	CFStringRef		if_summary;
	CFStringRef		name;
	SCNetworkInterfaceRef	netif;
	CFStringRef		proto_summary;
	CFStringRef		serviceID;
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	name = SCNetworkServiceGetName(service);	
	CFStringAppend(str, name);
	serviceID = SCNetworkServiceGetServiceID(service);
	CFStringAppendFormat(str, NULL, CFSTR(" (%@)"), serviceID);
	if (!SCNetworkServiceGetEnabled(service)) {
		CFStringAppend(str, CFSTR(" [DISABLED]"));
	}
	netif = SCNetworkServiceGetInterface(service);
	if_summary = interface_copy_summary(netif);
	if (if_summary != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" %@"), if_summary);
		CFRelease(if_summary);
	}
	proto_summary = service_copy_protocol_summary(service);
	if (proto_summary != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR("\n\t%@"), proto_summary);
		CFRelease(proto_summary);
	}
	return (str);
}

static CFStringRef
settings_service_copy_summary(SCNSServiceRef service)
{
	CFStringRef		if_summary;
	CFStringRef		name;
	SCNetworkInterfaceRef	netif;
	CFStringRef		proto_summary;
	CFStringRef		serviceID;
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	name = SCNSServiceGetName(service);	
	CFStringAppend(str, name);
	serviceID = SCNSServiceGetServiceID(service);
	CFStringAppendFormat(str, NULL, CFSTR(" (%@)"), serviceID);
	netif = SCNSServiceGetInterface(service);
	if_summary = interface_copy_summary(netif);
	if (if_summary != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" %@"), if_summary);
		CFRelease(if_summary);
	}
	proto_summary = settings_service_copy_protocol_summary(service);
	if (proto_summary != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR("\n\t%@"), proto_summary);
		CFRelease(proto_summary);
	}
	return (str);
}

static CFStringRef
setup_service_copy_summary(networkSetupRef setup)
{
	CFStringRef	str;

	if (setup->use_manager) {
		str = settings_service_copy_summary(setup->manager.service);
	}
	else {
		str = service_copy_summary(setup->prefs.service);
	}
	return (str);
}

static SCNetworkProtocolRef
service_copy_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	SCNetworkProtocolRef	protocol;

	protocol = SCNetworkServiceCopyProtocol(service, type);
	if (protocol == NULL) {
		if (!SCNetworkServiceAddProtocolType(service, type)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to add %@\n"),
				type);
			return (NULL);
		}
		protocol = SCNetworkServiceCopyProtocol(service, type);
		if (protocol == NULL) {
			SCPrint(TRUE, stderr,
				CFSTR("can't copy protocol %@\n"),
				type);
			return (NULL);
		}
	}
	return (protocol);
}

static Boolean
service_remove_protocol(networkSetupRef setup, CFStringRef type)
{
	Boolean		ok;

	if (setup->use_manager) {
		SCNSServiceRef	service = setup->manager.service;

		ok = SCNSServiceSetProtocolEntity(service, type, NULL);
	}
	else {
		SCNetworkServiceRef	service = setup->prefs.service;

		ok = SCNetworkServiceRemoveProtocolType(service, type);
	}
	return (ok);
}

static Boolean
service_set_protocol_enabled(SCNetworkServiceRef service, CFStringRef type,
			     Boolean enabled)
{
	SCNetworkProtocolRef	proto;
	Boolean			success;

	proto = SCNetworkServiceCopyProtocol(service, type);
	if (proto == NULL) {
		SCPrint(TRUE, stderr, CFSTR("protocol %@ not found\n"),
			type);
		return (FALSE);
	}
	success = SCNetworkProtocolSetEnabled(proto, enabled);
	if (!success) {
		SCPrint(TRUE, stderr, CFSTR("Failed to %s protocol %@\n"),
			enabled ? "enable" : "disable", type);
	}
	CFRelease(proto);
	return (success);
}

static Boolean
service_disable_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	return (service_set_protocol_enabled(service, type, FALSE));
}

static Boolean
service_enable_protocol(SCNetworkServiceRef service, CFStringRef type)
{
	return (service_set_protocol_enabled(service, type, TRUE));
}

static Boolean
set_service_enabled(SCNetworkServiceRef service, Boolean enabled)
{
	if (enabled) {
		StringMapRef	map;

		map = protocol_strings;
		for (unsigned int i = 0; i < countof(protocol_strings);
		     i++, map++) {
			(void)service_set_protocol_enabled(service,
							   *map->cfstring,
							   enabled);
		}
	}
	return (SCNetworkServiceSetEnabled(service, enabled));
}

static Boolean
enable_service(SCNetworkServiceRef service)
{
	return (set_service_enabled(service, TRUE));
}

static Boolean
disable_service(SCNetworkServiceRef service)
{
	return (set_service_enabled(service, FALSE));
}

static Boolean
matchInterface(SCNetworkInterfaceRef netif, CFStringRef name)
{
	Boolean		match = FALSE;
	CFStringRef	this_name;

	do { /* something to break out of */
		this_name = SCNetworkInterfaceGetBSDName(netif);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
		this_name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
	} while (0);

	return (match);
}

static Boolean
matchService(SCNetworkServiceRef service, SCNetworkInterfaceRef netif,
	     CFStringRef name)
{
	Boolean		match = FALSE;
	CFStringRef	this_name;

	do { /* something to break out of */
		this_name = SCNetworkServiceGetName(service);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
		this_name = SCNetworkServiceGetServiceID(service);
		if (this_name != NULL) {
			match = CFEqual(this_name, name);
			if (match) {
				break;
			}
		}
		if (netif != NULL) {
			match = matchInterface(netif, name);
		}
	} while (0);

	return (match);
}

static SCNetworkServiceRef
copy_configured_service_in_list(CFArrayRef services,
				CFStringRef name,
				Boolean is_bsd_name)
{
	SCNetworkServiceRef	service = NULL;

	for (CFIndex i = 0, count = CFArrayGetCount(services);
	     i < count; i++) {
		Boolean			found = FALSE;
		SCNetworkInterfaceRef	netif;
		SCNetworkServiceRef	s;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		netif = SCNetworkServiceGetInterface(s);
		if (netif == NULL) {
			continue;
		}
		if (is_bsd_name) {
			found = matchInterface(netif, name);
		}
		else {
			found = matchService(s, netif, name);
		}
		if (found) {
			CFRetain(s);
			service = s;
			break;
		}
	}
	return (service);
}


static SCNetworkServiceRef
copy_configured_service_in_set(SCNetworkSetRef set,
			       CFStringRef name,
			       Boolean is_bsd_name)
{
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkSetCopyServices(set);
	if (services == NULL) {
		goto done;
	}
	service = copy_configured_service_in_list(services, name, is_bsd_name);
 done:
	__SC_CFRELEASE(services);
	return (service);
}


static SCNetworkServiceRef
copy_category_service(SCNetworkCategoryRef category,
		      CFStringRef value, CFStringRef name,
		      Boolean is_bsd_name)
{
	SCNetworkServiceRef	service = NULL;
	CFArrayRef		services;

	services = SCNetworkCategoryCopyServices(category, value);
	if (services == NULL) {
		goto done;
	}
	service = copy_configured_service_in_list(services, name, is_bsd_name);
 done:
	__SC_CFRELEASE(services);
	return (service);
}

static SCNetworkServiceRef
copy_set_service(SCPreferencesRef prefs,
		 CFStringRef name,
		 Boolean is_bsd_name)
{
	SCNetworkSetRef		current_set;
	SCNetworkServiceRef	ret_service = NULL;

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		goto done;
	}
	ret_service = copy_configured_service_in_set(current_set,
						     name, is_bsd_name);
 done:
	__SC_CFRELEASE(current_set);
	return (ret_service);
}

static SCNetworkServiceRef
copy_prefs_service(SCPreferencesRef prefs, categoryOptionsRef cat_opt,
		   CFStringRef name, Boolean is_bsd_name)
{
	SCNetworkServiceRef	service;

	if (cat_opt->category_id != NULL) {
		SCNetworkCategoryRef	category;

		category = SCNetworkCategoryCreate(prefs,
						   cat_opt->category_id);
		service = copy_category_service(category,
						cat_opt->category_value, name,
						is_bsd_name);
		CFRelease(category);
	}
	else {
		service = copy_set_service(prefs, name, is_bsd_name);
	}
	return (service);
}

static Boolean
copy_setup_service(networkSetupRef setup, categoryOptionsRef cat_opt,
		      CFStringRef name, const char * name_c, Boolean is_bsd_name)
{
	if (setup->use_manager) {
		setup->netif = copy_available_interface(name, name_c);
		if (setup->netif == NULL) {
			goto done;
		}
		setup->manager.service
			= SCNSManagerCopyService(setup->manager.manager,
						 setup->netif,
						 cat_opt->category_id,
						 cat_opt->category_value);
		if (setup->manager.service == NULL) {
			/* no service, clear netif */
			__SC_CFRELEASE(setup->netif);
		}
	}
	else {
		setup->prefs.service
			= copy_prefs_service(setup->prefs.prefs,
					     cat_opt, name, is_bsd_name);
		setup->netif
			= SCNetworkServiceGetInterface(setup->prefs.service);
		if (setup->netif != NULL) {
			CFRetain(setup->netif);
		}
	}
 done:
	return (setup->netif != NULL);
}

static void
show_scerror(const char * message)
{
	fprintf(stderr, "%s failed: %s\n",
		message, SCErrorString(SCError()));
}

static SCNetworkServiceRef
create_service_in_set(SCPreferencesRef prefs, SCNetworkInterfaceRef netif)
{
	SCNetworkSetRef		current_set;
	SCNetworkServiceRef	service;
	SCNetworkServiceRef	ret_service = NULL;

	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set == NULL) {
		current_set = SCNetworkSetCreate(prefs);
		if (!SCNetworkSetSetCurrent(current_set)) {
			show_scerror("Set current set");
			goto done;
		}
		(void)SCNetworkSetSetName(current_set, CFSTR("Automatic"));
	}
	service = SCNetworkServiceCreate(prefs, netif);
	if (service == NULL) {
		show_scerror("Create service");
		goto done;
	}
	if (!SCNetworkSetAddService(current_set, service)) {
		CFRelease(service);
		show_scerror("Add service to set");
		goto done;
	}
	ret_service = service;

 done:
	__SC_CFRELEASE(current_set);
	return (ret_service);
}

static SCNetworkServiceRef
create_service_in_category(SCPreferencesRef prefs,
			   CFStringRef category_id,
			   CFStringRef category_value,
			   SCNetworkInterfaceRef netif)
{
	SCNetworkCategoryRef	category;
	SCNetworkServiceRef	ret_service = NULL;
	SCNetworkServiceRef	service;

	category = SCNetworkCategoryCreate(prefs, category_id);
	service = SCNetworkServiceCreate(prefs, netif);
	if (!SCNetworkCategoryAddService(category, category_value, service)) {
		CFRelease(service);
		show_scerror("failed to add service to category");
	}
	else {
		ret_service = service;
	}
	CFRelease(category);
	return (ret_service);
}

static Boolean
create_prefs_service(prefsServiceRef prefs, SCNetworkInterfaceRef netif,
		     categoryOptionsRef cat_opt)
{
	if (cat_opt->category_id != NULL) {
		prefs->service
			= create_service_in_category(prefs->prefs,
						     cat_opt->category_id,
						     cat_opt->category_value,
						     netif);
	}
	else {
		prefs->service
			= create_service_in_set(prefs->prefs, netif);
	}
	return (prefs->service != NULL);
}

static Boolean
create_manager_service(managerServiceRef manager, SCNetworkInterfaceRef netif,
		       categoryOptionsRef cat_opt)
{
	manager->service
		= SCNSManagerCreateService(manager->manager,
					   netif,
					   cat_opt->category_id,
					   cat_opt->category_value);
	return (manager->service != NULL);
}

static Boolean
create_setup_service(networkSetupRef setup, categoryOptionsRef cat_opt)
{
	Boolean		created;

	if (setup->use_manager) {
		created = create_manager_service(&setup->manager,
						 setup->netif,
						 cat_opt);
	}
	else {
		created = create_prefs_service(&setup->prefs,
					       setup->netif,
					       cat_opt);
	}
	return (created);
}


static Boolean
remove_prefs_service(SCPreferencesRef prefs,
		     SCNetworkServiceRef service, categoryOptionsRef cat_opt)
{
	Boolean			success = FALSE;

	if (cat_opt->category_id != NULL) {
		SCNetworkCategoryRef	category;

		category = SCNetworkCategoryCreate(prefs,
						   cat_opt->category_id);
		success = SCNetworkCategoryRemoveService(category,
							 cat_opt->category_value,
							 service);
		CFRelease(category);
	}
	else {
		success = SCNetworkServiceRemove(service);
	}
	if (!success) {
		show_scerror("Remove service");
	}
	return (success);
					    
}


static Boolean
remove_setup_service(networkSetupRef setup, categoryOptionsRef cat_opt)
{
	Boolean			success = FALSE;

	if (setup->use_manager) {
		SCNSManagerRemoveService(setup->manager.manager,
					 setup->manager.service);
		success = TRUE;
	}
	else {
		success = remove_prefs_service(setup->prefs.prefs,
					       setup->prefs.service,
					       cat_opt);
	}
	return (success);
}


static SCNetworkInterfaceRef
copy_bsd_interface(CFStringRef name_cf, const char * name)
{
	unsigned int		index;
	SCNetworkInterfaceRef	netif = NULL;

	index = if_nametoindex(name);
	if (index != 0) {
		netif = _SCNetworkInterfaceCreateWithBSDName(NULL, name_cf, 0);
	}
	return (netif);
}

static SCNetworkInterfaceRef
copy_available_interface(CFStringRef name_cf, const char * name)
{
	CFArrayRef		if_list;
	SCNetworkInterfaceRef	ret_netif = NULL;

	if_list = SCNetworkInterfaceCopyAll();
	if (if_list == NULL) {
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(if_list);
	     i < count; i++) {
		SCNetworkInterfaceRef	netif;

		netif = (SCNetworkInterfaceRef)
			CFArrayGetValueAtIndex(if_list, i);
		if (matchInterface(netif, name_cf)) {
			CFRetain(netif);
			ret_netif = netif;
			break;
		}
	}
	if (ret_netif == NULL) {
		ret_netif = copy_bsd_interface(name_cf, name);
	}
 done:
	__SC_CFRELEASE(if_list);
	return (ret_netif);
}

static Boolean
service_establish_default(SCNetworkServiceRef service)
{
	CFArrayRef 	protocols;
	Boolean		success = FALSE;

	protocols = SCNetworkServiceCopyProtocols(service);
	if (protocols != NULL) {
		for (CFIndex i = 0, count = CFArrayGetCount(protocols);
		     i < count; i++) {
			SCNetworkProtocolRef	proto;
			CFStringRef		type;

			proto = (SCNetworkProtocolRef)
				CFArrayGetValueAtIndex(protocols, i);
			type = SCNetworkProtocolGetProtocolType(proto);
			if (!SCNetworkServiceRemoveProtocolType(service, type)) {
				SCPrint(TRUE, stderr,
					CFSTR("Failed to remove %@\n"),
					type);
				goto done;
			}
		}
	}
	success = SCNetworkServiceEstablishDefaultConfiguration(service);
 done:
	__SC_CFRELEASE(protocols);
	return (success);
}

static SCNetworkInterfaceRef
array_copy_netif_with_name(CFArrayRef list, CFStringRef match_name,
			   Boolean bsd_name_only)
{
	SCNetworkInterfaceRef	ret_netif = NULL;

	for (CFIndex i = 0, count = CFArrayGetCount(list);
	     i < count; i++) {
		CFStringRef		name;
		SCNetworkInterfaceRef	netif;

		netif = (SCNetworkInterfaceRef)
			CFArrayGetValueAtIndex(list, i);
		name = SCNetworkInterfaceGetBSDName(netif);
		if (name != NULL && CFEqual(match_name, name)) {
			ret_netif = netif;
			CFRetain(ret_netif);
			break;
		}
		if (bsd_name_only) {
			continue;
		}
		name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
		if (name != NULL && CFEqual(match_name, name)) {
			ret_netif = netif;
			CFRetain(ret_netif);
			break;
		}
	}
	return (ret_netif);
}

static SCNetworkInterfaceRef
copy_bridge_interface(SCPreferencesRef prefs, CFStringRef bridge_name)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	ret_netif = NULL;

	list = SCBridgeInterfaceCopyAll(prefs);
	if (list != NULL) {
		ret_netif = array_copy_netif_with_name(list, bridge_name,
						       FALSE);
		CFRelease(list);
	}
	return (ret_netif);
}

static void
report_unavailable_interface(CFStringRef name_cf, CFArrayRef available,
			     const char * msg)
{
	unsigned int 	index;
	char		name[IFNAMSIZ];

	CFStringGetCString(name_cf, name, sizeof(name),
			   kCFStringEncodingUTF8);
	if (available == NULL) {
		fprintf(stderr,	"'%s' is unavailable for %s\n", name, msg);
		return;
	}
	index = if_nametoindex(name);
	if (index == 0) {
		fprintf(stderr, "'%s' does not exist", name);
	}
	else {
		fprintf(stderr,	"'%s' is unavailable for %s", name, msg);
	}
	fprintf(stderr, ", available interfaces are: ");
	for (CFIndex i = 0, count = CFArrayGetCount(available);
	     i < count; i++) {
		SCNetworkInterfaceRef	this;
		this = (SCNetworkInterfaceRef)
			CFArrayGetValueAtIndex(available, i);
		SCPrint(TRUE, stderr,
			CFSTR("%s%@"),
			i == 0 ? "" : ", ",
			SCNetworkInterfaceGetBSDName(this));
	}
	fprintf(stderr, "\n");
}

static CFArrayRef
copy_bridge_member_interfaces(SCBridgeInterfaceRef bridge_netif,
			      SCPreferencesRef prefs, CFArrayRef members)
{
	CFArrayRef		available;
	CFArrayRef		current = NULL;
	CFMutableArrayRef	netif_members = NULL;

	available = SCBridgeInterfaceCopyAvailableMemberInterfaces(prefs);
	if (available != NULL && CFArrayGetCount(available) == 0) {
		__SC_CFRELEASE(available);
	}
	if (bridge_netif != NULL) {
		current = SCBridgeInterfaceGetMemberInterfaces(bridge_netif);
	}
	if (available == NULL && current == NULL) {
		fprintf(stderr, "No available member interfaces for bridge\n");
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(members); i < count; i++) {
		CFStringRef		name;
		SCNetworkInterfaceRef	netif = NULL;

		name = (CFStringRef)CFArrayGetValueAtIndex(members, i);
		if (current != NULL) {
			netif = array_copy_netif_with_name(current,
							   name, TRUE);
		}
		if (netif == NULL && available != NULL) {
			netif = array_copy_netif_with_name(available,
							   name, TRUE);
		}
		if (netif == NULL) {
			report_unavailable_interface(name, available, "bridge");
			__SC_CFRELEASE(netif_members);
			goto done;
		}
		if (netif_members == NULL) {
			netif_members = array_create();
		}
		CFArrayAppendValue(netif_members, netif);
		CFRelease(netif);
	}
 done:
	__SC_CFRELEASE(available);
	return (netif_members);
}

static SCNetworkInterfaceRef
copy_vlan_interface(SCPreferencesRef prefs, CFStringRef vlan_name)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	ret_netif = NULL;

	list = SCVLANInterfaceCopyAll(prefs);
	if (list != NULL) {
		ret_netif = array_copy_netif_with_name(list, vlan_name,
						       FALSE);
		CFRelease(list);
	}
	return (ret_netif);
}

static SCNetworkInterfaceRef
copy_vlan_physical_interface(CFStringRef physical)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	netif = NULL;

	list = SCVLANInterfaceCopyAvailablePhysicalInterfaces();
	if (list == NULL) {
		fprintf(stderr, "No available physical interfaces for VLAN\n");
		goto done;
	}
	netif = array_copy_netif_with_name(list, physical, TRUE);
	CFRelease(list);
 done:
	return (netif);
}

static SCNetworkInterfaceRef
copy_vlan_device_and_id(const char * vlan_id, const char * vlan_device,
			CFNumberRef * vlan_id_cf_p)
{
	SCNetworkInterfaceRef	netif;
	int			vlan_id_num;
	CFStringRef		vlan_device_cf;
	
	vlan_id_num = atoi(vlan_id);
	if (vlan_id_num < 1 || vlan_id_num > 4094) {
		fprintf(stderr, "Invalid vlan_id %s\n", vlan_id);
		command_specific_help();
	}
	vlan_device_cf = my_CFStringCreate(vlan_device);
	netif = copy_vlan_physical_interface(vlan_device_cf);
	CFRelease(vlan_device_cf);
	if (netif == NULL) {
		unsigned int		index;
		
		index = if_nametoindex(vlan_device);
		if (index == 0) {
			fprintf(stderr, "Can't find physical interface '%s'\n",
				vlan_device);
		} else {
			fprintf(stderr, "Interface '%s' does not support VLAN\n",
				vlan_device);
		}
	}
	else {
		*vlan_id_cf_p
			= CFNumberCreate(NULL, kCFNumberIntType, &vlan_id_num);
	}
	return (netif);
}

/*
 * Typedef: UnConst
 * Work-around for -Wcast-qual not being happy about casting a const pointer
 * e.g. CFArrayRef to (void *).
 */
typedef union {
	void *		non_const_ptr;
	const void *	const_ptr;
} UnConst;

static CFArrayRef
copy_sorted_services(SCNetworkSetRef set)
{
	CFIndex			count;
	CFArrayRef		order = NULL;
	CFArrayRef		services = NULL;
	CFMutableArrayRef	sorted = NULL;
	UnConst			unconst;

	services = SCNetworkSetCopyServices(set);
	if (services == NULL) {
		goto done;
	}
	count = CFArrayGetCount(services);
	if (count == 0) {
		goto done;
	}
	order = SCNetworkSetGetServiceOrder(set);
	sorted = CFArrayCreateMutableCopy(NULL, 0, services);
	unconst.const_ptr = order;
	CFArraySortValues(sorted,
			  CFRangeMake(0, count),
			  _SCNetworkServiceCompare,
			  unconst.non_const_ptr);
 done:
	__SC_CFRELEASE(services);
	return (sorted);
}

static SCNetworkSetRef
set_copy(SCPreferencesRef prefs, CFStringRef set_name)
{
	SCNetworkSetRef	ret_set = NULL;
	CFArrayRef	sets = NULL;

	sets = SCNetworkSetCopyAll(prefs);
	if (sets == NULL) {
		goto done;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(sets);
	     i < count; i++) {
		CFStringRef	name;
		CFStringRef	setID;
		SCNetworkSetRef	s;

		s = (SCNetworkSetRef)CFArrayGetValueAtIndex(sets, i);
		name = SCNetworkSetGetName(s);
		setID = SCNetworkSetGetSetID(s);
		if ((setID != NULL && CFEqual(setID, set_name))
		    || (name != NULL && CFEqual(name, set_name))) {
			ret_set = s;
			CFRetain(s);
			break;
		}
	}

 done:
	__SC_CFRELEASE(sets);
	return (ret_set);
}

static void
commit_apply(SCPreferencesRef prefs)
{
	/* Commit/Apply changes */
	if (!SCPreferencesCommitChanges(prefs)) {
		show_scerror("Commit changes");
		exit(EX_SOFTWARE);
	}
	if (!SCPreferencesApplyChanges(prefs)) {
		show_scerror("Apply changes");
		exit(EX_SOFTWARE);
	}
}

static CFStringRef
createPath(const char * arg)
{
	CFStringRef	arg_str;
	char		path[MAXPATHLEN];
	CFStringRef	ret_path = NULL;

	arg_str = my_CFStringCreate(arg);
	if (arg_str == NULL) {
		/* unlikely failure */
		fprintf(stderr, "Can't convert '%s' to CFString\n", arg);
	}
	else if (arg[0] == '/') {
		ret_path = CFRetain(arg_str);
	}
	else if (getcwd(path, sizeof(path)) == NULL) {
		fprintf(stderr,
			"Can't get current working directory, %s\n",
			strerror(errno));
	}
	else {
		CFStringRef	path_str;

		/* relative path, fully qualify it */
		path_str = my_CFStringCreate(path);
		if (path_str != NULL) {
			ret_path = CFStringCreateWithFormat(NULL, NULL,
							    CFSTR("%@/%@"),
							    path_str, arg_str);
			__SC_CFRELEASE(path_str);
		}
		else {
			fprintf(stderr, "Can't convert path '%s' to CFString\n",
				path);
		}
	}
	__SC_CFRELEASE(arg_str);
	return (ret_path);
}

static void
setup_commit_apply(networkSetupRef setup)
{
	if (setup->use_manager) {
		if (!SCNSManagerApplyChanges(setup->manager.manager)) {
			show_scerror("Manager Apply Changes");
			exit(EX_SOFTWARE);
		}
	}
	else {
		commit_apply(setup->prefs.prefs);
	}
}

/**
 ** Add/Set routines
 **/
typedef struct {
	union {
		struct in_addr	ipv4;
		struct in6_addr	ipv6;
	};
	uint8_t			af;
	uint8_t			pad[3]; /* -Wpadded */
} IPAddress, *IPAddressRef;

typedef struct {
	const char *		dhcp_client_id;
	IPv4ConfigMethod	config_method;
	struct in_addr		address;
	struct in_addr		subnet_mask;
	struct in_addr		router;
} IPv4Params, *IPv4ParamsRef;

typedef struct {
	struct in6_addr		address;
	struct in6_addr		router;
	IPv6ConfigMethod	config_method;
	uint32_t		_pad; /* -Wpadded */
} IPv6Params, *IPv6ParamsRef;

typedef struct {
	CFMutableArrayRef	addresses;
	const char *		domain_name;
	CFMutableArrayRef	search_domains;
} DNSParams, *DNSParamsRef;

typedef struct {
	Protocol		protocol;
	Boolean			default_configuration;
	uint8_t			_pad[3]; /* -Wpadded */
	union {
		IPv4Params	ipv4;
		IPv6Params	ipv6;
		DNSParams	dns;
	};
} ProtocolParams, *ProtocolParamsRef;

#define ADD_SET_OPTSTRING	"A:c:C:Dhi:m:n:N:p:r:s:S:"

static Boolean
FieldSetIPAddress(struct in_addr * field_p, const char * label,
		  const char * arg)
{
	Boolean		success = FALSE;

	if (field_p->s_addr != 0) {
		fprintf(stderr, "%s specified multiple times\n", label);
	}
	else if (inet_pton(AF_INET, arg, field_p) != 1) {
		fprintf(stderr, "%s invalid IPv4 address '%s'\n", label,
			arg);
	}
	else {
		success = TRUE;
	}
	return (success);
}

static Boolean
FieldSetIPv6Address(struct in6_addr * field_p, const char * label,
		    const char * arg)
{
	Boolean		success = FALSE;

	if (!IN6_IS_ADDR_UNSPECIFIED(field_p)) {
		fprintf(stderr, "%s specified multiple times\n", label);
	}
	else if (inet_pton(AF_INET6, arg, field_p) != 1) {
		fprintf(stderr, "%s invalid IPv6 address '%s'\n", label,
			arg);
	}
	else {
		success = TRUE;
	}
	return (success);
}

static void
ProtocolParamsInit(ProtocolParamsRef params)
{
	bzero(params, sizeof(*params));
}

static void
ProtocolParamsRelease(ProtocolParamsRef params)
{
	switch (params->protocol) {
	case kProtocolIPv4:
		break;
	case kProtocolIPv6:
		break;
	case kProtocolDNS:
		__SC_CFRELEASE(params->dns.addresses);
		__SC_CFRELEASE(params->dns.search_domains);
		break;
	case kProtocolNone:
		break;
	}
}

static Boolean
ProtocolParamsAddDHCPClientID(ProtocolParamsRef params, const char * arg)
{
	Boolean		success = FALSE;

	if (params->protocol != kProtocolIPv4) {
		fprintf(stderr, "dhcp-client-id only applies to ivp4\n");
	}
	else if (params->ipv4.dhcp_client_id != NULL) {
		fprintf(stderr, "dhcp-client-id specified multiple times\n");
	}
	else {
		params->ipv4.dhcp_client_id = arg;
		success = TRUE;
	}
	return (success);
}

static Boolean
ProtocolParamsAddConfigMethod(ProtocolParamsRef params, const char * arg)
{
	Boolean success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		if (params->ipv4.config_method != kIPv4ConfigMethodNone) {
			fprintf(stderr,
				"config-method specified multiple times\n");
			break;
		}
		params->ipv4.config_method
			= IPv4ConfigMethodFromString(arg);
		if (params->ipv4.config_method == kIPv4ConfigMethodNone) {
			fprintf(stderr,
				"config-method must be one of "
				"dhcp, manual, inform, or linklocal\n");
			break;
		}
		success = TRUE;
		break;
	case kProtocolIPv6:
		if (params->ipv6.config_method != kIPv6ConfigMethodNone) {
			fprintf(stderr,
				"config-method specified multiple times\n");
			break;
		}
		params->ipv6.config_method
			= IPv6ConfigMethodFromString(arg);
		if (params->ipv6.config_method == kIPv6ConfigMethodNone) {
			fprintf(stderr,
				"config-method must be one of "
				"automatic, manual, or linklocal\n");
			break;
		}
		success = TRUE;
		break;
	case kProtocolNone:
	case kProtocolDNS:
		fprintf(stderr,	"config-method not valid with %s\n",
			ProtocolGetString(params->protocol));
		break;
	}
	return (success);
}

static Boolean
ProtocolParamsAddDNSServers(ProtocolParamsRef params, const char * arg)
{
	char *		address;
	char *		addresses;
	char *		dup_arg;
	Boolean		success = FALSE;

	dup_arg = addresses = strdup(arg);
	while ((address = strsep(&addresses, ",")) != NULL) {
		IPAddress	addr;
		CFStringRef	str;

		bzero(&addr, sizeof(addr));
		if (inet_pton(AF_INET, address, &addr.ipv4) == 1) {
			addr.af = AF_INET;
		}
		else if (inet_pton(AF_INET6, address, &addr.ipv6) == 1) {
			addr.af = AF_INET6;
		}
		else {
			fprintf(stderr, "Invalid IP address '%s'\n", address);
			success = FALSE;
			break;
		}
		str = my_CFStringCreateWithIPAddress(addr.af, &addr.ipv4);
		if (params->dns.addresses == NULL) {
			params->dns.addresses = array_create();
		}
		CFArrayAppendValue(params->dns.addresses, str);
		CFRelease(str);
		success = TRUE;
	}
	free(dup_arg);
	return (success);
}

static Boolean
ProtocolParamsAddAddress(ProtocolParamsRef params, const char * arg)
{
	Boolean	success = FALSE;

	switch (params->protocol) {
	case kProtocolDNS:
		success = ProtocolParamsAddDNSServers(params, arg);
		break;
	case kProtocolIPv4:
		success = FieldSetIPAddress(&params->ipv4.address,
					    kOptAddress, arg);
		break;
	case kProtocolIPv6:
		success = FieldSetIPv6Address(&params->ipv6.address,
					      kOptAddress, arg);
		break;
	case kProtocolNone:
		break;
	}
	return (success);
}

static Boolean
ProtocolParamsAddSubnetMask(ProtocolParamsRef params, const char * arg)
{
	Boolean	success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		success = FieldSetIPAddress(&params->ipv4.subnet_mask,
					    kOptSubnetMask, arg);
		break;
	case kProtocolNone:
	case kProtocolIPv6:
	case kProtocolDNS:
		fprintf(stderr,
			"%s only valid with IPv4\n",
			kOptSubnetMask);
	}
	return (success);
}

static Boolean
ProtocolParamsAddRouter(ProtocolParamsRef params, const char * arg)
{
	Boolean	success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		success = FieldSetIPAddress(&params->ipv4.router,
					    kOptRouter, arg);
		break;
	case kProtocolIPv6:
		success = FieldSetIPv6Address(&params->ipv6.router,
					      kOptRouter, arg);
		break;
	case kProtocolNone:
	case kProtocolDNS:
		fprintf(stderr,
			"%s only valid with IPv4/IPv6\n",
			kOptRouter);
		break;
	}
	return (success);
}

static Boolean
ProtocolParamsAddSearchDomain(ProtocolParamsRef params, const char * arg)
{
	Boolean		success = FALSE;

	if (params->protocol != kProtocolDNS) {
		fprintf(stderr,	"%s only applies to dns\n",
			kOptDNSSearchDomains);
	}
	else {
		CFArrayRef	array;
		CFStringRef	domain;
		CFRange		range;

		domain = my_CFStringCreate(arg);
		array = CFStringCreateArrayBySeparatingStrings(NULL, domain,
							       CFSTR(","));
		CFRelease(domain);
		if (params->dns.search_domains == NULL) {
			params->dns.search_domains = array_create();
		}
		range.location = 0;
		range.length = CFArrayGetCount(array);
		CFArrayAppendArray(params->dns.search_domains, array, range);
		success = TRUE;
		CFRelease(array);
	}
	return (success);
}

static Boolean
ProtocolParamsAddDomainName(ProtocolParamsRef params, const char * arg)
{
	Boolean		success = FALSE;

	if (params->protocol != kProtocolDNS) {
		fprintf(stderr, "%s only applies to dns\n",
			kOptDNSDomainName);
	}
	else if (params->dns.domain_name != NULL) {
		fprintf(stderr,	"%s specified multiple times\n",
			kOptDNSDomainName);
	}
	else {
		params->dns.domain_name = arg;
		success = TRUE;
	}
	return (success);
}

static Boolean
ProtocolParamsValidateIPv4(ProtocolParamsRef params)
{
	IPv4ConfigMethod 	method = params->ipv4.config_method;
	Boolean			success = FALSE;

	if (method != kIPv4ConfigMethodDHCP
	    && params->ipv4.dhcp_client_id != NULL) {
		fprintf(stderr,
			"%s not valid with %s\n",
			kOptDHCPClientID,
			IPv4ConfigMethodGetString(method));
		goto done;
	}
	if (method != kIPv4ConfigMethodManual
	    && params->ipv4.router.s_addr != 0) {
		fprintf(stderr,
			"%s not valid with %s\n",
			kOptRouter,
			IPv4ConfigMethodGetString(method));
		goto done;
	}
	switch (method) {
	case kIPv4ConfigMethodNone:
		fprintf(stderr,
			"%s must be specified\n",
			kOptConfigMethod);
		break;
	case kIPv4ConfigMethodInform:
	case kIPv4ConfigMethodManual:
		if (params->ipv4.address.s_addr == 0) {
			fprintf(stderr,
				"%s requires %s\n",
				IPv4ConfigMethodGetString(method),
				kOptAddress);
			break;
		}
		success = TRUE;
		break;
	case kIPv4ConfigMethodDHCP:
	case kIPv4ConfigMethodLinkLocal:
		if (params->ipv4.address.s_addr != 0) {
			fprintf(stderr,
				"%s not valid with %s\n",
				kOptAddress,
				IPv4ConfigMethodGetString(method));
			break;
		}
		if (params->ipv4.subnet_mask.s_addr != 0) {
			fprintf(stderr,
				"%s not valid with %s\n",
				kOptSubnetMask,
				IPv4ConfigMethodGetString(method));
			break;
		}
		success = TRUE;
		break;
	}
 done:
	return (success);
	
}

static Boolean
ProtocolParamsValidateIPv6(ProtocolParamsRef params)
{
	IPv6ConfigMethod 	method = params->ipv6.config_method;
	Boolean			success = FALSE;

	if (method != kIPv6ConfigMethodManual
	    && !IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.router)) {
		fprintf(stderr,
			"%s not valid with %s\n",
			kOptRouter,
			IPv6ConfigMethodGetString(method));
		goto done;
	}
	switch (params->ipv6.config_method) {
	case kIPv6ConfigMethodNone:
		fprintf(stderr,
			"%s must be specified\n",
			kOptConfigMethod);
		break;
	case kIPv6ConfigMethodManual:
		if (IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.address)) {
			fprintf(stderr,
				"%s requires %s\n",
				IPv6ConfigMethodGetString(method),
				kOptAddress);
			break;
		}
		success = TRUE;
		break;
	case kIPv6ConfigMethodAutomatic:
	case kIPv6ConfigMethodLinkLocal:
		success = TRUE;
		break;
	}

 done:
	return (success);
}

static Boolean
ProtocolParamsValidate(ProtocolParamsRef params)
{
	Boolean		success = FALSE;

	switch (params->protocol) {
	case kProtocolIPv4:
		success = ProtocolParamsValidateIPv4(params);
		break;
	case kProtocolIPv6:
		success = ProtocolParamsValidateIPv6(params);
		break;
	case kProtocolDNS:
		if (params->dns.addresses == NULL) {
			fprintf(stderr,
				"dns requires at least one address\n");
			break;
		}
		success = TRUE;
		break;
	case kProtocolNone:
		break;
	}
	return (success);
}

static void
print_invalid_interface_type(const char * arg)
{
	fprintf(stderr,
		"Invalid %s '%s', must be 'vlan' or 'bridge'\n",
		kOptInterfaceType, arg);
}

static void
print_invalid_protocol(const char * arg)
{
	fprintf(stderr,
		"Invalid protocol '%s', must be one of ipv4, ipv6, or dns\n",
		arg);
}

static Boolean
ProtocolParamsGet(ProtocolParamsRef params,
		  int argc, char * * argv)
{
	int		ch;
	int		opti;
	Boolean		protocol_done = FALSE;
	int		start_optind = optind;

	ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, &opti);
	if (ch == -1) {
		goto done;
	}
	switch (ch) {
	case 0:
		fprintf(stderr,
			"--%s must be specified before -%c or -%c\n",
			longopts[opti].name, OPT_INTERFACE, OPT_SERVICE);
		command_specific_help();

	case OPT_PROTOCOL:
		params->protocol = ProtocolFromString(optarg);
		if (params->protocol == kProtocolNone) {
			print_invalid_protocol(optarg);
			command_specific_help();
		}
		break;
	case OPT_DEFAULT_CONFIG:
		params->default_configuration = TRUE;
		protocol_done = TRUE;
		goto done;
	default:
		break;
	}
	if (params->protocol == kProtocolNone) {
		fprintf(stderr, "protocol must first be specified\n");
		command_specific_help();
	}
	while (!protocol_done) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, &opti);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case 0:
			fprintf(stderr,
				"--%s must be specified before -%c or -%c\n",
				longopts[opti].name, OPT_INTERFACE, OPT_SERVICE);
			command_specific_help();
		case OPT_ADDRESS:
			success = ProtocolParamsAddAddress(params, optarg);
			break;
		case OPT_DNS_SEARCH_DOMAINS:
			success = ProtocolParamsAddSearchDomain(params, optarg);
			break;
		case OPT_DNS_DOMAIN_NAME:
			success = ProtocolParamsAddDomainName(params, optarg);
			break;
		case OPT_CONFIG_METHOD:
			success = ProtocolParamsAddConfigMethod(params, optarg);
			break;
		case OPT_DHCP_CLIENT_ID:
			success = ProtocolParamsAddDHCPClientID(params, optarg);
			break;
		case OPT_NAME:
		case OPT_INTERFACE:
		case OPT_SERVICE:
			fprintf(stderr,
				"%s, %s, and %s may only be specified once\n",
				kOptInterface, kOptService, kOptName);
			break;
		case OPT_SUBNET_MASK:
			success = ProtocolParamsAddSubnetMask(params, optarg);
			break;
		case OPT_ROUTER:
			success = ProtocolParamsAddRouter(params, optarg);
			break;
		case OPT_PROTOCOL:
			/* we've moved onto the next protocol */
			protocol_done = TRUE;
			optind -= 2; /* backtrack */
			success = TRUE;
			break;
		case OPT_DEFAULT_CONFIG:
			params->default_configuration = TRUE;
			success = TRUE;
			break;
		default:
			fprintf(stderr, "Invalid option -%c\n", ch);
			command_specific_help();
		}
		if (!success) {
			exit(EX_USAGE);
		}
	}
	if (!ProtocolParamsValidate(params)) {
		exit(EX_USAGE);
	}
	if (optind != start_optind) {
		protocol_done = TRUE;
	}

 done:
	return (protocol_done);
}

static CFDictionaryRef
ProtocolParamsCopyIPv4(ProtocolParamsRef params)
{
	CFMutableDictionaryRef	config;
	CFStringRef		config_method;

	config_method = IPv4ConfigMethodGetCFString(params->ipv4.config_method);
	config = dict_create();
	CFDictionarySetValue(config, kSCPropNetIPv4ConfigMethod,
			     config_method);
	if (params->ipv4.address.s_addr != 0) {
		my_CFDictionarySetIPAddressAsArrayValue(config,
							kSCPropNetIPv4Addresses,
							AF_INET,
							&params->ipv4.address);
		if (params->ipv4.subnet_mask.s_addr != 0) {
			my_CFDictionarySetIPAddressAsArrayValue(config,
								kSCPropNetIPv4SubnetMasks,
								AF_INET,
								&params->ipv4.subnet_mask);
		}
		if (params->ipv4.router.s_addr != 0) {
			my_CFDictionarySetIPAddress(config,
						    kSCPropNetIPv4Router,
						    AF_INET,
						    &params->ipv4.router);
		}
	}
	if (params->ipv4.dhcp_client_id != NULL) {
		my_CFDictionarySetCString(config,
					  kSCPropNetIPv4DHCPClientID,
					  params->ipv4.dhcp_client_id);
	}
	return (config);
}

static CFDictionaryRef
ProtocolParamsCopyIPv6(ProtocolParamsRef params)
{
	CFMutableDictionaryRef	config;
	CFStringRef		config_method;

	config_method = IPv6ConfigMethodGetCFString(params->ipv6.config_method);
	config = dict_create();
	CFDictionarySetValue(config, kSCPropNetIPv6ConfigMethod,
			     config_method);
	if (!IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.address)) {
		my_CFDictionarySetIPAddressAsArrayValue(config,
							kSCPropNetIPv6Addresses,
							AF_INET6,
							&params->ipv6.address);
		my_CFDictionarySetNumberAsArrayValue(config,
						     kSCPropNetIPv6PrefixLength,
						     64);
		if (!IN6_IS_ADDR_UNSPECIFIED(&params->ipv6.router)) {
			my_CFDictionarySetIPAddress(config,
						    kSCPropNetIPv6Router,
						    AF_INET6,
						    &params->ipv6.router);
		}
	}
	return (config);
}

static CFDictionaryRef
ProtocolParamsCopyDNS(ProtocolParamsRef params)
{
	CFMutableDictionaryRef	config;

	if (params->dns.addresses == NULL) {
		fprintf(stderr, "DNS requires addresses\n");
		return (NULL);
	}
	config = dict_create();
	CFDictionarySetValue(config,
			     kSCPropNetDNSServerAddresses,
			     params->dns.addresses);
	if (params->dns.search_domains != NULL) {
		CFDictionarySetValue(config,
				     kSCPropNetDNSSearchDomains,
				     params->dns.search_domains);
	}
	if (params->dns.domain_name != NULL) {
		my_CFDictionarySetCString(config,
					  kSCPropNetDNSDomainName,
					  params->dns.domain_name);
	}
	return (config);
}

static Boolean
ProtocolParamsApply(ProtocolParamsRef params, networkSetupRef setup)
{
	CFDictionaryRef		config = NULL;
	Boolean			success = FALSE;
	CFStringRef		type;

	if (params->default_configuration) {
		if (setup->use_manager) {
			SCNSServiceRef	service = setup->manager.service;

			SCNSServiceUseDefaultProtocolEntities(service);
		}
		else if (!service_establish_default(setup->prefs.service)) {
			fprintf(stderr,
				"Failed to establish default configuration\n");
			goto done;
		}
		success = TRUE;
		goto done;
	}
	type = ProtocolGetCFString(params->protocol);
	if (type == NULL) {
		fprintf(stderr, "internal error: ProtocolGetCFString failed\n");
		goto done;
	}
	switch (params->protocol) {
	case kProtocolIPv4:
		config = ProtocolParamsCopyIPv4(params);
		break;
	case kProtocolIPv6:
		config = ProtocolParamsCopyIPv6(params);
		break;
	case kProtocolDNS:
		config = ProtocolParamsCopyDNS(params);
		break;
	case kProtocolNone:
		break;
	}
	if (config == NULL) {
		goto done;
	}
	if (setup->use_manager) {
		success = SCNSServiceSetProtocolEntity(setup->manager.service,
						       type, config);
	}
	else {
		SCNetworkProtocolRef	protocol;

		protocol = service_copy_protocol(setup->prefs.service, type);
		if (protocol == NULL) {
			fprintf(stderr, "failed to add protocol\n");
			goto done;
		}
		success = SCNetworkProtocolSetConfiguration(protocol, config);
		CFRelease(protocol);
	}

 done:
	return (success);
}

/*
 * Function: do_add_set
 * Purpose:
 *   Add or set the service configuration for an interface.
 */
static void
do_add_set(int argc, char * argv[])
{
	Boolean			by_interface = FALSE;
	categoryOptions		cat_opt;
	Boolean			changed = FALSE;
	int			ch;
	Boolean			have_S_or_i = FALSE;
	Boolean			have_service = FALSE;
	ProtocolParams		params;
	CFStringRef		name = NULL;
	const char *		name_c = "";
	CFStringRef		new_service_name = NULL;
	int			opti;
	const char *		optname;
	QoSMarkingOptions	qos;
	int			save_optind;
	networkSetup		setup;
	CFStringRef		str;
	Boolean			success = FALSE;

	networkSetupClear(&setup);
	categoryOptionsInit(&cat_opt);
	QoSMarkingOptionsInit(&qos);
	while (TRUE) {
		ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, &opti);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case 0:
			optname = longopts[opti].name;
			if (have_S_or_i) {
				fprintf(stderr,
					"--%s must be specified first",
					optname);
				command_specific_help();
			}
			if (strcmp(optname, kOptUseSettingsManager) == 0) {
				if (setup.manager.manager != NULL) {
					fprintf(stderr,
						"--%s already specified\n",
						kOptUseSettingsManager);
					command_specific_help();
				}
				networkSetupInitialize(&setup, TRUE);
			}
			else if (categoryOptionsAdd(&cat_opt, optname,
						    optarg, &success)) {
				if (!success) {
					command_specific_help();
				}
			}
			else if (QoSMarkingOptionsAdd(&qos, optname,
						      optarg, &success)) {
				if (!success) {
					command_specific_help();
				}
			}
			else {
				fprintf(stderr,
					"--%s not supported\n",
					optname);
				command_specific_help();
			}
			break;
		case OPT_HELP:
			command_specific_help();
			
		case OPT_INTERFACE:
		case OPT_SERVICE:
			if (!categoryOptionsAreValid(&cat_opt)) {
				command_specific_help();
			}
			if (setup.manager.manager == NULL) {
				networkSetupInitialize(&setup, FALSE);
			}
			have_S_or_i = TRUE;
			name_c = optarg;
			name = my_CFStringCreate(name_c);
			by_interface = (ch == OPT_INTERFACE);
			if (G_command == kCommandSet
			    && copy_setup_service(&setup, &cat_opt,
						  name, name_c,
						  by_interface)) {
				have_service = TRUE;
				break;
			}
			setup.netif = copy_available_interface(name, name_c);
			if (setup.netif == NULL) {
				break;
			}
			have_service = create_setup_service(&setup, &cat_opt);
			break;
		default:
			fprintf(stderr,
				"Either -%c or -%c must first be specified\n",
				OPT_INTERFACE, OPT_SERVICE);
			command_specific_help();
		}
		if (have_S_or_i) {
			break;
		}
	}
	__SC_CFRELEASE(name);
	if (!have_S_or_i) {
		fprintf(stderr,
			"Either -%c or -%c must be specified\n",
			OPT_INTERFACE, OPT_SERVICE);
		command_specific_help();
	}
	if (setup.netif == NULL) {
		fprintf(stderr, "Can't find %s\n", name_c);
		exit(EX_UNAVAILABLE);
	}
	if (!have_service) {
		fprintf(stderr, "Can't configure %s\n", name_c);
		exit(EX_UNAVAILABLE);
	}
	save_optind = optind;
	ch = getopt_long(argc, argv, ADD_SET_OPTSTRING, longopts, NULL);
	switch (ch) {
	case OPT_INTERFACE:
	case OPT_SERVICE:
		fprintf(stderr,
			"%s and %s may only be specified once\n",
			kOptInterface, kOptService);
		exit(EX_USAGE);

	case OPT_NAME:
		if (setup.use_manager) {
			fprintf(stderr, "-%c can't be used with --%s\n",
				OPT_NAME, kOptUseSettingsManager);
			command_specific_help();
		}
		new_service_name = my_CFStringCreate(optarg);
		break;
	default:
		/* backtrack */
		optind = save_optind;
		break;
	}
	ProtocolParamsInit(&params);
	while (ProtocolParamsGet(&params, argc, argv)) {
		/* process params */
		changed = TRUE;
		if (!ProtocolParamsApply(&params, &setup)) {
			exit(EX_SOFTWARE);
		}
		ProtocolParamsRelease(&params);
		ProtocolParamsInit(&params);
	}
	if (!setup.use_manager && new_service_name != NULL) {
		changed = SCNetworkServiceSetName(setup.prefs.service,
						  new_service_name);
		if (!changed) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to set service name '%@': %s\n"),
				new_service_name, SCErrorString(SCError()));
			exit(EX_USAGE);
		}
		__SC_CFRELEASE(new_service_name);
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (QoSMarkingOptionsSpecified(&qos)) {
		if (!QoSMarkingOptionsApply(&qos, &setup, &cat_opt)) {
			exit(EX_SOFTWARE);
		}
		if (G_command == kCommandSet) {
			changed = TRUE;
		}
	}
	if (!changed) {
		command_specific_help();
	}
	setup_commit_apply(&setup);
	str = setup_service_copy_summary(&setup);
	SCPrint(TRUE, stdout, CFSTR("%s %@\n"),
		CommandGetString(G_command), str);
	CFRelease(str);
	networkSetupRelease(&setup);
	categoryOptionsFree(&cat_opt);
	QoSMarkingOptionsFree(&qos);
	return;
}

/**
 ** Remove
 **/

#define REMOVE_ENABLE_DISABLE_OPTSTRING	"hi:p:S:"

/*
 * Function: do_remove_enable_disable
 * Purpose:
 *   Remove, enable, or disable a network service, or any of its
 *   protocols.
 */
static void
do_remove_enable_disable(int argc, char * argv[])
{
	categoryOptions		cat_opt;
	Boolean			by_interface = FALSE;
	Boolean			changed = FALSE;
	int			ch;
	Boolean			have_service = FALSE;
	Boolean			have_S_or_i = FALSE;
	CFStringRef		name;
	const char *		name_c = "";
	int			opti;
	const char *		optname;
	Protocol		protocol;
	SCNetworkServiceRef	service;
	networkSetup		setup;
	CFStringRef		str;
	CFStringRef		type;

	networkSetupClear(&setup);
	categoryOptionsInit(&cat_opt);
	while (TRUE) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, REMOVE_ENABLE_DISABLE_OPTSTRING,
				 longopts, &opti);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case 0:
			optname = longopts[opti].name;
			if (G_command != kCommandRemove) {
				fprintf(stderr, "--%s not allowed with %s\n",
					optname,
					CommandGetString(G_command));
				command_specific_help();
			}
			if (have_S_or_i) {
				fprintf(stderr,
					"--%s must be specified first",
					optname);
				command_specific_help();
			}
			if (strcmp(optname, kOptUseSettingsManager) == 0) {
				if (setup.manager.manager != NULL) {
					fprintf(stderr,
						"--%s already specified\n",
						kOptUseSettingsManager);
					command_specific_help();
				}
				networkSetupInitialize(&setup, TRUE);
			}
			else if (categoryOptionsAdd(&cat_opt, optname,
						    optarg, &success)) {
				if (!success) {
					command_specific_help();
				}
			}
			else {
				fprintf(stderr,
					"--%s not supported\n",
					optname);
				command_specific_help();
			}
			break;
		case OPT_HELP:
			command_specific_help();

		case OPT_INTERFACE:
		case OPT_SERVICE:
			if (!categoryOptionsAreValid(&cat_opt)) {
				command_specific_help();
			}
			if (setup.manager.manager == NULL) {
				networkSetupInitialize(&setup, FALSE);
			}
			have_S_or_i = TRUE;
			name_c = optarg;
			name = my_CFStringCreate(name_c);
			by_interface = (ch == OPT_INTERFACE);
			have_service
				= copy_setup_service(&setup, &cat_opt,
						     name, name_c, by_interface);
			__SC_CFRELEASE(name);
			break;
		default:
			fprintf(stderr,
				"Either -%c or -%c must first be specified\n",
				OPT_INTERFACE, OPT_SERVICE);
			command_specific_help();
		}
		if (have_S_or_i) {
			break;
		}
	}
	if (!have_S_or_i) {
		fprintf(stderr,
			"Either -%c or -%c must be specified\n",
			OPT_INTERFACE, OPT_SERVICE);
		command_specific_help();
	}
	if (!have_service) {
		fprintf(stderr, "Can't find %s\n", name_c);
		exit(EX_UNAVAILABLE);
	}
	service = setup.use_manager ? NULL : setup.prefs.service;
	while (TRUE) {
		Boolean			success = FALSE;

		ch = getopt_long(argc, argv, REMOVE_ENABLE_DISABLE_OPTSTRING,
				 longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_INTERFACE:
		case OPT_SERVICE:
			fprintf(stderr,
				"interface/service may only "
				"be specified once\n");
			break;
		case OPT_PROTOCOL:
			protocol = ProtocolFromString(optarg);
			if (protocol == kProtocolNone) {
				print_invalid_protocol(optarg);
				command_specific_help();
			}
			type = ProtocolGetCFString(protocol);
			if (G_command == kCommandRemove) {
				success = service_remove_protocol(&setup,
								  type);
			}
			else if (service == NULL) {
				fprintf(stderr,
					"%s: service is NULL, internal error\n",
					__func__);
				exit(EX_SOFTWARE);
			}
			else if (G_command == kCommandEnable) {
				success = service_enable_protocol(service, type);
			}
			else if (G_command == kCommandDisable) {
			      success = service_disable_protocol(service,
								 type);
			}
			if (!success) {
				fprintf(stderr,
					"Failed to %s protocol '%s'\n",
					CommandGetString(G_command),
					ProtocolGetString(protocol));
				exit(EX_USAGE);
			}
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
		changed = TRUE;
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-lpine arguments\n");
		exit(EX_USAGE);
	}
	if (!changed) {
		Boolean			success = FALSE;

		if (G_command == kCommandRemove) {
			success = remove_setup_service(&setup, &cat_opt);
		}
		else if (service == NULL) {
			fprintf(stderr,
				"%s: service is NULL, internal error\n",
				__func__);
		}
		else if (G_command == kCommandEnable) {
			success = enable_service(service);
		}
		else if (G_command == kCommandDisable) {
			success	= disable_service(service);
		}
		if (!success) {
			exit(EX_SOFTWARE);
		}
	}
	str = setup_service_copy_summary(&setup);
	SCPrint(TRUE, stdout, CFSTR("%s %@\n"),
		CommandGetString(G_command), str);
	CFRelease(str);
	setup_commit_apply(&setup);
	networkSetupRelease(&setup);
	categoryOptionsFree(&cat_opt);
}

/**
 ** Show
 **/
static void
protocol_populate_summary_dictionary(SCNetworkProtocolRef p,
				     CFMutableDictionaryRef dict)
{
	CFDictionaryRef		config;
	CFStringRef		type;

	config = SCNetworkProtocolGetConfiguration(p);
	if (config == NULL) {
		return;
	}
	type = SCNetworkProtocolGetProtocolType(p);
	CFDictionarySetValue(dict, CFSTR("type"), type);
	CFDictionarySetValue(dict, CFSTR("configuration"), config);
	if (!SCNetworkProtocolGetEnabled(p)) {
		CFDictionarySetValue(dict, CFSTR("enabled"), kCFBooleanFalse);
	}
}

static void
interface_populate_summary_dictionary(SCNetworkInterfaceRef netif,
				      CFMutableDictionaryRef dict)
{
	CFStringRef		name;
	CFStringRef		type;

	name = SCNetworkInterfaceGetBSDName(netif);
	if (name != NULL) {
		CFDictionarySetValue(dict, CFSTR("bsd_name"), name);
	}
	name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
	if (name != NULL) {
		CFDictionarySetValue(dict, CFSTR("name"), name);
	}
	if (CFDictionaryGetCount(dict) == 0) {
		SCNetworkInterfaceRef	child;

		child = SCNetworkInterfaceGetInterface(netif);
		if (child != NULL) {
			name = SCNetworkInterfaceGetBSDName(child);
			if (name != NULL) {
				CFDictionarySetValue(dict, CFSTR("bsd_name"),
						     name);
			}
			name = SCNetworkInterfaceGetLocalizedDisplayName(netif);
			if (name != NULL) {
				CFDictionarySetValue(dict, CFSTR("name"), name);
			}
		}
	}
	type = SCNetworkInterfaceGetInterfaceType(netif);
	CFDictionarySetValue(dict, CFSTR("type"), type);
}

static void
service_populate_summary_dictionary(SCNetworkServiceRef service,
				    CFMutableDictionaryRef dict)
{
	CFArrayRef		list;
	SCNetworkInterfaceRef	netif;
	CFMutableDictionaryRef	sub_dict;

	dict_set_val(dict, CFSTR("name"),
		     SCNetworkServiceGetName(service));
	dict_set_val(dict, CFSTR("serviceID"),
		     SCNetworkServiceGetServiceID(service));
	if (!SCNetworkServiceGetEnabled(service)) {
		CFDictionarySetValue(dict,
				     CFSTR("enabled"),
				     kCFBooleanFalse);
		
	}
	netif = SCNetworkServiceGetInterface(service);
	sub_dict = dict_create();
	interface_populate_summary_dictionary(netif, sub_dict);
	if (CFDictionaryGetCount(sub_dict) != 0) {
		CFDictionarySetValue(dict,
				     CFSTR("interface"),
				     sub_dict);
	}
	__SC_CFRELEASE(sub_dict);
	list = SCNetworkServiceCopyProtocols(service);
	if (list != NULL) {
		CFMutableArrayRef	descriptions;

		descriptions = array_create();
		for (CFIndex i = 0, count = CFArrayGetCount(list);
		     i < count; i++) {
			SCNetworkProtocolRef	p;

			sub_dict = dict_create();
			p = (SCNetworkProtocolRef)
				CFArrayGetValueAtIndex(list, i);
			protocol_populate_summary_dictionary(p, sub_dict);
			if (CFDictionaryGetCount(sub_dict) != 0) {
				CFArrayAppendValue(descriptions,
						   sub_dict);
			}
			CFRelease(sub_dict);
		}
		if (CFArrayGetCount(descriptions) > 0) {
			CFDictionarySetValue(dict,
					     CFSTR("protocols"),
					     descriptions);
			CFRelease(descriptions);
		}
		CFRelease(list);
	}
}

static void
show_interface(SCNetworkInterfaceRef netif, Boolean verbose)
{
	CFTypeRef	descr;

	if (verbose) {
		CFMutableDictionaryRef	dict;

		descr = dict = dict_create();
		interface_populate_summary_dictionary(netif, dict);
	}
	else {
		descr = (CFTypeRef)interface_copy_summary(netif);
	}
	SCPrint(TRUE, stdout, CFSTR("%@\n"), descr);
	CFRelease(descr);
}

static void
show_service(SCNetworkServiceRef service, Boolean verbose)
{
	CFTypeRef	descr;

	if (verbose) {
		CFMutableDictionaryRef	dict;

		descr = dict = dict_create();
		service_populate_summary_dictionary(service, dict);
	}
	else {
		descr = (CFTypeRef)service_copy_summary(service);
	}
	SCPrint(TRUE, stdout, CFSTR("%@\n"), descr);
	CFRelease(descr);
}

static void
find_and_show_service(SCNetworkSetRef set, const char * arg, Boolean verbose)
{
	CFStringRef		name;
	SCNetworkServiceRef	service = NULL;

	name = my_CFStringCreate(arg);
	service = copy_configured_service_in_set(set, name, FALSE);
	__SC_CFRELEASE(name);
	if (service == NULL) {
		fprintf(stderr, "Can't find %s\n", arg);
		exit(EX_UNAVAILABLE);
	}
	show_service(service, verbose);
	CFRelease(service);
}

static void
find_and_show_interface(SCNetworkSetRef set, const char * arg, Boolean verbose)
{
	CFStringRef		name;
	SCNetworkInterfaceRef	netif;
	SCNetworkServiceRef	service = NULL;

	name = my_CFStringCreate(arg);
	if (set != NULL) {
		service = copy_configured_service_in_set(set, name, TRUE);
	}
	if (service != NULL) {
		netif = SCNetworkServiceGetInterface(service);
		if (netif != NULL) {
			CFRetain(netif);
		}
	}
	else {
		netif = copy_available_interface(name, arg);
	}
	__SC_CFRELEASE(name);
	__SC_CFRELEASE(service);
	if (netif == NULL) {
		fprintf(stderr, "Can't find %s\n", arg);
		exit(EX_UNAVAILABLE);
	}
	show_interface(netif, verbose);
	CFRelease(netif);
}

static void
show_all_interfaces(Boolean verbose)
{
	CFArrayRef	list;

	list = SCNetworkInterfaceCopyAll();
	if (list == NULL) {
		return;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(list); i < count; i++) {
		CFTypeRef		descr;
		SCNetworkInterfaceRef	netif = CFArrayGetValueAtIndex(list, i);

		if (verbose) {
			CFMutableDictionaryRef	dict;

			dict = dict_create();
			interface_populate_summary_dictionary(netif, dict);
			descr = dict;
		}
		else {
			CFStringRef	str;

			str = interface_copy_summary(netif);
			descr = str;
		}
		SCPrint(TRUE, stdout, CFSTR("%d. %@\n"), (int)(i + 1), descr);
		CFRelease(descr);
	}
	CFRelease(list);
}

static void
show_all_services(SCNetworkSetRef set, Boolean display_set_name, Boolean verbose)
{
	CFArrayRef	services = NULL;

	if (display_set_name) {
		CFStringRef	name;
		CFStringRef	ID;
		name = SCNetworkSetGetName(set);
		ID = SCNetworkSetGetSetID(set);
		printf("Set");
		if (name != NULL || ID != NULL) {
			if (name != NULL) {
				SCPrint(TRUE, stdout, CFSTR(" \"%@\""), name);
			}
			if (ID != NULL) {
				SCPrint(TRUE, stdout, CFSTR(" (%@)"), ID);
			}
		}
		else {
			printf(" ?");
		}
		printf("\n");
	}
	services = copy_sorted_services(set);
	if (services == NULL) {
		printf("No services\n");
		return;
	}
	for (CFIndex i = 0, count = CFArrayGetCount(services);
	     i < count; i++) {
		CFTypeRef		descr;
		SCNetworkServiceRef	s;

		s = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
		if (verbose) {
			CFMutableDictionaryRef	dict;

			dict = dict_create();
			service_populate_summary_dictionary(s, dict);
			descr = dict;
		}
		else {
			CFStringRef		str;

			str = service_copy_summary(s);
			descr = str;
		}
		SCPrint(TRUE, stdout, CFSTR("%d. %@\n"), (int)(i + 1), descr);
		CFRelease(descr);
	}
	__SC_CFRELEASE(services);

}

static CFArrayRef
get_order_array_from_values(CFDictionaryRef values, CFStringRef order_key)
{
	CFDictionaryRef	dict;
	CFArrayRef		order_array = NULL;

	dict = isA_CFDictionary(CFDictionaryGetValue(values, order_key));
	if (dict) {
		order_array = CFDictionaryGetValue(dict,
						   kSCPropNetServiceOrder);
		order_array = isA_CFArray(order_array);
		if (order_array && CFArrayGetCount(order_array) == 0) {
			order_array = NULL;
		}
	}
	return (order_array);
}

#define ARBITRARILY_LARGE_NUMBER	(1000 * 1000)

static int
lookup_order(CFArrayRef order, CFStringRef serviceID)
{
	CFIndex 	count;
	int		i;

	if (order == NULL)
		goto done;

	count = CFArrayGetCount(order);
	for (i = 0; i < count; i++) {
		CFStringRef	sid = CFArrayGetValueAtIndex(order, i);

		if (CFEqual(sid, serviceID))
			return (i);
	}
 done:
	return (ARBITRARILY_LARGE_NUMBER);
}

static CFComparisonResult
compare_serviceIDs(const void *val1, const void *val2, void *context)
{
	CFArrayRef		order_array = (CFArrayRef)context;
	int			rank1;
	int			rank2;

	rank1 = lookup_order(order_array, (CFStringRef)val1);
	rank2 = lookup_order(order_array, (CFStringRef)val2);
	if (rank1 == rank2)
		return (kCFCompareEqualTo);
	if (rank1 < rank2)
		return (kCFCompareLessThan);
	return (kCFCompareGreaterThan);
}

#define kServiceID	CFSTR("ServiceID")
#define __SERVICE	CFSTR("__SERVICE")

static CFArrayRef
copy_services_from_info(CFDictionaryRef info, CFArrayRef order_array,
			CFDictionaryRef * ret_configured)
{
	CFIndex			info_count;
	const void * *		keys;
	CFMutableArrayRef	service_list = NULL;
	CFMutableArrayRef	serviceIDs;
	CFIndex			serviceIDs_count;
	CFRange			serviceIDs_range = { 0, 0 };
	unsigned long		size;
	CFMutableDictionaryRef	setup_dict;
	CFMutableDictionaryRef	state_dict;
	const void * *		values;

	*ret_configured = NULL;

	/* if there are no values, we're done */
	info_count = CFDictionaryGetCount(info);
	if (info_count == 0) {
		return (NULL);
	}
	state_dict = dict_create();
	setup_dict = dict_create();
	serviceIDs = array_create();
	size = ((unsigned long)info_count * 2) * sizeof(*keys);
	keys = (const void * *)malloc(size);
	values = keys + info_count;
	CFDictionaryGetKeysAndValues(info,
				     (const void * *)keys,
				     (const void * *)values);
	for (CFIndex i = 0; i < info_count; i++) {
		CFArrayRef		arr;
		Boolean			is_state;
		CFIndex			count;
		CFStringRef		key = (CFStringRef)keys[i];
		CFStringRef		proto;
		UnConst			ptr;
		CFMutableDictionaryRef	service_dict;
		CFStringRef		serviceID;
		CFDictionaryRef		value = (CFDictionaryRef)values[i];
		CFMutableDictionaryRef	which_dict;

		if (CFStringHasPrefix(key, CFSTR("State:/Network/Service/"))) {
			is_state = TRUE;
		}
		else if (CFStringHasPrefix(key,
					   CFSTR("Setup:/Network/Service/"))) {
			is_state = FALSE;
		}
		else {
			continue;
		}
		/* {State,Setup}:/Network/Service/<serviceID>[/<entity>] */
		arr = CFStringCreateArrayBySeparatingStrings(NULL, key,
							     CFSTR("/"));
		if (arr == NULL) {
			continue;
		}
		count = CFArrayGetCount(arr);
		if (count < 4) {
			CFRelease(arr);
			continue;
		}
		serviceID = CFArrayGetValueAtIndex(arr, 3);
		if (count > 4) {
			proto = CFArrayGetValueAtIndex(arr, 4);
		}
		else {
			proto = __SERVICE;
		}
		if (isA_CFDictionary(value) == NULL) {
			SCPrint(TRUE, stderr,
				CFSTR("key %@ value is not a dictionary %@\n"),
				key, value);
		}
		/* only accumulate serviceIDs for active services */
		if (is_state
		    && !CFArrayContainsValue(serviceIDs, serviceIDs_range,
					     serviceID)) {
			CFArrayAppendValue(serviceIDs, serviceID);
			serviceIDs_range.length++;
		}
		which_dict = is_state ? state_dict : setup_dict;
		ptr.const_ptr = CFDictionaryGetValue(which_dict, serviceID);
		service_dict = (CFMutableDictionaryRef)ptr.non_const_ptr;
		if (service_dict == NULL) {
			service_dict = dict_create();
			CFDictionarySetValue(which_dict, serviceID,
					     service_dict);
			CFDictionarySetValue(service_dict, kServiceID,
					     serviceID);
			CFRelease(service_dict);
		}
		/* save protocol specific dictionary */
		CFDictionarySetValue(service_dict, proto, value);
		CFRelease(arr);
	}
	free(keys);
	serviceIDs_count = CFArrayGetCount(serviceIDs);
	if (serviceIDs_count == 0) {
		__SC_CFRELEASE(serviceIDs);
		__SC_CFRELEASE(setup_dict);
	}
	else {
		if (order_array != NULL) {
			UnConst		ptr;

			ptr.const_ptr = order_array;
			/* use service order to sort the serviceIDs */
			CFArraySortValues(serviceIDs, serviceIDs_range,
					  compare_serviceIDs,
					  ptr.non_const_ptr);
		}
		/* transform dict[dict] into sorted array[dict] */
		service_list = array_create();
		for (CFIndex i = 0; i < serviceIDs_count; i++) {
			CFStringRef	serviceID;
			CFDictionaryRef	service_dict;

			serviceID = (CFStringRef)
				CFArrayGetValueAtIndex(serviceIDs, i);
			service_dict = CFDictionaryGetValue(state_dict,
							    serviceID);
			CFArrayAppendValue(service_list, service_dict);
		}
		*ret_configured = setup_dict;
	}
	__SC_CFRELEASE(serviceIDs);
	__SC_CFRELEASE(state_dict);
	return (service_list);
}

static CFArrayRef
copy_all_active_services(CFDictionaryRef * ret_configured)
{
	CFArrayRef		all_services = NULL; /* array of dict */
	CFMutableArrayRef	get_keys;
	CFMutableArrayRef	get_patterns;
	CFDictionaryRef		info;
	CFStringRef		key;
	CFArrayRef		order_array = NULL;
	CFStringRef		order_key;

	/* get State:/Network/Service/any/any */
	get_patterns = array_create();
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						kSCDynamicStoreDomainState,
						kSCCompAnyRegex,
						kSCCompAnyRegex);
	CFArrayAppendValue(get_patterns, key);
	CFRelease(key);

	/* get State:/Network/Service/any */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						kSCDynamicStoreDomainState,
						kSCCompAnyRegex,
						NULL);
	CFArrayAppendValue(get_patterns, key);
	CFRelease(key);

	/* get Setup:/Network/Service/any/any */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						kSCDynamicStoreDomainSetup,
						kSCCompAnyRegex,
						kSCCompAnyRegex);
	CFArrayAppendValue(get_patterns, key);
	CFRelease(key);

	/* get Setup:/Network/Service/any */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						kSCDynamicStoreDomainSetup,
						kSCCompAnyRegex,
						NULL);
	CFArrayAppendValue(get_patterns, key);
	CFRelease(key);

	/* get Setup:/Network/Global/IPv4 */
	get_keys = array_create();
	order_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						   kSCDynamicStoreDomainSetup,
						   kSCEntNetIPv4);
	CFArrayAppendValue(get_keys, order_key);

	/* get atomic snapshot */
	info = SCDynamicStoreCopyMultiple(NULL, get_keys, get_patterns);
	if (info != NULL) {
		/* grab the service order array */
		order_array = get_order_array_from_values(info, order_key);
	}

	all_services = copy_services_from_info(info, order_array,
					       ret_configured);
 	__SC_CFRELEASE(info);
	__SC_CFRELEASE(order_key);
	__SC_CFRELEASE(get_keys);
	__SC_CFRELEASE(get_patterns);
	if (all_services != NULL && CFArrayGetCount(all_services) == 0) {
		__SC_CFRELEASE(all_services);
		__SC_CFRELEASE(ret_configured);
	}
	return (all_services);
}

static void
append_ipv4_state_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef	router;

	CFStringAppend(str, CFSTR("\t"));
	if (CFDictionaryContainsKey(dict, kSCPropNetIPv4Addresses)) {
		CFStringAppend(str, CFSTR("inet "));
		append_array_values(str, dict, kSCPropNetIPv4Addresses);
	}
	if (CFDictionaryContainsKey(dict, kSCPropNetIPv4SubnetMasks)) {
		CFStringAppend(str, CFSTR(" netmask "));
		append_array_values(str, dict, kSCPropNetIPv4SubnetMasks);
	}
	router = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
	if (router != NULL) {
		CFStringAppendFormat(str, NULL,
				     CFSTR(" router %@"),
				     router);
	}
	CFStringAppend(str, CFSTR("\n"));
	return;
}

static void
append_ipv6_state_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef	router;

	CFStringAppend(str, CFSTR("\t"));
	if (CFDictionaryContainsKey(dict, kSCPropNetIPv6Addresses)) {
		CFStringAppend(str, CFSTR("inet6 "));
		append_array_values(str, dict, kSCPropNetIPv6Addresses);
	}
	router = CFDictionaryGetValue(dict, kSCPropNetIPv6Router);
	if (router != NULL) {
		CFStringAppendFormat(str, NULL,
				     CFSTR(" router %@"), router);
	}
	CFStringAppend(str, CFSTR("\n"));
	return;
}

static void
append_dns_state_descr(CFMutableStringRef str, CFDictionaryRef dict)
{
	CFStringRef	domain;

	CFStringAppend(str, CFSTR("\t"));
	if (CFDictionaryContainsKey(dict, kSCPropNetDNSServerAddresses)) {
		CFStringAppend(str, CFSTR("dns "));
		append_array_values(str, dict, kSCPropNetDNSServerAddresses);
	}
	domain = CFDictionaryGetValue(dict, kSCPropNetDNSDomainName);
	if (domain != NULL) {
		CFStringAppendFormat(str, NULL, CFSTR(" domain %@"),
				     domain);
	}
	if (CFDictionaryContainsKey(dict, kSCPropNetDNSSearchDomains)) {
		CFStringAppend(str, CFSTR(" search "));
		append_array_values(str, dict, kSCPropNetDNSSearchDomains);
	}
	CFStringAppend(str, CFSTR("\n"));
}

static void
append_service_descr(CFMutableStringRef str,
		     CFDictionaryRef ipv4_dict,
		     CFDictionaryRef ipv6_dict,
		     CFDictionaryRef dns_dict)
{
	if (ipv4_dict != NULL) {
		append_ipv4_state_descr(str, ipv4_dict);
	}
	if (ipv6_dict != NULL) {
		append_ipv6_state_descr(str, ipv6_dict);
	}
	if (dns_dict != NULL) {
		append_dns_state_descr(str, dns_dict);
	}
	return;
}

static CFDictionaryRef
copy_configured_services(SCNetworkSetRef set)
{
	CFIndex			count = 0;
	CFArrayRef		list;
	CFMutableDictionaryRef	ret_dict = NULL;

	list = SCNetworkSetCopyServices(set);
	if (list != NULL) {
		count = CFArrayGetCount(list);
	}
	if (count == 0) {
		goto done;
	}
	ret_dict = dict_create();
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkServiceRef	service;
		CFStringRef		serviceID;

		service = (SCNetworkServiceRef)
			CFArrayGetValueAtIndex(list, i);
		serviceID = SCNetworkServiceGetServiceID(service);
		CFDictionarySetValue(ret_dict, serviceID, service);
	}

 done:
	__SC_CFRELEASE(list);
	return (ret_dict);
}

static CFStringRef
dict_copy_service_summary(CFDictionaryRef setup_dict)
{
	CFDictionaryRef		dns;
	Boolean			hidden_configuration = FALSE;
	CFDictionaryRef		ifdict;
	CFDictionaryRef		ipv4;
	CFDictionaryRef		ipv6;
	CFStringRef		ifname = NULL;
	SCNetworkInterfaceRef	netif;
	CFStringRef		serviceID;
	CFDictionaryRef		service_dict;
	CFStringRef		service_name;
	CFMutableStringRef	str = NULL;

	serviceID = CFDictionaryGetValue(setup_dict, kServiceID);
	if (serviceID == NULL) {
		goto done;
	}
	service_dict = CFDictionaryGetValue(setup_dict, __SERVICE);
	if (service_dict == NULL) {
		goto done;
	}
	service_name = CFDictionaryGetValue(service_dict,
					    kSCPropUserDefinedName);
	ifdict = CFDictionaryGetValue(setup_dict, kSCEntNetInterface);
	if (ifdict == NULL) {
		goto done;
	}
	ipv4 = CFDictionaryGetValue(setup_dict, kSCEntNetIPv4);
	ipv6 = CFDictionaryGetValue(setup_dict, kSCEntNetIPv6);
	dns = CFDictionaryGetValue(setup_dict, kSCEntNetDNS);
	hidden_configuration
		= (CFDictionaryGetValue(ifdict,
			kSCNetworkInterfaceHiddenConfigurationKey) != NULL);
	ifname = CFDictionaryGetValue(ifdict, kSCPropNetInterfaceDeviceName);
	if (ifname == NULL) {
		goto done;
	}
	if (service_name == NULL) {
		service_name = CFDictionaryGetValue(ifdict,
						    kSCPropUserDefinedName);
		if (service_name == NULL) {
			service_name = CFSTR("Unknown");
		}
	}
	netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname, 0);
	str = CFStringCreateMutable(NULL, 0);
	CFStringAppend(str, service_name);
	CFStringAppendFormat(str, NULL, CFSTR(" (%@)"), serviceID);
	if (netif != NULL) {
		CFStringRef		if_summary;

		if_summary = interface_copy_summary(netif);
		CFRelease(netif);
		CFStringAppendFormat(str, NULL, CFSTR(" %@"), if_summary);
		CFRelease(if_summary);
	}
	if (hidden_configuration) {
		CFStringAppend(str, CFSTR(" [Hidden]"));
	}
	if (ipv4 != NULL || ipv6 != NULL || dns != NULL) {
		CFStringAppend(str, CFSTR("\n\t"));
		if (ipv4 != NULL) {
			CFStringAppend(str, CFSTR("IPv4={ "));
			append_ipv4_descr(str, ipv4);
			CFStringAppend(str, CFSTR(" } "));
		}
		if (ipv6 != NULL) {
			CFStringAppend(str, CFSTR("IPv6={ "));
			append_ipv6_descr(str, ipv6);
			CFStringAppend(str, CFSTR(" } "));
		}
		if (dns != NULL) {
			CFStringAppend(str, CFSTR("DNS={ "));
			append_dns_descr(str, dns);
			CFStringAppend(str, CFSTR(" } "));
		}
	}

 done:
	return (str);
}

static void
show_active_services(SCNetworkSetRef set, const char * name, Boolean verbose)
{
#pragma unused(verbose)
	CFArrayRef	active_services;
	CFDictionaryRef	setup_services;
	CFIndex		count;
	CFStringRef	name_cf = NULL;
	CFDictionaryRef services;

	active_services = copy_all_active_services(&setup_services);
	if (active_services == NULL) {
		fprintf(stderr, "No active services\n");
		return;
	}
	services = copy_configured_services(set);
	if (name != NULL) {
		name_cf = my_CFStringCreate(name);
	}
	count = CFArrayGetCount(active_services);
	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef		dict;
		CFDictionaryRef		dns;
		CFDictionaryRef		ipv4;
		CFDictionaryRef		ipv6;
		Boolean			match;
		CFStringRef		primary_rank = NULL;
		SCNetworkServiceRef	service = NULL;
		CFDictionaryRef		service_dict;
		CFStringRef		serviceID;
		CFDictionaryRef 	setup_dict = NULL;
		CFStringRef		service_summary = NULL;
		CFMutableStringRef 	str;
		CFStringRef		this_ifname;

		dict = (CFDictionaryRef)
			CFArrayGetValueAtIndex(active_services, i);
		serviceID = CFDictionaryGetValue(dict, kServiceID);
		service_dict = CFDictionaryGetValue(dict, __SERVICE);
		ipv4 = CFDictionaryGetValue(dict, kSCEntNetIPv4);
		ipv6 = CFDictionaryGetValue(dict, kSCEntNetIPv6);
		dns = CFDictionaryGetValue(dict, kSCEntNetDNS);
		if (ipv4 != NULL) {
			this_ifname
				= CFDictionaryGetValue(ipv4,
						       kSCPropInterfaceName);
		}
		else if (ipv6 != NULL) {
			this_ifname
				= CFDictionaryGetValue(ipv6,
						       kSCPropInterfaceName);
		}
		else {
			continue;
		}
		if (this_ifname == NULL) {
			continue;
		}
		if (services != NULL) {
			service = CFDictionaryGetValue(services, serviceID);
		}
		if (name_cf == NULL) {
			match = TRUE;
		}
		else if (CFEqual(name_cf, this_ifname)) {
			match = TRUE;
		}
		else if (service != NULL) {
			match = matchService(service, NULL, name_cf);
		}
		else {
			match = FALSE;
		}
		if (!match) {
			continue;
		}
		if (setup_services != NULL) {
			setup_dict = CFDictionaryGetValue(setup_services,
							  serviceID);
		}
		str = CFStringCreateMutable(NULL, 0);
		if (service_dict != NULL) {
			primary_rank = CFDictionaryGetValue(service_dict,
					kSCPropNetServicePrimaryRank);
			primary_rank = isA_CFString(primary_rank);
		}
		if (primary_rank != NULL) {
			CFStringAppendFormat(str, NULL,
					     CFSTR(" [%@]"), primary_rank);
		}
		CFStringAppend(str, CFSTR("\n"));
		append_service_descr(str, ipv4, ipv6, dns);
		if (service != NULL) {
			service_summary = service_copy_summary(service);
		}
		else if (setup_dict != NULL) {
			service_summary = dict_copy_service_summary(setup_dict);
		}
		if (service_summary != NULL) {
			SCPrint(TRUE, stdout,
				CFSTR("%@%@"), service_summary,
				str);
			CFRelease(service_summary);
		}
		else {
			SCPrint(TRUE, stdout,
				CFSTR("%@: %@%@"), this_ifname,
				serviceID, str);
		}
		CFRelease(str);
	}
	__SC_CFRELEASE(name_cf);
	__SC_CFRELEASE(active_services);
	__SC_CFRELEASE(setup_services);
	__SC_CFRELEASE(services);
	return;
}

static void
show_services_for_category(SCNetworkCategoryRef category,
			   CFStringRef category_value,
			   CFStringRef name,
			   Boolean verbose)
{
	CFIndex		count = 0;
	CFArrayRef	services;

	services = SCNetworkCategoryCopyServices(category, category_value);
	if (services != NULL) {
		count = CFArrayGetCount(services);
	}
	if (count == 0) {
		fprintf(stderr, "No services\n");
		goto done;
	}
	if (name != NULL) {
		SCNetworkServiceRef	service;
		
		service = copy_configured_service_in_list(services, name, FALSE);
		if (service == NULL) {
			fprintf(stderr, "No service\n");
		}
		else {
			SCPrint(TRUE, stdout, CFSTR("%@\n"), category_value);
			show_service(service, verbose);
			CFRelease(service);
		}
		goto done;
	}	
	SCPrint(TRUE, stdout, CFSTR("%@\n"), category_value);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkServiceRef	service;

		service = (SCNetworkServiceRef)
			CFArrayGetValueAtIndex(services, i);
		show_service(service, verbose);
	}

 done:
	if (services != NULL) {
		CFRelease(services);
	}
	return;
}

static void
show_category(SCNetworkCategoryRef category, CFStringRef name,
	      Boolean verbose)
{
	CFIndex		count = 0;
	CFArrayRef	values;

	values = SCNetworkCategoryCopyValues(category);
	if (values != NULL) {
		count = CFArrayGetCount(values);
	}
	if (count == 0) {
		goto done;
	}
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef	value;

		value = (CFStringRef)CFArrayGetValueAtIndex(values, i);
		show_services_for_category(category, value, name, verbose);
	}
 done:
	if (values != NULL) {
		CFRelease(values);
	}
	return;
	
}

static void
show_all_categories(SCPreferencesRef prefs, CFStringRef name, Boolean verbose)
{
	CFArrayRef	categories;
	CFIndex		count;

	categories = SCNetworkCategoryCopyAll(prefs);
	if (categories == NULL) {
		fprintf(stderr, "No categories\n");
		return;
	}
	count = CFArrayGetCount(categories);
	for (CFIndex i = 0; i < count; i++) {
		SCNetworkCategoryRef	category;

		category = (SCNetworkCategoryRef)
			CFArrayGetValueAtIndex(categories, i);
		show_category(category, name, verbose);
	}
	CFRelease(categories);
	return;
}

static void
show_category_services(SCPreferencesRef prefs, const char * name,
		       categoryOptionsRef cat_opt, Boolean verbose)
{
	CFStringRef	name_cf = NULL;

	if (name != NULL && name[0] != '\0') {
		name_cf = my_CFStringCreate(name);
	}
	if (CFStringGetLength(cat_opt->category_id) == 0) {
		show_all_categories(prefs, name_cf, verbose);
	}
	else {
		SCNetworkCategoryRef	category;

		category = SCNetworkCategoryCreate(prefs, cat_opt->category_id);
		if (CFStringGetLength(cat_opt->category_value) == 0) {
			show_category(category, name_cf, verbose);
		}
		else {
			show_services_for_category(category,
						   cat_opt->category_value,
						   name_cf, verbose);
		}
		CFRelease(category);
	}
	__SC_CFRELEASE(name_cf);
	return;
}

static void
show_manager_services(SCNSManagerRef manager, const char * name,
		      categoryOptionsRef cat_opt, Boolean active_state,
		      Boolean verbose)
{
#pragma unused(verbose)
	CFStringRef		name_cf = NULL;

	if (name != NULL && name[0] != '\0') {
		name_cf = my_CFStringCreate(name);
	}
	if (name_cf == NULL) {
		/* show all? */
	}
	else {
		SCNetworkInterfaceRef	netif = NULL;
		SCNSServiceRef		service;
		CFStringRef		service_summary;
		CFMutableStringRef	str = NULL;

		netif = copy_available_interface(name_cf, name);
		service = SCNSManagerCopyService(manager, netif,
						 cat_opt->category_id,
						 cat_opt->category_value);
		if (service == NULL) {
			fprintf(stderr, "No service for %s\n", name);
			exit(EX_SOFTWARE);
		}
		if (active_state) {
			CFDictionaryRef		dns;
			CFDictionaryRef		ipv4;
			CFDictionaryRef		ipv6;

			ipv4 = SCNSServiceCopyActiveEntity(service,
							   kSCEntNetIPv4);
			
			ipv6 = SCNSServiceCopyActiveEntity(service,
							   kSCEntNetIPv6);
			dns = SCNSServiceCopyActiveEntity(service,
							  kSCEntNetDNS);
			if (ipv4 == NULL && ipv6 == NULL && dns == NULL) {
				exit(EX_SOFTWARE);
			}
			str = CFStringCreateMutable(NULL, 0);
			append_service_descr(str, ipv4, ipv6, dns);
			__SC_CFRELEASE(dns);
			__SC_CFRELEASE(ipv4);
			__SC_CFRELEASE(ipv6);
		}
		service_summary
			= settings_service_copy_summary(service);
		if (service_summary != NULL) {
			SCPrint(TRUE, stdout, CFSTR("%@\n"), service_summary);
			CFRelease(service_summary);
		}
		if (str != NULL) {
			SCPrint(TRUE, stdout, CFSTR("%@"), str);
			CFRelease(str);
		}
		__SC_CFRELEASE(netif);
		__SC_CFRELEASE(name_cf);
	}
	return;
}

static void
show_all_sets(SCPreferencesRef prefs, Boolean all_services)
{
	SCNetworkSetRef	current_set;
	CFStringRef	current_setID = NULL;
	CFArrayRef	sets = NULL;

	sets = SCNetworkSetCopyAll(prefs);
	if (sets == NULL) {
		return;
	}
	current_set = SCNetworkSetCopyCurrent(prefs);
	if (current_set != NULL) {
		current_setID = SCNetworkSetGetSetID(current_set);
	}
	for (CFIndex i = 0, count = CFArrayGetCount(sets);
	     i < count; i++) {
		Boolean		is_current = FALSE;
		CFStringRef	name;
		CFStringRef	setID;
		SCNetworkSetRef	s;

		s = (SCNetworkSetRef)CFArrayGetValueAtIndex(sets, i);
		name = SCNetworkSetGetName(s);
		setID = SCNetworkSetGetSetID(s);
		if (setID != NULL && current_setID != NULL
		    && CFEqual(setID, current_setID)) {
			is_current = TRUE;
		}
		if (!all_services) {
			SCPrint(TRUE, stdout,
				CFSTR("%d. "), (int)(i + 1));
		}
		SCPrint(TRUE, stdout,
			CFSTR("%s%@ (%@)%s\n"),
			(all_services && (i != 0)) ? "\n" : "",
			name, setID, is_current ? " [CurrentSet]" : "");
		if (all_services) {
			show_all_services(s, FALSE, FALSE);
		}
	}
	__SC_CFRELEASE(current_set);
	__SC_CFRELEASE(sets);
}

#define SHOW_OPTSTRING	"e:f:hi:S:v"

/*
 * Function: do_show
 * Purpose:
 *   Display information about sets, services and interfaces.
 */

static void
do_show(int argc, char * argv[])
{
	Boolean			active_state = FALSE;
	Boolean			all_interfaces_or_services = FALSE;
	Boolean			all_sets = FALSE;
	categoryOptions		cat_opt;
	int			ch;
	CFStringRef		filename;
	int			interface_or_service = -1;
	SCNSManagerRef		manager = NULL;
	const char *		name = NULL;
	int			opti;
	const char *		optname;
	SCPreferencesRef	prefs = NULL;
	SCNetworkSetRef		set = NULL;
	const char *		set_name = NULL;
	Boolean			verbose = FALSE;

	categoryOptionsInit(&cat_opt);
	while (TRUE) {
		Boolean		is_active_state;
		Boolean		is_use_manager;
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, SHOW_OPTSTRING, longopts, &opti);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case 0:
			is_active_state = FALSE;
			is_use_manager = FALSE;
			optname = longopts[opti].name;
			if (strcmp(optname, kOptActiveState) == 0) {
				is_active_state = TRUE;
			}
			else if (strcmp(optname, kOptUseSettingsManager) == 0) {
				is_use_manager = TRUE;
			}
			if (is_active_state || is_use_manager) {
				if (set_name != NULL) {
					fprintf(stderr,
						"-%c and --%s don't make "
						"sense together\n",
						OPT_SET, optname);
					command_specific_help();
				}
				if (prefs != NULL) {
					fprintf(stderr,
						"-%c and --%s don't make "
						"sense together\n",
						OPT_FILE, optname);
					command_specific_help();
				}
				if (is_active_state) {
					if (active_state) {
						fprintf(stderr,
							"--%s specified more "
							"than once",
							kOptActiveState);
						command_specific_help();
					}
					active_state = TRUE;
				}
				else if (is_use_manager) {
					if (manager != NULL) {
						fprintf(stderr,
							"--%s specified more "
							"than once",
							kOptUseSettingsManager);
					}
					manager = manager_create();
				}
			}
			else if (categoryOptionsAdd(&cat_opt, optname,
						    optarg, &success)) {
				if (!success) {
					command_specific_help();
				}
			}
			else {
				fprintf(stderr,
					"--%s not supported\n",
					optname);
				command_specific_help();
			}
			break;
		case OPT_SET:
			if (active_state) {
				fprintf(stderr,
					"-%c and --%s don't make "
					"sense together\n",
					OPT_SET, kOptActiveState);
				command_specific_help();
			}
			if (manager != NULL) {
				fprintf(stderr,
					"-%c and --%s don't make "
					"sense together\n",
					OPT_SET, kOptUseSettingsManager);
				command_specific_help();
			}
			if (set_name != NULL) {
				fprintf(stderr,
					"-%c specified multiple times\n",
					OPT_SET);
				command_specific_help();
			}
			set_name = optarg;
			if (set_name[0] == '\0') {
				all_sets = TRUE;
			}
			break;
		case OPT_INTERFACE:
		case OPT_SERVICE:
			if (interface_or_service != -1) {
				fprintf(stderr,
					"-%c/-%c specified multiple times\n",
					OPT_INTERFACE, OPT_SERVICE);
				command_specific_help();
			}
			interface_or_service = ch;
			name = optarg;
			if (optarg[0] == '\0') {
				all_interfaces_or_services = TRUE;
			}
			break;
		case OPT_FILE:
			if (active_state) {
				fprintf(stderr,
					"-%c and --%s don't make "
					"sense together\n",
					OPT_FILE, kOptActiveState);
				command_specific_help();
			}
			if (manager != NULL) {
				fprintf(stderr,
					"-%c and --%s don't make "
					"sense together\n",
					OPT_FILE, kOptUseSettingsManager);
				command_specific_help();
			}
			if (prefs != NULL) {
				fprintf(stderr,
					"-%c specified multiple times\n",
					OPT_FILE);
				command_specific_help();
			}
			if (!file_exists(optarg)) {
				exit(EX_SOFTWARE);
			}
			filename = createPath(optarg);
			if (filename == NULL) {
				exit(EX_SOFTWARE);
			}
			prefs = prefs_create_with_file(filename);
			CFRelease(filename);
			break;
		case OPT_VERBOSE:
			verbose = TRUE;
			break;
		default:
			command_specific_help();
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (manager != NULL) {
		show_manager_services(manager, name, &cat_opt, active_state,
				      verbose);
		goto done;
	}
	if (prefs == NULL) {
		prefs = prefs_create();
	}
	if (set_name != NULL) {
		CFStringRef	set_name_cf;

		if (all_sets) {
			if (interface_or_service != -1) {
				fprintf(stderr,
					"Can't specify -%c when showing "
					"all sets\n", interface_or_service);
				command_specific_help();
			}
			show_all_sets(prefs, verbose);
			goto done;
		}
		set_name_cf = my_CFStringCreate(set_name);
		set = set_copy(prefs, set_name_cf);
		CFRelease(set_name_cf);
		if (set == NULL) {
			fprintf(stderr, "Can't find set '%s'\n", set_name);
			exit(EX_SOFTWARE);
		}
	}
	else {
		set = SCNetworkSetCopyCurrent(prefs);
		if (set == NULL && interface_or_service != OPT_INTERFACE) {
			fprintf(stderr, "No configuration\n");
			exit(EX_SOFTWARE);
		}
	}
	if (interface_or_service == -1) {
		interface_or_service = OPT_SERVICE;
		all_interfaces_or_services = TRUE;
	}
	if (cat_opt.category_id != NULL || cat_opt.category_value != NULL) {
		if (!categoryOptionsAreValid(&cat_opt)) {
			command_specific_help();
		}
		show_category_services(prefs, name, &cat_opt, verbose);
		goto done;
	}
	switch (interface_or_service) {
	case OPT_INTERFACE:
		if (all_interfaces_or_services) {
			printf("Showing interfaces for the current system\n");
			show_all_interfaces(verbose);
		}
		else {
			find_and_show_interface(set, name, verbose);
		}
		break;
	case OPT_SERVICE:
		if (all_interfaces_or_services) {
			if (active_state) {
				show_active_services(set, NULL, verbose);
			}
			else {
				show_all_services(set, TRUE, verbose);
			}
		}
		else {
			if (active_state) {
				show_active_services(set, name, verbose);
			}
			else {
				find_and_show_service(set, name, verbose);
			}
		}
		break;
	default:
		break;
	}

 done:
	__SC_CFRELEASE(manager);
	__SC_CFRELEASE(set);
	__SC_CFRELEASE(prefs);
	categoryOptionsFree(&cat_opt);
	return;
}


/**
 ** Create
 **/
#define CREATE_OPTSTRING	"a:b:hI:N:P:t:"

static SCNetworkInterfaceRef
create_bridge(SCPreferencesRef prefs, int argc, char * argv[])
{
	Boolean			auto_configure = FALSE;
	SCBridgeInterfaceRef	bridge_netif;
	int			ch;
	CFStringRef		member_name;
	CFMutableArrayRef	members = NULL;
	CFStringRef		name = NULL;
	CFArrayRef		netif_members;

	while (TRUE) {
		Boolean	success = FALSE;

		ch = getopt_long(argc, argv, CREATE_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_AUTO_CONFIGURE:
			if (!get_bool_from_string(optarg, &auto_configure,
						  OPT_AUTO_CONFIGURE,
						  kOptAutoConfigure)) {
				help_create_bridge();
			}
			success = TRUE;
			break;
		case OPT_NAME:
			if (name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptName);
				break;
			}
			name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_INTERFACE_TYPE:
			fprintf(stderr,
				"%s may only be specified once\n",
				kOptInterfaceType);
			break;
		case OPT_BRIDGE_MEMBER:
			member_name = my_CFStringCreate(optarg);
			if (members == NULL) {
				members = array_create();
			}
			else if (array_contains_value(members, member_name)) {
				CFRelease(member_name);
				fprintf(stderr,
					"member '%s' already specified\n",
					optarg);
				break;
			}
			CFArrayAppendValue(members, member_name);
			CFRelease(member_name);
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (members == NULL) {
		fprintf(stderr, "-%c/--%s must be specified at least once\n",
			OPT_BRIDGE_MEMBER, kOptBridgeMember);
		help_create_bridge();
	}
	netif_members = copy_bridge_member_interfaces(NULL, prefs, members);
	__SC_CFRELEASE(members);
	if (netif_members == NULL) {
		exit(EX_USAGE);
	}
	bridge_netif = SCBridgeInterfaceCreate(prefs);
	if (bridge_netif == NULL) {
		fprintf(stderr, "Failed to create bridge interface: %s\n",
			SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}

	if (!auto_configure) {
		/* don't auto-configure */
		SCNetworkInterfaceSetAutoConfigure(bridge_netif, FALSE);

		/* allow members to have configured services */
		SCBridgeInterfaceSetAllowConfiguredMembers(bridge_netif, TRUE);
	}

	/* set members */
	if (netif_members != NULL) {
		if (!SCBridgeInterfaceSetMemberInterfaces(bridge_netif,
							  netif_members)) {
			fprintf(stderr, "Failed to set member list: %s\n",
				SCErrorString(SCError()));
			exit(EX_SOFTWARE);
		}
		CFRelease(netif_members);
	}

	/* set display name if specified */
	if (name != NULL) {
		if (!SCBridgeInterfaceSetLocalizedDisplayName(bridge_netif,
							      name)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to set bridge name to '%@', %s\n"),
				name, SCErrorString(SCError()));
			exit(EX_SOFTWARE);
		}
		CFRelease(name);
	}
	return (bridge_netif);
}

static SCNetworkInterfaceRef
create_vlan(SCPreferencesRef prefs, int argc, char * argv[])
{
	Boolean			auto_configure = TRUE;
	int			ch;
	CFStringRef		name = NULL;
	SCNetworkInterfaceRef	netif;
	const char *		vlan_device = NULL;
	const char *		vlan_id = NULL;
	CFNumberRef		vlan_id_cf = NULL;
	SCVLANInterfaceRef	vlan_netif;

	while (TRUE) {
		Boolean	success = FALSE;

		ch = getopt_long(argc, argv, CREATE_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_AUTO_CONFIGURE:
			if (!get_bool_from_string(optarg, &auto_configure,
						  OPT_AUTO_CONFIGURE,
						  kOptAutoConfigure)) {
				break;
			}
			success = TRUE;
			break;
		case OPT_NAME:
			if (name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptName);
				break;
			}
			name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_INTERFACE_TYPE:
			fprintf(stderr,
				"%s may only be specified once\n",
				kOptInterfaceType);
			break;
		case OPT_VLAN_ID:
			if (vlan_id != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANID);
				break;
			}
			vlan_id = optarg;
			success = TRUE;
			break;
		case OPT_VLAN_DEVICE:
			if (vlan_device != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANDevice);
				break;
			}
			vlan_device = optarg;
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			help_create_vlan();
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (vlan_id == NULL || vlan_device == NULL) {
		fprintf(stderr, "Both -%c and -%c must be specified\n",
			OPT_VLAN_ID, OPT_VLAN_DEVICE);
		help_create_vlan();
	}
	netif = copy_vlan_device_and_id(vlan_id, vlan_device, &vlan_id_cf);
	if (netif == NULL) {
		exit(EX_USAGE);
	}
	vlan_netif = SCVLANInterfaceCreate(prefs, netif, vlan_id_cf);
	if (vlan_netif == NULL) {
		fprintf(stderr, "Failed to create VLAN interface: %s\n",
			SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	if (!auto_configure) {
		/* don't auto-configure */
		SCNetworkInterfaceSetAutoConfigure(vlan_netif, FALSE);
	}
	if (name != NULL
	    && !SCVLANInterfaceSetLocalizedDisplayName(vlan_netif, name)) {
		SCPrint(TRUE, stderr,
			CFSTR("Failed to set VLAN name to '%@', %s\n"),
			name, SCErrorString(SCError()));
		exit(EX_SOFTWARE);
	}
	__SC_CFRELEASE(name);
	__SC_CFRELEASE(netif);
	__SC_CFRELEASE(vlan_id_cf);
	return (vlan_netif);
}

/*
 * Function: do_create
 * Purpose:
 *   Create a virtual interface.
 */
static void
do_create(int argc, char * argv[])
{
	int			ch;
	SCNetworkInterfaceRef	netif;
	SCPreferencesRef	prefs = prefs_create();
	InterfaceType		type;

	ch = getopt_long(argc, argv, CREATE_OPTSTRING, longopts, NULL);
	switch (ch) {
	case OPT_HELP:
		command_specific_help();

	case OPT_INTERFACE_TYPE:
		type = InterfaceTypeFromString(optarg);
		break;
	default:
		fprintf(stderr, "-%c must first be specified\n",
			OPT_INTERFACE_TYPE);
		command_specific_help();
	}
	switch (type) {
	case kInterfaceTypeVLAN:
		netif = create_vlan(prefs, argc, argv);
		break;
	case kInterfaceTypeBridge:
		netif = create_bridge(prefs, argc, argv);
		break;
	case kInterfaceTypeNone:
		print_invalid_interface_type(optarg);
		exit(EX_USAGE);
	}
	commit_apply(prefs);
	SCPrint(TRUE, stdout,
		CFSTR("Created %@\n"),
		SCNetworkInterfaceGetBSDName(netif));
	CFRelease(netif);
	CFRelease(prefs);
	return;
}

/**
 ** Destroy
 **/

#define DESTROY_OPTSTRING	"hi:t:"

/*
 * Function: do_destroy
 * Purpose:
 *   Destroy a virtual interface.
 */
static void
do_destroy(int argc, char * argv[])
{
	int			ch;
	CFStringRef		ifname = NULL;
	SCNetworkInterfaceRef	netif = NULL;
	SCPreferencesRef	prefs = prefs_create();
	InterfaceType		type = kInterfaceTypeNone;
	const char *		type_str = NULL;

	while (TRUE) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, DESTROY_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_HELP:
			break;

		case OPT_INTERFACE:
			if (ifname != NULL) {
				fprintf(stderr,
					"-%c specified multiple times\n",
					OPT_INTERFACE);
				break;
			}
			if (type == kInterfaceTypeNone) {
				type = InterfaceTypeFromPrefix(optarg);
			}
			if (type == kInterfaceTypeNone) {
				fprintf(stderr,
					"Specify -%c (vlan | bridge )\n",
					OPT_INTERFACE_TYPE);
				break;
			}
			ifname = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_INTERFACE_TYPE:
			if (type_str != NULL) {
				fprintf(stderr,
					"-%c specified multiple times\n",
					OPT_INTERFACE_TYPE);
				break;
			}
			type_str = optarg;
			type = InterfaceTypeFromString(optarg);
			if (type == kInterfaceTypeNone) {
				print_invalid_interface_type(optarg);
				exit(EX_USAGE);
			}
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
	}
	if (ifname == NULL) {
		fprintf(stderr, "-%c must be specified\n",
			OPT_INTERFACE);
		command_specific_help();
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	switch (type) {
	case kInterfaceTypeNone:
		command_specific_help();

	case kInterfaceTypeVLAN:
		netif = copy_vlan_interface(prefs, ifname);
		if (netif == NULL) {
			SCPrint(TRUE, stderr,
				CFSTR("Can't find VLAN %@\n"),
				ifname);
			exit(EX_SOFTWARE);
		}
		if (!SCVLANInterfaceRemove(netif)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to remove %@: %s\n"),
				ifname, SCErrorString(SCError()));
			exit(EX_USAGE);
		}
		break;
	case kInterfaceTypeBridge:
		netif = copy_bridge_interface(prefs, ifname);
		if (netif == NULL) {
			SCPrint(TRUE, stderr,
				CFSTR("Can't find bridge %@\n"),
				ifname);
			exit(EX_SOFTWARE);
		}
		if (!SCBridgeInterfaceRemove(netif)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to remove %@: %s\n"),
				ifname, SCErrorString(SCError()));
			exit(EX_USAGE);
		}
		break;
	}
	commit_apply(prefs);
	__SC_CFRELEASE(ifname);
	__SC_CFRELEASE(netif);
}

/**
 ** SetVLAN
 **/

#define SET_VLAN_OPTSTRING	"hi:I:N:P:"

/*
 * Function: do_set_vlan
 * Purpose:
 *   Enables setting the VLAN ID, physical interface, and name for
 *   a VLAN interface,
 */

static void
do_set_vlan(int argc, char * argv[])
{
	int			ch;
	Boolean			changed = FALSE;
	CFStringRef		name = NULL;
	SCPreferencesRef	prefs = prefs_create();
	const char *		vlan_device = NULL;
	CFStringRef		vlan_name;
	const char *		vlan_id = NULL;
	SCNetworkInterfaceRef	vlan_netif = NULL;

	while (TRUE) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv, SET_VLAN_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_NAME:
			if (name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptName);
				break;
			}
			name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_HELP:
			command_specific_help();

		case OPT_INTERFACE:
			if (vlan_netif != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptInterface);
				break;
			}
			vlan_name = my_CFStringCreate(optarg);
			vlan_netif = copy_vlan_interface(prefs, vlan_name);
			CFRelease(vlan_name);
			if (vlan_netif == NULL) {
				SCPrint(TRUE, stderr,
					CFSTR("Can't find VLAN %@\n"),
					name);
				exit(EX_SOFTWARE);
			}
			success = TRUE;
			break;
		case OPT_VLAN_ID:
			if (vlan_id != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANID);
			}
			vlan_id = optarg;
			success = TRUE;
			break;
		case OPT_VLAN_DEVICE:
			if (vlan_device != NULL) {
				fprintf(stderr,
					"%s specified multiple times\n",
					kOptVLANDevice);
			}
			vlan_device = optarg;
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
	}

	if (vlan_netif == NULL) {
		fprintf(stderr, "-%c must be specified\n", OPT_INTERFACE);
		command_specific_help();
	}
	if (vlan_id != NULL || vlan_device != NULL) {
		CFNumberRef		vlan_id_cf;
		SCNetworkInterfaceRef	netif;

		if (vlan_id == NULL || vlan_device == NULL) {
			fprintf(stderr, "Both -%c and -%c must be specified\n",
				OPT_VLAN_ID, OPT_VLAN_DEVICE);
			command_specific_help();
		}
		netif = copy_vlan_device_and_id(vlan_id, vlan_device,
						&vlan_id_cf);
		if (netif == NULL) {
			exit(EX_USAGE);
		}
		if (!SCVLANInterfaceSetPhysicalInterfaceAndTag(vlan_netif,
							       netif,
							       vlan_id_cf)) {
			fprintf(stderr, "Failed to set vlan tag/device: %s\n",
				SCErrorString(SCError()));
			exit(EX_SOFTWARE);
		}
		changed = TRUE;
		CFRelease(netif);
		CFRelease(vlan_id_cf);
	}
	if (name != NULL) {
		if (!SCVLANInterfaceSetLocalizedDisplayName(vlan_netif, name)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to set VLAN name to '%@', %s\n"),
				name, SCErrorString(SCError()));
			exit(EX_SOFTWARE);
		}
		changed = TRUE;
		CFRelease(name);
	}
	if (!changed) {
		fprintf(stderr, "Must specify both -%c and -%c, and/or -%c\n",
			OPT_VLAN_ID, OPT_VLAN_DEVICE, OPT_NAME);
		command_specific_help();
	}
	commit_apply(prefs);
	CFRelease(prefs);
	CFRelease(vlan_netif);
}

/**
 ** SetBridge
 **/

#define SET_BRIDGE_OPTSTRING	"b:hi:N:"

/*
 * Function: do_set_bridge
 * Purpose:
 *   Enables setting the bridge members and/or the bridge name.
 */

static void
do_set_bridge(int argc, char * argv[])
{
	CFStringRef		bridge_name = NULL;
	SCNetworkInterfaceRef	bridge_netif = NULL;
	int			ch;
	CFStringRef		member_name;
	CFMutableArrayRef	members = NULL;
	CFStringRef		name = NULL;
	SCPreferencesRef	prefs = prefs_create();

	while (TRUE) {
		Boolean		success = FALSE;

		ch = getopt_long(argc, argv,
				 SET_BRIDGE_OPTSTRING, longopts, NULL);
		if (ch == -1) {
			break;
		}
		switch (ch) {
		case OPT_NAME:
			if (name != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptName);
				break;
			}
			name = my_CFStringCreate(optarg);
			success = TRUE;
			break;
		case OPT_HELP:
			command_specific_help();

		case OPT_INTERFACE:
			if (bridge_netif != NULL) {
				fprintf(stderr, "%s specified multiple times\n",
					kOptInterface);
				break;
			}
			bridge_name = my_CFStringCreate(optarg);
			bridge_netif = copy_bridge_interface(prefs, bridge_name);
			if (bridge_netif == NULL) {
				SCPrint(TRUE, stderr,
					CFSTR("Can't find bridge %@\n"),
					bridge_name);
				exit(EX_SOFTWARE);
			}
			CFRelease(bridge_name);
			bridge_name = NULL;
			success = TRUE;
			break;
		case OPT_BRIDGE_MEMBER:
			member_name = my_CFStringCreate(optarg);
			if (members == NULL) {
				members = array_create();
			}
			else if (array_contains_value(members, member_name)) {
				CFRelease(member_name);
				fprintf(stderr,
					"member '%s' already specified\n",
					optarg);
				break;
			}
			CFArrayAppendValue(members, member_name);
			CFRelease(member_name);
			success = TRUE;
			break;
		default:
			break;
		}
		if (!success) {
			command_specific_help();
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Extra command-line arguments\n");
		exit(EX_USAGE);
	}
	if (bridge_netif == NULL ) {
		fprintf(stderr, "-%c must be specified\n", OPT_INTERFACE);
		command_specific_help();
	}
	if (members == NULL && name == NULL) {
		fprintf(stderr, "Must specify -%c and/or -%c\n",
			OPT_BRIDGE_MEMBER, OPT_NAME);
		command_specific_help();
	}

	/* set members */
	if (members != NULL) {
		CFArrayRef	netif_members;

		netif_members = copy_bridge_member_interfaces(bridge_netif,
							      prefs, members);
		CFRelease(members);
		if (netif_members == NULL) {
			exit(EX_USAGE);
		}
		if (!SCBridgeInterfaceSetMemberInterfaces(bridge_netif,
							  netif_members)) {
			fprintf(stderr, "Failed to set member list: %s\n",
				SCErrorString(SCError()));
			exit(EX_SOFTWARE);
		}
		CFRelease(netif_members);
	}

	/* set display name if specified */
	if (name != NULL) {
		if (!SCBridgeInterfaceSetLocalizedDisplayName(bridge_netif,
							      name)) {
			SCPrint(TRUE, stderr,
				CFSTR("Failed to set bridge name to '%@', %s\n"),
				name, SCErrorString(SCError()));
			exit(EX_SOFTWARE);
		}
		CFRelease(name);
	}
	commit_apply(prefs);
	CFRelease(prefs);
	CFRelease(bridge_netif);
}

/**
 ** Help/usage
 **/
#define _CATEGORY 	"--ssid <ssid> | "		\
	"( --category <id> --category-value <value> )"
#define _IDENTIFIER	"( -i <interface> | -S <service> ) [ -N <service-name> ]"

#define _SET_ADD_OPT	"[ category-options ] " _IDENTIFIER 

static void
help_add_set(const char * command_str) __dead2;

static void
help_remove(const char * command_str) __dead2;

static void
help_enable_disable(const char * command_str) __dead2;

static void
help_show(const char * command_str) __dead2;

static void
help_create(const char * command_str) __dead2;

static void
help_destroy(const char * command_str) __dead2;

static void
help_setvlan(const char * command_str) __dead2;

static void
help_setbridge(const char * command_str) __dead2;

static void
help_add_set(const char * command_str)
{
	fprintf(stderr,
		"%s %s " _SET_ADD_OPT " -D\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _SET_ADD_OPT " -p ipv4 -c dhcp\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _SET_ADD_OPT " -p ipv4 -c manual -A <ip> -m <mask>\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _SET_ADD_OPT " -p ipv6 -c automatic\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _SET_ADD_OPT " -p ipv6 -c manual -A <ip>\n",
		G_argv0, command_str);
	fprintf(stderr,
		"%s %s " _SET_ADD_OPT " -p dns -A <dns-server> "
		"-n <domain> -s <domain>\n",
		G_argv0, command_str);
	fprintf(stderr, "\tcategory-options: " _CATEGORY "\n");
	exit(EX_USAGE);
}

static void
help_remove(const char * command_str)
{
	fprintf(stderr,
		"%s %s [ category-options ] ( -%c <interface> | -%c <service> ) "
		"[ -p ( ipv4 | ipv6 | dns ) ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_SERVICE);
	fprintf(stderr, "\tcategory-options: " _CATEGORY "\n");
	exit(EX_USAGE);
}

static void
help_enable_disable(const char * command_str)
{
	fprintf(stderr,
		"%s %s ( -%c <interface> | -%c <service> ) "
		"[ -p ( ipv4 | ipv6 | dns ) ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_SERVICE);
	exit(EX_USAGE);
}

static void
help_show(const char * command_str)
{
	fprintf(stderr,
		"%s %s [ -e <set> | -e \"\" ] [ -%c <interface> | -%c \"\" | "
		"-%c <service> | -%c \"\" ] [ -%c ] [ -%c <filename> | --%s ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_INTERFACE, 
		OPT_SERVICE, OPT_SERVICE, OPT_VERBOSE,
		OPT_FILE, kOptActiveState);
	exit(EX_USAGE);
}

#define BOOL_OPT_STRING	"true | yes | 1 | false | no | 0"
static void
help_create_vlan_no_exit(const char * command_str)
{
	fprintf(stderr,
		"%s %s -t vlan -%c <1..4095> -%c <interface> "
		"[ -N <name> ] [ -a " BOOL_OPT_STRING " )]\n",
		G_argv0, command_str,
		OPT_VLAN_ID, OPT_VLAN_DEVICE);
}

static void
help_create_vlan(void)
{
	const char *	str = CommandGetString(G_command);

	help_create_vlan_no_exit(str);
	exit(EX_USAGE);
}

static void
help_create_bridge_no_exit(const char * command_str)
{
	fprintf(stderr,
		"%s %s -t bridge -%c <member> [ -%c <member> ... ] "
		"[ -N <name> ] [ -a " BOOL_OPT_STRING " )]\n",
		G_argv0, command_str,
		OPT_BRIDGE_MEMBER, OPT_BRIDGE_MEMBER);
}

static void
help_create_bridge(void)
{
	const char *	str = CommandGetString(G_command);

	help_create_bridge_no_exit(str);
	exit(EX_USAGE);
}

static void
help_create(const char * command_str)
{
	help_create_vlan_no_exit(command_str);
	help_create_bridge_no_exit(command_str);
	exit(EX_USAGE);
}

static void
help_destroy(const char * command_str)
{
	fprintf(stderr,
		"%s %s [ -%c vlan' | bridge ] -%c <interface>\n",
		G_argv0, command_str, OPT_INTERFACE_TYPE, OPT_INTERFACE);
	exit(EX_USAGE);
}

static void
help_setvlan(const char * command_str)
{
	fprintf(stderr,
		"%s %s -%c <interface> -%c <1..4095> -%c <interface> "
		"[ -%c <name> ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_VLAN_ID, OPT_VLAN_DEVICE, OPT_NAME);
	exit(EX_USAGE);
}

static void
help_setbridge(const char * command_str)
{
	fprintf(stderr,
		"%s %s -%c <interface> [ -%c <member> [ -%c <member> ... ] ] "
		"[ -%c <name> ]\n",
		G_argv0, command_str,
		OPT_INTERFACE, OPT_BRIDGE_MEMBER, OPT_BRIDGE_MEMBER, OPT_NAME);
	exit(EX_USAGE);
}

static void
usage(void) __dead2;

static void
usage(void)
{
	fprintf(stderr,
		"Usage: %s <command> <options>\n"
		"\n<command> is one of:\n"
		"\tadd\n"
		"\tset\n"
		"\tremove\n"
		"\tdisable\n"
		"\tenable\n"
		"\tshow\n"
		"\tcreate\n"
		"\tdestroy\n"
		"\tsetvlan\n"
		"\tsetbridge\n"
		"\n<options>:\n"
		"\t--active-state            show active state\n"
		"\t--auto-config, -a         auto-configure\n"
		"\t--address, -A             IP address\n"
		"\t--bridge-member, -b       bridge member\n"
		"\t--category                category identifier\n"
		"\t--category-value          category value\n"
		"\t--config-method, -c       configuration method\n"
		"\t--dhcp-client-id, -C      DHCP client identifier\n"
		"\t--default-config, -D      establish default configuration\n"
		"\t--file, -f                filename\n"
		"\t--help, -h                get help\n"
		"\t--dns-domain-name, -n     DNS domain name\n"
		"\t--dns-search-domains, -S  DNS search domains\n"
		"\t--interface, -i           interface name e.g. en0\n"
		"\t--interface-type, -t      interface type e.g. vlan\n"
		"\t--new-name, -N            new name\n"
		"\t--subnet-mask, -m         subnet mask e.g. 255.255.255.0\n"
		"\t--protocol, -p            protocol e.g. ipv4, ipv6, dns\n"
		"\t--service, -s             service name/identifier\n"
		"\t--ssid                    Wi-Fi SSID\n"
		"\t--verbose, -v             be verbose\n"
		"\t--vlan-id, -I             VLAN identifier (1..4096)\n"
		"\t--vlan-device, -P         VLAN physical device e.g. en0\n",
		G_argv0);
	exit(EX_USAGE);
}

static void
command_specific_help(void)
{
	const char *	str = CommandGetString(G_command);
	
	switch (G_command) {
	case kCommandAdd:
	case kCommandSet:
		help_add_set(str);

	case kCommandRemove:
		help_remove(str);

	case kCommandEnable:
	case kCommandDisable:
		help_enable_disable(str);

	case kCommandShow:
		help_show(str);

	case kCommandCreate:
		help_create(str);

	case kCommandDestroy:
		help_destroy(str);

	case kCommandSetVLAN:
		help_setvlan(str);

	case kCommandSetBridge:
		help_setbridge(str);

	case kCommandNone:
		break;
	}
	usage();
}

int
main(int argc, char *argv[])
{
	const char *	slash;

	G_argv0 = argv[0];
	slash = strrchr(G_argv0, '/');
	if (slash != NULL) {
		G_argv0 = slash + 1;
	}
	if (argc < 2) {
		usage();
	}
	G_command = CommandFromString(argv[1]);
	if (G_command == kCommandNone) {
		usage();
	}
	switch (G_command) {
	case kCommandAdd:
	case kCommandSet:
		do_add_set(argc - 1, argv + 1);
		break;
	case kCommandRemove:
	case kCommandEnable:
	case kCommandDisable:
		do_remove_enable_disable(argc - 1, argv + 1);
		break;
	case kCommandShow:
		do_show(argc - 1, argv + 1);
		break;
	case kCommandCreate:
		do_create(argc - 1, argv + 1);
		break;
	case kCommandDestroy:
		do_destroy(argc - 1, argv + 1);
		break;
	case kCommandSetVLAN:
		do_set_vlan(argc - 1, argv + 1);
		break;
	case kCommandSetBridge:
		do_set_bridge(argc - 1, argv + 1);
		break;
	case kCommandNone:
		break;
	}
	exit(0);
}
