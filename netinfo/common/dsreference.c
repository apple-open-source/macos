/*
 * Copyright (c) 2002 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2002 PADL Software Pty Ltd.  All Rights
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

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <NetInfo/dsreference.h>

extern void
serialize_32(u_int32_t v, char **p);

extern u_int32_t
deserialize_32(char **p);

extern dsdata *
deserialize_dsdata(char **p);

dsreference *dsreference_new(void)
{
	dsreference *r;

	r = (dsreference *)malloc(sizeof(dsreference));
	if (r == NULL) return NULL;

	r->retain = 1;

	r->dsid = -1;
	r->serial = 0;
	r->vers = -1;
	r->timestamp = 0;

	memset(&r->uuid, 0, sizeof(dsuuid_t));

	r->dn = NULL;
	r->name = NULL;

	return r;
}

dsreference *dsreference_copy(dsreference *r)
{
	dsdata *d;
	dsreference *x;

	if (r == NULL) return NULL;

	d = dsreference_to_dsdata(r);
	if (d == NULL) return NULL;

	x = dsdata_to_dsreference(d);
	dsdata_release(d);

	return x;
}

dsreference *dsreference_retain(dsreference *r)
{
	if (r == NULL) return NULL;
	r->retain++;
	return r;
}

void dsreference_release(dsreference *r)
{
	if (r == NULL) return;

	r->retain--;
	if (r->retain > 0) return;

	if (r->dn != NULL) dsdata_release(r->dn);
	if (r->name != NULL) dsdata_release(r->name);

	free(r);
}

u_int32_t
deserialize_16(char **p)
{
	u_int16_t n, t;

	memmove(&t, *p, 2);
	*p += 2;

	n = ntohs(t);
	return n;
}

void
serialize_16(u_int16_t v, char **p)
{
	u_int16_t n;

	n = htons(v);
	memmove(*p, &n, 2);
	*p += 2;
}

dsreference *dsdata_to_dsreference(dsdata *d)
{
	dsreference *r;
	char *p;

	if (d == NULL) return NULL;
	if (d->type != DataTypeDSReference) return NULL;

	r = (dsreference *)malloc(sizeof(dsreference));
	if (r == NULL) return NULL;

	r->retain = 1;

	p = d->data;

	r->dsid = deserialize_32(&p);
	r->serial = deserialize_32(&p);
	r->vers = deserialize_32(&p);
	r->timestamp = deserialize_32(&p);

	r->uuid.time_low = deserialize_32(&p);
	r->uuid.time_mid = deserialize_16(&p);
	r->uuid.time_hi_and_version = deserialize_16(&p);
	r->uuid.clock_seq_hi_and_reserved = *p;
	p++;
	r->uuid.clock_seq_low = *p;
	p++;

	memmove(&r->uuid.node, p, 6);
	p += 6;

	r->dn = deserialize_dsdata(&p);
	if (r->dn->length == 0)
	{
		dsdata_release(r->dn);
		r->dn = NULL;
	}

	r->name = deserialize_dsdata(&p);
	if (r->name->length == 0)
	{
		dsdata_release(r->name);
		r->name = NULL;
	}
	
	return r;
}

dsdata *dsreference_to_dsdata(dsreference *r)
{
	dsdata *d;
	char *p;
	u_int32_t len;

	if (r == NULL) return NULL;

	len = 0;

	/* dsid */
	len += 4;

	/* serial */
	len += 4;

	/* vers */
	len += 4;

	/* timestamp */
	len += 4;

	/* uuid */
	len += 16;

	/* DN length + type + data */
	len += (4 + 4 + r->dn->length);

	/* name length + type + data */
	len += (4 + 4 + r->name->length);

	d = dsdata_alloc(len);
	d->retain = 1;
	d->type = DataTypeDSReference;

	p = d->data;

	serialize_32(r->dsid, &p);
	serialize_32(r->serial, &p);
	serialize_32(r->vers, &p);
	serialize_32(r->timestamp, &p);
	serialize_32(r->uuid.time_low, &p);
	serialize_16(r->uuid.time_mid, &p);
	serialize_16(r->uuid.time_hi_and_version, &p);
	*p = r->uuid.clock_seq_hi_and_reserved;
	p++;
	*p = r->uuid.clock_seq_low;
	p++;

	memmove(p, &r->uuid.node, 6);
	p += 6;

	if (r->dn == NULL)
	{
		serialize_32(DataTypeCaseUTF8Str, &p);
		serialize_32(0, &p);
	}
	else
	{
		serialize_32(r->dn->type, &p);
		len = r->dn->length;
		serialize_32(len, &p);
		memmove(p, r->dn->data, len);
		p += len;
	}

	if (r->name == NULL)
	{
		serialize_32(DataTypeCaseUTF8Str, &p);
		serialize_32(0, &p);
	}
	else
	{
		serialize_32(r->name->type, &p);
		len = r->name->length;
		serialize_32(len, &p);
		memmove(p, r->name->data, len);
		p += len;
	}

	return d;
}

int dsreference_equal(dsreference *r1, dsreference *r2)
{
	/* Compare UUID first, most unique. */
	if (!IsNullUuid(r1->uuid) && !IsNullUuid(r2->uuid))
	{
		if (memcmp(&r1->uuid, &r2->uuid, sizeof(dsuuid_t)) != 0)
			return 0;
	}

	if (r1->dn != NULL && r2->dn != NULL)
	{
		if (!dsdata_equal(r1->dn, r2->dn))
			return 0;
	}

	if (r1->name != NULL && r2->name != NULL)
	{
		if (!dsdata_equal(r1->name, r2->name))
			return 0;
	}

	if (r1->dsid != r2->dsid)
		return 0;

	if (r1->serial != r2->serial)
		return 0;

	if (r1->vers != r2->vers)
		return 0;

	if (r1->timestamp != r2->timestamp)
		return 0;

	return 1;
}

/*
 * Read attributes from a dsrecord to construct a 
 * reference.
 */
dsreference *dsrecord_to_dsreference(dsrecord *r)
{
	dsreference *x;
	dsdata *k;
	dsattribute *a;
	dsdata *v;

	if (r == NULL) return NULL;

	x = dsreference_new();
	if (x == NULL) return NULL;

	x->dsid = r->dsid;
	x->vers = r->vers;
	x->serial = r->serial;

	k = casecstring_to_dsdata("entryUUID");
	a = dsrecord_attribute(r, k, SELECT_ATTRIBUTE);
	v = dsattribute_value(a, 0);
	if (v != NULL)
	{
		int i;

		i = sscanf(v->data, "%8lx-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x",
			(unsigned long *)&x->uuid.time_low,
			(unsigned int *)&x->uuid.time_mid,
			(unsigned int *)&x->uuid.time_hi_and_version,
			(unsigned int *)&x->uuid.clock_seq_hi_and_reserved,
			(unsigned int *)&x->uuid.clock_seq_low,
			(unsigned int *)&x->uuid.node[0],
			(unsigned int *)&x->uuid.node[1],
			(unsigned int *)&x->uuid.node[2],
			(unsigned int *)&x->uuid.node[3],
			(unsigned int *)&x->uuid.node[4],
			(unsigned int *)&x->uuid.node[5]);
		if (i != 11)
		{
			dsreference_release(x);
			return NULL;
		}
		dsdata_release(v);
	}
	dsattribute_release(a);
	dsdata_release(k);

	k = casecstring_to_dsdata("distinguishedName");
	a = dsrecord_attribute(r, k, SELECT_ATTRIBUTE);
	x->dn = dsattribute_value(a, 0);
	dsattribute_release(a);
	dsdata_release(k);

	k = casecstring_to_dsdata("name");
	a = dsrecord_attribute(r, k, SELECT_ATTRIBUTE);
	x->name = dsattribute_value(a, 0);
	dsattribute_release(a);
	dsdata_release(k);

	x->timestamp = time(NULL);

	return x;
}
