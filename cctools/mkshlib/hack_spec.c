/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <stdio.h>
#include <mach-o/reloc.h>
#include "libgcc.h"
#include "hack_libgcc.h"
#include "stuff/bytesex.h"
#include "mkshlib.h"
#include "stuff/bool.h"
#include "stuff/allocate.h"

#undef DEBUG

/*
 * load_objects_from_specfile() parses the specfile and then loads the object
 * information for all the objects that are not old libgcc objects.
 */
void
load_objects_from_specfile(
void)
{
    unsigned long i;

	minor_version = 1;
	parse_spec();

	objects.names = reallocate(objects.names,
			sizeof(char *) * (objects.nobjects + nobject_list));
	objects.filelists = reallocate(objects.filelists,
			sizeof(char *) * (objects.nobjects + nobject_list));
	objects.libgcc = reallocate(objects.libgcc,
			sizeof(enum bool) * (objects.nobjects + nobject_list));

	for(i = 0; i < nobject_list; i++){
	    objects.names[objects.nobjects] = object_list[i]->name;
	    objects.filelists[objects.nobjects] = NULL;
	    objects.libgcc[objects.nobjects] = object_list[i]->libgcc_state;
	    objects.nobjects++;
	}
}
