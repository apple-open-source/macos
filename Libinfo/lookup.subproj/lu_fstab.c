/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <fstab.h>
#include <pthread.h>

#include "lookup.h"
#include "_lu_types.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static pthread_mutex_t _fstab_lock = PTHREAD_MUTEX_INITIALIZER;

#define FS_GET_SPEC 1
#define FS_GET_FILE 2
#define FS_GET_ENT 3

static void
free_fstab_data(struct fstab *f)
{
	if (f == NULL) return;

	if (f->fs_spec != NULL) free(f->fs_spec);
	if (f->fs_file != NULL) free(f->fs_file);
	if (f->fs_vfstype != NULL) free(f->fs_vfstype);
	if (f->fs_mntops != NULL) free(f->fs_mntops);
	if (f->fs_type != NULL) free(f->fs_type);
}

static void
free_fstab(struct fstab *f)
{
	if (f == NULL) return;
	free_fstab_data(f);
	free(f);
}

static void
free_lu_thread_info_fstab(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_fstab((struct fstab *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct fstab *
extract_fstab(XDR *xdr)
{
	int i, j, nkeys, nvals, status;
	char *key, **vals;
	struct fstab *f;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	f = (struct fstab *)calloc(1, sizeof(struct fstab));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_fstab(f);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((f->fs_spec == NULL) && (!strcmp("name", key)))
		{
			f->fs_spec = vals[0];
			j = 1;
		}
		else if ((f->fs_file == NULL) && (!strcmp("dir", key)))
		{
			f->fs_file = vals[0];
			j = 1;
		}
		else if ((f->fs_vfstype == NULL) && (!strcmp("vfstype", key)))
		{
			f->fs_vfstype = vals[0];
			j = 1;
		}
		else if ((f->fs_mntops == NULL) && (!strcmp("opts", key)))
		{
			f->fs_mntops = vals[0];
			j = 1;
		}
		else if ((f->fs_type == NULL) && (!strcmp("type", key)))
		{
			f->fs_type = vals[0];
			j = 1;
		}
		else if (!strcmp("freq", key))
		{
			f->fs_freq = atoi(vals[0]);
		}
		else if (!strcmp("passno", key))
		{
			f->fs_passno = atoi(vals[0]);
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (f->fs_spec == NULL) f->fs_spec = strdup("");
	if (f->fs_file == NULL) f->fs_file = strdup("");
	if (f->fs_vfstype == NULL) f->fs_vfstype = strdup("");
	if (f->fs_mntops == NULL) f->fs_mntops = strdup("");
	if (f->fs_type == NULL) f->fs_type = strdup("");

	return f;
}

static struct fstab *
copy_fstab(struct fstab *in)
{
	struct fstab *f;

	if (in == NULL) return NULL;

	f = (struct fstab *)calloc(1, sizeof(struct fstab));

	f->fs_spec = LU_COPY_STRING(in->fs_spec);
	f->fs_file = LU_COPY_STRING(in->fs_file);
	f->fs_vfstype = LU_COPY_STRING(in->fs_vfstype);
	f->fs_mntops = LU_COPY_STRING(in->fs_mntops);
	f->fs_type = LU_COPY_STRING(in->fs_type);

	f->fs_freq = in->fs_freq;
	f->fs_passno = in->fs_passno;

	return f;
}

static void
recycle_fstab(struct lu_thread_info *tdata, struct fstab *in)
{
	struct fstab *f;

	if (tdata == NULL) return;
	f = (struct fstab *)tdata->lu_entry;

	if (in == NULL)
	{
		free_fstab(f);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_fstab_data(f);

	f->fs_spec = in->fs_spec;
	f->fs_file = in->fs_file;
	f->fs_vfstype = in->fs_vfstype;
	f->fs_mntops = in->fs_mntops;
	f->fs_type = in->fs_type;
	f->fs_freq = in->fs_freq;
	f->fs_passno = in->fs_passno;

	free(in);
}

static struct fstab *
lu_getfsspec(const char *name)
{
	struct fstab *f;
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getfsbyname", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name))
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)namebuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	f = extract_fstab(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return f;
}

static void
lu_endfsent(void)
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_fstab, free_lu_thread_info_fstab);
	_lu_data_free_vm_xdr(tdata);
}

static int
lu_setfsent(void)
{
	lu_endfsent();
	return 1;
}

static struct fstab *
lu_getfsent()
{
	static int proc = -1;
	struct lu_thread_info *tdata;
	struct fstab *f;

	tdata = _lu_data_create_key(_lu_data_key_fstab, free_lu_thread_info_fstab);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_fstab, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getfsent", &proc) != KERN_SUCCESS)
			{
				lu_endfsent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endfsent();
			return NULL;
		}

		/* mig stubs measure size in words (4 bytes) */
		tdata->lu_vm_length *= 4;

		if (tdata->lu_xdr != NULL)
		{
			xdr_destroy(tdata->lu_xdr);
			free(tdata->lu_xdr);
		}
		tdata->lu_xdr = (XDR *)calloc(1, sizeof(XDR));

		xdrmem_create(tdata->lu_xdr, tdata->lu_vm, tdata->lu_vm_length, XDR_DECODE);
		if (!xdr_int(tdata->lu_xdr, &tdata->lu_vm_cursor))
		{
			lu_endfsent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endfsent();
		return NULL;
	}

	f = extract_fstab(tdata->lu_xdr);
	if (f == NULL)
	{
		lu_endfsent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return f;
}

static struct fstab *
lu_getfsfile(const char *name)
{
	struct fstab *fs;

	if (name == NULL) return (struct fstab *)NULL;

	setfsent();
	for (fs = lu_getfsent(); fs != NULL; fs = lu_getfsent())
		if (!strcmp(fs->fs_file, name)) return fs;

	endfsent();
	return (struct fstab *)NULL;
}

static struct fstab *
getfs(const char *spec, const char *file, int source)
{
	struct fstab *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_fstab, free_lu_thread_info_fstab);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_fstab, tdata);
	}

	if (_lu_running())
	{
		switch (source)
		{
			case FS_GET_SPEC:
				res = lu_getfsspec(spec);
				break;
			case FS_GET_FILE:
				res = lu_getfsfile(file);
				break;
			case FS_GET_ENT:
				res = lu_getfsent();
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

	recycle_fstab(tdata, res);
	return (struct fstab *)tdata->lu_entry;
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
	if (_lu_running()) return (lu_setfsent());
	return (_old_setfsent());
}

void
endfsent(void)
{
	if (_lu_running()) lu_endfsent();
	else _old_endfsent();
}

