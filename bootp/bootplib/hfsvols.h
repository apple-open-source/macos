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

#ifndef _S_HFSVOLS_H
#define _S_HFSVOLS_H

/*
 * hfsvols.h
 */

/*
 * Modification History:
 *
 * May 15, 1998	Dieter Siegmund (dieter@apple)
 * - created
 */

typedef struct {
    u_char *	name;
    u_char *	mounted_on;
    u_char *	mounted_from;
    long	device_id;
} hfsVol_t;

typedef void * hfsVolList_t;

hfsVol_t *		hfsVolList_entry(hfsVolList_t vols, int i);
int			hfsVolList_count(hfsVolList_t list);
void			hfsVolList_free(hfsVolList_t * list);
hfsVolList_t		hfsVolList_init();
void			hfsVolList_print(hfsVolList_t vols);
hfsVol_t *		hfsVolList_lookup(hfsVolList_t vols, u_char * name);

/*
 * HFS filesystem routines
 */
boolean_t		hfs_get_dirID(u_int32_t volumeID, 
				      u_char * path, u_int32_t * dirID_p);
int			hfs_set_file_size(int fd, off_t size);
#endif _S_HFSVOLS_H
