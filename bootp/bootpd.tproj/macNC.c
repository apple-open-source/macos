/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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
 * macNC.c
 * - macNC boot server
 * - supports Mac OS 9 AKA Classic netboot clients
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

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <pwd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <arpa/inet.h>
#include <mach/boolean.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <sys/attr.h>

#include "dhcp.h"
#include "netinfo.h"
#include "rfc_options.h"
#include "subnets.h"
#include "interfaces.h"
#include "bootpd.h"
#include "bsdpd.h"
#include "macnc_options.h"
#include "macNC.h"
#include "host_identifier.h"
#include "nbsp.h"
#include "nbimages.h"
#include "NetBootServer.h"
#include "util.h"

/* 
 * Function: timestamp_syslog
 *
 * Purpose:
 *   Log a timestamped event message to the syslog.
 */
static void
S_timestamp_syslog(const char * msg)
{
    static struct timeval	tvp = {0,0};
    struct timeval		tv;

    gettimeofday(&tv, 0);
    if (tvp.tv_sec) {
	struct timeval result;
      
	timeval_subtract(tv, tvp, &result);
	syslog(LOG_INFO, "%d.%06d (%d.%06d): %s", 
	       tv.tv_sec, tv.tv_usec, result.tv_sec, result.tv_usec, msg);
    }
    else 
	syslog(LOG_INFO, "%d.%06d (%d.%06d): %s", 
	       tv.tv_sec, tv.tv_usec, 0, 0, msg);
    tvp = tv;
}



static __inline__ void
S_timestamp(const char * msg)
{
    if (verbose)
	S_timestamp_syslog(msg);
}

static boolean_t
S_set_dimg_ddsk(const char * path)
{
    struct attrlist 	attrspec;
    struct {
	char	type_creator[8];
	u_long	pad[6];
    } finder =  {
	{ 'd', 'i', 'm', 'g', 'd', 'd', 's', 'k' },
	{ 0, 0, 0, 0, 0, 0 }
    };

    bzero(&attrspec, sizeof(attrspec));
    attrspec.bitmapcount	= ATTR_BIT_MAP_COUNT;
    attrspec.commonattr		= ATTR_CMN_FNDRINFO;
    if (setattrlist(path, &attrspec, &finder, sizeof(finder), 0)) {
	return (FALSE);
    }
    return (TRUE);
}

static boolean_t
set_privs_no_stat(const char * path, struct stat * sb_p, uid_t uid, gid_t gid,
		  mode_t mode, boolean_t unlock)
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
    }
    else if (unlock) {
	if (sb_p->st_flags & UF_IMMUTABLE) {
	    if (chflags(path, 0) < 0)
		return (FALSE);
	}
    }
    return (TRUE);
}

boolean_t
set_privs(const char * path, struct stat * sb_p, uid_t uid, gid_t gid,
	  mode_t mode, boolean_t unlock)
{
    if (stat(path, sb_p) != 0) {
	return (FALSE);
    }
    return (set_privs_no_stat(path, sb_p, uid, gid, mode, unlock));
}

/*
 * Function: S_set_uid_gid
 *
 * Purpose:
 *   Given a path to a file, make the owner of both the
 *   enclosing directory and the file itself to user/group uid/gid.
 */
static int
S_set_uid_gid(const char * file, uid_t uid, gid_t gid)
{
    char 	dir[PATH_MAX];
    char *	last_slash = strrchr(file, '/');

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

/*
 * Function: S_get_volpath
 *
 * Purpose:
 *   Format a volume pathname given a volume, directory and file name.
 */
static void
S_get_volpath(char * path, const NBSPEntry * entry, const char * dir, 
	      const char * file)
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
S_create_volume_dir(NBSPEntry * entry, char * dirname, mode_t mode)
{
    char 		path[PATH_MAX];

    S_get_volpath(path, entry, dirname, NULL);
    if (create_path(path, mode) < 0) {
	my_log(LOG_INFO, "macNC: create_volume_dir: create_path(%s)"
	       " failed, %m",  path);
	return (FALSE);
    }
    (void)chmod(path, mode);
    return (TRUE);
}

/*
 * Function: set_file_size
 * 
 * Purpose:
 *   Set a file to be a certain length.
 */
static int
set_file_size(int fd, off_t size)
{
#ifdef F_SETSIZE
    fcntl(fd, F_SETSIZE, &size);
#endif F_SETSIZE
    return (ftruncate(fd, size));
}

/*
 * Function: S_create_shadow_file
 *
 * Purpose:
 *   Create a new empty file with the right permissions, and optionally,
 *   set to type dimg/ddsk.
 */
static boolean_t
S_create_shadow_file(const char * shadow_path, uid_t uid, gid_t gid, 
		     unsigned long long size, boolean_t set_dimg)
{
    int 		fd;

    S_set_uid_gid(shadow_path, ROOT_UID, 0);

    fd = open(shadow_path, O_CREAT | O_TRUNC | O_WRONLY, CLIENT_FILE_PERMS);
    if (fd < 0) {
	my_log(LOG_INFO, "macNC: couldn't create file '%s': %m", shadow_path);
	return (FALSE);
    }
    if (set_file_size(fd, size)) {
	my_log(LOG_INFO, "macNC: set_file_size '%s' failed: %m",
	       shadow_path);
	goto err;
    }

    if (set_dimg) {
	if (S_set_dimg_ddsk(shadow_path) == FALSE) {
	    my_log(LOG_INFO, "macNC: set type/creator '%s' failed, %m", 
		   shadow_path);
	    goto err;
	}
    }

    fchmod(fd, CLIENT_FILE_PERMS);
    close(fd);

    /* correct the owner of the path */
    if (S_set_uid_gid(shadow_path, uid, gid)) {
	my_log(LOG_INFO, "macNC: setuidgid '%s' to %ld,%ld failed: %m", 
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
		     NBSPEntry * entry, const char * dir, const char * file,
		     int tag)
{
    char		buf[DHCP_OPTION_SIZE_MAX];
    dhcpo_err_str_t	err;
    int			len;
    char		path[PATH_MAX];

    if (dir && *dir)
	snprintf(path, sizeof(path), "%s/%s", dir, file);
    else {
	snprintf(path, sizeof(path), "%s", file);
    }
	
    len = sizeof(buf);
    if (macNCopt_encodeAFPPath(servip, AFP_PORT_NUMBER, entry->name,
			       AFP_DIRID_NULL, AFP_PATHTYPE_LONG,
			       path, '/', buf, &len, &err) == FALSE) {
	my_log(LOG_INFO, "macNC: couldn't encode %s:%s, %s", entry->name, path,
	       err.str);
	return (FALSE);
    }
    if (dhcpoa_add(options, tag, len, buf) != dhcpoa_success_e) {
	my_log(LOG_INFO, "macNC: couldn't add option %d failed: %s", tag,
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
S_stat_path_vol_file(char * path, NBSPEntry * entry, 
		     const char * dir, const char * file,
		     struct stat * sb_p)
{
    S_get_volpath(path, entry, dir, file);
    return (stat(path, sb_p));
}


static boolean_t
S_freespace(const char * path, unsigned long long * size)
{
    struct statfs 	fsb;

    if (statfs(path, &fsb) != 0) {
	my_log(LOG_INFO, "macNC: statfs on '%s' failed %m", path);
	return (FALSE);
    }
    *size = ((unsigned long long)fsb.f_bavail) 	
	* ((unsigned long long)fsb.f_bsize);
    if (debug)
        printf("%s %qu x %u = %qu bytes\n", path,
	       fsb.f_bavail, fsb.f_bsize, *size);
    return (TRUE);
}

static NBSPEntry *
S_find_volume_with_space(unsigned long long needspace, int def_vol_index,
			 boolean_t need_hfs)
{
    unsigned long long	freespace;
    int 		i;
    NBSPEntry *	entry = NULL;
    char		path[PATH_MAX];
    int			vol_index;

    for (i = 0, vol_index = def_vol_index; i < NBSPList_count(G_client_sharepoints);
	 i++) {
	NBSPEntry * shp = NBSPList_element(G_client_sharepoints, vol_index);

	if (need_hfs == FALSE || shp->is_hfs == TRUE) {
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
	}
	vol_index = (vol_index + 1) % NBSPList_count(G_client_sharepoints);
    }
    return (entry);
}

static boolean_t
S_remove_shadow(const char * shadow_path, NBSPEntry * entry, const char * dir)
{
    char path[PATH_MAX];

    /* remove the shadow file */
    S_set_uid_gid(shadow_path, ROOT_UID, 0);
    unlink(shadow_path);
    S_get_volpath(path, entry, dir, NULL);
    /* and its directory */
    if (rmdir(path)) {
	char new_path[PATH_MAX];
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

NBSPEntry *
macNC_allocate_shadow(const char * machine_name, int host_number, 
		      uid_t uid, gid_t gid, const char * shadow_name)
{
    int			def_vol_index;
    char		dir_path[PATH_MAX];
    struct stat		dir_statb;
    int			i;
    char		nc_images_dir[PATH_MAX];
    NBSPEntry *		nc_volume = NULL;
    unsigned long long	needspace;
    char		shadow_path[PATH_MAX];
    int			vol_index;

    if (G_client_sharepoints == NULL) {
	syslog(LOG_NOTICE, "macNC_allocate_shadow: no client sharepoints");
	return (NULL);
    }

    strncpy(nc_images_dir, machine_name, sizeof(nc_images_dir));

    /* attempt to round-robin images across multiple volumes */
    def_vol_index = (host_number - 1) % NBSPList_count(G_client_sharepoints);

    /* check all volumes for a client image directory starting at default */
    nc_volume = NULL;
    for (i = 0, vol_index = def_vol_index; i < NBSPList_count(G_client_sharepoints);
	 i++) {
	NBSPEntry * entry = NBSPList_element(G_client_sharepoints, vol_index);
	if (S_stat_path_vol_file(dir_path, entry,
				 nc_images_dir, NULL, &dir_statb) == 0) {
	    nc_volume = entry;
	    break;
	}
	vol_index = (vol_index + 1) % NBSPList_count(G_client_sharepoints);
    }
#define ONE_MEG		(1024UL * 1024UL)
    needspace = ((unsigned long long)G_shadow_size_meg) * ONE_MEG;
    if (nc_volume != NULL) {
	struct stat		sb_shadow;
	boolean_t		set_file_size = FALSE;
	boolean_t		set_owner_perms = FALSE;
	    
	S_get_volpath(shadow_path, nc_volume, nc_images_dir, 
		      shadow_name);
	if (stat(shadow_path, &sb_shadow) == 0) { /* shadow exists */
	    S_timestamp("shadow file exists");
	    if (debug)
		printf("shadow %qu need %qu\n", 
		       sb_shadow.st_size, needspace);
	    
	    if (sb_shadow.st_uid != uid
		|| sb_shadow.st_gid != gid
		|| (sb_shadow.st_mode & ACCESSPERMS) != CLIENT_FILE_PERMS)
		set_owner_perms = TRUE;
	    
	    if (sb_shadow.st_size < needspace) {
		unsigned long long  	difference;
		unsigned long long	freespace = 0;
		
		set_file_size = TRUE;
		S_timestamp("shadow file needs to be grown");
		/* check for enough space */
		(void)S_freespace(shadow_path, &freespace);
		difference = (needspace - sb_shadow.st_size);
		if (freespace < difference) {
		    my_log(LOG_INFO, "macNC: device full, "
			   "attempting to relocate %s",
			   shadow_path);
		    /* blow away the shadow */
		    if (S_remove_shadow(shadow_path, nc_volume,
					nc_images_dir) == FALSE) {
			my_log(LOG_INFO, "macNC: couldn't remove"
			       " shadow %s, %m", shadow_path);
			goto failed;
		    }
		    /* start fresh */
		    nc_volume = NULL;
		}
	    }
	}
	else {
	    /* start fresh */
	    if (S_remove_shadow(shadow_path, nc_volume, nc_images_dir)
		== FALSE) {
		my_log(LOG_INFO, "macNC: couldn't remove"
		       " shadow %s, %m", shadow_path);
		goto failed;
	    }
	    nc_volume = NULL;
	}
	if (nc_volume != NULL) {
	    if (set_file_size) {
		S_timestamp("setting shadow file size");
		if (S_create_shadow_file(shadow_path, uid, gid, needspace,
					 FALSE) == FALSE) {
		    my_log(LOG_INFO, "macNC: couldn't create %s, %m",
			   shadow_path);
		    goto failed;
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
	nc_volume = S_find_volume_with_space(needspace, def_vol_index, FALSE);
	if (nc_volume == NULL) {
	    if (G_disk_space_warned == FALSE)
		my_log(LOG_INFO, "macNC: can't create client image: "
		       "OUT OF DISK SPACE");
	    G_disk_space_warned = TRUE; /* don't keep complaining */
	    goto failed;
	}
	S_get_volpath(shadow_path, nc_volume, nc_images_dir, 
		      shadow_name);
	G_disk_space_warned = FALSE;
	if (S_create_volume_dir(nc_volume, nc_images_dir,
				CLIENT_DIR_PERMS) == FALSE) {
	    goto failed;
	}
	if (S_create_shadow_file(shadow_path, uid, gid, needspace,
				 FALSE) == FALSE) {
	    my_log(LOG_INFO, "macNC: couldn't create %s, %m",
		   shadow_path);
	    goto failed;
	}
    }
    return (nc_volume);

 failed:
    return (NULL);
}


/* 
 * Function: S_add_image_options
 * 
 * Purpose:
 *   Create/initialize image for client, format the paths into the
 *   response options.
 */
static boolean_t
S_add_image_options(NBImageEntryRef image_entry,
		    uid_t uid, gid_t gid, struct in_addr servip, 
		    dhcpoa_t * options, int host_number, 
		    const char * afp_hostname)
{
    int			def_vol_index;
    char		dir_path[PATH_MAX];
    struct stat		dir_statb;
    int			i;
    char		nc_images_dir[PATH_MAX];
    NBSPEntry *		nc_volume = NULL;
    char		path[PATH_MAX];
    struct stat		statb;
    int			vol_index;

    /* make sure the bootfile exists and the permissions are correct */
    snprintf(path, sizeof(path), "%s/%s/%s",
	     image_entry->sharepoint->path,
	     image_entry->dir_name,
	     image_entry->bootfile);
    if (set_privs(path, &statb, ROOT_UID, G_admin_gid,
		  SHARED_FILE_PERMS, FALSE) == FALSE) {
	syslog(LOG_INFO, "macNC: '%s' does not exist", path);
	return (FALSE);
    }

    snprintf(nc_images_dir, sizeof(nc_images_dir),
	     "%s", afp_hostname);

    /* attempt to round-robin images across multiple volumes */
    def_vol_index = (host_number - 1) % NBSPList_count(G_client_sharepoints);

    /* check all volumes for a client image directory starting at default */
    nc_volume = NULL;
    for (i = 0, vol_index = def_vol_index; i < NBSPList_count(G_client_sharepoints);
	 i++) {
	NBSPEntry * entry = NBSPList_element(G_client_sharepoints, vol_index);
	if (entry->is_hfs == TRUE
	    && S_stat_path_vol_file(dir_path, entry,
				    nc_images_dir, NULL, &dir_statb) == 0) {
	    nc_volume = entry;
	    break;
	}
	vol_index = (vol_index + 1) % NBSPList_count(G_client_sharepoints);
    }

    /* if the client has its own private copy of the image file, use it */
    if (nc_volume != NULL
	&& S_stat_path_vol_file(path, nc_volume, nc_images_dir, 
				image_entry->type_info.classic.shared, 
				&statb) == 0) {
	/* set the image file perms */
	if (set_privs_no_stat(path, &statb, uid, gid, CLIENT_FILE_PERMS, 
			      TRUE) == FALSE) {
	    my_log(LOG_INFO, "macNC: couldn't set permissions on path %s: %m",
		   path);
	    return (FALSE);
	}
	/* set the client dir perms */
	if (set_privs_no_stat(dir_path, &dir_statb, uid, gid, 
			      CLIENT_DIR_PERMS, TRUE) == FALSE) {
	    my_log(LOG_INFO, "macNC: couldn't set permissions on path %s: %m",
		   dir_path);
	    return (FALSE);
	}
	if (S_add_afppath_option(servip, options, nc_volume, nc_images_dir,
				 image_entry->type_info.classic.shared,
				 macNCtag_shared_system_file_e) == FALSE) {
	    return (FALSE);
	}
	/* does the client have its own Private image? */
	if (image_entry->type_info.classic.private != NULL) {
	    if (S_stat_path_vol_file(path, nc_volume, nc_images_dir,
				     image_entry->type_info.classic.private, 
				     &statb) == 0) {
		if (set_privs_no_stat(path, &statb, uid, gid, 
				      CLIENT_FILE_PERMS, TRUE) == FALSE) {
		    my_log(LOG_INFO, 
			   "macNC: couldn't set permissions on path %s: %m", 
			   path);
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
					 nc_images_dir, 
					 image_entry->type_info.classic.private,
					 macNCtag_page_file_e) == FALSE){
		    return (FALSE);
		}
	    }
	}
    }
    else { /* client gets shadow file(s) */
	unsigned long long	needspace;
	char 			private_path[PATH_MAX];
	char 			shadow_path[PATH_MAX];
	char 			shared_path[PATH_MAX];

	snprintf(shared_path, sizeof(shared_path),
		 "%s/%s/%s",
		 image_entry->sharepoint->path,
		 image_entry->dir_name,
		 image_entry->type_info.classic.shared);
	/* set the shared system image permissions */
	if (set_privs(shared_path, &statb, ROOT_UID, G_admin_gid,
		      SHARED_FILE_PERMS, FALSE) == FALSE) {
	    syslog(LOG_INFO, "macNC: '%s' does not exist", shared_path);
	    return (FALSE);
	}

	/* add the shared system image option */
	if (S_add_afppath_option(servip, options, 
				 image_entry->sharepoint,
				 image_entry->dir_name, 
				 image_entry->type_info.classic.shared,
				 macNCtag_shared_system_file_e) == FALSE) {
	    return (FALSE);
	}
	if (image_entry->type_info.classic.private != NULL) {
	    /* check for the private system image, set its permissions */
	    snprintf(private_path, sizeof(private_path),
		     "%s/%s/%s",
		     image_entry->sharepoint->path,
		     image_entry->dir_name,
		     image_entry->type_info.classic.private);
	    if (set_privs(private_path, &statb, ROOT_UID, G_admin_gid,
			  SHARED_FILE_PERMS, FALSE) == TRUE) {
		/* add the private image option */
		if (S_add_afppath_option(servip, options, 
					 image_entry->sharepoint,
					 image_entry->dir_name, 
					 image_entry->type_info.classic.private,
					 macNCtag_private_system_file_e) 
		    == FALSE) {
		    return (FALSE);
		}
	    }
	}

#define ONE_MEG		(1024UL * 1024UL)
	needspace = ((unsigned long long)G_shadow_size_meg) * ONE_MEG;
	if (nc_volume != NULL) {
	    struct stat		sb_shadow;
	    boolean_t		set_file_size = FALSE;
	    boolean_t		set_owner_perms = FALSE;
	    
	    S_get_volpath(shadow_path, nc_volume, nc_images_dir, 
			  kNetBootShadowName);
	    if (stat(shadow_path, &sb_shadow) == 0) { /* shadow exists */
		S_timestamp("shadow file exists");
		if (debug)
		    printf("shadow %qu need %qu\n", 
			   sb_shadow.st_size, needspace);

		if (sb_shadow.st_uid != uid
		    || sb_shadow.st_gid != gid
		    || (sb_shadow.st_mode & ACCESSPERMS) != CLIENT_FILE_PERMS)
		    set_owner_perms = TRUE;

		if (sb_shadow.st_size < needspace) {
		    unsigned long long  	difference;
		    unsigned long long	freespace = 0;
			
		    set_file_size = TRUE;
		    S_timestamp("shadow file needs to be grown");
		    /* check for enough space */
		    (void)S_freespace(shadow_path, &freespace);
		    difference = (needspace - sb_shadow.st_size);
		    if (freespace < difference) {
			my_log(LOG_INFO, "macNC: device full, "
			       "attempting to relocate %s",
			       shadow_path);
			/* blow away the shadow */
			if (S_remove_shadow(shadow_path, nc_volume,
					    nc_images_dir) == FALSE) {
			    my_log(LOG_INFO, "macNC: couldn't remove"
				   " shadow %s, %m", shadow_path);
			    return (FALSE);
			}
			/* start fresh */
			nc_volume = NULL;
		    }
		}
	    }
	    else {
		/* start fresh */
		if (S_remove_shadow(shadow_path, nc_volume, nc_images_dir)
		    == FALSE) {
		    my_log(LOG_INFO, "macNC: couldn't remove"
			   " shadow %s, %m", shadow_path);
		    return (FALSE);
		}
		nc_volume = NULL;
	    }
	    if (nc_volume != NULL) {
		if (set_file_size) {
		    S_timestamp("setting shadow file size");
		    if (S_create_shadow_file(shadow_path, uid, gid, needspace,
					     TRUE) 
			== FALSE) {
			my_log(LOG_INFO, "macNC: couldn't create %s, %m",
			       shadow_path);
			return (FALSE);
		    }
		    S_timestamp("shadow file size set");
		}
		else {
		    if (set_owner_perms) {
			S_timestamp("setting shadow file perms/owner");
			chmod(shadow_path, CLIENT_FILE_PERMS);
			S_set_uid_gid(shadow_path, uid, gid);
		    }
		    S_set_dimg_ddsk(shadow_path);
		    S_timestamp("shadow file perms/owner set");
		}
	    }
	}
	if (nc_volume == NULL) { /* locate the client's image dir */
	    nc_volume = S_find_volume_with_space(needspace, def_vol_index,
						 TRUE);
	    if (nc_volume == NULL) {
		if (G_disk_space_warned == FALSE)
		    my_log(LOG_INFO, "macNC: can't create client image: "
			   "OUT OF DISK SPACE");
		G_disk_space_warned = TRUE; /* don't keep complaining */
		return (FALSE);
	    }
	    S_get_volpath(shadow_path, nc_volume, nc_images_dir, 
			  kNetBootShadowName);
	    G_disk_space_warned = FALSE;
	    if (S_create_volume_dir(nc_volume, nc_images_dir,
				    CLIENT_DIR_PERMS) == FALSE) {
		return (FALSE);
	    }
	    if (S_create_shadow_file(shadow_path, uid, gid, needspace,
				     TRUE) == FALSE) {
		my_log(LOG_INFO, "macNC: couldn't create %s, %m",
		       shadow_path);
		return (FALSE);
	    }
	}

	/* add the shadow file option */
	if (S_add_afppath_option(servip, options, nc_volume, 
				 nc_images_dir, kNetBootShadowName,
				 macNCtag_shared_system_shadow_file_e) 
	    == FALSE) {
	    return (FALSE);
	}
    }
    return (TRUE);
}

boolean_t
macNC_allocate(NBImageEntryRef image_entry,
	       struct dhcp * reply, const char * hostname, 
	       struct in_addr servip, int host_number, dhcpoa_t * options,
	       uid_t uid, const char * afp_user, const char * passwd)
{
    if (G_client_sharepoints == NULL) {
	syslog(LOG_NOTICE, "macNC_allocate: no client sharepoints");
	return (FALSE);
    }

    if (dhcpoa_add(options, macNCtag_user_name_e, strlen(afp_user),
		   afp_user) != dhcpoa_success_e) {
	my_log(LOG_INFO, 
	       "macNC: afp user name option add %s failed, %s",
	       afp_user, dhcpoa_err(options));
	return (FALSE);
    }
    /* add the Mac OS machine name option */
    if (dhcpoa_add(options, macNCtag_MacOS_machine_name_e, 
		   strlen(hostname), hostname) != dhcpoa_success_e) {
	my_log(LOG_INFO, 
	       "macNC: machine name option add client %s failed, %s",
	       hostname, dhcpoa_err(options));
	return (FALSE);
    }
    {
	char	buf[16];
	int	buf_len = sizeof(buf);
	
	if (macNCopt_str_to_type(passwd, macNCtype_afp_password_e,
				 buf, &buf_len, NULL) == FALSE
	    || dhcpoa_add(options, macNCtag_password_e,
			  buf_len, buf) != dhcpoa_success_e) {
	    my_log(LOG_INFO, "macNC: failed add afp password for %d",
		   host_number);
	    return (FALSE);
	}
    }
    if (S_add_image_options(image_entry, uid, G_admin_gid, servip, 
			    options, host_number, hostname) == FALSE) {
	my_log(LOG_INFO, 
	       "macNC: S_add_image_options for %s failed", afp_user);
	return (FALSE);
    }
    return (TRUE);
}

void
macNC_unlink_shadow(int host_number, const char * hostname)
{
    int			def_vol_index;
    int			i;
    char		nc_images_dir[PATH_MAX];
    NBSPEntry *	nc_volume = NULL;
    struct stat		shadow_statb;
    char		shadow_path[PATH_MAX];
    int			vol_index;

    if (G_client_sharepoints == NULL) {
	return;
    }
    snprintf(nc_images_dir, sizeof(nc_images_dir),
	     "%s", hostname);

    def_vol_index = (host_number - 1) % NBSPList_count(G_client_sharepoints);

    /* check all volumes for a client image directory starting at default */
    nc_volume = NULL;
    for (i = 0, vol_index = def_vol_index; 
	 i < NBSPList_count(G_client_sharepoints); i++) {
	NBSPEntry * entry = NBSPList_element(G_client_sharepoints, vol_index);
	if (S_stat_path_vol_file(shadow_path, entry, nc_images_dir, 
				 kNetBootShadowName, &shadow_statb) == 0) {
	    if (unlink(shadow_path) < 0) {
		my_log(LOG_DEBUG, 
		       "macNC: unlink(%s) failed, %m", shadow_path);
	    }
	    return;
	}
	vol_index = (vol_index + 1) % NBSPList_count(G_client_sharepoints);
    }
    return;
}
