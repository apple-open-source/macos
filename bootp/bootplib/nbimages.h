
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
 * nbimages.h
 * - NetBoot image list routines
 */

#ifndef _S_NBIMAGES_H
#define _S_NBIMAGES_H

#include "nbsp.h"
#include "bsdp.h"

typedef enum {
    kNBImageTypeNone = 0,
    kNBImageTypeClassic,
    kNBImageTypeNFS,
    kNBImageTypeHTTP,
} NBImageType;

typedef union {
    struct {
	boolean_t	indirect;
	char *		root_path;
    } nfs;
    struct {
	char *		root_path;
    } http;
    struct {
	char *		shared;
	char *		private;
    } classic;
} NBImageTypeInfo;

typedef struct {
    NBSPEntry		sharepoint;
    char *		dir_name;	/* relative to sharepoint */
    char *		name;		/* not nul-terminated */
    int			name_length;
    bsdp_image_id_t	image_id;
    char *		bootfile;
    char *		tftp_path;
    bsdp_image_kind_t	kind;
    NBImageType		type;
    NBImageTypeInfo	type_info;
    boolean_t		is_default;
} NBImageEntry, * NBImageEntryRef;

struct NBImageList_s;

typedef struct NBImageList_s * NBImageListRef;

int		NBImageList_count(NBImageListRef list);
NBImageEntryRef	NBImageList_element(NBImageListRef list, int i);
NBImageEntryRef NBImageList_elementWithID(NBImageListRef list, bsdp_image_id_t);
NBImageListRef	NBImageList_init(NBSPListRef sharepoints);
void		NBImageList_free(NBImageListRef * list);
void		NBImageList_print(NBImageListRef images);
NBImageEntryRef NBImageList_default(NBImageListRef images);

#endif _S_NBIMAGES_H
