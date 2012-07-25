/*
 * Copyright (c) 1999 Apple Inc. All rights reserved.
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
 * hfsVols.c
 * - implements a table of hfs volume entries with creation/lookup functions
 * - the volume entry maps between the hfs volume name and its mount point
 */

/*
 * Modification History:
 *
 * May 15, 1998	Dieter Siegmund (dieter@apple)
 * - created
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <mach/boolean.h>
#include <string.h>
#include <sys/attr.h>

#include "hfsvols.h"
#include "afp.h"
#include "dynarray.h"

typedef struct {
    long		length;
    long		device_id;
    u_long              signature;
    attrreference_t     volume_name;
    u_char              extra[512];
} volumeInfo_t;

static boolean_t
volumeInfo_get(u_char * path, volumeInfo_t * volinfo)
{
    struct attrlist     attrspec;

    bzero(volinfo, sizeof(*volinfo));
    attrspec.bitmapcount        = ATTR_BIT_MAP_COUNT;
    attrspec.reserved           = 0;
    attrspec.commonattr         = ATTR_CMN_DEVID;
    attrspec.volattr            = ATTR_VOL_SIGNATURE | ATTR_VOL_INFO | ATTR_VOL_NAME;
    attrspec.dirattr            = 0;
    attrspec.fileattr           = 0;
    attrspec.forkattr           = 0;

    if (getattrlist(path, &attrspec, volinfo, sizeof(*volinfo), 0)) {
#ifdef DEBUG
        perror("getattrlist");
#endif /* DEBUG */
        return (FALSE);
    }
    return (TRUE);
}


/* Signatures used to differentiate between HFS and HFS Plus volumes */
enum {
    kSignatureHFS                 = 0x4244,	/* 'BD' in ASCII */
    kSignatureHFSPlus             = 0x482b,	/* 'H+' in ASCII */
};

static u_char *
S_get_volume_name(volumeInfo_t * volinfo, int * len_p)
{
    u_char *	source_name;

    source_name = ((u_char *)&volinfo->volume_name) 
	+ volinfo->volume_name.attr_dataoffset;
    *len_p = volinfo->volume_name.attr_length;
    return (source_name);
}

static __inline__ void
print_fsstat_list(struct statfs * stat_p, int number)
{
    int i;

    for (i = 0; i < number; i++) {
	struct statfs * p = stat_p + i;
	printf("%s (%x %x) on %s from %s\n", p->f_fstypename, 
	       p->f_fsid.val[0], p->f_fsid.val[1], p->f_mntonname, 
	       p->f_mntfromname);
    }
}

static __inline__ struct statfs *
get_fsstat_list(int * number)
{
    int n;
    struct statfs * stat_p;

    n = getfsstat(NULL, 0, MNT_NOWAIT);
    if (n <= 0)
	return (NULL);

    stat_p = (struct statfs *)malloc(n * sizeof(*stat_p));
    if (stat_p == NULL)
	return (NULL);

    if (getfsstat(stat_p, n * sizeof(*stat_p), MNT_NOWAIT) <= 0) {
	free(stat_p);
	return (NULL);
    }
    *number = n;
    return (stat_p);
}

static __inline__ struct statfs *
fsstat_lookup(struct statfs * list_p, int n, dev_t dev)
{
    struct statfs * scan;
    int i;

    for (i = 0, scan = list_p; i < n; i++, scan++) {
	if (scan->f_fsid.val[0] == dev)
	    return (scan);
    }
    return (NULL);
}

static void
hfsVol_print(hfsVol_t * entry)
{
    printf("%s: mounted on %s from %s, device_id 0x%x (%ld)\n",
	   entry->name, entry->mounted_on, entry->mounted_from,
	   (int)entry->device_id, entry->device_id);
    return;
}

hfsVol_t *
hfsVolList_entry(hfsVolList_t vols, int i)
{
    dynarray_t *	list = (dynarray_t *)vols;

    return (dynarray_element(list, i));
}


int
hfsVolList_count(hfsVolList_t vols)
{
    dynarray_t *	list = (dynarray_t *)vols;
    
    return (dynarray_count(list));
}
void
hfsVolList_print(hfsVolList_t vols)
{
    dynarray_t *	list = (dynarray_t *)vols;
    int 		i;

    printf("There are %d HFS+ volume(s) on this computer\n", 
	   dynarray_count(list));
    for (i = 0; i < dynarray_count(list); i++) {
	hfsVol_t * entry = (hfsVol_t *)dynarray_element(list, i);
	hfsVol_print(entry);
    }
    return;
}

void
hfsVolList_free(hfsVolList_t * l)
{
    dynarray_t * list;
    if (l == NULL)
	return;
    list = *((dynarray_t * *)l);
    if (list == NULL)
	return;
    dynarray_free(list);
    free(list);
    *l = NULL;
    return;
}

hfsVolList_t
hfsVolList_init()
{
    dynarray_t *		list = NULL;			
    int				i;
    struct statfs * 		stat_p;
    int				stat_number;

    stat_p = get_fsstat_list(&stat_number);
    if (stat_p == NULL || stat_number == 0)
	goto err;

    for (i = 0; i < stat_number; i++) {
	hfsVol_t *		entry;
	struct statfs * 	p = stat_p + i;
	volumeInfo_t		volinfo;
	int			mounted_on_len = 0;
	int			mounted_from_len = 0;
	u_char *		name = NULL;
	int			name_len = 0;

	if (strcmp(p->f_fstypename, "hfs"))
	    continue;
	if (volumeInfo_get(p->f_mntonname, &volinfo) == FALSE)
	    continue;
	if (volinfo.signature != kSignatureHFSPlus)
	    continue;
	if (list == NULL) {
	    list = (dynarray_t *)malloc(sizeof(*list));
	    if (list == NULL) {
		goto err;
	    }
	    bzero(list, sizeof(*list));
	    dynarray_init(list, free, NULL);
	}
	mounted_on_len = strlen(p->f_mntonname);
	mounted_from_len = strlen(p->f_mntfromname);
	name = S_get_volume_name(&volinfo, &name_len);
	entry = malloc(sizeof(*entry) + mounted_on_len + mounted_from_len
		       + name_len + 3);
	if (entry == NULL)
	    continue;
	bzero(entry, sizeof(*entry));
	entry->device_id = volinfo.device_id;
	entry->mounted_on = (char *)(entry + 1);
	strncpy(entry->mounted_on, p->f_mntonname, mounted_on_len);
	entry->mounted_on[mounted_on_len] = '\0';

	entry->mounted_from = entry->mounted_on + mounted_on_len + 1;
	strncpy(entry->mounted_from, p->f_mntfromname, mounted_from_len);
	entry->mounted_from[mounted_from_len] = '\0';

	entry->name = entry->mounted_from + mounted_from_len + 1;
	strncpy(entry->name, name, name_len);
	entry->name[name_len] = '\0';
	dynarray_add(list, entry);
    }
 err:
    if (stat_p) {
	free(stat_p);
    }
    return ((hfsVolList_t)list);
}

/*
 * Function: hfs_set_file_size
 * 
 * Purpose:
 *   Set a file to be a certain length.
 */
int
hfs_set_file_size(int fd, off_t size)
{
#ifdef F_SETSIZE
    fcntl(fd, F_SETSIZE, &size);
#endif /* F_SETSIZE */
    return (ftruncate(fd, size));
}

#ifdef TEST_HFSVOLS
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int
main(int argc, u_char * argv[])
{
    int number;
    hfsVolList_t vlist;
    int ret;

    vlist = hfsVolList_init();
    if (vlist)
	hfsVolList_print(vlist);
    exit(0);
}
#endif /* TEST_HFSVOLS */

