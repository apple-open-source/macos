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

#import "LUDictionaryExtras.h"
#import <stdlib.h>
#import <NetInfo/nilib2.h>

@implementation LUDictionary (LUDictionaryExtras)

- (ni_proplist *)niProplist
{
	int i, j;
	ni_proplist *pl;

	pl = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(pl);

	for (i = 0; i < count; i++)
	{
		nipl_createprop(pl, prop[i].key);

		for (j = 0; j < prop[i].len; j++)
		{
			nipl_appendprop(pl, prop[i].key, prop[i].val[j]);
		}
	}

	return pl;
}

@end
