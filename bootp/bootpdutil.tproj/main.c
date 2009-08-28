
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFArray.h>
#include <netinfo/ni.h>
#include "dhcp_options.h"
#include "subnets.h"
#include "bootpd-plist.h"
#include "cfutil.h"

#define NIDIR_CONFIG_DHCP		"/config/dhcp"
#define NIDIR_CONFIG_NETBOOTSERVER	"/config/NetBootServer"
#define NIDIR_CONFIG_DHCP_SUBNETS	"/config/dhcp/subnets"

#define NIPROP_NAME		"name"

/*
 * /config/dhcp:
 */
#define CFGPROP_BOOTP_ENABLED		"bootp_enabled"
#define CFGPROP_DHCP_ENABLED		"dhcp_enabled"
#define CFGPROP_OLD_NETBOOT_ENABLED	"old_netboot_enabled"
#define CFGPROP_NETBOOT_ENABLED		"netboot_enabled"
#define CFGPROP_RELAY_ENABLED		"relay_enabled"
#define CFGPROP_ALLOW			"allow"
#define CFGPROP_DENY			"deny"
#define CFGPROP_RELAY_IP_LIST		"relay_ip_list"
#define CFGPROP_DETECT_OTHER_DHCP_SERVER	"detect_other_dhcp_server"
#define CFGPROP_REPLY_THRESHOLD_SECONDS	"reply_threshold_seconds"

/*
 * /config/NetBootServer:
 */
#define CFGPROP_SHADOW_SIZE_MEG		"shadow_size_meg"
#define CFGPROP_AFP_USERS_MAX		"afp_users_max"
#define CFGPROP_AGE_TIME_SECONDS	"age_time_seconds"
#define CFGPROP_AFP_UID_START		"afp_uid_start"
#define CFGPROP_MACHINE_NAME_FORMAT	"machine_name_format"

/*
 * /config/dhcp/subnets/<subnet>:
 */

/*
 * /etc/bootpd.plist is an xml plist.  The structure is:
 *
 * -------------------+-------------------+------------------------------------
 * Top-level Key      | Type	          | NetInfo Source Directory
 * -------------------+-------------------+------------------------------------
 * (root)	      | <dict>	          | /config/dhcp
 * NetBoot            | <dict>	          | /config/NetBootServer
 * Subnets	      | <array> of <dict> | /config/dhcp/subnets
 * -------------------+-------------------+------------------------------------
 * 
 * (root) <dict>
 * --------------------------+----------------------------------------------
 * Property		     | Encoding	
 * --------------------------+----------------------------------------------
 * detect_other_dhcp_server  | numeric: <boolean> 0=>false 1=>true
 * --------------------------+----------------------------------------------
 * bootp_enabled	     | no value: <boolean> true
 * dhcp_enabled,	     | single empty value: <boolean> false
 * old_netboot_enabled,	     | 1 or more values: <array> of <string>
 * netboot_enabled,	     |
 * relay_enabled 	     |
 * --------------------------+----------------------------------------------
 * allow, deny,		     | <array> of <string>
 * relay_ip_list	     |
 * --------------------------+----------------------------------------------
 * reply_threshold_seconds   | <integer>
 * --------------------------+----------------------------------------------
 * 
 *
 * NetBoot <dict>
 * --------------------------+----------------------------------------------
 * Property		     | Encoding	
 * --------------------------+----------------------------------------------
 * shadow_size_meg	     | <integer>
 * afp_users_max	     |
 * age_time_seconds	     |
 * afp_uid_start	     |
 * --------------------------+----------------------------------------------
 * machine_name_format	     | <string>
 * --------------------------+----------------------------------------------
 *
 *
 * Subnets <array> of <dict>
 *
 * <dict> contains:
 * --------------------------+----------------------------------------------
 * Property		     | Encoding	
 * --------------------------+----------------------------------------------
 * name  		     | <string>
 * net_address		     |
 * net_mask		     |
 * supernet		     |
 * _creator		     |
 * --------------------------+----------------------------------------------
 * net_range		     | <array>[2] of <string>
 * --------------------------+----------------------------------------------
 * client_types		     | replace with "allocate" <boolean>
 *     			     | if client_types contains "dhcp"
 * 			     |     allocate = <true>
 *			     | else
 *			     |	   allocate = <false>
 * --------------------------+----------------------------------------------
 * lease_min, lease_max	     | <integer>
 * --------------------------+----------------------------------------------
 * dhcp_*		     | convert using dhcp option conversion table
 * --------------------------+----------------------------------------------
 */

ni_status
ni_read_dir(void * handle, char * path, ni_proplist * pl_p, ni_id * dir_id_p)
{
    ni_id		dir_id = {0,0};
    ni_status		status;

    NI_INIT(pl_p);
    status = ni_pathsearch(handle, &dir_id, path);
    if (status != NI_OK) {
	return (status);
    }
    status = ni_read(handle, &dir_id, pl_p);
    if (status != NI_OK) {
	return (status);
    }
    if (dir_id_p) {
	*dir_id_p = dir_id;
    }
    return (status);

}

typedef struct {
    ni_proplist *	ni_propids_props;
    u_long *		ni_propids_ids;
    int			ni_propids_len;
    ni_id		ni_propids_dir;
    void *		ni_propids_domain;
} ni_propids;

void
ni_propids_free(ni_propids * props_p)
{
    int i;

    for (i = 0; i < props_p->ni_propids_len; i++) {
	ni_proplist_free(props_p->ni_propids_props + i);
    }
    if (props_p->ni_propids_props != NULL) {
	free(props_p->ni_propids_props);
    }
    if (props_p->ni_propids_ids != NULL) {
	free(props_p->ni_propids_ids);
    }
    NI_INIT(props_p);
    return;
}

static ni_status
ni_propids_read(void * domain,
		const char * path, 
		ni_propids * props_p)
{
    ni_id		dir;
    int			i;
    ni_proplist *	p = NULL;
    ni_status 		status = NI_OK;
    ni_idlist		ids;

    NI_INIT(&ids);
    NI_INIT(props_p);

    props_p->ni_propids_domain = domain;

    status = ni_pathsearch(domain, &dir, (char *)path);
    if (status != NI_OK) {
	goto done;
    }

    props_p->ni_propids_dir = dir;

    status = ni_children(domain, &dir, &ids);
    if (status != NI_OK) {
	goto done;
    }
    if (ids.ni_idlist_len == 0) {
	goto done;
    }
    p = (ni_proplist *)malloc(sizeof(*p) * ids.ni_idlist_len);
    bzero(p, sizeof(*p) * ids.ni_idlist_len);
    for (i = 0; i < ids.ni_idlist_len; i++) {
	ni_id	d;

	d.nii_object = ids.ni_idlist_val[i];
	status = ni_read(domain, &d, p + i);
	if (status != NI_OK) {
	    int j;
	    for (j = 0; j < i; j++) {
		ni_proplist_free(p + j);
	    }
	    goto done;
	}
    }
    props_p->ni_propids_props = p;
    props_p->ni_propids_ids = ids.ni_idlist_val;
    props_p->ni_propids_len = ids.ni_idlist_len;
    ids.ni_idlist_val = NULL;
    ids.ni_idlist_len = 0;

 done:
    ni_idlist_free(&ids);
    if (status != NI_OK) {
	if (p != NULL) {
	    free(p);
	}
    }
    return (status);
}

static CFArrayRef
CFStringArrayCreateWithCStringArray(const char * * list, int count)
{
    int				i;
    CFMutableArrayRef		list_cf = NULL;

    if (count == 0) {
	goto done;
    }
    list_cf = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	CFStringRef	str;

	str = CFStringCreateWithCString(NULL, list[i], kCFStringEncodingUTF8);
	CFArrayAppendValue(list_cf, str);
	CFRelease(str);
    }
 done:
    return (list_cf);
}

static CFNumberRef
number_create(const char * str)
{
    unsigned long	val;
		    
    val = strtoul(str, NULL, 0);
    if (val != ULONG_MAX && errno != ERANGE) {
	return (CFNumberCreate(NULL, kCFNumberLongType, &val));
    }
    return (NULL);
}

static CFArrayRef
CFNumberArrayCreateWithCStringArray(const char * * list, int count)
{
    int				i;
    CFMutableArrayRef		list_cf = NULL;

    if (count == 0) {
	goto done;
    }
    list_cf = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	CFNumberRef	num;

	num = number_create(list[i]);
	if (num != NULL) {
	    CFArrayAppendValue(list_cf, num);
	    CFRelease(num);
	}
    }
 done:
    return (list_cf);
}

static CFDictionaryRef
config_dhcp_dict_create_from_proplist(ni_proplist * pl)
{
    CFMutableDictionaryRef	dict;
    int 			i;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (i = 0; i < pl->nipl_len; i++) {
	bool		handled = FALSE;
	CFArrayRef	list;
	ni_namelist * 	nl_p;
	ni_property * 	prop;
	const char *	prop_name;
	CFStringRef	prop_name_cf;

	prop = &(pl->nipl_val[i]);
	nl_p = &prop->nip_val;
	prop_name = prop->nip_name;
	if (strcmp(prop_name, NIPROP_NAME) == 0) {
	    continue;
	}
	prop_name_cf = CFStringCreateWithCString(NULL, prop_name,
						 kCFStringEncodingASCII);
	if ((strcmp(prop_name, CFGPROP_BOOTP_ENABLED) == 0
	     || strcmp(prop_name, CFGPROP_DHCP_ENABLED) == 0
	     || strcmp(prop_name, CFGPROP_OLD_NETBOOT_ENABLED) == 0
	     || strcmp(prop_name, CFGPROP_NETBOOT_ENABLED) == 0
	     || strcmp(prop_name, CFGPROP_RELAY_ENABLED) == 0)) {
	    switch (nl_p->ninl_len) {
	    case 0:
		handled = TRUE;
		CFDictionarySetValue(dict, prop_name_cf, kCFBooleanTrue);
		break;
	    case 1:
		if (strcmp(nl_p->ninl_val[0], "") == 0) {
		    handled = TRUE;
		    CFDictionarySetValue(dict, prop_name_cf, kCFBooleanFalse);
		    break;
		}
		break;
	    default:
		break;
	    }
	}
	else if (strcmp(prop_name, CFGPROP_DETECT_OTHER_DHCP_SERVER) == 0) {
	    handled = TRUE;
	    if (nl_p->ninl_len != 0 
		&& strtol(nl_p->ninl_val[0], NULL, 0) != 0) {
		CFDictionarySetValue(dict, prop_name_cf, kCFBooleanTrue);
	    }
	    else {
		CFDictionarySetValue(dict, prop_name_cf, kCFBooleanFalse);
	    }
	}
	else if (strcmp(prop_name, CFGPROP_REPLY_THRESHOLD_SECONDS) == 0) {
	    handled = TRUE;
	    if (nl_p->ninl_len != 0) {
		CFNumberRef	num;

		num = number_create(nl_p->ninl_val[0]);
		if (num != NULL) {
		    CFDictionarySetValue(dict, prop_name_cf, num);
		    CFRelease(num);
		}
	    }
	}

	if (handled == FALSE
	    && CFDictionaryContainsKey(dict, prop_name_cf) == FALSE
	    && nl_p->ninl_len > 0) {
	    list = CFStringArrayCreateWithCStringArray((const char * *)
						       nl_p->ninl_val,
						       nl_p->ninl_len);
	    CFDictionarySetValue(dict, prop_name_cf, list);
	    CFRelease(list);
	}
	CFRelease(prop_name_cf);
    }
    return (dict);
}

static CFDictionaryRef
config_dhcp_dict_create(void * domain)
{
    CFDictionaryRef		dict = NULL;
    ni_proplist			pl;
    ni_status			status;

    NI_INIT(&pl);
    status = ni_read_dir(domain, NIDIR_CONFIG_DHCP, &pl, NULL);
    if (status == NI_OK) {
	dict = config_dhcp_dict_create_from_proplist(&pl);
	ni_proplist_free(&pl);
    }
    return (dict);
}

static CFDictionaryRef
config_NetBootServer_dict_create_from_proplist(ni_proplist * pl)
{
    CFMutableDictionaryRef	dict;
    int 			i;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (i = 0; i < pl->nipl_len; i++) {
	ni_namelist * 	nl_p;
	ni_property * 	prop;
	const char *	prop_name;
	CFStringRef	prop_name_cf;

	prop = &(pl->nipl_val[i]);
	nl_p = &prop->nip_val;
	prop_name = prop->nip_name;
	if (strcmp(prop_name, NIPROP_NAME) == 0) {
	    continue;
	}
	prop_name_cf = CFStringCreateWithCString(NULL, prop_name,
						 kCFStringEncodingASCII);

	if (strcmp(CFGPROP_SHADOW_SIZE_MEG, prop_name) == 0
	    || strcmp(CFGPROP_AFP_USERS_MAX, prop_name) == 0
	    || strcmp(CFGPROP_AGE_TIME_SECONDS, prop_name) == 0
	    || strcmp(CFGPROP_AFP_UID_START, prop_name) == 0) {
	    if (nl_p->ninl_len != 0) {
		CFNumberRef	num;
	    
		num = number_create(nl_p->ninl_val[0]);
		if (num != NULL) {
		    CFDictionarySetValue(dict, prop_name_cf, num);
		    CFRelease(num);
		}
	    }
	}
	else if (strcmp(CFGPROP_MACHINE_NAME_FORMAT, prop_name) == 0
		 && nl_p->ninl_len != 0) {
	    CFStringRef	format;

	    format = CFStringCreateWithCString(NULL, nl_p->ninl_val[0],
					       kCFStringEncodingUTF8);
	    CFDictionarySetValue(dict, prop_name_cf, format);
	    CFRelease(format);
	}
	CFRelease(prop_name_cf);
    }
    return (dict);
}

static CFDictionaryRef
config_NetBootServer_dict_create(void * domain)
{
    CFDictionaryRef		dict = NULL;
    ni_proplist			pl;
    ni_status			status;

    NI_INIT(&pl);
    status = ni_read_dir(domain, NIDIR_CONFIG_NETBOOTSERVER, &pl, NULL);
    if (status == NI_OK) {
	dict = config_NetBootServer_dict_create_from_proplist(&pl);
	ni_proplist_free(&pl);
    }
    return (dict);
}

static bool
stringlist_contains_string(const char * * list, int list_len,
			   const char * str)
{
    int		i;

    for (i = 0; i < list_len; i++) {
	if (strcmp(list[i], str) == 0) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static CFDictionaryRef
subnet_dict_create_from_proplist(ni_proplist * pl_p)
{
    CFMutableDictionaryRef	dict;
    int 			i;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (i = 0; i < pl_p->nipl_len; i++) {
	bool		handled = FALSE;
	ni_namelist * 	nl_p;
	ni_property * 	prop;
	const char *	prop_name;
	CFStringRef	prop_name_cf;

	prop = &(pl_p->nipl_val[i]);
	nl_p = &prop->nip_val;
	prop_name = prop->nip_name;
	prop_name_cf = CFStringCreateWithCString(NULL, prop_name,
						 kCFStringEncodingASCII);
	
	if (strncmp(prop_name, "dhcp_", 5) == 0) {
	    dhcptag_t			tag;
	    const dhcptag_info_t *	tag_info;
	    const dhcptype_info_t *	type_info = NULL;

	    /* this is a DHCP option */
	    handled = TRUE;
	    tag = dhcptag_with_name(prop_name + 5);
	    if (tag != -1) {
		tag_info = dhcptag_info(tag);
		if (tag_info != NULL) {
		    type_info = dhcptype_info(tag_info->type);
		}
	    }
	    if (type_info != NULL) {
		switch (tag_info->type) {
		case dhcptype_bool_e:
		    if (nl_p->ninl_len != 0 
			&& strtol(nl_p->ninl_val[0], NULL, 0) != 0) {
			CFDictionarySetValue(dict, prop_name_cf,
					     kCFBooleanTrue);
		    }
		    else {
			CFDictionarySetValue(dict, prop_name_cf,
					     kCFBooleanFalse);
		    }
		    break;
		case dhcptype_int32_e:
		case dhcptype_uint8_e:
		case dhcptype_uint16_e:
		case dhcptype_uint32_e:
		    if (nl_p->ninl_len != 0) {
			CFNumberRef	num;
			num = number_create(nl_p->ninl_val[0]);
			if (num != NULL) {
			    CFDictionarySetValue(dict, prop_name_cf, num);
			    CFRelease(num);
			}
		    }
		    break;
		case dhcptype_uint8_mult_e:
		case dhcptype_uint16_mult_e: {
		    CFArrayRef	list;

		    list = CFNumberArrayCreateWithCStringArray((const char * *)
							       nl_p->ninl_val,
							       nl_p->ninl_len);
		    if (list != NULL) {
			CFDictionarySetValue(dict, prop_name_cf, list);
			CFRelease(list);
		    }
		    break;
		}
		case dhcptype_string_e:
		case dhcptype_ip_e: {
		    CFStringRef		str;

		    str = CFStringCreateWithCString(NULL, nl_p->ninl_val[0],
						    kCFStringEncodingASCII);
		    CFDictionarySetValue(dict, prop_name_cf, str);
		    CFRelease(str);

		    break;
		}
		case dhcptype_ip_mult_e:
		case dhcptype_ip_pairs_e:
		case dhcptype_dns_namelist_e:
		    handled = FALSE;
		    break;

		case dhcptype_opaque_e:
		default:
		    /* don't know what to do with these */
		    break;
		}
	    }
	}
	else if (strcmp(SUBNET_PROP_NAME, prop_name) == 0
		 || strcmp(SUBNET_PROP_NET_ADDRESS, prop_name) == 0
		 || strcmp(SUBNET_PROP_NET_MASK, prop_name) == 0
		 || strcmp(SUBNET_PROP_SUPERNET, prop_name) == 0
		 || strcmp(SUBNET_PROP__CREATOR, prop_name) == 0) {
	    CFStringRef		str;

	    str = CFStringCreateWithCString(NULL, nl_p->ninl_val[0],
					    kCFStringEncodingASCII);
	    CFDictionarySetValue(dict, prop_name_cf, str);
	    CFRelease(str);
	    handled = TRUE;
	}
	else if (strcmp(SUBNET_PROP_LEASE_MIN, prop_name) == 0
		 || strcmp(SUBNET_PROP_LEASE_MAX, prop_name) == 0) {
	    handled = TRUE;
	    if (nl_p->ninl_len != 0) {
		CFNumberRef	num;
		num = number_create(nl_p->ninl_val[0]);
		if (num != NULL) {
		    CFDictionarySetValue(dict, prop_name_cf, num);
		    CFRelease(num);
		}
		else {
		    printf("bad conversion of %s\n", prop_name);
		}
	    }
	}
	else if (strcmp(SUBNET_PROP_CLIENT_TYPES, prop_name) == 0) {
	    if (stringlist_contains_string((const char * *)nl_p->ninl_val,
					   nl_p->ninl_len,
					   "dhcp")) {
		CFDictionarySetValue(dict, CFSTR("allocate"),
				     kCFBooleanTrue);
	    }
	    handled = TRUE;
	}
	if (handled == FALSE
	    && nl_p->ninl_len > 0) {
	    CFArrayRef	list;
	    list = CFStringArrayCreateWithCStringArray((const char * *)
						       nl_p->ninl_val,
						       nl_p->ninl_len);
	    CFDictionarySetValue(dict, prop_name_cf, list);
	    CFRelease(list);
	}
	CFRelease(prop_name_cf);
    }
    return (dict);
}

static CFArrayRef
config_dhcp_subnets_list_create(void * domain)
{
    int 			i;
    CFMutableArrayRef		list = NULL;
    ni_propids			subnets;
    ni_status			status;

    status = ni_propids_read(domain, NIDIR_CONFIG_DHCP_SUBNETS, &subnets);
    if (status != NI_OK) {
	return (NULL);
    }
    for (i = 0; i < subnets.ni_propids_len; i++) {
	CFDictionaryRef	dict;
	ni_proplist *	pl_p = subnets.ni_propids_props + i;

	dict = subnet_dict_create_from_proplist(pl_p);
	if (dict != NULL) {
	    if (list == NULL) {
		list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    CFArrayAppendValue(list, dict);
	    CFRelease(dict);
	}
    }
    ni_propids_free(&subnets);
    return (list);

}

int
main()
{
    CFMutableDictionaryRef	config;
    CFDataRef			data;
    CFDictionaryRef		dict;
    CFArrayRef			list;
    void *			ni_local;
    ni_status			status;

    status = ni_open(NULL, ".", &ni_local);
    if (status != NI_OK) {
	fprintf(stderr, "ni_open . failed, %s\n", ni_error(status));
	exit(1);
    }
    dict = config_dhcp_dict_create(ni_local);
    if (dict != NULL) {
	config = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFRelease(dict);
    }
    else {
	config = CFDictionaryCreateMutable(NULL, 0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
    }
    dict = config_NetBootServer_dict_create(ni_local);
    if (dict != NULL) {
	CFDictionarySetValue(config, BOOTPD_PLIST_NETBOOT, dict);
	CFRelease(dict);
    }
    list = config_dhcp_subnets_list_create(ni_local);
    if (list != NULL) {
	CFDictionarySetValue(config, BOOTPD_PLIST_SUBNETS, list);
	CFRelease(list);
    }
    data = CFPropertyListCreateXMLData(NULL, config);
    CFRelease(config);
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, stdout);
    CFRelease(data);
    exit(0);
}
