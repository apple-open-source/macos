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

#include <NetInfo/dsrecord.h>
#include <NetInfo/dsindex.h>
#include <stdlib.h>
#include <string.h>

void
serialize_32(u_int32_t v, char **p)
{
	u_int32_t n;

	n = htonl(v);
	memmove(*p, &n, 4);
	*p += 4;
}

/* serialize a record into a machine-independent string of bytes */
dsdata *
dsrecord_to_dsdata(dsrecord *r)
{
	u_int32_t i, len, type, attrx, valx;
	dsdata *d;
	char *p;

	if (r == NULL) return NULL;

	len = 0;

	/* dsid */
	len += 4;

	/* serial */
	len += 4;

	/* vers */
	len += 4;

	/* super */
	len += 4;

	/* sub_count */
	len += 4;

	/* sub */
	len += (4 * r->sub_count);

	/* count */
	len += 4;

	/* attributes */
	for (attrx = 0; attrx < r->count; attrx++)
	{
		/* key length + type + data */
		len += (4 + 4 + r->attribute[attrx]->key->length);

		/* count */
		len += 4;

		/* values */
		for (valx = 0; valx < r->attribute[attrx]->count; valx++)
		{
			/* value length + type + data */
			len += (4 + 4 + r->attribute[attrx]->value[valx]->length);
		}
	}

	/* meta_count */
	len += 4;

	/* meta attributes */
	for (attrx = 0; attrx < r->meta_count; attrx++)
	{
		/* key length + type + data */
		len += (4 + 4 + r->meta_attribute[attrx]->key->length);

		/* count */
		len += 4;

		/* values */
		for (valx = 0; valx < r->meta_attribute[attrx]->count; valx++)
		{
			/* value length + type + data */
			len += (4 + 4 + r->meta_attribute[attrx]->value[valx]->length);
		}
	}

	d = dsdata_alloc(len);
	d->retain = 1;
	d->type = DataTypeDSRecord;

	p = d->data;

	/* record id */
	serialize_32(r->dsid, &p);

	/* record serial number */
	serialize_32(r->serial, &p);

	/* datastore version */
	serialize_32(r->vers, &p);

	/* super */
	serialize_32(r->super, &p);

	/* sub_count */
	serialize_32(r->sub_count, &p);

	/* sub */
	for (i = 0; i < r->sub_count; i++)
	{
		serialize_32(r->sub[i], &p);
	}

	/* count */
	serialize_32(r->count, &p);

	/* attributes */
	for (attrx = 0; attrx < r->count; attrx++)
	{
		/* key type */
		type = r->attribute[attrx]->key->type;
		serialize_32(type, &p);

		/* key length */
		len = r->attribute[attrx]->key->length;
		serialize_32(len, &p);

		/* key data */
		memmove(p, r->attribute[attrx]->key->data, len);
		p += len;

		/* count */
		serialize_32(r->attribute[attrx]->count, &p);

		/* values */
		for (valx = 0; valx < r->attribute[attrx]->count; valx++)
		{
			/* value type  */
			type = r->attribute[attrx]->value[valx]->type;
			serialize_32(type, &p);

			/* value length  */
			len = r->attribute[attrx]->value[valx]->length;
			serialize_32(len, &p);

			/* value data */
			memmove(p, r->attribute[attrx]->value[valx]->data, len);
			p += len;
		}
	}

	/* meta_count */
	serialize_32(r->meta_count, &p);

	/* meta_attributes */
	for (attrx = 0; attrx < r->meta_count; attrx++)
	{
		/* key type */
		type = r->meta_attribute[attrx]->key->type;
		serialize_32(type, &p);

		/* key length */
		len = r->meta_attribute[attrx]->key->length;
		serialize_32(len, &p);

		/* key data */
		memmove(p, r->meta_attribute[attrx]->key->data, len);
		p += len;

		/* count */
		serialize_32(r->meta_attribute[attrx]->count, &p);

		/* values */
		for (valx = 0; valx < r->meta_attribute[attrx]->count; valx++)
		{
			/* value type  */
			type = r->meta_attribute[attrx]->value[valx]->type;
			serialize_32(type, &p);

			/* value length  */
			len = r->meta_attribute[attrx]->value[valx]->length;
			serialize_32(len, &p);

			/* value data */
			memmove(p, r->meta_attribute[attrx]->value[valx]->data, len);
			p += len;
		}
	}

	return d;
}

u_int32_t
deserialize_32(char **p)
{
	u_int32_t n, t;
	
	memmove(&t, *p, 4);
	*p += 4;

	n = ntohl(t);
	return n;
}

dsdata *
deserialize_dsdata(char **p)
{
	dsdata *udata;
	u_int32_t t, l;

	t = deserialize_32(p);
	l = deserialize_32(p);

	udata = dsdata_alloc(l);
	udata->type = t;
	udata->retain = 1;
	if (udata->length > 0) memmove(udata->data, *p, udata->length);

	*p += udata->length;

	return udata;
}

dsrecord *
dsdata_to_dsrecord(dsdata *d)
{
	dsrecord *r;
	char *p;
	u_int32_t i, len, attrx, valx;

	if (d == NULL) return NULL;
	if (d->type != DataTypeDSRecord) return NULL;

	r = (dsrecord *)malloc(sizeof(dsrecord));
	r->retain = 1;
	
	p = d->data;

	r->dsid = deserialize_32(&p);
	r->serial = deserialize_32(&p);
	r->vers = deserialize_32(&p);
	r->super = deserialize_32(&p);
	r->sub_count = deserialize_32(&p);

	if (r->sub_count > 0)
		r->sub = (u_int32_t *)malloc(r->sub_count * sizeof(u_int32_t));
	else
		r->sub = NULL;

	for (i = 0; i < r->sub_count; i++)
	{
		r->sub[i] = deserialize_32(&p);
	}

	r->count = deserialize_32(&p);
	if (r->count > 0)
		r->attribute = (dsattribute **)malloc(r->count * sizeof(dsattribute *));
	else
		r->attribute = NULL;

	for (attrx = 0; attrx < r->count; attrx++)
	{
		r->attribute[attrx] = dsattribute_alloc();
		r->attribute[attrx]->retain = 1;

	
		r->attribute[attrx]->key = deserialize_dsdata(&p);

		len = deserialize_32(&p);
		r->attribute[attrx]->count = len;
		if (len > 0)
			r->attribute[attrx]->value = (dsdata **)malloc(len * sizeof(dsdata *));
		else 
			r->attribute[attrx]->value = NULL;

		for (valx = 0; valx < r->attribute[attrx]->count; valx++)
			r->attribute[attrx]->value[valx] = deserialize_dsdata(&p);
	}

	r->meta_count = deserialize_32(&p);
	if (r->meta_count  > 0)
		r->meta_attribute = (dsattribute **)malloc(r->meta_count * sizeof(dsattribute *));
	else
		r->meta_attribute = NULL;

	for (attrx = 0; attrx < r->meta_count; attrx++)
	{
		r->meta_attribute[attrx] = dsattribute_alloc();
		r->meta_attribute[attrx]->retain = 1;

		r->meta_attribute[attrx]->key = deserialize_dsdata(&p);

		len = deserialize_32(&p);
		r->meta_attribute[attrx]->count = len;
		if (len > 0)
			r->meta_attribute[attrx]->value = (dsdata **)malloc(len * sizeof(dsdata *));
		else
			r->meta_attribute[attrx]->value = NULL;

		for (valx = 0; valx < r->meta_attribute[attrx]->count; valx++)
			r->meta_attribute[attrx]->value[valx] = deserialize_dsdata(&p);
	}

	r->index = NULL;
	r->next = NULL;

	return r;
}

/*
 * Performance hack to get a record's vital stats (dsid, vers, serial, and super)
 */
dsstatus
dsrecord_fstats(FILE *f, u_int32_t *dsid, u_int32_t *vers, u_int32_t *serial, u_int32_t *super)
{
	int n;
	u_int32_t x;
	
	if (dsid != NULL) *dsid = IndexNull;
	if (vers != NULL) *vers = IndexNull;
	if (serial != NULL) *serial = IndexNull;
	if (super != NULL) *super = IndexNull;

	if (f == NULL) return DSStatusInvalidStore;

	/* dsdata type */
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusReadFailed;

	n = ntohl(x);
	if (n != DataTypeDSRecord) return DSStatusInvalidRecord;
	
	/* dsdata length */
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusReadFailed;

	/* dsrecord dsid */
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusReadFailed;
	if (dsid != NULL) *dsid = ntohl(x);

	/* dsrecord serial */
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusReadFailed;
	if (serial != NULL) *serial = ntohl(x);

	/* dsrecord vers */
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusReadFailed;
	if (vers != NULL) *vers = ntohl(x);

	/* dsrecord super */
	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return DSStatusReadFailed;
	if (super != NULL) *super = ntohl(x);
	
	return DSStatusOK;
}

dsrecord *
dsrecord_new(void)
{
	dsrecord *r;

	r = (dsrecord *)malloc(sizeof(dsrecord));
	r->retain = 1;

	r->dsid = -1;
	r->serial = 0;
	r->vers = -1;

	r->super = -1;

	r->sub_count = 0;
	r->sub = NULL;

	r->count = 0;
	r->attribute = NULL;

	r->meta_count = 0;
	r->meta_attribute = NULL;

	r->index = NULL;
	r->next = NULL;

	return r;
}

dsrecord *
dsrecord_retain(dsrecord *r)
{
	if (r == NULL) return NULL;
	r->retain++;
	return r;
}	

static dsrecord *
dsrecord_release_worker(dsrecord *r)
{
	u_int32_t i;
	dsrecord *n;

	if (r == NULL) return NULL;
	
	r->retain--;
	if (r->retain > 0) return NULL;

	if (r->sub_count > 0) free(r->sub);

	for (i = 0; i < r->count; i++)
		dsattribute_release(r->attribute[i]);
	if (r->count > 0) free(r->attribute);

	for (i = 0; i < r->meta_count; i++)
		dsattribute_release(r->meta_attribute[i]);
	if (r->meta_count > 0) free(r->meta_attribute);

	if (r->index != NULL) dsindex_free(r->index);

	n = r->next;

	free(r);
	
	return n;
}

void
dsrecord_release(dsrecord *r)
{
	dsrecord *n;

	n = r;
	while (n != NULL)
	{
		n = dsrecord_release_worker(n);
	}
}

dsrecord *
dsrecord_read(char *filename)
{
	dsdata *d;
	dsrecord *r;

	d = dsdata_read(filename);
	r = dsdata_to_dsrecord(d);
	dsdata_release(d);
	return r;
}


dsrecord *
dsrecord_fread(FILE *f)
{
	dsdata *d;
	dsrecord *r;

	d = dsdata_fread(f);
	r = dsdata_to_dsrecord(d);
	dsdata_release(d);
	return r;
}

dsstatus
dsrecord_write(dsrecord *r, char *filename)
{
	dsdata *d;
	dsstatus ds;

	d = dsrecord_to_dsdata(r);
	ds = dsdata_write(d, filename);
	dsdata_release(d);
	return ds;
}

dsstatus
dsrecord_fwrite(dsrecord *r, FILE *f)
{
	dsdata *d;
	dsstatus ds;

	d = dsrecord_to_dsdata (r);
	ds = dsdata_fwrite(d, f);
	dsdata_release(d);
	return ds;
}

dsrecord *
dsrecord_copy(dsrecord *r)
{
	dsdata *d;
	dsrecord *x;

	if (r == NULL) return NULL;

	d = dsrecord_to_dsdata(r);
	if (d == NULL) return NULL;

	x = dsdata_to_dsrecord(d);
	dsdata_release(d);

	x->dsid = -1;
	return x;
}

u_int32_t
dsrecord_has_sub(dsrecord *r, u_int32_t c)
{
	u_int32_t i, len;

	if (r == NULL) return 0;

	len = r->sub_count;
	for (i = 0; i < len; i++) if (r->sub[i] == c) return 1;
	return 0;
}

void
dsrecord_append_sub(dsrecord *r, u_int32_t c)
{
	u_int32_t i, len;

	if (r == NULL) return;
	if (c == 0) return;
	if (c == r->dsid) return;
	if (c == r->super) return;

	len = r->sub_count;
	for (i = 0; i < len; i++) if (r->sub[i] == c) return;
	
	r->sub_count++;
	if (len == 0)
		r->sub = (u_int32_t *)malloc(sizeof(u_int32_t));
	else
		r->sub = (u_int32_t *)realloc(r->sub, r->sub_count * sizeof(u_int32_t));

	r->sub[len] = c;
}

void
dsrecord_remove_sub(dsrecord *r, u_int32_t c)
{
	u_int32_t i;

	if (r == NULL) return;

	for (i = 0; i < r->sub_count; i++) if (r->sub[i] == c) break;

	if (i >= r->sub_count) return;

	if (r->sub_count == 1)
	{
		free(r->sub);
		r->sub = NULL;
		r->sub_count = 0;
		return;
	}

	for (i += 1; i < r->sub_count; i++) r->sub[i - 1] = r->sub[i];

	r->sub_count--;
	r->sub = (u_int32_t *)realloc(r->sub, r->sub_count * sizeof(u_int32_t));
}

void
dsrecord_merge_attribute(dsrecord *r, dsattribute *a, u_int32_t asel)
{
	u_int32_t i, x, len, merge;
	dsattribute **attribute;

	if (r == NULL) return;
	if (a == NULL) return;

	attribute = r->attribute;
	len = r->count;
	
	if (asel == SELECT_META_ATTRIBUTE)
	{
		attribute = r->meta_attribute;
		len = r->meta_count;
	}
	
	merge = 0;
	x = 0;
	for (i = 0; i < len; i++)
	{
		if (dsattribute_equal(attribute[i], a)) return;
		if (dsdata_equal(attribute[i]->key, a->key))
		{
			x = i;
			merge = 1;
			break;
		}
	}
	
	if (merge == 1)
	{
		for (i = 0; i < a->count; i++)
			dsattribute_merge(attribute[x], a->value[i]);
		return;
	}

	dsrecord_append_attribute(r, a, asel);
}

void
dsrecord_insert_attribute(dsrecord *r, dsattribute *a, u_int32_t where, u_int32_t asel)
{
	u_int32_t len, x, i;

	if (r == NULL) return;
	if (a == NULL) return;

	if (asel == SELECT_ATTRIBUTE)
	{
		len = r->count;
		r->count++;
		if (len == 0)
			r->attribute = (dsattribute **)malloc(sizeof(dsattribute *));
		else
			r->attribute = (dsattribute **)realloc(r->attribute,
				r->count * sizeof(dsattribute *));

		x = where;
		if (x > len) x = len;
		for (i = len; i > x; i--) r->attribute[i] = r->attribute[i-1];

		r->attribute[x] = dsattribute_retain(a);
	}
	else
	{
		len = r->meta_count;
		r->meta_count++;
		if (len == 0)
			r->meta_attribute = (dsattribute **)malloc(sizeof(dsattribute *));
		else
			r->meta_attribute = (dsattribute **)realloc(r->meta_attribute,
				r->meta_count * sizeof(dsattribute *));

		x = where;
		if (x > len) x = len;
		for (i = len; i > x; i--) r->meta_attribute[i] = r->meta_attribute[i-1];

		r->meta_attribute[x] = dsattribute_retain(a);
	}
}

void
dsrecord_append_attribute(dsrecord *r, dsattribute *a, u_int32_t asel)
{
	u_int32_t len;

	if (r == NULL) return;
	if (a == NULL) return;

	if (asel == SELECT_ATTRIBUTE)
	{
		len = r->count;
		r->count++;
		if (len == 0)
			r->attribute = (dsattribute **)malloc(sizeof(dsattribute *));
		else
			r->attribute = (dsattribute **)realloc(r->attribute,
				r->count * sizeof(dsattribute *));

		r->attribute[len] = dsattribute_retain(a);
	}
	else
	{
		len = r->meta_count;
		r->meta_count++;
		if (len == 0)
			r->meta_attribute = (dsattribute **)malloc(sizeof(dsattribute *));
		else
			r->meta_attribute = (dsattribute **)realloc(r->meta_attribute,
				r->meta_count * sizeof(dsattribute *));

		r->meta_attribute[len] = dsattribute_retain(a);
	}
}

static void
dsrecord_remove_attribute_index(dsrecord *r, u_int32_t x, u_int32_t asel)
{
	u_int32_t i, len;
	dsattribute **attribute;

	if (r == NULL) return;

	len = r->count;
	attribute = r->attribute;

	if (asel == SELECT_META_ATTRIBUTE)
	{
		len = r->meta_count;
		attribute = r->meta_attribute;
	}

	if (x >= len) return;

	dsattribute_release(attribute[x]);

	if (len == 1)
	{
		if (asel == SELECT_ATTRIBUTE)
		{
			free(r->attribute);
			r->attribute = NULL;
			r->count = 0;
		}
		else
		{
			free(r->meta_attribute);
			r->meta_attribute = NULL;
			r->meta_count = 0;
		}
		return;
	}

	for (i = x + 1; i < len; i++) attribute[i - 1] = attribute[i];

	if (asel == SELECT_ATTRIBUTE)
	{
		r->count--;
		r->attribute = (dsattribute **)realloc(r->attribute, r->count * sizeof(dsattribute *));
	}
	else
	{
		r->meta_count--;
		r->meta_attribute = (dsattribute **)realloc(r->meta_attribute, r->meta_count * sizeof(dsattribute *));
	}
}

void
dsrecord_remove_attribute(dsrecord *r, dsattribute *a, u_int32_t asel)
{
	u_int32_t i, len;
	dsattribute **attribute;

	if (r == NULL) return;
	if (a == NULL) return;

	attribute = r->attribute;
	len = r->count;
	
	if (asel == SELECT_META_ATTRIBUTE)
	{
		attribute = r->meta_attribute;
		len = r->meta_count;
	}

	for (i = 0; i < len; i++)
		if (dsattribute_equal(attribute[i], a)) break;

	if (i >= len) return;
	dsrecord_remove_attribute_index(r, i, asel);
}

dsattribute *
dsrecord_attribute(dsrecord *r, dsdata *k, u_int32_t asel)
{
	u_int32_t i, len;
	dsattribute **attribute;
	dsattribute *a;

	if (r == NULL) return NULL;
	if (k == NULL) return NULL;

	attribute = r->attribute;
	len = r->count;
	
	if (asel == SELECT_META_ATTRIBUTE)
	{
		attribute = r->meta_attribute;
		len = r->meta_count;
	}		

	for (i = 0; i < len; i++)
	{
		if (dsdata_equal(attribute[i]->key, k)) 
		{
			a = attribute[i];
			return dsattribute_retain(a);
		}
	}
	
	return NULL;
}

void
dsrecord_remove_key(dsrecord *r, dsdata *k, u_int32_t asel)
{
	u_int32_t i, len;
	dsattribute **attribute;

	if (r == NULL) return;
	if (k == NULL) return;

	attribute = r->attribute;
	len = r->count;
	
	if (asel == SELECT_META_ATTRIBUTE)
	{
		attribute = r->meta_attribute;
		len = r->meta_count;
	}		

	for (i = 0; i < len; i++)
	{
		if (dsdata_equal(attribute[i]->key, k)) break;
	}
	
	if (i >= len) return;
	dsrecord_remove_attribute_index(r, i, asel);
}

/*
 * Find a key or meta key and return its index.
 */
u_int32_t
dsrecord_attribute_index(dsrecord *r, dsdata *key, u_int32_t asel)
{
	u_int32_t i;

	if (asel == SELECT_ATTRIBUTE)
	{
		for (i = 0; i < r->count; i++)
			if (dsdata_equal(r->attribute[i]->key, key)) return i;
	}
	else
	{
		for (i = 0; i < r->meta_count; i++)
			if (dsdata_equal(r->meta_attribute[i]->key, key)) return i;
	}

	return IndexNull;
}

/*
 * Test a record for given key and value.
 * Returns 1 for match, 0 for no match.
 * If value is NULL,  tests for presence of key.
 * If key is NULL, tests all keys for this value.
 * If key and value are both NULL, returns 1.
 */
int
dsrecord_match_key_val(dsrecord *r, dsdata *k, dsdata *v, u_int32_t asel)
{
	u_int32_t i, len;
	dsattribute **attribute;
	
	if (r == NULL) return 0;
	if ((k == NULL) && (v == NULL)) return 1;

	attribute = r->attribute;
	len = r->count;
	
	if (asel == SELECT_META_ATTRIBUTE)
	{
		attribute = r->meta_attribute;
		len = r->meta_count;
	}

	if (k == NULL)
	{
		/* check all attributes for this value */
		for (i = 0; i < len; i++)
		{
			if (dsattribute_index(attribute[i], v) != IndexNull) return 1;
		}
		
		return 0;
	}
	
	i = dsrecord_attribute_index(r, k, asel);
	if (i != IndexNull)
	{
		if (v == NULL) return 1;
		if (dsattribute_index(attribute[i], v) != IndexNull) return 1;
	}
	
	return 0;
}

/*
 * Test a record against a pattern.
 * Returns 1 for match, 0 for no match.
 */
int
dsrecord_match(dsrecord *r, dsrecord *pattern)
{
	u_int32_t i, x;
	dsattribute *pa, *ra;

	if (r == pattern) return 1;
	if (r == NULL) return 0;
	if (pattern == NULL) return 1;
	
	/* check each attribute in the pattern */
	for (i = 0; i < pattern->count; i++)
	{
		pa = pattern->attribute[i];
		x = dsrecord_attribute_index(r, pa->key, SELECT_ATTRIBUTE);
		if (x == IndexNull) return 0;
		ra = r->attribute[x];
		
		if (dsattribute_match(ra, pa) == 0) return 0;
	}
	
	/* check each meta-attribute in the pattern */
	for (i = 0; i < pattern->meta_count; i++)
	{
		pa = pattern->meta_attribute[i];
		x = dsrecord_attribute_index(r, pa->key, SELECT_META_ATTRIBUTE);
		if (x == IndexNull) return 0;
		ra = r->meta_attribute[x];
		
		if (dsattribute_match(ra, pa) == 0) return 0;
	}

	return 1;
}

/*
 * Match just attributes or just meta-attributes
 */
int
dsrecord_match_select(dsrecord *r, dsrecord *pattern, u_int32_t asel)
{
	u_int32_t i, x, count;
	dsattribute *pa, *ra;

	if (r == pattern) return 1;
	if (r == NULL) return 0;
	if (pattern == NULL) return 1;
	
	/* check each (meta)attribute in the pattern */
	count = 0;
	if (asel == SELECT_ATTRIBUTE) count = pattern->count;
	else count = pattern->meta_count;

	for (i = 0; i < count; i++)
	{
		pa = NULL;
		if (asel == SELECT_ATTRIBUTE) pa = pattern->attribute[i];
		else pa = pattern->meta_attribute[i];

		x = dsrecord_attribute_index(r, pa->key, asel);
		if (x == IndexNull) return 0;

		ra = NULL;
		if (asel == SELECT_ATTRIBUTE) ra = r->attribute[x];
		else ra = r->meta_attribute[x];

		if (dsattribute_match(ra, pa) == 0) return 0;
	}

	return 1;
}

int
dsrecord_equal(dsrecord *a, dsrecord *b)
{
	u_int32_t i, x;
	dsattribute *aa, *ba;

	if (a == b) return 1;

	if (a == NULL) return 0;
	if (b == NULL) return 0;

	if (a->count != b->count) return 0;

	/* check each attribute in a */
	for (i = 0; i < a->count; i++)
	{
		aa = a->attribute[i];
		x = dsrecord_attribute_index(b, aa->key, SELECT_ATTRIBUTE);
		if (x == IndexNull) return 0;
		ba = b->attribute[x];
		
		if (dsattribute_equal(aa, ba) == 0) return 0;
	}
	
	/* check each meta-attribute in a */
	for (i = 0; i < a->meta_count; i++)
	{
		aa = a->meta_attribute[i];
		x = dsrecord_attribute_index(b, aa->key, SELECT_META_ATTRIBUTE);
		if (x == IndexNull) return 0;
		ba = b->meta_attribute[x];
		
		if (dsattribute_equal(aa, ba) == 0) return 0;
	}

	return 1;
}
