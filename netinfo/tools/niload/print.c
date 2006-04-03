/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import <stdio.h>
#import <netinfo/ni.h>

void
print_property(ni_property *p)
{
	int i, len;
	ni_namelist *nl;

	if (p == NULL) return;

	printf("%s:", p->nip_name);
	nl = &(p->nip_val);

	if (nl == NULL) len = 0;
	else len = nl->ni_namelist_len;
	for (i = 0; i < len; i++) printf(" %s", nl->ni_namelist_val[i]);
	printf("\n");
}

void
print_proplist(ni_proplist *pl)
{
	int i, len;

	if (pl == NULL) return;

	len = pl->ni_proplist_len;
	for (i = 0; i < len; i++) print_property(&pl->ni_proplist_val[i]);
}		
