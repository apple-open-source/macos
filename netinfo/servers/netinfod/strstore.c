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
 * String storage
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * Simple scheme to store strings in binary tree and reference count them.
 */
#include <stdlib.h>
#include <string.h>
#include "ni_globals.h"
#include <NetInfo/system_log.h>
#include "strstore.h"
#include "ranstrcmp.h"

typedef struct strtreenode *strtree;

typedef struct strtreenode {
	char *string;
	int refcount;
	strtree left;
	strtree right;
} strtreenode;


static strtree
treealloc(const char *string) 
{
	strtree res;
	int len;

	res = malloc(sizeof(*res));
	len = strlen(string);
	res->string = malloc(len + 1);
	memmove(res->string, string, len);
	res->string[len] = '\0';
	res->refcount = 1;
	res->left = NULL;
	res->right = NULL;
	return (res);
}

static const char *
strstore(strtree *thetree, const char *string)
{
	int res;

	if (*thetree == NULL) {
		*thetree = treealloc(string);
		return ((*thetree)->string);
	}
	res = ranstrcmp(string, (*thetree)->string);
	if (res < 0) {
		return (strstore(&(*thetree)->left, string));
	} else if (res == 0) {
		(*thetree)->refcount++;
		return ((*thetree)->string);
	} else if (res > 0) {
		return (strstore(&(*thetree)->right, string));
	}
	system_log(LOG_DEBUG, "strstore ranstrcmp impossible result");
	return (NULL);
}


static void
insertnode(strtree *thetree, strtree thenode)
{
	int res;

	if (*thetree == NULL) {
		*thetree = thenode;
		return;
	}
	res = ranstrcmp(thenode->string, (*thetree)->string);
	if (res < 0) {
		insertnode(&(*thetree)->left, thenode);
	} else if (res == 0) {
		system_log(LOG_DEBUG, "insertnode something already there");
	} else if (res > 0) {
		insertnode(&(*thetree)->right, thenode);
	}
}

static unsigned
pickleft(strtree *thetree)
{
	unsigned long x = (unsigned long)thetree;
	unsigned count;

	for (count = 0; x > 0; x >>= 1) {
		count += x & 1;
	}
	return (count & 1);
}

static void
removenode(strtree *thetree)
{
	strtree save;

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
	free(save->string);
	free(save);
}

static void
strdelete(strtree *thetree, const char *string)
{
	int res;

	if (*thetree == NULL) {
		system_log(LOG_DEBUG, "strdelete tree already deleted");
		return;
	}
	res = ranstrcmp(string, (*thetree)->string);
	if (res < 0) {
		strdelete(&(*thetree)->left, string);
	} else if (res == 0) {
		if ((*thetree)->refcount > 0) {
			(*thetree)->refcount--;
		}
		if ((*thetree)->refcount == 0) {
			removenode(thetree);
		}
	} else if (res > 0) {
		strdelete(&(*thetree)->right, string);
	}
}


static strtree handle = NULL;

const char *
ss_alloc(
	 const char *string
	 )
{
	return (strstore(&handle, string));
}

void
ss_unalloc(
	   const char *string
	   )
{
	strdelete(&handle, string);
}

