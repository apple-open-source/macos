/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * fstab entry lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <fstab.h>
#include <pthread.h>
#include "lu_utils.h"
#include "lu_overrides.h"

#define ENTRY_SIZE sizeof(struct fstab)
#define ENTRY_KEY _li_data_key_fstab

static pthread_mutex_t _fstab_lock = PTHREAD_MUTEX_INITIALIZER;

#define FS_GET_SPEC 1
#define FS_GET_FILE 2
#define FS_GET_ENT 3

static struct fstab *
copy_fstab(struct fstab *in)
{
	if (in == NULL) return NULL;

	return (struct fstab *)LI_ils_create("sssss44", in->fs_spec, in->fs_file, in->fs_vfstype, in->fs_mntops, in->fs_type, in->fs_freq, in->fs_passno);
}

/*
 * Extract the next fstab entry from a kvarray.
 */
static void *
extract_fstab(kvarray_t *in)
{
	struct fstab tmp;
	uint32_t d, k, kcount;

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	memset(&tmp, 0, ENTRY_SIZE);

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "fs_spec"))
		{
			if (tmp.fs_spec != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_spec = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "fs_file"))
		{
			if (tmp.fs_file != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_file = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "fs_vfstype"))
		{
			if (tmp.fs_vfstype != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_vfstype = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "fs_mntops"))
		{
			if (tmp.fs_mntops != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_mntops = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "fs_type"))
		{
			if (tmp.fs_type != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_type = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "fs_freq"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.fs_freq = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "fs_passno"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.fs_passno = atoi(in->dict[d].val[k][0]);
		}
	}

	if (tmp.fs_spec == NULL) tmp.fs_spec = "";
	if (tmp.fs_file == NULL) tmp.fs_file = "";
	if (tmp.fs_vfstype == NULL) tmp.fs_vfstype = "";
	if (tmp.fs_mntops == NULL) tmp.fs_mntops = "";
	if (tmp.fs_type == NULL) tmp.fs_type = "";

	return copy_fstab(&tmp);
}

static struct fstab *
ds_getfsspec(const char *name)
{
	static int proc = -1;

	return (struct fstab *)LI_getone("getfsbyname", &proc, extract_fstab, "name", name);
}

static void
ds_endfsent(void)
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static int
ds_setfsent(void)
{
	ds_endfsent();
	return 1;
}

static struct fstab *
ds_getfsent()
{
	static int proc = -1;

	return (struct fstab *)LI_getent("getfsent", &proc, extract_fstab, ENTRY_KEY, ENTRY_SIZE);
}

static struct fstab *
ds_getfsfile(const char *name)
{
	struct fstab *fs;

	if (name == NULL) return NULL;

	ds_setfsent();

	for (fs = ds_getfsent(); fs != NULL; fs = ds_getfsent())
	{
		if (!strcmp(fs->fs_file, name)) return fs;
	}

	ds_endfsent();

	return NULL;
}

static struct fstab *
getfs(const char *spec, const char *file, int source)
{
	struct fstab *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running())
	{
		switch (source)
		{
			case FS_GET_SPEC:
				res = ds_getfsspec(spec);
				break;
			case FS_GET_FILE:
				res = ds_getfsfile(file);
				break;
			case FS_GET_ENT:
				res = ds_getfsent();
			break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_fstab_lock);

		switch (source)
		{
			case FS_GET_SPEC:
				res = copy_fstab(_old_getfsspec(spec));
				break;
			case FS_GET_FILE:
				res = copy_fstab(_old_getfsfile(file));
				break;
			case FS_GET_ENT:
				res = copy_fstab(_old_getfsent());
				break;
			default: res = NULL;
		}

		pthread_mutex_unlock(&_fstab_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct fstab *)tdata->li_entry;
}


struct fstab *
getfsbyname(const char *name)
{
	return getfs(name, NULL, FS_GET_SPEC);
}

struct fstab *
getfsspec(const char *name)
{
	return getfs(name, NULL, FS_GET_SPEC);
}

struct fstab *
getfsfile(const char *name)
{
	return getfs(NULL, name, FS_GET_FILE);
}

struct fstab *
getfsent(void)
{
	return getfs(NULL, NULL, FS_GET_ENT);
}

int
setfsent(void)
{
	if (_ds_running()) return (ds_setfsent());
	return (_old_setfsent());
}

void
endfsent(void)
{
	if (_ds_running()) ds_endfsent();
	else _old_endfsent();
}
