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

static void
im_namelist_unalloc(
		    ni_namelist *nl
		    )
{
	int i;

	for (i = 0; i < nl->ninl_len; i++) {
		ss_unalloc(nl->ninl_val[i]);
		nl->ninl_val[i] = NULL;
	}
}

void
im_entrylist_unalloc(
		     ni_entrylist *entries
		     )
{
	ni_index i;
	ni_entry *entry;
	
	for (i = 0; i < entries->niel_len; i++) {
		entry = &entries->niel_val[i];
		if (entry->names != NULL) {
			im_namelist_unalloc(entry->names);
			ni_namelist_free(entry->names);
		}
	}
}

static ni_namelist *
im_namelist_dup(
		ni_namelist nl
		)
{
	ni_namelist *ret;
	int i;

	ret = malloc(sizeof(*ret));

	ret->ninl_len = nl.ninl_len;
	if (ret->ninl_len == 0) {
		ret->ninl_val = NULL;
		return (ret);
	}
	ret->ninl_val = malloc(nl.ninl_len * sizeof(nl.ninl_val[0]));
	for (i = 0; i < nl.ninl_len; i++) {
		ret->ninl_val[i] = (ni_name)ss_alloc(nl.ninl_val[i]);
	}
	return (ret);
}

ni_entrylist
im_entrylist_dup(
		 ni_entrylist entries
		 )
{
	ni_index i;
	ni_entry *entry;
	ni_entrylist ret;
	
	ret.niel_len = entries.niel_len;
	if (entries.niel_len == 0) {
		ret.niel_val = NULL;
		return (ret);
	}
	ret.niel_val = malloc(entries.niel_len * sizeof(entries.niel_val[0]));
	for (i = 0; i < entries.niel_len; i++) {
		entry = &entries.niel_val[i];
		ret.niel_val[i].id = entry->id;
		if (entry->names != NULL) {
			ret.niel_val[i].names = im_namelist_dup(*entry->names);
		} else {
			ret.niel_val[i].names = NULL;
		}
	}
	return (ret);
}


void
list_insert(
	    ni_entrylist *entries, 
	    ni_namelist nl, 
	    ni_index which,
	    int shouldgrow,
	    ni_index where
	    )
{
	ni_index i;
	ni_entry *entry;

	for (i = 0; i < entries->niel_len; i++) {
		entry = &entries->niel_val[i];
		if (entry->id == which) {
			if (entry->names != NULL) {
				im_namelist_unalloc(entry->names);
				ni_namelist_free(entry->names);
				free(entry->names);
			}
			entry->names = im_namelist_dup(nl);
			return;
		}
	}
	if (shouldgrow) {
		MM_GROW_ARRAY(entries->niel_val, entries->niel_len);
		for (i = entries->niel_len; i > where; i--) {
			entries->niel_val[i] = entries->niel_val[i - 1];
		}
		entries->niel_val[i].id = which;
		entries->niel_val[i].names = im_namelist_dup(nl);
		entries->niel_len++;
	}
	return;
}

void
list_delete(
	    ni_entrylist *entries, 
	    ni_index which,
	    int shouldshrink
	    )
{
	ni_index i;
	ni_entry *entry;

	for (i = 0; i < entries->niel_len; i++) {
		entry = &entries->niel_val[i];
		if (entry->id == which) {
			if (entry->names != NULL) {
				im_namelist_unalloc(entry->names);
				ni_namelist_free(entry->names);
				free(entry->names);
			}
			entry->names = NULL;
			if (shouldshrink) {
				ni_entrylist_delete(entries, i);
			}
			return;
		}
	}
	return;
}


static void
im_namelist_delete(
		   ni_namelist *nl,
		   ni_index which
		   )
{
	int i;

	ss_unalloc(nl->ninl_val[which]);
	for (i = which + 1; i < nl-> ninl_len; i++) {
		nl->ninl_val[i - 1] = nl->ninl_val[i];
	}
	MM_SHRINK_ARRAY(nl->ninl_val, nl->ninl_len--);
}

void
list_delete_one(
		ni_entrylist *entries, 
		ni_index which,
		ni_index where
		)
{
	ni_index i;
	ni_entry *entry;

	for (i = 0; i < entries->niel_len; i++) {
		entry = &entries->niel_val[i];
		if (entry->id == which) {
			if (entry->names != NULL) {
				im_namelist_delete(entry->names, where);
			}
			return;
		}
	}
	return;
}

static void
im_namelist_insert(
		   ni_namelist *nl,
		   ni_name_const nm,
		   ni_index where
		   )
{
	ni_index i;

	MM_GROW_ARRAY(nl->ninl_val, nl->ninl_len);
	for (i = nl->ninl_len; i > where; i--) {
		nl->ninl_val[i] = nl->ninl_val[i - 1];
	}
	nl->ninl_val[i] = (ni_name)ss_alloc(nm);
	nl->ninl_len++;
}

void
list_insert_one(
		ni_entrylist *entries, 
		ni_name_const nm, 
		ni_index which,
		ni_index where
		)
{
	ni_index i;
	ni_entry *entry;

	for (i = 0; i < entries->niel_len; i++) {
		entry = &entries->niel_val[i];
		if (entry->id == which) {
			if (entry->names != NULL) {
				im_namelist_insert(entry->names, nm, where);
			}
			return;
		}
	}
	return;
}









