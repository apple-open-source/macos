
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
};

extern void
my_log(int priority, const char *message, ...);

static Boolean
myCFStringArrayToCStringArray(CFArrayRef arr, char * buffer, int * buffer_size,
			      int * ret_count)
{
    int		count = CFArrayGetCount(arr);
    int 	i;
    char *	offset = NULL;	
    int		space;
    char * *	strlist = NULL;

    space = count * sizeof(char *);
    if (buffer != NULL) {
	if (*buffer_size < space) {
	    /* not enough space for even the pointer list */
	    return (FALSE);
	}
	strlist = (char * *)buffer;
	offset = buffer + space; /* the start of the 1st string */
    }
    for (i = 0; i < count; i++) {
	CFIndex		len = 0;
	CFStringRef	str;

	str = CFArrayGetValueAtIndex(arr, i);
	if (buffer != NULL) {
	    len = *buffer_size - space;
	    if (len < 0) {
		return (FALSE);
	    }
	}
	CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)),
			 kCFStringEncodingASCII, '\0',
			 FALSE, offset, len - 1, &len);
	if (buffer != NULL) {
	    strlist[i] = offset;
	    offset[len] = '\0';
	    offset += len + 1;
	}
	space += len + 1;
    }
    *buffer_size = space;
    *ret_count = count;
    return (TRUE);
}

static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    if (CFStringGetCString(cfstr, str, len, kCFStringEncodingUTF8)) {
	return (TRUE);
    }
    *str = '\0';
    return (FALSE);
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
 * Function: parse_nfs_path
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
parse_nfs_path(char * path, struct in_addr * iaddr_p,
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

/*
 * Function: parse_http_path
 * Purpose:
 *   Parse a string of the form:
 *        "http://[user:password@]<IP | hostname>[:port]/<image_path>"
 *   into the given ip address, image_path, and option user/password and port
 * Notes:
 * - the passed in string is modified i.e. ':' is replaced by '\0'
 *
 * Examples:
 * http://17.203.12.194:8080/NetBootSP0/Jaguar/Jaguar.dmg
 * http://foo:bar@17.203.12.194/NetBootSP0/Jaguar/Jaguar.dmg
 */

static __inline__ boolean_t
parse_http_path(char * path, struct in_addr * iaddr_p,
		char * * username, char * * password, int * port,
		char * * image_path)
{
    char *	atchar;
    char *      colon;
    char *      slash;
    char *	start;

    *username = NULL;
    *password = NULL;
    *port = 0;

#define HTTP_URL_PREFIX		"http://"
#define HTTP_URL_PREFIX_LEN	7

    /* scheme */
    start = path;
    if (strncmp(HTTP_URL_PREFIX, start, HTTP_URL_PREFIX_LEN) != 0) {
	return (FALSE);
    }
    start += HTTP_URL_PREFIX_LEN;

    /* look for start of image path */
    slash = strchr(start, '/');
    if (slash == NULL) {
	return (FALSE);
    }
    *slash = '\0';
    *image_path = slash + 1;

    /* check for optional username:password@... */
    atchar = strchr(start, '@');
    if (atchar != NULL && atchar < slash) {
	*atchar = '\0';
	*username = start;
	*password = strsep(username, ":");
	if (*password == NULL) {
	    /* both username and password need to specified */
	    return (FALSE);
	}
	start = atchar + 1;
    }
    
    /* check for optional port in server_name_or_ip[:port] */
    colon = strchr(start, ':');
    if (colon) {
	*colon = '\0';
	*port = atoi(colon + 1);
    }

    /* if the server specification isn't an IP address, look it up by name */
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
    int			count;

    count = dynarray_count(dlist);
    for (i = 0; i < count; i++) {
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

static const char *
NBImageTypeStr(NBImageType type)
{
    switch (type) {
    case kNBImageTypeClassic:
	return "Classic";
    case kNBImageTypeNFS:
	return "NFS";
    case kNBImageTypeHTTP:
	return "HTTP";
    case kNBImageTypeBootFileOnly:
	return "BootFile";
    default:
	return "<unknown>";
    }
}

static void
dump_strlist(const char * * strlist, int count)
{
    int i;

    for (i = 0; i < count; i++) {
	printf("%s%s", (i != 0) ? ", " : "", strlist[i]);
    }
    return;
}

static void
NBImageEntry_print(NBImageEntryRef entry)
{
    printf("%-12s %-35.*s 0x%08x %-9s %-12s", 
	   entry->sharepoint.name,
	   entry->name_length, entry->name,
	   entry->image_id, 
	   NBImageTypeStr(entry->type),
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
    case kNBImageTypeHTTP:
	printf(" %-12s%s", entry->type_info.http.root_path,
	       (entry->type_info.http.indirect == TRUE)? " [indirect]" : "");
	break;
    default:
	break;
    }
    if (entry->sysids != NULL) {
	printf(" [ ");
	dump_strlist(entry->sysids, entry->sysids_count);
	printf(" ]");
    }
    if (entry->filter_only) {
	printf(" <filter>");
    }
    printf("\n");
    return;
}

static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key, boolean_t d)
{
    CFBooleanRef	b;
    boolean_t 		ret = d;

    b = CFDictionaryGetValue(plist, key);
    if (isA_CFBoolean(b) != NULL) {
	ret = CFBooleanGetValue(b);
    }
    return (ret);
}

static int
my_ptrstrcmp(const void * v1, const void * v2)
{
    const char * * s1 = (const char * *)v1;
    const char * * s2 = (const char * *)v2;

    return (strcmp(*s1, *s2));
}

static NBImageEntryRef
NBImageEntry_create(NBSPEntryRef sharepoint, char * dir_name,
		    char * dir_path, char * info_plist_path)
{
    u_int16_t		attr = 0;
    CFStringRef		bootfile;
    boolean_t		diskless;
    NBImageEntryRef	entry = NULL;
    char *		ent_bootfile = NULL;
    char *		ent_name = NULL;
    CFIndex		ent_name_len = 0;
    char *		ent_private = NULL;
    char *		ent_root_path = NULL;
    char *		ent_shared = NULL;
    char *		ent_tftp_path = NULL;
    boolean_t		filter_only;
    int32_t		idx_val = -1;
    CFNumberRef		idx;
    char *		image_file = NULL;
    boolean_t		image_is_default;
    boolean_t		indirect = FALSE;
    CFNumberRef		kind;
    int32_t		kind_val = -1;
    char *		mount_point = NULL;
    CFStringRef		name;
    char *		offset;
    char		path[PATH_MAX];
    CFPropertyListRef	plist;
    CFStringRef		private;
    CFStringRef		root_path;
    struct in_addr	server_ip;
    char *              server_password;
    int                 server_port;
    char *              server_username;
    CFStringRef		shared;
    int			string_space = 0;
    int			sysids_space = 0;
    CFArrayRef		sysids_prop;
    char		tmp[PATH_MAX];
    CFStringRef		type;
    NBImageType		type_val = kNBImageTypeNone;

    string_space = strlen(dir_name) + strlen(sharepoint->path) 
	+ strlen(sharepoint->name) + 3;
    plist = my_CFPropertyListCreateFromFile(info_plist_path);
    if (isA_CFDictionary(plist) == NULL) {
	goto failed;
    }
    if (S_get_plist_boolean(plist, kNetBootImageInfoIsEnabled, TRUE) == FALSE) {
	/* image is disabled */
	goto failed;
    }
    if (S_get_plist_boolean(plist, kNetBootImageInfoIsInstall, FALSE) == TRUE) {
	attr |= BSDP_IMAGE_ATTRIBUTES_INSTALL;
    }
    image_is_default = S_get_plist_boolean(plist, kNetBootImageInfoIsDefault,
					   FALSE);
    diskless = S_get_plist_boolean(plist, kNetBootImageInfoSupportsDiskless, 
				   FALSE);
    filter_only = S_get_plist_boolean(plist, kNetBootImageInfoFilterOnly,
				      FALSE);
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
	|| CFNumberGetValue(idx, kCFNumberSInt32Type, &idx_val) == FALSE
	|| idx_val <= 0 || idx_val > BSDP_IMAGE_INDEX_MAX) {
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
	diskless = TRUE;	/* Mac OS 9 requires diskless */
    }
    else if (CFEqual(type, kNetBootImageInfoTypeNFS)) {
	type_val = kNBImageTypeNFS;
	if (kind_val == -1) {
	    kind_val = bsdp_image_kind_MacOSX;
	}
    }
    else if (CFEqual(type, kNetBootImageInfoTypeHTTP)) {
	type_val = kNBImageTypeHTTP;
	if (kind_val == -1) {
	    kind_val = bsdp_image_kind_MacOSX;
	}
    }
    else if (CFEqual(type, kNetBootImageInfoTypeBootFileOnly)) {
	type_val = kNBImageTypeBootFileOnly;
	diskless = FALSE;
    }
    if (type_val == kNBImageTypeNone) {
	fprintf(stderr, "unrecognized Type property\n");
	goto failed;
    }
    if (kind_val == -1) {
	fprintf(stderr, "missing/unrecognized Kind value\n");
	goto failed;
    }
    if (kind_val == bsdp_image_kind_Diagnostics) {
	/* if FilterOnly was not set, set it to TRUE */
	if (CFDictionaryContainsKey(plist, 
				    kNetBootImageInfoFilterOnly) == FALSE) {
	    filter_only = TRUE;
	}
    }
    attr |= bsdp_image_attributes_from_kind(kind_val);

    /* bootfile */
    bootfile = CFDictionaryGetValue(plist, kNetBootImageInfoBootFile);
    if (isA_CFString(bootfile) == NULL) {
	fprintf(stderr, "missing/invalid BootFile property\n");
	goto failed;
    }
    if (cfstring_to_cstring(bootfile, tmp, sizeof(tmp)) == FALSE) {
	fprintf(stderr, "BootFile could not be converted\n");
	goto failed;
    }
    if (stat_file(dir_path, tmp) == FALSE) {
	fprintf(stderr, "BootFile does not exist\n");
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

    /* supported system ids */
    sysids_prop 
	= CFDictionaryGetValue(plist, 
			       kNetBootImageInfoEnabledSystemIdentifiers);
    if (sysids_prop != NULL) {
	int sysids_count;

	if (isA_CFArray(sysids_prop) == NULL) {
	    fprintf(stderr, "EnabledSystemIdentifiers isn't an array\n");
	    goto failed;
	}
	if (myCFStringArrayToCStringArray(sysids_prop, NULL, &sysids_space,
					  &sysids_count) == FALSE) {
	    fprintf(stderr, 
		    "Couldn't calculate EnabledSystemIdentifiers length\n");
	    goto failed;
	}
	string_space += sysids_space;
    }
    switch (type_val) {
    case kNBImageTypeClassic:
	/* must have Shared */
	shared = CFDictionaryGetValue(plist, kNetBootImageInfoSharedImage);
	if (isA_CFString(shared) == NULL) {
	    fprintf(stderr, "missing/invalid SharedImage property\n");
	    goto failed;
	}
	if (cfstring_to_cstring(shared, tmp, sizeof(tmp)) == FALSE) {
	    fprintf(stderr, "SharedImage could not be converted\n");
	    goto failed;
	}
	if (stat_file(dir_path, tmp) == FALSE) {
	    fprintf(stderr, "SharedImage does not exist\n");
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
	    if (cfstring_to_cstring(private, tmp, sizeof(tmp))
		&& stat_file(dir_path, tmp)) {
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
	if (cfstring_to_cstring(root_path, tmp, sizeof(tmp)) == FALSE) {
	    fprintf(stderr, "RootPath could not be converted\n");
	    goto failed;
	}
	if (stat_file(dir_path, tmp) == TRUE) {
	    ent_root_path = strdup(tmp);
	}
	else if (parse_nfs_path(tmp, &server_ip, &mount_point,
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
    case kNBImageTypeHTTP:
	/* must have RootPath */
	root_path = CFDictionaryGetValue(plist, kNetBootImageInfoRootPath);
	if (isA_CFString(root_path) == NULL) {
	    fprintf(stderr, "missing/invalid RootPath property\n");
	    goto failed;
	}
	if (cfstring_to_cstring(root_path, tmp, sizeof(tmp)) == FALSE) {
	    fprintf(stderr, "RootPath could not be converted\n");
	    goto failed;
	}
	if (stat_file(dir_path, tmp) == TRUE) {
	    ent_root_path = strdup(tmp);
	}
	else if (parse_http_path(tmp, &server_ip, &server_username,
				 &server_password, &server_port,
				 &image_file) == TRUE) {
	    if (server_username && server_password) {
		if (server_port != 0) {
		    snprintf(path, sizeof(path), "http://%s:%s@%s:%d/%s",
			     server_username, server_password, 
			     inet_ntoa(server_ip), 
			     server_port, image_file);
		}
		else {
		    snprintf(path, sizeof(path), "http://%s:%s@%s/%s",
			     server_username, server_password, 
			     inet_ntoa(server_ip),
			     image_file);
		}
	    } 
	    else {
		if (server_port != 0) {
		    snprintf(path, sizeof(path), "http://%s:%d/%s",
			     inet_ntoa(server_ip), server_port,
			     image_file);
		}
		else {
		    snprintf(path, sizeof(path), "http://%s/%s",
			     inet_ntoa(server_ip), image_file);
		}
	    }
	    indirect = TRUE;
	    ent_root_path = strdup(path);
	}
	if (ent_root_path == NULL) {
	    goto failed;
	}
	string_space += strlen(ent_root_path) + 1;
	break;
    case kNBImageTypeBootFileOnly:
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
    entry->diskless = diskless;
    entry->filter_only = filter_only;

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

    /* supported system ids */
    if (sysids_space > 0) {
	entry->sysids = (const char * *)offset;
	(void)myCFStringArrayToCStringArray(sysids_prop, offset, &sysids_space,
					    &entry->sysids_count);
	qsort(entry->sysids, entry->sysids_count, sizeof(char *),
	      my_ptrstrcmp);
	offset += sysids_space;
    }

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
    case kNBImageTypeHTTP:
	entry->type_info.http.root_path = offset;
	strcpy(entry->type_info.http.root_path, ent_root_path);
	offset += strlen(ent_root_path) + 1;
	entry->type_info.http.indirect = indirect;
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

boolean_t
NBImageEntry_supported_sysid(NBImageEntryRef entry, 
			     const char * sysid)
{
    if (entry->sysids == NULL) {
	return (TRUE);
    }
    return (bsearch(&sysid, entry->sysids, entry->sysids_count,
		    sizeof(char *), my_ptrstrcmp) != NULL);
}

boolean_t
NBImageEntry_attributes_match(NBImageEntryRef entry,
			      const u_int16_t * attrs_list, int n_attrs_list)
{
    u_int16_t	attrs;
    int		i;

    if (attrs_list == NULL) {
	return (!entry->filter_only);
    }
    attrs = bsdp_image_attributes(entry->image_id);
    for (i = 0; i < n_attrs_list; i++) {
	if (attrs_list[i] == attrs) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static void
NBImageList_add_default_entry(NBImageListRef image_list,
			      NBImageEntryRef entry)
{
    dynarray_t *	dlist = &image_list->list;
    int 		i;
    int			count;

    if (entry->sysids == NULL) {
	dynarray_insert(dlist, entry, 0);
	return;
    }
    count = dynarray_count(dlist);
    for (i = 0; i < count; i++) {
	NBImageEntryRef	scan = dynarray_element(dlist, i);

	if (scan->is_default == FALSE
	    || scan->sysids != NULL) {
	    dynarray_insert(dlist, entry, i);
	    return;
	}
    }
    dynarray_add(dlist, entry);
    return;
}

static void
NBImageList_add_entry(NBImageListRef image_list, NBImageEntryRef entry)
{
    NBImageEntryRef	scan;

    scan = NBImageList_elementWithID(image_list, entry->image_id);
    if (scan != NULL) {
	fprintf(stderr, 
		"Ignoring image with non-unique image index %d:\n",
		bsdp_image_index(entry->image_id));
	NBImageEntry_print(entry);
	free(entry);
	return;
    }
    if (entry->is_default) {
	NBImageList_add_default_entry(image_list, entry);
    }
    else {
	dynarray_add(&image_list->list, entry);
    }
    return;
}

static void
NBImageList_add_images(NBImageListRef image_list, NBSPEntryRef sharepoint)
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
	    NBImageList_add_entry(image_list, entry);
	}
    }
 done:
    if (dir_p)
	closedir(dir_p);
    return;
}

NBImageEntryRef 
NBImageList_default(NBImageListRef image_list, const char * sysid,
		    const u_int16_t * attrs, int n_attrs)
{
    int			count;
    dynarray_t *	dlist = &image_list->list;
    int			i;

    count = dynarray_count(dlist);
    for (i = 0; i < count; i++) {
	NBImageEntryRef	scan = dynarray_element(dlist, i);

	if (NBImageEntry_supported_sysid(scan, sysid)
	    && NBImageEntry_attributes_match(scan, attrs, n_attrs)) {
	    return (scan);
	}
    }
    return (NULL);
}

NBImageListRef
NBImageList_init(NBSPListRef sharepoints)
{
    int				count;
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

    count = NBSPList_count(sharepoints);
    for (i = 0; i < count; i++) {
	NBSPEntryRef	entry = NBSPList_element(sharepoints, i);

	NBImageList_add_images(image_list, entry);
    }
 done:
    if (image_list != NULL) {
	if (dynarray_count(&image_list->list) == 0) {
	    dynarray_free(&image_list->list);
	    free(image_list);
	    image_list = NULL;
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
    int			count;
    int			i;

    printf("%-12s %-35s %-10s %-9s %-12s Image(s)\n", "Sharepoint", "Name",
	   "Identifier", "Type", "BootFile");

    count = dynarray_count(&image_list->list);
    for (i = 0; i < count; i++) {
	NBImageEntryRef	entry;

	entry = (NBImageEntryRef)dynarray_element(&image_list->list, i);
	NBImageEntry_print(entry);
    }
    return;
}

#ifdef TEST_NBIMAGES
#if 0
#include <syslog.h>
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
#endif 0
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




