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
 * Server-side implementation of NetInfo interface
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ni_server.h"
#include "ni_file.h"
#include "ni_globals.h"
#include <NetInfo/system_log.h>
#include <NetInfo/mm.h>
#include <NetInfo/dsstore.h>
#include "index.h"
#include "index_manager.h"

#define NIOP_READ 0
#define NIOP_WRITE 1

/*
 * What's inside the opaque handle we pass to clients of this layer
 */
typedef struct ni_handle
{
	ni_entrylist nilistres;	/* result of ni_list (efficiency hack) */
	char *user;		/* username of caller */
	void *file_hdl;		/* hook into next layer */
	im_handle im_hdl;	/* handle into index manager */
} ni_handle;

#define NH(handle) ((struct ni_handle *)handle)
#define FH(handle) NH(handle)->file_hdl
#define IMH(handle) (&NH(handle)->im_hdl)

static ni_status
ni_inaccesslist(void *handle, ni_namelist access_list)
{
	if (ni_namelist_match(access_list, ACCESS_USER_ANYBODY) != NI_INDEX_NULL)
	{
		auth_count[WGOOD]++;
		if (!i_am_clone)
		{
			system_log(LOG_DEBUG,
				"Allowing any user [%s] to modify domain",
				(NH(handle)->user == NULL) ? "(unknown)" : NH(handle)->user);
		}
		return NI_OK;
	}

	if (NH(handle)->user == NULL)
	{
		auth_count[WBAD]++;
		system_log(LOG_ERR, "Anonymous user may not modify domain");
		return NI_PERM;
	}

	if (ni_namelist_match(access_list, NH(handle)->user) != NI_INDEX_NULL)
	{
		auth_count[WGOOD]++;
		if (!i_am_clone)
		{
			system_log(LOG_DEBUG,
				"Allowing user %s to modify domain", NH(handle)->user);
		}
		return NI_OK;
	}

	auth_count[WBAD]++;
	system_log(LOG_ERR,
		"Remote user %s may not modify domain", NH(handle)->user);
	return NI_PERM;
}

static ni_status 
ni_validate_dir(void *handle, ni_object *obj, int complain)
{
	ni_proplist *pl;
	ni_index i;
	
	if ((NH(handle)->user != NULL) &&
		(ni_name_match(NH(handle)->user, ACCESS_USER_SUPER)))
	{
		auth_count[WGOOD]++;
		if (!i_am_clone)
		{
			system_log(LOG_DEBUG,
				"Allowing superuser %s to modify directory %d",
				NH(handle)->user, obj->nio_id.nii_object);
		}
		return NI_OK;
	}

	pl = &obj->nio_props;
	for (i = 0; i < pl->nipl_len; i++)
	{
		if (ni_name_match(pl->nipl_val[i].nip_name, ACCESS_DIR_KEY))
		{
			return ni_inaccesslist(handle, pl->nipl_val[i].nip_val);
		}
	}

	if (complain == 0) return NI_PERM;

	auth_count[WBAD]++;
	system_log(LOG_ERR,
	   	"Remote user %s may not modify directory %d",
	   	(NH(handle)->user == NULL) ? "(unknown)" : NH(handle)->user,
	   	obj->nio_id.nii_object);

	return NI_PERM;
}

static ni_status 
ni_validate_name(void *handle, ni_object *obj, ni_index prop_index)
{
	ni_proplist *pl;
	ni_name key = NULL;
	ni_name propkey;
	ni_index i;

	if ((NH(handle)->user != NULL) &&
		(ni_name_match(NH(handle)->user, ACCESS_USER_SUPER)))
	{
		auth_count[WGOOD]++;
		if (!i_am_clone)
		{
			system_log(LOG_DEBUG,
				"Allowing superuser %s to modify "
			   "property %s in directory %d", NH(handle)->user,
			   obj->nio_props.nipl_val[prop_index].nip_name,
			   obj->nio_id.nii_object);
		}
		return NI_OK;
	}

	propkey = obj->nio_props.nipl_val[prop_index].nip_name;
	MM_ALLOC_ARRAY(key, (strlen(ACCESS_NAME_PREFIX) + 
			 	strlen(propkey) + 1));
	sprintf(key, "%s%s", ACCESS_NAME_PREFIX, propkey);
	
	pl = &obj->nio_props;
	for (i = 0; i < pl->nipl_len; i++)
	{
		if (ni_name_match(pl->nipl_val[i].nip_name, key))
		{
			ni_name_free(&key);
			return ni_inaccesslist(handle, pl->nipl_val[i].nip_val);
		}
	}

	auth_count[WBAD]++;
	system_log(LOG_ERR,
	   	"Remote user %s may not modify property %s in directory %d",
	   	NH(handle)->user, key, obj->nio_id.nii_object);
	ni_name_free(&key);
	return NI_PERM;
}

/*
 * Free an object
 */
static void
obj_free(ni_object *obj)
{
	int i, j, nvals, len;
	ni_property *p;

	if (obj == NULL) return;

	if (obj->nio_children.ni_idlist_len > 0)
		free(obj->nio_children.ni_idlist_val);

	len = obj->nio_props.ni_proplist_len;
	for (i = 0; i < len; i++)
	{
		p = &(obj->nio_props.ni_proplist_val[i]);
		free(p->nip_name);

		nvals = p->nip_val.ni_namelist_len;
		for (j = 0; j < nvals; j++)
			free(p->nip_val.ni_namelist_val[j]);

		free(p->nip_val.ni_namelist_val);
	}

	if (len > 0) free(obj->nio_props.ni_proplist_val);

	free(obj);
}

/*
 * Lookup an object.  If we are about to do a write,
 * make sure object is not stale.
 */
static ni_status
obj_lookup(void *handle, ni_id *idp, int op, ni_object **objp)
{
	ni_status status;
	ni_id id;

	id = *idp;
	status = file_read(FH(handle), &id, objp);
	if (status != NI_OK) return status;

	if ((op == NIOP_WRITE) && (id.nii_instance != idp->nii_instance))
	{
		obj_free(*objp);
		return NI_STALE;
	}

	*idp = id;
	return NI_OK;
}

/*
 * Allocates a new object, returned ID is arbitrary
 */
static ni_status
obj_alloc(void *handle, ni_object **objp)
{
	ni_object *obj;
	ni_status status;

	MM_ALLOC(obj);
	MM_ZERO(obj);
	
	status = file_idalloc(FH(handle), &obj->nio_id);
	if (status != NI_OK)
	{
		MM_FREE(obj);
		return status;
	}

	*objp = obj;
	return NI_OK;
}

/*
 * Destroys an object
 */
static void
obj_unalloc(void *handle, ni_object *obj)
{
	file_idunalloc(FH(handle), obj->nio_id);
	obj_free(obj);
}

/*
 * Initilialize this layer
 */
ni_status
ni_init(char *rootdir, void **handle)
{
	ni_handle *ni;
	ni_status status;

	MM_ALLOC(ni);
	ni->nilistres.ni_entrylist_len = 0;
	ni->nilistres.ni_entrylist_val = NULL;
	ni->user = NULL;

	status = file_init(rootdir, &ni->file_hdl);
	if (status != NI_OK)
	{
		system_log(LOG_ERR, "file_init failed: %s", ni_error(status));
		MM_FREE(ni);
		return status;
	}

	ni->im_hdl = im_alloc();

	/*
	 * Make sure the index manager cache is flushed
	 * when the store is changed by another process.
	 */
	dsstore_set_sync_delegate((dsstore *)ni->file_hdl, (void (*)(void *))im_forget, &ni->im_hdl);

	*handle = ni;

	return status;
}

/*
 * Return the database tag
 */
char *
ni_tagname(void *handle)
{
	ni_name tag;
	ni_name dot;

	tag = ni_name_dup(file_dirname(FH(handle)));
	dot = rindex(tag, '.');
	if (dot != NULL) *dot = 0;

	return tag;
}

/*
 * Forget what we know (cache flushing)
 */
void
ni_forget(void *handle)
{
	file_forget(FH(handle));
	im_forget(IMH(handle));
}

/*
 * Rename this database
 */
void
ni_renamedir(void *handle, char *name)
{
	file_renamedir(FH(handle), name);
}

/*
 * Set the username of the caller
 */
ni_status
ni_setuser(void *handle, ni_name_const user)
{
	if (NH(handle)->user != NULL) ni_name_free(&NH(handle)->user);

	if (user != NULL)
		NH(handle)->user = ni_name_dup(user);
	else
		NH(handle)->user = NULL;

	return NI_OK;
}

/*
 * Free up allocated resources
 */
void
ni_free(void *handle)
{
	ni_list_const_free(handle);
	if (NH(handle)->user != NULL) ni_name_free(&NH(handle)->user);

	file_free(FH(handle));
	im_free(IMH(handle));
	MM_FREE(NH(handle));
}

unsigned
ni_getchecksum(void *handle)
{
	return file_getchecksum(FH(handle));
}

void
ni_shutdown(void *handle, unsigned checksum)
{
	file_shutdown(FH(handle), checksum);
}

ni_status
ni_root(void *handle, ni_id *id_p)
{
	ni_object *obj;
	ni_id root_id;
	ni_status status;

	root_id.nii_object = 0;
	root_id.nii_instance = 0;

	status = file_read(FH(handle), &root_id, &obj);
	if (status != NI_OK)
	{
		/*
		 * Create root node
		 */
		MM_ALLOC(obj);
		MM_ZERO(obj);

		obj->nio_id.nii_object=-1;

		status = file_write(FH(handle), obj);
		if (status != NI_OK)
		{
			obj_free(obj);
			return status;
		}
	}

	root_id = obj->nio_id;
	obj_free(obj);
	*id_p = root_id;
	return NI_OK;
}

int
ni_idlist_hasid(ni_idlist *idlist, ni_index id)
{
	ni_index i;

	for (i = 0; i < idlist->ni_idlist_len; i++)
	{
		if (idlist->ni_idlist_val[i] == id) return 1;
	}

	return 0;
}

ni_status
ni_create(void *handle, ni_id *parent_id, ni_proplist pl, ni_id *child_id_p, ni_index where)
{
	ni_object *child;
	ni_object *parent;
	ni_status status;

	status = obj_lookup(handle, parent_id, NIOP_WRITE, &parent);
	if (status != NI_OK) return status;

	status = ni_validate_dir(handle, parent, 1);
	if (status != NI_OK)
	{
		obj_free(parent);
		return status;
	}

	status = obj_alloc(handle, &child);
	if (status != NI_OK)
	{
		obj_free(parent);
		return status;
	}

	child->nio_props = ni_proplist_dup(pl);
	child->nio_parent = parent_id->nii_object;

	status = file_write(FH(handle), child);
	if (status != NI_OK)
	{
		obj_free(parent);
		obj_unalloc(handle, child);
		return status;
	}

	*child_id_p = child->nio_id;

	/*
	 * Update parent
	 */
	if (!ni_idlist_hasid(&parent->nio_children, child_id_p->nii_object))
	{
		/*
		 * Only insert if it isn't already there. This is so people
		 * can fix databases with hard-linked directories. Hard links
		 * shouldn't happen, but did in previous releases. The way 
		 * to fix it is to cause both links to be in the same 
		 * directory where they then merge.
		 */
		ni_idlist_insert(&parent->nio_children, child_id_p->nii_object,
				 where);
	}
	status = file_write(FH(handle), parent);
	if (status != NI_OK)
	{
		obj_unalloc(handle, child);
		obj_free(parent);
		return status;
	}

	im_newnode(IMH(handle), child, where);
	*parent_id = parent->nio_id;
	obj_free(child);
	obj_free(parent);

	file_notify(FH(handle));

	return NI_OK;
}

ni_status
ni_destroy(void *handle, ni_id *parent_id, ni_id child_id)
{
	ni_object *parent;
	ni_object *child;
	ni_status status;
	int badid = 0;

	status = obj_lookup(handle, parent_id, NIOP_WRITE, &parent);
	if (status != NI_OK) return status;

	status = ni_validate_dir(handle, parent, 1);
	if (status != NI_OK)
	{
		obj_free(parent);
		return status;
	}

	status = obj_lookup(handle, &child_id, NIOP_WRITE, &child);
	if (status != NI_OK)
	{
		if (status != NI_BADID)
		{
			obj_free(parent);
			return status;
		}
		else badid++;
	}

	if (!badid)
	{
		if (child->nio_children.ni_idlist_len != 0)
		{
			obj_free(child);
			obj_free(parent);
			return NI_NOTEMPTY;
		}
	}
	
	if (!ni_idlist_delete(&parent->nio_children, child_id.nii_object))
	{
		obj_free(parent);
		if (!badid) obj_free(child);
		return NI_UNRELATED;
	}

	/*
	 * Commit changes
	 */
	status = file_write(FH(handle), parent);
	if (status != NI_OK)
	{
		obj_free(parent);
		if (!badid) obj_free(child);
		return status;
	}

	*parent_id = parent->nio_id;
	obj_free(parent);

	if (!badid)
	{
		im_remove(IMH(handle), child);
		obj_unalloc(handle, child);
	}

	file_notify(FH(handle));

	return NI_OK;
}

/*
 * Copy properties for read functions ONLY (ni_read, ni_lookupread).
 */
static inline void
copyprops(ni_proplist *inprops, ni_proplist *outprops)
{
	/*
	 * Do not copy the data, only the pointers to it. 
	 * Zeroing out the props prevents obj_free() from freeing these
	 * pointers.
	 */
	*outprops = *inprops;
	MM_ZERO(inprops);
}

ni_status
ni_read(void *handle, ni_id *id, ni_proplist *props)
{
	ni_status status;
	ni_object *obj;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	copyprops(&obj->nio_props, props);
	obj_free(obj);
	return status;
}

ni_status
ni_write(void *handle, ni_id *id, ni_proplist props)
{
	ni_status status;
	ni_object *obj;
	ni_proplist saveprops;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	status = ni_validate_dir(handle, obj, 1);
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	saveprops = obj->nio_props;
	obj->nio_props = ni_proplist_dup(props);

	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		ni_proplist_free(&saveprops);
		obj_free(obj);
		return status;
	}

	im_destroy_all(IMH(handle), obj, saveprops);
	ni_proplist_free(&saveprops);
	im_create_all(IMH(handle), obj, obj->nio_props);

	*id = obj->nio_id;
	obj_free(obj);

	file_notify(FH(handle));

	return NI_OK;
}


static ni_index
ni_prop_find(ni_proplist props, ni_name_const pname)
{
	ni_index i;

	for (i = 0; i < props.nipl_len; i++)
	{
	  	if (ni_name_match(props.nipl_val[i].nip_name, pname)) return i;
	}

	return NI_INDEX_NULL;
}


ni_status
ni_lookup(void *handle, ni_id *id, ni_name_const pname, ni_name_const pval, ni_idlist *children_p)
{
	ni_object *obj, *tmp;
	ni_index i;
	ni_status status;
	ni_id tmpid;
	ni_idlist res;
	ni_index ndirs, *dirs, which, oid;
	index_handle index;
	int please_index;
	ni_property *prop;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	if (im_has_indexed_dir(IMH(handle), id->nii_object, pname, pval, &dirs, &ndirs))
	{
		res.ni_idlist_len = ndirs;
		res.ni_idlist_val = NULL;

		if (ndirs > 0)
		{
			MM_ALLOC_ARRAY(res.ni_idlist_val, res.ni_idlist_len);
			MM_BCOPY(dirs, res.ni_idlist_val, res.ni_idlist_len * sizeof(dirs[0]));
		}
	}
	else
	{
		res.ni_idlist_len = 0;
		res.ni_idlist_val = NULL;
		index = index_alloc();
		please_index = 1;

		for (i = 0; i < obj->nio_children.ni_idlist_len; i++)
		{
			tmpid.nii_object = obj->nio_children.ni_idlist_val[i];
			status = obj_lookup(handle, &tmpid, NIOP_READ, &tmp);
			if (status != NI_OK)
			{
				system_log(LOG_ERR, "cannot lookup child");
				if (please_index)
				{
					index_unalloc(&index);
					please_index = 0;
				}
				continue;
			}
			which = ni_prop_find(tmp->nio_props, pname);
			if (which != NI_INDEX_NULL)
			{
				prop = &tmp->nio_props.nipl_val[which];
				oid = tmp->nio_id.nii_object;
				if (ni_namelist_match(prop->nip_val, pval) != NI_INDEX_NULL)
				{
					ni_idlist_insert(&res, oid, NI_INDEX_NULL);
				}

				if (please_index)
				{
					index_insert_list(&index, prop->nip_val, oid);
				}
			}
			obj_free(tmp);
		}
		if (please_index)
		{
			im_store_index(IMH(handle), index, obj->nio_id.nii_object, pname);
		}
	}

	obj_free(obj);
	if (res.ni_idlist_len == 0) return NI_NODIR;

	*children_p = res;
	return NI_OK;
}

ni_status
ni_lookupread(void *handle, ni_id *id, ni_name_const pname, ni_name_const pval, ni_proplist *props)
{
	ni_object *obj;
	ni_object *tmp;
	ni_index i;
	ni_status status;
	ni_id tmpid;
	int found;
	ni_index ndirs;
	ni_index *dirs;
	ni_index which;
	index_handle index;
	int please_index;
	ni_index oid;
	ni_property *prop;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;
	if (im_has_indexed_dir(IMH(handle), id->nii_object, pname, pval, &dirs, &ndirs))
	{
		found = ndirs;
		if (found)
		{
			tmpid.nii_object = dirs[0];
			status = obj_lookup(handle, &tmpid, NIOP_READ, &tmp);
			if (status != NI_OK) return status;

			copyprops(&tmp->nio_props, props);
			obj_free(tmp);
		}
	}
	else
	{
		index = index_alloc();
		please_index = 1;

		found = 0;
		for (i = 0; (i < obj->nio_children.ni_idlist_len && (please_index || !found)); i++)
		{
			tmpid.nii_object = obj->nio_children.ni_idlist_val[i];
			status = obj_lookup(handle, &tmpid, NIOP_READ, &tmp);
			if (status != NI_OK)
			{
				system_log(LOG_ERR, "cannot lookup child");
				if (please_index)
				{
					index_unalloc(&index);
					please_index = 0;
				}
				continue;
			}

			which = ni_prop_find(tmp->nio_props, pname);
			if (which != NI_INDEX_NULL)
			{
				prop = &tmp->nio_props.nipl_val[which];
				oid = tmp->nio_id.nii_object;
				if (ni_namelist_match(prop->nip_val, pval) != NI_INDEX_NULL)
				{
					if (!found)
					{
						copyprops(&tmp->nio_props, props);
						found++;
					} 
				}
				if (please_index)
				{
					index_insert_list(&index, prop->nip_val, oid);
				}
			}
			obj_free(tmp);
		}
		if (please_index)
		{
			im_store_index(IMH(handle), index, obj->nio_id.nii_object, pname);
		}
	}

	obj_free(obj);
	return (found ? NI_OK : NI_NODIR);
}

void
ni_list_const_free(void *handle)
{
	if (NH(handle)->nilistres.ni_entrylist_val != NULL)
	{
		ni_entrylist_free(&NH(handle)->nilistres);
		NH(handle)->nilistres.ni_entrylist_len = 0;
		NH(handle)->nilistres.ni_entrylist_val = NULL;
	}
}

/*
 * Like ni_list, but for efficiency, the returned entries are to be
 * considered "const" and NOT freed.
 */
ni_status
ni_list_const(void *handle, ni_id *id, ni_name_const pname, ni_entrylist *entries)
{
	ni_object *obj;
	ni_object *tmp;
	ni_index i;
	ni_index j;
	ni_status status;
	ni_id tmpid;
	ni_proplist *pl;
	ni_namelist *nl;
	ni_entrylist res;
	int use_store;

	ni_list_const_free(handle);

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;
	use_store = (NIOP_READ == NIOP_READ);
	if (!use_store || !im_has_saved_list(IMH(handle), id->nii_object, pname, &res))
	{
		res.ni_entrylist_len = obj->nio_children.ni_idlist_len;
		MM_ALLOC_ARRAY(res.ni_entrylist_val, res.ni_entrylist_len);
		MM_ZERO_ARRAY(res.ni_entrylist_val, res.ni_entrylist_len);
		for (i = 0; i < obj->nio_children.ni_idlist_len; i++)
		{
			tmpid.nii_object = obj->nio_children.ni_idlist_val[i];
			status = obj_lookup(handle, &tmpid, NIOP_READ, &tmp);
			if (status != NI_OK)
			{
				system_log(LOG_ERR, "cannot lookup child");
				continue;
			}
			res.ni_entrylist_val[i].id = tmpid.nii_object;
			res.ni_entrylist_val[i].names = NULL;
			pl = &tmp->nio_props;
			for (j = 0; j < pl->nipl_len; j++)
			{
				nl = &pl->nipl_val[j].nip_val;
				if (ni_name_match(pl->nipl_val[j].nip_name, pname) && nl->ninl_len > 0)
				{
					MM_ALLOC(res.ni_entrylist_val[i].names);
					(*res.ni_entrylist_val[i].names = ni_namelist_dup(*nl));
					break;
				}
			}
			obj_free(tmp);
		}
		if (use_store)
		{
			im_store_list(IMH(handle), obj->nio_id.nii_object, pname, res);
		}
		NH(handle)->nilistres = res;
	}
	obj_free(obj);
	*entries = res;
	return NI_OK;
}

ni_status
ni_listall(void *handle, ni_id *id, ni_proplist_list *entries)
{
	ni_object *obj;
	ni_object *tmp;
	ni_index i;
	ni_status status;
	ni_id tmpid;
	ni_proplist_list res;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	res.nipll_len = obj->nio_children.ni_idlist_len;
	MM_ALLOC_ARRAY(res.nipll_val, res.nipll_len);
	MM_ZERO_ARRAY(res.nipll_val, res.nipll_len);

	for (i = 0; i < obj->nio_children.ni_idlist_len; i++)
	{
		tmpid.nii_object = obj->nio_children.ni_idlist_val[i];
		status = obj_lookup(handle, &tmpid, NIOP_READ, &tmp);
		if (status != NI_OK)
		{
			system_log(LOG_ERR, "cannot lookup child");
			continue;
		}
		res.nipll_val[i] = ni_proplist_dup(tmp->nio_props);
		obj_free(tmp);
	}

	obj_free(obj);
	*entries = res;
	return NI_OK;
}

ni_status
ni_children(void *handle, ni_id *id, ni_idlist *children_p)
{
	ni_object *obj;
	ni_status status;
	
	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	*children_p = ni_idlist_dup(obj->nio_children);
	obj_free(obj);

	return NI_OK;
}

ni_status
ni_parent(void *handle, ni_id *id, ni_index *parent_id_p)
{
	ni_object *obj;
	ni_status status;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	*parent_id_p = obj->nio_parent;
	obj_free(obj);

	return NI_OK;
}

ni_status
ni_self(void *handle, ni_id *id)
{
	ni_object *obj;
	ni_status status;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	id->nii_instance = obj->nio_id.nii_instance;
	id->nii_object = obj->nio_id.nii_object;
	obj_free(obj);

	return NI_OK;
}

ni_status
ni_readprop(void *handle, ni_id *id, ni_index prop_index, ni_namelist *propval_p)
{
	ni_status status;
	ni_object *obj;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	*propval_p = ni_namelist_dup(obj->nio_props.nipl_val[prop_index].nip_val);
	obj_free(obj);

	return status;
}

ni_status
ni_writeprop(void *handle, ni_id *id, ni_index prop_index, ni_namelist values)
{
	ni_status status;
	ni_object *obj;
	ni_namelist savelist;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	/* check for directory access */
	status = ni_validate_dir(handle, obj, 0);
	if (status != NI_OK)
	{
		/* no directory access - check for access to this property */
		status = ni_validate_name(handle, obj, prop_index);
	}
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	savelist = obj->nio_props.nipl_val[prop_index].nip_val;
	obj->nio_props.nipl_val[prop_index].nip_val = ni_namelist_dup(values);

	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		obj_free(obj);
		ni_namelist_free(&savelist);
		return status;
	}

	im_destroy_list(IMH(handle), obj, obj->nio_props.nipl_val[prop_index].nip_name, savelist);
	ni_namelist_free(&savelist);
	im_create_list(IMH(handle), obj, obj->nio_props.nipl_val[prop_index].nip_name, obj->nio_props.nipl_val[prop_index].nip_val);

	*id = obj->nio_id;
	obj_free(obj);

	file_notify(FH(handle));

	return status;
}

ni_status
ni_createprop(void *handle, ni_id *id, ni_property prop, ni_index where)
{
	ni_status status;
	ni_object *obj;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	status = ni_validate_dir(handle, obj, 1);
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	/*
	 * Add property 
	 */
	ni_proplist_insert(&obj->nio_props, prop, where);
	
	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	*id = obj->nio_id;
	im_create_list(IMH(handle), obj, prop.nip_name, prop.nip_val);
	obj_free(obj);

	file_notify(FH(handle));

	return NI_OK;
}

ni_status
ni_destroyprop(void *handle, ni_id *id, ni_index prop_index)
{
	ni_status status;
	ni_object *obj;
	ni_property saveprop;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	status = ni_validate_dir(handle, obj, 1);
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	/*
	 * Save property, zero out old pointers so ni_proplist_delete()
	 * doesn't free them.
	 */
	saveprop = ni_prop_dup(obj->nio_props.nipl_val[prop_index]);
	MM_ZERO(&obj->nio_props.nipl_val[prop_index]);
	ni_proplist_delete(&obj->nio_props, prop_index);

	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		obj_free(obj);
		ni_prop_free(&saveprop);
		return status;
	}

	*id = obj->nio_id;
	im_destroy_list(IMH(handle), obj, saveprop.nip_name, saveprop.nip_val);
	ni_prop_free(&saveprop);
	obj_free(obj);

	file_notify(FH(handle));

	return NI_OK;
}

ni_status
ni_renameprop(void *handle, ni_id *id, ni_index prop_index, ni_name_const name)
{
	ni_status status;
	ni_object *obj;
	ni_name savename;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	status = ni_validate_dir(handle, obj, 1);
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	savename = obj->nio_props.nipl_val[prop_index].nip_name;
	obj->nio_props.nipl_val[prop_index].nip_name = ni_name_dup(name);

	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		ni_name_free(&savename);
		obj_free(obj);
		return status;
	}

	im_destroy_list(IMH(handle), obj, savename, obj->nio_props.nipl_val[prop_index].nip_val);
	ni_name_free(&savename);
	im_create_list(IMH(handle), obj, obj->nio_props.nipl_val[prop_index].nip_name, obj->nio_props.nipl_val[prop_index].nip_val);
	
	*id = obj->nio_id;
	obj_free(obj);

	file_notify(FH(handle));

	return NI_OK;
}

ni_status
ni_listprops(void *handle, ni_id *id, ni_namelist *propnames)
{
	ni_status status;
	ni_object *obj;
	ni_index i;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	propnames->ninl_len = 0;
	propnames->ninl_val = NULL;

	for (i = 0; i < obj->nio_props.nipl_len; i++)
	{
		ni_namelist_insert(propnames, obj->nio_props.nipl_val[i].nip_name, NI_INDEX_NULL);
	}

	*id = obj->nio_id;
	obj_free(obj);

	return NI_OK;
}

ni_status
ni_createname(void *handle, ni_id *id, ni_index prop_index, ni_name_const name, ni_index where)
{
	ni_status status;
	ni_object *obj;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	/* check for directory access */
	status = ni_validate_dir(handle, obj, 0);
	if (status != NI_OK)
	{
		/* no directory access - check for access to this property */
		status = ni_validate_name(handle, obj, prop_index);
	}

	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	ni_namelist_insert(&obj->nio_props.nipl_val[prop_index].nip_val, name, where);
	
	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	im_create(IMH(handle), obj,  obj->nio_props.nipl_val[prop_index].nip_name, name, where);
	*id = obj->nio_id;
	obj_free(obj);

	file_notify(FH(handle));

	return NI_OK;
}

ni_status
ni_destroyname(void *handle, ni_id *id, ni_index prop_index, ni_index name_index)
{
	ni_status status;
	ni_object *obj;
	ni_namelist *nl;
	ni_name savename;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	nl = &obj->nio_props.nipl_val[prop_index].nip_val;
	if (name_index >= nl->ninl_len)
	{
		obj_free(obj);
		return NI_NONAME;
	}

	/* check for directory access */
	status = ni_validate_dir(handle, obj, 0);
	if (status != NI_OK)
	{
		/* no directory access - check for access to this property */
		status = ni_validate_name(handle, obj, prop_index);
	}
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	/* 
	 * Copy name and set to NULL so ni_namelist_free() doesn't free it.
	 */
	savename = nl->ninl_val[name_index];
	nl->ninl_val[name_index] = NULL; 
	ni_namelist_delete(nl, name_index);
	
	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		obj_free(obj);
		ni_name_free(&savename);
		return status;
	}

	*id = obj->nio_id;
	im_destroy(IMH(handle), obj, obj->nio_props.nipl_val[prop_index].nip_name, savename, name_index);
	ni_name_free(&savename);
	obj_free(obj);

	return NI_OK;
}

ni_index
ni_highestid(void *handle)
{
	return file_highestid(FH(handle));
}

ni_status
ni_writename(void *handle, ni_id *id, ni_index prop_index, ni_index name_index, ni_name_const name)
{
	ni_status status;
	ni_object *obj;
	ni_namelist *nl;
	ni_name savename;

	status = obj_lookup(handle, id, NIOP_WRITE, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len) return NI_NOPROP;
	
	nl = &obj->nio_props.nipl_val[prop_index].nip_val;
	if (name_index >= nl->ninl_len)
	{
		obj_free(obj);
		return NI_NONAME;
	}

	/* check for directory access */
	status = ni_validate_dir(handle, obj, 0);
	if (status != NI_OK)
	{
		/* no directory access - check for access to this property */
		status = ni_validate_name(handle, obj, prop_index);
	}
	if (status != NI_OK)
	{
		obj_free(obj);
		return status;
	}

	savename = nl->ninl_val[name_index];
	nl->ninl_val[name_index] = ni_name_dup(name);
	
	status = file_write(FH(handle), obj);
	if (status != NI_OK)
	{
		obj_free(obj);
		ni_name_free(&savename);
		return status;
	}

	im_destroy(IMH(handle), obj, obj->nio_props.nipl_val[prop_index].nip_name, savename, name_index);
	ni_name_free(&savename);
	im_create(IMH(handle), obj, obj->nio_props.nipl_val[prop_index].nip_name, name, name_index);

	*id = obj->nio_id;
	obj_free(obj);

	file_notify(FH(handle));

	return NI_OK;
}

ni_status
ni_readname(void *handle, ni_id *id, ni_index prop_index, ni_index name_index, ni_name *name)
{
	ni_status status;
	ni_object *obj;
	ni_namelist *nl;

	status = obj_lookup(handle, id, NIOP_READ, &obj);
	if (status != NI_OK) return status;

	if (prop_index >= obj->nio_props.nipl_len)
	{
		obj_free(obj);
		return NI_NOPROP;
	}

	nl = &obj->nio_props.nipl_val[prop_index].nip_val;
	if (name_index >= nl->ninl_len)
	{
		obj_free(obj);
		return NI_NONAME;
	}

	*name = ni_name_dup(nl->ninl_val[name_index]);
	obj_free(obj);

	return NI_OK;
}

