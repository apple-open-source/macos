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

#include <NetInfo/dsfilter.h>
#include <NetInfo/dsassertion.h>
#include <NetInfo/dsrecord.h>
#include <stdlib.h>

dsfilter *dsfilter_new_assert(dsassertion *a)
{
	dsfilter *f;
	
	if (a == NULL) return NULL;
	
	f = (dsfilter *)malloc(sizeof(dsfilter));
	f->assert = dsassertion_retain(a);
	f->op = DSF_OP_ASSERT;
	f->count = 0;
	f->retain = 1;
	
	return f;
}

dsfilter *dsfilter_new_composite(u_int32_t op)
{
	dsfilter *f;
	
	f = (dsfilter *)malloc(sizeof(dsfilter));
	f->assert = NULL;
	f->op = op;
	f->count = 0;
	f->retain = 1;
	
	return f;
}

dsfilter *dsfilter_new_and(void)
{
	return dsfilter_new_composite(DSF_OP_AND);
}

dsfilter *dsfilter_new_or(void)
{
	return dsfilter_new_composite(DSF_OP_OR);
}

dsfilter *dsfilter_new_not(void)
{
	return dsfilter_new_composite(DSF_OP_NOT);
}

dsfilter *dsfilter_append_filter(dsfilter *f, dsfilter *x)
{
	if (f == NULL) return NULL;
	if (x == NULL) return f;

	if (f->count == 0)
		f->filter = (dsfilter **)malloc(sizeof(dsfilter *));
	else
		f->filter = (dsfilter **)realloc(f->filter, (f->count + 1) * sizeof(dsfilter *));

	f->filter[f->count] = dsfilter_retain(x);
	f->count++;
	
	return f;
}

dsfilter *dsfilter_append_assertion(dsfilter *f, dsassertion *a)
{
	if (f == NULL) return NULL;
	if (a == NULL) return f;

	if (f->count == 0)
		f->filter = (dsfilter **)malloc(sizeof(dsfilter *));
	else
		f->filter = (dsfilter **)realloc(f->filter, (f->count + 1) * sizeof(dsfilter *));

	f->filter[f->count] = dsfilter_new_assert(a);
	return f;
}

dsfilter *dsfilter_retain(dsfilter *f)
{
	if (f == NULL) return NULL;
	f->retain++;
	return f;
}

void dsfilter_release(dsfilter *f)
{
	u_int32_t i;

	if (f == NULL) return;

	f->retain--;
	if (f->retain > 0) return;

	if (f->assert != NULL) dsassertion_release(f->assert);
	for (i = 0; i < f->count; i++) dsfilter_release(f->filter[i]);
	if (f->count != 0) free(f->filter);

	free(f);
}


Logic3 dsfilter_test(dsfilter *f, dsrecord *r)
{
	u_int32_t i, hasUndef;
	Logic3 x;
	
	if (f == NULL) return L3Undefined;
	if (r == NULL) return L3Undefined;

	if (f->op == DSF_OP_ASSERT) return dsassertion_test(f->assert, r);

	switch (f->op)
	{
		case DSF_OP_AND:
			hasUndef = 0;
			for (i = 0; i < f->count; i++)
			{
				x = dsfilter_test(f->filter[i], r);
				if (x == L3False) return L3False;
				if (x == L3Undefined) hasUndef = 1;
			}
			if (hasUndef == 0) return L3True;
			return L3Undefined;

		case DSF_OP_OR:
			hasUndef = 0;
			for (i = 0; i < f->count; i++)
			{
				x = dsfilter_test(f->filter[i], r);
				if (x == L3True) return L3True;
				if (x == L3Undefined) hasUndef = 1;
			}
			if (hasUndef == 0) return L3False;
			return L3Undefined;
			
		case DSF_OP_NOT:
			if (f->count == 0) return L3Undefined;
			x = dsfilter_test(f->filter[0], r);
			if (x == L3True) return L3False;
			if (x == L3False) return L3True;
			return L3Undefined;
	}

	return L3Undefined;
}
