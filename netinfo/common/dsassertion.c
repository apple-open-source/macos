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

#include <NetInfo/dsassertion.h>
#include <stdlib.h>
#include <string.h>

dsassertion *
dsassertion_new(int32_t a, int32_t m, dsdata *k, dsdata *v)
{
	dsassertion *x;

	if (k == NULL) return NULL;
	
	x = (dsassertion *)malloc(sizeof(dsassertion));
	
	x->assertion = a;
	x->meta = m;
	x->key = dsdata_retain(k);
	x->value = dsdata_retain(v);

	x->retain = 1;
	
	return x;
}

dsassertion *
dsassertion_retain(dsassertion *a)
{
	if (a == NULL) return NULL;
	a->retain++;
	return a;
}

void
dsassertion_release(dsassertion *a)
{
	if (a == NULL) return;

	a->retain--;
	if (a->retain > 0) return;

	dsdata_release(a->key);
	dsdata_release(a->value);
	free(a);
}

Logic3
dsassertion_test(dsassertion *t, dsrecord *r)
{
	u_int32_t i, len, start, ttype, n;
	dsattribute *a;

	if (t == NULL) return L3Undefined;
	if (r == NULL) return L3Undefined;

	ttype = DataTypeAny;
	a = NULL;

	if (t->assertion != DSA_PRECOMPUTED)
	{
		a = dsrecord_attribute(r, t->key, t->meta);
		if (a == NULL) return L3Undefined;
		if (t->assertion != DSA_HAS_KEY) ttype = t->value->type;
	}

	switch (t->assertion)
	{
		/* a has a value less than the assertion's value */
		case DSA_LESS:
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (dsdata_compare(a->value[i], t->value) < 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;
			
		/* a has a value less than or equal to the assertion's value */
		case DSA_LESS_OR_EQUAL:
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (dsdata_compare(a->value[i], t->value) <= 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;
			
		/* a has a value equal to the assertion's value */
		case DSA_EQUAL:
		case DSA_APPROX:
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (dsdata_compare(a->value[i], t->value) == 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;
			
		/* a has a value greater than or equal to the assertion's value */
		case DSA_GREATER_OR_EQUAL:
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (dsdata_compare(a->value[i], t->value) >= 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;
			
		/* a has a value greater than the assertion's value */
		case DSA_GREATER:
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (dsdata_compare(a->value[i], t->value) > 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;

		/* a's key is the same as the assertions's key */
		case DSA_HAS_KEY:
			if (dsdata_compare(a->key, t->key) == 0)
			{
				dsattribute_release(a);
				return L3True;
			}
			dsattribute_release(a);			
			return L3False;

		/* a has a value with the assertion's value as a prefix */
		case DSA_PREFIX:
			len = t->value->length;
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (a->value[i]->length < len) continue;
				if (dsdata_compare_sub(a->value[i], t->value, 0, len) == 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;
	
		/* a has a value with the assertion's value as a substring */
		case DSA_SUBSTR:
			len = t->value->length;
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (a->value[i]->length < len) continue;
				n = a->value[i]->length - len;
				for (start = 0; start <= n; start++)
				{
					if (dsdata_compare_sub(a->value[i], t->value, start, len) == 0)
					{
						dsattribute_release(a);
						return L3True;
					}
				}
			}
			dsattribute_release(a);
			return L3False;

		/* a has a value with the assertion's value as a suffix */
		case DSA_SUFFIX:
			len = t->value->length;
			for (i = 0; i < a->count; i++)
			{
				if (ComparableDataTypes(a->value[i]->type, ttype) == 0)
					continue;
				if (a->value[i]->length < len) continue;
				start = a->value[i]->length - len;
				if (dsdata_compare_sub(a->value[i], t->value, start, len) == 0)
				{
					dsattribute_release(a);
					return L3True;
				}
			}
			dsattribute_release(a);
			return L3False;

		/* Precomputed assertion. */
		case DSA_PRECOMPUTED:
			i = dsdata_to_uint32(t->key);
			/* bound check */
			if (i > L3Undefined) i = L3Undefined;
			return i;
	}

	/* Not reached? */
	if (t->assertion != DSA_PRECOMPUTED) dsattribute_release(a);

	return L3False;
}
