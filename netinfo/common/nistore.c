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

#include <NetInfo/nistore.h>
#include <NetInfo/nilib2.h>
#include <netinfo/ni.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_TIMEOUT 15

static dsstatus
nitodsstatus(ni_status ns)
{
	if (ns == NI_OK) return DSStatusOK;
	if (ns == NI_BADID) return DSStatusInvalidRecordID;
	if (ns == NI_STALE) return DSStatusStaleRecord;
	if (ns == NI_NOSPACE) return DSStatusWriteFailed;
	if (ns == NI_PERM) return DSStatusAccessRestricted;
	if (ns == NI_NODIR) return DSStatusInvalidPath;
	if (ns == NI_NOPROP) return DSStatusInvalidKey;
	if (ns == NI_NONAME) return DSStatusNoData;
	if (ns == NI_RDONLY) return DSStatusWriteFailed;
	if (ns == NI_AUTHERROR) return DSStatusAccessRestricted;
	if (ns == NI_NOUSER) return DSStatusAccessRestricted;
	return DSStatusFailed;
}

static char *
dsdatatostring(dsdata *d)
{
	char *s;

	if (d == NULL) return NULL;
	s = malloc(d->length + 1);
	memmove(s, d->data, d->length);
	s[d->length] = '\0';
	return s;
}

static char *
dsmetadatatostring(dsdata *d)
{
	char *s;

	if (d == NULL) return NULL;
	s = malloc(d->length + 2);
	memmove(s + 1, d->data, d->length);
	s[0] = '_';
	s[d->length + 1] = '\0';
	return s;
}

static ni_property *
dsattribute_to_property(dsattribute *a, u_int32_t asel)
{
	ni_property *p;
	int i, len;

	if (a == NULL) return NULL;

	p = (ni_property *)malloc(sizeof(ni_property));
	NI_INIT(p);

	if (asel == SELECT_ATTRIBUTE)
		p->nip_name = dsdatatostring(a->key);
	else
		p->nip_name = dsmetadatatostring(a->key);

	len = a->count;
	p->nip_val.ni_namelist_len = len;
	p->nip_val.ni_namelist_val = NULL;

	if (len > 0)
		p->nip_val.ni_namelist_val = (ni_name *)malloc(len * sizeof(ni_name));

	for (i = 0; i < len; i++)
	{
		p->nip_val.ni_namelist_val[i] = dsdatatostring(a->value[i]);
	}

	return p;
}

static ni_proplist *
dsrecord_to_ni_proplist(dsrecord *r)
{
	ni_proplist *pl;
	int i, j, a, len;
	ni_property *p;

	if (r == NULL) return NULL;
	pl = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(pl);

	len = r->count + r->meta_count;

	pl->ni_proplist_len = len;
	pl->ni_proplist_val = NULL;
	if (len > 0)
		pl->ni_proplist_val =
			(ni_property *)malloc(len * sizeof(ni_property));

	i = 0;
	for (a = 0; a < r->count; a++, i++)
	{
		p = &(pl->ni_proplist_val[i]);
		p->nip_name = dsdatatostring(r->attribute[a]->key);

		len = r->attribute[a]->count;
		p->nip_val.ni_namelist_len = len;
		p->nip_val.ni_namelist_val = NULL;
		if (len > 0)
			p->nip_val.ni_namelist_val =
				(ni_name *)malloc(len * sizeof(ni_name));

		for (j = 0; j < len; j++)
		{
			p->nip_val.ni_namelist_val[j] = dsdatatostring(r->attribute[a]->value[j]);
		}
	}
	
	for (a = 0; a < r->meta_count; a++, i++)
	{
		p = &(pl->ni_proplist_val[i]);
		p->nip_name = dsmetadatatostring(r->meta_attribute[a]->key);

		len = r->meta_attribute[a]->count;
		p->nip_val.ni_namelist_len = len;
		p->nip_val.ni_namelist_val = NULL;
		if (len > 0)
			p->nip_val.ni_namelist_val =
				(ni_name *)malloc(len * sizeof(ni_name));

		for (j = 0; j < len; j++)
		{
			p->nip_val.ni_namelist_val[j] = dsdatatostring(r->meta_attribute[a]->value[j]);
		}
	}

	return pl;	
}

void
dsrecord_insert_ni_proplist(dsrecord *r, ni_proplist *p)
{
	int pn, vn, x, mx, len;
	ni_property prop;
	ni_namelist values;
	dsattribute *a;
	char *s;

	for (pn = 0; pn < p->ni_proplist_len; pn++)
	{
		if (p->ni_proplist_val[pn].nip_name[0] == '_')
			 r->meta_count++;
	}

	r->count = p->ni_proplist_len - r->meta_count;

	if (r->count > 0)
		r->attribute = (dsattribute **)malloc(r->count * sizeof(dsattribute *));

	if (r->meta_count > 0)
		r->meta_attribute = (dsattribute **)malloc(r->meta_count * sizeof(dsattribute *));

	x = 0;
	mx = 0;

	/* for each property */
	for (pn = 0; pn < p->ni_proplist_len; pn++)
	{
		prop = p->ni_proplist_val[pn];
		len = strlen(prop.nip_name);

		if (prop.nip_name[0] == '_')
		{
			len--;
			s = prop.nip_name + 1;
			r->meta_attribute[mx] = (dsattribute *)malloc(sizeof(dsattribute));
			a = r->meta_attribute[mx];
			mx++;
		}
		else
		{
			s = prop.nip_name;
			r->attribute[x] = (dsattribute *)malloc(sizeof(dsattribute));
			a = r->attribute[x];
			x++;
		}

		a->retain = 1;

		a->key = cstring_to_dsdata(s);
		values = prop.nip_val;

		len = values.ni_namelist_len;
		a->count = len;
		a->value = NULL;

		if (len > 0) a->value = (dsdata **)malloc(len * sizeof(dsdata *));

		/* for each value in the namelist for this property */
		for (vn = 0; vn < len; vn++)
		{
			a->value[vn] = cstring_to_dsdata(values.ni_namelist_val[vn]);
		}
	}
}

dsstatus
nistore_open(dsstore **s, char *dname, u_int32_t flags)
{
	void *d;
	ni_status status;
	int timeout, by_tag;

	timeout = DEFAULT_TIMEOUT;
	by_tag = flags & DSSTORE_FLAGS_OPEN_BY_TAG;

	status = do_open("nicl", dname, &d, by_tag, timeout, NULL, NULL);
	if (status != NI_OK) return DSStatusInvalidStore;

	*s = (dsstore *)malloc(sizeof(dsstore));
	memset(*s, 0, sizeof(dsstore));

	(*s)->flags = flags;
	(*s)->index = (void **)malloc(sizeof(void *));
	(*s)->index[0] = d;

	return DSStatusOK;
}

dsstatus
nistore_close(dsstore *s)
{
	if (s == NULL) return DSStatusInvalidStore;

	if (s->index != NULL)
	{
		ni_free(s->index[0]);
		free(s->index);
	}

	free(s);
	return DSStatusOK;
}

dsstatus
nistore_authenticate(dsstore *s, dsdata *user, dsdata *password)
{
	char *x;

	if (s == NULL) return DSStatusInvalidStore;

	x = dsdata_to_cstring(user);
	ni_setuser(s->index[0], x);
	
	x = dsdata_to_cstring(password);
	ni_setpassword(s->index[0], x);

	return DSStatusOK;
}

dsrecord *
nistore_fetch(dsstore *s, u_int32_t dsid)
{
	int x;
	ni_proplist plist;
	ni_idlist kids;
	ni_status status;
	dsrecord *r;
	ni_id nid;

	if (s == NULL) return NULL;
	
	nid.nii_object = dsid;
	nid.nii_instance = -1;

	/* get the property list stored in the this directory */
	NI_INIT(&plist);
	status = ni_read(s->index[0], &nid, &plist);
	if (status != NI_OK) return NULL;

	r = dsrecord_new();

	r->dsid = nid.nii_object;
	r->serial = nid.nii_instance;

	/* Try ni_self() to get r->version */
	nid.nii_object = -2;
	nid.nii_instance = dsid;
	
	status = ni_self(s->index[0], &nid);
	if ((status == NI_OK) && (nid.nii_object == dsid))
		r->vers = nid.nii_instance;

	dsrecord_insert_ni_proplist(r, &plist);

	ni_proplist_free(&plist);

	nid.nii_object = dsid;
	nid.nii_instance = r->serial;

	status = ni_parent(s->index[0], &nid, (ni_index *)&(r->super));
	if (status != NI_OK)
	{
		dsrecord_release(r);
		return NI_OK;
	}

	NI_INIT(&kids);
	status = ni_children(s->index[0], &nid, &kids);
	if (status != NI_OK)
	{
		dsrecord_release(r);
		return NI_OK;
	}
	
	r->sub_count = kids.ni_idlist_len;
	if (r->sub_count > 0)
	{
		r->sub = (u_int32_t *)malloc(r->sub_count * sizeof(u_int32_t));
		for (x = 0; x < r->sub_count; x++) r->sub[x] = kids.ni_idlist_val[x];
	}
	ni_idlist_free(&kids);

	return r;
}

dsstatus
nistore_save(dsstore *s, dsrecord *r)
{
	ni_proplist *p;
	ni_status status;
	ni_id n, parent;

	if (s == NULL) return DSStatusInvalidStore;
	p = dsrecord_to_ni_proplist(r);
	if (p == NULL) return DSStatusInvalidRecord;

	if (r->dsid == IndexNull)
	{
		n.nii_object = 0;
		n.nii_instance = 0;
		parent.nii_object = r->super;
		status = ni_self(s->index[0], &parent);
		if (status == NI_OK)
		{
			status = ni_create(s->index[0], &parent, *p, &n, NI_INDEX_NULL);
			if (status == NI_OK)
			{
				r->dsid = n.nii_object;	
				status = ni_self(s->index[0], &n);
				if (status == NI_OK) r->serial = n.nii_instance;
			}
		}
	}
	else 
	{
		n.nii_object = r->dsid;
		n.nii_instance = r->serial;
		status = ni_write(s->index[0], &n, *p);
		if (status == NI_OK)
		{
			status = ni_self(s->index[0], &n);
			if (status == NI_OK) r->serial = n.nii_instance;
		}
	}

	ni_proplist_free(p);
	return nitodsstatus(status);
}

dsstatus
nistore_save_attribute(dsstore *s, dsrecord *r, dsattribute *a, u_int32_t asel)
{
	ni_status status;
	ni_id n;
	ni_proplist pl;
	ni_property *p;
	ni_index where;
	
	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = r->dsid;
	NI_INIT(&pl);
	status = ni_read(s->index[0], &n, &pl);
	if (status != NI_OK) return nitodsstatus(status);

	p = dsattribute_to_property(a, asel);
	if (p == NULL)
	{
		ni_proplist_free(&pl);
		return DSStatusWriteFailed;
	}

	where = ni_proplist_match(pl, p->nip_name, NULL);
	ni_proplist_free(&pl);

	if (where == NI_INDEX_NULL)
		status = ni_createprop(s->index[0], &n, *p, where);
	else
		status = ni_writeprop(s->index[0], &n, where, p->nip_val);
	ni_prop_free(p);
	
	return nitodsstatus(status);
}

dsstatus
nistore_remove(dsstore *s, u_int32_t dsid)
{
	ni_status status;
	ni_id n, parent;

	if (s == NULL) return DSStatusInvalidStore;
	
	n.nii_object = dsid;
	status = ni_parent(s->index[0], &n, &parent.nii_object);
	if (status != NI_OK) return nitodsstatus(status);
	
	status = ni_self(s->index[0], &parent);
	if (status != NI_OK) return nitodsstatus(status);

	status = ni_destroy(s->index[0], &parent, n);
	return nitodsstatus(status);
}

u_int32_t
nistore_version(dsstore *s)
{
	ni_id n;
	ni_status status;

	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = -1;
	n.nii_instance = -1;

	status = ni_self(s->index[0], &n);
	if (status != NI_OK) return IndexNull;
	return n.nii_instance;
}

u_int32_t
nistore_version_record(dsstore *s, u_int32_t vers)
{
	ni_id n;
	ni_status status;

	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = -1;
	n.nii_instance = vers;

	if (vers == -1)
	{
		status = ni_self(s->index[0], &n);
		if (status != NI_OK) return IndexNull;

		vers = n.nii_instance;
	}
	
	status = ni_self(s->index[0], &n);
	if (status != NI_OK) return IndexNull;

	return n.nii_object;
}
	
u_int32_t
nistore_record_super(dsstore *s, u_int32_t dsid)
{
	ni_id n;
	u_int32_t pdsid;
	ni_status status;

	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = dsid;
	status = ni_parent(s->index[0], &n, (ni_index *)&pdsid);
	if (status != NI_OK) return IndexNull;
	return pdsid;
}

u_int32_t
nistore_record_serial(dsstore *s, u_int32_t dsid)
{
	ni_id n;
	ni_status status;

	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = dsid;
	status = ni_self(s->index[0], &n);
	if (status != NI_OK) return IndexNull;
	return n.nii_instance;
}

u_int32_t
nistore_record_version(dsstore *s, u_int32_t dsid)
{
	ni_id n;
	ni_status status;

	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = -2;
	n.nii_instance = dsid;
	status = ni_self(s->index[0], &n);
	if (status != NI_OK) return IndexNull;
	return n.nii_instance;
}

dsstatus
nistore_vital_statistics(dsstore *s, u_int32_t dsid, u_int32_t *vers, u_int32_t *serial, u_int32_t *super)
{
	ni_id n;
	u_int32_t pdsid;
	ni_status status;

	if (s == NULL) return DSStatusInvalidStore;

	n.nii_object = -2;
	n.nii_instance = dsid;
	status = ni_self(s->index[0], &n);
	if (status != NI_OK) *vers = IndexNull;
	else *vers = n.nii_instance;

	n.nii_object = dsid;
	status = ni_self(s->index[0], &n);
	if (status != NI_OK) *serial = IndexNull;
	else *serial = n.nii_instance;

	n.nii_object = dsid;
	status = ni_parent(s->index[0], &n, (ni_index *)&pdsid);
	if (status != NI_OK) *super = pdsid;
	else *super = pdsid;

	return DSStatusOK;
}

dsstatus
nistore_list(dsstore *s, u_int32_t dsid, dsdata *key, u_int32_t asel, dsrecord **list)
{
	ni_id n;
	ni_entrylist el;
	ni_status status;
	int i, j;
	dsdata *d, *k;
	dsattribute *a;

	n.nii_object = dsid;
	n.nii_instance = 0;

	k = key;
	if (k == NULL) k = cstring_to_dsdata("name");

	NI_INIT(&el);
	status = ni_list(s->index[0], &n, dsdata_to_cstring(k), &el);
	if (status != NI_OK)
	{
	    if (key == NULL) dsdata_release(k);
	    return nitodsstatus(status);
	}

	*list = dsrecord_new();
	d = cstring_to_dsdata("key");
	a = dsattribute_new(d);
	dsattribute_append(a, k);
	dsrecord_append_attribute(*list, a, SELECT_META_ATTRIBUTE);
	dsdata_release(d);
	dsattribute_release(a);
	if (key == NULL) dsdata_release(k);

	for (i = 0; i < el.ni_entrylist_len; i++)
	{
		if (el.ni_entrylist_val[i].names == NULL) continue;

		d = int32_to_dsdata(el.ni_entrylist_val[i].id);
		a = dsattribute_new(d);
		dsdata_release(d);
		dsrecord_append_attribute(*list, a, SELECT_ATTRIBUTE);

		a->count = el.ni_entrylist_val[i].names->ni_namelist_len;
		if (a->count > 0)
			a->value = (dsdata **)malloc(a->count * sizeof(dsdata *));

		for (j = 0; j < a->count; j++)
			a->value[j] = cstring_to_dsdata(el.ni_entrylist_val[i].names->ni_namelist_val[j]);

		dsattribute_release(a);
	}

	ni_entrylist_free(&el);
	return DSStatusOK;
}

dsstatus
nistore_match(dsstore *s, u_int32_t dsid, dsdata *key, dsdata *val, u_int32_t asel, u_int32_t *match)
{
	ni_id n;
	ni_idlist idl;
	ni_status status;
	char *ckey, *cval;

	if (s == NULL) return DSStatusInvalidStore;
	ckey = dsdata_to_cstring(key);
	if (ckey == NULL) return DSStatusInvalidKey;
	cval = dsdata_to_cstring(val);
	if (cval == NULL) return DSStatusNoData;

	*match = IndexNull;
	NI_INIT(&idl);
	n.nii_object = dsid;
	status = ni_lookup(s->index[0], &n, ckey, cval, &idl);
	if (status == NI_NODIR) return DSStatusOK;
	if (status != NI_OK) return nitodsstatus(status);

	if (idl.ni_idlist_len > 0) *match = idl.ni_idlist_val[0];
	ni_idlist_free(&idl);

	return DSStatusOK;
}

dsrecord *
nistore_statistics(dsstore *s)
{
	ni_status status;
	ni_proplist pl;
	dsrecord *r;

	if (s == NULL) return NULL;

	NI_INIT(&pl);
	status = ni_statistics(s->index[0], &pl);
	if (status != NI_OK) return NULL;

	r = dsrecord_new();
	dsrecord_insert_ni_proplist(r, &pl);
	ni_proplist_free(&pl);
	return r;
}
