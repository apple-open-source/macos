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

#include <stdlib.h>
#include <string.h>
#include <netinfo/ni.h>
#include "index.h"
#include "index_manager.h"
#include "listops.h"
#include <NetInfo/mm.h>
#include "strstore.h"

typedef struct im_holder {
	ni_index id;
	int refcount;
	ni_name propname;
	index_handle *index;
	ni_entrylist *entries;
} im_holder;

typedef struct im_private {
	ni_index nheld;
	im_holder *held;
} im_private;

static im_holder *findholder(im_handle *, ni_index, ni_name_const);

im_handle
im_alloc(
	 void
	 )
{
	im_private *res;
	im_handle handle;

	res = malloc(sizeof(*res));
	res->nheld = 0;
	res->held = NULL;
	handle.private = res;
	return (handle);
}

static void
im_holder_unalloc(
		  im_holder *held
		  )
{
	ss_unalloc(held->propname);
	if (held->index != NULL) {
		index_unalloc(held->index);
		free(held->index);
	}
	if (held->entries != NULL) {
		im_entrylist_unalloc(held->entries);
		free(held->entries);
	}
}

void
im_forget(
	  im_handle *handle
	  )
{
	ni_index i;
	im_private *priv = (im_private *)handle->private;

	for (i = 0; i < priv->nheld; i++) im_holder_unalloc(&priv->held[i]);

	free(priv->held);
	priv->held = NULL;
	priv->nheld = 0;
}

void
im_free(
	im_handle *handle
	)
{
	im_private *priv = (im_private *)handle->private;
	ni_index i;

	for (i = 0; i < priv->nheld; i++) {
		im_holder_unalloc(&priv->held[i]);
	}
	free(priv->held);
	free(priv);
}

/*
** GRS - 12/18/92 - Cleaved im_create_all away from _im_create_all.
** im_create_all used to just call _im_create_all with shouldgrow = 0
** and where = NI_INDEX_NULL.  The problem with this was that when a 
** new directory was added, it didn't go into all the ni_list caches for 
** its parent, only the ones for which it had a property.  Later ni_lists
** could fail to see the newly-added directory.  
**
** Now I've put the old _im_create_all behavior into im_create_all, which
** is only called by ni_write.  In _im_create_all, called only by im_newnode,
** I've added a loop to iterate through all the cached entries so that 
** something is added for each property, even if the new directory doesn't
** have a matching property.
*/

void
im_create_all(
    im_handle *handle,
    ni_object *obj,
    ni_proplist props
    )
{
    im_holder *held;
    ni_index i;

    for (i = 0; i < props.nipl_len; i++) {
	held = findholder(handle, obj->nio_parent, 
	    props.nipl_val[i].nip_name);
	if (held == NULL) {
	    continue;
	}
	if (held->index != NULL) {
	    index_insert_list(held->index, 
	    props.nipl_val[i].nip_val,
	    obj->nio_id.nii_object);
	}
	if (held->entries != NULL) {
	    list_insert(held->entries,
	    props.nipl_val[i].nip_val,
	    obj->nio_id.nii_object, 0,
	    NI_INDEX_NULL);
		
	}
    }
}


static void
_im_create_all(
    im_handle *handle,
    ni_object *obj,
    ni_proplist props,
    int shouldgrow,
    ni_index where
    )
{
    im_holder *held;
    ni_index i, p;
    im_private *priv = (im_private *)handle->private;
    ni_name prop_name;
    int found;
    ni_namelist nl = {0, NULL};

    for (p = 0; p < priv->nheld; p++) {
	if (!(held = &priv->held[p]) || !held->propname) {
	    continue;
	}
	if (held->id == obj->nio_parent) {
	    found = 0;
	    for (i = 0; i < props.nipl_len; i++) {
		prop_name = props.nipl_val[i].nip_name;
		if (ni_name_match(held->propname, prop_name)) {
		    held->refcount++;
		    if (held->index != NULL) {
			index_insert_list(held->index, 
			    props.nipl_val[i].nip_val,
			    obj->nio_id.nii_object);
			}
		    if (held->entries != NULL) {
			list_insert(held->entries,
			    props.nipl_val[i].nip_val,
			    obj->nio_id.nii_object, shouldgrow,
			    where);
			
		    }
		    found = 1;
		    break;
		}
	    }
	    if (!found && (held->entries != NULL)) {
		list_insert(held->entries, nl, obj->nio_id.nii_object, 
		    shouldgrow, where);
	    }
	}
    }
}

void
im_newnode(
	   im_handle *handle,
	   ni_object *obj,
	   ni_index where
	   )
{
	return (_im_create_all(handle, obj, obj->nio_props, 1, where));
}

static void
_im_destroy_all(
		im_handle *handle,
		ni_object *obj,
		ni_proplist props,
		int shouldshrink
		)
{
	im_holder *held;
	ni_index i;
	ni_index j;

	for (j = 0; j < props.nipl_len; j++) {
		held = findholder(handle, obj->nio_parent, 
				 props.nipl_val[j].nip_name);
		if (held == NULL) {
			continue;
		}
		for (i = 0; i < props.nipl_val[j].nip_val.ninl_len; i++) {
			if (held->index != NULL) {
				index_delete(held->index, 
					     props.nipl_val[j].nip_val.ninl_val[i], 
					     obj->nio_id.nii_object);
			}
		}
		if (held->entries != NULL) {
			list_delete(held->entries,
				    obj->nio_id.nii_object, shouldshrink);
		}
	}
}

void
im_destroy_all(
		im_handle *handle,
		ni_object *obj,
		ni_proplist props
		)
{
	return (_im_destroy_all(handle, obj, props, 0));
}

void
im_remove(
	  im_handle *handle,
	  ni_object *obj
	  )
{
	im_private *priv = (im_private *)handle->private;
	ni_index i;
	im_holder *save;

	for (i = 0; i < priv->nheld; i++) {
		if (priv->held[i].id != obj->nio_id.nii_object) {
			continue;
		}

		if (priv->held[i].index != NULL) {
			index_unalloc(priv->held[i].index);
			free(priv->held[i].index);
		}
		if (priv->held[i].entries != NULL) {
			im_entrylist_unalloc(priv->held[i].entries);
			free(priv->held[i].entries);
		}
		ss_unalloc(priv->held[i].propname);
		
		save = &priv->held[priv->nheld - 1];
		priv->held[i] = *save;

		MM_SHRINK_ARRAY(priv->held, priv->nheld);
		priv->nheld--;
	}
	_im_destroy_all(handle, obj, obj->nio_props, 1);
}


int
im_has_indexed_dir(
		   im_handle *handle,
		   ni_index which,
		   ni_name_const key,
		   ni_name_const val,
		   ni_index **dirs,
		   ni_index *ndirs
		   )
{
	im_holder *held;

	held = findholder(handle, which, key);
	if (held == NULL || held->index == NULL) {
		return (0);
	}
	*ndirs = index_lookup(*held->index, val, dirs);
	return (1);
}


void
im_store_index(
	       im_handle *handle,
	       index_handle index,
	       ni_index which,
	       ni_name_const val
	       )
{
	im_private *priv = (im_private *)handle->private;
	im_holder *held;

	held = findholder(handle, which, val);
	if (held == NULL) {
		MM_GROW_ARRAY(priv->held, priv->nheld);
		held = &priv->held[priv->nheld];
		priv->nheld++;
		held->id = which;
		held->propname = (char *)ss_alloc(val);
		held->entries = NULL;
		held->refcount = 0;
	}
	held->index = malloc(sizeof(index));
	*held->index = index;
}


void
im_destroy_list(
		im_handle *handle,
		ni_object *obj,
		ni_name_const key,
		ni_namelist vals
		)
{
	im_holder *held;
	ni_index i;

	held = findholder(handle, obj->nio_parent, key);
	if (held == NULL) {
		return;
	}
	for (i = 0; i < vals.ninl_len; i++) {
		if (held->index != NULL) {
			index_delete(held->index, vals.ninl_val[i], obj->nio_id.nii_object);
		}
	}
	if (held->entries != NULL) {
		list_delete(held->entries, 
			    obj->nio_id.nii_object, 0);
	}
}


void
im_create_list(
	       im_handle *handle,
	       ni_object *obj,
	       ni_name_const key,
	       ni_namelist vals
	       )
{
	im_holder *held;

	held = findholder(handle, obj->nio_parent, key);
	if (held == NULL) {
		return;
	}
	if (held->index != NULL) {
		index_insert_list(held->index, vals, obj->nio_id.nii_object);
	}
	if (held->entries != NULL) {
		list_insert(held->entries, vals, obj->nio_id.nii_object, 0,
			    NI_INDEX_NULL);
	}
}


void
im_destroy(
	   im_handle *handle,
	   ni_object *obj,
	   ni_name_const key,
	   ni_name_const val,
	   ni_index where
	   )
{
	im_holder *held;

	held = findholder(handle, obj->nio_parent, key);
	if (held == NULL) {
		return;
	}
	if (held->index != NULL) {
		index_delete(held->index, val, obj->nio_id.nii_object);
	}
	if (held->entries != NULL) {
		list_delete_one(held->entries, obj->nio_id.nii_object, where);
	}
}


void
im_create(
	  im_handle *handle,
	  ni_object *obj,
	  ni_name_const key,
	  ni_name_const val,
	  ni_index where
	  )
{
	im_holder *held;

	held = findholder(handle, obj->nio_parent, key);
	if (held == NULL) {
		return;
	}
	if (held->index != NULL) {
		index_insert(held->index, val, obj->nio_id.nii_object);
	}
	if (held->entries != NULL) {
		list_insert_one(held->entries, val, obj->nio_id.nii_object, where);
	}
}

static im_holder *
findholder(
	  im_handle *handle, 
	  ni_index which, 
	  ni_name_const key
	  )
{
	im_private *priv = (im_private *)handle->private;
	ni_index i;

	for (i = 0; i < priv->nheld; i++) {
		if (priv->held[i].id == which &&
		    ni_name_match(priv->held[i].propname, key)) {
			priv->held[i].refcount++;
			return (&priv->held[i]);
		}
	}
	return (NULL);
}


#ifdef notdef
static ni_entrylist
ni_entrylist_dup(
		 ni_entrylist entries
		 )
{
	ni_index i;
	ni_entry *entry;
	ni_entrylist ret;
	
	ret.niel_len = entries.niel_len;
	ret.niel_val = malloc(entries.niel_len * sizeof(entries.niel_val[0]));
	for (i = 0; i < entries.niel_len; i++) {
		entry = &entries.niel_val[i];
		ret.niel_val[i].id = entry->id;
		if (entry->names != NULL) {
			ret.niel_val[i].names = malloc(sizeof(ni_namelist));
			(*ret.niel_val[i].names = 
			 ni_namelist_dup(*entry->names));
		} else {
			ret.niel_val[i].names = NULL;
		}
	}
	return (ret);

}
#endif

int 
im_has_saved_list(
		  im_handle *handle, 
		  ni_index id, 
		  ni_name_const pname, 
		  ni_entrylist *entries
		  )
{
	im_holder *held;

	held = findholder(handle, id, pname);
	if (held == NULL || held->entries == NULL) {
		return (0);
	}
	*entries = *held->entries;
	return (1);
}

void 
im_store_list(
	      im_handle *handle, 
	      ni_index id, 
	      ni_name_const pname, 
	      ni_entrylist entries
	      )
{
	im_private *priv = (im_private *)handle->private;
	im_holder *held;

	held = findholder(handle, id, pname);
	if (held == NULL) {
		MM_GROW_ARRAY(priv->held, priv->nheld);
		held = &priv->held[priv->nheld];
		priv->nheld++;
		held->id = id;
		held->propname = (char *)ss_alloc(pname);
		held->index = NULL;
		held->refcount = 0;
	}
	held->entries = malloc(sizeof(ni_entrylist));
	*held->entries = im_entrylist_dup(entries);
}
