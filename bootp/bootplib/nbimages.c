
/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
 * nbimages.c
 * - NetBoot image list routines
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/syslimits.h>
#include <dirent.h>

#include "dynarray.h"
#include "nbimages.h"
#include "cfutil.h"
#include "NetBootServer.h"
#include "NetBootImageInfo.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <SystemConfiguration/SCValidation.h>

struct NBImageList_s {
    dynarray_t		list;
    NBImageEntryRef	default_entry;
};

extern void
my_log(int priority, const char *message, ...);

static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex		l;
    CFIndex		n;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    n = CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			 0, FALSE, str, len, &l);
    str[l] = '\0';
    return (l);
}

/*
 * Function: find_colon
 * Purpose:
 *   Find the next unescaped instance of the colon character.
 */
static __inline__ char *
find_colon(char * str)
{
    char * start = str;
    char * colon;
    
    while ((colon = strchr(start, ':')) != NULL) {
	if (colon == start) {
	    break;
	}
	if (colon[-1] != '\\')
	    break;
	start = colon;
    }
    return (colon);
}

/*
 * Function: parse_netboot_path
 * Purpose:
 *   Parse a string of the form:
 *        "<IP | hostname>:<mount>[:<image_path>]"
 *   into the given ip address, mount point, and optionally, image_path.
 * Notes:
 * - the passed in string is modified i.e. ':' is replaced by '\0'
 * - literal colons must be escaped with a backslash
 *
 * Examples:
 * 17.202.42.112:/Library/NetBoot/NetBootSP0:Jaguar/Jaguar.dmg
 * siegdi6:/Volumes/Foo\:/Library/NetBoot/NetBootSP0:Jaguar/Jaguar.dmg
 */
static __inline__ boolean_t
parse_netboot_path(char * path, struct in_addr * iaddr_p,
		   char * * mount_dir, char * * image_path)
{
    char *	start;
    char *	colon;

    /* IP address */
    start = path;
    colon = strchr(start, ':');
    if (colon == NULL) {
	return (FALSE);
    }
    *colon = '\0';
    if (inet_aton(start, iaddr_p) != 1) {
	struct in_addr * * 	addr;
	struct hostent * 	ent;

	ent = gethostbyname(start);
	if (ent == NULL) {
	    return (FALSE);
	}
	addr = (struct in_addr * *)ent->h_addr_list;
	if (*addr == NULL)
	    return (FALSE);
	*iaddr_p = **addr;
    }

    /* mount point */
    start = colon + 1;
    colon = find_colon(start);
    *mount_dir = start;
    if (colon == NULL) {
	*image_path = NULL;
    }
    else {
	/* image path */
	*colon = '\0';
	start = colon + 1;
	(void)find_colon(start);
	*image_path = start;
    }
    return (TRUE);
}

int
NBImageList_count(NBImageListRef image_list)
{
    dynarray_t *	dlist = &image_list->list;

    return (dynarray_count(dlist));
}

NBImageEntryRef
NBImageList_element(NBImageListRef image_list, int i)
{
    dynarray_t *	dlist = &image_list->list;

    return (dynarray_element(dlist, i));
}


NBImageEntryRef
NBImageList_elementWithID(NBImageListRef image_list, bsdp_image_id_t image_id)
{
    dynarray_t *	dlist = &image_list->list;
    int 		i;

    for (i = 0; i < dynarray_count(dlist); i++) {
	NBImageEntryRef	entry = dynarray_element(dlist, i);

	if (image_id == entry->image_id) {
	    return (entry);
	}
    }
    return (NULL);
}

void
NBImageList_free(NBImageListRef * l)
{
    NBImageListRef	image_list;

    if (l == NULL) {
	return;
    }
    image_list = *l;
    if (image_list == NULL) {
	return;
    }
    dynarray_free(&image_list->list);
    free(image_list);
    *l = NULL;
    return;
}

static boolean_t
stat_file(char * dir, char * file)
{
    char		path[PATH_MAX];
    struct stat		sb;

    snprintf(path, sizeof(path), "%s/%s", dir, file);
    if (stat(path, &sb) < 0) {
	fprintf(stderr, "stat %s failed, %s\n",
		path, strerror(errno));
	return (FALSE);
    }
    if ((sb.st_mode & S_IFREG) == 0) {
	fprintf(stderr, "%s is not a file\n", path);
	return (FALSE);
    }
    return (TRUE);
}

static void
NBImageEntry_print(NBImageEntryRef entry)
{
    printf("%-12s %-25.*s 0x%08x %-8s %-12s", 
	   entry->sharepoint.name,
	   entry->name_length, entry->name,
	   entry->image_id, 
	   (entry->type == kNBImageTypeClassic) ? "Classic" : "NFS",
	   entry->bootfile);
    switch (entry->type) {
    case kNBImageTypeClassic:
	printf(" %-12s", entry->type_info.classic.shared);
	if (entry->type_info.classic.private != NULL) {
	    printf(" %-12s", entry->type_info.classic.private);
	}
	break;
    case kNBImageTypeNFS:
	printf(" %-12s%s", entry->type_info.nfs.root_path,
	       (entry->type_info.nfs.indirect == TRUE)? " [indirect]" : "");
	break;
    default:
	break;
    }
    printf("\n");
    return;
}

static NBImageEntryRef
NBImageEntry_create(NBSPEntryRef sharepoint, char * dir_name,
		    char * dir_path, char * info_plist_path)
{
    u_int16_t		attr = 0;
    CFStringRef		bootfile;
    NBImageEntryRef	entry = NULL;
    char *		ent_bootfile = NULL;
    char *		ent_name = NULL;
    CFIndex		ent_name_len = 0;
    char *		ent_private = NULL;
    char *		ent_root_path = NULL;
    char *		ent_tftp_path = NULL;
    char *		ent_shared = NULL;
    u_int16_t		idx_val;
    CFNumberRef		idx;
    char *		image_file = NULL;
    boolean_t		image_is_default = FALSE;
    boolean_t		indirect = FALSE;
    CFBooleanRef	is_default;
    CFBooleanRef	is_enabled;
    CFBooleanRef	is_install;
    CFNumberRef		kind;
    int			kind_val = -1;
    char *		mount_point = NULL;
    CFStringRef		name;
    char *		offset;
    char		path[PATH_MAX];
    CFPropertyListRef	plist;
    CFStringRef		private;
    CFStringRef		root_path;
    struct in_addr	server_ip;
    CFStringRef		shared;
    int			string_space = 0;
    char		tmp[PATH_MAX];
    CFStringRef		type;
    NBImageType		type_val = kNBImageTypeNone;

    string_space = strlen(dir_name) + strlen(sharepoint->path) 
	+ strlen(sharepoint->name) + 3;
    plist = my_CFPropertyListCreateFromFile(info_plist_path);
    if (isA_CFDictionary(plist) == NULL) {
	goto failed;
    }
    is_enabled = CFDictionaryGetValue(plist, kNetBootImageInfoIsEnabled);
    if (is_enabled != NULL) {
	if (isA_CFBoolean(is_enabled) == NULL
	    || CFBooleanGetValue(is_enabled) == FALSE) {
	    /* image is disabled */
	    goto failed;
	}
    }
    is_install = CFDictionaryGetValue(plist, kNetBootImageInfoIsInstall);
    if (isA_CFBoolean(is_install) != NULL
	&& CFBooleanGetValue(is_install) == TRUE) {
	attr |= BSDP_IMAGE_ATTRIBUTES_INSTALL;
    }
    is_default = CFDictionaryGetValue(plist, kNetBootImageInfoIsDefault);
    if (isA_CFBoolean(is_default) != NULL
	&& CFBooleanGetValue(is_default) == TRUE) {
	image_is_default = TRUE;
    }
    name = CFDictionaryGetValue(plist, kNetBootImageInfoName);
    if (isA_CFString(name) == NULL) {
	fprintf(stderr, "missing/invalid Name property\n");
	goto failed;
    }
    ent_name_len = CFStringGetLength(name);
    CFStringGetBytes(name, CFRangeMake(0, ent_name_len),
		     kCFStringEncodingUTF8, '?', TRUE,
		     tmp, BSDP_IMAGE_NAME_MAX,
		     &ent_name_len);
    if (ent_name_len == 0) {
	printf("zero name length\n");
	goto failed;
    }
    ent_name = malloc(ent_name_len);
    if (ent_name == NULL) {
	goto failed;
    }
    bcopy(tmp, ent_name, ent_name_len);
    string_space += ent_name_len;

    idx = CFDictionaryGetValue(plist, kNetBootImageInfoIndex);
    if (isA_CFNumber(idx) == NULL
	|| CFNumberGetValue(idx, kCFNumberSInt16Type, &idx_val) == FALSE
	|| idx_val == 0) {
	fprintf(stderr, "missing/invalid Index property\n");
	goto failed;
    }
    kind = CFDictionaryGetValue(plist, kNetBootImageInfoKind);
    if (isA_CFNumber(kind) != NULL) {
	if (CFNumberGetValue(kind, kCFNumberSInt32Type, &kind_val) == FALSE
	    || kind_val < 0 || kind_val > BSDP_IMAGE_ATTRIBUTES_KIND_MAX) {
	    kind_val = -1;
	}
    }

    type = CFDictionaryGetValue(plist, kNetBootImageInfoType);
    if (isA_CFString(type) == NULL) {
	fprintf(stderr, "missing/invalid Type property\n");
	goto failed;
    }

    if (CFEqual(type, kNetBootImageInfoTypeClassic)) {
	type_val = kNBImageTypeClassic;
	if (kind_val == -1) {
	    kind_val = bsdp_image_kind_MacOS9;
	}
    }
    else if (CFEqual(type, kNetBootImageInfoTypeNFS)) {
	type_val = kNBImageTypeNFS;
	if (kind_val == -1) {
	    kind_val = bsdp_image_kind_MacOSX;
	}
    }
    if (type_val == kNBImageTypeNone) {
	fprintf(stderr, "unrecognized Type property\n");
	goto failed;
    }

    if (kind_val == -1) {
	fprintf(stderr, "unrecognized Kind value\n");
	goto failed;
    }
    attr |= bsdp_image_attributes_from_kind(kind_val);

    /* bootfile */
    bootfile = CFDictionaryGetValue(plist, kNetBootImageInfoBootFile);
    if (isA_CFString(bootfile) == NULL) {
	fprintf(stderr, "missing/invalid BootFile property\n");
	goto failed;
    }
    cfstring_to_cstring(bootfile, tmp, sizeof(tmp));
    if (stat_file(dir_path, tmp) == FALSE) {
	goto failed;
    }
    ent_bootfile = strdup(tmp);
    if (ent_bootfile == NULL) {
	goto failed;
    }
    string_space += strlen(ent_bootfile) + 1;

    /* tftp_path */
    snprintf(tmp, sizeof(tmp),
	     NETBOOT_TFTP_DIRECTORY "/%s/%s/%s", sharepoint->name, 
	     dir_name, ent_bootfile);
    ent_tftp_path = strdup(tmp);
    string_space += strlen(ent_tftp_path) + 1;

    switch (type_val) {
    case kNBImageTypeClassic:
	/* must have Shared */
	shared = CFDictionaryGetValue(plist, kNetBootImageInfoSharedImage);
	if (isA_CFString(shared) == NULL) {
	    fprintf(stderr, "missing/invalid SharedImage property\n");
	    goto failed;
	}
	cfstring_to_cstring(shared, tmp, sizeof(tmp));
	if (stat_file(dir_path, tmp) == FALSE) {
	    goto failed;
	}
	ent_shared = strdup(tmp);
	if (ent_shared == NULL) {
	    goto failed;
	}
	string_space += strlen(ent_shared) + 1;

	/* may have Private */
	private 
	    = isA_CFString(CFDictionaryGetValue(plist, 
						kNetBootImageInfoPrivateImage));
	if (private != NULL) {
	    cfstring_to_cstring(private, tmp, sizeof(tmp));
	    if (stat_file(dir_path, tmp) == TRUE) {
		ent_private = strdup(tmp);
		if (ent_private == NULL) {
		    goto failed;
		}
		string_space += strlen(ent_private) + 1;
	    }
	}
	break;
    case kNBImageTypeNFS:
	/* must have RootPath */
	root_path = CFDictionaryGetValue(plist, kNetBootImageInfoRootPath);
	if (isA_CFString(root_path) == NULL) {
	    fprintf(stderr, "missing/invalid RootPath property\n");
	    goto failed;
	}
	cfstring_to_cstring(root_path, tmp, sizeof(tmp));
	if (stat_file(dir_path, tmp) == TRUE) {
	    ent_root_path = strdup(tmp);
	}
	else if (parse_netboot_path(tmp, &server_ip, &mount_point,
			       &image_file) == TRUE) {
	    if (image_file) {
		snprintf(path, sizeof(path), "nfs:%s:%s:%s",
			 inet_ntoa(server_ip), mount_point,
			 image_file);
	    }
	    else {
		snprintf(path, sizeof(path), "nfs:%s:%s",
			 inet_ntoa(server_ip), mount_point);
	    }
	    indirect = TRUE;
	    ent_root_path = strdup(path);
	}
	if (ent_root_path == NULL) {
	    goto failed;
	}
	string_space += strlen(ent_root_path) + 1;
	break;
    default:
	break;
    }

    entry = (NBImageEntryRef)malloc(sizeof(*entry) + string_space);
    if (entry == NULL) {
	goto failed;
    }
    bzero(entry, sizeof(*entry));
    entry->image_id = bsdp_image_id_make(idx_val, attr);
    entry->type = type_val;
    entry->is_default = image_is_default;

    offset = (char *)(entry + 1);

    /* sharepoint */
    entry->sharepoint.path = offset;
    strcpy(entry->sharepoint.path, sharepoint->path);
    offset += strlen(sharepoint->path) + 1;
    entry->sharepoint.name = offset;
    strcpy(entry->sharepoint.name, sharepoint->name);
    offset += strlen(sharepoint->name) + 1;

    /* dir_name */
    entry->dir_name = offset;
    strcpy(entry->dir_name, dir_name);
    offset += strlen(dir_name) + 1;

    /* name */
    entry->name = offset;
    entry->name_length = ent_name_len;
    bcopy(ent_name, entry->name, ent_name_len);
    offset += ent_name_len; /* no nul termination */

    /* bootfile */
    entry->bootfile = offset;
    strcpy(entry->bootfile, ent_bootfile);
    offset += strlen(ent_bootfile) + 1;

    /* tftp_path */
    entry->tftp_path = offset;
    strcpy(entry->tftp_path, ent_tftp_path);
    offset += strlen(ent_tftp_path) + 1;

    switch (type_val) {
    case kNBImageTypeClassic:
	entry->type_info.classic.shared = offset;
	strcpy(entry->type_info.classic.shared, ent_shared);
	offset += strlen(ent_shared) + 1;
	if (ent_private != NULL) {
	    entry->type_info.classic.private = offset;
	    strcpy(entry->type_info.classic.private, ent_private);
	    offset += strlen(ent_private) + 1;
	}
	break;
    case kNBImageTypeNFS:
	entry->type_info.nfs.root_path = offset;
	strcpy(entry->type_info.nfs.root_path, ent_root_path);
	offset += strlen(ent_root_path) + 1;
	entry->type_info.nfs.indirect = indirect;
	break;
    default:
	break;
    }

 failed:
    if (ent_name != NULL) {
	free(ent_name);
    }
    if (ent_bootfile != NULL) {
	free(ent_bootfile);
    }
    if (ent_tftp_path != NULL) {
	free(ent_tftp_path);
    }
    if (ent_shared != NULL) {
	free(ent_shared);
    }
    if (ent_private != NULL) {
	free(ent_private);
    }
    if (ent_root_path != NULL) {
	free(ent_root_path);
    }
    my_CFRelease(&plist);
    return (entry);
}

static void
add_images(NBImageListRef image_list, NBSPEntryRef sharepoint)
{
    char		dir[PATH_MAX];
    DIR *		dir_p;
    NBImageEntryRef	entry;
    char		info_path[PATH_MAX];
    int			suffix_len;
    struct dirent *	scan;
    struct stat		sb;

    dir_p = opendir(sharepoint->path);
    if (dir_p == NULL) {
	goto done;
    }
    suffix_len = strlen(NETBOOT_IMAGE_SUFFIX);
    while ((scan = readdir(dir_p)) != NULL) {
	int	entry_len = strlen(scan->d_name);

	if (entry_len < suffix_len
	  || strcmp(scan->d_name + entry_len - suffix_len,
		    NETBOOT_IMAGE_SUFFIX) != 0) {
	    continue;
	}
	snprintf(dir, sizeof(dir), "%s/%s", 
		 sharepoint->path, scan->d_name);
	if (stat(dir, &sb) != 0 || (sb.st_mode & S_IFDIR) == 0) {
	    continue;
	}
	snprintf(info_path, sizeof(info_path), 
		 "%s/" NETBOOT_IMAGE_INFO_PLIST, dir);
	if (stat(info_path, &sb) != 0 || (sb.st_mode & S_IFREG) == 0) {
	    continue;
	}
	entry = NBImageEntry_create(sharepoint, scan->d_name, dir, info_path);
	if (entry != NULL) {
	    boolean_t		add_it = TRUE;
	    NBImageEntryRef	scan;

	    scan = NBImageList_elementWithID(image_list, entry->image_id);
	    if (scan != NULL) {
		fprintf(stderr, 
			"Ignoring image with non-unique image index %d:\n",
			bsdp_image_index(entry->image_id));
		NBImageEntry_print(entry);
		add_it = FALSE;
	    }
	    else if (entry->is_default == TRUE) {
		if (image_list->default_entry != NULL) {
		    fprintf(stderr, 
			    "More than one image claims to be default:\n");
		    NBImageEntry_print(entry);
		}
		else {
		    image_list->default_entry = entry;
		}
	    }
	    if (add_it) {
		dynarray_add(&image_list->list, entry);
	    }
	    else {
		free(entry);
	    }
	}
    }
 done:
    if (dir_p)
	closedir(dir_p);
    return;
}

NBImageEntryRef 
NBImageList_default(NBImageListRef image_list)
{
    return (image_list->default_entry);
}

NBImageListRef
NBImageList_init(NBSPListRef sharepoints)
{
    int				i;
    NBImageListRef		image_list = NULL;
    boolean_t			needs_free = FALSE;

    if (sharepoints == NULL) {
	needs_free = TRUE;
	sharepoints = NBSPList_init(NETBOOT_SHAREPOINT_LINK);
	if (sharepoints == NULL) {
	    goto done;
	}
    }
    image_list = (NBImageListRef)malloc(sizeof(*image_list));
    if (image_list == NULL) {
	goto done;
    }
    bzero(image_list, sizeof(*image_list));
    dynarray_init(&image_list->list, free, NULL);

    for (i = 0; i < NBSPList_count(sharepoints); i++) {
	NBSPEntryRef	entry = NBSPList_element(sharepoints, i);

	add_images(image_list, entry);
    }
 done:
    if (image_list != NULL) {
	if (dynarray_count(&image_list->list) == 0) {
	    dynarray_free(&image_list->list);
	    free(image_list);
	    image_list = NULL;
	}
	else {
	    /* no default specified, pick the first one */
	    if (image_list->default_entry == NULL) {
		image_list->default_entry = (NBImageEntryRef)
		    dynarray_element(&image_list->list, 0);
		image_list->default_entry->is_default = TRUE;
	    }
	}
    }
    if (sharepoints != NULL && needs_free) {
	NBSPList_free(&sharepoints);
    }
    return (image_list);
}

void
NBImageList_print(NBImageListRef image_list)
{
    int			i;

    printf("%-12s %-25s %-10s %-8s %-12s Image(s)\n", "Sharepoint", "Name",
	   "Identifier", "Type", "BootFile");

    for (i = 0; i < dynarray_count(&image_list->list); i++) {
	NBImageEntryRef	entry;

	entry = (NBImageEntryRef)dynarray_element(&image_list->list, i);
	NBImageEntry_print(entry);
    }
    return;
}

#ifdef TEST_NBIMAGES
void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (G_verbose == FALSE)
	    return;
	priority = LOG_INFO;
    }

    va_start(ap, message);
    vsyslog(priority, message, ap);
    return;
}

int
main()
{
    NBImageListRef	images = NBImageList_init(NULL);

    if (images != NULL) {
	NBImageList_print(images);
	NBImageList_free(&images);
    }
    
    exit(0);
}

#endif TEST_NBIMAGES




