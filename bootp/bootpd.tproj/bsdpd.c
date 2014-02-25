/*
 * Copyright (c) 1999-2013 Apple Inc. All rights reserved.
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
 * bsdpd.c
 * - NetBoot Server implementing Boot Server Discovery Protocol (BSDP)
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com)		November 24, 1999
 * - created
 * Dieter Siegmund (dieter@apple.com)		September 6, 2001
 * - added AFP user/shadow file reclamation/aging
 * Dieter Siegmund (dieter@apple.com)		April 10, 2002
 * - added multiple image support and support for Mac OS X NetBoot
 * Dieter Siegmund (dieter@apple.com)		February 20, 2003
 * - added support for diskless X netboot
 * Dieter Siegmund (dieter@apple.com)		April 15, 2003
 * - added support for HTTP netboot
 * Dieter Siegmund (dieter@apple.com)		December 4, 2003
 * - added support for BootFileOnly images
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include <netinet/if_ether.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <mach/boolean.h>
#include <sys/errno.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#include "subnets.h"
#include "afp.h"
#include "bsdp.h"
#include "bsdplib.h"
#include "host_identifier.h"
#include "interfaces.h"
#include "dhcpd.h"
#include "globals.h"
#include "bootp_transmit.h"
#include "netinfo.h"
#include "bsdpd.h"
#include "bootpd.h"
#include "macNC.h"
#include "macnc_options.h"
#include "nbsp.h"
#include "nbimages.h"
#include "NICache.h"
#include "NICachePrivate.h"
#include "AFPUsers.h"
#include "NetBootServer.h"
#include "bootpd-plist.h"
#include "cfutil.h"
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#define ARCH_PPC			"ppc"

#define CFGPROP_SHADOW_SIZE_MEG		"shadow_size_meg"
#define CFGPROP_AFP_USERS_MAX		"afp_users_max"
#define CFGPROP_AGE_TIME_SECONDS	"age_time_seconds"
#define CFGPROP_AFP_UID_START		"afp_uid_start"
#define CFGPROP_MACHINE_NAME_FORMAT	"machine_name_format"

#define DEFAULT_MACHINE_NAME_FORMAT	"NetBoot%03d"

#define AGE_TIME_SECONDS	(60 * 15)
#define AFP_USERS_MAX		50
#define ADMIN_GROUP_NAME	"admin"

typedef struct {
    PLCache_t		list;
} BSDPClients_t;

/* global variables */
gid_t			G_admin_gid = 0;
boolean_t		G_disk_space_warned = FALSE;
uint32_t		G_shadow_size_meg = SHADOW_SIZE_DEFAULT;
NBSPListRef		G_client_sharepoints = NULL;
NBImageListRef		G_image_list = NULL;

/* local variables */
static uint32_t		S_age_time_seconds = AGE_TIME_SECONDS;
static gid_t		S_netboot_gid;
static BSDPClients_t	S_clients;
static AFPUserList	S_afp_users;
static uint32_t		S_afp_users_max = AFP_USERS_MAX;
static NBSPListRef	S_sharepoints = NULL;
static char *		S_machine_name_format;

#define AFP_UID_START	100
static uint32_t		S_afp_uid_start = AFP_UID_START;
static int		S_next_host_number = 0;

void
BSDPClients_free(BSDPClients_t * clients)
{
    PLCache_free(&clients->list);
    bzero(clients, sizeof(*clients));
}

#define BSDP_CLIENTS_FILE	"/var/db/bsdpd_clients"
boolean_t
BSDPClients_init(BSDPClients_t * clients)
{
    bzero(clients, sizeof(*clients));
    PLCache_init(&clients->list);
#define ARBITRARILY_LARGE_NUMBER	(100 * 1024 * 1024)
    PLCache_set_max(&clients->list, ARBITRARILY_LARGE_NUMBER);

    if (PLCache_read(&clients->list, BSDP_CLIENTS_FILE) == FALSE) {
	goto failed;
    }
    return (TRUE);
 failed:
    BSDPClients_free(clients);
    return (FALSE);
}

static int
S_host_number_max()
{
    PLCacheEntry_t * 	scan;
    int			max_number = 0;

    for (scan = S_clients.list.head; scan; scan = scan->next) {
	int			number_index;
	ni_namelist *		number_nl_p;
	int			val;

	number_index = ni_proplist_match(scan->pl, NIPROP_NETBOOT_NUMBER, NULL);
	if (number_index == NI_INDEX_NULL)
	    continue; /* this can't happen */
	number_nl_p = &scan->pl.nipl_val[number_index].nip_val;
	val = strtol(number_nl_p->ninl_val[0], NULL, 0);
	if (val > max_number) {
	    max_number = val;
	}
    }
    return (max_number);
}

static boolean_t
S_gid_taken(ODNodeRef node, CFStringRef gid)
{
    CFErrorRef	error;
    boolean_t	taken	= FALSE;
    ODQueryRef	query;
    CFArrayRef	results;

    query = ODQueryCreateWithNode(NULL,
				  node,					// inNode
				  CFSTR(kDSStdRecordTypeGroups),	// inRecordTypeOrList
				  CFSTR(kDS1AttrPrimaryGroupID),	// inAttribute
				  kODMatchEqualTo,			// inMatchType
				  gid,					// inQueryValueOrList
				  NULL,					// inReturnAttributeOrList
				  0,					// inMaxResults
				  &error);
    if (query == NULL) {
	my_log(LOG_INFO, "bsdpd: S_gid_taken: ODQueryCreateWithNode() failed");
	my_CFRelease(&error);
	goto failed;
    }

    results = ODQueryCopyResults(query, FALSE, &error);
    CFRelease(query);
    if (results == NULL) {
	my_log(LOG_INFO, "bsdpd: S_gid_taken: ODQueryCopyResults() failed");
	my_CFRelease(&error);
	goto failed;
    }

    if (CFArrayGetCount(results) > 0) {
	taken = TRUE;
    }
    CFRelease(results);

 failed:
    return (taken);
}

static void
_myCFDictionarySetStringValueAsArray(CFMutableDictionaryRef dict,
				     CFStringRef key,  CFStringRef str)
{
    CFArrayRef			array;

    array = CFArrayCreate(NULL, (const void **)&str,
			  1, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(dict, key, array);
    CFRelease(array);
    return;
}


static boolean_t
S_create_netboot_group(gid_t preferred_gid, gid_t * actual_gid)
{
    CFErrorRef	error	= NULL;
    ODNodeRef	node;
    boolean_t	ret	= FALSE;
    gid_t	scan;

    node = ODNodeCreateWithNodeType(NULL, kODSessionDefault,
				    kODNodeTypeLocalNodes, &error);
    if (node == NULL) {
	my_log(LOG_INFO, "bsdpd: S_create_netboot_group:"
	       " ODNodeCreateWithNodeType() failed");
	return (FALSE);
    }

    for (scan = preferred_gid; !ret; scan++) {
	CFMutableDictionaryRef	attributes;
	char			buf[64];
	ODRecordRef		record	= NULL;
	CFStringRef		gidStr;

	snprintf(buf, sizeof(buf), "%d", scan);
	gidStr = CFStringCreateWithCString(NULL, buf, kCFStringEncodingASCII);
	if (S_gid_taken(node, gidStr)) {
	    goto nextGid;
	}
	attributes 
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	_myCFDictionarySetStringValueAsArray(attributes,
					     CFSTR(kDS1AttrPrimaryGroupID),
					     gidStr);
	_myCFDictionarySetStringValueAsArray(attributes,
					     CFSTR(kDS1AttrPassword),
					     CFSTR("*"));
	record = ODNodeCreateRecord(node,
				    CFSTR(kDSStdRecordTypeGroups),
				    CFSTR(NETBOOT_GROUP),
				    attributes,
				    &error);
	CFRelease(attributes);
	if (record == NULL) {
	    CFRelease(gidStr);
	    my_log(LOG_INFO,
		   "bsdpd: S_create_netboot_group:"
		   " ODNodeCreateRecord() failed");
	    goto done;
	}

	if (!ODRecordSynchronize(record, &error)) {
	    CFRelease(gidStr);
	    my_log(LOG_INFO,
		   "bsdpd: S_create_netboot_group:"
		   " ODRecordSynchronize() failed");
	    goto done;
	}

	ret = TRUE;

     nextGid:
	my_CFRelease(&record);
	my_CFRelease(&gidStr);
	if (error != NULL) {
	    my_CFRelease(&error);
	    goto done;
	}
    }

 done:
    my_CFRelease(&node);
    return (ret);
}

static boolean_t
S_host_format_valid(const char * format)
{
    char	buf1[256];
    char	buf2[256];

    snprintf(buf1, sizeof(buf1), format, 0);
    snprintf(buf2, sizeof(buf2), format, 999);
    if (strcmp(buf1, buf2) == 0) {
	return (FALSE);
    }
    return (TRUE);
}

static void
S_read_config(CFDictionaryRef plist)
{
    CFTypeRef		prop;

    my_log(LOG_INFO, "bsdpd: re-reading configuration");

    G_shadow_size_meg = SHADOW_SIZE_DEFAULT;
    S_afp_users_max = AFP_USERS_MAX;
    S_afp_uid_start = AFP_UID_START;
    S_age_time_seconds = AGE_TIME_SECONDS;
    if (S_machine_name_format != NULL) {
	free(S_machine_name_format);
	S_machine_name_format = NULL;
    }
    if (plist != NULL) {
	set_number_from_plist(plist, CFSTR(CFGPROP_SHADOW_SIZE_MEG),
			      CFGPROP_SHADOW_SIZE_MEG, 
			      &G_shadow_size_meg);
	set_number_from_plist(plist, CFSTR(CFGPROP_AFP_USERS_MAX),
			      CFGPROP_AFP_USERS_MAX, 
			      &S_afp_users_max);
	set_number_from_plist(plist, CFSTR(CFGPROP_AFP_UID_START),
			      CFGPROP_AFP_UID_START, 
			      &S_afp_uid_start);
	set_number_from_plist(plist, CFSTR(CFGPROP_AGE_TIME_SECONDS),
			      CFGPROP_AGE_TIME_SECONDS, 
			      &S_age_time_seconds);
	prop = CFDictionaryGetValue(plist, CFSTR(CFGPROP_MACHINE_NAME_FORMAT));
	if (isA_CFString(prop) != NULL) {
	    char	host_format[256];

	    if (CFStringGetCString(prop, host_format, sizeof(host_format),
				   kCFStringEncodingUTF8)
		&& S_host_format_valid(host_format)) {
		S_machine_name_format = strdup(host_format);
	    }
	    else {
		my_log(LOG_NOTICE, "Invalid '%s' property",
		       CFGPROP_MACHINE_NAME_FORMAT);
	    }
	}
    }
    if (S_machine_name_format == NULL) {
	S_machine_name_format = strdup(DEFAULT_MACHINE_NAME_FORMAT);
    }

    my_log(LOG_INFO, 
	   "bsdpd: shadow file size will be set to %d megabytes",
	   G_shadow_size_meg);
    {
	u_int32_t	hours = 0;
	u_int32_t	minutes = 0;
	u_int32_t	seconds = 0;
	u_int32_t	remainder = S_age_time_seconds;

#define SECS_PER_MINUTE	60
#define SECS_PER_HOUR	(60 * SECS_PER_MINUTE)

	hours = remainder / SECS_PER_HOUR;
	remainder = remainder % SECS_PER_HOUR;
	if (remainder > 0) {
	    minutes = remainder / SECS_PER_MINUTE;
	    remainder = remainder % SECS_PER_MINUTE;
	    seconds = remainder;
	}
	my_log(LOG_INFO,
	       "bsdpd: age time %02u:%02u:%02u", hours, minutes, seconds);
    }
    return;
}

static void
S_set_sharepoint_permissions(NBSPListRef list, uid_t user, gid_t group)
{
    int 		i;
	
    for (i = 0; i < NBSPList_count(list); i++) {
	NBSPEntry * 	entry = NBSPList_element(list, i);
	struct stat	sb;

	/*
	 * Verify permissions/ownership
	 */
	if (set_privs(entry->path, &sb, user, group, SHARED_DIR_PERMS, 
		      FALSE) == FALSE
	    && entry->is_readonly == FALSE) {
	    my_log(LOG_INFO, "bsdpd: setting permissions on '%s' failed: %m", 
		   entry->path);
	}
    }
    return;
}

static void
S_set_bootfile_permissions(NBImageEntryRef entry, const char * dir,
			   uid_t user, gid_t group)
{
    int 	i;
    char	path[PATH_MAX];
    struct stat	sb;

    /* set permissions on bootfile */
    for (i = 0; i < entry->archlist_count; i++) {
	const char *	arch = entry->archlist[i];
    
	if (strcmp(arch, ARCH_PPC) == 0 && entry->ppc_bootfile_no_subdir) {
	    snprintf(path, sizeof(path), "%s/%s", dir, entry->bootfile);
	}
	else {
	    snprintf(path, sizeof(path), "%s/%s/%s", dir, arch, 
		     entry->bootfile);
	}
	if (set_privs(path, &sb, user, group, SHARED_FILE_PERMS, FALSE) == FALSE
	    && entry->sharepoint->is_readonly == FALSE) {
	    my_log(LOG_INFO, "bsdpd: setting permissions on '%s' failed: %m", 
		   path);
	}
    }
    return;
}

static void
S_set_image_permissions(NBImageListRef list, uid_t user, gid_t group)
{
    int 		i;
    char		dir[PATH_MAX];
    char		file[PATH_MAX];
	
    for (i = 0; i < NBImageList_count(list); i++) {
	NBImageEntryRef entry = NBImageList_element(list, i);
	struct stat	sb;

	/* set permissions on .nbi directory */
	snprintf(dir, sizeof(dir), "%s/%s", entry->sharepoint->path,
		 entry->dir_name);
	if (set_privs(dir, &sb, user, group, SHARED_DIR_PERMS, FALSE) == FALSE
	    && entry->sharepoint->is_readonly == FALSE) {
	    my_log(LOG_INFO, "bsdpd: setting permissions on '%s' failed: %m", 
		   dir);
	}
	S_set_bootfile_permissions(entry, dir, user, group);
	switch (entry->type) {
	case kNBImageTypeClassic:
	    /* set permissions on shared image */
	    snprintf(file, sizeof(file), "%s/%s", dir, 
		     entry->type_info.classic.shared);
	    if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		== FALSE
		&& entry->sharepoint->is_readonly == FALSE) {
		my_log(LOG_INFO, 
		       "bsdpd: setting permissions on '%s' failed: %m", 
		       file);
	    }
	    /* set permissions on private image */
	    if (entry->type_info.classic.private != NULL) {
		snprintf(file, sizeof(file), "%s/%s", dir, 
			 entry->type_info.classic.private);
		if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		    == FALSE
		    && entry->sharepoint->is_readonly == FALSE) {
		    my_log(LOG_INFO, 
			   "bsdpd: setting permissions on '%s' failed: %m", 
			   file);
		}
	    }
	    break;

	case kNBImageTypeHTTP:
	    if (entry->type_info.http.indirect == FALSE) {
		/* set the permissions on the root image */
		snprintf(file, sizeof(file), "%s/%s", dir, 
			 entry->type_info.http.root_path);
		if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		    == FALSE
		    && entry->sharepoint->is_readonly == FALSE) {
		    my_log(LOG_INFO, 
			   "bsdpd: setting permissions on '%s' failed: %m", 
			   file);
		}
	    }
	    break;

	case kNBImageTypeNFS:
	    if (entry->type_info.nfs.indirect == FALSE) {
		/* set the permissions on the root image */
		snprintf(file, sizeof(file), "%s/%s", dir, 
			 entry->type_info.nfs.root_path);
		if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		    == FALSE
		    && entry->sharepoint->is_readonly == FALSE) {
		    my_log(LOG_INFO, 
			   "bsdpd: setting permissions on '%s' failed: %m", 
			   file);
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return;
}

static boolean_t
S_insert_image_list(const char * arch, const char * sysid, 
		    const struct ether_addr * ether,
		    const u_int16_t * attr_filter_list,
		    int n_attr_filter_list, dhcpoa_t * options, 
		    dhcpoa_t * bsdp_options)
{
    char			buf[DHCP_OPTION_SIZE_MAX];
    int				freespace;
    int 			i;
    int				image_count;
    char *			offset;

    if (G_image_list == NULL) {
	goto done;
    }

    /* space available for options minus size of tag/len (2) */
    freespace = dhcpoa_freespace(bsdp_options) - OPTION_OFFSET;
    offset = buf;
    image_count = 0;
    for (i = 0; i < NBImageList_count(G_image_list); i++) {
	char				descr_buf[255];
	bsdp_image_description_t *	descr_p = (void *)descr_buf;
	int				descr_len;
	NBImageEntryRef			image_entry;
	int				name_length;

	/*
	 * If the client's system identifier is not supported by the image, 
	 * or the image attributes don't match those requested by the client,
	 * don't supply the image.
	 */
	image_entry = NBImageList_element(G_image_list, i);
	if (!NBImageEntry_supported_sysid(image_entry, arch, sysid, ether)
	    || !NBImageEntry_attributes_match(image_entry, attr_filter_list,
					      n_attr_filter_list)) {
	    continue;
	}
	name_length = image_entry->name_length;
	if (name_length > BSDP_IMAGE_NAME_MAX)
	    name_length = BSDP_IMAGE_NAME_MAX;
	descr_p->name_length = name_length;
	*((bsdp_image_id_t *)(descr_p->boot_image_id))
	    = htonl(image_entry->image_id);
	bcopy(image_entry->name, descr_p->name, name_length);
	descr_len = sizeof(*descr_p) + name_length;
	if (descr_len > freespace) {
	    int		space;

	    if (offset > buf) {
		if (dhcpoa_add(bsdp_options, bsdptag_boot_image_list_e,
			       offset - buf, buf) != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: add BSDP boot image list failed, %s",
			   dhcpoa_err(bsdp_options));
		    return (FALSE);
		}
	    }
	    if (dhcpoa_used(bsdp_options) > 0
		&& dhcpoa_add(options, dhcptag_vendor_specific_e,
			      dhcpoa_used(bsdp_options),
			      dhcpoa_buffer(bsdp_options))
		!= dhcpoa_success_e) {
		my_log(LOG_INFO, 
		       "NetBoot: add vendor specific failed, %s",
		       dhcpoa_err(options));
		return (FALSE);
	    }
	    space = dhcpoa_freespace(options) - OPTION_OFFSET;
	    if (space > DHCP_OPTION_SIZE_MAX) {
		space = DHCP_OPTION_SIZE_MAX;
	    }
	    freespace = space - OPTION_OFFSET; /* leave room for tag/len */
	    if (descr_len > freespace) {
		/* the packet is full */
		my_log(LOG_NOTICE,
		       "NetBoot: image list truncated to first %d images",
		       image_count);
		return (TRUE);
	    }
	    dhcpoa_init_no_end(bsdp_options, dhcpoa_buffer(bsdp_options), 
			       space);
	    offset = buf;
	}
	image_count++;
	bcopy(descr_p, offset, descr_len);
	offset += descr_len;
	freespace -= descr_len;
    }

    /* add trailing image description(s) */
    if (offset > buf) {
	if (dhcpoa_add(bsdp_options, bsdptag_boot_image_list_e,
		       offset - buf, buf) != dhcpoa_success_e) {
	    my_log(LOG_INFO, 
		   "NetBoot: add BSDP boot image list failed, %s",
		   dhcpoa_err(bsdp_options));
	    return (FALSE);
	}
    }
 done:
    /* add the BSDP options to the packet */
    if (dhcpoa_used(bsdp_options) > 0) {
	if (dhcpoa_add(options, dhcptag_vendor_specific_e,
		       dhcpoa_used(bsdp_options),
		       dhcpoa_buffer(bsdp_options))
	    != dhcpoa_success_e) {
	    my_log(LOG_INFO, 
		   "NetBoot: add vendor specific failed, %s",
		   dhcpoa_err(options));
	    return (FALSE);
	}
    }
    return (TRUE);
}

boolean_t
bsdp_init(CFDictionaryRef plist)
{
    static boolean_t 	first = TRUE;

    G_disk_space_warned = FALSE;
    if (first == TRUE) {
	struct group *	group_ent_p;

	/* get the netboot group id, or create the group if necessary */
	group_ent_p = getgrnam(NETBOOT_GROUP);
	if (group_ent_p == NULL) {
#define NETBOOT_GID	120
	    if (S_create_netboot_group(NETBOOT_GID, &S_netboot_gid) == FALSE) {
		goto failed;
	    }
	}
	else {
	    S_netboot_gid = group_ent_p->gr_gid;
	}
	/* get the admin group id */
	group_ent_p = getgrnam(ADMIN_GROUP_NAME);
	if (group_ent_p == NULL) {
	    my_log(LOG_INFO, "bsdpd: getgrnam " ADMIN_GROUP_NAME " failed");
	    goto failed;
	}
	G_admin_gid = group_ent_p->gr_gid;
	first = FALSE;
    }
    if (plist != NULL) {
	plist = CFDictionaryGetValue(plist, BOOTPD_PLIST_NETBOOT);
    }
    S_read_config(plist);

    /* free the old information */
    NBSPList_free(&S_sharepoints);
    NBSPList_free(&G_client_sharepoints);
    NBImageList_free(&G_image_list);
    BSDPClients_free(&S_clients);
    AFPUserList_free(&S_afp_users);

    /* get the list of image sharepoints */
    S_sharepoints = NBSPList_init(NETBOOT_SHAREPOINT_LINK,
				  NBSP_READONLY_OK);
    if (S_sharepoints == NULL) {
	my_log(LOG_INFO, "bsdpd: no sharepoints defined");
	goto failed;
    }
    S_set_sharepoint_permissions(S_sharepoints, ROOT_UID, 
				 G_admin_gid);
    if (debug) {
	printf("NetBoot image sharepoints\n");
	NBSPList_print(S_sharepoints);
    }

    /* get the list of client sharepoints */
    G_client_sharepoints = NBSPList_init(NETBOOT_CLIENTS_SHAREPOINT_LINK,
					 NBSP_NO_READONLY);
    if (G_client_sharepoints == NULL) {
	my_log(LOG_INFO, "bsdpd: no client sharepoints defined");
    }
    else {
	S_set_sharepoint_permissions(G_client_sharepoints, ROOT_UID,
				     G_admin_gid);
	if (debug) {
	    printf("NetBoot client sharepoints\n");
	    NBSPList_print(G_client_sharepoints);
	}
    }

    /* get the list of netboot images */
    G_image_list = NBImageList_init(S_sharepoints,
				    G_client_sharepoints != NULL);
    if (G_image_list == NULL) {
	my_log(LOG_INFO, "bsdpd: no NetBoot images found");
	goto failed;
    }
    if (debug) {
	NBImageList_print(G_image_list);
    }
    S_set_image_permissions(G_image_list, ROOT_UID, G_admin_gid);
    if (BSDPClients_init(&S_clients) == FALSE) {
	my_log(LOG_INFO, "bsdpd: BSDPClients_init failed");
	goto failed;
    }
    if (AFPUserList_init(&S_afp_users) == FALSE) {
	my_log(LOG_INFO, "bsdpd: AFPUserList_init failed");
	goto failed;
    }
    AFPUserList_create(&S_afp_users, S_netboot_gid, 
		       S_afp_uid_start, S_afp_users_max);
    S_next_host_number = S_host_number_max() + 1;
    return (TRUE);

 failed:
    return (FALSE);
}

static AFPUserRef
S_reclaim_afp_user(struct timeval * time_in_p, boolean_t * modified)
{
    AFPUserRef		reclaimed_entry = NULL;
    PLCacheEntry_t *	scan;

    for (scan = S_clients.list.tail; scan; scan = scan->prev) {
	char *			afp_user;
	CFStringRef		afp_user_cf;
	char *			bound;
	int			host_number = 0;
	char *			last_boot;
	char *			name;
	char *			number;

	bound = ni_valforprop(&scan->pl, NIPROP_NETBOOT_BOUND);
	if (bound == NULL) {
	    /* already reclaimed */
	}
	afp_user = ni_valforprop(&scan->pl, NIPROP_NETBOOT_AFP_USER);
	if (afp_user == NULL) {
	    /* doesn't have an AFP user to reclaim */
	    continue;
	}
	name = ni_valforprop(&scan->pl, NIPROP_NAME);
	last_boot = ni_valforprop(&scan->pl, NIPROP_NETBOOT_LAST_BOOT_TIME);
	if (last_boot) {
	    long	t;

	    t = strtol(last_boot, NULL, 0);
	    if (t == LONG_MAX && errno == ERANGE) {
		continue;
	    }
	    if ((time_in_p->tv_sec - t) < S_age_time_seconds) {
		continue;
	    }
	}
	/* lookup the entry we're going to steal first */
	afp_user_cf = CFStringCreateWithCString(NULL, afp_user,
						kCFStringEncodingASCII);
	reclaimed_entry = AFPUserList_lookup(&S_afp_users, afp_user_cf);
	CFRelease(afp_user_cf);
	if (reclaimed_entry == NULL) {
	    /* netboot user has been removed, stale entry */
	    ni_delete_prop(&scan->pl, NIPROP_NETBOOT_BOUND, modified);
	    ni_delete_prop(&scan->pl, NIPROP_NETBOOT_AFP_USER, modified);
	    continue;
	}
	number = ni_valforprop(&scan->pl, NIPROP_NETBOOT_NUMBER);
	if (number != NULL) {
	    host_number = strtol(number, 0, 0);
	}
	/* unlink the shadow file: if not in use, will save disk space */
	if (name) {
	    macNC_unlink_shadow(host_number, name);
	    my_log(LOG_DEBUG, "NetBoot: reclaimed login %s from %s",
		   afp_user, name);
	}
	/* mark the client as no longer bound */
	ni_delete_prop(&scan->pl, NIPROP_NETBOOT_AFP_USER, modified);
	ni_delete_prop(&scan->pl, NIPROP_NETBOOT_BOUND, modified);
	break;
    }
    return (reclaimed_entry);
}

static AFPUserRef
S_next_afp_user()
{
    int		i;
    int		n;

    n = CFArrayGetCount(S_afp_users.list);
    for (i = 0; i < n; i++) {
	char		*afp_user;
	char		afp_user_buf[256];
	AFPUserRef	user;

	user = (AFPUserRef)CFArrayGetValueAtIndex(S_afp_users.list, i);
	afp_user = AFPUser_get_user(user, afp_user_buf, sizeof(afp_user_buf));
	if (PLCache_lookup_prop(&S_clients.list, NIPROP_NETBOOT_AFP_USER,
				afp_user, FALSE) == NULL) {
	    return (user);
	}
    }

    return (NULL);
}

static __inline__ struct in_addr
image_server_ip(NBImageEntryRef image_entry, struct in_addr server_ip)
{
    if (image_entry->load_balance_ip.s_addr != 0) {
	server_ip = image_entry->load_balance_ip;
    }
    return (server_ip);
}

static boolean_t
escape_password(const char * password, int password_length, 
		char * escaped_password, int escaped_password_length)
{
    boolean_t	escaped = FALSE;
    CFStringRef	pass_str;
    CFStringRef	str;

#define PUNCTUATION  CFSTR(CHARSET_SYMBOLS)
    pass_str = CFStringCreateWithCString(NULL, password, kCFStringEncodingUTF8);
    str = CFURLCreateStringByAddingPercentEscapes(NULL,
						  pass_str, 
						  NULL,
						  PUNCTUATION,
						  kCFStringEncodingUTF8);
    CFRelease(pass_str);
    if (CFStringGetCString(str,
			   escaped_password,
			   escaped_password_length,
			   kCFStringEncodingUTF8)) {
	escaped = TRUE;
    }
    else {
	my_log(LOG_NOTICE, "failed to URL escape password");
    }
    CFRelease(str);
    return (escaped);

}

static boolean_t
X_netboot(NBImageEntryRef image_entry, struct in_addr server_ip,
	  const char * hostname, int host_number, uid_t uid,
	  const char * afp_user, const char * password,
	  struct dhcp * reply, dhcpoa_t * options,
	  dhcpoa_t * bsdp_options)
{
    const char *	root_path = NULL;	
    char		tmp[256];

    if (image_entry->type == kNBImageTypeNFS) {
	if (image_entry->type_info.nfs.indirect == TRUE) {
	    /* pre-formatted */
	    root_path = image_entry->type_info.nfs.root_path;
	}
	else {
	    snprintf(tmp, sizeof(tmp), "nfs:%s:%s:%s/%s",
		     inet_ntoa(image_server_ip(image_entry, server_ip)),
		     image_entry->sharepoint->path, image_entry->dir_name,
		     image_entry->type_info.nfs.root_path);
	    root_path = tmp;
	}
    }
    else {
	if (image_entry->type_info.http.indirect == TRUE) {
	    /* pre-formatted */
	    root_path = image_entry->type_info.http.root_path;
	}
	else {
	    snprintf(tmp, sizeof(tmp), "http://%s/NetBoot/%s/%s/%s",
		     inet_ntoa(image_server_ip(image_entry, server_ip)),
		     image_entry->sharepoint->name,
		     image_entry->dir_name_esc,
		     image_entry->type_info.http.root_path_esc);
	    root_path = tmp;
	}
    }
    if (dhcpoa_add(options, 
		   dhcptag_root_path_e,
		   strlen(root_path),
		   root_path) != dhcpoa_success_e) {
	my_log(LOG_INFO, "NetBoot: add root_path failed, %s",
	       dhcpoa_err(options));
	return (FALSE);
    }
    if (dhcpoa_vendor_add(options, bsdp_options, bsdptag_machine_name_e,
			  strlen(hostname), (void *)hostname) 
	!= dhcpoa_success_e) {
	my_log(LOG_INFO, "NetBoot: add machine name failed, %s",
	       dhcpoa_err(bsdp_options));
	return (FALSE);
    }
    if (image_entry->diskless) {
	char 		escaped_password[3 * AFP_PASSWORD_LEN + 1];
	const char *	passwd;
	char 		shadow_mount_path[256];
	char 		shadow_path[256];
	NBSPEntry *	vol;

	/* allocate shadow for diskless client */
	vol = macNC_allocate_shadow(hostname, host_number, uid, G_admin_gid,
				    kNetBootShadowName);
	if (vol == NULL) {
	    return (FALSE);
	}
	if (escape_password(password, strlen(password),
			    escaped_password, sizeof(escaped_password))) {
	    passwd = escaped_password;
	}
	else {
	    passwd = password;
	}
	snprintf(shadow_mount_path, sizeof(shadow_mount_path),
		 "afp://%s:%s@%s/%s",
		 afp_user, passwd, inet_ntoa(server_ip), vol->name);
	if (dhcpoa_vendor_add(options, bsdp_options, 
			      bsdptag_shadow_mount_path_e,
			      strlen(shadow_mount_path), shadow_mount_path)
	    != dhcpoa_success_e) {
	    my_log(LOG_INFO, "NetBoot: add shadow_mount_path failed, %s",
		   dhcpoa_err(bsdp_options));
	    return (FALSE);
	}
	snprintf(shadow_path, sizeof(shadow_path), "%s/%s",
		 hostname, kNetBootShadowName);
	if (dhcpoa_vendor_add(options, bsdp_options, bsdptag_shadow_file_path_e,
			      strlen(shadow_path), shadow_path)
	    != dhcpoa_success_e) {
	    my_log(LOG_INFO, "NetBoot: add shadow_file_path failed, %s",
		   dhcpoa_err(bsdp_options));
	    return (FALSE);
	}
    }
    return (TRUE);
}

static boolean_t
S_add_bootfile(NBImageEntryRef entry, const char * arch, const char * hostname,
	       char * pkt_bootfile, int pkt_bootfile_size)
{
    char	tftp_path[PATH_MAX];

    if (strcmp(arch, ARCH_PPC) == 0 && entry->ppc_bootfile_no_subdir) {
	snprintf(tftp_path, sizeof(tftp_path),
		 NETBOOT_TFTP_DIRECTORY "/%s/%s/%s", 
		 entry->sharepoint->name, 
		 entry->dir_name, entry->bootfile);
    }
    else {
	snprintf(tftp_path, sizeof(tftp_path),
		 NETBOOT_TFTP_DIRECTORY "/%s/%s/%s/%s", 
		 entry->sharepoint->name, 
		 entry->dir_name, arch, entry->bootfile);
    }
    if (bootp_add_bootfile(NULL, hostname, tftp_path, pkt_bootfile,
			   pkt_bootfile_size) == FALSE) {
	my_log(LOG_INFO, "NetBoot: bootp_add_bootfile %s failed",
	       tftp_path);
	return (FALSE);
    }
    return (TRUE);
}

static boolean_t
S_client_update(struct in_addr * client_ip_p, const char * arch,
		PLCacheEntry_t * entry, struct dhcp * reply, 
		char * idstr, struct in_addr server_ip,
		NBImageEntryRef image_entry,
		dhcpoa_t * options, dhcpoa_t * bsdp_options,
		struct timeval * time_in_p)
{
    char *		afp_user = NULL;
    char		afp_user_buf[256];
    char *		hostname;
    int			host_number;
    bsdp_image_id_t    	image_id;
    boolean_t		modified = FALSE;
    char 		passwd[AFP_PASSWORD_LEN + 1];
    boolean_t		ret = TRUE;
    uid_t		uid = 0;
    AFPUserRef		user_entry = NULL;
    char *		val = NULL;

    image_id = image_entry->image_id;
    hostname = ni_valforprop(&entry->pl, NIPROP_NAME);
    val = ni_valforprop(&entry->pl, NIPROP_NETBOOT_NUMBER);
    if (hostname == NULL || val == NULL) {
	my_log(LOG_INFO, "NetBoot: %s missing " NIPROP_NAME 
	       " or " NIPROP_NETBOOT_NUMBER, idstr);
	return (FALSE);
    }
    host_number = strtol(val, NULL, 0);
    if (image_entry->diskless || image_entry->type == kNBImageTypeClassic) {
	afp_user = ni_valforprop(&entry->pl, NIPROP_NETBOOT_AFP_USER);
	if (afp_user != NULL) {
	    CFStringRef	name;

	    name = CFStringCreateWithCString(NULL, afp_user,
					     kCFStringEncodingASCII);
	    user_entry = AFPUserList_lookup(&S_afp_users, name);
	    CFRelease(name);
	    if (user_entry == NULL) {
		ni_delete_prop(&entry->pl, NIPROP_NETBOOT_AFP_USER, &modified);
	    }
	}
	if (user_entry == NULL) {
	    user_entry = S_next_afp_user();
	    if (user_entry == NULL) {
		user_entry = S_reclaim_afp_user(time_in_p, &modified);
	    }
	    if (user_entry == NULL) {
		my_log(LOG_INFO, 
		       "NetBoot: AFP login capacity of %d reached servicing %s",
		       S_afp_users_max, hostname);
		return (FALSE);
	    }
	    afp_user = AFPUser_get_user(user_entry, afp_user_buf,
					sizeof(afp_user_buf));
	    ni_set_prop(&entry->pl, NIPROP_NETBOOT_AFP_USER, afp_user,
			&modified);
	}

	uid = AFPUser_get_uid(user_entry);

	if (AFPUser_set_random_password(user_entry,
					passwd, sizeof(passwd)) == FALSE) {
	    my_log(LOG_INFO, "NetBoot: failed to set password for %s",
		   hostname);
	    return (FALSE);
	}
    }
    else {
	ni_delete_prop(&entry->pl, NIPROP_NETBOOT_AFP_USER, &modified);
    }
    switch (image_entry->type) {
    case kNBImageTypeClassic:
	if (macNC_allocate(image_entry, reply, hostname, 
			   server_ip, host_number,
			   options, uid, afp_user, passwd) == FALSE) {
	    return (FALSE);
	}
	break;
    case kNBImageTypeHTTP:
    case kNBImageTypeNFS:
	if (X_netboot(image_entry, server_ip, hostname, host_number, uid,
		      afp_user, passwd, reply, options, bsdp_options) 
	    == FALSE) {
	    return (FALSE);
	}
	break;
    case kNBImageTypeBootFileOnly:
	break;
    default:
	my_log(LOG_INFO, "NetBoot: invalid type %d", image_entry->type);
	return (FALSE);
	break;
    }
    if (S_add_bootfile(image_entry, arch, hostname, 
		       (char *)reply->dp_file, sizeof(reply->dp_file))
	== FALSE) {
	return (FALSE);
    }
    {
	char	buf[32];

	sprintf(buf, "0x%x", image_id);
	ni_set_prop(&entry->pl, NIPROP_NETBOOT_IMAGE_ID, buf, &modified);

	sprintf(buf, "0x%x", (unsigned)time_in_p->tv_sec);
	ni_set_prop(&entry->pl, NIPROP_NETBOOT_LAST_BOOT_TIME, buf, &modified);

    }
    if (client_ip_p != NULL) {
	ni_set_prop(&entry->pl, NIPROP_IPADDR, inet_ntoa(*client_ip_p),
		    &modified);
    }
    ni_set_prop(&entry->pl, NIPROP_NETBOOT_BOUND, "true", &modified);
    if (PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE) == FALSE) {
	my_log(LOG_INFO, 
	       "NetBoot: failed to save file " BSDP_CLIENTS_FILE ", %m");
    }
    return (ret);
}

static boolean_t
S_client_create(struct in_addr client_ip, 
		struct dhcp * reply, char * idstr, 
		const char * arch, const char * sysid, 
		struct in_addr server_ip,
		NBImageEntryRef image_entry,
		dhcpoa_t * options,
		dhcpoa_t * bsdp_options, struct timeval * time_in_p)
{
    char *		afp_user = NULL;
    char		afp_user_buf[256];
    char		hostname[256];
    bsdp_image_id_t 	image_id; 
    int			host_number;
    char 		passwd[AFP_PASSWORD_LEN + 1];
    ni_proplist		pl;
    AFPUserRef		user_entry;
    uid_t		uid = 0;

    image_id = image_entry->image_id;
    host_number = S_next_host_number;
    NI_INIT(&pl);

    sprintf(hostname, S_machine_name_format, host_number);
    ni_proplist_addprop(&pl, NIPROP_NAME, (ni_name)hostname);
    ni_proplist_addprop(&pl, NIPROP_IDENTIFIER, (ni_name)idstr);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_ARCH, (char *)arch);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_SYSID, (char *)sysid);
    {
	char buf[32];

	sprintf(buf, "0x%x", image_id);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_IMAGE_ID, (ni_name)buf);
	sprintf(buf, "%d", host_number);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_NUMBER, (ni_name)buf);
	sprintf(buf, "0x%x", (unsigned)time_in_p->tv_sec);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_LAST_BOOT_TIME, buf);
    }
    ni_proplist_addprop(&pl, NIPROP_IPADDR, inet_ntoa(client_ip));
    if (image_entry->diskless || image_entry->type == kNBImageTypeClassic) {
	user_entry = S_next_afp_user();
	if (user_entry == NULL) {
	    user_entry = S_reclaim_afp_user(time_in_p, NULL);
	    if (user_entry == NULL) {
		my_log(LOG_INFO, "NetBoot: AFP login capacity of %d reached",
		       S_afp_users_max);
		goto failed;
	    }
	}

	uid = AFPUser_get_uid(user_entry);
	afp_user = AFPUser_get_user(user_entry, afp_user_buf,
				    sizeof(afp_user_buf));
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_AFP_USER, afp_user);

	if (AFPUser_set_random_password(user_entry,
					passwd, sizeof(passwd)) == FALSE) {
	    my_log(LOG_INFO, "NetBoot: failed to set password for %s",
		   hostname);
	    goto failed;
	}
    }

    switch (image_entry->type) {
    case kNBImageTypeClassic:
	if (macNC_allocate(image_entry, reply, hostname, server_ip, 
			   host_number, options, uid, afp_user, passwd) 
	    == FALSE) {
	    goto failed;
	}
	break;
    case kNBImageTypeHTTP:
    case kNBImageTypeNFS:
	if (X_netboot(image_entry, server_ip, hostname, host_number, uid,
		      afp_user, passwd, reply, options, bsdp_options) 
	    == FALSE) {
	    goto failed;
	}
	break;
    case kNBImageTypeBootFileOnly:
	break;
    default:
	my_log(LOG_INFO, "NetBoot: invalid type %d", image_entry->type);
	goto failed;
	break;
    }
    if (S_add_bootfile(image_entry, arch, hostname, 
		       (char *)reply->dp_file, sizeof(reply->dp_file))
	== FALSE) {
	goto failed;
    }

    ni_set_prop(&pl, NIPROP_NETBOOT_BOUND, "true", NULL);

    PLCache_add(&S_clients.list, PLCacheEntry_create(pl));
    if (PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE) == FALSE) {
	my_log(LOG_INFO, 
	       "NetBoot: failed to save file " BSDP_CLIENTS_FILE ", %m");
    }
    ni_proplist_free(&pl);

    /* increment for the next host */
    S_next_host_number++;
    return (TRUE);

 failed:
    ni_proplist_free(&pl);
    return (FALSE);
}

static boolean_t
S_client_remove(PLCacheEntry_t * * entry)
{
    PLCacheEntry_t *	ent = *entry;
    int			host_number = 0;
    char *		name;
    char *		number;

    /* clean-up shadow file */
    name = ni_valforprop(&ent->pl, NIPROP_NAME);
    number = ni_valforprop(&ent->pl, NIPROP_NETBOOT_NUMBER);
    if (number != NULL) {
	host_number = strtol(number, 0, 0);
    }
    if (name) {
	macNC_unlink_shadow(host_number, name);
	my_log(LOG_DEBUG, "NetBoot: removed shadow file for %s", name);
    }
    PLCache_remove(&S_clients.list, ent);
    PLCacheEntry_free(ent);
    *entry = NULL;
    PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE);
    return (TRUE);
}

static struct dhcp * 
make_bsdp_reply(struct dhcp * reply, int pkt_size, 
		struct in_addr server_id, dhcp_msgtype_t msg, 
		struct dhcp * request, dhcpoa_t * options)
{
    struct dhcp * r;

    r = make_dhcp_reply(reply, pkt_size, server_id, msg, request, options);
    if (r == NULL) {
	return (NULL);
    }
    if (dhcpoa_add(options, 
		   dhcptag_vendor_class_identifier_e, 
		   strlen(BSDP_VENDOR_CLASS_ID),
		   BSDP_VENDOR_CLASS_ID) != dhcpoa_success_e) {
	my_log(LOG_INFO, "NetBoot: add class id failed, %s",
	       dhcpoa_err(options));
	return (NULL);
    }
    return (r);
}

static struct dhcp * 
make_bsdp_failed_reply(struct dhcp * reply, int pkt_size, 
		       struct in_addr server_id,
		       struct dhcp * request, dhcpoa_t * options_p)
{
    unsigned char		msgtype = bsdp_msgtype_failed_e;
    char			bsdp_buf[32];
    dhcpoa_t			bsdp_options;

    reply = make_bsdp_reply(reply, pkt_size, server_id, dhcp_msgtype_ack_e,
			    request, options_p);
    if (reply == NULL) {
	return (NULL);
    }
    dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
		   sizeof(msgtype), &msgtype) != dhcpoa_success_e) {
	my_log(LOG_INFO, 
	       "NetBoot: add BSDP end tag failed, %s",
	       dhcpoa_err(&bsdp_options));
	return (NULL);
    }

    /* add the BSDP options to the packet */
    if (dhcpoa_add(options_p, dhcptag_vendor_specific_e,
		   dhcpoa_used(&bsdp_options),
		   dhcpoa_buffer(&bsdp_options))
	!= dhcpoa_success_e) {
	my_log(LOG_INFO, "NetBoot: add vendor specific failed, %s",
	       dhcpoa_err(options_p));
	return (NULL);
    }
    if (dhcpoa_add(options_p, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	my_log(LOG_INFO, "NetBoot: add dhcp options end failed, %s",
	       dhcpoa_err(options_p));
	return (NULL);
    }
    return (reply);
}


static boolean_t
S_prop_u_int32(ni_proplist * pl_p, const char * prop, u_int32_t * retval)
{
    ni_name str = ni_valforprop(pl_p, (char *)prop);
    unsigned long val;

    if (str == NULL)
	return (FALSE);
    val = strtoul(str, 0, 0);
    if (val == ULONG_MAX && errno == ERANGE) {
	return (FALSE);
    }
    *retval = val;
    return (TRUE);
}

boolean_t
is_bsdp_packet(dhcpol_t * rq_options, char * arch, char * sysid,
	       dhcpol_t * rq_vsopt, bsdp_version_t * client_version,
	       boolean_t * is_old_netboot)
{
    void *		classid;
    int			classid_len;
    dhcpo_err_str_t	err;
    bsdp_version_t *	vers;

    *is_old_netboot = FALSE;
    dhcpol_init(rq_vsopt);

    classid = dhcpol_find(rq_options, dhcptag_vendor_class_identifier_e,
			  &classid_len, NULL);
    if (classid == NULL 
	|| bsdp_parse_class_id(classid, classid_len, arch, sysid) == FALSE) {
	goto failed;
    }

    /* parse the vendor-specific option area */
    if (dhcpol_parse_vendor(rq_vsopt, rq_options, &err) == FALSE) {
	if (verbose) {
	    my_log(LOG_INFO, 
		   "NetBoot: parse vendor specific options failed, %s", 
		   err.str);
	}
	goto failed;
    }

    /* check the client version */
    vers = (bsdp_version_t *)
	dhcpol_find(rq_vsopt, bsdptag_version_e, NULL, NULL);
    if (vers == NULL) {
	if (verbose) {
	    my_log(LOG_INFO, "NetBoot: BSDP version missing");
	}
	goto failed;
    }
    *client_version = ntohs(*vers);
    switch (*client_version) {
    case BSDP_VERSION_1_0:
    case BSDP_VERSION_1_1:
    case BSDP_VERSION_0_0:
	break;
    default:
	if (!quiet) {
	    my_log(LOG_INFO, "NetBoot: unsupported BSDP version %d",
		   *client_version);
	}
	goto failed;
	break;
    }
    if (client_version == BSDP_VERSION_0_0
	|| (dhcpol_find(rq_vsopt, bsdptag_netboot_1_0_firmware_e, NULL, NULL)
	    != NULL)) {
	*is_old_netboot = TRUE;
    }
    return (TRUE);

 failed:
    dhcpol_free(rq_vsopt);
    return (FALSE);
}

static const u_int16_t *
attributes_filter_list_copy(dhcpol_t * vsopt, 
			    u_int16_t * scratch, int scratch_elements,
			    int * ret_n_attrs)
{
    int			i;
    int			len = 0;
    const u_int16_t * 	option_data;
    u_int16_t *		ret_attrs = NULL;

    *ret_n_attrs = 0;
    option_data = (const u_int16_t *)
	dhcpol_find(vsopt, bsdptag_image_attributes_filter_list_e,
		    &len, NULL);
    if (option_data == NULL) {
	goto done;
    }

    /* two bytes per filter attribute */
    len >>= 1;
    if (len == 0) {
	goto done;
    }
    if (scratch == NULL || len > scratch_elements) {
	ret_attrs = (u_int16_t *)malloc(sizeof(*ret_attrs) * len);
    }
    else {
	ret_attrs = scratch;
    }
    for (i = 0; i < len; i++) {
	ret_attrs[i] = ntohs(option_data[i]);
    }
    *ret_n_attrs = len;
 done:
    return (ret_attrs);
}

void
bsdp_dhcp_request(request_t * request, dhcp_msgtype_t dhcpmsg)
{
    PLCacheEntry_t *	entry;
    char *		idstr;
    boolean_t		modified = FALSE;
    int		 	optlen;
    struct in_addr * 	req_ip;
    struct dhcp *	rq = request->pkt;
    char		scratch_idstr[3 * sizeof(rq->dp_chaddr)];

    if (dhcpmsg != dhcp_msgtype_request_e
	|| rq->dp_htype != ARPHRD_ETHER 
	|| rq->dp_hlen != ETHER_ADDR_LEN) {
	return;
    }
    req_ip = (struct in_addr *)
	dhcpol_find(request->options_p, 
		    dhcptag_requested_ip_address_e,
		    &optlen, NULL);
    if (req_ip == NULL || optlen != 4) {
	return;
    }
    idstr = identifierToStringWithBuffer(rq->dp_htype, rq->dp_chaddr, 
					 rq->dp_hlen, scratch_idstr,
					 sizeof(scratch_idstr));
    if (idstr == NULL) {
	return;
    }
    entry = PLCache_lookup_identifier(&S_clients.list, idstr,
				      NULL, NULL, NULL, NULL);
    if (entry == NULL
	|| ni_valforprop(&entry->pl, NIPROP_NETBOOT_BOUND) == NULL) {
	goto done;
    }

    /* update our notion of the client's IP address */
    ni_set_prop(&entry->pl, NIPROP_IPADDR, 
		inet_ntoa(*req_ip),
		&modified);
    if (modified) {
	(void)PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE);
    }

 done:
    if (idstr != scratch_idstr) {
	free(idstr);
    }
    return;
}

int
bsdp_max_message_size(dhcpol_t * bsdp_options, dhcpol_t * dhcp_options) 
{
    u_char * 	opt;
    int 	opt_len;
    int		val = DHCP_PACKET_MIN;

    /* first look for the max message size option in the BSDP options */
    opt = dhcpol_find(bsdp_options, bsdptag_max_message_size_e,
		      &opt_len, NULL);
    if (opt == NULL || opt_len != 2) {
	/* if not there, look in the DHCP options */
	opt = dhcpol_find(dhcp_options, dhcptag_max_dhcp_message_size_e,
			  &opt_len, NULL);
    }
    if (opt != NULL && opt_len == 2) {
	u_int16_t 	sval;

	sval = ntohs(*((u_int16_t *)opt));
	if (sval > DHCP_PACKET_MIN) {
	    val = sval;
	}
    }
    return (val);
}

#define N_SCRATCH_ATTRS		4

void
bsdp_request(request_t * request, dhcp_msgtype_t dhcpmsg,
	     const char * arch, const char * sysid, dhcpol_t * rq_vsopt,
	     bsdp_version_t client_version, boolean_t is_old_netboot)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    PLCacheEntry_t *	entry;
    char *		idstr = NULL;
    const u_int16_t *	filter_attrs = NULL;
    int			max_packet;
    dhcpoa_t		options;
    int			n_filter_attrs = 0;
    u_int16_t		reply_port = IPPORT_BOOTPC;
    struct dhcp *	reply = NULL;
    struct dhcp *	rq = request->pkt;
    u_int16_t		scratch_attrs[N_SCRATCH_ATTRS];
    char		scratch_idstr[3 * sizeof(rq->dp_chaddr)];
    uint32_t		txbuf[8 * 1024 / sizeof(uint32_t)];

    if (rq->dp_htype != ARPHRD_ETHER || rq->dp_hlen != ETHER_ADDR_LEN) {
	return;
    }
    switch (dhcpmsg) {
    case dhcp_msgtype_discover_e:
	/* we send using bpf which must fit into a single packet */
	max_packet = dhcp_max_message_size(request->options_p);
	if (max_packet > ETHERMTU) {
	    max_packet = ETHERMTU;
	}
	break;
    case dhcp_msgtype_inform_e:
	/* we send using a socket so can send larger packets */
	max_packet = bsdp_max_message_size(rq_vsopt, 
					   request->options_p);
	if (max_packet > sizeof(txbuf)) {
	    max_packet = sizeof(txbuf);
	}
	break;
    case dhcp_msgtype_request_e:
	bsdp_dhcp_request(request, dhcpmsg);
	return;
    default:
	return;
    }

    /* maximum message size includes IP/UDP header, subtract that */
    max_packet -= DHCP_PACKET_OVERHEAD;

    idstr = identifierToStringWithBuffer(rq->dp_htype, rq->dp_chaddr, 
					 rq->dp_hlen, scratch_idstr,
					 sizeof(scratch_idstr));
    if (idstr == NULL) {
	return;
    }
    entry = PLCache_lookup_identifier(&S_clients.list, idstr,
				      NULL, NULL, NULL, NULL);
    if (!quiet) {
	char *		name = NULL;

	if (entry) {
	    name = ni_valforprop(&entry->pl, NIPROP_NAME);
	}
	my_log(LOG_INFO, "BSDP %s [%s] %s %s%sarch=%s sysid=%s", 
	       dhcp_msgtype_names(dhcpmsg), 
	       if_name(request->if_p), idstr, 
	       name ? name : "",
	       name ? " " : "",
	       arch, sysid);
	if (debug && verbose) {
	    bsdp_print_packet(request->pkt, request->pkt_length, 1);
	}
    }
    filter_attrs = attributes_filter_list_copy(rq_vsopt, 
					       scratch_attrs, 
					       N_SCRATCH_ATTRS, 
					       &n_filter_attrs);
    switch (dhcpmsg) {
      case dhcp_msgtype_discover_e: { /* DISCOVER */
	  char *		bound = NULL;
	  u_int32_t		image_id;
	  NBImageEntryRef	image_entry;

	  if (strchr(testing_control, 'd')) {
	      printf("NetBoot: Ignoring DISCOVER\n");
	      goto no_reply;
	  }

	  /* have an entry, but not bound */
	  if (entry) {
	      bound = ni_valforprop(&entry->pl, NIPROP_NETBOOT_BOUND);
	  }

	  /* no entry or not bound */
	  if (bound == NULL) {
	      goto no_reply;
	  }
	  if (S_prop_u_int32(&entry->pl, NIPROP_NETBOOT_IMAGE_ID, &image_id) 
	      == FALSE) {
	      my_log(LOG_INFO, "NetBoot: [%s] image id invalid", idstr);
	      goto no_reply;
	  }
	  /*
	   * If the image is no longer present, or the client's system
	   * identifier is not supported by the image, or the image attributes
	   * don't match those requested by the client, ignore the request.
	   */
	  image_entry = NBImageList_elementWithID(G_image_list, image_id);
	  if (image_entry == NULL
	      || !NBImageEntry_supported_sysid(image_entry, arch, sysid,
					       (const struct ether_addr *)
					       rq->dp_chaddr)
	      || !NBImageEntry_attributes_match(image_entry, filter_attrs,
						n_filter_attrs)) {
	      goto no_reply;
	  }
	  /* reply with a BSDP OFFER packet */
	  reply = make_bsdp_reply((struct dhcp *)txbuf, max_packet,
				  if_inet_addr(request->if_p), 
				  dhcp_msgtype_offer_e,
				  rq, &options);
	  if (reply == NULL) {
	      goto no_reply;
	  }

	  /* client has no IP address yet */
	  reply->dp_ciaddr.s_addr = 0;
	  reply->dp_yiaddr.s_addr = 0;

	  /* set the selected image id property */
	  dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
	  image_id = htonl(image_id); /* put into network order */
	  if (dhcpoa_add(&bsdp_options, 
			 bsdptag_selected_boot_image_e,
			 sizeof(image_id), &image_id)
	      != dhcpoa_success_e) {
	      my_log(LOG_INFO, 
		     "NetBoot: [%s] add selected image id failed, %s", idstr,
		     dhcpoa_err(&bsdp_options));
	      goto no_reply;
	  }

	  if (S_client_update(NULL, arch,
			      entry, reply, idstr, if_inet_addr(request->if_p),
			      image_entry, &options, &bsdp_options,
			      request->time_in_p) == FALSE) {
	      goto no_reply;
	  }
	  /* add the BSDP options to the packet */
	  if (dhcpoa_used(&bsdp_options) > 0) {
	      if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
			     dhcpoa_used(&bsdp_options), 
			     dhcpoa_buffer(&bsdp_options))
		  != dhcpoa_success_e) {
		  my_log(LOG_INFO, 
			 "NetBoot: [%s] add vendor specific failed, %s",
			 idstr, dhcpoa_err(&options));
		  goto no_reply;
	      }
	  }
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	      != dhcpoa_success_e) {
	      my_log(LOG_INFO, "NetBoot: [%s] add dhcp options end failed, %s",
		     idstr, dhcpoa_err(&options));
	      goto no_reply;
	  }
	  { /* send a reply */
	      int 		size;
	      
	      if (image_entry->load_balance_ip.s_addr != 0) {
		  reply->dp_siaddr = image_entry->load_balance_ip;
	      }
	      else {
		  reply->dp_siaddr = if_inet_addr(request->if_p);
		  strcpy((char *)reply->dp_sname, server_name);
	      }

	      size = sizeof(struct dhcp) + sizeof(rfc_magic) +
		  dhcpoa_used(&options);
	      if (size < sizeof(struct bootp)) {
		  /* pad out to BOOTP-sized packet */
		  size = sizeof(struct bootp);
	      }
	      if (sendreply(request->if_p, (struct bootp *)reply, size, 
			    FALSE, &reply->dp_yiaddr)) {
		  if (!quiet) {
		      my_log(LOG_INFO, "BSDP OFFER sent [%s] pktsize %d",
			     idstr, size);
		  }
		  if (debug && verbose) {
		      bsdp_print_packet(reply, size, 1);
		  }
	      }
	  }
	  break;
      } /* DISCOVER */
    
      case dhcp_msgtype_inform_e: { /* INFORM */
	  NBImageEntryRef	image_entry;
	  const char *		filter_sysid;
	  unsigned char		msgtype;
	  u_int16_t *		port;
	  void *		ptr;

	  port = (u_int16_t *)dhcpol_find(rq_vsopt, bsdptag_reply_port_e, 
					  NULL, NULL);
	  if (port) { /* client wants reply on alternate port */
	      reply_port = ntohs(*port);
	      if (reply_port >= IPPORT_RESERVED)
		  goto no_reply; /* client must be on privileged port */
	  }

	  if (rq->dp_ciaddr.s_addr == 0) {
	      if (!quiet) {
		  my_log(LOG_INFO, "NetBoot: [%s] INFORM with no IP address",
			 idstr);
	      }
	      goto no_reply;
	  }
	  ptr = dhcpol_find(rq_vsopt, bsdptag_message_type_e, NULL, NULL);
	  if (ptr == NULL) {
	      if (!quiet) {
		  my_log(LOG_INFO, "NetBoot: [%s] BSDP message type missing",
			 idstr);
	      }
	      goto no_reply;
	  }
	  msgtype = *(unsigned char *)ptr;

	  /* ready the vendor-specific option area to hold bsdp options */
	  dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));

	  if (is_old_netboot) {
	      filter_sysid = OLD_NETBOOT_SYSID;
	  }
	  else {
	      filter_sysid = sysid;
	  }
	  switch (msgtype) {
	    case bsdp_msgtype_list_e: {
		int		current_count;
		bsdp_priority_t	priority;
		NBImageEntryRef	default_image = NULL;
		u_int32_t	default_image_id;
		u_int32_t	image_id;

		default_image = NBImageList_default(G_image_list,
						    arch,
						    filter_sysid,
						    (const struct ether_addr *)
						    rq->dp_chaddr,
						    filter_attrs,
						    n_filter_attrs);
		if (default_image == NULL) {
		    /* no applicable images */
		    goto no_reply;
		}
		default_image_id = htonl(default_image->image_id);
		/* reply with an ACK[LIST] packet */
		reply = make_bsdp_reply((struct dhcp *)txbuf, max_packet,
					if_inet_addr(request->if_p), 
					dhcp_msgtype_ack_e,
					rq, &options);
		if (reply == NULL) {
		    goto no_reply;
		}
		/* formulate the BSDP options */
		if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
			       sizeof(msgtype), &msgtype) 
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: [%s] add message type failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		current_count = PLCache_count(&S_clients.list);
		if (current_count > server_priority) {
		    priority = 0;
		}
		else {
		    priority = htons(server_priority - current_count);
				     
		}
		if (dhcpoa_add(&bsdp_options, bsdptag_server_priority_e,
			       sizeof(priority), &priority) 
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, "NetBoot: [%s] add priority failed, %s", 
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (dhcpoa_add(&bsdp_options, bsdptag_default_boot_image_e,
			       sizeof(default_image_id), &default_image_id)
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: [%s] add default image id failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (entry) {
		    char * 		bound;

		    /*
		     * If we don't have a binding for the client,
		     * or the image no longer exists, or the
		     * client's system identifier is not support by the image,
		     * or the image attributes don't match those requested
		     * by the client, don't supply the selected image.
		     */
		    bound = ni_valforprop(&entry->pl, NIPROP_NETBOOT_BOUND);
		    if ((bound == NULL)
			|| (strchr(testing_control, 'e') != NULL)
			|| !S_prop_u_int32(&entry->pl,
					   NIPROP_NETBOOT_IMAGE_ID, 
					   &image_id)
			|| ((image_entry 
			     = NBImageList_elementWithID(G_image_list, 
							 image_id)) == NULL)
			|| !NBImageEntry_supported_sysid(image_entry, arch,
							 filter_sysid,
							 (const struct ether_addr *)
							 rq->dp_chaddr)
			|| !NBImageEntry_attributes_match(image_entry, 
							  filter_attrs,
							  n_filter_attrs)) {
			/* don't supply the selected image */
		    }
		    else {
			image_id = htonl(image_id); /* put into network order */
			if (dhcpoa_add(&bsdp_options, 
				       bsdptag_selected_boot_image_e,
				       sizeof(image_id), &image_id)
			    != dhcpoa_success_e) {
			    my_log(LOG_INFO, 
				   "NetBoot: [%s] add selected"
				   " image id failed, %s",
				   idstr, dhcpoa_err(&bsdp_options));
			    goto no_reply;
			}
		    }
		}
		if (client_version == BSDP_VERSION_1_0) {
		    /* add the BSDP options to the packet */
		    if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
				   dhcpoa_used(&bsdp_options),
				   dhcpoa_buffer(&bsdp_options))
			!= dhcpoa_success_e) {
			my_log(LOG_INFO, 
			       "NetBoot: add vendor specific failed, %s",
			       dhcpoa_err(&options));
			goto no_reply;
		    }
		}
		else {
		    if (!S_insert_image_list(arch, filter_sysid, 
					     (const struct ether_addr *)
					     rq->dp_chaddr,
					     filter_attrs, n_filter_attrs,
					     &options, &bsdp_options)) {
			goto no_reply;
		    }
		}
		if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: [%s] add dhcp options end failed, %s",
			   idstr, dhcpoa_err(&options));
		    goto no_reply;
		}
		break;
	    }
	    case bsdp_msgtype_select_e: {
		NBImageEntryRef image_entry;
		bsdp_image_id_t	image_id;
		u_int32_t *	selected_image_id;
		struct in_addr *server_id;

		server_id = (struct in_addr *)
		    dhcpol_find(rq_vsopt, bsdptag_server_identifier_e, 
				NULL, NULL);
		if (server_id == NULL) {
		    if (!quiet) {
			my_log(LOG_INFO, 
			       "NetBoot: [%s] INFORM[SELECT] missing server id",
			       idstr);
		    }
		    goto no_reply;
		}
		if (server_id->s_addr != if_inet_addr(request->if_p).s_addr) {
		    if (debug)
			printf("client selected %s\n", inet_ntoa(*server_id));
		    if (entry) {
			/* we have a binding, delete it */
			(void)S_client_remove(&entry);
		    }
		    goto no_reply;
		}
		selected_image_id = (u_int32_t *)
		    dhcpol_find(rq_vsopt, bsdptag_selected_boot_image_e,
				NULL, NULL);
		if (selected_image_id) {
		    image_id = ntohl(*selected_image_id);
		    image_entry = NBImageList_elementWithID(G_image_list, 
							    image_id);
		    if (image_entry == NULL
			|| !NBImageEntry_supported_sysid(image_entry, arch,
							 filter_sysid,
							 (const struct ether_addr *)
							 rq->dp_chaddr)) {
			/* stale image ID */
			goto send_failed;
		    }
		}
		else {
		    image_entry = NBImageList_default(G_image_list, 
						      arch,
						      filter_sysid,
						      (const struct ether_addr *)
						      rq->dp_chaddr,
						      NULL, 0);
		    image_id = image_entry->image_id;
		    if (image_entry == NULL) {
			/* no longer a default image */
			goto send_failed;
		    }
		}
		
		/* reply with an ACK[SELECT] packet */
		reply = make_bsdp_reply((struct dhcp *)txbuf, max_packet,
					if_inet_addr(request->if_p), 
					dhcp_msgtype_ack_e,
					rq, &options);
		if (reply == NULL) {
		    goto no_reply;
		}
		
		/* formulate the BSDP options */
		if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
			       sizeof(msgtype), &msgtype) 
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: [%s] add message type failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		image_id = htonl(image_id);
		if (dhcpoa_add(&bsdp_options, bsdptag_selected_boot_image_e,
			       sizeof(image_id), &image_id)
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: [%s] add selected image id failed, %s",
			   idstr, dhcpoa_err(&bsdp_options));
		    goto no_reply;
		}
		if (entry != NULL) {
		    if (S_client_update(&rq->dp_ciaddr, arch,
					entry, reply, idstr, 
					if_inet_addr(request->if_p),
					image_entry, &options, &bsdp_options, 
					request->time_in_p) == FALSE) {
			goto send_failed;
		    }
		}
		else {
		    if (S_client_create(rq->dp_ciaddr,
					reply, idstr, arch, sysid, 
					if_inet_addr(request->if_p),
					image_entry, &options, &bsdp_options,
					request->time_in_p) == FALSE) {
			goto send_failed;
		    }
		}
		/* add the BSDP options to the packet */
		if (dhcpoa_used(&bsdp_options) > 0) {
		    if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
				   dhcpoa_used(&bsdp_options), 
				   dhcpoa_buffer(&bsdp_options))
			!= dhcpoa_success_e) {
			my_log(LOG_INFO, 
			       "NetBoot: [%s] add vendor specific failed, %s",
			       idstr, dhcpoa_err(&options));
			goto no_reply;
		    }
		}
		if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, 
			   "NetBoot: [%s] add dhcp options end failed, %s",
			   idstr, dhcpoa_err(&options));
		    goto no_reply;
		}
		if (image_entry->load_balance_ip.s_addr != 0) {
		    reply->dp_siaddr = image_entry->load_balance_ip;
		}
		else {
		    reply->dp_siaddr = if_inet_addr(request->if_p);
		    strcpy((char *)reply->dp_sname, server_name);
		}
		break;
	    }
	    default: {
		/* invalid request */
		if (!quiet) {
		    my_log(LOG_INFO, "NetBoot: [%s] invalid BSDP message %d",
			   idstr, msgtype);
		}
		goto no_reply;
		break;
	    }
	  }
	  goto send_reply;

      send_failed:
	  reply = make_bsdp_failed_reply((struct dhcp *)txbuf,
					 max_packet,
					 if_inet_addr(request->if_p),
					 rq, &options);
	  if (reply) {
	      /* send an ACK[FAILED] */
	      msgtype = bsdp_msgtype_failed_e;
	      goto send_reply;
	  }
	  goto no_reply;

      send_reply:
	  { /* send a reply */
	      int size;

	      size = sizeof(struct dhcp) + sizeof(rfc_magic) +
		  dhcpoa_used(&options);
	      if (size < sizeof(struct bootp)) {
		  /* pad out to BOOTP-sized packet */
		  size = sizeof(struct bootp);
	      }
	      if (bootp_transmit(bootp_socket, transmit_buffer, 
				 if_name(request->if_p),
				 rq->dp_htype, NULL, 0, 
				 rq->dp_ciaddr, 
				 if_inet_addr(request->if_p),
				 reply_port, IPPORT_BOOTPS,
				 reply, size) < 0) {
		  my_log(LOG_INFO, "send failed, %m");
	      }
	      else {
		  if (debug && verbose) {
		      printf("\n=================== Server Reply ===="
			     "=================\n");
		      bsdp_print_packet(reply, size, 0);
		  }
		  if (!quiet) {
		      my_log(LOG_INFO, "NetBoot: [%s] BSDP ACK[%s] sent %s "
			     "pktsize %d", idstr, 
			     bsdp_msgtype_names(msgtype),
			     inet_ntoa(rq->dp_ciaddr), size);
		  }
	      }
	  }
	  break;
      } /* INFORM */
      default: {
	  /* invalid request */
	  if (!quiet) {
	      my_log(LOG_INFO, "NetBoot: [%s] DHCP message %s not supported",
		     idstr, dhcp_msgtype_names(dhcpmsg));
	  }
	  goto no_reply;
	  break;
      }
    }

 no_reply:
    if (idstr != scratch_idstr) {
	free(idstr);
    }
    if (filter_attrs != NULL && filter_attrs != scratch_attrs) {
	free((void *)filter_attrs);
    }
    return;
}


struct dhcp * 
make_bsdp_bootp_reply(struct dhcp * reply, int pkt_size, 
		      struct dhcp * request, dhcpoa_t * options)
{
    *reply = *request;
    reply->dp_hops = 0;
    reply->dp_secs = 0;
    reply->dp_op = BOOTREPLY;
    bcopy(rfc_magic, reply->dp_options, sizeof(rfc_magic));
    dhcpoa_init(options, reply->dp_options + sizeof(rfc_magic),
		pkt_size - sizeof(struct dhcp) - sizeof(rfc_magic));
    return (reply);
}

boolean_t
old_netboot_request(request_t * request)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    PLCacheEntry_t *	bsdp_entry = NULL;
    NBImageEntryRef	default_image = NULL;
    NBImageEntryRef	image_entry;
    struct in_addr	iaddr = {0};
    char *		idstr = NULL;
    dhcpoa_t		options;
    struct dhcp *	rq = request->pkt;
    struct dhcp *	reply = NULL;
    char		scratch_idstr[32];
    SubnetRef		subnet;
    char		txbuf[DHCP_PACKET_MIN];
    u_int32_t		version = MACNC_SERVER_VERSION;

    if (macNC_get_client_info(rq, request->pkt_length,
			      request->options_p, NULL) == FALSE) {
	return (FALSE);
    }
    default_image = NBImageList_default(G_image_list, 
					ARCH_PPC,
					OLD_NETBOOT_SYSID,
					(const struct ether_addr *)rq->dp_chaddr,
					NULL, 0);
    if (default_image == NULL) {
	/* no NetBoot 1.0 images */
	goto no_reply;
    }
    idstr = identifierToStringWithBuffer(rq->dp_htype, rq->dp_chaddr, 
					 rq->dp_hlen, scratch_idstr,
					 sizeof(scratch_idstr));
    if (idstr == NULL) {
	goto no_reply;
    }
    if (dhcp_bootp_allocate(idstr, idstr, rq, request->if_p, 
			    request->time_in_p, &iaddr, &subnet) == FALSE) {
	/* no client binding available */
	goto no_reply;
    }
    bsdp_entry = PLCache_lookup_identifier(&S_clients.list, idstr,
					   NULL, NULL, NULL, NULL);
    if (!quiet) {
	char *		name = NULL;

	if (bsdp_entry) {
	    name = ni_valforprop(&bsdp_entry->pl, NIPROP_NAME);
	}

	my_log(LOG_INFO, "NetBoot[BOOTP]: [%s] %s %s",
	       if_name(request->if_p), idstr, name ? name : "");
    }
    reply = make_bsdp_bootp_reply((struct dhcp *)txbuf, sizeof(txbuf),
				  rq, &options);
    if (reply == NULL)
	goto no_reply;

    reply->dp_yiaddr = iaddr;

    /* add the client-specified parameters */
    (void)add_subnet_options(NULL, iaddr, 
			     request->if_p, &options, NULL, 0);

    /* ready the vendor-specific option area to hold bsdp options */
    dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    
    if (bsdp_entry) {
	bsdp_image_id_t	image_id;

	if ((S_prop_u_int32(&bsdp_entry->pl, NIPROP_NETBOOT_IMAGE_ID, 
			    &image_id) == FALSE)
	    || ((image_entry 
		 = NBImageList_elementWithID(G_image_list, image_id)) == NULL)
	    || (NBImageEntry_supported_sysid(image_entry, ARCH_PPC,
					     OLD_NETBOOT_SYSID, 
					     (const struct ether_addr *)
					     rq->dp_chaddr)
		== FALSE)) {
	    /* stale image id, use default */
	    image_entry = default_image;
	}
	if (S_client_update(&iaddr, ARCH_PPC, bsdp_entry, reply, idstr, 
			    if_inet_addr(request->if_p),
			    image_entry, &options, &bsdp_options,
			    request->time_in_p) == FALSE) {
	    goto no_reply;
	}
    }
    else {
	image_entry = default_image;
	if (S_client_create(iaddr, reply, idstr, ARCH_PPC, "unknown", 
			    if_inet_addr(request->if_p), 
			    image_entry,
			    &options, &bsdp_options, request->time_in_p) 
	    == FALSE) {
	    goto no_reply;
	}
    }
    if (dhcpoa_add(&options, macNCtag_server_version_e, 
		   sizeof(version), &version) != dhcpoa_success_e) {
	goto no_reply;
    }
    if (dhcpoa_used(&bsdp_options) > 0) {
	/* add the BSDP options to the packet */
	if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
		       dhcpoa_used(&bsdp_options), 
		       dhcpoa_buffer(&bsdp_options))
	    != dhcpoa_success_e) {
	    my_log(LOG_INFO, 
		   "NetBoot[BOOTP]: [%s] add vendor specific failed, %s",
		   idstr, dhcpoa_err(&options));
	    goto no_reply;
	}
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	my_log(LOG_INFO, 
	       "NetBoot[BOOTP]: [%s] add dhcp options end failed, %s",
	       idstr, dhcpoa_err(&options));
	goto no_reply;
    }
    { /* send a reply */
	int size;
	
	if (image_entry->load_balance_ip.s_addr != 0) {
	    reply->dp_siaddr = image_entry->load_balance_ip;
	}
	else {
	    reply->dp_siaddr = if_inet_addr(request->if_p);
	    strcpy((char *)reply->dp_sname, server_name);
	}
	size = sizeof(struct dhcp) + sizeof(rfc_magic) +
	    dhcpoa_used(&options);
	if (size < sizeof(struct bootp)) {
	    /* pad out to BOOTP-sized packet */
	    size = sizeof(struct bootp);
	}
	if (sendreply(request->if_p, (struct bootp *)reply, size, 
		      FALSE, &iaddr)) {
	    if (!quiet) {
		my_log(LOG_INFO, "NetBoot[BOOTP]: reply sent %s pktsize %d",
		       inet_ntoa(iaddr), size);
	    }
	    if (debug && verbose) {
		bsdp_print_packet(reply, size, 1);
	    }
	}
    }

 no_reply:
    if (idstr != NULL && idstr != scratch_idstr) {
	free(idstr);
    }
    return (TRUE);
}

#ifdef TEST_BSDPD

#include "AFPUsers.c"
#include "bootpdfile.c"
#define main bootpd_main
#include "bootpd.c"
#undef main
#include "dhcpd.c"
#include "macNC.c"

int 
main(int argc, char * argv[])
{
    struct group *	group_ent_p;

    group_ent_p = getgrnam(NETBOOT_GROUP);
    if (group_ent_p == NULL) {
#define NETBOOT_GID	120
	if (S_create_netboot_group(NETBOOT_GID, &S_netboot_gid) == FALSE) {
	    printf("Could not create group '%s'\n", NETBOOT_GROUP);
	    exit(1);
	}
    }

    exit(0);
    return (0);
}

#endif /* TEST_BSDPD */
