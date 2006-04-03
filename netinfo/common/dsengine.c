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

#include <NetInfo/dsengine.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/dsx500.h>
#include <NetInfo/dsindex.h>
#include <NetInfo/utf-8.h>

/*
 * N.B.  This is also defined in netinfod/ni_file.c
 */
#define ORDERLIST_KEY "ni_attribute_order"

/*
 * Creates and opens a new data store with the given pathname.
 */
dsstatus
dsengine_new(dsengine **s, char *name, u_int32_t flags)
{
	dsstatus status;
	dsstore *x;
	dsrecord *r;

	if (name == NULL) return DSStatusInvalidStore;

	status = dsstore_new(&x, name, flags);
	if (status != DSStatusOK) return status;

	*s = (dsengine *)malloc(sizeof(dsengine));
	(*s)->store = x;
	(*s)->delegate = NULL;
	(*s)->private = NULL;

	/* create root if necessary */
	r = dsstore_fetch(x, 0);
	if (r == NULL)
	{
		r = dsrecord_new();
		r->super = 0;

		status = dsstore_save((*s)->store, r);
	}
	
	dsrecord_release(r);
	return status;
}

/*
 * Opens a data store with the given pathname.
 */
dsstatus
dsengine_open(dsengine **s, char *name, u_int32_t flags)
{
	dsstatus status;
	dsstore *x;

	if (name == NULL) return DSStatusInvalidStore;

	status = dsstore_open(&x, name, flags);
	if (status != DSStatusOK) return status;

	*s = (dsengine *)malloc(sizeof(dsengine));
	(*s)->store = x;

	return status;
}

/*
 * Closes the data store.
 */
dsstatus
dsengine_close(dsengine *s)
{
	if (s == NULL) return DSStatusOK;
	if (s->store == NULL) return DSStatusInvalidStore;

	dsstore_close(s->store);
	free(s);

	return DSStatusOK;
}

/*
 * Autheticate.
 */
dsstatus
dsengine_authenticate(dsengine *s, dsdata *user, dsdata *password)
{
	if (s == NULL) return DSStatusOK;
	if (s->store == NULL) return DSStatusInvalidStore;
	return dsstore_authenticate(s->store, user, password);
}

/*
 * Detach a child (specified by its ID) from a record.
 * This just edits the record's list of children, and does not remove the
 * child record from the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus
dsengine_detach(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	/* Remove child's dsid */
	dsrecord_remove_sub(r, dsid);

	/* Reset parent's index */
	dsindex_free(r->index);
	r->index = NULL;

	status = dsstore_save(s->store, r);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Attach a child (specified by its ID) to a record.
 * This just edits the record's list of children, and does not create the
 * child record in the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus
dsengine_attach(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	/* Add child's dsid */
	dsrecord_append_sub(r, dsid);

	/* Reset parent's index */
	dsindex_free(r->index);
	r->index = NULL;

	status = dsstore_save(s->store, r);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Sets a record's parent.  This is for emergency repairs.
 */
dsstatus
dsengine_set_parent(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsstatus status;
	dsrecord *parent;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	parent = dsstore_fetch(s->store, dsid);
	if (parent == NULL) return DSStatusInvalidRecord;

	/* Reset parent's index */
	dsindex_free(parent->index);
	parent->index = NULL;

	dsrecord_release(parent);

	r->super = dsid;
	status = dsstore_save(s->store, r);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Adds a new record as a child of another record (specified by its ID).
 */
dsstatus
dsengine_create(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsrecord *parent;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	parent = dsstore_fetch(s->store, dsid);
	if (parent == NULL) return DSStatusInvalidPath;

	r->super = dsid;

	status = dsstore_save(s->store, r);
	if (status != DSStatusOK)
	{
		dsrecord_release(parent);
		return status;
	}

	dsrecord_append_sub(parent, r->dsid);
	dsindex_insert_record(parent->index, r);

	if (s->store->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		status = DSStatusOK;
	else
		status = dsstore_save(s->store, parent);

	dsrecord_release(parent);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Fetch a record specified by its ID.
 */
dsstatus
dsengine_fetch(dsengine *s, u_int32_t dsid, dsrecord **r)
{
	dsdata *key;

	if (s == NULL) return DSStatusInvalidStore;

	*r = dsstore_fetch(s->store, dsid);
	if (*r == NULL) return DSStatusInvalidPath;

	/*
	 * Remove the ordering meta-attribute - it is private to netinfod
	 * (which doesn't use dsengine).  If a client edits a record and
	 * saves it using dsengine_save or dsengine_save_attribute,
	 * the record switches to "native" ordering.
	 */
	key = cstring_to_dsdata(ORDERLIST_KEY);
	dsrecord_remove_key(*r, key, SELECT_META_ATTRIBUTE);
	dsdata_release(key);
	 
	return DSStatusOK;
}

/*
 * Fetch a list of records specified by ID.
 */
dsstatus
dsengine_fetch_list(dsengine *s, u_int32_t count, u_int32_t *dsid,
	dsrecord ***l)
{
	dsstatus status;
	u_int32_t i, j;
	dsrecord **list;

	if (s == NULL) return DSStatusInvalidStore;

	if ((count == 0) || (dsid == NULL))
	{
		*l = NULL;
		return DSStatusOK;
	}

	list = (dsrecord **)malloc(count * sizeof(dsrecord *));
	*l = list;

	status = DSStatusOK;
	for (i = 0; i < count; i++)
	{
		status = dsengine_fetch(s, dsid[i], &(list[i]));
		if (status != DSStatusOK) break;
	}

	if (status != DSStatusOK)
	{
		for (j = 0; j < i; j++) dsrecord_release(list[j]);
		free(list);
		*l = NULL;
		return status;
	}

	return DSStatusOK;
}

/*
 * Save a record.
 */
dsstatus
dsengine_save(dsengine *s, dsrecord *r)
{
	u_int32_t i;
	dsrecord *parent;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	i = dsstore_record_super(s->store, r->dsid);
	if (i == IndexNull) return DSStatusInvalidPath;

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		return DSStatusInvalidPath;
	}

	/* Update child in parent's index */
	dsindex_delete_dsid(parent->index, r->dsid);
	dsindex_insert_record(parent->index, r);

	dsrecord_release(parent);
	status = dsstore_save(s->store, r);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Save a record quickly.
 */
dsstatus
dsengine_save_fast(dsengine *s, dsrecord *r)
{
	u_int32_t i;
	dsrecord *parent;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	i = dsstore_record_super(s->store, r->dsid);
	if (i == IndexNull) return DSStatusInvalidPath;

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		return DSStatusInvalidPath;
	}

	/* Update child in parent's index */
	dsindex_delete_dsid(parent->index, r->dsid);
	dsindex_insert_record(parent->index, r);

	dsrecord_release(parent);
	status = dsstore_save_fast(s->store, r, 0);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Modify a record attribute. 
 */
dsstatus
dsengine_save_attribute(dsengine *s, dsrecord *r, dsattribute *a, u_int32_t asel)
{
	u_int32_t i;
	dsrecord *parent;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	i = dsstore_record_super(s->store, r->dsid);
	if (i == IndexNull) return DSStatusInvalidPath;

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		return DSStatusInvalidPath;
	}

	/* Update child in parent's index */
	dsindex_delete_dsid(parent->index, r->dsid);
	dsindex_insert_record(parent->index, r);

	dsrecord_release(parent);
	status = dsstore_save_attribute(s->store, r, a, asel);
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Remove a record from the data store.
 */
dsstatus
dsengine_remove(dsengine *s, u_int32_t dsid)
{
	u_int32_t i, n, *kids;
	dsrecord *r, *parent;
	dsstatus status;
	char *ns;

	if (s == NULL) return DSStatusInvalidStore;

	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusOK;
	
	/* PROTECT FROM MULTIPLE NOTIFICATIONS */
	ns = s->store->notification_name;
	s->store->notification_name = NULL;

	if (r->sub_count > 0)
	{
		n = r->sub_count;
		kids = (u_int32_t *)malloc(n * sizeof(u_int32_t));

		for (i = 0; i < n; i++) kids[i] = r->sub[i];

		for (i = 0; i < n; i++)
		{
			status = dsengine_remove(s, kids[i]);
			if (status != DSStatusOK)
			{
				free(kids);
				s->store->notification_name = ns;
				return status;
			}
		}

		free(kids);
	}

	i = dsstore_record_super(s->store, dsid);
	if (i == IndexNull)
	{
		s->store->notification_name = ns;
		return DSStatusInvalidPath;
	}

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		s->store->notification_name = ns;
		return DSStatusInvalidPath;
	}

	/* Remove the record from the parent's sub list */
	dsrecord_remove_sub(parent, dsid);

	/* Remove child from parent's index */
	dsindex_delete_dsid(parent->index, dsid);

	if (s->store->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		status = DSStatusOK;
	else
		status = dsstore_save(s->store, parent);

	dsrecord_release(parent);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to parent! */
		s->store->notification_name = ns;
		return status;
	}

	status = dsstore_remove(s->store, dsid);

	s->store->notification_name = ns;
	if (status == DSStatusOK) dsstore_notify(s->store);

	return status;
}

/*
 * Move a record to a new parent.
 */
dsstatus
dsengine_move(dsengine *s, u_int32_t dsid, u_int32_t pdsid)
{
	u_int32_t old;
	dsrecord *r, *child;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (dsid == 0) return DSStatusInvalidRecordID;
	
	child = dsstore_fetch(s->store, dsid);
	if (child == NULL) return DSStatusInvalidPath;
	if (child->super == pdsid)
	{
		dsrecord_release(child);
		return DSStatusOK;
	}

	old = child->super;
	child->super = pdsid;
	status = dsstore_save(s->store, child);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to record! */
		dsrecord_release(child);
		return status;
	}

	r = dsstore_fetch(s->store, old);
	if (r == NULL)
	{
		dsrecord_release(child);
		return DSStatusInvalidPath;
	}

	dsrecord_remove_sub(r, dsid);

	/* Remove child from original parent's index */
	dsindex_delete_dsid(r->index, dsid);

	status = dsstore_save(s->store, r);
	dsrecord_release(r);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to original parent! */
		dsrecord_release(child);
		return status;
	}

	r = dsstore_fetch(s->store, pdsid);
	if (r == NULL) return DSStatusInvalidPath;

	dsrecord_append_sub(r, dsid);

	/* Add child to new parent's index */
	dsindex_insert_record(r->index, child);
	dsrecord_release(child);

	status = dsstore_save(s->store, r);
	dsrecord_release(r);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to original parent! */
		return status;
	}

	dsstore_notify(s->store);

	return DSStatusOK;
}
	
/*
 * Copy a record to a new parent.
 */
dsstatus
dsengine_copy(dsengine *s, u_int32_t dsid, u_int32_t pdsid)
{
	dsrecord *r, *x;
	u_int32_t *kids, i, count;
	dsstatus status;
	char *ns;

	if (s == NULL) return DSStatusInvalidStore;
	
	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusInvalidPath;

	x = dsrecord_copy(r);
	dsrecord_release(r);

	count = x->sub_count;
	kids = x->sub;
	x->sub_count = 0;
	x->sub = NULL;

	/* PROTECT FROM MULTIPLE NOTIFICATIONS */
	ns = s->store->notification_name;
	s->store->notification_name = NULL;

	status = dsengine_create(s, x, pdsid);
	if (status != DSStatusOK)
	{
		/* XXX Can't add new record! */
		dsrecord_release(x);
		s->store->notification_name = ns;
		return status;
	}
		
	for (i = 0; i < count; i++)
	{
		status = dsengine_copy(s, kids[i], x->dsid);
		if (status != DSStatusOK)
		{
			/* XXX Can't add child! */
			dsrecord_release(x);
			s->store->notification_name = ns;
			return status;
		}
	}

	s->store->notification_name = ns;
	dsstore_notify(s->store);

	return DSStatusOK;
}

/*
 * Warning! count is overloaded - if we are searching for the
 * first match (dsengine_find_pattern()) then count is the output parameter
 * returning the dsid of the matching record.
 */
static dsstatus
_pattern_searcher(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count, u_int32_t findall)
{
	dsrecord *r;
	u_int32_t i, x;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusInvalidPath;
	
	if (findall == 0) *count = (u_int32_t)-1;

	if (scopemin == 0)
	{
		if (dsrecord_match(r, pattern) == 1)
		{
			if (findall == 0)
			{
				*count = dsid;
				dsrecord_release(r);
				return DSStatusOK;
			}

			if (*count == 0) *match = (u_int32_t *)malloc(sizeof(u_int32_t));
			else *match = (u_int32_t *)realloc(*match, (1 + *count) * sizeof(u_int32_t));
			(*match)[*count] = dsid;
			*count = *count + 1;
		}
	}
	else scopemin--;

	if (scopemax == 0)
	{
		dsrecord_release(r);
		return DSStatusOK;
	}

	x = scopemax - 1;
	if (scopemax == (u_int32_t)-1) x = scopemax;

	for (i = 0; i < r->sub_count; i++)
	{
		status = _pattern_searcher(s, r->sub[i], pattern, scopemin, x, match, count, findall);
		if (status  != DSStatusOK)
		{
			dsrecord_release(r);
			return status;
		}

		if ((findall == 0) && (*count != (u_int32_t)-1)) break;
	}

	dsrecord_release(r);
	return DSStatusOK;
}

/*
 * Search starting at a given record (specified by ID dsid) for all
 * records matching the attributes of the "pattern" record.
 * Scope is n for n-level deep search. Use (u-int32_t)-1 for unlimited
 * depth.  Scope 0 tests the given node against the pattern.
 * Search with null pattern or null attributes matchs all.
 * Search with null pattern or attributes and scope 1 returns all child IDs.
 */
dsstatus
dsengine_search_pattern(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count)
{
	*count = 0;	
	return _pattern_searcher(s, dsid, pattern, scopemin, scopemax, match, count, 1);
}

/*
 * Find first match.
 */
dsstatus
dsengine_find_pattern(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t *match)
{
	return _pattern_searcher(s, dsid, pattern, scopemin, scopemax, NULL, match, 0);
}

/*
 * Warning! count is overloaded - if we are searching for the
 * first match (dsengine_find_pattern()) then count is the output parameter
 * returning the dsid of the matching record.
 */
static dsstatus
_filter_searcher(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count, u_int32_t findall)
{
	dsrecord *r;
	u_int32_t i, x;
	dsstatus status;
	Logic3 eval;

	if (s == NULL) return DSStatusInvalidStore;

	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusInvalidPath;
	
	if (findall == 0) *count = (u_int32_t)-1;

	if (scopemin == 0)
	{
		if (s->delegate != NULL)
			eval = (s->delegate)(f, r, s->private);
		else
			eval = dsfilter_test(f, r);

		if (eval == L3True)
		{
			if (findall == 0)
			{
				*count = dsid;
				dsrecord_release(r);
				return DSStatusOK;
			}

			if (*count == 0) *match = (u_int32_t *)malloc(sizeof(u_int32_t));
			else *match = (u_int32_t *)realloc(*match, (1 + *count) * sizeof(u_int32_t));
			(*match)[*count] = dsid;
			*count = *count + 1;
		}
	}
	else scopemin--;

	if (scopemax == 0)
	{
		dsrecord_release(r);
		return DSStatusOK;
	}

	x = scopemax - 1;
	if (scopemax == (u_int32_t)-1) x = scopemax;

	for (i = 0; i < r->sub_count; i++)
	{
		status = _filter_searcher(s, r->sub[i], f, scopemin, x, match, count, findall);
		if (status  != DSStatusOK)
		{
			dsrecord_release(r);
			return status;
		}

		if ((findall == 0) && (*count != (u_int32_t)-1)) break;
	}

	dsrecord_release(r);
	return DSStatusOK;
}

dsstatus
dsengine_search_filter(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count)
{
	*count = 0;	
	return _filter_searcher(s, dsid, f, scopemin, scopemax, match, count, 1);
}

/*
 * Find first match.
 */
dsstatus
dsengine_find_filter(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t *match)
{
	return _filter_searcher(s, dsid, f, scopemin, scopemax, NULL, match, 0);
}

/*
 * Find the first child record of a given record that has an attribute with "key" and "val".
 */
dsstatus
dsengine_match(dsengine *s, u_int32_t dsid, dsdata *key, dsdata *val, u_int32_t *match)
{
	return dsstore_match(s->store, dsid, key, val, SELECT_ATTRIBUTE, match);
}

static dsstatus
dsengine_pathutil(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match, u_int32_t *create)
{
	u_int32_t i, n, c, do_create;
	dsstatus status;
	dsattribute *a;
	dsdata *k, *v;
	dsrecord *r;
	dsdata *keyname, *dot, *dotdot;

	do_create = *create;
	*create = 0;

	*match = (u_int32_t)-1;

	if (s == NULL) return DSStatusInvalidStore;

	if (path == NULL)
	{
		*match = dsid;
		return DSStatusOK;
	}
	
	/* Special cases: */
	/* key="name" val="."  matches this  record. */
	/* key="name" val=".." matches super record. */
	keyname = cstring_to_dsdata("name");
	dot = cstring_to_dsdata(".");
	dotdot = cstring_to_dsdata("..");

	n = dsid;
	for (i = 0; i < path->count; i++)
	{
		a = path->attribute[i];
		k = a->key;
		v = NULL;

		if (a->count > 0) v = a->value[0];

		if (dsdata_equal(k, keyname))
		{
			if (dsdata_equal(v, dot)) continue;
			if (dsdata_equal(v, dotdot))
			{
				r = dsstore_fetch(s->store, n);
				if (r == NULL)
				{
					dsdata_release(keyname);
					dsdata_release(dot);
					dsdata_release(dotdot);
					return DSStatusInvalidPath;
				}
				n = r->super;
				dsrecord_release(r);
				continue;
			}
		}

		status = dsengine_match(s, n, k, v, &c);
		if (status != DSStatusOK)
		{
			dsdata_release(keyname);
			dsdata_release(dot);
			dsdata_release(dotdot);
			return status;
		}

		if (c == (u_int32_t)-1)
		{
			if (do_create == 0)
			{
				dsdata_release(keyname);
				dsdata_release(dot);
				dsdata_release(dotdot);
				return DSStatusInvalidPath;
			}
			else
			{
				/* Create the path component */
				*create = 1;
				r = dsrecord_new();
				dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);
				status = dsengine_create(s, r, n);
				c = r->dsid;
				dsrecord_release(r);
				if (status != DSStatusOK)
				{
					dsdata_release(keyname);
					dsdata_release(dot);
					dsdata_release(dotdot);
					return status;
				}
			}
		}
		n = c;
	}

	*match = n;

	dsdata_release(keyname);
	dsdata_release(dot);
	dsdata_release(dotdot);

	return DSStatusOK;
}

/*
 * Returns a list of dsids, representing the path the given record to root.
 * The final dsid in the list will always be 0 (root).
 */
dsstatus
dsengine_path(dsengine *s, u_int32_t dsid, u_int32_t **list)
{
	u_int32_t i, n;

	i = dsid;

	*list = (u_int32_t *)malloc(sizeof(u_int32_t));
	(*list)[0] = i;
	n = 1;

	while (i != 0)
	{
		i = dsstore_record_super(s->store, i);
		if (i == IndexNull) return DSStatusReadFailed;
	
		*list = (u_int32_t *)realloc(*list, (n + 1) * sizeof(u_int32_t));
		(*list)[n] = i;
		n++;
	}

	return DSStatusOK;
}

/*
 * Find a record following a list of key=value pairs, which are given as
 * the attributes of a "path" record.
 */
dsstatus
dsengine_pathmatch(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match)
{
	int x;

	x = 0;
	return dsengine_pathutil(s, dsid, path, match, &x);
}

/*
 * Create a path following a list of key=value pairs, which are given as
 * the attributes of a "path" record.  Returns dsid of last directory in the
 * chain of created directories.  Follows existing directories if they exist.
 */
dsstatus
dsengine_pathcreate(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match)
{
	dsstatus status;
	char *ns;
	u_int32_t create;

	/* PROTECT FROM MULTIPLE NOTIFICATIONS */
	ns = s->store->notification_name;
	s->store->notification_name = NULL;

	create = 1;
	status = dsengine_pathutil(s, dsid, path, match, &create);

	s->store->notification_name = ns;

	if ((status == DSStatusOK) && (create != 0))
	{
		/* Send notification */
		dsstore_notify(s->store);
	}

	return status;
}

/*
 * Returns a list of dsids and values for a given attribute key.
 * Results are returned in a dsrecord.  Keys are dsids encoded using
 * int32_to_dsdata().  Values are attribute values for the
 * corresponding record.
 */
dsstatus
dsengine_list(dsengine *s, u_int32_t dsid, dsdata *key, u_int32_t scopemin, u_int32_t scopemax, dsrecord **list)
{
	dsrecord *r;
	dsattribute *a;
	u_int32_t i, j, *matches, len, x;
	dsdata *n;
	dsstatus status;
	
	if (s == NULL) return DSStatusInvalidStore;
	if (list == NULL) return DSStatusFailed;

	*list = NULL;

	if ((scopemin == 1) && (scopemax == 1))
	{
		return dsstore_list(s->store, dsid, key, SELECT_ATTRIBUTE, list);
	}

	if (key == NULL) return DSStatusInvalidKey;

	r = dsrecord_new();
	a = dsattribute_new(key);
	dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);
	dsattribute_release(a);

	status = dsengine_search_pattern(s, dsid, r, scopemin, scopemax, &matches, &len);
	dsrecord_release(r);
	if (status != DSStatusOK) return status;

	*list = dsrecord_new();
	n = cstring_to_dsdata("key");
	a = dsattribute_new(n);
	dsattribute_append(a, key);
	dsrecord_append_attribute(*list, a, SELECT_META_ATTRIBUTE);
	dsdata_release(n);
	dsattribute_release(a);

	for (i = 0; i < len; i++)
	{
		status = dsengine_fetch(s, matches[i], &r);
		if (status != DSStatusOK)
		{
			dsrecord_release(*list);
			return status;
		}

		x = dsrecord_attribute_index(r, key, SELECT_ATTRIBUTE);
		if (x == IndexNull)
		{
			dsrecord_release(r);
			continue;
		}

		n = int32_to_dsdata(r->dsid);
		a = dsattribute_new(n);
		dsdata_release(n);

		a->count = r->attribute[x]->count;

		if (a->count > 0)
			a->value = (dsdata **)malloc(a->count * sizeof(dsdata *));

		for (j = 0; j < a->count; j++)
			a->value[j] = dsdata_retain(r->attribute[x]->value[j]);

		dsrecord_append_attribute(*list, a, SELECT_ATTRIBUTE);
		dsattribute_release(a);
		dsrecord_release(r);
	}

	if (len > 0) free(matches);
	
	return DSStatusOK;
}

dsstatus
dsengine_netinfo_string_pathmatch(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match)
{
	dsrecord *p;
	u_int32_t i, numeric;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	*match = dsid;

	if (path == NULL) return DSStatusOK;

	numeric = 1;
	for (i = 0; (numeric == 1) && (path[i] != '\0'); i++)
	{
		if ((path[i] < '0') || (path[i] > '9')) numeric = 0;
	}
	
	if (numeric == 1)
	{
		i = atoi(path);
		if ((i == 0) && (strcmp(path, "0")))
		{
			*match = (u_int32_t)-1;
			return DSStatusInvalidRecordID;
		}
		*match = i;
		return DSStatusOK;
	}

	p = dsutil_parse_netinfo_string_path(path);
	if (p == NULL) return DSStatusInvalidPath;

	if (path[0] == '/') *match = 0;
	status = dsengine_pathmatch(s, *match, p, match);
	dsrecord_release(p);

	return status;
}

dsstatus
dsengine_netinfo_string_pathcreate(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match)
{
	dsrecord *p;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	*match = dsid;

	if (path == NULL) return DSStatusOK;

	p = dsutil_parse_netinfo_string_path(path);
	if (p == NULL) return DSStatusInvalidPath;

	if (path[0] == '/') *match = 0;
	status = dsengine_pathcreate(s, *match, p, match);
	dsrecord_release(p);

	return status;
}

char *
dsengine_netinfo_string_path(dsengine *s, u_int32_t dsid)
{
	char *p, *path, str[64], *x;
	u_int32_t i, len, plen, dirno;
	dsrecord *r;
	dsattribute *name;
	dsstatus status;
	dsdata *d;

	if (s == NULL) return NULL;

	if (dsid == 0)
	{
		path = malloc(2);
		path[0] = '/';
		path[1] = '\0';
		return path;
	}

	path = NULL;
	plen = 0;
	d = cstring_to_dsdata("name");

	i = dsid;
	while (i != 0)
	{
		dirno = 0;
		r = NULL;
		name = NULL;

		status = dsengine_fetch(s, i, &r);
		if (status != DSStatusOK)
		{
			dirno = 1;
			i = 0;
		}
		else
		{
			i = r->super;
			name = dsrecord_attribute(r, d, SELECT_ATTRIBUTE);
			if (name == NULL) dirno = 1;
			else if (name->count == 0) dirno = 1;
		}

		if ((dirno == 1) ||
			((x = dsdata_to_cstring(name->value[0])) == NULL))
		{
			if (r == NULL) sprintf(str, "dir:?");
			else sprintf(str, "dir:%u", r->dsid);
			x = str;
		}

		len = strlen(x);
		p = malloc(1 + len + plen + 1);
		if (path == NULL)
		{
			sprintf(p, "/%s", x);
		}
		else
		{
			sprintf(p, "/%s%s", x, path);
			free(path);
		}
		
		path = p;
		plen = strlen(path);

		dsattribute_release(name);
		dsrecord_release(r);
	}
	
	dsdata_release(d);

	return path;
}

/*
 * Written by Luke Howard 02-07-2000 to work with OpenLDAP back-netinfo.
 *
 * Here is the algorithm we use to construct a DN:
 *
 * for each directory in the name {
 *  if directory has "_rdn" attribute {
 *    use attribute named by value of rdn attribute as RDN.
 *  } else if directory has "name "attribute {
 *    use "name" as RDN
 *  } else {
 *    use DSID=%s as RDN
 *  }
 * }
 *
 * A remaining issue is dealing with the fact that NetInfo
 * names are case sensitive but X.500 names are not. We may
 * need to resort to DSID=%s for all RDNs.
 *
 * Also, multi-valued RDNs are not supported. We could support
 * them by having multiple values for the _rdn attribute (for
 * example, to form a DN cn=foo+sn=bar,c=US) but there are 
 * tricky canonicalization issues to be dealt with.
 *
 */
char *
dsengine_x500_string_path(dsengine *s, u_int32_t dsid)
{
	char *p, *path, str[64], *escaped;
	u_int32_t plen;
	dsrecord *r;
	dsattribute *rdn_attr, *name;
	dsstatus status;
	dsdata *rdn_key, *default_rdn_key, *name_rdn, *name_dsid;
	dsdata *x, tmp;

	if (s == NULL) return NULL;
	if (dsid == 0) return copyString("");

	path = NULL;
	plen = 0;

	default_rdn_key = cstring_to_dsdata("name");
	name_rdn = cstring_to_dsdata("rdn");
	name_dsid = casecstring_to_dsdata("DSID");

	status = dsengine_fetch(s, dsid, &r);
	if (status != DSStatusOK) return NULL;

	while (r->dsid != 0)
	{
		dsrecord *parent;

		rdn_attr = dsrecord_attribute(r, name_rdn, SELECT_META_ATTRIBUTE);
		if (rdn_attr == NULL || rdn_attr->count == 0)
		{
			rdn_key = dsdata_retain(default_rdn_key);
		}
		else
		{
			rdn_key = dsdata_retain(rdn_attr->value[0]);
		}
		dsattribute_release(rdn_attr);

		name = dsrecord_attribute(r, rdn_key, SELECT_ATTRIBUTE);
		if (name == NULL || name->count == 0)
		{
			/* No name, so let's use the directory ID. */
			dsdata_release(rdn_key);
			rdn_key = dsdata_retain(name_dsid);
		}

		escaped = escape_rdn(rdn_key);

		if (path == NULL)
		{
			plen = strlen(escaped) + 1;
			path = malloc(plen + 1);
			sprintf(path, "%s=", escaped);
		}
		else
		{
			plen += strlen(escaped) + 2;
			p = malloc(plen + 1);
			sprintf(p, "%s,%s=", path, escaped);
			free(path);
			path = p;
		}

		free(escaped);

		if (name == NULL || name->count == 0)
		{
			sprintf(str, "%u", r->dsid);
			tmp.type = DataTypeCStr;
			tmp.length = strlen(str) + 1;
			tmp.data = str;
			tmp.retain = 1;
			x = &tmp;
		}
		else
		{
			x = name->value[0];
		}

		escaped = escape_rdn(x);

		plen += strlen(escaped);
		p = malloc(plen + 1);
		sprintf(p, "%s%s", path, escaped);
		free(path);
		path = p;

		free(escaped);
		dsdata_release(rdn_key);
		dsattribute_release(name);

		status = dsengine_fetch(s, r->super, &parent);
		if (status != DSStatusOK)
		{
			dsrecord_release(r);
			dsdata_release(default_rdn_key);
			dsdata_release(name_rdn);
			dsdata_release(name_dsid);
			return NULL;
		}
		dsrecord_release(r);
		r = parent;
	}

	dsrecord_release(r);
	dsdata_release(default_rdn_key);
	dsdata_release(name_rdn);
	dsdata_release(name_dsid);

	return path;
}

/*
 * Get a record's parent dsid.
 */
dsstatus
dsengine_record_super(dsengine *s, u_int32_t dsid, u_int32_t *super)
{
	if (s == NULL) return DSStatusInvalidStore;

	*super = dsstore_record_super(s->store, dsid);
	if (*super == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get a record's version number.
 */
dsstatus
dsengine_record_version(dsengine *s, u_int32_t dsid, u_int32_t *version)
{
	if (s == NULL) return DSStatusInvalidStore;

	*version = dsstore_record_version(s->store, dsid);
	if (*version == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get a record's serial number.
 */
dsstatus
dsengine_record_serial(dsengine *s, u_int32_t dsid, u_int32_t *serial)
{
	if (s == NULL) return DSStatusInvalidStore;

	*serial = dsstore_record_serial(s->store, dsid);
	if (*serial == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get the dsid of the record with a given version number.
 */
dsstatus
dsengine_version_record(dsengine *s, u_int32_t version, u_int32_t *dsid)
{
	if (s == NULL) return DSStatusInvalidStore;

	*dsid = dsstore_version_record(s->store, version);
	if (*dsid == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get data store version number.
 */
dsstatus
dsengine_version(dsengine *s, u_int32_t *version)
{
	if (s == NULL) return DSStatusInvalidStore;

	*version = dsstore_version(s->store);
	if (*version == IndexNull) return DSStatusInvalidStore;
	return DSStatusOK;
}

/*
 * Get a record's version number, serial number, and parent's dsid.
 */
dsstatus dsengine_vital_statistics(dsengine *s, u_int32_t dsid, u_int32_t *version, u_int32_t *serial, u_int32_t *super)
{
	if (s == NULL) return DSStatusInvalidStore;
	return dsstore_vital_statistics(s->store, dsid, version, serial, super);
}

void
dsengine_flush_cache(dsengine *s)
{
	if (s == NULL) return;
	dsstore_flush_cache(s->store);
}

dsstatus
dsengine_x500_string_pathmatch(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match)
{
	dsstatus status;
	dsrecord *p;
	char **exploded;
	char *key, *value;

	if (s == NULL) return DSStatusInvalidStore;

	*match = dsid;

	if (path == NULL) return DSStatusOK;

	/*
	 * As a short cut, we let clients use the special base DN
	 *     DSID=<dsid>,<arbitary DN info>
	 * to refer to a specific directory ID.
	 */
	exploded = dsx500_explode_dn(path, 0);
	if (exploded != NULL)
	{
		if (exploded[0] != NULL)
		{
			key = dsx500_rdn_attr_type(exploded[0]);
			value = dsx500_rdn_attr_value(exploded[0]);

			if ((key != NULL) && (value != NULL) && (strcasecmp(key, "DSID") == 0))
			{
				*match = strtoul(value, (char **)NULL, 10);
				free(key);
				free(value);
				freeList(exploded);

				return DSStatusOK;
			}

			if (key != NULL) free(key);
			if (value != NULL) free(value);
		}
		freeList(exploded);
	}

	p = dsutil_parse_x500_string_path(path);
	if (p == NULL) return DSStatusInvalidPath;

	status = dsengine_pathmatch(s, dsid, p, match);
	dsrecord_release(p);

	return status;
}

dsstatus
dsengine_x500_string_pathcreate(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match)
{
	dsrecord *p;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	*match = dsid;

	if (path == NULL) return DSStatusOK;

	p = dsutil_parse_x500_string_path(path);
	if (p == NULL) return DSStatusInvalidPath;

	status = dsengine_pathcreate(s, dsid, p, match);
	dsrecord_release(p);

	return status;
}

dsdata *
dsengine_map_name(dsengine *e, dsdata *name, u_int32_t intype)
{
	if (e == NULL) return NULL;

	/* This in principle only works if name types and flags are in sync. */
	return dsengine_convert_name(e, name, intype, (e->store->flags & DSENGINE_FLAGS_NAMING_MASK));
}

static dsstatus
find_user(dsengine *s, u_int32_t keyType, dsdata *name, u_int32_t *match)
{
	dsrecord *r;
	dsdata *k;
	dsattribute *a;
	dsstatus status;
	u_int32_t users;

	/* get the /users directory for future use */
	status = dsengine_netinfo_string_pathmatch(s, 0, "users", &users);
	if (status != DSStatusOK) return status;

	r = dsrecord_new();
	k = cstring_to_dsdata(keyType == NameTypeUserName ? "name" : "principal");
	a = dsattribute_new(k);
	dsattribute_insert(a, name, 0);
	dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);

	status = dsengine_find_pattern(s, users, r, 1, 1, match);

	dsattribute_release(a);
	dsdata_release(k);
	dsrecord_release(r);

	return status;
}

static dsdata *
read_user_attribute(dsengine *s, char *attr, u_int32_t match)
{
	dsrecord *r;
	dsdata *k, *d;
	dsattribute *a;
	dsstatus status;
	u_int32_t users, p;

	/* get the /users directory for future use */
	status = dsengine_netinfo_string_pathmatch(s, 0, "users", &users);
	if (status != DSStatusOK) return NULL;

	status = dsengine_record_super(s, match, &p);
	if (status != DSStatusOK) return NULL;

	if (p != users) return NULL;

	status = dsengine_fetch(s, match, &r);
	if (status != DSStatusOK) return NULL;

	k = cstring_to_dsdata(attr);
	a = dsrecord_attribute(r, k, SELECT_ATTRIBUTE);
	d = dsattribute_value(a, 0);

	dsattribute_release(a);
	dsdata_release(k);
	dsrecord_release(r);

	return d;
}

dsdata *
dsengine_convert_name(dsengine *e, dsdata *name, u_int32_t intype, u_int32_t desiredtype)
{
	/* Name map switcheroo. */
	dsdata *d;
	char *str, *tname;
	u_int32_t match;

	if (name == NULL) return NULL;

	d = NULL;
	str = NULL;
	tname = NULL;
	match = 0;

	switch (name->type)
	{
		case DataTypeCStr:
		case DataTypeCaseCStr:
		case DataTypeUTF8Str:
		case DataTypeCaseUTF8Str:
			tname = dsdata_to_utf8string(name);
			break;
		case DataTypeDirectoryID:
			match = dsdata_to_dsid(name);
			break;
		default:
			break;
	}

	switch (intype)
	{
		case NameTypeNetInfo:
			switch (desiredtype)
			{
				case NameTypeNetInfo:
					d = dsdata_retain(name);
					break;
				case NameTypeX500:
					str = dsx500_netinfo_string_path_to_dn(tname);
					break;
				case NameTypeDirectoryID:
					if (dsengine_netinfo_string_pathmatch(e, 0, tname, &match) != DSStatusOK)
						return NULL;
					d = dsid_to_dsdata(match);
					break;
				case NameTypeUserName:
					if (dsengine_netinfo_string_pathmatch(e, 0, tname, &match) != DSStatusOK)
						return NULL;
					d = read_user_attribute(e, "name", match);
					break;
				case NameTypePrincipalName:
					if (dsengine_netinfo_string_pathmatch(e, 0, tname, &match) != DSStatusOK)
						return NULL;
					d = read_user_attribute(e, "principal", match);
					break;
				default:
					break;
			}
			break;
		case NameTypeX500:
			switch (desiredtype)
			{
				case NameTypeNetInfo:
					str = dsx500_dn_to_netinfo_string_path(tname);
					break;
				case NameTypeX500:
					d = dsdata_retain(name);
					break;
				case NameTypeDirectoryID:
					if (dsengine_x500_string_pathmatch(e, 0, tname, &match) != DSStatusOK)
						return NULL;
					d = dsid_to_dsdata(match);
					break;
				case NameTypeUserName:
					if (dsengine_x500_string_pathmatch(e, 0, tname, &match) != DSStatusOK)
						return NULL;
					d = read_user_attribute(e, "name", match);
					break;
				case NameTypePrincipalName:
					if (dsengine_x500_string_pathmatch(e, 0, tname, &match) != DSStatusOK)
						return NULL;
					d = read_user_attribute(e, "principal", match);
					break;
				default:
					break;
			}
			break;
		case NameTypeUserName:
		case NameTypePrincipalName:
			switch (desiredtype)
			{
				case NameTypeUserName:
				case NameTypePrincipalName:
					d = dsdata_retain(name);
					break;
				case NameTypeNetInfo:
					if (find_user(e, intype, name, &match) != DSStatusOK)
						return NULL;
					str = dsengine_netinfo_string_path(e, match);
					break;
				case NameTypeX500:
					if (find_user(e, intype, name, &match) != DSStatusOK)
						return NULL;
					str = dsengine_x500_string_path(e, match);
					break;
				case NameTypeDirectoryID:
					if (find_user(e, intype, name, &match) != DSStatusOK)
						return NULL;
					d = dsid_to_dsdata(match);
					break;
				default:
					break;
			}
			break;
		case NameTypeDirectoryID:
			switch (desiredtype)
			{
				case NameTypeNetInfo:
					str = dsengine_netinfo_string_path(e, match);
					break;
				case NameTypeX500:
					str = dsengine_x500_string_path(e, match);
					break;
				case NameTypeDirectoryID:
					d = dsdata_retain(name);
					break;
				case NameTypeUserName:
					d = read_user_attribute(e, "name", match);
					break;
				case NameTypePrincipalName:
					d = read_user_attribute(e, "principal", match);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	if (str != NULL) 
	{
		if (d != NULL) dsdata_release(d);
		d = cstring_to_dsdata(str);
		free(str);
	}

	return d;
}

void dsengine_set_filter_test_delegate(dsengine *e, Logic3 (*delegate)(dsfilter *, dsrecord *, void *), void *private)
{
	if (e == NULL) return;
	e->private = private;
	e->delegate = delegate;
}

