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

/*
 * Shim between old ni_file handling and new dsstore.
 * Copyright (C) 1999 by Apple Computer, Inc.
 * Written by Marc Majka.
 */

#include <NetInfo/config.h>
#include <netinfo/ni.h>
#include <string.h>
#include <stdlib.h>
#include <notify.h>
#include <sys/param.h>
#include <sys/dir.h>
#include "ni_file.h"
#include <NetInfo/dsstore.h>
#include <NetInfo/system_log.h>
#include "ni_globals.h"

#define STORE(X) ((dsstore *)X)

/*
 * N.B. This is also defined in common/dsengine.c
 */
#define ORDERLIST_KEY "ni_attribute_order"

ni_status
dstonistatus(dsstatus s)
{
	if (s == DSStatusOK) return NI_OK;
	return NI_SYSTEMERR;
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

static ni_object *
dstoni(dsrecord *r)
{
	ni_object *o;
	int i, j, a, len, ix;
	ni_property *p;
	dsattribute *orderlist;
	dsdata *odata;

	if (r == NULL) return NULL;
	o = (ni_object *)malloc(sizeof(ni_object));

	o->nio_id.nii_object = r->dsid;
	o->nio_id.nii_instance = r->serial;

	o->nio_parent = r->super;

	len = r->sub_count;
	o->nio_children.ni_idlist_len = len;
	o->nio_children.ni_idlist_val = NULL;
	if (len > 0)
		o->nio_children.ni_idlist_val = (long *)malloc(len * sizeof(long));
	for (i = 0; i < len; i++)
		o->nio_children.ni_idlist_val[i] = r->sub[i];

	len = r->count + r->meta_count;

	odata = cstring_to_dsdata(ORDERLIST_KEY);
	orderlist = dsrecord_attribute(r, odata, SELECT_META_ATTRIBUTE);
	dsdata_release(odata);

	if (orderlist != NULL)
	{
		len--;
		r->meta_count--;
	}

	o->nio_props.ni_proplist_len = len;
	o->nio_props.ni_proplist_val = NULL;
	if (len > 0)
		o->nio_props.ni_proplist_val =
			(ni_property *)malloc(len * sizeof(ni_property));

	i = 0;
	for (a = 0; a < r->count; a++, i++)
	{
		ix = i;
		if (orderlist != NULL)
		{
			odata = dsattribute_value(orderlist, i);
			ix = dsdata_to_int32(odata);
			dsdata_release(odata);
		}

		p = &(o->nio_props.ni_proplist_val[ix]);
		p->nip_name = dsdatatostring(r->attribute[a]->key);

		len = r->attribute[a]->count;
		p->nip_val.ni_namelist_len = len;
		p->nip_val.ni_namelist_val = NULL;
		if (len > 0)
			p->nip_val.ni_namelist_val =
				(ni_name *)malloc(len * sizeof(ni_name));

		for (j = 0; j < r->attribute[a]->count; j++)
		{
			p->nip_val.ni_namelist_val[j] = dsdatatostring(r->attribute[a]->value[j]);
		}
	}
	
	for (a = 0; a < r->meta_count; a++, i++)
	{
		ix = i;
		if (orderlist != NULL)
		{
			odata = dsattribute_value(orderlist, i);
			ix = dsdata_to_int32(odata);
			dsdata_release(odata);
		}

		p = &(o->nio_props.ni_proplist_val[ix]);
		p->nip_name = dsmetadatatostring(r->meta_attribute[a]->key);

		len = r->meta_attribute[a]->count;
		p->nip_val.ni_namelist_len = len;
		p->nip_val.ni_namelist_val = NULL;
		if (len > 0)
			p->nip_val.ni_namelist_val =
				(ni_name *)malloc(len * sizeof(ni_name));

		for (j = 0; j < r->meta_attribute[a]->count; j++)
		{
			p->nip_val.ni_namelist_val[j] = dsdatatostring(r->meta_attribute[a]->value[j]);
		}
	}

	if (orderlist != NULL)
	{
		r->meta_count++;
		dsattribute_release(orderlist);
	}

	return o;	
}

static dsrecord *
nitods(ni_object *n)
{
	int i, pn, vn, x, mx, len;
	ni_property prop;
	ni_namelist values;
	dsrecord *r;
	dsattribute *a;
	dsattribute *orderlist;
	dsdata *odata;
	u_int32_t where, ix, embedded;
	char *s;

	if (n == NULL) return NULL;

	r = dsrecord_new();

	r->dsid = n->nio_id.nii_object;
	r->serial = n->nio_id.nii_instance;
	r->vers = 0;

	r->super = n->nio_parent;

	len = n->nio_children.ni_idlist_len;
	r->sub_count = len;
	if (len > 0)
		r->sub = (u_int32_t *)malloc(len * sizeof(u_int32_t));

	for (i = 0; i < len; i++)
		r->sub[i] = n->nio_children.ni_idlist_val[i];

	/*
	 * orderlist is a meta-attribute we use to retain property order.
	 * This is required for ni_writeprop and other API that indexes
	 * into the proplist.  We only need this if there are meta-atributes
	 * which appear "embedded" in front of regular attributes.  We keep
	 * track of what goes where so that we can re-construct the original
	 * ordering in dstoni().
	 */

	odata = cstring_to_dsdata(ORDERLIST_KEY);
	orderlist = dsattribute_new(odata);
	dsdata_release(odata);

	ix = 0;
	where = 0;
	embedded = 0;

	for (pn = 0; pn < n->nio_props.ni_proplist_len; pn++)
	{
		if (n->nio_props.ni_proplist_val[pn].nip_name[0] == '_')
		{
			where = IndexNull;
			r->meta_count++;
		}
		else
		{
			if (r->meta_count > 0) embedded = 1;
			where = ix;
			ix++;
		}
		
		odata = int32_to_dsdata(pn);
		dsattribute_insert(orderlist, odata, where);
		dsdata_release(odata);
	}

	r->count = n->nio_props.ni_proplist_len - r->meta_count;

	if (r->count > 0)
		r->attribute = (dsattribute **)malloc(r->count * sizeof(dsattribute *));

	if (r->meta_count > 0)
	{
		r->meta_count += embedded;
		r->meta_attribute = (dsattribute **)malloc(r->meta_count * sizeof(dsattribute *));
	}

	x = 0;
	mx = 0;

	/* for each property */
	for (pn = 0; pn < n->nio_props.ni_proplist_len; pn++)
	{
		prop = n->nio_props.ni_proplist_val[pn];
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
		for (vn = 0; vn < values.ni_namelist_len; vn++)
		{
			len = strlen(values.ni_namelist_val[vn]);
			a->value[vn] = cstring_to_dsdata(values.ni_namelist_val[vn]);
		}
	}

	if (embedded) r->meta_attribute[mx] = orderlist;
	else dsattribute_release(orderlist);

	return r;
}

static int
_read_int(FILE *f, int *v)
{
	int status, x;

	status = fread(&x, 4, 1, f);
	if (status != 1) return -1;

	*v = ntohl(x);
	return 0;
}

static dsrecord *
_convert_read(FILE *f)
{
	unsigned int i, j, x, pad, n, v;
	int status;
	char junk[4], *s;
	dsrecord *r;
 	dsattribute *a;
	dsattribute *orderlist;
	dsdata *odata;
	unsigned int where, ix, embedded;

	if (f == NULL) return NULL;

	r = dsrecord_new();

	if (_read_int(f, &(r->dsid)) < 0)
	{
		dsrecord_release(r);
		return NULL;
	}

	if (r->dsid == (u_int32_t)-1)
	{
		dsrecord_release(r);
		return NULL;
	}
	
#ifdef DEBUG
	fprintf(stderr, "Convert DSID %u\n", r->dsid);
#endif

	if (_read_int(f, &(r->serial)) < 0)
	{
		dsrecord_release(r);
		return NULL;
	}

	if (_read_int(f, &n) < 0)
	{
		dsrecord_release(r);
		return NULL;
	}

	if (n > 0)
	{
		r->attribute = (dsattribute **)malloc(n * sizeof(dsattribute *));
		r->meta_attribute = (dsattribute **)malloc((n + 1) * sizeof(dsattribute *));
	}

	/*
	 * orderlist is a meta-attribute we use to retain property order.
	 * See the comment in nitods().
	 */

	odata = cstring_to_dsdata(ORDERLIST_KEY);
	orderlist = dsattribute_new(odata);
	dsdata_release(odata);

	ix = 0;
	where = 0;
	embedded = 0;

	for (i = 0; i < n; i++)
	{
		/* Key Length */
		if (_read_int(f, &x) < 0)
		{
			dsattribute_release(orderlist);
			dsrecord_release(r);
			return NULL;
		}

		/* Key */
		s = malloc(x + 1);
		if (x > 0) 
		{
			status = fread(s, x, 1, f);
			if (status != 1)
			{
				free(s);
				dsattribute_release(orderlist);
				dsrecord_release(r);
				return NULL;
			}
		}

		s[x] = '\0';
		pad = x % 4;
		if (pad != 0)
		{
			pad = 4 - pad;
			status = fread(junk, pad, 1, f);
			if (status != 1)
			{
				free(s);
				dsattribute_release(orderlist);
				dsrecord_release(r);
				return NULL;
			}
		}

		if (s[0] == '_')
		{
			a = dsattribute_new(cstring_to_dsdata(s+1));
			r->meta_attribute[r->meta_count] = a;
			r->meta_count++;
			where = IndexNull;
		}
		else
		{
			a = dsattribute_new(cstring_to_dsdata(s));
			r->attribute[r->count] = a;
			r->count++;
			where = ix;
			ix++;
			if (r->meta_count > 0) embedded = 1;
		}

		odata = int32_to_dsdata(i);
		dsattribute_insert(orderlist, odata, where);	
		dsdata_release(odata);

#ifdef DEBUG
		fprintf(stderr, "        %s:", s);
#endif
		free(s);

		/* Value Count */
		if (_read_int(f, &v) < 0)
		{
#ifdef DEBUG
			fprintf(stderr, "\n**** Bad value count\n");
#endif
			dsattribute_release(orderlist);
			dsrecord_release(r);
			return NULL;
		}

		for (j = 0; j < v; j++)
		{
			/* Value Length */
			if (_read_int(f, &x) < 0)
			{
#ifdef DEBUG
				fprintf(stderr, "\n**** Bad value length\n");
#endif
				dsattribute_release(orderlist);
				dsrecord_release(r);
				return NULL;
			}

			/* Value */
			s = malloc(x + 1);
			if (x == 0) status = 1;
			else status = fread(s, x, 1, f);
			if (status != 1)
			{
#ifdef DEBUG
				fprintf(stderr, "\n**** Bad value\n");
#endif
				free(s);
				dsattribute_release(orderlist);
				dsrecord_release(r);
				return NULL;
			}

			s[x] = '\0';
			dsattribute_append(a, cstring_to_dsdata(s));
#ifdef DEBUG
			fprintf(stderr, " %s", s);
#endif
			free(s);

			pad = x % 4;
			if (pad != 0)
			{
				pad = 4 - pad;
				status = fread(junk, pad, 1, f);
				if (status != 1)
				{
					dsattribute_release(orderlist);
					dsrecord_release(r);
					return NULL;
				}
			}
		}
#ifdef DEBUG
		fprintf(stderr, "\n");
#endif
	}

	if (n > 0)
	{
		if (r->count == 0)
		{
			free(r->attribute);
			r->attribute = NULL;
		}
		else
		{
			r->attribute = (dsattribute **)realloc(r->attribute, r->count * sizeof(dsattribute *));
		}

		if (r->meta_count == 0)
		{
			free(r->meta_attribute);
			r->meta_attribute = NULL;
		}
		else
		{
			r->meta_count += embedded;
			r->meta_attribute = (dsattribute **)realloc(r->meta_attribute, r->meta_count * sizeof(dsattribute *));
			if (embedded != 0) r->meta_attribute[r->meta_count - 1] = dsattribute_retain(orderlist);
		}
	}

	dsattribute_release(orderlist);

	/* Parent ID */
	if (_read_int(f, &(r->super)) < 0)
	{
		dsrecord_release(r);
		return NULL;
	}
#ifdef DEBUG
	fprintf(stderr, "       Parent DSID %u\n", r->super);
#endif

	/* Child Count */
	if (_read_int(f, &(r->sub_count)) < 0)
	{
		dsrecord_release(r);
		return NULL;
	}

	if (r->sub_count > 0)
		r->sub = (u_int32_t *)malloc(r->sub_count * sizeof(u_int32_t));

	for (i = 0; i < r->sub_count; i++)
	{
		if (_read_int(f, &(r->sub[i])) < 0)
		{
			dsrecord_release(r);
			return NULL;
		}
#ifdef DEBUG
		fprintf(stderr, "       Child %u DSID %u\n", i, r->sub[i]);
#endif
	}

	return r;
}

static char *
_extension_name(void *hdl, u_int32_t nid)
{
	static char name[MAXPATHLEN + 1];

	sprintf(name, "%s/extension_%u", file_dirname(hdl), nid);
	return name;
}

static dsrecord *
_convert_entry(void *hdl, u_int32_t nid, FILE *cf, int size)
{
	FILE *f;
	dsrecord *r;

	/* Check for an extension file for this record */
	f = fopen(_extension_name(hdl, nid), "r");
	if (f == NULL)
	{
		f = cf;

		/* Skip forward to the desired record in the collection file */
		fseek(f, nid * size, SEEK_SET);
	}

	r = _convert_read(f);

	if (f != cf)
	{
		fclose(f);
		/* XXX unlink(_extension_name(hdl, nid)); */
	}

	return r;
}

ni_status
file_init(char *rootdir, void **hdl)
{
	dsstore *s;
	dsstatus status;
	u_int32_t i, size, nrecords, flags, doconvert, tenth, pcnt;
	DIR *dp;
	struct direct *d;
	FILE *cf;
	char name[MAXPATHLEN + 1];
	dsrecord *r;

	doconvert = 1;
	size = 0;

	dp = opendir(rootdir);
	if (dp == NULL) return DSStatusReadFailed;

	while ((d = readdir(dp)))
	{
		if (!strcmp(d->d_name, "Config"))
		{
			doconvert = 0;
			break;
		}

		if (!strcmp(d->d_name, "Collection")) size = 512;
		else if (!strcmp(d->d_name, "collection")) size = 256;
	}

	closedir(dp);

	if (size == 0) doconvert = 0;

	flags = DSSTORE_FLAGS_ACCESS_READWRITE;
	flags |= DSSTORE_FLAGS_NOTIFY_CHANGES;
	if (!i_am_clone) flags |= DSSTORE_FLAGS_SERVER_MASTER;

	status = dsstore_new(&s, rootdir, flags);
	if (status != DSStatusOK) return NI_SYSTEMERR;

	*hdl = s;

	if (doconvert == 1)
	{
		if (size == 256) sprintf(name, "%s/collection", rootdir);
		else sprintf(name, "%s/Collection", rootdir);
		cf = fopen(name, "r");
		if (cf == NULL) return DSStatusReadFailed;

		fseek(cf, 0, SEEK_END);
		nrecords = ftell(cf) / size;
		tenth = 1;
		if (nrecords > 10) tenth = (nrecords + 5) / 10;
		pcnt = 0;

		system_log(LOG_NOTICE, "upgrading database format (%d records)...",
			nrecords);

		for (i = 0; i < nrecords; i++)
		{
			r = _convert_entry(*hdl, i, cf, size);
			if (r != NULL) 
			{
				dsstore_save_copy(s, r);
				dsrecord_release(r);
			}
			
			if (tenth == 1) system_log(LOG_NOTICE, "record %d", i);
			else if ((i % tenth) == 0)
			{
				if (pcnt > 0) system_log(LOG_NOTICE, "%d%% complete (%d records)", pcnt, i);
				pcnt += 10;
			}
		}
		fclose(cf);
		
		if ((tenth > 1) && (pcnt <= 100)) system_log(LOG_NOTICE, "100%%");
		system_log(LOG_NOTICE, "finished database upgrade");

		/* XXX unlink(name); */
	}

	return NI_OK;
}

void
file_renamedir(void *hdl, char *newname)
{
	unsigned int len;

	if (hdl == NULL) return;
	if (newname == NULL) return;

	free(STORE(hdl)->dsname);
	len = strlen(newname);
	STORE(hdl)->dsname = malloc(len + 1);
	memmove(STORE(hdl)->dsname, newname, len);
	STORE(hdl)->dsname[len] = '\0';
}

char *
file_dirname(void *hdl)
{
	if (hdl == NULL) return NULL;
	return STORE(hdl)->dsname;
}

void
file_free(void *hdl)
{
	if (hdl == NULL) return;
	dsstore_close(STORE(hdl));
}

void
file_shutdown(void *hdl, unsigned x)
{
	file_free(hdl);
}

ni_status
file_rootid(void *hdl, ni_id *idp)
{
	if (hdl == NULL) return NI_SYSTEMERR;
	idp->nii_object = 0;
	idp->nii_instance = dsstore_record_serial(STORE(hdl), 0);
	return NI_OK;
}

ni_status
file_idalloc(void *hdl, ni_id *idp)
{
	if (hdl == NULL) return NI_SYSTEMERR;
	idp->nii_object = -1;
	idp->nii_instance = -1;
	return NI_OK;
}

ni_status file_idunalloc(void *hdl, ni_id id)
{
	dsstatus status;

	if (hdl == NULL) return NI_SYSTEMERR;
	status = dsstore_remove(STORE(hdl), id.nii_object);

	return dstonistatus(status);
}

ni_status
file_read(void *hdl, ni_id *idp, ni_object **obj)
{
	dsrecord *r;
	u_int32_t dsid, vers;

	if (hdl == NULL) return NI_SYSTEMERR;
	if (idp == NULL) return NI_SYSTEMERR;
	if (obj == NULL) return NI_SYSTEMERR;

	r = NULL;

	/*
	 * We overload lookups to allow clients to use record versioning.
	 * This is only supported for ni_self.
	 *
	 * If the nii_object == -1, we find the record with
	 * version == nii_instance.  If nii_instance == -1,
	 * we find the record with the max version.
	 *
	 * If the nii_object == -2, we return the version
	 * for the record with dsid == nii_instance;
	 */
	if (idp->nii_object == -1)
	{
		vers = idp->nii_instance;
		if (vers == -1)
		{
			vers = dsstore_version(STORE(hdl));
			idp->nii_instance = vers;
		}

		dsid = dsstore_version_record(STORE(hdl), vers);
		if (dsid == -1) return NI_NODIR;

		r = dsrecord_new();
		r->dsid = dsid;
		r->serial = idp->nii_instance;
	}
	else if (idp->nii_object == -2)
	{
		r = dsrecord_new();
		r->dsid = idp->nii_instance;
		r->serial = dsstore_record_version(STORE(hdl), idp->nii_instance);
	}
	else 
	{
		r = dsstore_fetch(STORE(hdl), idp->nii_object);
		if (r == NULL) return NI_NODIR;
		idp->nii_instance = r->serial;
	}

	idp->nii_object = r->dsid;
	*obj = dstoni(r);
	dsrecord_release(r);
	return NI_OK;
}

ni_status
file_write(void *hdl, ni_object *obj)
{
	dsrecord *r;
	dsstatus status;

	if (hdl == NULL) return NI_SYSTEMERR;
	if (obj == NULL) return NI_SYSTEMERR;
	
	r = nitods(obj);
	if (r == NULL) return NI_NODIR;
	status = dsstore_save(STORE(hdl), r);
	if (status == DSStatusOK)
	{
		obj->nio_id.nii_object = r->dsid;
		obj->nio_id.nii_instance = r->serial;
	}

	dsrecord_release(r);
	return dstonistatus(status);
}

ni_status
file_writecopy(void *hdl, ni_object *obj)
{
	dsrecord *r;
	dsstatus status;

	if (hdl == NULL) return NI_SYSTEMERR;
	if (obj == NULL) return NI_SYSTEMERR;

	r = nitods(obj);
	if (r == NULL) return NI_NODIR;
	status = dsstore_save_copy(STORE(hdl), r);
	if (status == DSStatusOK) obj->nio_id.nii_instance = r->serial;
	dsrecord_release(r);

	return dstonistatus(status);
}

unsigned
file_getchecksum(void *hdl)
{
	unsigned c;
	
	if (hdl == NULL) return -1;
	c = dsstore_nichecksum(STORE(hdl));
	return c;
}

ni_index
file_highestid(void *hdl)
{
	ni_index x;

	if (hdl == NULL) return -1;
	x = dsstore_max_id(STORE(hdl));
	return x;
}

void
file_forget(void *hdl)
{
	dsstore_flush_cache(STORE(hdl));
}

ni_index
file_store_version(void* hdl)
{
	return dsstore_version(STORE(hdl));
}

ni_index
file_version(void *hdl, ni_id id)
{
	return dsstore_record_version(STORE(hdl), id.nii_object);
}

void
file_notify(void *hdl)
{
	dsstore_notify(STORE(hdl));
}

