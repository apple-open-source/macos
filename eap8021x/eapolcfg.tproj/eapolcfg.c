/*
 * Copyright (c) 2009-2011 Apple Inc. All rights reserved.
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
 * eapolcfg.c
 * - tool to manipulate configuration profiles
 */

/* 
 * Modification History
 *
 * December 10, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <SystemConfiguration/SCPrivate.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPOLClientConfiguration.h>
#include <EAP8021X/EAPOLControl.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <readpassphrase.h>
#include "myCFUtil.h"
#include "EAPSecurity.h"
#include "symbol_scope.h"

typedef int func_t(const char * command, int argc, char * const * argv);
typedef func_t * funcptr_t;

struct command_info {
    const char *	command;
    funcptr_t		func;
    int			argc;
    char *		usage;
};

STATIC void
show_command_usage(const char * cmd);

#define kCommandListProfiles 		"listProfiles"
#define kCommandShowProfile 		"showProfile"
#define kCommandImportProfile 		"importProfile"
#define kCommandExportProfile 		"exportProfile"
#define kCommandRemoveProfile 		"removeProfile"
#define kCommandCreateProfile 		"createProfile"
#define kCommandSetProfileInformation	"setProfileInformation"
#define kCommandGetProfileInformation	"getProfileInformation"
#define kCommandClearProfileInformation	"clearProfileInformation"
#define kCommandSetPasswordItem		"setPasswordItem"
#define kCommandGetPasswordItem		"getPasswordItem"
#define kCommandRemovePasswordItem 	"removePasswordItem"
#define kCommandSetIdentity		"setIdentity"
#define kCommandClearIdentity		"clearIdentity"
#define kCommandGetIdentity		"getIdentity"
#define kCommandGetLoginWindowProfiles	"getLoginWindowProfiles"
#define kCommandSetLoginWindowProfiles	"setLoginWindowProfiles"
#define kCommandClearLoginWindowProfiles "clearLoginWindowProfiles"
#define kCommandGetSystemProfile	"getSystemProfile"
#define kCommandSetSystemProfile	"setSystemProfile"
#define kCommandGetLoginWindowInterfaces "getLoginWindowInterfaces"
#define kCommandGetSystemInterfaces	"getSystemInterfaces"
#define kCommandClearSystemProfile	"clearSystemProfile"
#define kCommandStartAuthentication	"startAuthentication"

/**
 ** Utility routines
 **/

typedef struct num_to_string {
    const char *	str;
    int			num;
} num_to_string;


STATIC SecPreferencesDomain
map_preferences_domain(EAPOLClientDomain domain)
{
    if (domain == kEAPOLClientDomainSystem) {
	return (kSecPreferencesDomainSystem);
    }
    return (kSecPreferencesDomainUser);
}

STATIC bool
set_keychain_domain(SecPreferencesDomain required_domain,
		    bool * previous_set,
		    SecPreferencesDomain * previous)
{
    SecPreferencesDomain 	current_domain;
    OSStatus			status;

    *previous_set = FALSE;
    status = SecKeychainGetPreferenceDomain(&current_domain);
    if (status != noErr) {
	return (FALSE);
    }
    if (required_domain != current_domain) {
	status = SecKeychainSetPreferenceDomain(required_domain);
	if (status != noErr) {
	    return (FALSE);
	}
    }
    *previous = current_domain;
    *previous_set = TRUE;
    return (TRUE);
}

STATIC CFDataRef
CFDataCreateWithFile(const char * filename)
{
    CFMutableDataRef 	data = NULL;
    size_t		len = 0;
    int			fd = -1;
    struct stat		sb;

    if (stat(filename, &sb) < 0) {
	goto done;
    }
    len = sb.st_size;
    if (len == 0) {
	goto done;
    }
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
	goto done;
    }
    data = CFDataCreateMutable(NULL, len);
    CFDataSetLength(data, len);
    if (read(fd, CFDataGetMutableBytePtr(data), len) != len) {
	my_CFRelease(&data);
	goto done;
    }

 done:
    if (fd >= 0) {
	close(fd);
    }
    if (data != NULL && CFDataGetLength(data) == 0) {
	CFRelease(data);
	data = NULL;
    }
    return (data);
}

STATIC void
fwrite_plist(FILE * f, CFPropertyListRef p)
{
    CFDataRef	data;

    data = CFPropertyListCreateData(NULL, p, 
				    kCFPropertyListXMLFormat_v1_0,
				    0, NULL);
    if (data == NULL) {
	return;
    }
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, f);
    CFRelease(data);
    return;
}

STATIC int
auth_type_from_string(const char * value)
{
    int			i;
    num_to_string *	scan;
    num_to_string	tbl[] = {
	{ "TLS", kEAPTypeTLS },
	{ "TTLS", kEAPTypeTTLS },
	{ "PEAP", kEAPTypePEAP },
	{ "EAP-FAST", kEAPTypeEAPFAST },
	{ "LEAP", kEAPTypeCiscoLEAP }
    };
    int		tbl_size = sizeof(tbl) / sizeof(*tbl);
    
    for (i = 0, scan = tbl; i < tbl_size; i++, scan++) {
	if (strcasecmp(scan->str, value) == 0) {
	    return (scan->num);
	}
    }
    return (-1);
}

STATIC CFNumberRef
make_auth_type(const char * str)
{
    int			type;

    type = auth_type_from_string(str);
    if (type == -1) {
	type = strtol(str, NULL, 0);
	if (type <= 0 || type > 255) {
	    return (NULL);
	}
    }
    return (CFNumberCreate(NULL, kCFNumberIntType, &type));
}

STATIC CFStringRef
copy_wlan_security_type(const char * str)
{
    int			i;
    CFStringRef		values[] = {
	kEAPOLClientProfileWLANSecurityTypeWEP,
	kEAPOLClientProfileWLANSecurityTypeWPA,
	kEAPOLClientProfileWLANSecurityTypeWPA2,
	kEAPOLClientProfileWLANSecurityTypeAny
    };
    int			values_count = sizeof(values) / sizeof(values[0]);
    CFStringRef		result = NULL;
    CFStringRef		str_cf;

    str_cf = CFStringCreateWithCString(NULL, str, kCFStringEncodingASCII);
    for (i = 0; i < values_count; i++) {
	if (CFStringCompare(str_cf, values[i], kCFCompareCaseInsensitive)
	    == kCFCompareEqualTo) {
	    result = CFRetain(values[i]);
	    break;
	}
    }
    CFRelease(str_cf);
    return (result);
}

STATIC CFStringRef
copy_ttls_inner_auth(const char * str)
{
    int			i;
    char * const	values[] = {
	"PAP",
	"CHAP",
	"MSCHAP",
	"MSCHAPv2"
    };
    int			values_count = sizeof(values) / sizeof(values[0]);

    for (i = 0; i < values_count; i++) {
	if (strcasecmp(str, values[i]) == 0) {
	    return (CFStringCreateWithCString(NULL, values[i],
					      kCFStringEncodingASCII));
	}
    }
    return (NULL);
}

STATIC SecIdentityRef
find_identity_using_cert(EAPOLClientDomain domain, SecCertificateRef cert)
{
    int				count;
    int				i;
    CFArrayRef			list;
    SecPreferencesDomain	previous_domain = kSecPreferencesDomainUser;
    bool			previous_domain_set = FALSE;
    SecIdentityRef		ret_identity = NULL;
    OSStatus			status;

    previous_domain_set
	= set_keychain_domain(map_preferences_domain(domain),
			      &previous_domain_set, &previous_domain);
    status = EAPSecIdentityListCreate(&list);
    if (status != noErr) {
	goto done;
    }
    count = CFArrayGetCount(list);
    for (i = 0; i < count && ret_identity == NULL; i++) {
	SecIdentityRef		identity;
	SecCertificateRef	this_cert;

	identity = (SecIdentityRef)CFArrayGetValueAtIndex(list, i);
	status = SecIdentityCopyCertificate(identity, &this_cert);
	if (status == noErr) {
	    if (CFEqual(this_cert, cert)) {
		ret_identity = identity;
		CFRetain(identity);
	    }
	    CFRelease(this_cert);
	}
    }

 done:
    if (previous_domain_set) {
	SecKeychainSetPreferenceDomain(previous_domain);
    }
    return (ret_identity);
}

STATIC EAPOLClientConfigurationRef
configuration_open(bool need_auth)
{
    AuthorizationRef			auth;
    EAPOLClientConfigurationRef		cfg;
    OSStatus				status;

    if (geteuid() == 0 || need_auth == FALSE) {
	return (EAPOLClientConfigurationCreate(NULL));
    }
    status = AuthorizationCreate(NULL,
				 kAuthorizationEmptyEnvironment,
				 kAuthorizationFlagDefaults,
				 &auth);
    if (status != errAuthorizationSuccess) {
	fprintf(stderr, "Failed to get authorization\n");
	return (NULL);
    }
    cfg = EAPOLClientConfigurationCreateWithAuthorization(NULL, auth);
    return (cfg);
}

STATIC void
show_profile(EAPOLClientProfileRef profile)
{
    CFArrayRef		eap_types;
    CFDictionaryRef	auth_props;
    CFStringRef		security_type;
    CFDataRef		ssid;
    CFStringRef		user_defined_name;

    SCPrint(TRUE, stdout, CFSTR("%@\n"),
	    EAPOLClientProfileGetID(profile));
    auth_props = EAPOLClientProfileGetAuthenticationProperties(profile);
    user_defined_name = EAPOLClientProfileGetUserDefinedName(profile);
    if (user_defined_name != NULL) {
	SCPrint(TRUE, stdout, CFSTR("\tUserDefinedName = '%@'\n"),
		user_defined_name);
    }
    ssid = EAPOLClientProfileGetWLANSSIDAndSecurityType(profile,
							&security_type);
    if (ssid != NULL) {
	printf("\tSSID = '");
	fwrite(CFDataGetBytePtr(ssid), CFDataGetLength(ssid), 1, stdout);
	SCPrint(TRUE, stdout, CFSTR("', SecurityType = '%@'\n"),
		security_type);
    }
    eap_types = CFDictionaryGetValue(auth_props,
				     kEAPClientPropAcceptEAPTypes);
    if (eap_types == NULL) {
	fprintf(stderr, "framework error - AcceptEAPTypes can't be NULL\n");
    }
    else {
	int		count;
	int		i;
	
	count = CFArrayGetCount(eap_types);
	printf("\tAcceptEAPTypes = { ");
	for (i = 0; i < count; i++) {
	    const char *	str;
	    CFNumberRef	type = CFArrayGetValueAtIndex(eap_types, i);
	    int		val;
	    
	    if (isA_CFNumber(type) == NULL
		|| CFNumberGetValue(type, kCFNumberIntType,
				    &val) == FALSE) {
		fprintf(stderr, "bogus element in AcceptEAPTypes array\n");
		continue;
	    }
	    str = EAPTypeStr(val);
	    if (strcmp(str, "<unknown>") == 0) {
		printf("%s%d", (i == 0) ? "" : ", ", val);
	    }
	    else {
		printf("%s%s", (i == 0) ? "" : ", ", str);
	    }
	}
	printf(" }\n");
    }
    return;
}

/**
 ** Command entry points
 **/
STATIC int
S_list_profiles(const char * command, int argc, char * const * argv)
{
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    int				count;
    bool			details = FALSE;
    int				i;
    struct option 		longopts[] = {
	{ "details",	no_argument,	NULL,	'd' },
	{ NULL,		0,		NULL,	0 }
    };
    CFArrayRef			profiles = NULL;
    int				ret = 1;

    while ((ch = getopt_long(argc, argv, "d", longopts, NULL)) != -1) {
	switch (ch) {
	case 'd':
	    details = TRUE;
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (argc != 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }

    cfg = EAPOLClientConfigurationCreate(NULL);
    if (cfg == NULL) {
	fprintf(stderr, "EAPOLClientConfigurationCreate failed\n");
	goto done;
    }
    profiles = EAPOLClientConfigurationCopyProfiles(cfg);
    if (profiles == NULL) {
	fprintf(stderr, "No profiles\n");
	goto done;
    }
    count = CFArrayGetCount(profiles);
    if (details) {
	printf("There %s %d profile%s\n",
	       (count == 1) ? "is" : "are",
	       count,
	       (count == 1) ? "" : "s");
    }
    for (i = 0; i < count; i++) {
	EAPOLClientProfileRef	profile;

	profile = (EAPOLClientProfileRef)CFArrayGetValueAtIndex(profiles, i);
	if (details) {
	    SCPrint(TRUE, stdout, CFSTR("%d. "), i + 1);
	    show_profile(profile);
	}
	else {
	    SCPrint(TRUE, stdout, CFSTR("%@\n"),
		    EAPOLClientProfileGetID(profile));
	}
    }
    ret = 0;

 done:
    my_CFRelease(&profiles);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_profile_show_export_remove(const char * command,
			     int argc, char * const * argv)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    struct option 		longopts[] = {
	{ "profileID",	required_argument,	NULL,	'p' },
	{ "profileid",	required_argument,	NULL,	'p' },
	{ "SSID",	required_argument,	NULL,	's' },
	{ "ssid",	required_argument,	NULL,	's' },
	{ NULL,		0,			NULL,	0 }
    };
    EAPOLClientProfileRef	profile;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    CFDataRef			ssid = NULL;

    while ((ch = getopt_long(argc, argv, "p:s:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'p':
	    if (profileID != NULL) {
		fprintf(stderr, "profileID specified twice\n");
		goto done;
	    }
	    if (ssid != NULL) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (ssid != NULL) {
		fprintf(stderr, "SSID specified twice\n");
		goto done;
	    }
	    if (profileID != NULL) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (profileID == NULL && ssid == NULL) {
	fprintf(stderr, "No profile specified\n");
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = configuration_open(strcmp(command, kCommandRemoveProfile) == 0);
    if (cfg == NULL) {
	fprintf(stderr, "EAPOLClientConfigurationCreate failed\n");
	goto done;
    }
    if (profileID != NULL) {
	profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
    }
    else {
	profile = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid);
    }
    if (profile == NULL) {
	fprintf(stderr, "No such profile\n");
	ret = 1;
	goto done;
    }
    if (strcmp(command, kCommandShowProfile) == 0) {
	show_profile(profile);
    }
    else if (strcmp(command, kCommandExportProfile) == 0) {
	CFPropertyListRef	plist;

	plist = EAPOLClientProfileCreatePropertyList(profile);
	fwrite_plist(stdout, plist);
	CFRelease(plist);
    }
    else if (strcmp(command, kCommandRemoveProfile) == 0) {
	if (EAPOLClientConfigurationRemoveProfile(cfg, profile) == FALSE) {
	    fprintf(stderr, "Failed to remove profile\n");
	    goto done;
	}
	if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	    fprintf(stderr,
		    "Failed to save the configuration, %s\n",
		    SCErrorString(SCError()));
	    ret = 2;
	    goto done;
	}
    }
    else {
	fprintf(stderr, "internal error - unrecognized command %s\n",
		command);
	goto done;
    }
    ret = 0;

 done:
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_profile_create(const char * command, int argc, char * const * argv)
{
    CFMutableDictionaryRef	auth_props = NULL;
    CFMutableArrayRef		auth_types = NULL;
    CFRange			auth_types_range = { 0, 0 };
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    bool			eapfast_use_pac = FALSE;
    bool			eapfast_provision_pac = FALSE;
    bool			eapfast_provision_pac_anonymously = FALSE;
    struct option 		longopts[] = {
	{ "authType",	required_argument,		NULL,	'a' },
	{ "userDefinedName", required_argument,		NULL,	'u' },
	{ "SSID",	required_argument,		NULL,	's' },
	{ "securityType", required_argument,		NULL,	'S' },
	{ "trustedCertificate", required_argument,	NULL,	'c' },
	{ "trustedServerName", required_argument,	NULL,	'n' },
	{ "oneTimePassword", no_argument,		NULL,	'o' },
	{ "outerIdentity", required_argument,		NULL,	'O' },
	{ "TTLSInnerAuthentication", required_argument,	NULL,	't' },
	{ "EAPFASTUsePAC", no_argument,			NULL,	'U' },
	{ "EAPFASTProvisionPAC", no_argument,		NULL,	'p' },
	{ "EAPFASTProvisionPACAnonymously", no_argument, NULL, 	'A' },
	{ NULL,		0,				NULL,	0 }
    };
    bool			one_time_password = FALSE;
    CFStringRef			outer_identity = NULL;
    EAPOLClientProfileRef	profile = NULL;
    int				ret = 1;
    CFStringRef			security_type = NULL;
    CFDataRef			ssid = NULL;
    CFStringRef			str;
    CFMutableArrayRef		trusted_certs = NULL;
    CFRange			trusted_certs_range = { 0, 0 };
    CFMutableArrayRef		trusted_server_names = NULL;
    CFRange			trusted_server_names_range = { 0, 0 };
    CFStringRef			ttls_inner_auth = NULL;
    CFStringRef			user_defined_name = NULL;

    auth_types = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    while ((ch = getopt_long(argc, argv, "a:s:S:c:n:o:O:t:u:p:",
			     longopts, NULL)) != -1) {
	CFDictionaryRef 	attrs;
	SecCertificateRef	cert;
	CFDataRef		data;
	CFNumberRef		num;

	switch (ch) {
	case 'a': /* authType */
	    num = make_auth_type(optarg);
	    if (num == NULL) {
		fprintf(stderr, "invalid auth type '%s'\n",
			optarg);
		goto done;
	    }
	    if (CFArrayContainsValue(auth_types, auth_types_range, num)) {
		fprintf(stderr, "auth type '%s' specified multiple times\n",
			optarg);
		goto done;
	    }
	    CFArrayAppendValue(auth_types, num);
	    auth_types_range.length++;
	    CFRelease(num);
	    break;
	case 'u': /* userDefinedName */
	    if (user_defined_name != NULL) {
		fprintf(stderr, "userDefinedName specified multiple times\n");
		goto done;
	    }
	    user_defined_name 
		= CFStringCreateWithCString(NULL, optarg,
					    kCFStringEncodingUTF8);
	    break;
	case 's': /* SSID */
	    if (ssid != NULL) {
		fprintf(stderr, "SSID specified multiple times\n");
		goto done;
	    }
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	case 'S': /* securityType */
	    if (security_type != NULL) {
		fprintf(stderr, "securityType specified multiple times\n");
		goto done;
	    }
	    security_type = copy_wlan_security_type(optarg);
	    if (security_type == NULL) {
		fprintf(stderr, "securityType '%s' not recognized\n", optarg);
		goto done;
	    }
	    break;
	case 'c': /* trustedCertificate */
	    data = CFDataCreateWithFile(optarg);
	    if (data == NULL) {
		fprintf(stderr, "could not load file '%s'\n",
			optarg);
		goto done;
	    }
	    cert = SecCertificateCreateWithData(NULL, data);
	    if (cert == NULL) {
		my_CFRelease(&data);
		fprintf(stderr, "could not load certificate from file '%s'\n",
			optarg);
		goto done;
	    }
	    attrs = EAPSecCertificateCopyAttributesDictionary(cert);
	    if (attrs == NULL) {
		my_CFRelease(&data);
		fprintf(stderr, "bad certificate in file '%s'\n",
			optarg);
		goto done;
	    }
	    my_CFRelease(&attrs);
	    my_CFRelease(&cert);
	    if (trusted_certs == NULL) {
		trusted_certs 
		    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    if (CFArrayContainsValue(trusted_certs, trusted_certs_range,
				     data)) {
		my_CFRelease(&data);
		fprintf(stderr, "cert specified multiple times\n");
		goto done;
	    }
	    CFArrayAppendValue(trusted_certs, data);
	    my_CFRelease(&data);
	    trusted_certs_range.length++;
	    break;
	case 'n': /* trustedServerName */
	    str = CFStringCreateWithCString(NULL, optarg,
					    kCFStringEncodingUTF8);
	    if (trusted_server_names == NULL) {
		trusted_server_names 
		    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    if (CFArrayContainsValue(trusted_server_names, 
				     trusted_server_names_range,
				     str)) {
		CFRelease(str);
		fprintf(stderr, "server name '%s' specified multiple times\n",
			optarg);
		goto done;
	    }
	    CFArrayAppendValue(trusted_server_names, str);
	    trusted_server_names_range.length++;
	    CFRelease(str);
	    break;
	case 'o': /* oneTimePassword */
	    one_time_password = TRUE;
	    break;
	case 'O': /* outerIdentity */
	    if (outer_identity != NULL) {
		fprintf(stderr, "outerIdentity specified multiple times\n");
		goto done;
	    }
	    outer_identity = CFStringCreateWithCString(NULL, optarg,
						       kCFStringEncodingUTF8);
	    break;
	case 't': /* TTLSInnerAuthentication */
	    if (ttls_inner_auth != NULL) {
		fprintf(stderr,
			"TTLSInnerAuthentication specified multiple times\n");
		goto done;
	    }
	    ttls_inner_auth = copy_ttls_inner_auth(optarg);
	    if (ttls_inner_auth == NULL) {
		fprintf(stderr,
			"TTLSInnerAuthentication '%s' invalid\n", optarg);
		goto done;
	    }
	    break;
	case 'U': /* EAPFASTUsePAC */
	    eapfast_use_pac = TRUE;
	    break;
	case 'p': /* EAPFASTProvisionPAC */
	    eapfast_provision_pac = TRUE;
	    break;
	case 'A': /* EAPFASTProvisionPACAnonymously */
	    eapfast_provision_pac_anonymously = TRUE;
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (argc != 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (auth_types_range.length == 0) {
	fprintf(stderr, "--authType must be specified\n");
	goto done;
    }
    /* XXX check if TTLS is enabled, if not, ignore TTLSInnerAuthentication */
    if (ssid != NULL && security_type != NULL) {
	/* both specified */
    }
    else if (ssid != NULL || security_type != NULL) {
	fprintf(stderr, "both SSID and securityType must be specified\n");
	goto done;
    }

    auth_props = CFDictionaryCreateMutable(NULL, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(auth_props, kEAPClientPropAcceptEAPTypes, auth_types);

    /* SSID and securityType */
    if (trusted_certs != NULL) {
	CFDictionarySetValue(auth_props, kEAPClientPropTLSTrustedCertificates,
			     trusted_certs);
    }
    if (trusted_server_names != NULL) {
	CFDictionarySetValue(auth_props, kEAPClientPropTLSTrustedServerNames,
			     trusted_server_names);
    }
    if (one_time_password) {
	CFDictionarySetValue(auth_props, kEAPClientPropOneTimeUserPassword,
			     kCFBooleanTrue);
    }
    if (outer_identity != NULL) {
	CFDictionarySetValue(auth_props, kEAPClientPropOuterIdentity,
			     outer_identity);
    }
    if (ttls_inner_auth != NULL) {
	CFDictionarySetValue(auth_props, kEAPClientPropTTLSInnerAuthentication,
			     ttls_inner_auth);
    }

    /* XXX if EAP-FAST isn't enabled, don't bother with these */
    if (eapfast_use_pac) {
	CFDictionarySetValue(auth_props, kEAPClientPropEAPFASTUsePAC,
			     kCFBooleanTrue);
    }
    if (eapfast_provision_pac) {
	CFDictionarySetValue(auth_props,
			     kEAPClientPropEAPFASTProvisionPAC,
			     kCFBooleanTrue);
    }
    if (eapfast_provision_pac_anonymously) {
	CFDictionarySetValue(auth_props,
			     kEAPClientPropEAPFASTProvisionPACAnonymously,
			     kCFBooleanTrue);
    }
    cfg = configuration_open(TRUE);
    if (cfg == NULL) {
	fprintf(stderr, "EAPOLClientConfigurationCreate failed\n");
	goto done;
    }
    profile = EAPOLClientProfileCreate(cfg);
    if (user_defined_name == NULL) {
	if (ssid != NULL) {
	    user_defined_name = my_CFStringCreateWithData(ssid);
	}
	else {
	    user_defined_name = EAPOLClientProfileGetID(profile);
	    CFRetain(user_defined_name);
	}
    }
    if (user_defined_name != NULL) {
	EAPOLClientProfileSetUserDefinedName(profile, user_defined_name);
    }
    EAPOLClientProfileSetAuthenticationProperties(profile, auth_props);
    if (EAPOLClientProfileSetWLANSSIDAndSecurityType(profile, ssid,
						     security_type) == FALSE) {
	fprintf(stderr, "Profile with same SSID already present\n");
	goto done;
    }

    if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	fprintf(stderr,
		"Failed to save the configuration, %s\n",
		SCErrorString(SCError()));
	ret = 2;
	goto done;
    }
    SCPrint(TRUE, stdout, CFSTR("%@\n"), EAPOLClientProfileGetID(profile));
    ret = 0;

 done:
    my_CFRelease(&user_defined_name);
    my_CFRelease(&ssid);
    my_CFRelease(&security_type);
    my_CFRelease(&outer_identity);
    my_CFRelease(&ttls_inner_auth);
    my_CFRelease(&trusted_certs);
    my_CFRelease(&trusted_server_names);
    my_CFRelease(&profile);
    my_CFRelease(&auth_props);
    my_CFRelease(&auth_types);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_profile_information_set_clear_get(const char * command, int argc,
				    char * const * argv)
{
    CFStringRef			applicationID = NULL;
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    enum {
	kInfoUnspecified,
	kInfoGet,
	kInfoSet,
	kInfoClear
    } cmd = kInfoUnspecified;
    struct option 		longopts[] = {
	{ "applicationID", 	required_argument,	NULL,	'a' },
	{ "applicationid", 	required_argument,	NULL,	'a' },
	{ "information", 	required_argument, 	NULL, 	'i' },
	{ "profileID",		required_argument,	NULL,	'p' },
	{ "profileid",		required_argument,	NULL,	'p' },
	{ "SSID",		required_argument,	NULL,	's' },
	{ "ssid",		required_argument,	NULL,	's' },
	{ NULL,			0,			NULL,	0 }
    };
    CFPropertyListRef		plist = NULL;
    EAPOLClientProfileRef	profile = NULL;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    CFDataRef			ssid = NULL;

    if (strcmp(command, kCommandSetProfileInformation) == 0) {
	cmd = kInfoSet;
    }
    else if (strcmp(command, kCommandGetProfileInformation) == 0) {
	cmd = kInfoGet;
    }
    else if (strcmp(command, kCommandClearProfileInformation) == 0) {
	cmd = kInfoClear;
    }
    else {
	fprintf(stderr, "Internal error - unrecognized command '%s'\n",
		command);
	goto done;
    }
    while ((ch = getopt_long(argc, argv, "a:i:p:s:", longopts, NULL)) != -1) {
	CFDataRef		data;

	switch (ch) {
	case 'a':
	    if (applicationID != NULL) {
		fprintf(stderr, "applicationID specified twice\n");
		goto done;
	    }
	    applicationID = CFStringCreateWithCString(NULL, optarg,
						      kCFStringEncodingUTF8);
	    break;
	case 'p':
	    if (profileID != NULL) {
		fprintf(stderr, "profileID specified twice\n");
		goto done;
	    }
	    if (ssid != NULL) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (ssid != NULL) {
		fprintf(stderr, "SSID specified twice\n");
		goto done;
	    }
	    if (profileID != NULL) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	case 'i': /* information plist */
	    if (cmd != kInfoSet) {
		show_command_usage(command);
		goto done;
	    }
	    if (plist != NULL) {
		fprintf(stderr, "information specified multiple times\n");
		goto done;
	    }
	    data = CFDataCreateWithFile(optarg);
	    if (data == NULL) {
		fprintf(stderr, "could not load file '%s'\n",
			optarg);
		goto done;
	    }
	    plist = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
	    CFRelease(data);
	    if (isA_CFDictionary(plist) == NULL) {
		fprintf(stderr, "Contents of '%s are invalid'\n",
			optarg);
		goto done;
	    }
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (profileID == NULL && ssid == NULL) {
	fprintf(stderr, "No profile specified\n");
	show_command_usage(command);
	goto done;
    }
    if (applicationID == NULL) {
	fprintf(stderr, "applicationID not specified\n");
	show_command_usage(command);
	goto done;
    }
    if (cmd == kInfoSet) {
	if (plist == NULL) {
	    fprintf(stderr, "%s requires specifying --information\n",
		    command);
	    goto done;
	}
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = configuration_open(cmd != kInfoGet);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (profileID != NULL) {
	profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
    }
    else {
	profile = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid);
    }
    if (profile == NULL) {
	fprintf(stderr, "No such profile\n");
	goto done;
    }
    switch (cmd) {
    case kInfoGet:
	plist = EAPOLClientProfileGetInformation(profile, applicationID);
	if (plist == NULL) {
	    fprintf(stderr, "No information present\n");
	    goto done;
	}
	else {
	    fwrite_plist(stdout, plist);
	}
	break;
    case kInfoSet:
    case kInfoClear:
	if (EAPOLClientProfileSetInformation(profile, applicationID,
					     plist) == FALSE) {
	    fprintf(stderr, "Failed to %s information\n",
		    cmd == kInfoSet ? "set" : "clear");
	    goto done;
	}
	if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	    fprintf(stderr,
		    "Failed to save the configuration, %s\n",
		    SCErrorString(SCError()));
	    ret = 2;
	    goto done;
	}
	break;
    default:
	break;
    }
    ret = 0;

 done:
    my_CFRelease(&applicationID);
    my_CFRelease(&plist);
    my_CFRelease(&cfg);
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    return (ret);
}

STATIC int
S_profile_import(const char * command, int argc, char * const * argv)
{
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    CFDataRef			data = NULL;
    struct option 		longopts[] = {
	{ "replace",	no_argument,	NULL,	'r' },
	{ NULL,		0,		NULL,	0 }
    };
    CFArrayRef			matches = NULL;
    CFPropertyListRef		plist = NULL;
    EAPOLClientProfileRef	profile = NULL;
    bool			replace = FALSE;
    int				ret = 1;

    while ((ch = getopt_long(argc, argv, "r", longopts, NULL)) != -1) {
	switch (ch) {
	case 'r':
	    replace = TRUE;
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc > 1) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    data = CFDataCreateWithFile(argv[0]);
    if (data == NULL) {
	fprintf(stderr, "Can't read file '%s'\n", argv[0]);
	goto done;
    }
    plist = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
    if (plist == NULL) {
	fprintf(stderr, "Contents of '%s' are invalid\n", argv[0]);
	goto done;
    }
    profile = EAPOLClientProfileCreateWithPropertyList(plist);
    if (profile == NULL) {
	fprintf(stderr, "File '%s' does not contain a valid profile\n",
		argv[0]);
	goto done;
    }
    cfg = configuration_open(TRUE);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    matches = EAPOLClientConfigurationCopyMatchingProfiles(cfg, profile);
    if (matches != NULL) {
	int	count;
	int	i;

	if (replace == FALSE) {
	    fprintf(stderr, "Importing conflicts with existing profiles"
		    ", specify --replace to replace\n");
	    goto done;
	}
	count = CFArrayGetCount(matches);
	for (i = 0; i < count; i++) {
	    EAPOLClientProfileRef	matching_profile;

	    matching_profile = (EAPOLClientProfileRef)
		CFArrayGetValueAtIndex(matches, i);
	    if (EAPOLClientConfigurationRemoveProfile(cfg, matching_profile)
		== FALSE) {
		fprintf(stderr, "Failed to remove conflicting profile\n");
		goto done;
	    }
	}
    }
    if (EAPOLClientConfigurationAddProfile(cfg, profile) == FALSE) {
	fprintf(stderr, "Failed to import profile\n");
	goto done;
    }
    if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	fprintf(stderr,
		"Failed to save the configuration, %s\n",
		SCErrorString(SCError()));
	ret = 2;
	goto done;
    }

 done:
    my_CFRelease(&plist);
    my_CFRelease(&cfg);
    my_CFRelease(&profile);
    my_CFRelease(&matches);
    my_CFRelease(&data);
    my_CFRelease(&plist);
    return (ret);
}

STATIC bool
S_item_already_specified(bool d_flag, CFStringRef profileID, CFDataRef ssid)
{
    if (d_flag || profileID != NULL || ssid != NULL) {
	fprintf(stderr,	"Can't specify --profileID, --SSID, or -default"
		" more than once \n");
	return (TRUE);
    }
    return (FALSE);
}

STATIC bool
S_ensure_item_specified(bool d_flag, CFStringRef profileID, CFDataRef ssid)
{
    if (d_flag == FALSE && profileID == NULL && ssid == NULL) {
	fprintf(stderr,
		"Must specify one of --profileID, --SSID, or -default\n");
	return (FALSE);
    }
    return (TRUE);
}

STATIC EAPOLClientConfigurationRef
S_create_cfg_and_item(bool system_flag, bool d_flag,
		      CFStringRef profileID, CFDataRef ssid,
		      EAPOLClientItemIDRef * itemID_p)
{
    EAPOLClientConfigurationRef		cfg;
    EAPOLClientItemIDRef		itemID = NULL;
    EAPOLClientProfileRef		profile;

    cfg = configuration_open(system_flag);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (profileID != NULL) {
	profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	if (profile != NULL) {
	    itemID = EAPOLClientItemIDCreateWithProfile(profile);
	}
	else {
	    itemID = EAPOLClientItemIDCreateWithProfileID(profileID);
	}
    }
    else if (ssid != NULL) {
	profile = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid);
	if (profile != NULL) {
	    itemID = EAPOLClientItemIDCreateWithProfile(profile);
	}
	else {
	    itemID = EAPOLClientItemIDCreateWithWLANSSID(ssid);
	}
    }
    else if (d_flag) {
	itemID = EAPOLClientItemIDCreateDefault();
    }

 done:
    if (itemID == NULL) {
	my_CFRelease(&cfg);
    }
    *itemID_p = itemID;
    return (cfg);
}


STATIC int
S_password_item_set(const char * command, int argc, char * const * argv)
{
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    bool			d_flag = FALSE;
    EAPOLClientItemIDRef	itemID = NULL;
    struct option 		longopts[] = {
	{ "system",	no_argument,		NULL,	'S' },
	{ "default", 	no_argument,		NULL, 	'd' },
	{ "profileID",	required_argument,	NULL,	'p' },
	{ "profileid",	required_argument,	NULL,	'p' },
	{ "SSID",	required_argument,	NULL,	's' },
	{ "ssid",	required_argument,	NULL,	's' },
	{ "password", 	required_argument,	NULL,	'P' },
	{ "name",	required_argument,	NULL, 	'n' },
	{ NULL,		0,		NULL,	0 }
    };
    CFDataRef			name = NULL;
    bool			P_flag = FALSE;
    CFDataRef			password = NULL;
    EAPOLClientProfileRef	profile = NULL;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    bool			system_flag = FALSE;
    CFDataRef			ssid = NULL;

    while ((ch = getopt_long(argc, argv, "dSp:s:P:n:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'S':
	    system_flag = TRUE;
	    break;
	case 'd':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    d_flag = TRUE;
	    break;
	case 'p':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&profileID);
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&ssid);
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	case 'P':
	    if (P_flag) {
		fprintf(stderr, "password specified multiple times\n");
		goto done;
	    }
	    if (strcmp(optarg, "-") != 0) {
		password = CFDataCreate(NULL, (const UInt8 *)optarg,
					strlen(optarg));
	    }
	    P_flag = TRUE;
	    break;
	case 'n':
	    if (name != NULL) {
		fprintf(stderr, "name specified multiple times\n");
		goto done;
	    }
	    name = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (S_ensure_item_specified(d_flag, profileID, ssid) == FALSE) {
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = S_create_cfg_and_item(system_flag, d_flag, profileID, ssid, &itemID);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (P_flag && password == NULL) {
	char		buf[256];
	int		n;

	if (isatty(STDIN_FILENO)) {
	    if (readpassphrase("Enter password: ",
			       buf, sizeof(buf), 0) == NULL) {
		fprintf(stderr, "Failed to read password\n");
		goto done;
	    }
	    n = strlen(buf);
	}
	else {
	    n = read(STDIN_FILENO, buf, sizeof(buf));
	}
	if (n == 0) {
	    fprintf(stderr, "No password entered\n");
	    goto done;
	}
	password = CFDataCreate(NULL, (const UInt8 *)buf, n);
    }
    if (name == NULL && password == NULL) {
	fprintf(stderr, "Must specify at least one of name or password\n");
	show_command_usage(command);
	goto done;
    }
    if (EAPOLClientItemIDSetPasswordItem(itemID,
					 (system_flag ? kEAPOLClientDomainSystem
					  : kEAPOLClientDomainUser),
					 name, password) == FALSE) {
	fprintf(stderr, "Failed to set password item\n");
	goto done;
    }
    ret = 0;

 done:
    my_CFRelease(&itemID);
    my_CFRelease(&name);
    my_CFRelease(&password);
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_password_item_get(const char * command, int argc, char * const * argv)
{
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    bool			d_flag = FALSE;
    EAPOLClientItemIDRef	itemID = NULL;
    struct option 		longopts[] = {
	{ "system",	no_argument,		NULL,	'S' },
	{ "default", 	no_argument,		NULL, 	'd' },
	{ "profileID",	required_argument,	NULL,	'p' },
	{ "profileid",	required_argument,	NULL,	'p' },
	{ "SSID",	required_argument,	NULL,	's' },
	{ "ssid",	required_argument,	NULL,	's' },
	{ "password", 	no_argument,		NULL,	'P' },
	{ "name",	no_argument,		NULL, 	'n' },
	{ NULL,		0,			NULL,	0 }
    };
    bool			P_flag = FALSE;
    CFDataRef			password = NULL;
    EAPOLClientProfileRef	profile = NULL;
    CFStringRef			profileID = NULL;
    CFDataRef			name = NULL;
    bool			n_flag = FALSE;
    int				ret = 1;
    bool			system_flag = FALSE;
    CFDataRef			ssid = NULL;


    while ((ch = getopt_long(argc, argv, "Sp:s:Pn", longopts, NULL)) != -1) {
	switch (ch) {
	case 'S':
	    system_flag = TRUE;
	    break;
	case 'd':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    d_flag = TRUE;
	    break;
	case 'p':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&profileID);
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&ssid);
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	case 'P':
	    P_flag = TRUE;
	    break;
	case 'n':
	    n_flag = TRUE;
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (S_ensure_item_specified(d_flag, profileID, ssid) == FALSE) {
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (n_flag == FALSE && P_flag == FALSE) {
	fprintf(stderr, "Must specify at least one of name or password\n");
	show_command_usage(command);
	goto done;
    }
    cfg = S_create_cfg_and_item(system_flag, d_flag, profileID, ssid, &itemID);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (EAPOLClientItemIDCopyPasswordItem(itemID,
					  (system_flag
					   ? kEAPOLClientDomainSystem
					   : kEAPOLClientDomainUser),
					  n_flag ? &name : NULL, 
					  P_flag ? &password : NULL)
	== FALSE) {
	fprintf(stderr, "Failed to get password item\n");
	goto done;
    }
    if (n_flag && name != NULL) {
	printf("Name: ");
	fwrite(CFDataGetBytePtr(name), CFDataGetLength(name),
	       1, stdout);
	printf("\n");
    }
    if (P_flag && password != NULL) {
	printf("Password: ************\n");
    }
    ret = 0;

 done:
    my_CFRelease(&itemID);
    my_CFRelease(&name);
    my_CFRelease(&password);
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_password_item_remove(const char * command, int argc, char * const * argv)
{
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    bool			d_flag = FALSE;
    EAPOLClientItemIDRef	itemID = NULL;
    struct option 		longopts[] = {
	{ "system",	no_argument,		NULL,	'S' },
	{ "default", 	no_argument,		NULL, 	'd' },
	{ "profileID",	required_argument,	NULL,	'p' },
	{ "profileid",	required_argument,	NULL,	'p' },
	{ "SSID",	required_argument,	NULL,	's' },
	{ "ssid",	required_argument,	NULL,	's' },
	{ NULL,		0,		NULL,	0 }
    };
    EAPOLClientProfileRef	profile = NULL;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    bool			system_flag = FALSE;
    CFDataRef			ssid = NULL;

    while ((ch = getopt_long(argc, argv, "dSp:s:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'S':
	    system_flag = TRUE;
	    break;
	case 'd':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    d_flag = TRUE;
	    break;
	case 'p':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&profileID);
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&ssid);
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (S_ensure_item_specified(d_flag, profileID, ssid) == FALSE) {
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = S_create_cfg_and_item(system_flag, d_flag, profileID, ssid, &itemID);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (EAPOLClientItemIDRemovePasswordItem(itemID, 
					    (system_flag 
					     ? kEAPOLClientDomainSystem
					     : kEAPOLClientDomainUser))
	== FALSE) {
	fprintf(stderr, "Failed to remove password item\n");
	goto done;
    }
    ret = 0;

 done:
    my_CFRelease(&cfg);
    my_CFRelease(&itemID);
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    return (ret);
}

STATIC int
S_identity_set_clear_get(const char * command, int argc, char * const * argv)
{
    SecCertificateRef		cert = NULL;
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    bool			d_flag = FALSE;
    SecIdentityRef		identity = NULL;
    bool			is_set;
    EAPOLClientItemIDRef	itemID = NULL;
    struct option 		longopts[] = {
	{ "system",		no_argument,		NULL,	'S' },
	{ "default", 		no_argument,		NULL, 	'd' },
	{ "profileID",		required_argument,	NULL,	'p' },
	{ "profileid",		required_argument,	NULL,	'p' },
	{ "SSID",		required_argument,	NULL,	's' },
	{ "ssid",		required_argument,	NULL,	's' },
	{ "certificate", 	required_argument,	NULL,	'c' },
	{ NULL,			0,			NULL,	0 }
    };
    EAPOLClientProfileRef	profile = NULL;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    CFDataRef			ssid = NULL;
    OSStatus			status;
    bool			system_flag = FALSE;

    is_set = (strcmp(command, kCommandSetIdentity) == 0);
    while ((ch = getopt_long(argc, argv, "cd:Sp:s:", longopts, NULL)) != -1) {
	CFDictionaryRef 	attrs;
	CFDataRef		data;

	switch (ch) {
	case 'S':
	    system_flag = TRUE;
	    break;
	case 'd':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    d_flag = TRUE;
	    break;
	case 'p':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&profileID);
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&ssid);
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	case 'c': /* certificate */
	    if (!is_set) {
		show_command_usage(command);
		goto done;
	    }
	    data = CFDataCreateWithFile(optarg);
	    if (data == NULL) {
		fprintf(stderr, "could not load file '%s'\n",
			optarg);
		goto done;
	    }
	    cert = SecCertificateCreateWithData(NULL, data);
	    CFRelease(data);
	    if (cert == NULL) {
		fprintf(stderr, "could not load certificate from file '%s'\n",
			optarg);
		goto done;
	    }
	    attrs = EAPSecCertificateCopyAttributesDictionary(cert);
	    if (attrs == NULL) {
		fprintf(stderr, "bad certificate in file '%s'\n",
			optarg);
		goto done;
	    }
	    CFRelease(attrs);
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (S_ensure_item_specified(d_flag, profileID, ssid) == FALSE) {
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (is_set) {
	if (cert == NULL) {
	    fprintf(stderr, "certificate not specified\n");
	    goto done;
	}
	identity = find_identity_using_cert(system_flag
					    ? kEAPOLClientDomainSystem
					    : kEAPOLClientDomainUser, cert);
	if (identity == NULL) {
	    fprintf(stderr,
		    "The specified identity is not in %skeychain\n",
		    system_flag ? "the system ": "your ");
	    goto done;
	}
    }
    cfg = S_create_cfg_and_item(system_flag
				&& (strcmp(command, kCommandGetIdentity) != 0),
				d_flag, profileID, ssid, &itemID);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (strcmp(command, kCommandGetIdentity) == 0) {
	CFDictionaryRef		attrs;

	identity = EAPOLClientItemIDCopyIdentity(itemID,
						 (system_flag 
						  ? kEAPOLClientDomainSystem
						  : kEAPOLClientDomainUser));
	if (identity == NULL) {
	    fprintf(stderr, "No associated identity found\n");
	    goto done;
	}
	status = SecIdentityCopyCertificate(identity, &cert);
	if (status != noErr) {
	    /* this can't happen */
	    fprintf(stderr, "Identity has no associated certificate\n");
	    goto done;
	}
	attrs = EAPSecCertificateCopyAttributesDictionary(cert);
	if (attrs == NULL) {
	    fprintf(stderr, "Can't get certificate attributes\n");
	    goto done;
	}
	SCPrint(TRUE, stdout, CFSTR("%@\n"), attrs);
	CFRelease(attrs);
    }
    else {
	if (EAPOLClientItemIDSetIdentity(itemID,
					 (system_flag 
					  ? kEAPOLClientDomainSystem
					  : kEAPOLClientDomainUser),
					 identity) == FALSE) {
	    fprintf(stderr, "Failed to %s identity\n",
		    is_set ? "set" : "clear");
	    goto done;
	}
	if (identity != NULL) {
	    status = EAPOLClientSetACLForIdentity(identity);
	    if (status != noErr) {
		fprintf(stderr, "Failed to set ACL for identity, %d\n",
			(int)status);
	    }
	}
    }
    ret = 0;

 done:
    my_CFRelease(&cert);
    my_CFRelease(&identity);
    my_CFRelease(&cfg);
    my_CFRelease(&itemID);
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    return (ret);
}

STATIC int
S_set_loginwindow_profiles(const char * command, int argc, char * const * argv)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    CFStringRef			if_name = NULL;
    int				i;
    const char *		ifn = NULL;
    struct option 		longopts[] = {
	{ "interface",		required_argument,	NULL,	'i' },
	{ "profileID",		no_argument,		NULL,	'p' },
	{ "profileid",		no_argument,		NULL,	'p' },
	{ "SSID",		no_argument,		NULL,	's' },
	{ "ssid",		no_argument,		NULL,	's' },
	{ NULL,			0,			NULL,	0 }
    };
    bool			p_flag = FALSE;
    CFMutableArrayRef		profiles = NULL;
    int				ret = 1;
    bool			s_flag = FALSE;

    while ((ch = getopt_long(argc, argv, "i:p:s:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'i':
	    if (if_name != NULL) {
		fprintf(stderr, "interface specified twice\n");
		goto done;
	    }
	    ifn = optarg;
	    if_name = CFStringCreateWithCString(NULL, optarg,
						kCFStringEncodingASCII);
	    break;
	case 'p':
	    if (p_flag) {
		fprintf(stderr, "profileID specified twice\n");
		goto done;
	    }
	    if (s_flag) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    p_flag = TRUE;
	    break;
	case 's':
	    if (s_flag) {
		fprintf(stderr, "SSID specified twice\n");
		goto done;
	    }
	    if (p_flag) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    s_flag = TRUE;
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    argv += optind;
    if (!p_flag && !s_flag) {
	if (argc == 0) {
	    fprintf(stderr, "No profile specified\n");
	    show_command_usage(command);
	    goto done;
	}
	p_flag = TRUE;
    }
    if (if_name == NULL) {
	fprintf(stderr, "Interface must be specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = configuration_open(TRUE);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    profiles = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < argc; i++) {
	EAPOLClientProfileRef		profile;

	if (p_flag) {
	    CFStringRef			profileID = NULL;
	    
	    profileID = CFStringCreateWithCString(NULL, argv[i],
						  kCFStringEncodingUTF8);
	    profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
	    CFRelease(profileID);
	    if (profile == NULL) {
		fprintf(stderr, "No such profileID %s\n",
			argv[i]);
		goto done;
	    }
	}
	else {
	    CFDataRef			ssid = NULL;

	    ssid = CFDataCreate(NULL, (const UInt8 *)argv[i], strlen(argv[i]));
	    profile = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid);
	    CFRelease(ssid);
	    if (profile == NULL) {
		fprintf(stderr, "No profile with SSID '%s'\n",
			argv[i]);
		goto done;
	    }
	}
	CFArrayAppendValue(profiles, profile);
    }
    if (EAPOLClientConfigurationSetLoginWindowProfiles(cfg, if_name,
						       profiles) == FALSE) {
	fprintf(stderr, "Failed to set LoginWindowProfiles on '%s'\n", ifn);
	goto done;
    }
    if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	fprintf(stderr,
		"Failed to save the configuration, %s\n",
		SCErrorString(SCError()));
	ret = 2;
	goto done;
    }
    ret = 0;

 done:
    my_CFRelease(&if_name);
    my_CFRelease(&cfg);
    my_CFRelease(&profiles);
    return (ret);
}

STATIC int
S_get_clear_loginwindow_profiles(const char * command, 
				 int argc, char * const * argv)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    CFStringRef			if_name = NULL;
    const char *		ifn = NULL;
    bool			is_clear;
    struct option 		longopts[] = {
	{ "interface",		required_argument,	NULL,	'i' },
	{ NULL,			0,			NULL,	0 }
    };
    int				ret = 1;

    is_clear = (strcmp(command, kCommandClearLoginWindowProfiles) == 0);
    while ((ch = getopt_long(argc, argv, "i:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'i':
	    if (if_name != NULL) {
		fprintf(stderr, "interface specified twice\n");
		goto done;
	    }
	    ifn = optarg;
	    if_name = CFStringCreateWithCString(NULL, optarg,
						kCFStringEncodingASCII);
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (if_name == NULL) {
	fprintf(stderr, "Interface must be specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = configuration_open(is_clear);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (is_clear) {
	if (EAPOLClientConfigurationSetLoginWindowProfiles(cfg, if_name,
							   NULL) == FALSE) {
	    fprintf(stderr, "Failed to clear LoginWindowProfiles on '%s'\n",
		    ifn);
	    goto done;
	}
	if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	    fprintf(stderr,
		    "Failed to save the configuration, %s\n",
		    SCErrorString(SCError()));
	    ret = 2;
	    goto done;
	}
    }
    else {
	int				count;
	int				i;
	CFArrayRef			profiles = NULL;

	profiles
	    = EAPOLClientConfigurationCopyLoginWindowProfiles(cfg, if_name);
	if (profiles == NULL) {
	    fprintf(stderr, "No LoginWindow profiles on '%s'\n", ifn);
	    goto done;
	}
	count = CFArrayGetCount(profiles);
	for (i = 0; i < count; i++) {
	    EAPOLClientProfileRef	profile;
	    
	    profile = (EAPOLClientProfileRef)
		CFArrayGetValueAtIndex(profiles, i);
	    SCPrint(TRUE, stdout, CFSTR("%@\n"),
		    EAPOLClientProfileGetID(profile));
	}
	my_CFRelease(&profiles);
    }
    ret = 0;

 done:
    my_CFRelease(&if_name);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_set_system_profile(const char * command, int argc, char * const * argv)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    CFStringRef			if_name = NULL;
    const char *		ifn = NULL;
    struct option 		longopts[] = {
	{ "interface",		required_argument,	NULL,	'i' },
	{ "profileID",		required_argument,	NULL,	'p' },
	{ "profileid",		required_argument,	NULL,	'p' },
	{ "SSID",		required_argument,	NULL,	's' },
	{ "ssid",		required_argument,	NULL,	's' },
	{ NULL,			0,			NULL,	0 }
    };
    EAPOLClientProfileRef	profile;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    CFDataRef			ssid = NULL;

    while ((ch = getopt_long(argc, argv, "i:p:s:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'i':
	    if (if_name != NULL) {
		fprintf(stderr, "interface specified twice\n");
		goto done;
	    }
	    ifn = optarg;
	    if_name = CFStringCreateWithCString(NULL, optarg,
						kCFStringEncodingASCII);
	    break;
	case 'p':
	    if (profileID != NULL) {
		fprintf(stderr, "profileID specified twice\n");
		goto done;
	    }
	    if (ssid != NULL) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (ssid != NULL) {
		fprintf(stderr, "SSID specified twice\n");
		goto done;
	    }
	    if (profileID != NULL) {
		fprintf(stderr, "can't specify both profileID and SSID\n");
		goto done;
	    }
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (profileID == NULL && ssid == NULL) {
	fprintf(stderr, "No profile specified\n");
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (if_name == NULL) {
	fprintf(stderr, "Interface must be specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = configuration_open(TRUE);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (profileID != NULL) {
	profile = EAPOLClientConfigurationGetProfileWithID(cfg, profileID);
    }
    else {
	profile = EAPOLClientConfigurationGetProfileWithWLANSSID(cfg, ssid);
    }
    if (profile == NULL) {
	fprintf(stderr, "No such profile\n");
	ret = 1;
	goto done;
    }
    if (EAPOLClientConfigurationSetSystemProfile(cfg, if_name,
						 profile) == FALSE) {
	fprintf(stderr, "Failed to set System profile on '%s'\n", ifn);
	goto done;
    }
    if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	fprintf(stderr,
		"Failed to save the configuration, %s\n",
		SCErrorString(SCError()));
	ret = 2;
	goto done;
    }
    ret = 0;

 done:
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    my_CFRelease(&if_name);
    my_CFRelease(&cfg);
    return (ret);
}

STATIC int
S_get_clear_system_profile(const char * command, int argc, char * const * argv)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    CFStringRef			if_name = NULL;
    const char *		ifn = NULL;
    bool			is_clear;
    struct option 		longopts[] = {
	{ "interface",		required_argument,	NULL,	'i' },
	{ NULL,			0,			NULL,	0 }
    };
    int				ret = 1;

    is_clear = (strcmp(command, kCommandClearSystemProfile) == 0);
    while ((ch = getopt_long(argc, argv, "i:", longopts, NULL)) != -1) {
	switch (ch) {
	case 'i':
	    if (if_name != NULL) {
		fprintf(stderr, "interface specified twice\n");
		goto done;
	    }
	    ifn = optarg;
	    if_name = CFStringCreateWithCString(NULL, optarg,
						kCFStringEncodingASCII);
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (if_name == NULL) {
	fprintf(stderr, "Interface must be specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = configuration_open(is_clear);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (is_clear) {
	if (EAPOLClientConfigurationSetSystemProfile(cfg, if_name,
						     NULL) == FALSE) {
	    fprintf(stderr, "Failed to clear System profile on '%s'\n", ifn);
	    goto done;
	}
	if (EAPOLClientConfigurationSave(cfg) == FALSE) {
	    fprintf(stderr,
		    "Failed to save the configuration, %s\n",
		    SCErrorString(SCError()));
	    ret = 2;
	    goto done;
	}
    }
    else {
	EAPOLClientProfileRef	profile;

	profile = EAPOLClientConfigurationGetSystemProfile(cfg, if_name);
	if (profile == NULL) {
	    fprintf(stderr, "No System profile on '%s'\n", ifn);
	    goto done;
	}
	SCPrint(TRUE, stdout, CFSTR("%@\n"), EAPOLClientProfileGetID(profile));
    }
    ret = 0;

 done:
    my_CFRelease(&if_name);
    my_CFRelease(&cfg);
    return (ret);
}


STATIC int
S_get_system_loginwindow_interfaces(const char * command,
				    int argc, char * const * argv)
{
    int				ch;
    EAPOLClientConfigurationRef	cfg = NULL;
    int				count;
    bool			details = FALSE;
    int				i;
    const void * *		keys = NULL;
    struct option 		longopts[] = {
	{ "details",	no_argument,	NULL,	'd' },
	{ NULL,		0,		NULL,	0 }
    };
    CFDictionaryRef		profiles = NULL;
    int				ret = 1;

    while ((ch = getopt_long(argc, argv, "d", longopts, NULL)) != -1) {
	switch (ch) {
	case 'd':
	    details = TRUE;
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (argc != 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }

    cfg = EAPOLClientConfigurationCreate(NULL);
    if (cfg == NULL) {
	fprintf(stderr, "EAPOLClientConfigurationCreate failed\n");
	goto done;
    }

    if (strcmp(command, kCommandGetSystemInterfaces) == 0) {
	profiles = EAPOLClientConfigurationCopyAllSystemProfiles(cfg);
    }
    else {
	profiles = EAPOLClientConfigurationCopyAllLoginWindowProfiles(cfg);
    }
    if (profiles == NULL) {
	fprintf(stderr, "No interfaces\n");
	goto done;
    }
    count = CFDictionaryGetCount(profiles);
    if (details) {
	printf("There %s %d interface%s\n",
	       (count == 1) ? "is" : "are",
	       count,
	       (count == 1) ? "" : "s");
    }
    keys = (const void * *)malloc(sizeof(*keys) * count);
    CFDictionaryGetKeysAndValues(profiles, keys, NULL);
    for (i = 0; i < count; i++) {
	if (details == FALSE) {
	    SCPrint(TRUE, stdout, CFSTR("%@\n"), keys[i]);
	}
	else {
	    CFTypeRef	val;

	    val = CFDictionaryGetValue(profiles, keys[i]);
	    if (isA_CFArray(val) != NULL) {
		int		j;
		CFArrayRef	list = (CFArrayRef)val;
		int		profile_count;
		
		SCPrint(TRUE, stdout, CFSTR("%@: "), keys[i]);
		profile_count = CFArrayGetCount(list);
		for (j = 0; j < profile_count; j++) {
		    EAPOLClientProfileRef	profile;

		    profile = (EAPOLClientProfileRef)
			CFArrayGetValueAtIndex(list, j);
		    SCPrint(TRUE, stdout, CFSTR("%s%@"),
			    j == 0 ? "" : ", ",
			    EAPOLClientProfileGetID(profile));
		}
		printf("\n");
	    }
	    else {
		EAPOLClientProfileRef	profile;
		    
		profile = (EAPOLClientProfileRef)val;
		SCPrint(TRUE, stdout, CFSTR("%@: %@\n"), keys[i],
			EAPOLClientProfileGetID(profile));
	    }
	}
    }
    ret = 0;

 done:
    if (keys != NULL) {
	free(keys);
    }
    my_CFRelease(&profiles);
    my_CFRelease(&cfg);
    return (ret);

}


STATIC int
S_authentication_start(const char * command, int argc, char * const * argv)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    int				ch;
    bool			d_flag = FALSE;
    const char *		ifn = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    struct option 		longopts[] = {
	{ "system",		no_argument,		NULL,	'S' },
	{ "default", 		no_argument,		NULL, 	'd' },
	{ "interface",		required_argument,	NULL,	'i' },
	{ "profileID",		required_argument,	NULL,	'p' },
	{ "profileid",		required_argument,	NULL,	'p' },
	{ "SSID",		required_argument,	NULL,	's' },
	{ "ssid",		required_argument,	NULL,	's' },
	{ NULL,			0,			NULL,	0 }
    };
    EAPOLClientProfileRef	profile;
    CFStringRef			profileID = NULL;
    int				ret = 1;
    CFDataRef			ssid = NULL;
    bool			system_flag = FALSE;

    while ((ch = getopt_long(argc, argv, "di:p:s:S", longopts, NULL)) != -1) {
	switch (ch) {
	case 'S':
	    system_flag = TRUE;
	    break;
	case 'i':
	    if (ifn != NULL) {
		fprintf(stderr, "interface specified twice\n");
		goto done;
	    }
	    ifn = optarg;
	    break;
	case 'd':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    d_flag = TRUE;
	    break;
	case 'p':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&profileID);
	    profileID = CFStringCreateWithCString(NULL, optarg,
						  kCFStringEncodingUTF8);
	    break;
	case 's':
	    if (S_item_already_specified(d_flag, profileID, ssid)) {
		goto done;
	    }
	    my_CFRelease(&ssid);
	    ssid = CFDataCreate(NULL, (const UInt8 *)optarg, strlen(optarg));
	    break;
	default:
	    show_command_usage(command);
	    goto done;
	}
    }
    argc -= optind;
    /* argv += optind; */
    if (S_ensure_item_specified(d_flag, profileID, ssid) == FALSE) {
	show_command_usage(command);
	goto done;
    }
    if (argc > 0) {
	fprintf(stderr, "Too many arguments specified\n");
	show_command_usage(command);
	goto done;
    }
    if (ifn == NULL) {
	fprintf(stderr, "Interface must be specified\n");
	show_command_usage(command);
	goto done;
    }
    cfg = S_create_cfg_and_item(FALSE, d_flag, profileID, ssid, &itemID);
    if (cfg == NULL) {
	fprintf(stderr, "Can't open configuration\n");
	goto done;
    }
    if (system_flag) {
	ret = EAPOLControlStartSystemWithClientItemID(ifn, itemID);
    }
    else {
	ret = EAPOLControlStartWithClientItemID(ifn, itemID, NULL);
    }
    if (ret != 0) {
	fprintf(stderr, "Failed to start authentication client, %s (%d)\n",
		strerror(ret), ret);
	ret = 2;
    }
    else {
	ret = 0;
    }

 done:
    my_CFRelease(&itemID);
    my_CFRelease(&profileID);
    my_CFRelease(&ssid);
    my_CFRelease(&cfg);
    return (ret);
}

/**
 ** Main
 **/
STATIC const char * progname = NULL;

STATIC struct command_info 	commands[] = {
    { kCommandListProfiles, S_list_profiles, 0,
      "[ --details ]"
    },
    { kCommandShowProfile, S_profile_show_export_remove, 1,
      "--profileID <profileID> | --SSID <SSID>"
    },
    { kCommandExportProfile, S_profile_show_export_remove, 1,
      "--profileID <profileID> | --SSID <SSID>"
    },
    { kCommandRemoveProfile, S_profile_show_export_remove, 1,
      "--profileID <profileID> | --SSID <SSID>"
    },
    { kCommandImportProfile, S_profile_import, 1,
      "[ --replace ] <plist_file>"
    },
    { kCommandCreateProfile, S_profile_create, 1,
      "<options>\nwhere <options> are:\n"
      "--authType <auth_type> [ --authType <auth_type> ... ]\n"
      "[ --userDefinedName <user_defined_name> ]\n"
      "[ --SSID <SSID> --securityType <security_type> ]\n"
      "[ --trustedCertificate <cert_file> [ --trustedCertificate <cert_file> ... ] ]\n"
      "[ --trustedServerName <server_name> [ --trustedServerName <server_name> ... ] ]\n"
      "[ --outerIdentity <outer_identity> ]\n"
      "[ --oneTimePassword ]\n"
      "[ --TTLSInnerAuthentication <ttls_inner> ]\n"
      "[ --EAPFASTUsePAC ]\n"
      "[ --EAPFASTProvisionPAC ]\n"
      "[ --EAPFASTProvisionPACAnonymously ]\n"
      "\nWhere:\n"
      "<user_defined_name> is a string to identify the profile\n"
      "<auth_type> is either an integer or <auth_type_str>\n"
      "<auth_type_str> is \"TLS\", \"TTLS\", \"PEAP\", \"EAP-FAST\", or \"LEAP\"\n"
      "<security_type> is \"WEP\", \"WPA\", \"WPA2\", or \"Any\"\n"
      "<ttls_inner> is \"PAP\", \"CHAP\", \"MSCHAP\", or \"MSCHAPv2\""
    },
    { kCommandSetProfileInformation, S_profile_information_set_clear_get, 1,
      "--applicationID <applicationID> --information <plist_file>\n"
      "( --profileID <profileID> | --SSID <SSID> )"
    },
    { kCommandGetProfileInformation, S_profile_information_set_clear_get, 1,
      "--applicationID <applicationID>\n"
      "( --profileID <profileID> | --SSID <SSID> )"
    },
    { kCommandClearProfileInformation, S_profile_information_set_clear_get, 1,
      "--applicationID <applicationID>\n"
      "( --profileID <profileID> | --SSID <SSID> )"
    },
    { kCommandSetPasswordItem, S_password_item_set, 1,
      "[ --system ] [ --name <name>] [ --password <password> | \"-\" ] "
      "( --profileID <profileID> | --SSID <SSID> | --default )"
    },
    { kCommandGetPasswordItem, S_password_item_get, 1,
      "[ --system ] [ --name ] [ --password ] "
      "( --profileID <profileID> | --SSID <SSID> | --default )"
    },
    { kCommandRemovePasswordItem, S_password_item_remove, 1,
      "[ --system ] ( --profileID <profileID> | --SSID <SSID> | --default )"
    },
    { kCommandSetIdentity, S_identity_set_clear_get, 1,
      "[ --system ] --certificate <cert_file>"
      "( --profileID <profileID> | --SSID <SSID> | --default )"
    },
    { kCommandClearIdentity, S_identity_set_clear_get, 1,
      "[ --system ] ( --profileID <profileID> | --SSID <SSID> | --default ) "
    },
    { kCommandGetIdentity, S_identity_set_clear_get, 1,
      "[ --system ] ( --profileID <profileID> | --SSID <SSID> | --default ) "
    },
    { kCommandGetLoginWindowProfiles, S_get_clear_loginwindow_profiles, 1, 
      "--interface <ifname>" 
    },
    { kCommandSetLoginWindowProfiles, S_set_loginwindow_profiles, 1,
      "--interface <ifname> "
      "( --profileID <profileID> [ <profileID> ... ] "
      "| --SSID <SSID> [ <SSID ... ] )"
    },
    { kCommandClearLoginWindowProfiles, S_get_clear_loginwindow_profiles, 1,
      "--interface <ifname> "
    },
    { kCommandGetSystemProfile, S_get_clear_system_profile, 1,
      "--interface <ifname>" 
    },
    { kCommandSetSystemProfile, S_set_system_profile, 1,
      "--interface <ifname> "
      "( --profileID <profileID> | --SSID <SSID> )"
    },
    { kCommandClearSystemProfile, S_get_clear_system_profile, 1,
      "--interface <ifname>" 
    },
    { kCommandGetLoginWindowInterfaces, S_get_system_loginwindow_interfaces, 0,
      "--details"
    },
    { kCommandGetSystemInterfaces, S_get_system_loginwindow_interfaces, 0,
      "--details" },
    { kCommandStartAuthentication, S_authentication_start, 1,
      "[ --system ] --interface <ifname> "
      "( --profileID <profileID> | --SSID <SSID> | --default )"
    },
    { NULL, NULL, 0, NULL },
};

STATIC void
usage()
{
    int i;

    fprintf(stderr, "usage: %s <command> <args>\n", progname);
    fprintf(stderr, "where <command> is one of:\n");
    for (i = 0; commands[i].command; i++) {
	fprintf(stderr, "\t%s\n", commands[i].command);
    }
    exit(1);
}

STATIC struct command_info *
lookup_command_info(const char * cmd)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcasecmp(cmd, commands[i].command) == 0) {
	    return (commands + i);
	}
    }
    return (NULL);
}

STATIC void
show_command_usage(const char * cmd)
{
    struct command_info *	info;

    info = lookup_command_info(cmd);
    if (info == NULL || info->usage == NULL) {
	return;
    }
    fprintf(stderr, "usage: %s %s %s\n", progname, cmd, info->usage);
    return;
}

STATIC struct command_info *
lookup_func(const char * cmd, int argc)
{
    struct command_info *	info;

    info = lookup_command_info(cmd);
    if (info == NULL) {
	return (NULL);
    }
    if (argc < info->argc) {
	fprintf(stderr, "usage: %s %s %s\n", progname, info->command,
			info->usage ? info->usage : "");
	exit(1);
    }
    return (info);
}

int
main(int argc, char * const * argv)
{
    struct command_info *	info;
    const char *		slash;

    slash = strrchr(argv[0], '/');
    if (slash != NULL) {
	progname = slash + 1;
    }
    else {
	progname = argv[0];
    }
    if (argc < 2) {
	usage();
    }
    argv++; argc--;

    info = lookup_func(argv[0], argc - 1);
    if (info == NULL) {
	usage();
    }
    else {
	exit ((*info->func)(info->command, argc, argv));
    }
    return (0);
}
