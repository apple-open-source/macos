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
 * Directory Index
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * Make lookups (key = val) faster by storing vals in a binary tree.
 */
#include <stdlib.h>
#include <string.h>
#include <netinfo/ni.h>
#include "ni_globals.h"
#include <NetInfo/system.h>
#include "index.h"
#include "strstore.h"
#include "ranstrcmp.h"

#define index_compare(a, b) ranstrcmp(a, b)

#ifdef INDEX_DEBUG
#include <stdio.h>
#define debug(msg) system_log(LOG_ERR, "Error: %s", msg)
#else
#define debug(msg)
#endif

typedef struct itreenode *itree;

typedef struct itreenode {
	ni_name val;
	ni_index length;
	union {
		ni_index single;
		ni_index *multiple;
	} dir;
	itree left;
	itree right;
} itreenode;
	
#define ITREE(x) ((itree)((x).private))

#ifdef INDEX_DEBUG

void
_index_dump(itree tree, int level)
{
	int i;

	if (tree == NULL) {
		return;
	}

	for (i = 0; i < level; i++) {
		printf(" ");
	}
	printf("R:\n");
	_index_dump(tree->left, level + 1);

	for (i = 0; i < level; i++) {
		printf(" ");
	}
	printf("%s: ", tree->val);
	if (tree->length == 1) {
		printf("%d\n", tree->dir.single);
	} else {
		for (i = 0; i < tree->length; i++) {
			printf("%d ", tree->dir.multiple[i]);
		}
		printf("\n");
	}
	for (i = 0; i < level; i++) {
		printf(" ");
	}
	printf("L:\n");
	_index_dump(tree->right, level + 1);
}

void
index_dump(itree tree)
{
	_index_dump(tree, 0);
}
#endif

index_handle 
index_alloc(void)
{
	index_handle handle;

	handle.private = NULL;
	return (handle);
}


static void
freetree(itree tree)
{
	if (tree == NULL) {
		return;
	}
	ss_unalloc(tree->val);
	freetree(tree->left);
	freetree(tree->right);
	if (tree->length > 1) {
		free(tree->dir.multiple);
	}
	free(tree);
}

void
index_unalloc(index_handle *handle)
{
	itree tree = ITREE(*handle);
	
	freetree(tree);
}


static itree
treealloc(ni_name_const val, ni_index which)
{
	itree res;

	res = malloc(sizeof(*res));
	res->val = (char *)ss_alloc(val);
	res->length = 1;
	res->dir.single = which;
	res->left = NULL;
	res->right = NULL;
	return (res);
}

static void
adddir(itree thetree, ni_index which)
{
	ni_index i;
	ni_index save;

	if (thetree->length == 1) {
		if (thetree->dir.single == which) {
			/*
			 * already here
			 */
			return;
		}
		save = thetree->dir.single;
		thetree->dir.multiple = malloc(2 * sizeof(ni_index));
		thetree->dir.multiple[0] = save;
		thetree->dir.multiple[1] = which;
		thetree->length++;
	} else if (thetree->length > 1) {
		for (i = 0; i < thetree->length; i++) {
			if (thetree->dir.multiple[i] == which) {
				/*
				 * already here
				 */
				return;
			}
		}
		thetree->length++;
		thetree->dir.multiple = realloc(thetree->dir.multiple,
						(thetree->length * 
						 sizeof(ni_index)));
		thetree->dir.multiple[thetree->length - 1] = which;
	} else {
		debug("adddir tree length = 0");
	}
}

static void
remdir(itree thetree, ni_index which)
{
	ni_index i;
	ni_index save;
	
	if (thetree->length == 2) {
		if (thetree->dir.multiple[0] == which) {
			save = thetree->dir.multiple[1];
		} else if (thetree->dir.multiple[1] == which) {
			save = thetree->dir.multiple[0];
		} else {
			return;
		}
		free(thetree->dir.multiple);
		thetree->dir.single = save;
		thetree->length--;
	} else if (thetree->length > 2) {
		for (i = 0; i < thetree->length; i++) {
			if (thetree->dir.multiple[i] == which) {
				/*
				 * Found it. Remove from list.
				 */
				for (i++; i < thetree->length; i++) {
					(thetree->dir.multiple[i - 1] =
					 thetree->dir.multiple[i]);
				}
				thetree->length--;
				(thetree->dir.multiple = 
				 realloc(thetree->dir.multiple,
					 (thetree->length * 
					  sizeof(ni_index))));
				return;
			}
		}
	} else {
		debug("remdir tree length < 2");
	}
}



static void
_index_insert(itree *thetree, ni_name_const val, ni_index which)
{
	int res;

	if (*thetree == NULL) {
		*thetree = treealloc(val, which);
		return;
	}
	res = index_compare(val, (*thetree)->val);
	if (res < 0) {
		_index_insert(&(*thetree)->left, val, which);
	} else if (res == 0) {
		adddir(*thetree, which);
	} else if (res > 0) {
		_index_insert(&(*thetree)->right, val, which);
	}
}

void
index_insert(index_handle *handle, ni_name_const val, ni_index which)
{
	_index_insert((itree *)&handle->private, val, which);
}

void
index_insert_list(index_handle *handle, ni_namelist vals, ni_index which)
{
	ni_index i;
	
	for (i = 0; i < vals.ninl_len; i++) {
		_index_insert((itree *)&handle->private, vals.ninl_val[i], 
			      which);
	}
}


static void
insertnode(itree *thetree, itree thenode)
{
	int res;

	if (*thetree == NULL) {
		*thetree = thenode;
		return;
	}
	res = index_compare(thenode->val, (*thetree)->val);
	if (res < 0) {
		insertnode(&(*thetree)->left, thenode);
	} else if (res == 0) {
		debug("insert_node something already there");
	} else if (res > 0) {
		insertnode(&(*thetree)->right, thenode);
	}
}

static unsigned
pickleft(itree *thetree)
{
	unsigned long x = (unsigned long)thetree;
	unsigned count;

	for (count = 0; x > 0; x >>= 1) {
		count += x & 1;
	}
	return (count & 1);
}

static void
removenode(itree *thetree)
{
	itree save;

	save = *thetree;
	if ((*thetree)->left == NULL) {
		*thetree = (*thetree)->right;
	} else if ((*thetree)->right == NULL) {
		*thetree = (*thetree)->left;
	} else {
		if (pickleft(thetree)) {
			*thetree = (*thetree)->left;
			insertnode(thetree, save->right);
		} else {
			*thetree = (*thetree)->right;
			insertnode(thetree, save->left);
		}
	}
	save->left = NULL;
	save->right = NULL;
	freetree(save);
}

static void
_index_delete(itree *thetree, ni_name_const val, ni_index which)
{
	int res;

	if (*thetree == NULL) {
		debug("_index_delete tree already deleted");
		return;
	}
	res = index_compare(val, (*thetree)->val);
	if (res < 0) {
		_index_delete(&(*thetree)->left, val, which);
	} else if (res == 0) {
		if ((*thetree)->length > 1) {
			remdir(*thetree, which);
		} else {
			removenode(thetree);
		}
	} else if (res > 0) {
		_index_delete(&(*thetree)->right, val, which);
	}
}



void
index_delete(index_handle *handle, ni_name_const val, ni_index which)
{
	_index_delete((itree *)&handle->private, val, which);
}

static ni_index
_index_lookup(
	      itree tree,
	      ni_name_const val, 
	      ni_index **dirs
	      )
{
	int res;

	if (tree == NULL) {
		return (0);
	}
	res = index_compare(val, tree->val);
	if (res < 0) {
		return (_index_lookup(tree->left, val, dirs));
	} else if (res == 0) {
		if ((tree)->length > 1) {
			*dirs = tree->dir.multiple;
		} else {
			*dirs = &tree->dir.single;
		}
		return (tree->length);
	} else if (res > 0) {
		return (_index_lookup(tree->right, val, dirs));
	}
	debug("index_compare impossible result");
	return (0);
}

ni_index 
index_lookup(
	     index_handle handle, 
	     ni_name_const val, 
	     ni_index **dirs
	     )
{
	itree tree = ITREE(handle);
	
	return (_index_lookup(tree, val, dirs));
}

