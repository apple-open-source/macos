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

#include <NetInfo/dsdata.h>
#include <NetInfo/dsattribute.h>
#include <stdlib.h>
#include <string.h>

//#define _ALLOC_DEBUG_
//#define _DEALLOC_DEBUG_

extern void
serialize_32(u_int32_t v, char **p);

extern u_int32_t
deserialize_32(char **p);

extern dsdata *
deserialize_dsdata(char **p);

dsattribute *
dsattribute_alloc(void)
{
	dsattribute *x;

	x = (dsattribute *)malloc(sizeof(dsattribute));
	memset(x, 0, sizeof(dsattribute));

#ifdef _ALLOC_DEBUG_
	fprintf(stderr, "dsattribute_alloc   0x%08x\n", (unsigned int)x);
#endif

	return x;
}

/* serialize an attribute into a machine-independent string of bytes */
dsdata *
dsattribute_to_dsdata(dsattribute *a)
{
	u_int32_t i, len, type;
	dsdata *d;
	char *p;

	if (a == NULL) return NULL;
	
	/* key length + type (4 byte int) + data (4) + val count (4) */
	len = a->key->length + 12;

	/* values */
	for (i = 0; i < a->count; i++)
	{
		/* value length + type + data */
		len += (4 + 4 + a->value[i]->length);
	}

	d = dsdata_alloc(len);
	d->retain = 1;
	d->type = DataTypeDSAttribute;

	p = d->data;

	/* key type */
	type = a->key->type;
	serialize_32(type, &p);

	/* key length */
	len = a->key->length;
	serialize_32(len, &p);
	
	/* key data */
	memmove(p, a->key->data, len);
	p += len;

	/* count */
	serialize_32(a->count, &p);

	/* values */
	for (i = 0; i < a->count; i++)
	{
		/* value type  */
		type = a->value[i]->type;
		serialize_32(type, &p);

		/* value length  */
		len = a->value[i]->length;
		serialize_32(len, &p);

		/* value data */
		memmove(p, a->value[i]->data, len);
		p += len;
	}

	return d;
}

dsattribute *
dsdata_to_dsattribute(dsdata *d)
{
	dsattribute *a;
	char *p;
	u_int32_t i, len;

	if (d == NULL) return NULL;
	if (d->type != DataTypeDSAttribute) return NULL;

	a = dsattribute_alloc();
	a->retain = 1;
	
	p = d->data;

	a->key = deserialize_dsdata(&p);

	len = deserialize_32(&p);
	a->count = len;
	a->value = NULL;

	if (len > 0) a->value = (dsdata **)malloc(len * sizeof(dsdata *));

	for (i = 0; i < a->count; i++) a->value[i] = deserialize_dsdata(&p);

	return a;
}

dsattribute *
dsattribute_new(dsdata *k)
{
	dsattribute *x;

	if (k == NULL) return NULL;
	
	x = dsattribute_alloc();
	x->key = dsdata_retain(k);
	
	x->count = 0;
	x->value = NULL;

	x->retain = 1;
	
	return x;
}

dsattribute *
dsattribute_copy(dsattribute *a)
{
	dsattribute *x;
	int i;

	if (a == NULL) return NULL;

	x = dsattribute_alloc();
	x->key = dsdata_copy(a->key);
	
	x->count = a->count;
	x->value = NULL;

	if (x->count > 0)
		x->value = (dsdata **)malloc(x->count * sizeof(dsdata *));

	for (i = 0; i < x->count; i++)
		x->value[i] = dsdata_copy(a->value[i]);

	x->retain = 1;
	
	return x;
}

dsattribute *
dsattribute_retain(dsattribute *a)
{
	if (a == NULL) return NULL;
	a->retain++;
	return a;
}

static void
dsattribute_dealloc(dsattribute *x)
{
#ifdef _DEALLOC_DEBUG_
	fprintf(stderr, "dsattribute_dealloc   0x%08x\n", (unsigned int)x);
#endif
	free(x);
}

void
dsattribute_release(dsattribute *a)
{
	u_int32_t i;

	if (a == NULL) return;

	a->retain--;
	if (a->retain > 0) return;

	dsdata_release(a->key);
	for (i = 0; i < a->count; i++) dsdata_release(a->value[i]);
	if (a->count > 0) free(a->value);
	dsattribute_dealloc(a);
}

void
dsattribute_insert(dsattribute *a, dsdata *d, u_int32_t w)
{
	u_int32_t i;

	if (a == NULL) return;
	if (d == NULL) return;

	if (w > a->count) w = a->count;

	if (a->count == 0)
		a->value = (dsdata **)malloc(sizeof(dsdata *));
	else
		a->value = (dsdata **)realloc(a->value, (a->count + 1) * sizeof(dsdata *));

	for (i = a->count; i > w; i--) a->value[i] = a->value[i-1];
	a->value[w] = dsdata_retain(d);

	a->count++;
}

void
dsattribute_append(dsattribute *a, dsdata *d)
{
	if (a == NULL) return;
	if (d == NULL) return;

	if (a->count == 0)
		a->value = (dsdata **)malloc(sizeof(dsdata *));
	else
		a->value = (dsdata **)realloc(a->value, (a->count + 1) * sizeof(dsdata *));

	a->value[a->count] = dsdata_retain(d);
	a->count++;
}

void
dsattribute_remove(dsattribute *a, u_int32_t w)
{
	u_int32_t i;

	if (a == NULL) return;
	if (w >= a->count) return;

	dsdata_release(a->value[w]);
	a->count--;

	if (a->count == 0)
	{
		free(a->value);
		a->value = NULL;
		return;
	}
	
	for (i = w; i < a->count; i++) a->value[i] = a->value[i+1];
	a->value = (dsdata **)realloc(a->value, a->count * sizeof(dsdata *));
}

void
dsattribute_merge(dsattribute *a, dsdata *d)
{
	u_int32_t i, len;

	if (a == NULL) return;
	if (d == NULL) return;

	len = a->count;
	for (i = 0; i < len; i++)
		if (dsdata_equal(a->value[i], d)) return;

	a->count++;
	if (len == 0)
		a->value = (dsdata **)malloc(sizeof(dsdata *));
	else
		a->value = (dsdata **)realloc(a->value, a->count * sizeof(dsdata *));

	a->value[len] = dsdata_retain(d);
}

u_int32_t
dsattribute_index(dsattribute *a, dsdata *d)
{
	u_int32_t i;
	
	if (a == NULL) return IndexNull;
	if (d == NULL) return IndexNull;

	for (i = 0; i < a->count; i++)
		if (dsdata_equal(a->value[i], d)) return i;

	return IndexNull;
}

dsdata *
dsattribute_key(dsattribute *a)
{
	dsdata *k;

	if (a == NULL) return NULL;

	k = a->key;
	return dsdata_retain(k);
}

dsdata *
dsattribute_value(dsattribute *a, u_int32_t w)
{
	dsdata *v;

	if (a == NULL) return NULL;

	if (w >= a->count) return NULL;
	v = a->value[w];
	return dsdata_retain(v);
}

int
dsattribute_match(dsattribute *a, dsattribute *p)
{
	u_int32_t i, j, found;
	dsdata *v;

	if (a == p) return 1;

	if (a == NULL) return 0;
	if (p == NULL) return 1;

	/* compare keys */
	if (dsdata_equal(a->key, p->key) == 0) return 0;
	
	/* check each value in pattern */
	for (i = 0; i < p->count; i++)
	{
		v = p->value[i];
		found = 0;
		
		for (j = 0; j < a->count; j++)
		{
			if (dsdata_equal(a->value[j], v))
			{
				found = 1;
				break;
			}
		}

		if (found == 0) return 0;
	}

	return 1;
}

int
dsattribute_equal(dsattribute *a, dsattribute *b)
{
	u_int32_t i, j, found;
	dsdata *v;

	if (a == b) return 1;

	if (a == NULL) return 0;
	if (b == NULL) return 0;

	if (a->count != b->count) return 0;

	/* compare keys */
	if (dsdata_equal(a->key, b->key) == 0) return 0;
	
	/* check each value in a */
	for (i = 0; i < a->count; i++)
	{
		v = a->value[i];
		found = 0;
		
		for (j = 0; j < b->count; j++)
		{
			if (dsdata_equal(b->value[j], v))
			{
				found = 1;
				break;
			}
		}

		if (found == 0) return 0;
	}

	return 1;
}

void
dsattribute_setkey(dsattribute *a, dsdata *k)
{
	if (a == NULL) return;
	if (k == NULL) return;

	dsdata_release(a->key);
	a->key = dsdata_retain(k);
}
