/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* outform.c	manages a list of output formats, and associates
 * 		them with their relevant drivers. Also has a
 * 		routine to find the correct driver given a name
 * 		for it
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */

#include <stdio.h>
#include <string.h>
#include "outform.h"

static struct ofmt *drivers[MAX_OUTPUT_FORMATS];
static int ndrivers = 0;

struct ofmt *ofmt_find(char *name)     /* find driver */
{
    int i;

    for (i=0; i<ndrivers; i++)
	if (!strcmp(name,drivers[i]->shortname))
	    return drivers[i];

    return NULL;
}

void ofmt_list(struct ofmt *deffmt, FILE *fp)
{
    int i;
    for (i=0; i<ndrivers; i++)
	fprintf(fp, "  %c %-7s%s\n",
		drivers[i] == deffmt ? '*' : ' ',
		drivers[i]->shortname,
		drivers[i]->fullname);
}

void ofmt_register (struct ofmt *info) {
    drivers[ndrivers++] = info;
}
