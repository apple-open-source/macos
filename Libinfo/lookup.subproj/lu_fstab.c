/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <fstab.h>

#include "lookup.h"
#include "_lu_types.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static struct fstab global_fs;
static int global_free = 1;
static char *fs_data = NULL;
static unsigned fs_datalen = 0;
static int fs_nentries = 0;
static int fs_start = 1;
static XDR fs_xdr = { 0 };

static void
freeold(void)
{
	if (global_free == 1) return;

	free(global_fs.fs_spec);
	free(global_fs.fs_file);
	free(global_fs.fs_type);
	free(global_fs.fs_vfstype);
	free(global_fs.fs_mntops);

	global_free = 1;
}

static void
convert_fs(_lu_fsent *lu_fs)
{
	freeold();

	global_fs.fs_spec = strdup(lu_fs->fs_spec);
	global_fs.fs_file = strdup(lu_fs->fs_file);

	/*
	 * Special case - if vfstype is unknown and spec is
	 * of the form foo:bar, then assume nfs.
	 */
	if (lu_fs->fs_vfstype[0] == '\0')
	{
		if (strchr(lu_fs->fs_spec, ':') != NULL)
		{
			global_fs.fs_vfstype = malloc(4);
			strcpy(global_fs.fs_vfstype, "nfs");
		}
		else global_fs.fs_vfstype = strdup(lu_fs->fs_vfstype);
	}
	else
	{
		global_fs.fs_vfstype = strdup(lu_fs->fs_vfstype);
	}

	global_fs.fs_mntops = strdup(lu_fs->fs_mntops);
	global_fs.fs_type = strdup(lu_fs->fs_type);
	global_fs.fs_freq = lu_fs->fs_freq;
	global_fs.fs_passno = lu_fs->fs_passno;

	global_free = 0;
}

static struct fstab *
lu_getfsbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_fsent_ptr lu_fs;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getfsbyname", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &name))
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen,
		XDR_DECODE);
	lu_fs = NULL;
	if (!xdr__lu_fsent_ptr(&inxdr, &lu_fs) || (lu_fs == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_fs(lu_fs);
	xdr_free(xdr__lu_fsent_ptr, &lu_fs);
	return (&global_fs);
}

static void
lu_endfsent(void)
{
	fs_nentries = 0;
	if (fs_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)fs_data, fs_datalen);
		fs_data = NULL;
	}
}

static int
lu_setfsent(void)
{
	lu_endfsent();
	fs_start = 1;
	return (1);
}

static struct fstab *
lu_getfsent()
{
	static int proc = -1;
	_lu_fsent lu_fs;

	if (fs_start == 1)
	{
		fs_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getfsent", &proc) !=
				KERN_SUCCESS)
			{
				lu_endfsent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &fs_data, &fs_datalen)
			!= KERN_SUCCESS)
		{
			lu_endfsent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		fs_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&fs_xdr, fs_data,
			fs_datalen, XDR_DECODE);
		if (!xdr_int(&fs_xdr, &fs_nentries))
		{
			xdr_destroy(&fs_xdr);
			lu_endfsent();
			return (NULL);
		}
	}

	if (fs_nentries == 0)
	{
		xdr_destroy(&fs_xdr);
		lu_endfsent();
		return (NULL);
	}

	bzero(&lu_fs, sizeof(lu_fs));
	if (!xdr__lu_fsent(&fs_xdr, &lu_fs))
	{
		xdr_destroy(&fs_xdr);
		lu_endfsent();
		return (NULL);
	}

	fs_nentries--;
	convert_fs(&lu_fs);
	xdr_free(xdr__lu_fsent, &lu_fs);
	return (&global_fs);
}

struct fstab *
lu_getfsspec(const char *name)
{
	if (name == NULL) return (struct fstab *)NULL;
	return lu_getfsbyname(name);
}

struct fstab *
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

struct fstab *
getfsbyname(const char *name)
{
	if (_lu_running()) return (lu_getfsbyname(name));
	return (NULL);
}

struct fstab *
getfsent(void)
{
	if (_lu_running()) return (lu_getfsent());
	return (_old_getfsent());
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

struct fstab *
getfsspec(const char *name)
{
	if (_lu_running()) return (lu_getfsspec(name));
	return (_old_getfsspec(name));
}

struct fstab *
getfsfile(const char *name)
{
	if (_lu_running()) return (lu_getfsfile(name));
	return (_old_getfsfile(name));
}
