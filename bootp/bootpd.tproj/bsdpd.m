/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * bsdpd.m
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

#import "subnetDescr.h"

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
#include "NIDomain.h"
#include "bootpd.h"
#include "macNC.h"
#include "macnc_options.h"
#include "nbsp.h"
#include "nbimages.h"
#include "NICache.h"
#include "NICachePrivate.h"
#include "AFPUsers.h"
#include "NetBootServer.h"

#define CFGPROP_SHADOW_SIZE_MEG		"shadow_size_meg"
#define CFGPROP_AFP_USERS_MAX		"afp_users_max"
#define CFGPROP_AGE_TIME_SECONDS	"age_time_seconds"
#define CFGPROP_AFP_UID_START		"afp_uid_start"
#define CFGPROP_MACHINE_NAME_FORMAT	"machine_name_format"

#define DEFAULT_MACHINE_NAME_FORMAT	"NetBoot%03d"

#define AGE_TIME_SECONDS	(60 * 60 * 24)
#define AFP_USERS_MAX		50
#define ADMIN_GROUP_NAME	"admin"

typedef struct {
    PLCache_t		list;
} BSDPClients_t;

/* global variables */
gid_t			G_admin_gid = 0;
boolean_t		G_disk_space_warned = FALSE;
u_long			G_shadow_size_meg = SHADOW_SIZE_DEFAULT;
NBSPListRef		G_client_sharepoints = NULL;
NBImageListRef		G_image_list = NULL;

/* local variables */
u_int32_t		S_age_time_seconds = AGE_TIME_SECONDS;
static gid_t		S_netboot_gid;
static BSDPClients_t	S_clients;
static AFPUsers_t	S_afp_users;
static int		S_afp_users_max = AFP_USERS_MAX;
static NBSPListRef	S_sharepoints = NULL;
static NBImageEntryRef	S_default_old_netboot_image = NULL;
static boolean_t	S_no_default_old_netboot_image_warned = FALSE;
static char *		S_machine_name_format = DEFAULT_MACHINE_NAME_FORMAT;

#define AFP_UID_START	100
static int		S_afp_uid_start = AFP_UID_START;
static int		S_next_host_number = 0;
static PropList_t 	S_config_netboot;

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
	val = strtol(number_nl_p->ninl_val[0], NULL, NULL);
	if (val > max_number) {
	    max_number = val;
	}
    }
    return (max_number);
}

static boolean_t
S_gid_taken(ni_entrylist * id_list, gid_t gid)
{
    int 		i;

    for (i = 0; i < id_list->niel_len; i++) {
	ni_namelist * 	nl_p = id_list->niel_val[i].names;
	gid_t		group_id;

	if (nl_p == NULL || nl_p->ninl_len == 0)
	    continue;

	group_id = strtoul(nl_p->ninl_val[0], NULL, NULL);
	if (group_id == gid)
	    return (TRUE);
    }
    return (FALSE);
}

static boolean_t
S_create_netboot_group(gid_t preferred_gid, gid_t * actual_gid)
{
    ni_id		dir;
    ni_entrylist	id_list;
    ni_proplist		pl;
    boolean_t		ret = FALSE;
    ni_status		status;
    gid_t		scan;

    *actual_gid = NULL;
    NI_INIT(&id_list);
    NI_INIT(&pl);

    status = ni_pathsearch(NIDomain_handle(ni_local), &dir,
			   NIDIR_GROUPS);
    if (status != NI_OK) {
	my_log(LOG_INFO, "bsdpd: ni_pathsearch '%s' failed, %s",
	       NIDIR_GROUPS, ni_error(status));
	return (FALSE);
    }
    status = ni_list(NIDomain_handle(ni_local), &dir,
		     NIPROP_GID, &id_list);
    if (status != NI_OK) {
	my_log(LOG_INFO, "bsdpd: ni_list '%s' failed, %s",
	       NIDIR_GROUPS, ni_error(status));
	return (FALSE);
    }
    
    ni_set_prop(&pl, NIPROP_NAME, NETBOOT_GROUP, NULL);
    ni_set_prop(&pl, NIPROP_PASSWD, "*", NULL);

    for (scan = preferred_gid; TRUE; scan++) {
	char		buf[64];
	ni_id		child;

	if (S_gid_taken(&id_list, scan)) {
	    continue;
	}
	snprintf(buf, sizeof(buf), "%d", scan);
	ni_set_prop(&pl, NIPROP_GID, buf, NULL);
	{
	    int		i;
#define MAX_RETRY		5
	    for (i = 0; i < MAX_RETRY; i++) {
		status = ni_create(NIDomain_handle(ni_local), 
				   &dir, pl, &child, NI_INDEX_NULL);
		if (status == NI_STALE) {
		    ni_self(NIDomain_handle(ni_local),
			    &dir);
		    continue;
		}
		*actual_gid = scan;
		ret = TRUE;
		goto done;
	    }
	}

	if (status != NI_OK) {
	    my_log(LOG_INFO, "bsdpd: create " NETBOOT_GROUP 
		   " group failed, %s", ni_error(status));
	    goto done;
	}
    }

 done:
    ni_proplist_free(&pl);
    ni_entrylist_free(&id_list);
    return (ret);

}

boolean_t
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
S_read_config()
{
    ni_namelist *	nl_p;

    my_log(LOG_INFO, "bsdpd: re-reading configuration");

    G_shadow_size_meg = SHADOW_SIZE_DEFAULT;
    S_afp_users_max = AFP_USERS_MAX;
    S_afp_uid_start = AFP_UID_START;
    S_age_time_seconds = AGE_TIME_SECONDS;
    S_machine_name_format = DEFAULT_MACHINE_NAME_FORMAT;

    if (PropList_read(&S_config_netboot) == TRUE) {
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_SHADOW_SIZE_MEG);
	if (nl_p && nl_p->ninl_len) {
	    G_shadow_size_meg = strtol(nl_p->ninl_val[0], 0, 0);
	}
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_AFP_USERS_MAX);
	if (nl_p && nl_p->ninl_len) {
	    S_afp_users_max = strtol(nl_p->ninl_val[0], 0, 0);
	}
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_AFP_UID_START);
	if (nl_p && nl_p->ninl_len) {
	    S_afp_uid_start = strtol(nl_p->ninl_val[0], 0, 0);
	}
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_AGE_TIME_SECONDS);
	if (nl_p && nl_p->ninl_len) {
	    S_age_time_seconds = strtoul(nl_p->ninl_val[0], 0, 0);
	}
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_MACHINE_NAME_FORMAT);
	if (nl_p && nl_p->ninl_len && S_host_format_valid(nl_p->ninl_val[0])) {
	    S_machine_name_format = nl_p->ninl_val[0];
	}
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

static boolean_t
S_set_sharepoint_permissions(NBSPListRef list, uid_t user, gid_t group)
{
    boolean_t		ret = TRUE;
    int 		i;
	
    for (i = 0; i < NBSPList_count(list); i++) {
	NBSPEntry * 	entry = NBSPList_element(list, i);
	struct stat	sb;

	/*
	 * Verify permissions/ownership
	 */
	if (set_privs(entry->path, &sb, user, group, SHARED_DIR_PERMS, 
		      FALSE) == FALSE) {
	    my_log(LOG_INFO, "bsdpd: setting permissions on '%s' failed: %m", 
		   entry->path);
	    ret = FALSE;
	}
    }

    return (ret);
}

static boolean_t
S_set_image_permissions(NBImageListRef list, uid_t user, gid_t group)
{
    boolean_t		ret = TRUE;
    int 		i;
    char		dir[PATH_MAX];
    char		file[PATH_MAX];
	
    for (i = 0; i < NBImageList_count(list); i++) {
	NBImageEntry * 	entry = NBImageList_element(list, i);
	struct stat	sb;

	/* set permissions on .nbi directory */
	snprintf(dir, sizeof(dir), "%s/%s", entry->sharepoint.path,
		 entry->dir_name);
	if (set_privs(dir, &sb, user, group, SHARED_DIR_PERMS, FALSE)
	    == FALSE) {
	    my_log(LOG_INFO, "bsdpd: setting permissions on '%s' failed: %m", 
		   dir);
	    ret = FALSE;
	}
	/* set permissions on bootfile */
	snprintf(file, sizeof(file), "%s/%s", dir, entry->bootfile);
	if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
	    == FALSE) {
	    my_log(LOG_INFO, "bsdpd: setting permissions on '%s' failed: %m", 
		   file);
	    ret = FALSE;
	}
	switch (entry->type) {
	case kNBImageTypeClassic:
	    /* set permissions on shared image */
	    snprintf(file, sizeof(file), "%s/%s", dir, 
		     entry->type_info.classic.shared);
	    if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		== FALSE) {
		my_log(LOG_INFO, 
		       "bsdpd: setting permissions on '%s' failed: %m", 
		       file);
		ret = FALSE;
	    }
	    /* set permissions on private image */
	    if (entry->type_info.classic.private != NULL) {
		snprintf(file, sizeof(file), "%s/%s", dir, 
			 entry->type_info.classic.private);
		if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		    == FALSE) {
		    my_log(LOG_INFO, 
			   "bsdpd: setting permissions on '%s' failed: %m", 
			   file);
		    ret = FALSE;
		}
	    }
	    break;
	case kNBImageTypeNFS:
	    if (entry->type_info.nfs.indirect == FALSE) {
		/* set the permissions on the root image */
		snprintf(file, sizeof(file), "%s/%s", dir, 
			 entry->type_info.nfs.root_path);
		if (set_privs(file, &sb, user, group, SHARED_FILE_PERMS, FALSE)
		    == FALSE) {
		    my_log(LOG_INFO, 
			   "bsdpd: setting permissions on '%s' failed: %m", 
			   file);
		    ret = FALSE;
		}
	    }
	    break;
	default:
	    break;
	}
    }

    return (ret);
}

static boolean_t
S_insert_image_list(const char * sysid, dhcpoa_t * options, 
		    dhcpoa_t * bsdp_options)
{
    char			buf[DHCP_OPTION_SIZE_MAX - 2 * OPTION_OFFSET];
    int				freespace;
    int 			i;
    char *			offset;

    if (G_image_list == NULL) {
	goto done;
    }

    /* space available for options minus size of tag/len (2) */
    freespace = dhcpoa_freespace(bsdp_options) - OPTION_OFFSET;
    offset = buf;

    for (i = 0; i < NBImageList_count(G_image_list); i++) {
	char				descr_buf[255];
	bsdp_image_description_t *	descr_p = (void *)descr_buf;
	int				descr_len;
	NBImageEntryRef			image_entry;
	int				name_length;

	image_entry = NBImageList_element(G_image_list, i);
	if (NBImageEntry_supported_sysid(image_entry, sysid) == FALSE) {
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
		return (TRUE);
	    }
	    dhcpoa_init_no_end(bsdp_options, dhcpoa_buffer(bsdp_options), 
			       space);
	    offset = buf;
	}
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
bsdp_init()
{
    static boolean_t 	first = TRUE;
    NBImageListRef	new_image_list;
    NBSPListRef 	new_sharepoints;
    BSDPClients_t 	new_clients;
    AFPUsers_t		new_users;

    G_disk_space_warned = FALSE;
    if (first == TRUE) {
	struct group *	group_ent_p;
	struct timeval 	tv;

	if (ni_local == NULL) {
	    my_log(LOG_INFO,
		   "bsdpd: local netinfo domain not yet open");
	    goto failed;
	}
	PropList_init(&S_config_netboot, "/config/NetBootServer");

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
	/* use microseconds for the random seed: password is a random number */
	gettimeofday(&tv, 0);
	srandom(tv.tv_usec);
	first = FALSE;
    }
    S_read_config();

    /* get the list of image sharepoints */
    new_sharepoints = NBSPList_init(NETBOOT_SHAREPOINT_LINK);
    if (new_sharepoints == NULL) {
	my_log(LOG_INFO, "bsdpd: no sharepoints defined");
	goto failed;
    }
    NBSPList_free(&S_sharepoints);
    S_sharepoints = new_sharepoints;
    if (S_set_sharepoint_permissions(S_sharepoints, ROOT_UID, 
				     G_admin_gid) == FALSE) {
	goto failed;
    }
    if (debug) {
	printf("NetBoot image sharepoints\n");
	NBSPList_print(S_sharepoints);
    }

    /* get the list of client sharepoints */
    new_sharepoints = NBSPList_init(NETBOOT_CLIENTS_SHAREPOINT_LINK);
    if (new_sharepoints == NULL) {
	my_log(LOG_INFO, "bsdpd: no client sharepoints defined");
	goto failed;
    }
    NBSPList_free(&G_client_sharepoints);
    G_client_sharepoints = new_sharepoints;
    if (S_set_sharepoint_permissions(G_client_sharepoints, ROOT_UID, 
				     G_admin_gid) == FALSE) {
	goto failed;
    }
    if (debug) {
	printf("NetBoot client sharepoints\n");
	NBSPList_print(G_client_sharepoints);
    }

    /* get the list of netboot images */
    new_image_list = NBImageList_init(S_sharepoints);
    if (new_image_list == NULL) {
	my_log(LOG_INFO, "bsdpd: no NetBoot images found");
	goto failed;
    }
    NBImageList_free(&G_image_list);
    G_image_list = new_image_list;
    if (debug) {
	NBImageList_print(G_image_list);
    }
    S_default_old_netboot_image = NBImageList_default(G_image_list, 
						      OLD_NETBOOT_SYSID);
    S_no_default_old_netboot_image_warned = FALSE;
    if (S_set_image_permissions(G_image_list, ROOT_UID, G_admin_gid)
	== FALSE) {
	goto failed;
    }
    if (BSDPClients_init(&new_clients) == FALSE) {
	my_log(LOG_INFO, "bsdpd: BSDPClients_init failed");
	goto failed;
    }
    BSDPClients_free(&S_clients);
    S_clients = new_clients;
    if (AFPUsers_init(&new_users, ni_local) == FALSE) {
	my_log(LOG_INFO, "bsdpd: AFPUsers_init failed");
	goto failed;
    }
    AFPUsers_create(&new_users, S_netboot_gid, 
		    S_afp_uid_start, S_afp_users_max);
    AFPUsers_free(&S_afp_users);
    S_afp_users = new_users;
    S_next_host_number = S_host_number_max() + 1;
    return (TRUE);

 failed:
    return (FALSE);
}

static PLCacheEntry_t *
S_reclaim_afp_user(struct timeval * time_in_p, char * * afp_user_p,
		   boolean_t * modified)
{
    PLCacheEntry_t *	reclaimed_entry = NULL;
    PLCacheEntry_t *	scan;

    *afp_user_p = NULL;

    for (scan = S_clients.list.tail; scan; scan = scan->prev) {
	char *			afp_user;
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
	    u_int32_t	t;

	    t = strtol(last_boot, NULL, NULL);
	    if (t == LONG_MAX && errno == ERANGE) {
		continue;
	    }
	    if ((time_in_p->tv_sec - t) < S_age_time_seconds) {
		/* no point in continuing, the list is kept in sorted order */
		break;
	    }
	}
	/* lookup the entry we're going to steal first */
	reclaimed_entry = PLCache_lookup_prop(&S_afp_users.list, 
					      NIPROP_NAME, afp_user, TRUE);
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
	/* mark the client has no longer bound */
	ni_delete_prop(&scan->pl, NIPROP_NETBOOT_AFP_USER, modified);
	ni_delete_prop(&scan->pl, NIPROP_NETBOOT_BOUND, modified);
	*afp_user_p = ni_valforprop(&reclaimed_entry->pl, NIPROP_NAME);
	break;
    }
    return (reclaimed_entry);
}

static PLCacheEntry_t *
S_next_afp_user(char * * afp_user)
{
    PLCacheEntry_t * scan;

    for (scan = S_afp_users.list.head; scan; scan = scan->next) {
	int		name_index;
	ni_namelist *	nl_p;

	name_index = ni_proplist_match(scan->pl, NIPROP_NAME, NULL);
	if (name_index == NI_INDEX_NULL)
	    continue;
	nl_p = &scan->pl.nipl_val[name_index].nip_val;
	if (nl_p->ninl_len == 0)
	    continue;

	if (PLCache_lookup_prop(&S_clients.list, NIPROP_NETBOOT_AFP_USER,
				nl_p->ninl_val[0], FALSE) == NULL) {
	    *afp_user = nl_p->ninl_val[0];
	    return (scan);
	}
    }
    return (NULL);
}

static boolean_t
X_netboot(NBImageEntryRef image_entry, struct in_addr server_ip,
	  const char * hostname, int host_number, uid_t uid,
	  const char * afp_user, const char * password,
	  struct dhcp * reply, dhcpoa_t * options,
	  dhcpoa_t * bsdp_options)
{
    char *	root_path = NULL;	
    char	tmp[256];

    if (image_entry->type == kNBImageTypeNFS) {
	if (image_entry->type_info.nfs.indirect == TRUE) {
	    /* pre-formatted */
	    root_path = image_entry->type_info.nfs.root_path;
	}
	else {
	    snprintf(tmp, sizeof(tmp), "nfs:%s:%s:%s/%s",
		     inet_ntoa(server_ip),
		     image_entry->sharepoint.path, image_entry->dir_name,
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
		     inet_ntoa(server_ip), 
		     image_entry->sharepoint.name,
		     image_entry->dir_name,
		     image_entry->type_info.http.root_path);
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
	char 		shadow_mount_path[256];
	char 		shadow_path[256];
	NBSPEntry *	vol;

	/* allocate shadow for diskless client */
	vol = macNC_allocate_shadow(hostname, host_number, uid, G_admin_gid,
				    kNetBootShadowName);
	if (vol == NULL) {
	    return (FALSE);
	}
	snprintf(shadow_mount_path, sizeof(shadow_mount_path), 
		 "afp://%s:%s@%s/%s",
		 afp_user, password, inet_ntoa(server_ip), vol->name);
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
S_client_update(PLCacheEntry_t * entry, struct dhcp * reply, 
		char * idstr, struct in_addr server_ip,
		NBImageEntryRef image_entry,
		dhcpoa_t * options, dhcpoa_t * bsdp_options,
		struct timeval * time_in_p)
{
    char *		afp_user = NULL;
    char *		hostname;
    int			host_number;
    bsdp_image_id_t    	image_id;
    boolean_t		modified = FALSE;
    char 		passwd[AFP_PASSWORD_LEN + 1];
    unsigned long	password = 0;
    boolean_t		ret = TRUE;
    uid_t		uid = 0;
    PLCacheEntry_t *	user_entry = NULL;
    char *		val = NULL;

    image_id = image_entry->image_id;
    hostname = ni_valforprop(&entry->pl, NIPROP_NAME);
    val = ni_valforprop(&entry->pl, NIPROP_NETBOOT_NUMBER);
    if (hostname == NULL || val == NULL) {
	my_log(LOG_INFO, "NetBoot: %s missing " NIPROP_NAME 
	       " or " NIPROP_NETBOOT_NUMBER, idstr);
	return (FALSE);
    }
    host_number = strtol(val, NULL, NULL);
    if (image_entry->diskless || image_entry->type == kNBImageTypeClassic) {
	afp_user = ni_valforprop(&entry->pl, NIPROP_NETBOOT_AFP_USER);
	if (afp_user != NULL) {
	    user_entry = PLCache_lookup_prop(&S_afp_users.list,
					     NIPROP_NAME, afp_user, TRUE);
	    if (user_entry == NULL) {
		ni_delete_prop(&entry->pl, NIPROP_NETBOOT_AFP_USER, &modified);
	    }
	}
	if (user_entry == NULL) {
	    user_entry = S_next_afp_user(&afp_user);
	    if (user_entry == NULL) {
		user_entry = S_reclaim_afp_user(time_in_p, &afp_user, 
						&modified);
	    }
	    if (user_entry == NULL) {
		my_log(LOG_INFO, 
		       "NetBoot: AFP login capacity of %d reached servicing %s",
		       S_afp_users_max, hostname);
		return (FALSE);
	    }
	    ni_set_prop(&entry->pl, NIPROP_NETBOOT_AFP_USER, afp_user, 
			&modified);
	}
	password = random();
	uid = strtoul(ni_valforprop(&user_entry->pl, NIPROP_UID), NULL, NULL);
	
	sprintf(passwd, "%08lx", password);
	if (AFPUsers_set_password(&S_afp_users, user_entry, passwd)
	    == FALSE) {
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
    default:
	my_log(LOG_INFO, "NetBoot: invalid type %d\n", image_entry->type);
	return (FALSE);
	break;
    }
    if (bootp_add_bootfile(NULL, hostname, image_entry->tftp_path,
			   reply->dp_file) == FALSE) {
	my_log(LOG_INFO, "NetBoot: bootp_add_bootfile failed");
	return (FALSE);
    }
    {
	char	buf[32];

	sprintf(buf, "0x%x", image_id);
	ni_set_prop(&entry->pl, NIPROP_NETBOOT_IMAGE_ID, buf, &modified);

	sprintf(buf, "0x%x", time_in_p->tv_sec);
	ni_set_prop(&entry->pl, NIPROP_NETBOOT_LAST_BOOT_TIME, buf, &modified);

    }
    ni_set_prop(&entry->pl, NIPROP_NETBOOT_BOUND, "true", &modified);
    if (PLCache_write(&S_clients.list, BSDP_CLIENTS_FILE) == FALSE) {
	my_log(LOG_INFO, 
	       "NetBoot: failed to save file " BSDP_CLIENTS_FILE ", %m");
    }
    return (ret);
}

static boolean_t
S_client_create(struct dhcp * reply, char * idstr, 
		char * arch, char * sysid, 
		struct in_addr server_ip,
		NBImageEntryRef image_entry,
		dhcpoa_t * options,
		dhcpoa_t * bsdp_options, struct timeval * time_in_p)
{
    char *		afp_user = NULL;
    ni_id		child = {0, 0};
    char		hostname[256];
    bsdp_image_id_t 	image_id; 
    int			host_number;
    char 		passwd[AFP_PASSWORD_LEN + 1];
    unsigned long	password;
    ni_proplist		pl;
    PLCacheEntry_t *	user_entry;
    uid_t		uid = 0;

    image_id = image_entry->image_id;
    host_number = S_next_host_number;
    NI_INIT(&pl);

    sprintf(hostname, S_machine_name_format, host_number);
    ni_proplist_addprop(&pl, NIPROP_NAME, (ni_name)hostname);
    ni_proplist_addprop(&pl, NIPROP_IDENTIFIER, (ni_name)idstr);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_ARCH, arch);
    ni_proplist_addprop(&pl, NIPROP_NETBOOT_SYSID, sysid);
    {
	char buf[32];

	sprintf(buf, "0x%x", image_id);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_IMAGE_ID, (ni_name)buf);
	sprintf(buf, "%d", host_number);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_NUMBER, (ni_name)buf);
	sprintf(buf, "0x%x", time_in_p->tv_sec);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_LAST_BOOT_TIME, buf);
    }
    if (image_entry->diskless || image_entry->type == kNBImageTypeClassic) {
	user_entry = S_next_afp_user(&afp_user);
	if (user_entry == NULL) {
	    user_entry = S_reclaim_afp_user(time_in_p, &afp_user, NULL);
	    if (user_entry == NULL) {
		my_log(LOG_INFO, "NetBoot: AFP login capacity of %d reached",
		       S_afp_users_max);
		goto failed;
	    }
	}
	password = random();
	uid = strtoul(ni_valforprop(&user_entry->pl, NIPROP_UID), NULL, NULL);
	ni_proplist_addprop(&pl, NIPROP_NETBOOT_AFP_USER, afp_user);
	sprintf(passwd, "%08lx", password);
	if (AFPUsers_set_password(&S_afp_users, user_entry, passwd)
	    == FALSE) {
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
    default:
	my_log(LOG_INFO, "NetBoot: invalid type %d\n", image_entry->type);
	return (FALSE);
	break;
    }
    if (bootp_add_bootfile(NULL, hostname, image_entry->tftp_path,
			   reply->dp_file) == FALSE) {
	my_log(LOG_INFO, "NetBoot: bootp_add_bootfile failed");
	return (FALSE);
    }
    ni_set_prop(&pl, NIPROP_NETBOOT_BOUND, "true", NULL);

    PLCache_add(&S_clients.list, PLCacheEntry_create(child, pl));
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
S_prop_u_int32(ni_proplist * pl_p, u_char * prop, u_int32_t * retval)
{
    ni_name str = ni_valforprop(pl_p, prop);

    if (str == NULL)
	return (FALSE);
    *retval = strtoul(str, 0, 0);
    if (*retval == ULONG_MAX && errno == ERANGE) {
	return (FALSE);
    }
    return (TRUE);
}

#define TXBUF_SIZE	2048
static char	txbuf[TXBUF_SIZE];


boolean_t
is_bsdp_packet(dhcpol_t * rq_options, char * arch, char * sysid,
	       dhcpol_t * rq_vsopt, bsdp_version_t * client_version,
	       boolean_t * is_old_netboot)
{
    void *		classid;
    int			classid_len;
    char		err[256];
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
    if (dhcpol_parse_vendor(rq_vsopt, rq_options, err) == FALSE) {
	if (verbose) {
	    my_log(LOG_INFO, 
		   "NetBoot: parse vendor specific options failed, %s", err);
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

void
bsdp_request(request_t * request, dhcp_msgtype_t dhcpmsg,
	     char * arch, char * sysid, dhcpol_t * rq_vsopt,
	     bsdp_version_t client_version, boolean_t is_old_netboot)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    PLCacheEntry_t *	entry;
    u_char *		idstr = NULL;
    int			max_packet = dhcp_max_message_size(request->options_p);
    dhcpoa_t		options;
    u_int16_t		reply_port = IPPORT_BOOTPC;
    struct dhcp *	reply = NULL;
    struct dhcp *	rq = request->pkt;

    if (dhcpmsg != dhcp_msgtype_discover_e
	&& dhcpmsg != dhcp_msgtype_inform_e) {
	return;
    }
    if (is_old_netboot
	&& S_default_old_netboot_image == NULL) {
	if (S_no_default_old_netboot_image_warned == FALSE) {
	    my_log(LOG_INFO, 
		   "BSDP: no NetBoot 1.0 images, ignoring NetBoot 1.0 client"
		   " requests");
	    S_no_default_old_netboot_image_warned = TRUE;
	}
	/* no NetBoot 1.0 images */
	return;
    }

    if (strcmp(arch, "ppc")) {
	return;
    }
    idstr = identifierToString(rq->dp_htype, 
			       rq->dp_chaddr, 
			       rq->dp_hlen);
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
	  image_entry = NBImageList_elementWithID(G_image_list, image_id);
	  if (image_entry == NULL
	      || NBImageEntry_supported_sysid(image_entry, sysid)) {
	      /* stale image ID */
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

	  if (S_client_update(entry, reply, idstr, if_inet_addr(request->if_p),
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
	      
	      reply->dp_siaddr = if_inet_addr(request->if_p);
	      strcpy(reply->dp_sname, server_name);

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

		if (is_old_netboot) {
		    default_image = S_default_old_netboot_image;
		}
		else {
		    default_image = NBImageList_default(G_image_list, 
							filter_sysid);
		}
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
		if (reply == NULL)
		    goto no_reply;
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

		    bound = ni_valforprop(&entry->pl, NIPROP_NETBOOT_BOUND);
		    if ((bound == NULL)
			|| (strchr(testing_control, 'e') != NULL)
			|| (S_prop_u_int32(&entry->pl,
					   NIPROP_NETBOOT_IMAGE_ID, 
					   &image_id) == FALSE)
			|| ((image_entry 
			     = NBImageList_elementWithID(G_image_list, 
							 image_id)) == NULL)
			|| (NBImageEntry_supported_sysid(image_entry, 
							 filter_sysid)
			    == FALSE)) {
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
		    if (S_insert_image_list(filter_sysid, &options, 
					    &bsdp_options) 
			== FALSE) {
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
			|| NBImageEntry_supported_sysid(image_entry, 
							filter_sysid)
			    == FALSE) {
			/* stale image ID */
			goto send_failed;
		    }
		}
		else {
		    if (is_old_netboot == FALSE) {
			image_entry = NBImageList_default(G_image_list,
							  filter_sysid);
			if (image_entry == NULL) {
			    /* no longer a default image */
			    goto send_failed;
			}
		    }
		    else {
			image_entry = S_default_old_netboot_image;
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
		    my_log(LOG_INFO, "NetBoot: [%s] add message type failed, %s",
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
		if (entry) {
		    if (S_client_update(entry, reply, idstr, 
					if_inet_addr(request->if_p),
					image_entry, &options, &bsdp_options, 
					request->time_in_p) == FALSE) {
			goto send_failed;
		    }
		}
		else {
		    if (S_client_create(reply, idstr, arch, sysid, 
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
	      
	      reply->dp_siaddr = if_inet_addr(request->if_p);
	      strcpy(reply->dp_sname, server_name);

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
    if (idstr)
	free(idstr);
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
    struct in_addr	iaddr = {0};
    char *		idstr = NULL;
    dhcpoa_t		options;
    struct dhcp *	rq = request->pkt;
    struct dhcp *	reply = NULL;
    id			subnet = nil;
    u_int32_t		version = MACNC_SERVER_VERSION;

    if (macNC_get_client_info(rq, request->pkt_length,
			      request->options_p, NULL) == FALSE) {
	return (FALSE);
    }
    if (S_default_old_netboot_image == NULL) {
	if (S_no_default_old_netboot_image_warned == FALSE) {
	    my_log(LOG_INFO, 
		   "NetBoot[BOOTP]: no NetBoot 1.0 images, "
		   "ignoring NetBoot 1.0 client requests");
	    S_no_default_old_netboot_image_warned = TRUE;
	}
	/* no NetBoot 1.0 images */
	goto no_reply;
    }

    idstr = identifierToString(rq->dp_htype, 
			       rq->dp_chaddr, rq->dp_hlen);
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
    reply = make_bsdp_bootp_reply((struct dhcp *)txbuf, DHCP_PACKET_MIN,
				  rq, &options);
    if (reply == NULL)
	goto no_reply;

    reply->dp_yiaddr = iaddr;
    reply->dp_siaddr = if_inet_addr(request->if_p);
    strcpy(reply->dp_sname, server_name);

    /* add the client-specified parameters */
    (void)add_subnet_options(NULL, NULL, iaddr, 
			     request->if_p, &options, NULL, 0);

    /* ready the vendor-specific option area to hold bsdp options */
    dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    
    if (bsdp_entry) {
	NBImageEntryRef	image_entry;
	bsdp_image_id_t	image_id;

	if ((S_prop_u_int32(&bsdp_entry->pl, NIPROP_NETBOOT_IMAGE_ID, 
			    &image_id) == FALSE)
	    || ((image_entry 
		 = NBImageList_elementWithID(G_image_list, image_id)) == NULL)
	    || (NBImageEntry_supported_sysid(image_entry, OLD_NETBOOT_SYSID)
		== FALSE)) {
	    /* stale image id, use default */
	    image_entry = S_default_old_netboot_image;
	}
	if (S_client_update(bsdp_entry, reply, idstr, 
			    if_inet_addr(request->if_p),
			    image_entry, &options, &bsdp_options,
			    request->time_in_p) == FALSE) {
	    goto no_reply;
	}
    }
    else {
	if (S_client_create(reply, idstr, "ppc", "unknown", 
			    if_inet_addr(request->if_p), 
			    S_default_old_netboot_image,
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
	
	reply->dp_siaddr = if_inet_addr(request->if_p);
	strcpy(reply->dp_sname, server_name);
	
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
    if (idstr != NULL) {
	free(idstr);
    }
    return (TRUE);
}

