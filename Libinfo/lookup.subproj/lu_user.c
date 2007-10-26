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
#include <pwd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include "lu_utils.h"
#include "lu_overrides.h"

#define USER_CACHE_SIZE 10

static pthread_mutex_t _user_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static void *_user_cache[USER_CACHE_SIZE] = { NULL };
static unsigned int _user_cache_index = 0;
static unsigned int _user_cache_init = 0;

static pthread_mutex_t _user_lock = PTHREAD_MUTEX_INITIALIZER;

#define PW_GET_NAME 1
#define PW_GET_UID 2
#define PW_GET_ENT 3

#define ENTRY_SIZE sizeof(struct passwd)
#define ENTRY_KEY _li_data_key_user

__private_extern__ struct passwd *LI_files_getpwent();
__private_extern__ struct passwd *LI_files_getpwnam(const char *name);
__private_extern__ struct passwd *LI_files_getpwuid(uid_t uid);
__private_extern__ void LI_files_setpwent();
__private_extern__ void LI_files_endpwent();

static struct passwd *
copy_user(struct passwd *in)
{
	if (in == NULL) return NULL;

	return (struct passwd *)LI_ils_create("ss44LssssL", in->pw_name, in->pw_passwd, in->pw_uid, in->pw_gid, in->pw_change, in->pw_class, in->pw_gecos, in->pw_dir, in->pw_shell, in->pw_expire);
}

/*
 * Extract the next user entry from a kvarray.
 */
static void *
extract_user(kvarray_t *in)
{
	struct passwd tmp;
	uint32_t d, k, kcount;

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	memset(&tmp, 0, ENTRY_SIZE);

	tmp.pw_uid = -2;
	tmp.pw_gid = -2;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "pw_name"))
		{
			if (tmp.pw_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "pw_passwd"))
		{
			if (tmp.pw_passwd != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_passwd = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "pw_uid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_uid = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "pw_gid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_gid = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "pw_change"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_change = atol(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "pw_expire"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_expire = atol(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "pw_class"))
		{
			if (tmp.pw_class != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_class = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "pw_gecos"))
		{
			if (tmp.pw_gecos != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_gecos = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "pw_dir"))
		{
			if (tmp.pw_dir != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_dir = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "pw_shell"))
		{
			if (tmp.pw_shell != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_shell = (char *)in->dict[d].val[k][0];
		}
	}

	if (tmp.pw_name == NULL) tmp.pw_name = "";
	if (tmp.pw_passwd == NULL) tmp.pw_passwd = "";
	if (tmp.pw_class == NULL) tmp.pw_class = "";
	if (tmp.pw_gecos == NULL) tmp.pw_gecos = "";
	if (tmp.pw_dir == NULL) tmp.pw_dir = "";
	if (tmp.pw_shell == NULL) tmp.pw_shell = "";

	return copy_user(&tmp);
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
	if (in->pw_name != NULL) hsize += (strlen(in->pw_name) + 1);
	if (in->pw_passwd != NULL) hsize += (strlen(in->pw_passwd) + 1);
	if (in->pw_class != NULL) hsize += (strlen(in->pw_class) + 1);
	if (in->pw_gecos != NULL) hsize += (strlen(in->pw_gecos) + 1);
	if (in->pw_dir != NULL) hsize += (strlen(in->pw_dir) + 1);
	if (in->pw_shell != NULL) hsize += (strlen(in->pw_shell) + 1);

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
cache_user(struct passwd *pw)
{
	struct passwd *pwcache;

	if (pw == NULL) return;

	pthread_mutex_lock(&_user_cache_lock);

	pwcache = copy_user(pw);

	if (_user_cache[_user_cache_index] != NULL) LI_ils_free(_user_cache[_user_cache_index], ENTRY_SIZE);

	_user_cache[_user_cache_index] = pwcache;
	_user_cache_index = (_user_cache_index + 1) % USER_CACHE_SIZE;

	_user_cache_init = 1;

	pthread_mutex_unlock(&_user_cache_lock);
}

static int
user_cache_check()
{
	uint32_t i, status;

	/* don't consult cache if it has not been initialized */
	if (_user_cache_init == 0) return 1;

	status = LI_L1_cache_check(ENTRY_KEY);

	/* don't consult cache if it is disabled or if we can't validate */
	if ((status == LI_L1_CACHE_DISABLED) || (status == LI_L1_CACHE_FAILED)) return 1;

	/* return 0 if cache is OK */
	if (status == LI_L1_CACHE_OK) return 0;

	/* flush cache */
	pthread_mutex_lock(&_user_cache_lock);

	for (i = 0; i < USER_CACHE_SIZE; i++)
	{
		LI_ils_free(_user_cache[i], ENTRY_SIZE);
		_user_cache[i] = NULL;
	}

	_user_cache_index = 0;

	pthread_mutex_unlock(&_user_cache_lock);

	/* don't consult cache - it's now empty */
	return 1;
}

static struct passwd *
cache_getpwnam(const char *name)
{
	uint32_t i;
	struct passwd *pw, *res;

	if (name == NULL) return NULL;
	if (user_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_user_cache_lock);

	for (i = 0; i < USER_CACHE_SIZE; i++)
	{
		pw = (struct passwd *)_user_cache[i];
		if (pw == NULL) continue;
		if (pw->pw_name == NULL) continue;

		if (!strcmp(name, pw->pw_name))
		{
			res = copy_user(pw);
			pthread_mutex_unlock(&_user_cache_lock);
			return res;
		}
	}

	pthread_mutex_unlock(&_user_cache_lock);
	return NULL;
}

static struct passwd *
cache_getpwuid(uid_t uid)
{
	uint32_t i;
	struct passwd *pw, *res;

	if (user_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_user_cache_lock);

	for (i = 0; i < USER_CACHE_SIZE; i++)
	{
		pw = (struct passwd *)_user_cache[i];
		if (pw == NULL) continue;

		if (uid == pw->pw_uid)
		{
			res = copy_user(pw);
			pthread_mutex_unlock(&_user_cache_lock);
			return res;
		}
	}

	pthread_mutex_unlock(&_user_cache_lock);

	return NULL;
}

static struct passwd *
ds_getpwuid(uid_t uid)
{
	static int proc = -1;
	char val[16];

	snprintf(val, sizeof(val), "%d", (int)uid);
	return (struct passwd *)LI_getone("getpwuid", &proc, extract_user, "uid", val);
}

static struct passwd *
ds_getpwnam(const char *name)
{
	static int proc = -1;

	return (struct passwd *)LI_getone("getpwnam", &proc, extract_user, "login", name);
}

static void
ds_endpwent(void)
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static int
ds_setpwent(void)
{
	ds_endpwent();
	return 1;
}

static struct passwd *
ds_getpwent()
{
	static int proc = -1;

	return (struct passwd *)LI_getent("getpwent", &proc, extract_user, ENTRY_KEY, ENTRY_SIZE);
}

static struct passwd *
getpw_internal(const char *name, uid_t uid, int source)
{
	struct passwd *res;
	int add_to_cache;

	add_to_cache = 0;
	res = NULL;

	switch (source)
	{
		case PW_GET_NAME:
			res = cache_getpwnam(name);
			break;
		case PW_GET_UID:
			res = cache_getpwuid(uid);
			break;
		default: res = NULL;
	}

	if (res != NULL)
	{
	}
	else if (_ds_running())
	{
		switch (source)
		{
			case PW_GET_NAME:
				res = ds_getpwnam(name);
				break;
			case PW_GET_UID:
				res = ds_getpwuid(uid);
				break;
			case PW_GET_ENT:
				res = ds_getpwent();
				break;
			default: res = NULL;
		}

		if (res != NULL) add_to_cache = 1;
	}
	else
	{
		pthread_mutex_lock(&_user_lock);

		switch (source)
		{
			case PW_GET_NAME:
				res = copy_user(LI_files_getpwnam(name));
				break;
			case PW_GET_UID:
				res = copy_user(LI_files_getpwuid(uid));
				break;
			case PW_GET_ENT:
				res = copy_user(LI_files_getpwent());
				break;
			default: res = NULL;
		}

		pthread_mutex_unlock(&_user_lock);
	}

	if (add_to_cache == 1) cache_user(res);

	return res;
}

static struct passwd *
getpw(const char *name, uid_t uid, int source)
{
	struct passwd *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	res = getpw_internal(name, uid, source);

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct passwd *)tdata->li_entry;
}

static int
getpw_r(const char *name, uid_t uid, int source, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result)
{
	struct passwd *res = NULL;
	int status;

	*result = NULL;

	res = getpw_internal(name, uid, source);
	if (res == NULL) return 0;

	status = copy_user_r(res, pwd, buffer, bufsize);

	LI_ils_free(res, ENTRY_SIZE);

	if (status != 0) return ERANGE;

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

void
setpwent(void)
{
	if (_ds_running()) ds_setpwent();
	else LI_files_setpwent();
}

void
endpwent(void)
{
	if (_ds_running()) ds_endpwent();
	else LI_files_endpwent();
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
