/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * macNC.c
 * - macNC boot server
 * - supports netboot clients by:
 *   + allocating IP addresses
 *   + locating/creating disk image files
 *   + creating/providing AFP login
 */

/*
 * Modification History:
 *
 * December 2, 1997	Dieter Siegmund (dieter@apple.com)
 * - created
 * February 1, 1999	Dieter Siegmund (dieter@apple.com)
 * - create sharepoints at init time (and anytime we get a SIGHUP)
 *   and ensure permissions are correct
 * November 2, 2000	Dieter Siegmund (dieter@apple.com)
 * - removed code that creates sharepoints
 */

#import <unistd.h>
#import <stdlib.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/file.h>
#import	<pwd.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <netinet/if_ether.h>
#import <stdio.h>
#import <strings.h>
#import <errno.h>
#import <fcntl.h>
#import <ctype.h>
#import <netdb.h>
#import <syslog.h>
#import <sys/param.h>
#import <sys/mount.h>
#import <arpa/inet.h>
#import <mach/boolean.h>
#import <sys/wait.h>
#import <sys/resource.h>
#import <ctype.h>
#import <grp.h>

#import "dhcp.h"
#import "netinfo.h"
#import "rfc_options.h"
#import "subnetDescr.h"
#import "interfaces.h"
#import "bootpd.h"
#import "macnc_options.h"
#import "macNC.h"
#import "host_identifier.h"
#import "NICache.h"
#import "hfsvols.h"
#import "nbsp.h"
#import "AFPUsers.h"
#import "NetBootServer.h"

#define AGE_TIME_SECONDS	(60 * 60 * 24)
#define AFP_USERS_MAX		50
#define ADMIN_GROUP_NAME	"admin"
#define ROOT_UID		0

/* external functions */
char *  	ether_ntoa(struct ether_addr *e);

/* globals */
gid_t		netboot_gid;
int		afp_users_max = AFP_USERS_MAX;
u_int32_t	age_time_seconds = AGE_TIME_SECONDS;

/* local defines/variables */
#define NIPROP__CREATOR		"_creator"

#define MAX_RETRY		5

#define SHARED_DIR_PERMS	0775
#define SHARED_FILE_PERMS	0664

#define CLIENT_DIR_PERMS	0770
#define CLIENT_FILE_PERMS	0660

/*
 * Define: SHADOW_SIZE_SAME
 * Meaning:
 *   Make the shadow size file exactly the same size as the file
 *   being shadowed (default behaviour required by NetBoot in Mac OS 8.5).
 */ 
#define SHADOW_SIZE_SAME	0 
#define SHADOW_SIZE_DEFAULT	48
#define MACOSROM	"Mac OS ROM"
static gid_t		S_admin_gid;

static nbspList_t	S_sharepoints = NULL;
static boolean_t	S_disk_space_warned = FALSE;
static boolean_t	S_init_done = FALSE;
static u_long		S_shadow_size_meg = SHADOW_SIZE_DEFAULT;

/* strings retrieved from the configuration directory: */
static ni_name		S_client_image_dir = "Clients";
static ni_name		S_default_bootfile = MACOSROM;
static ni_name		S_images_dir = "Images";
static ni_name		S_private_image_name = "Applications HD.img";
static ni_name		S_shadow_name = "Shadow";
static nbspEntry_t *	S_private_image_volume = NULL;
static ni_name		S_shared_image_name = "NetBoot HD.img";
static nbspEntry_t *	S_shared_image_volume = NULL;

static PropList_t 	S_config_netboot;

static __inline__ void
S_timestamp(char * msg)
{
    if (verbose)
	timestamp_syslog(msg);
}

static __inline__ boolean_t
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
S_set_privs_no_stat(u_char * path, struct stat * sb_p, uid_t uid, gid_t gid,
		    mode_t mode, boolean_t lock)
{
    boolean_t		needs_chown = FALSE;
    boolean_t		needs_chmod = FALSE;

    if (sb_p->st_uid != uid || sb_p->st_gid != gid)
	needs_chown = TRUE;

    if ((sb_p->st_mode & ACCESSPERMS) != mode)
	needs_chmod = TRUE;
     
    if (needs_chown || needs_chmod) {
	if (sb_p->st_flags & UF_IMMUTABLE) {
	    if (chflags(path, 0) < 0)
		return (FALSE);
	}
	if (needs_chown) {
	    if (chown(path, uid, gid) < 0)
		return (FALSE);
	}
	if (needs_chmod) {
	    if (chmod(path, mode) < 0)
		return (FALSE);
	}
	if (lock) {
	    if (chflags(path, UF_IMMUTABLE) < 0)
		return (FALSE);
	}
    }
    else if (lock) {
	if ((sb_p->st_flags & UF_IMMUTABLE) == 0) {
	    if (chflags(path, UF_IMMUTABLE) < 0)
		return (FALSE);
	}
    }
    else if (sb_p->st_flags & UF_IMMUTABLE) {
	if (chflags(path, 0) < 0)
	    return (FALSE);
    }
    return (TRUE);
}

static boolean_t
S_set_privs(u_char * path, struct stat * sb_p, uid_t uid, gid_t gid,
	    mode_t mode, boolean_t lock)
{
    if (stat(path, sb_p) != 0) {
	return (FALSE);
    }
    return (S_set_privs_no_stat(path, sb_p, uid, gid, mode, lock));
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
	syslog(LOG_INFO, "macNC: ni_pathsearch '%s' failed, %s",
	       NIDIR_GROUPS, ni_error(status));
	return (FALSE);
    }
    status = ni_list(NIDomain_handle(ni_local), &dir,
		     NIPROP_GID, &id_list);
    if (status != NI_OK) {
	syslog(LOG_INFO, "macNC: ni_list '%s' failed, %s",
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
	    syslog(LOG_INFO, "macNC: create " NETBOOT_GROUP 
		   " group failed, %s", ni_error(status));
	    goto done;
	}
    }

 done:
    ni_proplist_free(&pl);
    ni_entrylist_free(&id_list);
    return (ret);

}

static void
S_read_config()
{
    ni_namelist *	nl_p;

    S_shadow_size_meg = SHADOW_SIZE_DEFAULT;
    afp_users_max = AFP_USERS_MAX;
    age_time_seconds = AGE_TIME_SECONDS;

    if (PropList_read(&S_config_netboot) == TRUE) {
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_SHADOW_SIZE_MEG);
	if (nl_p && nl_p->ninl_len) {
	    S_shadow_size_meg = strtol(nl_p->ninl_val[0], 0, 0);
	}
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_AFP_USERS_MAX);
	if (nl_p && nl_p->ninl_len) {
	    afp_users_max = strtol(nl_p->ninl_val[0], 0, 0);
	}
	nl_p = PropList_lookup(&S_config_netboot, CFGPROP_AGE_TIME_SECONDS);
	if (nl_p && nl_p->ninl_len) {
	    age_time_seconds = strtoul(nl_p->ninl_val[0], 0, 0);
	}
    }
    if (S_shadow_size_meg == SHADOW_SIZE_SAME) {
	syslog(LOG_INFO, 
	       "macNC: shadow file size will be set to image file size");
    }
    else {
	syslog(LOG_INFO, 
	       "macNC: shadow file size will be set to %d megabytes",
	       S_shadow_size_meg);
    }
    {
	u_int32_t	hours = 0;
	u_int32_t	minutes = 0;
	u_int32_t	seconds = 0;
	u_int32_t	remainder = age_time_seconds;

#define SECS_PER_MINUTE	60
#define SECS_PER_HOUR	(60 * SECS_PER_MINUTE)

	hours = remainder / SECS_PER_HOUR;
	remainder = remainder % SECS_PER_HOUR;
	if (remainder > 0) {
	    minutes = remainder / SECS_PER_MINUTE;
	    remainder = remainder % SECS_PER_MINUTE;
	    seconds = remainder;
	}
	syslog(LOG_INFO,
	       "macNC: age time %02u:%02u:%02u", hours, minutes, seconds);
    }
    return;
}

static void
S_update_bootfile_symlink(char * path)
{
    (void)mkdir("/private/tftpboot", 0755);
    (void)symlink(path, "/private/tftpboot/" MACOSROM);
    return;
}

static nbspEntry_t *
S_find_images(nbspList_t list)
{
    nbspEntry_t * images = NULL;
    int i;

    for (i = 0; i < nbspList_count(list); i++) {
	nbspEntry_t * 	entry = nbspList_element(list, i);
	char		path[PATH_MAX];
	struct stat	sb;

	snprintf(path, sizeof(path), "%s/%s/%s",
		 entry->path, S_images_dir, S_shared_image_name);
	if (stat(path, &sb) < 0) {
	    continue;
	}
	snprintf(path, sizeof(path), "%s/%s/%s",
		 entry->path, S_images_dir, S_default_bootfile);
	if (stat(path, &sb) < 0) {
	    continue;
	}
	snprintf(path, sizeof(path), "%s/%s", entry->path, S_images_dir);
	if (S_set_privs(path, &sb, ROOT_UID, S_admin_gid, SHARED_DIR_PERMS, FALSE)
	    == FALSE) {
	    syslog(LOG_INFO, "macNC: setting permissions on '%s' failed: %m", 
		   path);
	    continue;
	}
	images = entry;
	snprintf(path, sizeof(path), "%s/%s/%s",
		 entry->path, S_images_dir, S_default_bootfile);
	S_update_bootfile_symlink(path);
	break;
    }
    return (images);
}

boolean_t
S_set_sharepoint_permissions(nbspList_t list, uid_t user, gid_t group)
{
    boolean_t		ret = TRUE;
    int 		i;
	
    for (i = 0; i < nbspList_count(list); i++) {
	nbspEntry_t * 	entry = nbspList_element(list, i);
	char		path[PATH_MAX];
	struct stat	sb;

	/*
	 * Verify permissions/ownership
	 */
	if (S_set_privs(entry->path, &sb, user, group, SHARED_DIR_PERMS, 
			FALSE) == FALSE) {
	    syslog(LOG_INFO, "macNC: setting permissions on '%s' failed: %m", 
		   entry->path);
	    ret = FALSE;
	}
	snprintf(path, sizeof(path), "%s/%s", entry->path, S_client_image_dir);
	(void)mkdir(path, SHARED_DIR_PERMS);
	if (S_set_privs(path, &sb, user, group, SHARED_DIR_PERMS, FALSE)
	    == FALSE) {
	    syslog(LOG_INFO, "macNC: setting permissions on '%s' failed: %m", 
		   path);
	    ret = FALSE;
	}
    }

    return (ret);
}

/*
 * Function: S_cfg_init
 *
 * Purpose:
 *   This function does all of the variable initialization needed by the
 *   boot server.  It can be called multiple times if necessary.
 */
static boolean_t
S_cfg_init()
{
    nbspList_t new_list;

    syslog(LOG_INFO, "macNC: re-reading configuration");

    S_read_config();

    /* get the list of sharepoints */
    new_list = nbspList_init();
    if (new_list) {
	if (S_sharepoints)
	    nbspList_free(&S_sharepoints);
	S_sharepoints = new_list;
	S_shared_image_volume = S_find_images(S_sharepoints);
	if (S_shared_image_volume == NULL) {
	    syslog(LOG_INFO, "macNC: NetBoot images not found");
	    return (FALSE);
	}
	S_private_image_volume = S_shared_image_volume;
    }
    else if (S_sharepoints == NULL) {
	return (FALSE);
    }

    if (S_set_sharepoint_permissions(S_sharepoints, ROOT_UID, 
				     S_admin_gid) == FALSE) {
	return (FALSE);
    }

    if (debug) {
	nbspList_print(S_sharepoints);
    }

    return (TRUE);
}

/*
 * Function: S_init
 *
 * Purpose:
 *   Initialize state for dealing with macNC's:
 * Returns:
 *   TRUE if success, FALSE if failure
 */
static boolean_t
S_init()
{
    struct group *	group_ent_p;
    struct timeval 	tv;

    /* one-time initialization */
    if (S_init_done)
	return (TRUE);

    if (ni_local == NULL) {
	syslog(LOG_INFO,
	       "macNC: local netinfo domain not yet open");
	return (FALSE);
    }

    PropList_init(&S_config_netboot, "/config/NetBootServer");

    /* get the netboot group id, or create the group if necessary */
    group_ent_p = getgrnam(NETBOOT_GROUP);
    if (group_ent_p == NULL) {
#define NETBOOT_GID	120
	if (S_create_netboot_group(NETBOOT_GID, &netboot_gid) == FALSE) {
	    return (FALSE);
	}
    }
    else {
	netboot_gid = group_ent_p->gr_gid;
    }

    /* get the admin group id */
    group_ent_p = getgrnam(ADMIN_GROUP_NAME);
    if (group_ent_p == NULL) {
	syslog(LOG_INFO, "macNC: getgrnam " ADMIN_GROUP_NAME " failed");
	return (FALSE);
    }
    S_admin_gid = group_ent_p->gr_gid;

    /* read the configuration directory */
    if (S_cfg_init() == FALSE) {
	return (FALSE);
    }

    /* use microseconds for the random seed: password is a random number */
    gettimeofday(&tv, 0);
    srandom(tv.tv_usec);

    /* one-time initialization */
    S_init_done = TRUE;
    return (TRUE);
}

/*
 * Function: macNC_init
 *
 * Purpose:
 *   Called from bootp if we received a SIGHUP.
 */
boolean_t
macNC_init()
{
    S_disk_space_warned = FALSE;

    if (S_init_done == TRUE)
	(void)S_cfg_init(); /* subsequent initialization */
    else if (S_init() == FALSE) {  /* one-time initialization */
	syslog(LOG_INFO, "macNC: NetBoot service turned off");
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: S_set_uid_gid
 *
 * Purpose:
 *   Given a path to a file, make the owner of both the
 *   enclosing directory and the file itself to user/group uid/gid.
 */
static int
S_set_uid_gid(u_char * file, uid_t uid, gid_t gid)
{
    u_char 	dir[PATH_MAX];
    u_char *	last_slash = strrchr(file, '/');

    if (file[0] != '/' || last_slash == NULL) {
	if (debug)
	    printf("path '%s' is not valid\n", file);
	return (-1);
    }

    strncpy(dir, file, last_slash - file);
    dir[last_slash - file] = '\0';
    if (chown(dir, uid, gid) == -1)
	return (-1);
    if (chown(file, uid, gid) == -1)
	return (-1);
    return (0);
}

/**
 ** Other local utility routines:
 **/


/*
 * Function: S_get_client_info
 *
 * Purpose:
 *   Retrieve the macNC client information from the given packet.
 *   First try to parse the dhcp options, then look for the client
 *   version tag and client info tag.  The client info tag will
 *   contain "Apple MacNC".
 *
 * Returns:
 *   TRUE and client_version if client version is present in packet
 *   FALSE otherwise
 */
boolean_t
macNC_get_client_info(struct dhcp * pkt, int pkt_size, dhcpol_t * options, 
		      u_int * client_version)
{ /* get the client version info - if not present, not an NC */
    void *		client_id;
    int			opt_len;
    void *		vers;

    vers = dhcpol_find(options, macNCtag_client_version_e,
		       &opt_len, NULL);
    if (vers == NULL)
	return (FALSE);

    client_id = dhcpol_find(options, macNCtag_client_info_e,
			    &opt_len, NULL);
    if (client_id == NULL)
	return (FALSE);
    if (opt_len != strlen(MACNC_CLIENT_INFO) 
	|| bcmp(client_id, MACNC_CLIENT_INFO, opt_len)) {
	return (FALSE);
    }
    if (client_version)
	*client_version = ntohl(*((unsigned long *)vers));
    return (TRUE);
}

static __inline__ boolean_t
S_make_finder_info(u_char * shadow_path, u_char * real_path)
{
    return (hfs_copy_finder_info(shadow_path, real_path));
}


/*
 * Function: S_get_volpath
 *
 * Purpose:
 *   Format a volume pathname given a volume, directory and file name.
 */
static void
S_get_volpath(u_char * path, nbspEntry_t * entry, u_char * dir, u_char * file)
{
    snprintf(path, PATH_MAX, "%s%s%s%s%s",
	     entry->path,
	     (dir && *dir) ? "/" : "",
	     (dir && *dir) ? (char *)dir : "",
	     (file && *file) ? "/" : "",
	     (file && *file) ? (char *)file : "");
    return;
}

/*
 * Function: S_create_volume_dir
 *
 * Purpose:
 *   Create the given directory path on the given volume.
 */
static boolean_t
S_create_volume_dir(nbspEntry_t * entry, u_char * dirname, mode_t mode)
{
    u_char 		path[PATH_MAX];

    S_get_volpath(path, entry, dirname, NULL);
    if (create_path(path, mode) < 0) {
	syslog(LOG_INFO, "macNC: create_volume_dir: create_path(%s)"
	       " failed, %m",  path);
	return (FALSE);
    }
    (void)chmod(path, mode);
    return (TRUE);
}

/*
 * Function: S_create_shadow_file
 *
 * Purpose:
 *   Create a new empty file with the given size and attributes
 *   from another file.
 */
static boolean_t
S_create_shadow_file(u_char * shadow_path, u_char * real_path,
		     uid_t uid, gid_t gid, unsigned long long size)
{
    int 		fd;

    S_set_uid_gid(shadow_path, ROOT_UID, 0);

    fd = open(shadow_path, O_CREAT | O_TRUNC | O_WRONLY, CLIENT_FILE_PERMS);
    if (fd < 0) {
	syslog(LOG_INFO, "macNC: couldn't create file '%s': %m", shadow_path);
	return (FALSE);
    }
    if (hfs_set_file_size(fd, size)) {
	syslog(LOG_INFO, "macNC: hfs_set_file_size '%s' failed: %m",
	       shadow_path);
	goto err;
    }

    if (S_make_finder_info(shadow_path, real_path) == FALSE) 
	goto err;

    fchmod(fd, CLIENT_FILE_PERMS);
    close(fd);

    /* correct the owner of the path */
    if (S_set_uid_gid(shadow_path, uid, gid)) {
	syslog(LOG_INFO, "macNC: setuidgid '%s' to %ld,%ld failed: %m", 
	       shadow_path, uid, gid);
	return (FALSE);
    }
    return (TRUE);

  err:
    close(fd);
    return (FALSE);
}

static boolean_t
S_add_afppath_option(struct in_addr servip, dhcpoa_t * options, 
		     nbspEntry_t * entry, u_char * dir, u_char * file, int tag)
{
    u_char		buf[DHCP_OPTION_SIZE_MAX];
    u_char		err[256];
    int			len;
    u_char		path[PATH_MAX];

    if (dir && *dir)
	snprintf(path, sizeof(path), "%s/%s", dir, file);
    else {
	snprintf(path, sizeof(path), "%s", file);
    }
	
    len = sizeof(buf);
    if (macNCopt_encodeAFPPath(servip, AFP_PORT_NUMBER, entry->name,
			       AFP_DIRID_NULL, AFP_PATHTYPE_LONG,
			       path, '/', buf, &len, err) == FALSE) {
	syslog(LOG_INFO, "macNC: couldn't encode %s:%s, %s", entry->name, path,
	       err);
	return (FALSE);
    }
    if (dhcpoa_add(options, tag, len, buf) != dhcpoa_success_e) {
	syslog(LOG_INFO, "macNC: couldn't add option %d failed: %s", tag,
	       dhcpoa_err(options));
	return (FALSE);
    }
    return (TRUE);
}


/*
 * Function: S_stat_path_vol_file
 *
 * Purpose:
 *   Return the stat structure for the given volume/dir/file.
 */
static __inline__ int
S_stat_path_vol_file(u_char * path, nbspEntry_t * entry, 
		     u_char * dir, u_char * file,
		     struct stat * sb_p)
{
    S_get_volpath(path, entry, dir, file);
    return (stat(path, sb_p));
}


static __inline__ boolean_t
S_stat_shared(u_char * shared_path, struct stat * sb_p)
{
    S_get_volpath(shared_path, S_shared_image_volume, S_images_dir,
		  S_shared_image_name);
    return (S_set_privs(shared_path, sb_p, ROOT_UID, S_admin_gid,
			SHARED_FILE_PERMS, TRUE));
}

static __inline__ boolean_t
S_stat_private(u_char * private_path, struct stat * sb_p)
{
    S_get_volpath(private_path, S_private_image_volume, S_images_dir,
		  S_private_image_name);
    return (S_set_privs(private_path, sb_p, ROOT_UID, S_admin_gid,
			SHARED_FILE_PERMS, TRUE));
}

static __inline__ boolean_t
S_stat_bootfile(u_char * bootfile_path, struct stat * sb_p)
{
    S_get_volpath(bootfile_path, S_shared_image_volume, S_images_dir,
		  S_default_bootfile);
    return (S_set_privs(bootfile_path, sb_p, ROOT_UID, S_admin_gid,
			SHARED_FILE_PERMS, TRUE));
}

static boolean_t
S_freespace(u_char * path, unsigned long long * size)
{
    struct statfs 	fsb;

    if (statfs(path, &fsb) != 0) {
	syslog(LOG_INFO, "macNC: statfs on '%s' failed %m", path);
	return (FALSE);
    }
    *size = ((unsigned long long)fsb.f_bavail) 	
	* ((unsigned long long)fsb.f_bsize);
    if (debug)
	printf("%s %lu x %lu = %qu bytes\n", path,
	       fsb.f_bavail, fsb.f_bsize, *size);
    return (TRUE);
}

static nbspEntry_t *
S_find_volume_with_space(unsigned long long needspace, int def_vol_index)
{
    unsigned long long	freespace;
    int 		i;
    nbspEntry_t *	entry = NULL;
    u_char		path[PATH_MAX];
    int			vol_index;

    for (i = 0, vol_index = def_vol_index; i < nbspList_count(S_sharepoints);
	 i++) {
	nbspEntry_t * shp = nbspList_element(S_sharepoints, vol_index);
	
	S_get_volpath(path, shp, NULL, NULL);
	if (S_freespace(path, &freespace) == TRUE) {
#define SLOP_SPACE_BYTES	(20 * 1024 * 1024)
	    /* make sure there's some space left on the volume */
	    if (freespace >= (needspace + SLOP_SPACE_BYTES)) {
		entry = shp;
		if (debug)
		    printf("selected volume %s\n", entry->name);
		break; /* out of for */
	    }
	}
	vol_index = (vol_index + 1) % nbspList_count(S_sharepoints);
    }
    return (entry);
}

static boolean_t
S_remove_shadow(u_char * shadow_path, nbspEntry_t * entry, u_char * dir)
{
    u_char path[PATH_MAX];

    /* remove the shadow file */
    S_set_uid_gid(shadow_path, ROOT_UID, 0);
    unlink(shadow_path);
    S_get_volpath(path, entry, dir, NULL);
    /* and its directory */
    if (rmdir(path)) {
	u_char new_path[PATH_MAX];
	if (debug)
	    perror(path);
	S_get_volpath(new_path, entry, "Delete Me", NULL);
	/* couldn't delete it, try to rename it */
	if (rename(path, new_path)) {
	    return (FALSE);
	}
    }
    return (TRUE);
}

/* 
 * Function: S_add_image_options
 * 
 * Purpose:
 *   Create/initialize image for client, format the paths into the
 *   response options.
 */
static boolean_t
S_add_image_options(uid_t uid, gid_t gid, struct in_addr servip, 
		    dhcpoa_t * options, int host_number, 
		    u_char * afp_hostname)
{
    int			def_vol_index;
    u_char		dir_path[PATH_MAX];
    struct stat		dir_statb;
    int			i;
    u_char		nc_images_dir[PATH_MAX];
    nbspEntry_t *	nc_volume = NULL;
    u_char		path[PATH_MAX];
    struct stat		statb;
    int			vol_index;

    if (S_stat_bootfile(path, &statb) == FALSE) {
	syslog(LOG_INFO, "macNC: '%s' does not exist", path);
	return (FALSE);
    }
    snprintf(nc_images_dir, sizeof(nc_images_dir),
	     "%s/%s", S_client_image_dir, afp_hostname);

    /* attempt to round-robin images across multiple volumes */
    def_vol_index = (host_number - 1) % nbspList_count(S_sharepoints);

    /* check all volumes for a client image directory starting at default */
    nc_volume = NULL;
    for (i = 0, vol_index = def_vol_index; i < nbspList_count(S_sharepoints);
	 i++) {
	nbspEntry_t * entry = nbspList_element(S_sharepoints, vol_index);
	if (S_stat_path_vol_file(dir_path, entry,
				 nc_images_dir, NULL, &dir_statb) == 0) {
	    nc_volume = entry;
	    break;
	}
	vol_index = (vol_index + 1) % nbspList_count(S_sharepoints);
    }

    /* if the client has its own private copy of the image file, use it */
    if (nc_volume != NULL
	&& S_stat_path_vol_file(path, nc_volume, nc_images_dir, 
				S_shared_image_name, &statb) == 0) {
	/* set the image file perms */
	if (S_set_privs_no_stat(path, &statb, uid, gid, CLIENT_FILE_PERMS, 
				FALSE) == FALSE) {
	    syslog(LOG_INFO, "macNC: couldn't set permissions on path %s: %m",
		   path);
	    return (FALSE);
	}
	/* set the client dir perms */
	if (S_set_privs_no_stat(dir_path, &dir_statb, uid, gid, 
				CLIENT_DIR_PERMS, FALSE) == FALSE) {
	    syslog(LOG_INFO, "macNC: couldn't set permissions on path %s: %m",
		   dir_path);
	    return (FALSE);
	}
	if (S_add_afppath_option(servip, options, nc_volume, nc_images_dir,
				 S_shared_image_name,
				 macNCtag_shared_system_file_e) == FALSE) {
	    return (FALSE);
	}
	/* does the client have its own Private image? */
	if (S_stat_path_vol_file(path, nc_volume, nc_images_dir,
				 S_private_image_name, &statb) == 0) {
	    if (S_set_privs_no_stat(path, &statb, uid, gid, 
				    CLIENT_FILE_PERMS, FALSE) == FALSE) {
		syslog(LOG_INFO, 
		       "macNC: couldn't set permissions on path %s: %m", path);
		return (FALSE);
	    }
	    /*
	     * We use macNCtag_page_file_e instead of 
	     * macNCtag_private_system_file_e as you would expect.
	     * The reason is that the client ROM software assumes
	     * that the private_system_file is read-only. It also
	     * assumes that page_file is read-write.  Since we don't
	     * use page_file for anything else, we use that instead.
	     * This is a hack/workaround.
	     */
	    if (S_add_afppath_option(servip, options, nc_volume, 
				     nc_images_dir, S_private_image_name,
				     macNCtag_page_file_e) == FALSE){
		return (FALSE);
	    }
	}
    }
    else { /* client gets shadow file(s) */
	unsigned long long	needspace;
	u_char			private_path[PATH_MAX];
	struct stat		sb_shared;
	struct stat		sb_private;
	u_char 			shadow_path[PATH_MAX];
	u_char 			shared_path[PATH_MAX];

	/* make sure that the shared system image exists */
	if (S_stat_shared(shared_path, &sb_shared) == FALSE) {
	    syslog(LOG_INFO, "macNC: '%s' does not exist", shared_path);
	    return (FALSE);
	}
	/* add the shared system image option */
	if (S_add_afppath_option(servip, options, S_shared_image_volume,
				 S_images_dir, S_shared_image_name, 
				 macNCtag_shared_system_file_e) == FALSE)
	    return (FALSE);
	if (S_stat_private(private_path, &sb_private)) {
	    if (S_add_afppath_option(servip, options, S_private_image_volume,
				     S_images_dir, S_private_image_name, 
				     macNCtag_private_system_file_e) == FALSE)
		return (FALSE);
	}

#define ONE_MEG		(1024UL * 1024UL)
	needspace = (S_shadow_size_meg == SHADOW_SIZE_SAME) 
	    ? sb_shared.st_size
	    : ((unsigned long long)S_shadow_size_meg) * ONE_MEG;

	if (nc_volume != NULL) {
	    struct stat		sb_shadow;
	    boolean_t		set_file_size = FALSE;
	    boolean_t		set_owner_perms = FALSE;
	    
	    S_get_volpath(shadow_path, nc_volume, nc_images_dir, 
			  S_shadow_name);
	    if (stat(shadow_path, &sb_shadow) == 0) { /* shadow exists */
		S_timestamp("shadow file exists");
		if (debug)
		    printf("shadow %qu need %qu\n", 
			   sb_shadow.st_size, needspace);

		if (sb_shadow.st_uid != uid
		    || sb_shadow.st_gid != gid
		    || (sb_shadow.st_mode & ACCESSPERMS) != CLIENT_FILE_PERMS)
		    set_owner_perms = TRUE;

		if (sb_shadow.st_size != needspace) {
		    set_file_size = TRUE;
		    if (sb_shadow.st_size < needspace) {
			unsigned long long  	difference;
			unsigned long long	freespace = 0;
			
			S_timestamp("shadow file needs to be grown");
			/* check for enough space */
			(void)S_freespace(shadow_path, &freespace);
			difference = (needspace - sb_shadow.st_size);
			if (freespace < difference) {
			    syslog(LOG_INFO, "macNC: device full, "
				   "attempting to relocate %s",
				   shadow_path);
			    /* blow away the shadow */
			    if (S_remove_shadow(shadow_path, nc_volume,
						nc_images_dir) == FALSE) {
				syslog(LOG_INFO, "macNC: couldn't remove"
				       " shadow %s, %m", shadow_path);
				return (FALSE);
			    }
			    /* start fresh */
			    nc_volume = NULL;
			}
		    }
		}
	    }
	    else {
		/* start fresh */
		if (S_remove_shadow(shadow_path, nc_volume, nc_images_dir)
		    == FALSE) {
		    syslog(LOG_INFO, "macNC: couldn't remove"
			   " shadow %s, %m", shadow_path);
		    return (FALSE);
		}
		nc_volume = NULL;
	    }
	    if (nc_volume) {
		if (set_file_size) {
		    S_timestamp("setting shadow file size");
		    if (S_create_shadow_file(shadow_path, shared_path, 
					     uid, gid, needspace) == FALSE) {
			syslog(LOG_INFO, "macNC: couldn't create %s, %m",
			       shadow_path);
			return (FALSE);
		    }
		    S_timestamp("shadow file size set");
		}
		else if (set_owner_perms) {
		    S_timestamp("setting shadow file perms/owner");
		    chmod(shadow_path, CLIENT_FILE_PERMS);
		    S_set_uid_gid(shadow_path, uid, gid);
		    S_timestamp("shadow file perms/owner set");
		}
	    }
	}
	if (nc_volume == NULL) { /* locate the client's image dir */
	    nc_volume = S_find_volume_with_space(needspace, def_vol_index);
	    if (nc_volume == NULL) {
		if (S_disk_space_warned == FALSE)
		    syslog(LOG_INFO, "macNC: can't create client image: "
			   "OUT OF DISK SPACE");
		S_disk_space_warned = TRUE; /* don't keep complaining */
		return (FALSE);
	    }
	    S_get_volpath(shadow_path, nc_volume, nc_images_dir, 
			  S_shadow_name);
	    S_disk_space_warned = FALSE;
	    if (S_create_volume_dir(nc_volume, nc_images_dir,
				    CLIENT_DIR_PERMS) == FALSE) {
		return (FALSE);
	    }
	    if (S_create_shadow_file(shadow_path, shared_path, 
				     uid, gid, needspace) == FALSE) {
		syslog(LOG_INFO, "macNC: couldn't create %s, %m",
		       shadow_path);
		return (FALSE);
	    }
	}

	/* add the shadow file option */
	if (S_add_afppath_option(servip, options, nc_volume, 
				 nc_images_dir, S_shadow_name,
				 macNCtag_shared_system_shadow_file_e) 
	    == FALSE) {
	    return (FALSE);
	}
    }
    return (TRUE);
}

boolean_t
macNC_allocate(struct dhcp * reply, u_char * hostname, struct in_addr servip, 
	       int host_number, dhcpoa_t * options,
	       uid_t uid, u_char * afp_user, u_char * passwd)
{
    if (bootp_add_bootfile(NULL, hostname, S_default_bootfile, 
			   reply->dp_file) == FALSE) {
	syslog(LOG_INFO, "macNC: bootp_add_bootfile failed");
	return (FALSE);
    }

    if (dhcpoa_add(options, macNCtag_user_name_e, strlen(afp_user),
		   afp_user) != dhcpoa_success_e) {
	if (!quiet) {
	    syslog(LOG_INFO, 
		   "macNC: afp user name option add %s failed, %s",
		   afp_user, dhcpoa_err(options));
	}
	return (FALSE);
    }
    /* add the Mac OS machine name option */
    if (dhcpoa_add(options, macNCtag_MacOS_machine_name_e, 
		   strlen(hostname), hostname) != dhcpoa_success_e) {
	if (!quiet) {
	    syslog(LOG_INFO, 
		   "macNC: machine name option add client %s failed, %s",
		   hostname, dhcpoa_err(options));
	}
	return (FALSE);
    }
    {
	u_char	buf[16];
	int	buf_len = sizeof(buf);
	
	if (macNCopt_str_to_type(passwd, macNCtype_afp_password_e,
				 buf, &buf_len, NULL) == FALSE
	    || dhcpoa_add(options, macNCtag_password_e,
			  buf_len, buf) != dhcpoa_success_e) {
	    if (!quiet) {
		syslog(LOG_INFO, "macNC: failed add afp password for %d",
		       host_number);
	    }
	    return (FALSE);
	}
    }
    if (S_add_image_options(uid, S_admin_gid, servip, 
			    options, host_number, hostname) == FALSE) {
	if (!quiet) {
	    syslog(LOG_INFO, 
		   "macNC: S_add_image_options for %s failed", afp_user);
	}
    }
    return (TRUE);
}

void
macNC_unlink_shadow(int host_number, u_char * hostname)
{
    int			def_vol_index;
    int			i;
    u_char		nc_images_dir[PATH_MAX];
    nbspEntry_t *	nc_volume = NULL;
    struct stat		shadow_statb;
    u_char		shadow_path[PATH_MAX];
    int			vol_index;

    snprintf(nc_images_dir, sizeof(nc_images_dir),
	     "%s/%s", S_client_image_dir, hostname);

    def_vol_index = (host_number - 1) % nbspList_count(S_sharepoints);

    /* check all volumes for a client image directory starting at default */
    nc_volume = NULL;
    for (i = 0, vol_index = def_vol_index; i < nbspList_count(S_sharepoints);
	 i++) {
	nbspEntry_t * entry = nbspList_element(S_sharepoints, vol_index);
	if (S_stat_path_vol_file(shadow_path, entry, nc_images_dir, 
				 S_shadow_name, &shadow_statb) == 0) {
	    if (unlink(shadow_path) < 0) {
		if (verbose) {
		    syslog(LOG_INFO, 
			   "macNC: unlink(%s) failed, %m", shadow_path);
		}
	    }
	    return;
	}
	vol_index = (vol_index + 1) % nbspList_count(S_sharepoints);
    }
    return;
}
