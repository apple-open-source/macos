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
 * user information (passwd) lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <pwd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static pthread_mutex_t _user_lock = PTHREAD_MUTEX_INITIALIZER;

#define PW_GET_NAME 1
#define PW_GET_UID 2
#define PW_GET_ENT 3

static void
free_user_data(struct passwd *p)
{
	if (p == NULL) return;

	if (p->pw_name != NULL) free(p->pw_name);
	if (p->pw_passwd != NULL) free(p->pw_passwd);
	if (p->pw_class != NULL) free(p->pw_class);
	if (p->pw_gecos != NULL) free(p->pw_gecos);
	if (p->pw_dir != NULL) free(p->pw_dir);
	if (p->pw_shell != NULL) free(p->pw_shell);
}

static void
free_user(struct passwd *p)
{
	if (p == NULL) return;
	free_user_data(p);
	free(p);
}

static void
free_lu_thread_info_user(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_user((struct passwd *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct passwd *
extract_user(XDR *xdr)
{
	int i, j, nvals, nkeys, status;
	char *key, **vals;
	struct passwd *p;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	p = (struct passwd *)calloc(1, sizeof(struct passwd));

	p->pw_uid = -2;
	p->pw_gid = -2;

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_user(p);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((p->pw_name == NULL) && (!strcmp("name", key)))
		{
			p->pw_name = vals[0];
			j = 1;
		}
		else if ((p->pw_passwd == NULL) && (!strcmp("passwd", key)))
		{
			p->pw_passwd = vals[0];
			j = 1;
		}
		else if ((p->pw_class == NULL) && (!strcmp("class", key)))
		{
			p->pw_class = vals[0];
			j = 1;
		}
		else if ((p->pw_gecos == NULL) && (!strcmp("realname", key)))
		{
			p->pw_gecos = vals[0];
			j = 1;
		}
		else if ((p->pw_dir == NULL) && (!strcmp("home", key)))
		{
			p->pw_dir = vals[0];
			j = 1;
		}
		else if ((p->pw_shell == NULL) && (!strcmp("shell", key)))
		{
			p->pw_shell = vals[0];
			j = 1;
		}
		else if ((p->pw_uid == -2) && (!strcmp("uid", key)))
		{
			p->pw_uid = atoi(vals[0]);
		}
		else if ((p->pw_gid == -2) && (!strcmp("gid", key)))
		{
			p->pw_gid = atoi(vals[0]);
		}
		else if (!strcmp("change", key))
		{
			p->pw_change = atoi(vals[0]);
		}		
		else if (!strcmp("expire", key))
		{
			p->pw_expire = atoi(vals[0]);
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (p->pw_name == NULL) p->pw_name = strdup("");
	if (p->pw_passwd == NULL) p->pw_passwd = strdup("");
	if (p->pw_class == NULL) p->pw_class = strdup("");
	if (p->pw_gecos == NULL) p->pw_gecos = strdup("");
	if (p->pw_dir == NULL) p->pw_dir = strdup("");
	if (p->pw_shell == NULL) p->pw_shell = strdup("");

	return p;
}

static struct passwd *
copy_user(struct passwd *in)
{
	struct passwd *p;

	if (in == NULL) return NULL;

	p = (struct passwd *)calloc(1, sizeof(struct passwd));

	p->pw_name = LU_COPY_STRING(in->pw_name);
	p->pw_passwd = LU_COPY_STRING(in->pw_passwd);
	p->pw_uid = in->pw_uid;
	p->pw_gid = in->pw_gid;
	p->pw_change = in->pw_change;
	p->pw_class = LU_COPY_STRING(in->pw_class);
	p->pw_gecos = LU_COPY_STRING(in->pw_gecos);
	p->pw_dir = LU_COPY_STRING(in->pw_dir);
	p->pw_shell = LU_COPY_STRING(in->pw_shell);
	p->pw_expire = in->pw_expire;

	return p;
}

static int
copy_user_r(struct passwd *in, struct passwd *out, char *buffer, int buflen)
{
	int hsize;
	char *bp;

	if (in == NULL) return -1;
	if (out == NULL) return -1;

	if (buffer == NULL) buflen = 0;

	/* Calculate size of input */
	hsize = 0;
	if (in->pw_name != NULL) hsize += strlen(in->pw_name);
	if (in->pw_passwd != NULL) hsize += strlen(in->pw_passwd);
	if (in->pw_class != NULL) hsize += strlen(in->pw_class);
	if (in->pw_gecos != NULL) hsize += strlen(in->pw_gecos);
	if (in->pw_dir != NULL) hsize += strlen(in->pw_dir);
	if (in->pw_shell != NULL) hsize += strlen(in->pw_shell);

	/* Check buffer space */
	if (hsize > buflen) return -1;

	/* Copy result into caller's struct passwd, using buffer for memory */
	bp = buffer;

	out->pw_name = NULL;
	if (in->pw_name != NULL)
	{
		out->pw_name = bp;
		hsize = strlen(in->pw_name) + 1;
		memmove(bp, in->pw_name, hsize);
		bp += hsize;
	}

	out->pw_passwd = NULL;
	if (in->pw_passwd != NULL)
	{
		out->pw_passwd = bp;
		hsize = strlen(in->pw_passwd) + 1;
		memmove(bp, in->pw_passwd, hsize);
		bp += hsize;
	}

	out->pw_uid = in->pw_uid;

	out->pw_gid = in->pw_gid;

	out->pw_change = in->pw_change;

	out->pw_class = NULL;
	if (in->pw_class != NULL)
	{
		out->pw_class = bp;
		hsize = strlen(in->pw_class) + 1;
		memmove(bp, in->pw_class, hsize);
		bp += hsize;
	}

	out->pw_gecos = NULL;
	if (in->pw_gecos != NULL)
	{
		out->pw_gecos = bp;
		hsize = strlen(in->pw_gecos) + 1;
		memmove(bp, in->pw_gecos, hsize);
		bp += hsize;
	}

	out->pw_dir = NULL;
	if (in->pw_dir != NULL)
	{
		out->pw_dir = bp;
		hsize = strlen(in->pw_dir) + 1;
		memmove(bp, in->pw_dir, hsize);
		bp += hsize;
	}

	out->pw_shell = NULL;
	if (in->pw_shell != NULL)
	{
		out->pw_shell = bp;
		hsize = strlen(in->pw_shell) + 1;
		memmove(bp, in->pw_shell, hsize);
		bp += hsize;
	}

	out->pw_expire = in->pw_expire;

	return 0;
}

static void
recycle_user(struct lu_thread_info *tdata, struct passwd *in)
{
	struct passwd *p;

	if (tdata == NULL) return;
	p = (struct passwd *)tdata->lu_entry;

	if (in == NULL)
	{
		free_user(p);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_user_data(p);

	p->pw_name = in->pw_name;
	p->pw_passwd = in->pw_passwd;
	p->pw_uid = in->pw_uid;
	p->pw_gid = in->pw_gid;
	p->pw_change = in->pw_change;
	p->pw_class = in->pw_class;
	p->pw_gecos = in->pw_gecos;
	p->pw_dir = in->pw_dir;
	p->pw_shell = in->pw_shell;
	p->pw_expire = in->pw_expire;

	free(in);
}

static struct passwd *
lu_getpwuid(int uid)
{
	struct passwd *p;
	unsigned int datalen;
	XDR inxdr;
	static int proc = -1;
	int count;
	char *lookup_buf;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getpwuid_A", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	uid = htonl(uid);
	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)&uid, 1, &lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		return NULL;
	}

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

	p = extract_user(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return p;
}

static struct passwd *
lu_getpwnam(const char *name)
{
	struct passwd *p;
	unsigned int datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	int count;
	char *lookup_buf;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getpwnam_A", &proc) != KERN_SUCCESS)
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

	if (_lookup_all(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen)
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

	p = extract_user(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);


	return p;
}

static void
lu_endpwent(void)
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_user, free_lu_thread_info_user);
	_lu_data_free_vm_xdr(tdata);
}

static int
lu_setpwent(void)
{
	lu_endpwent();
	return 1;
}

static struct passwd *
lu_getpwent()
{
	struct passwd *p;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_user, free_lu_thread_info_user);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_user, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getpwent_A", &proc) != KERN_SUCCESS)
			{
				lu_endpwent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endpwent();
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
			lu_endpwent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endpwent();
		return NULL;
	}

	p = extract_user(tdata->lu_xdr);
	if (p == NULL)
	{
		lu_endpwent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return p;
}

static struct passwd *
getpw_internal(const char *name, uid_t uid, int source)
{
	struct passwd *res = NULL;
	static char		*loginName = NULL;
	static struct passwd	*loginEnt  = NULL;

	if (loginName == NULL)
	{
		char	*l = getlogin();

		pthread_mutex_lock(&_user_lock);
		if ((loginEnt == NULL) && (l != NULL) && (*l != '\0'))
		{
			if (_lu_running())
			{
				loginEnt = lu_getpwnam(l);
			}
			else
			{
				loginEnt = copy_user(_old_getpwnam(l));
			}
	
			loginName = l;
		}
		pthread_mutex_unlock(&_user_lock);
	}

	if (loginEnt != NULL)
	{
		switch (source)
		{
			case PW_GET_NAME:
				if (strcmp(name, loginEnt->pw_name) == 0)
				{
					name = loginName;
				}
				if (strcmp(name, loginEnt->pw_gecos) == 0)
				{
					name = loginName;
				}
				break;
			case PW_GET_UID:
				if (uid == loginEnt->pw_uid)
				{
					source = PW_GET_NAME;
					name = loginName;
				}
				break;
			default:
				break;
		}
	}

	if (_lu_running())
	{
		switch (source)
		{
			case PW_GET_NAME:
				res = lu_getpwnam(name);
				break;
			case PW_GET_UID:
				res = lu_getpwuid(uid);
				break;
			case PW_GET_ENT:
				res = lu_getpwent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_user_lock);
		switch (source)
		{
			case PW_GET_NAME:
				res = copy_user(_old_getpwnam(name));
				break;
			case PW_GET_UID:
				res = copy_user(_old_getpwuid(uid));
				break;
			case PW_GET_ENT:
				res = copy_user(_old_getpwent());
				break;
			default: res = NULL;
		}
		pthread_mutex_unlock(&_user_lock);
	}

	return res;
}

static struct passwd *
getpw(const char *name, uid_t uid, int source)
{
	struct passwd *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_user, free_lu_thread_info_user);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_user, tdata);
	}

	res = getpw_internal(name, uid, source);

	recycle_user(tdata, res);

	return (struct passwd *)tdata->lu_entry;
}

static int
getpw_r(const char *name, uid_t uid, int source, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result)
{
	struct passwd *res = NULL;
	int status;

	*result = NULL;
	errno = 0;

	res = getpw_internal(name, uid, source);
	if (res == NULL) return -1;

	status = copy_user_r(res, pwd, buffer, bufsize);
	free_user(res);

	if (status != 0)
	{
		errno = ERANGE;
		return -1;
	}

	*result = pwd;
	return 0;
}

struct passwd *
getpwnam(const char *name)
{
	return getpw(name, -2, PW_GET_NAME);
}

struct passwd *
getpwuid(uid_t uid)
{
	return getpw(NULL, uid, PW_GET_UID);
}

struct passwd *
getpwent(void)
{
	return getpw(NULL, -2, PW_GET_ENT);
}

int
setpwent(void)
{
	if (_lu_running()) lu_setpwent();
	else _old_setpwent();
	return 1;
}

void
endpwent(void)
{
	if (_lu_running()) lu_endpwent();
	else _old_endpwent();
}
int
getpwnam_r(const char *name, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result)
{
	return getpw_r(name, -2, PW_GET_NAME, pwd, buffer, bufsize, result);
}

int
getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result)
{
	return getpw_r(NULL, uid, PW_GET_UID, pwd, buffer, bufsize, result);
}
