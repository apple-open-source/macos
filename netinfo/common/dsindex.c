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

#include <NetInfo/dsindex.h>
#include <stdlib.h>
#include <string.h>

/*
 * Create a new index.
 */
dsindex *
dsindex_new(void)
{
	dsindex *x;

	x = (dsindex *)malloc(sizeof(dsindex));
	x->key_count = 0;
	x->kindex = NULL;
	return x;
}

/*
 * Free an index and release all the data objects it holds.
 */
static void
dsindex_val_free(dsindex_val_t *v)
{
	if (v == NULL) return;

	if (v->val != NULL) dsdata_release(v->val);
	if (v->dsid != NULL) free(v->dsid);
	free(v);
}

static void
dsindex_key_free(dsindex_key_t *k)
{
	u_int32_t i;

	if (k == NULL) return;

	if (k->key != NULL) dsdata_release(k->key);

	for (i = 0; i < k->val_count; i++)
	{
		dsindex_val_free(k->vindex[i]);
	}

	if (k->vindex != NULL) free(k->vindex);
	free(k);
}

void
dsindex_free(dsindex *x)
{
	u_int32_t i;

	if (x == NULL) return;

	for (i = 0; i < x->key_count; i++)
	{
		dsindex_key_free(x->kindex[i]);
	}

	if (x->kindex != NULL) free(x->kindex);
	free(x);
}

/*
 * Find / Insert / Delete a key.
 */
static dsindex_key_t *
dsindex_util(dsindex *x, dsdata *key, int32_t what)
{
	u_int32_t i, j;

	if (x == NULL) return NULL;
	if (key == NULL) return NULL;

	for (i = 0; i < x->key_count; i++)
	{
		if (dsdata_equal(key, x->kindex[i]->key) == 1)
		{
			if (what < 0)
			{
				free(x->kindex[i]);
				for (j = i + 1; j < x->key_count; j++)
					x->kindex[j - 1] = x->kindex[j];
				x->key_count--;
				if (x->key_count == 0)
				{
					free(x->kindex);
					x->kindex = NULL;
					return NULL;
				}
				x->kindex = (dsindex_key_t **)realloc(x->kindex, x->key_count * sizeof(dsindex_key_t *));
				return NULL;
			}

			return x->kindex[i];
		}
	}

	if (what <= 0) return NULL;

	i = x->key_count;
	x->key_count++;

	if (i == 0)
		x->kindex = (dsindex_key_t **)malloc(sizeof(dsindex_key_t *));
	else
		x->kindex = (dsindex_key_t **)realloc(x->kindex, x->key_count * sizeof(dsindex_key_t *));

	x->kindex[i] = (dsindex_key_t *)malloc(sizeof(dsindex_key_t));
	x->kindex[i]->key = dsdata_retain(key);
	x->kindex[i]->val_count = 0;
	x->kindex[i]->vindex = NULL;

	return x->kindex[i];
}

/*
 * Insert a val at a specific location.
 */
static dsindex_val_t *
kindex_insert_val(dsindex_key_t *kx, dsdata *val, u_int32_t where)
{
	u_int32_t i, n;

	if (kx == NULL) return NULL;
	if (val == NULL) return NULL;

	n = kx->val_count;
	kx->val_count++;

	if (n == 0)
	{
		kx->vindex = (dsindex_val_t **)malloc(sizeof(dsindex_val_t *));
		i = 0;
	}
	else
	{
		kx->vindex = (dsindex_val_t **)realloc(kx->vindex, kx->val_count * sizeof(dsindex_val_t *));
		for (i = n; i > where; i--)
		{
			kx->vindex[i] = kx->vindex[i - 1];
		}
		i = where;
	}

	kx->vindex[i] = (dsindex_val_t *)malloc(sizeof(dsindex_val_t));
	kx->vindex[i]->val = dsdata_retain(val);
	kx->vindex[i]->dsid_count = 0;
	kx->vindex[i]->dsid = NULL;

	return kx->vindex[i];
}

/*
 * Delete a val at a specific location.
 */
static dsindex_val_t *
kindex_delete_val(dsindex_key_t *kx, u_int32_t where)
{
	u_int32_t i;

	if (kx == NULL) return NULL;
	if (kx->val_count == 0) return NULL;
	if (where >= kx->val_count) return NULL;

	if (kx->val_count == 1)
	{
		free(kx->vindex[0]);
		free(kx->vindex);
		kx->vindex = NULL;
		kx->val_count = 0;
		return NULL;
	}

	
	free(kx->vindex[where]);
	for (i = where + 1; i < kx->val_count; i++)
	{
		kx->vindex[i - 1] = kx->vindex[i];
	}

	kx->val_count--;
	kx->vindex = (dsindex_val_t **)realloc(kx->vindex, kx->val_count * sizeof(dsindex_val_t *));

	return NULL;
}

/*
 * Find / Insert / Delete a val.
 */
static dsindex_val_t *
kindex_util(dsindex_key_t *kx, dsdata *val, int32_t what)
{
	u_int32_t jump, v;
	int32_t comp;

	if (kx == NULL) return NULL;
	if (val == NULL) return NULL;

	if (kx->val_count == 0)
	{
		if (what > 0) return kindex_insert_val(kx, val, 0);
		return NULL;
	}

	v = kx->val_count / 2;
	jump = v / 2;

	while (jump != 0)
	{
		comp = dsdata_compare(kx->vindex[v]->val, val);
		if (comp == 0)
		{
			if (what < 0) return kindex_delete_val(kx, v);
			return kx->vindex[v];
		}

		if (comp > 0) v -= jump;
		else v += jump;
		jump /= 2;
	}

	comp = dsdata_compare(kx->vindex[v]->val, val);
	if (comp == 0)
	{
		if (what < 0) return kindex_delete_val(kx, v);
		return kx->vindex[v];
	}

	while (comp > 0)
	{
		if (v == 0) 
		{
			if (what <= 0) return NULL;
			return kindex_insert_val(kx, val, v);
		}

		v--;
		comp = dsdata_compare(kx->vindex[v]->val, val);
		if (comp == 0)
		{
			if (what < 0) return kindex_delete_val(kx, v);
			return kx->vindex[v];
		}
		if (comp < 0) 
		{
			if (what <= 0) return NULL;
			v++;
			return kindex_insert_val(kx, val, v);
		}
	}

	while (comp < 0)
	{
		v++;
		if (v == kx->val_count)
		{
			if (what <= 0) return NULL;
			return kindex_insert_val(kx, val, v);
		}
		comp = dsdata_compare(kx->vindex[v]->val, val);
		if (comp == 0)
		{
			if (what < 0) return kindex_delete_val(kx, v);
			return kx->vindex[v];
		}
		if (comp > 0)
		{
			if (what <= 0) return NULL;
			return kindex_insert_val(kx, val, v);
		}
	}

	return NULL;
}

/*
 * Insert a dsid at a specific location.
 */
static u_int32_t
vindex_insert_dsid(dsindex_val_t *vx, u_int32_t dsid, u_int32_t where)
{
	u_int32_t i, n;

	if (vx == NULL) return 0;

	n = vx->dsid_count;
	vx->dsid_count++;

	if (n == 0)
	{
		vx->dsid = (u_int32_t *)malloc(sizeof(u_int32_t));
		i = 0;
	}
	else
	{
		vx->dsid = (u_int32_t *)realloc(vx->dsid, vx->dsid_count * sizeof(u_int32_t));
		for (i = n; i > where; i--)
		{
			vx->dsid[i] = vx->dsid[i - 1];
		}
		i = where;
	}

	vx->dsid[where] = dsid;
	return 0;
}

/*
 * Delete a dsid at a specific location.
 */
static u_int32_t 
vindex_delete_dsid(dsindex_val_t *vx, u_int32_t where)
{
	u_int32_t i;

	if (vx == NULL) return 0;
	if (vx->dsid_count == 0) return 0;
	if (where >= vx->dsid_count) return 0;

	if (vx->dsid_count == 1)
	{
		free(vx->dsid);
		vx->dsid = NULL;
		vx->dsid_count = 0;
		return 0;
	}

	
	for (i = where + 1; i < vx->dsid_count; i++)
	{
		vx->dsid[i - 1] = vx->dsid[i];
	}

	vx->dsid_count--;
	vx->dsid = (u_int32_t *)realloc(vx->dsid, vx->dsid_count * sizeof(u_int32_t));

	return 0;
}

/*
 * Find / Insert / Delete a dsid.
 * Returns 1 if found, 0 is not found.
 * Returns 0 on Insert or Delete.
 */
static u_int32_t
vindex_util(dsindex_val_t *vx, u_int32_t dsid, int32_t what)
{
	u_int32_t jump, w;

	if (vx == NULL) return 0;

	if (vx->dsid_count == 0)
	{
		if (what > 0) return vindex_insert_dsid(vx, dsid, 0);
		return 0;
	}

	w = vx->dsid_count / 2;
	jump = w / 2;

	while (jump != 0)
	{
		if (vx->dsid[w] == dsid)
		{
			if (what < 0) return vindex_delete_dsid(vx, w);
			return 1;
		}

		if (vx->dsid[w] > dsid) w -= jump;
		else w += jump;
		jump /= 2;
	}

	if (vx->dsid[w] == dsid)
	{
		if (what < 0) return vindex_delete_dsid(vx, w);
		return 1;
	}

	while (vx->dsid[w] > dsid)
	{
		if (w == 0) 
		{
			if (what <= 0) return 0;
			return vindex_insert_dsid(vx, dsid, w);
		}

		w--;
		if (vx->dsid[w] == dsid)
		{
			if (what < 0) return vindex_delete_dsid(vx, w);
			return 1;
		}
		if (vx->dsid[w] < dsid) 
		{
			if (what <= 0) return 0;
			w++;
			return vindex_insert_dsid(vx, dsid, w);
		}
	}

	while (vx->dsid[w] < dsid)
	{
		w++;
		if (w == vx->dsid_count)
		{
			if (what <= 0) return 0;
			return vindex_insert_dsid(vx, dsid, w);
		}
		if (vx->dsid[w] == dsid) 
		{
			if (what < 0) return vindex_delete_dsid(vx, w);
			return 1;
		}
		if (vx->dsid[w] > dsid)
		{
			if (what <= 0) return 0;
			return vindex_insert_dsid(vx, dsid, w);
		}
	}

	return 0;
}

/*
 * Adds a new key.
 */
void
dsindex_insert_key(dsindex *x, dsdata *key)
{
	dsindex_util(x, key, 1);
}

/*
 * Adds a new key, val, dsid.
 * key and val are added if they doesn't exist.
 */
void
dsindex_insert(dsindex *x, dsdata *key, dsdata *val, u_int32_t dsid)
{
	dsindex_key_t *kx;
	dsindex_val_t *vx;

	kx = dsindex_util(x, key, 1);
	if (kx == NULL) return;

	vx = kindex_util(kx, val, 1);
	if (vx == NULL) return;
	
	vindex_util(vx, dsid, 1);
}

static void
kindex_delete_dsid(dsindex_key_t *kx, u_int32_t dsid)
{
	u_int32_t i, j, p, l;
	dsindex_val_t *vx;

	if (kx == NULL) return;

	for (i = 0; i < kx->val_count; i++)
	{
		vx = kx->vindex[i];
		l = vx->dsid_count;

		for (p = 0, j = 0; j < vx->dsid_count; j++)
		{
			if (vx->dsid[j] == dsid)
			{
				l--;
				continue;
			}

			vx->dsid[p++] = vx->dsid[j];
		}

		if (l == 0)
		{
			free(vx->dsid);
			vx->dsid_count = 0;
		}
		else if (l < vx->dsid_count)
		{
			vx->dsid = (u_int32_t *)realloc(vx->dsid, l * sizeof(u_int32_t));
			vx->dsid_count = l;
		}
	}

	l = kx->val_count;

	for (p = 0, i = 0; i < kx->val_count; i++)
	{
		if (kx->vindex[i]->dsid_count == 0)
		{
			dsdata_release(kx->vindex[i]->val);
			free(kx->vindex[i]);
			l--;
			continue;
		}
		kx->vindex[p++] = kx->vindex[i];
	}

	if (l == 0)
	{
		free(kx->vindex);
		kx->vindex = NULL;
		kx->val_count = 0;
	}
	else if (l < kx->val_count)
	{
		kx->vindex = (dsindex_val_t **)realloc(kx->vindex, l * sizeof(dsindex_val_t *));
		kx->val_count = l;
	}
}

void
dsindex_delete_dsid(dsindex *x, u_int32_t dsid)
{
	int i;

	if (x == NULL) return;
	
	for (i = 0; i < x->key_count; i++)
	{
		kindex_delete_dsid(x->kindex[i], dsid);
	}
}

static void
dsindex_attribute_util(dsindex *x, dsattribute *a, u_int32_t dsid, int32_t what)
{
	u_int32_t i, w;
	dsindex_key_t *kx;
	dsindex_val_t *vx;

	if (x == NULL) return;
	if (a == NULL) return;
	if (dsid == IndexNull) return;

	w = 0;
	if (what > 0) w = 1;
	kx = dsindex_util(x, a->key, w);
	if (kx == NULL) return;

	for (i = 0; i < a->count; i++)
	{
		vx = kindex_util(kx, a->value[i], w);
		vindex_util(vx, dsid, what);
	}
}

void
dsindex_insert_attribute(dsindex *x, dsattribute *a, u_int32_t dsid)
{
	dsindex_attribute_util(x, a, dsid, 1);
}

void
dsindex_insert_record(dsindex *x, dsrecord *r)
{
	u_int32_t i;

	if (x == NULL) return;
	if (r == NULL) return;
	
	for (i = 0; i < r->count; i++)
	{
		dsindex_attribute_util(x, r->attribute[i], r->dsid, 1);
	}
}

dsindex_val_t *
dsindex_lookup(dsindex *x, dsdata *key, dsdata *val)
{
	dsindex_key_t *kx;
	
	kx = dsindex_util(x, key, 0);
	return kindex_util(kx, val, 0);
}

dsindex_key_t *
dsindex_lookup_key(dsindex *x, dsdata *key)
{
	return dsindex_util(x, key, 0);
}

dsindex_val_t *
dsindex_lookup_val(dsindex_key_t *kx, dsdata *val)
{
	return kindex_util(kx, val, 0);
}

void
dsindex_print(dsindex *x, FILE *f)
{
	u_int32_t i, j, k;
	
	if (x == NULL)
	{
		fprintf(f, "-nil-\n");
		return;
	}

	for (i = 0; i < x->key_count; i++)
	{
		fprintf(f, "Key: ");
		dsdata_print(x->kindex[i]->key, f);
		fprintf(f, "\n");

		for (j = 0; j < x->kindex[i]->val_count; j++)
		{
			fprintf(f, "    ");
			dsdata_print(x->kindex[i]->vindex[j]->val, f);
			fprintf(f, " @");
			for (k = 0; k < x->kindex[i]->vindex[j]->dsid_count; k++)
				fprintf(f, " %u", x->kindex[i]->vindex[j]->dsid[k]);
			fprintf(f, "\n");
		}
	}
}
