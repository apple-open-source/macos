
#ifndef _S_NETINFO_H
#define _S_NETINFO_H
/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * netinfo.h - Header file for netinfo routines.
 */
#import <string.h>
#import <unistd.h>
#import <stdlib.h>
#import	<mach/boolean.h>
#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/if_ether.h>
#import <netinet/in.h>
#import <netinfo/ni.h>
#import <netinfo/ni_util.h>

/*
 * Constants
 */

/* Important directories */
#define	NIDIR_MACHINES		"/machines"
#define NIDIR_USERS		"/users"
#define NIDIR_GROUPS		"/groups"

/* Important properties */
#define	NIPROP_NAME		"name"

#define NIPROP_UID		"uid"
#define NIPROP_GID		"gid"
#define NIPROP_PASSWD		"passwd"
#define NIPROP_SHELL		"shell"
#define NIPROP_REALNAME		"realname"

#define	NIPROP_BOOTFILE		"bootfile"
#define NIPROP_DHCP_RELEASED	"released"
#define NIPROP_DHCP_DECLINED	"declined"
#define NIPROP_DHCP_LEASE	"lease"
#define	NIPROP_ENADDR		"en_address"
#define	NIPROP_HWADDR		"hw_address"
#define NIPROP_IDENTIFIER	"identifier"
#define	NIPROP_IPADDR		"ip_address"
#define NIPROP_NETBOOT_AFP_USER	"afp_user"
#define NIPROP_NETBOOT_ARCH	"arch"
#define NIPROP_NETBOOT_IMAGE_ID	"image_id"
#define NIPROP_NETBOOT_IMAGE_INDEX "image_index"
#define NIPROP_NETBOOT_IMAGE_KIND "image_kind"
#define NIPROP_NETBOOT_IMAGE_IS_INSTALL "image_is_install"
#define NIPROP_NETBOOT_NUMBER	"number"
#define NIPROP_NETBOOT_SYSID	"sysid"
#define NIPROP_NETBOOT_LAST_BOOT_TIME	"last_boot_time"
#define NIPROP_NETBOOT_BOUND	"bound"
#define NIPROP_SERVES		"serves"
#define NIPROP__CREATOR		"_creator"

/*
 * Prototypes
 */
void		ni_proplist_dump(ni_proplist * pl);

boolean_t	ni_get_checksum(void * h, unsigned long * checksum);

void		ni_set_prop(ni_proplist * pl_p, ni_name prop, ni_name value, 
			    boolean_t * modified);
void		ni_delete_prop(ni_proplist * pl_p, ni_name prop, 
			       boolean_t * modified);
/*
 * Function: ni_proplist_insertprop
 * Purpose:
 *	Add a property with a given value to a property list at the
 *	specified index.
 */
static __inline__ void
ni_proplist_insertprop(
		       ni_proplist	*proplist,
		       ni_name	key,
		       ni_name	value,
		       ni_index	where
		       )
{
	ni_property	prop;

	NI_INIT(&prop);
	prop.nip_name = key;
	if (value) {
		ni_namelist_insert(&prop.nip_val, value, 0);
	}
	ni_proplist_insert(proplist, prop, where);
	ni_namelist_free(&prop.nip_val);
}

/*
 * Function: ni_proplist_addprop
 * Purpose:
 *	Add a property with a given value to a property list.
 */
static __inline__ void
ni_proplist_addprop(
		    ni_proplist	*proplist,
		    ni_name	key,
		    ni_name	value
		    )
{
    	ni_proplist_insertprop(proplist, key, value, proplist->nipl_len);
	return;
}

/*
 * Function: ni_proplist_addprops
 *
 * Purpose:
 *	Add a property with the given values to a property list.
 */
static __inline__ void
ni_proplist_addprops(
		    ni_proplist	*proplist,
		    ni_name	key,
		    ni_name *	values,
		    int		count
		    )
{
	ni_property	prop;
	int		i;

	NI_INIT(&prop);
	prop.nip_name = key;
	for (i = count - 1; i >= 0; i--) {
	    ni_namelist_insert(&prop.nip_val, values[i], 0);
	}
	ni_proplist_insert(proplist, prop, proplist->nipl_len);
	ni_namelist_free(&prop.nip_val);
}

/*
 * Function: ni_create_path
 * Purpose:
 *   Create directories for each component of the given path if they
 *   don't exist.
 */

static __inline__ boolean_t
ni_create_path(void * d, u_char * dirname, ni_id * dir_p)
{
    boolean_t	done = FALSE;
    u_char	path[128];
    u_char *	scan;

    if (ni_pathsearch(d, dir_p, dirname) == NI_OK) { /* it already exists */
	return (TRUE);
    }
    if (ni_root(d, dir_p) != NI_OK) {
	return (FALSE);
    }

    path[0] = '\0';
    scan = dirname;
    while (done == FALSE) {
	ni_id		child_id;
	u_char		component[32];
	u_char * 	next_sep;

	if (scan == NULL || *scan != '/')
	    return (FALSE);
	scan++;
	next_sep = strchr(scan, '/');
	if (next_sep == 0) {
	    done = TRUE;
	    next_sep = dirname + strlen(dirname);
	}
	strncpy(component, scan , next_sep - scan);
	component[next_sep - scan] = '\0';
	strcat(path, "/");
	strcat(path, component);
	if (ni_pathsearch(d, &child_id, path) != NI_OK) {
	    ni_proplist pl;
	    /* create it */
	    NI_INIT(&pl);
	    ni_proplist_addprop(&pl, "name", component);
	    if (ni_create(d, dir_p, pl, &child_id, NI_INDEX_NULL) != NI_OK) {
		ni_proplist_free(&pl);
		return (FALSE);
	    }
	    ni_proplist_free(&pl);
	}
	*dir_p = child_id;
	scan = next_sep;
    }
    return (TRUE);
}

static __inline__ ni_namelist *
ni_nlforprop(ni_proplist * pl, ni_name name)
{
    int i;

    for (i = 0; i < pl->nipl_len; i++) {
	ni_property * p = &(pl->nipl_val[i]);
	if (strcmp(name, p->nip_name) == 0) {
	    return (&p->nip_val);
	}
    }
    return (NULL);
}

static __inline__ const ni_name
ni_valforprop(ni_proplist * pl, ni_name name)
{
    ni_namelist * nl_p = ni_nlforprop(pl, name);
    if (nl_p == NULL || nl_p->ninl_len == 0)
	return (NULL);
    return (nl_p->ninl_val[0]);
}

static __inline__ void
ni_proplist_append(ni_proplist * proplist, ni_proplist * new_pl)
{
    int i;

    for (i = 0; i < new_pl->nipl_len; i++) {
	ni_proplist_insert(proplist, new_pl->nipl_val[i], proplist->nipl_len);
    }
    return;
}

static __inline__ int
ni_nlvalindex(ni_namelist * nl_p, ni_name value)
{
    int i;
    if (nl_p) {
	for (i = 0; i < nl_p->ninl_len; i++) {
	    if (strcmp(nl_p->ninl_val[i], value) == 0)
		return (i);
	}
    }
    return (-1);
}

static __inline__ int
ni_indexforprop(ni_proplist * pl, ni_name name, ni_name value)
{
    ni_namelist * nl_p = ni_nlforprop(pl, name);
    if (nl_p == NULL)
	return (-1);
    return (ni_nlvalindex(nl_p, value));

}

#endif _S_NETINFO_H
